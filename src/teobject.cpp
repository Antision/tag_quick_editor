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

void teObject::teDisconnectExceptLocked(int keepType)
{
    // Callbacks *this emits.
    for (auto it = linked_callback_call.begin(); it != linked_callback_call.end(); ) {
        if (it->second && it->second->type == keepType) {
            ++it;
            continue;
        }
        teCallbackPtr cb = it->second;
        if (cb)
            cb->connected = false;
        if (it->first) {
            auto& peer = it->first->linked_callback_recive;
            for (auto p = peer.begin(); p != peer.end(); ) {
                if (p->first == this && p->second == cb)
                    p = peer.erase(p);
                else
                    ++p;
            }
        }
        it = linked_callback_call.erase(it);
    }

    // Callbacks *this receives.
    for (auto it = linked_callback_recive.begin(); it != linked_callback_recive.end(); ) {
        if (it->second && it->second->type == keepType) {
            ++it;
            continue;
        }
        teCallbackPtr cb = it->second;
        if (cb)
            cb->connected = false;
        if (it->first) {
            auto& peer = it->first->linked_callback_call;
            for (auto p = peer.begin(); p != peer.end(); ) {
                if (p->first == this && p->second == cb)
                    p = peer.erase(p);
                else
                    ++p;
            }
        }
        it = linked_callback_recive.erase(it);
    }
}

void teObject::teDisconnect(teObject* obj, int type)
{
    std::lock_guard<std::recursive_mutex> lg(teCallbackMutex());

    auto matches = [&](const CallbackMap::value_type& entry) {
        if (obj != nullptr && entry.first != obj)
            return false;
        if (type >= 0 && (!entry.second || entry.second->type != type))
            return false;
        return true;
    };

    for (auto it = linked_callback_call.begin(); it != linked_callback_call.end(); ) {
        if (!matches(*it)) {
            ++it;
            continue;
        }
        teCallbackPtr cb = it->second;
        if (cb)
            cb->connected = false;
        if (it->first) {
            auto& peer = it->first->linked_callback_recive;
            for (auto p = peer.begin(); p != peer.end(); ) {
                if (p->first == this && p->second == cb)
                    p = peer.erase(p);
                else
                    ++p;
            }
        }
        it = linked_callback_call.erase(it);
    }

    for (auto it = linked_callback_recive.begin(); it != linked_callback_recive.end(); ) {
        if (!matches(*it)) {
            ++it;
            continue;
        }
        teCallbackPtr cb = it->second;
        if (cb)
            cb->connected = false;
        if (it->first) {
            auto& peer = it->first->linked_callback_call;
            for (auto p = peer.begin(); p != peer.end(); ) {
                if (p->first == this && p->second == cb)
                    p = peer.erase(p);
                else
                    ++p;
            }
        }
        it = linked_callback_recive.erase(it);
    }
}

void teObject::teemit(teCallbackType calltype, bool autodelete)
{
    std::vector<teCallbackPtr> snapshot;
    {
        std::lock_guard<std::recursive_mutex> lg(teCallbackMutex());
        for (const auto& [receiver, cb] : linked_callback_call) {
            if (cb && cb->type == calltype && cb->connected)
                snapshot.push_back(cb);
        }
    }

    // `cb` keeps the callback alive even if a slot disconnects it (or the whole
    // sender) in the middle of this loop.
    for (const teCallbackPtr& cb : snapshot) {
        if (cb->connected)
            (*cb)();
    }
}

void teObject::onDestroy()
{
    if (onDestroyCalled.exchange(true))
        return;

    std::vector<teCallbackPtr> destroyCallbacks;
    {
        std::lock_guard<std::recursive_mutex> lg(teCallbackMutex());
        for (const auto& [receiver, cb] : linked_callback_call) {
            if (cb && cb->type == teCallbackType::destroy && cb->connected)
                destroyCallbacks.push_back(cb);
        }
        // Drop every other connection first so the destroy slots see an object
        // that no longer emits or receives anything else.
        teDisconnectExceptLocked(teCallbackType::destroy);
    }

    for (const teCallbackPtr& cb : destroyCallbacks) {
        if (cb->connected)
            (*cb)();
    }

    // The destroy slots usually disconnect themselves; remove whatever is left.
    teDisconnect(nullptr);
}


teObject::~teObject()
{
    onDestroy();
}
