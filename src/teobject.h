#ifndef TEOBJECT_H
#define TEOBJECT_H

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

class teObject;
class obj_callback_function_base;

/**
 * A connection is held by std::shared_ptr so that a callback that is currently
 * being dispatched can never be destroyed underneath the dispatcher, even if a
 * slot disconnects itself (or is disconnected by whoever it calls into).
 */
using teCallbackPtr = std::shared_ptr<obj_callback_function_base>;

enum teCallbackType{
    edit=0,
    edit_with_layout,
    destroy,
    ready_destroy,
    insert,
    loading_finished,
    extraWidget_removed
};

/**
 * @brief Process-wide mutex serialising every teObject connection operation.
 *
 * Connections are rare and emits are cheap, so one global recursive mutex costs
 * nothing measurable while removing the data races that exist between the
 * image-loader worker threads and the GUI thread.
 */
std::recursive_mutex& teCallbackMutex();

/**
 * @brief Hand-rolled signal/slot facility for classes that cannot be a QObject.
 *
 * Its users are the tag layer's data objects (teTag, teWord): they are copied,
 * they are created and dropped by the hundred on every picture switch, and they
 * still have to tell the views and the editors that they changed or are going
 * away. None of that justifies a QObject base - and a QObject cannot be copied
 * at all, which is what the tag layer is built on.
 *
 * Shape: every teConnect() makes one connection object. The sender owns it in a
 * flat list and the receiver keeps a raw pointer to it, so a connection costs
 * one allocation and is unlinked from both ends in O(n), n being that object's
 * own connections (three or four in practice). Dispatching does not allocate:
 * the walk holds a shared_ptr, so a slot may disconnect itself, disconnect the
 * next one, or connect new ones while the emit is running.
 *
 * Dispatch order is registration order.
 *
 * Not provided on purpose: a deferred delete. Lifetime here is owned by
 * std::shared_ptr; the `deleteLater()` that only pretended to be deferred (it
 * deleted immediately) is gone, and nothing used it.
 */
class teObject {
public:
    QString info;

    teObject() = default;
    teObject(const teObject& in) : info(in.info) {}  ///< connections are never copied
    teObject& operator=(const teObject& in) { info = in.info; return *this; }
    virtual ~teObject();

    /// Disconnects everything and runs pending `destroy` callbacks. Idempotent.
    void onDestroy();
    bool isDestroyed() const { return onDestroyCalled.load(std::memory_order_relaxed); }

    template<typename OBJT, typename T, typename RET, typename... Args, typename... ArgTypes>
        requires std::derived_from<OBJT, T>
    teCallbackPtr teConnect(teCallbackType when, OBJT* reciver, RET (T::*in_func_ptr)(Args...), ArgTypes... args);

    /**
     * @brief Subscribes a callable that has no receiver object.
     *
     * QObject::connect can do this and the tag layer could not, which is why
     * anything that wanted to subscribe had to be a teObject itself. The sender
     * still owns the connection: keep the handle and pass it to
     * teDisconnect(handle) to stop it (dropping the handle does not stop it).
     */
    teCallbackPtr teConnect(teCallbackType when, std::function<void()> slot);

    /// Disconnects connections with `obj`. `obj == nullptr` disconnects everything.
    /// `type < 0` ignores the callback type.
    void teDisconnect(teObject* obj = nullptr, int type = -1);
    /// Disconnects exactly one connection - the only way to stop a lambda one.
    void teDisconnect(const teCallbackPtr& connection);

    /**
     * @brief Copies of the outgoing connections of `type` (`type < 0`: all).
     *
     * For a caller that has to look at them, or run them later, without racing
     * whoever owns them: the copies keep the callbacks alive after a disconnect.
     */
    std::vector<teCallbackPtr> teCollectCallbacks(int type = -1);

    /// Invokes every connected callback of `calltype`.
    void teemit(teCallbackType calltype);

private:
    /**
     * @brief Removes `cb` from both index lists and marks it dead.
     *
     * Caller must hold teCallbackMutex(). The sender's list owns `cb`, so
     * erasing it there may destroy the callback: nothing may touch `cb` after
     * this call returns.
     */
    static void unlinkConnection(obj_callback_function_base* cb);
    /// Detaches every connection whose type differs from `keepType`.
    /// Caller must hold teCallbackMutex().
    void teDisconnectExceptLocked(int keepType);

    /// Outgoing connections, in registration order. Owns the callbacks.
    std::vector<teCallbackPtr> m_out;
    /// Kept equal to m_out.size() so teemit() can answer "does anybody listen?"
    /// without the mutex. Read on the emit path only; written under the mutex.
    std::atomic<size_t> m_outCount{0};
    /// Incoming connections, raw: the sender's list owns them, this one only has
    /// to know whom to tell when this object goes away. Order is irrelevant.
    std::vector<obj_callback_function_base*> m_in;

    std::atomic<bool> onDestroyCalled{false};
};

/// See teLiveCallbackObjects() below; defined in teobject.cpp.
void teCountLiveCallbackObject(int delta, const char* receiverType, int type);

class obj_callback_function_base {
public:
    int type;
    /// `typeid` of the receiver, for the leak diagnostics (see teLiveCallbackBreakdown).
    const char* receiverType = "?";
    /// Set to false by teDisconnect(); a dispatch in flight checks it so a
    /// callback disconnected during the same emit is not invoked.
    std::atomic<bool> connected{true};
    /// Who to unlink from when this connection goes away. Set by teConnect*().
    teObject* sender = nullptr;
    /// The receiver, or nullptr for a lambda subscription.
    teObject* receiver = nullptr;

    explicit obj_callback_function_base(int in_type) : type(in_type) {}
    virtual void operator()() = 0;
    virtual ~obj_callback_function_base();
};

/**
 * @brief Number of connection objects currently alive.
 *
 * A connection object is shared by the sender's and the receiver's list, so this
 * is exactly "how many teConnect() calls are still in effect". Every editor link
 * makes three of them (destroy / edit / edit_with_layout), which makes this the
 * cheapest possible sentinel for the one thing a per-image leak would hit first:
 * the connection bookkeeping that runs on every picture switch.
 */
long teLiveCallbackObjects();
/// The same number, split by (receiver class, callback type). Used to attribute a
/// connection leak to the class that forgot to unlink.
std::map<std::string,long> teLiveCallbackBreakdown();
/// Turns the per-class bookkeeping on or off (off by default: it costs a map
/// lookup per connection). Clearing on enable keeps the numbers comparable.
void teSetCallbackBreakdownEnabled(bool enabled);

template <typename T, typename RET, typename... Args>
class obj_callback_function : public obj_callback_function_base {
public:
    T* obj_ptr;
    RET (T::*func_ptr)(Args...);
    std::tuple<Args...> params;

    obj_callback_function(int type, T* reciver, RET (T::*in_func_ptr)(Args...), Args... args)
        : obj_callback_function_base(type), obj_ptr(reciver), func_ptr(in_func_ptr), params(std::make_tuple(args...)) {
        receiverType = typeid(T).name();
        teCountLiveCallbackObject(1, receiverType, type);
    }

    template <std::size_t... Indexes>
    RET call_impl(std::index_sequence<Indexes...>) {
        return (obj_ptr->*func_ptr)(std::get<Indexes>(params)...);
    }
    void operator()() override {
        if (!connected)
            return;
        call_impl(std::make_index_sequence<sizeof...(Args)>{});
    }
};

/// A subscription that has no receiver object; see teObject::teConnect().
class obj_callback_function_lambda : public obj_callback_function_base {
public:
    std::function<void()> slot;

    obj_callback_function_lambda(int type, std::function<void()> in_slot)
        : obj_callback_function_base(type), slot(std::move(in_slot)) {
        receiverType = "lambda";
        teCountLiveCallbackObject(1, receiverType, type);
    }
    void operator()() override {
        if (!connected || !slot)
            return;
        slot();
    }
};

template<typename OBJT, typename T, typename RET, typename... Args, typename... ArgTypes>
    requires std::derived_from<OBJT, T>
teCallbackPtr teObject::teConnect(teCallbackType when, OBJT* reciver, RET (T::*in_func_ptr)(Args...), ArgTypes... args) {
    auto new_callback_func = std::make_shared<obj_callback_function<T, RET, Args...>>(
        when, static_cast<T*>(reciver), in_func_ptr, static_cast<Args>(args)...);
    new_callback_func->sender = this;
    new_callback_func->receiver = static_cast<teObject*>(reciver);
    std::lock_guard<std::recursive_mutex> lg(teCallbackMutex());
    if (new_callback_func->receiver)
        new_callback_func->receiver->m_in.push_back(new_callback_func.get());
    m_out.push_back(new_callback_func);
    m_outCount.fetch_add(1, std::memory_order_relaxed);
    return new_callback_func;
}

#endif // TEOBJECT_H
