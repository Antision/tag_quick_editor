#ifndef TEOBJECT_H
#define TEOBJECT_H

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

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
 * nothing measurable while removing the data races that used to exist between
 * the image-loader worker threads and the GUI thread.
 */
std::recursive_mutex& teCallbackMutex();

/**
 * @brief Hand-rolled signal/slot facility for classes that cannot inherit QObject.
 *
 * Known limitations (see the refactoring notes in the repository):
 *  - no automatic disconnection when the *receiver* dies: the receiver must be
 *    destroyed before, or disconnect itself from, the sender;
 *  - `deleteLater()` deletes immediately; it is not a deferred delete.
 *
 * Anything that already is a QObject should use QObject::connect instead.
 */
class teObject {
public:
    using CallbackMap = std::multimap<teObject*, teCallbackPtr>;

    CallbackMap linked_callback_call;   ///< connections where *this is the sender
    CallbackMap linked_callback_recive; ///< connections where *this is the receiver

    QString info;

    teObject() = default;
    teObject(const teObject& in) : info(in.info) {}  ///< connections are never copied
    teObject& operator=(const teObject& in) { info = in.info; return *this; }
    virtual ~teObject();

    /// Disconnects everything and runs pending `destroy` callbacks. Idempotent.
    void onDestroy();

    void checkDeleteLater();
    void deleteLater() { deleteLaterFlag = true; }
    bool ifDeleteLater() const { return deleteLaterFlag; }

    template<typename OBJT, typename T, typename RET, typename... Args, typename... ArgTypes>
        requires std::derived_from<OBJT, T>
    teCallbackPtr teConnect(teCallbackType when, OBJT* reciver, RET (T::*in_func_ptr)(Args...), ArgTypes... args);

    template<typename OBJT, typename T, typename RET, typename... Args, typename... ArgTypes>
        requires std::derived_from<OBJT, T>
    teCallbackPtr teConnect(teCallbackType when, QString in_info, OBJT* reciver, RET (T::*in_func_ptr)(Args...), ArgTypes... args);

    /// Disconnects connections with `obj`. `obj == nullptr` disconnects everything.
    /// `type < 0` ignores the callback type.
    void teDisconnect(teObject* obj = nullptr, int type = -1);

    /// Invokes every connected callback of `calltype`.
    void teemit(teCallbackType calltype, bool autodelete = true);

private:
    /// Detaches every connection whose callback type differs from `keepType`.
    /// Caller must hold teCallbackMutex().
    void teDisconnectExceptLocked(int keepType);

    std::atomic<bool> deleteLaterFlag{false};
    std::atomic<bool> onDestroyCalled{false};
};

class obj_callback_function_base {
public:
    int type;
    QString info;
    /// Set to false by teDisconnect(); a dispatch in flight checks it so a
    /// callback disconnected during the same emit is not invoked.
    std::atomic<bool> connected{true};

    explicit obj_callback_function_base(int in_type) : type(in_type) {}
    virtual void operator()() = 0;
    virtual ~obj_callback_function_base() = default;
};

template <typename T, typename RET, typename... Args>
class obj_callback_function : public obj_callback_function_base {
public:
    T* obj_ptr;
    RET (T::*func_ptr)(Args...);
    std::tuple<Args...> params;

    obj_callback_function(int type, T* reciver, RET (T::*in_func_ptr)(Args...), Args... args)
        : obj_callback_function_base(type), obj_ptr(reciver), func_ptr(in_func_ptr), params(std::make_tuple(args...)) {}

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

template<typename OBJT, typename T, typename RET, typename... Args, typename... ArgTypes>
    requires std::derived_from<OBJT, T>
teCallbackPtr teObject::teConnect(teCallbackType when, OBJT* reciver, RET (T::*in_func_ptr)(Args...), ArgTypes... args) {
    auto new_callback_func = std::make_shared<obj_callback_function<T, RET, Args...>>(
        when, static_cast<T*>(reciver), in_func_ptr, static_cast<Args>(args)...);
    std::lock_guard<std::recursive_mutex> lg(teCallbackMutex());
    linked_callback_call.insert({static_cast<teObject*>(reciver), new_callback_func});
    reciver->linked_callback_recive.insert({this, new_callback_func});
    return new_callback_func;
}

template<typename OBJT, typename T, typename RET, typename... Args, typename... ArgTypes>
    requires std::derived_from<OBJT, T>
teCallbackPtr teObject::teConnect(teCallbackType when, QString in_info, OBJT* reciver, RET (T::*in_func_ptr)(Args...), ArgTypes... args) {
    auto new_callback_func = std::make_shared<obj_callback_function<T, RET, Args...>>(
        when, static_cast<T*>(reciver), in_func_ptr, static_cast<Args>(args)...);
    new_callback_func->info = std::move(in_info);
    std::lock_guard<std::recursive_mutex> lg(teCallbackMutex());
    linked_callback_call.insert({static_cast<teObject*>(reciver), new_callback_func});
    reciver->linked_callback_recive.insert({this, new_callback_func});
    return new_callback_func;
}

#endif // TEOBJECT_H
