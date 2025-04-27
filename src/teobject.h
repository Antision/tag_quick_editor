#ifndef TEOBJECT_H
#define TEOBJECT_H
struct obj_callback_function_base;
bool teobj_caller_callback_cmp_func(obj_callback_function_base*a,obj_callback_function_base*b);
bool teobj_reciver_callback_cmp_func(obj_callback_function_base*a,obj_callback_function_base*b);
enum teCallbackType{
    edit=0,
    edit_with_layout,
    destroy,
    ready_destroy,
    insert,
    loading_finished,
    extraWidget_removed
};

struct teObject{
    std::multimap<teObject*,obj_callback_function_base*>linked_callback_call;
    std::multimap<teObject*,obj_callback_function_base*>linked_callback_recive;
    QString info;
    bool onDestroyCalled=false;
    teObject();
    void onDestroy();
    virtual ~teObject();
    void checkDeleteLater();
    void deleteLater(){
        deleteLaterFlag=true;
    }
    bool ifDeleteLater(){
        return deleteLaterFlag;
    }
    template<typename OBJT ,typename T, typename RET, typename... Args,typename ...ArgTypes> requires std::derived_from<OBJT,T>
    obj_callback_function_base *teConnect(teCallbackType when,OBJT* reciver, RET (T::*in_func_ptr)(Args...), ArgTypes... args);

    template<typename OBJT ,typename T, typename RET, typename... Args,typename ...ArgTypes>requires std::derived_from<OBJT,T>
    obj_callback_function_base *teConnect(teCallbackType when, QString in_info, OBJT *reciver, RET (T::*in_func_ptr)(Args...), ArgTypes...args);
    void teDisconnect(teObject* obj=nullptr,int type=-1);
    void teemit(teCallbackType,bool autodelete=true);

private:
    bool deleteLaterFlag=false;
};

extern std::set<int> funcList;
extern int totalcount;
struct obj_callback_function_base {
    int type;
    QString info;
    obj_callback_function_base(int type):type(type){
        funcList.insert(totalcount);
    }
    virtual void operator()() = 0;
    virtual ~obj_callback_function_base(){
    }
};

template <typename T, typename RET, typename... Args>
struct obj_callback_function : obj_callback_function_base {
    T* obj_ptr;
    RET (T::*func_ptr)(Args...);
    std::tuple<Args...> params;
    virtual ~obj_callback_function(){
    }
    obj_callback_function(int type,T* reciver, RET (T::*in_func_ptr)(Args...), Args... args)
        :obj_callback_function_base(type),obj_ptr(reciver), func_ptr(in_func_ptr), params(std::make_tuple(args...)) {
    }
    template <std::size_t... Indexes>
    RET call_impl(std::index_sequence<Indexes...>){
        return (obj_ptr->*func_ptr)(std::get<Indexes>(params)...);
    }
    virtual void operator()() override{
        call_impl(std::make_index_sequence<sizeof...(Args)>{});
    }
};
template<typename OBJT ,typename T, typename RET, typename... Args,typename ...ArgTypes> requires std::derived_from<OBJT,T>
obj_callback_function_base *teObject::teConnect(teCallbackType when,OBJT* reciver, RET (T::*in_func_ptr)(Args...), ArgTypes... args){
    obj_callback_function_base * new_callback_func = new obj_callback_function(when,static_cast<T*>(reciver),in_func_ptr,static_cast<Args>(args)...);
    linked_callback_call.insert({reciver,new_callback_func});
    reciver->linked_callback_recive.insert({this,new_callback_func});
    return new_callback_func;
}
template<typename OBJT ,typename T, typename RET, typename... Args,typename ...ArgTypes> requires std::derived_from<OBJT,T>
obj_callback_function_base *teObject::teConnect(teCallbackType when,QString in_info,OBJT* reciver, RET (T::*in_func_ptr)(Args...), ArgTypes... args){
    obj_callback_function_base * new_callback_func = new obj_callback_function(when,static_cast<T*>(reciver),in_func_ptr,static_cast<Args>(args)...);
    new_callback_func->info=std::move(in_info);
    linked_callback_call.insert({reciver,new_callback_func});
    reciver->linked_callback_recive.insert({this,new_callback_func});
    return new_callback_func;
}

#endif // TEOBJECT_H
