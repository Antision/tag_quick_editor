#include "teobject.h"
template<typename T,typename U>
void erase_specified_keyvalue(std::multimap<T,U>&mmap,const T key,const U value){
    auto range = mmap.equal_range(key);
    for (auto it = range.first; it != range.second; ) {
        if (it->second == value) {
            it = mmap.erase(it);
            break;
        } else {
            ++it;
        }
    }
}
void teObject::teDisconnect(teObject* obj,int type){
    if(obj==nullptr){
        for(auto&[obj,func]:linked_callback_call){
            delete func;
            if(obj){
                obj->linked_callback_recive.erase(this);
            }
        }
        linked_callback_call.clear();
        for(auto&[obj,func]:linked_callback_recive){
            delete func;
            if(obj){
                obj->linked_callback_call.erase(this);
            }
        }
        linked_callback_recive.clear();
    }else{
        {
            if(type<0){
                auto[rg_begin,rg_end] = linked_callback_call.equal_range(obj);
                for(;rg_begin!=rg_end;++rg_begin){
                    delete rg_begin->second;
                }
                linked_callback_call.erase(obj);
                obj->linked_callback_recive.erase(this);
            }else{
                auto it =linked_callback_call.lower_bound(obj);
                auto upper = linked_callback_call.upper_bound(obj);
                while(it!=upper&&upper!=linked_callback_call.end()){
                    if(it->second->type==type){
                        delete it->second;
                        erase_specified_keyvalue<teObject*,obj_callback_function_base*>(it->first->linked_callback_recive,this,it->second);
                        it=linked_callback_call.erase(it);
                        upper=linked_callback_call.upper_bound(obj);
                    }else{
                        ++it;
                    }
                }
            }
        }{
            if(type<0){
                auto[rg_begin,rg_end] = linked_callback_recive.equal_range(obj);
                for(;rg_begin!=rg_end;++rg_begin){
                    delete rg_begin->second;
                }
                linked_callback_recive.erase(obj);
                obj->linked_callback_call.erase(this);
            }
        }
    }
}
void teObject::teemit(teCallbackType calltype,bool autodelete){
    std::vector<std::pair<teObject*,obj_callback_function_base*>> snapshot(linked_callback_call.begin(), linked_callback_call.end());
    for(auto[obj,func]:snapshot){
        if(func->type!=calltype||(linked_callback_call.find(obj)==linked_callback_call.end())){
            continue;
        }
        (*func)();
    }
    if(autodelete&&calltype!=ready_destroy&&calltype!=destroy)
        checkDeleteLater();
}
teObject::teObject(){
}

void teObject::onDestroy(){
    if(onDestroyCalled)return;
    onDestroyCalled=true;
    obj_callback_function_base* last_call;
    while(!linked_callback_call.empty()){
        auto[obj,func] = *linked_callback_call.begin();
        if(obj&&last_call!=func){
            obj->linked_callback_recive.erase(this);
        }
        if(last_call==func||func->type!=teCallbackType::destroy){
            delete func;
            linked_callback_call.erase(linked_callback_call.begin());
        }else if(func->type==teCallbackType::destroy){
            last_call=func;
            (*func)();
        }
    }
    for(auto&[obj,func]:linked_callback_recive){
        if(obj){
            obj->linked_callback_call.erase(this);
        }
        delete func;
    }
    linked_callback_recive.clear();
}
int totalcount=0;
std::set<int> funcList;
teObject::~teObject(){
    onDestroy();
}

void teObject::checkDeleteLater(){
    if(deleteLaterFlag){
        delete this;
    }
}



