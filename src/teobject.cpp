#include "teobject.h"

namespace {
std::atomic<long> g_liveCallbackObjects{0};
/// Attribution is a debugging aid with a map lookup per connection, so it is off
/// unless someone asks for it (the regression harness does).
std::atomic<bool> g_callbackBreakdownEnabled{false};
/// (receiver class, callback type) -> live connections.
std::map<std::string,long>& callbackBreakdown()
{
    static std::map<std::string,long> m;
    return m;
}
}

void teCountLiveCallbackObject(int delta, const char* receiverType, int type)
{
    g_liveCallbackObjects.fetch_add(delta, std::memory_order_relaxed);
    if(!g_callbackBreakdownEnabled.load(std::memory_order_relaxed))
        return;
    std::lock_guard<std::recursive_mutex> lg(teCallbackMutex());
    const std::string key = std::string(receiverType) + " #" + std::to_string(type);
    long& slot = callbackBreakdown()[key];
    slot += delta;
    if(slot == 0)
        callbackBreakdown().erase(key);
}

void teSetCallbackBreakdownEnabled(bool enabled)
{
    std::lock_guard<std::recursive_mutex> lg(teCallbackMutex());
    callbackBreakdown().clear();
    g_callbackBreakdownEnabled = enabled;
}

obj_callback_function_base::~obj_callback_function_base()
{
    teCountLiveCallbackObject(-1, receiverType, type);
}

long teLiveCallbackObjects()
{
    return g_liveCallbackObjects.load(std::memory_order_relaxed);
}

std::map<std::string,long> teLiveCallbackBreakdown()
{
    std::lock_guard<std::recursive_mutex> lg(teCallbackMutex());
    return callbackBreakdown();
}

std::recursive_mutex& teCallbackMutex()
{
    static std::recursive_mutex mtx;
    return mtx;
}

teCallbackPtr teObject::teConnect(teCallbackType when, std::function<void()> slot)
{
    auto connection = std::make_shared<obj_callback_function_lambda>(when, std::move(slot));
    connection->sender = this;
    std::lock_guard<std::recursive_mutex> lg(teCallbackMutex());
    m_out.push_back(connection);
    m_outCount.fetch_add(1, std::memory_order_relaxed);
    return connection;
}

void teObject::unlinkConnection(obj_callback_function_base* cb)
{
    if(!cb)
        return;
    cb->connected = false;
    teObject* sender = cb->sender;
    teObject* receiver = cb->receiver;
    // Cleared before either list is touched: erasing the sender's entry below may
    // destroy the callback, and a dangling pointer must not be reachable from
    // either end afterwards.
    cb->sender = nullptr;
    cb->receiver = nullptr;

    if(receiver){
        auto& in = receiver->m_in;
        const auto it = std::find(in.begin(),in.end(),cb);
        if(it != in.end())
            in.erase(it);
    }

    // The connection is owned by the sender's list, so unlinking it there is what
    // can destroy it - and its arguments, which hold shared_ptrs to tags. A tag
    // dying runs its `destroy` callbacks, and those reach back into this very
    // list (unlink, refresh), so the erase must not be the place where that
    // happens: the vector would be mutated while it is being shrunk.
    //
    // Hence the two guards. `unlinked` holds the connection itself, `tailGuard`
    // holds whatever element erase() is about to destroy (one atomic increment),
    // so the erase only ever drops an empty pointer. Both guards die at the end of
    // this function, once both index lists are consistent again.
    teCallbackPtr unlinked;
    if(sender){
        auto& out = sender->m_out;
        const auto it = std::find_if(out.begin(),out.end(),
                                     [cb](const teCallbackPtr& held){ return held.get() == cb; });
        if(it != out.end()){
            unlinked = std::move(*it);
            const teCallbackPtr tailGuard = out.back();
            out.erase(it);
            sender->m_outCount.fetch_sub(1, std::memory_order_relaxed);
        }
    }
}

void teObject::teDisconnect(teObject* obj, int type)
{
    std::lock_guard<std::recursive_mutex> lg(teCallbackMutex());

    // Outgoing: this is the sender. Removing one shifts the rest down, so the
    // index stays where it is.
    for(size_t index=0;index<m_out.size();){
        obj_callback_function_base* cb = m_out[index].get();
        const bool match = (obj == nullptr || cb->receiver == obj)
                        && (type < 0 || cb->type == type);
        if(!match){
            ++index;
            continue;
        }
        unlinkConnection(cb);
    }

    // Incoming: this is the receiver, and the sender owns the connection.
    for(size_t index=0;index<m_in.size();){
        obj_callback_function_base* cb = m_in[index];
        const bool match = (obj == nullptr || cb->sender == obj)
                        && (type < 0 || cb->type == type);
        if(!match){
            ++index;
            continue;
        }
        unlinkConnection(cb);
    }
}

void teObject::teDisconnect(const teCallbackPtr& connection)
{
    if(!connection)
        return;
    std::lock_guard<std::recursive_mutex> lg(teCallbackMutex());
    obj_callback_function_base* cb = connection.get();
    if(cb->sender == this || cb->receiver == this)
        unlinkConnection(cb);
}

void teObject::teDisconnectExceptLocked(int keepType)
{
    for(size_t index=0;index<m_out.size();){
        obj_callback_function_base* cb = m_out[index].get();
        if(cb->type == keepType){
            ++index;
            continue;
        }
        unlinkConnection(cb);
    }
    for(size_t index=0;index<m_in.size();){
        obj_callback_function_base* cb = m_in[index];
        if(cb->type == keepType){
            ++index;
            continue;
        }
        unlinkConnection(cb);
    }
}

void teObject::teemit(teCallbackType calltype)
{
    // Most emits reach nobody: a tag does not have a control for every word it
    // carries, and a list announces edits nobody listens to. The counter makes
    // that case free - not even the mutex - and it is only ever read here, so a
    // stale value can cost at most one skipped announcement on a thread that is
    // connecting concurrently, which is already unordered.
    if(m_outCount.load(std::memory_order_relaxed) == 0)
        return;

    // No snapshot vector and no allocation: hold the current connection, call it,
    // then decide from the list itself whether the next slot is the one to look
    // at. A slot that disconnects itself (or the next one) is therefore handled
    // the same way Qt handles it, and a slot that connects somebody new adds them
    // to the end without invalidating this walk.
    size_t index = 0;
    for(;;){
        teCallbackPtr cb;
        {
            std::lock_guard<std::recursive_mutex> lg(teCallbackMutex());
            if(index >= m_out.size())
                break;
            cb = m_out[index];
        }
        if(cb->connected && cb->type == calltype)
            (*cb)();
        std::lock_guard<std::recursive_mutex> lg(teCallbackMutex());
        if(index < m_out.size() && m_out[index] == cb)
            ++index;
        // Otherwise the element at `index` is a different one now (something was
        // unlinked): do not advance, the slot has to be looked at again.
    }
}

std::vector<teCallbackPtr> teObject::teCollectCallbacks(int type)
{
    std::lock_guard<std::recursive_mutex> lg(teCallbackMutex());
    std::vector<teCallbackPtr> collected;
    collected.reserve(m_out.size());
    for(const teCallbackPtr& cb : m_out)
        if(cb && cb->connected && (type < 0 || cb->type == type))
            collected.push_back(cb);
    return collected;
}

void teObject::onDestroy()
{
    if(onDestroyCalled.exchange(true))
        return;

    std::vector<teCallbackPtr> destroyCallbacks;
    {
        std::lock_guard<std::recursive_mutex> lg(teCallbackMutex());
        for(const teCallbackPtr& cb : m_out)
            if(cb && cb->type == teCallbackType::destroy && cb->connected)
                destroyCallbacks.push_back(cb);
        // Drop every other connection first so the destroy slots see an object
        // that no longer emits or receives anything else.
        teDisconnectExceptLocked(teCallbackType::destroy);
    }

    for(const teCallbackPtr& cb : destroyCallbacks)
        if(cb->connected)
            (*cb)();

    // The destroy slots usually disconnect themselves; remove whatever is left.
    teDisconnect(nullptr);
}

teObject::~teObject()
{
    onDestroy();
}
