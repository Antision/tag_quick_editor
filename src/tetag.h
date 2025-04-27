#ifndef TETAG_H
#define TETAG_H
#include"pch.h"
#include"suggestionlineedit.h"
#define telog(x) qDebug()<<x

struct teWord;
struct teTagBase;
struct obj_callback_function_base;

typedef struct teWordCore:teObject{
    QString text;
    teWord*widget=nullptr;
    teWordCore(){}
    teWordCore(const QString& input_string){
        text=input_string;
    }
    teWordCore(QString&& input_string){
        text=std::move(input_string);
    }
    teWordCore(const teWordCore& in_core){
        text = in_core.text;
    }
    teWord* load();
    void unload();
    bool operator==(const teWordCore& in) const{
        return text == in.text;
    }
    bool operator==(const QString& in) const{
        return text == in;
    }
    operator QString(){
        return text;
    }
    ~teWordCore();
}tewordcore;


typedef struct teWordBase:public QLabel,public teObject{
    Q_OBJECT
public:
    tewordcore*core=nullptr;
    teWordBase(QWidget*parent=nullptr):QLabel(parent){initialize();}
    teWordBase(const QString& input_string,QWidget* parent=nullptr):
        QLabel(input_string,parent){initialize();}
    teWordBase(QString&& input_string,QWidget* parent=nullptr):
        QLabel(input_string,parent)
    {initialize();}
    teWordBase(tewordcore&in):QLabel(in.text),core(&in)
    {initialize();}
    teWordBase(const teWordBase&in):QLabel(in.core->text),core(in.core)
    {initialize();}
    teWordBase(teWordBase&&in)=delete;
    virtual void setText(const QString&text){
        if(!core)telog("[teRefWord::setText]:no core specified for this refword");
        core->text=text;
        QLabel::setText(text);
    }
    void initialize();
    virtual void readCore(tewordcore*in);
    bool event(QEvent*e)override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event)override;
    bool isDragging;
    int start_x;
    int mousePosInWidget;
    bool operator==(const teWordBase& in) const{
        return core->text == in.core->text;
    }
    bool operator==(const QString& in) const{
        return core->text == in;
    }
    teWordBase& operator=(QString&&in){
        core->text = std::move(in);
        QLabel::setText(core->text);
        return *this;
    }
    teWordBase& operator=(const teWordBase&in){
        core->text = in.core->text;
        QLabel::setText(core->text);
        return *this;
    }
    teWordBase& operator=(teWordBase&&in){
        core->text = std::move(in.core->text);
        QLabel::setText(core->text);
        return *this;
    }
    operator std::string() const{
        if(!core) telog("core is nullptr");
        return core->text.toStdString();
    };
    operator QString() const{
        if(!core) telog("core is nullptr");
        return core->text;
    };
    tewordcore& get_core(){
        return *core;
    }
    virtual ~teWordBase(){
        if(core!=nullptr){
            core->widget=nullptr;
            core=nullptr;
        }
    }
signals:
    void mouseDoubleClicked(teWordBase*);
    void droped(teWordBase*,int xpos);
}tewordbase;

typedef struct teWord:public teWordBase{
    teWord(QWidget*parent=nullptr):teWordBase(parent){}
    ~teWord(){

    }
    teWord(const QString& input_string,QWidget* parent=nullptr):teWordBase(input_string,parent){
        core=new tewordcore(input_string);
        core->widget=this;
    }
    teWord(QString&& input_string,QWidget* parent=nullptr):teWord(input_string,parent){
        core=new tewordcore(std::move(input_string));
        core->widget=this;
    }
    teWord(tewordcore&in):teWordBase(in){
        in.widget=this;
    }
    teWord(const teWordBase&in):teWordBase(in){
        core->widget=this;
    }

    teWord(teWordBase&&in)=delete;
    virtual void readCore(tewordcore*in)override{
        teWordBase::readCore(in);
        in->widget=this;
    }

}teword;
typedef struct teRefWord:public teWordBase{
    teRefWord(QWidget*parent=nullptr):teWordBase(parent){initialize();}
    teRefWord(const QString& input_string,QWidget* parent=nullptr)=delete;
    teRefWord(QString&& input_string,QWidget* parent=nullptr)=delete;
    teRefWord(tewordcore&in_word):teWordBase(in_word){
        in_word.teConnect(teCallbackType::edit,this,&teRefWord::readCore,&in_word);
        initialize();
    }
    teRefWord(const teWordBase&in):teWordBase(in){initialize();}
    teRefWord(teWordBase&&in)=delete;
    void initialize(){
        setMaximumHeight(20);
    }
}terefword;
typedef struct teTagCore:teObject,std::enable_shared_from_this<teTagCore>{
public:
    enum teTagType{
        other=0,
        sentence,
        deleteTag
    };

    QList<tewordcore*> words;
    teTag* widget=nullptr;
    int weight=99;
    teTagType type = other;
    teTagCore(){

    }
    teTagCore(bool iftmp){
        if(iftmp)info=QStringLiteral("tmp tagcore");
    }
    teTagCore(const QList<teWordCore*>in,teTag*child=nullptr);
    teTagCore(QList<teWordCore*>&&in,teTag*child=nullptr):words(std::move(in)),widget(child){}
    teTagCore(const QString &str,teTag*child=nullptr);
    teTagCore(const char* str,teTag*child=nullptr);
    teTagCore(const teTagCore&in):weight(in.weight),type(in.type){
        info=in.info;
        for(tewordcore*wc:in.words)
            words.push_back(new tewordcore{*wc});
    }
    teTagCore(teTagCore&&in);
    void load();
    void unload();
    void read(const QString &str,bool ifclear=true);
    teWordCore* takeWordAt(int index,bool ifSendSignal=true);
    QList<tewordcore*>::iterator begin(){
        return words.begin();
    }
    QList<tewordcore*>::iterator end(){
        return words.end();
    }
    QList<tewordcore*>::const_iterator begin() const{
        return words.begin();
    }
    QList<tewordcore*>::const_iterator end() const {
        return words.end();
    }
    void clear(){
        for(auto word:words)
            delete word;
        words.clear();
    }
    bool contains(const QString&str)const{
        for(const tewordcore*wc:words)
            if(wc->text==str)
                return true;
        return false;
    }
    teTagCore& operator=(const teTagCore&in);
    bool operator== (const teTagCore&in)const;
    operator QString() const;
    void edited(){
        teemit(teCallbackType::edit);
    };
    void edited_with_layout(){
        teemit(teCallbackType::edit_with_layout);
    }
    ~teTagCore();
}tetagcore;
struct teInputWidget;
struct teEditorControl;

typedef struct teTagBase: public QFrame,public teObject{
    Q_OBJECT
public:
    teTagBase(){ }
    virtual ~teTagBase();
    teTagBase(QWidget*parent):QFrame(parent){initialize();}
    teTagBase(std::shared_ptr<tetagcore>incore,QWidget*parent=nullptr);
    std::shared_ptr<teTagCore> core{};
    QHBoxLayout* layout=nullptr;

    std::multimap<teEditorControl*,QWidget*>extra_widgets;
    void reset(){
        core.reset();
    }
    virtual void clearWordWidgets();
    void takeWordWidgets();
    void clear(){core->clear();}
    void clearExtraWidgets(bool ifnotify=true){
        for(auto&[e,w]:extra_widgets)
            delete w;
        extra_widgets.clear();
        for(auto&[obj,func]:linked_callback_call){
            if(ifnotify&&func->type==teCallbackType::extraWidget_removed)
                (*func)();
            teDisconnect(obj,teCallbackType::extraWidget_removed);
        }
    }
    void clearExtraWidgets(teEditorControl*e,bool ifnotify=true){
        auto[begin,end] = extra_widgets.equal_range(e);
        auto it = begin;
        for(;it!=end;++it)
            delete it->second;
        extra_widgets.erase(e);
        if(ifnotify){
            auto[begin,end] = linked_callback_call.equal_range((teObject*)e);
            for(auto it = begin;it!=end;++it){
                if(it->second->type==teCallbackType::extraWidget_removed)
                    (*it->second)();
                teDisconnect(it->first,teCallbackType::extraWidget_removed);
            }
        }
    }
    void insertExtraWidgets(teEditorControl*e,QWidget*w){
        extra_widgets.insert({e,w});
        layout->addWidget(w);
    }
    virtual void initialize();
    void dragEnterEvent(QDragEnterEvent *event) override {
        if (event->mimeData()->hasText()) {
            event->acceptProposedAction();
        }
    }

    enum tetagStyle{
        normal,
        select,
        select_current,
        multi,
        multi_select,
        multi_select_current,
    };
    void setStyle(tetagStyle in);
    virtual void readCore(std::shared_ptr<tetagcore>in_core)=0;
    virtual void setText(const QString& str){
        if(core==nullptr){
            core = std::make_shared<tetagcore>(str);
        }
        else
            core->read(str,true);
    }
    void destroyWord(int index,bool ifSendSignal=true){
        while(index<0)index += core->words.size();
        delete core->words[index];
        core->words.erase(core->words.begin()+index);
        if(ifSendSignal)core->edited_with_layout();
    }
    teWordCore* takeWordAt(int index,bool ifSendSignal=true){
        while(index<0)index += core->words.size();
        teWordCore* wc = core->words.takeAt(index);
        if(wc->widget){
            wc->widget->teDisconnect(this);
            QApplication::disconnect(wc->widget,0,this,0);
        }
        if(ifSendSignal)
            core->edited_with_layout();
        return wc;
    }
    void insertWord(int index,QString string ,bool ifSendSignal=true){
        while(index<0)index += core->words.size()+1;
        tewordcore*wordcore = new tewordcore{std::move(string)};
        core->words.insert(index,wordcore);
        wordcore->load();
        layout->insertWidget(index,wordcore->widget);
        connectWord(wordcore->widget);
        if(ifSendSignal)core->edited_with_layout();
    }
    void insertWord(int index,teWordCore* inwc, bool ifconnect,bool ifSendSignal=true){
        while(index<0)index += core->words.size()+1;
        core->words.insert(index,inwc);
        if(!inwc->widget)
            inwc->load();
        layout->insertWidget(index,inwc->widget);
        if(ifconnect)
            connectWord(inwc->widget);
        if(ifSendSignal)core->edited_with_layout();
    }
    void connectWord(teWordBase*inw){
        connect(inw,&teWordBase::droped,this,&teTagBase::worddroped);
        connect(inw,&teWordBase::mouseDoubleClicked,this,[this](teWordBase*inw){
            emit mouseDoubleClicked(this,inw);},Qt::DirectConnection
                );
    }
    void disconnectWord(teWordBase*inw=nullptr){
        if(!inw){
            for(tewordcore*wc:core->words)
                if(wc->widget){
                    disconnect(wc->widget,0,this,0);
                    wc->teDisconnect(this);
                }
        }else{
            disconnect(inw,0,this,0);
            inw->teDisconnect(this);
        }
    }
    virtual void load()=0;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event)override {
        event->ignore();
    }
    void keyReleaseEvent(QKeyEvent *event)override {
        event->ignore();
    }
    void mouseDoubleClickEvent(QMouseEvent *event)override{
        emit mouseDoubleClicked(this,nullptr);
    }
    teTagBase& operator=(const QString& input_string);
    teTagBase &operator=(const teTagBase &in);
    bool operator== (const teTag&in)const;
    teTagCore& getcore()const{return *core;}
    virtual operator QString() const {return *core;}
    int fit_goodness(const QString& find_word,int*return_index=nullptr,int*return_questionable_index=nullptr) const;
    QList<teWordCore*>::iterator begin(){return core->words.begin();}
    QList<teWordCore*>::iterator end(){return core->words.end();}
    QList<teWordCore*>::const_iterator begin() const{return core->words.begin();}
    QList<teWordCore*>::const_iterator end() const {return core->words.end();}
    bool isDragging;
    int start_y;
    int mousePosInWidget;
signals:
    void rightButtonPress(teTagBase*,QPoint,int);
    void leftButtonPress(teTagBase*,QPoint,int);
    void droped(teTagBase*,int);
    void mouseDoubleClicked(teTagBase*,teWordBase*);
public slots:
    virtual void worddroped(teWordBase*in_word,int xpos);
}tetagbase;

typedef struct teTag:public teTagBase
{
    Q_OBJECT
public:
    teTag(QWidget*parent=nullptr):teTagBase(parent){setStyle(teTag::normal);}
    teTag(const QString &str,QWidget*parent=nullptr);
    teTag(std::shared_ptr<tetagcore>incore,QWidget*parent=nullptr);
    virtual void readCore(std::shared_ptr<tetagcore>in_core)override;
    void load()override;
} tetag;
struct teTagOperation{
public:
    enum teOperationType{
        taginsert,
        tagedit,
        tagedit_type,
        tagedit_weight,
        tagmove,
        tagerase
    };
    std::weak_ptr<tetagcore> tag_ptr;
    teOperationType type;
    teTagCore prevCore{true};
    teTagCore nowCore{true};
    int idp=-1;
    int idn=-1;
};

class teOperationList{
public:
    QVector<teTagOperation>operations;
    int inip=0;
    int rwp=0;
    int end=0;
    void addEditOperation(std::shared_ptr<tetagcore> in_ptr,teTagCore in_now){
        if(rwp==operations.size())
            operations.push_back({});
        teTagOperation&newoperation=operations[rwp++];
        end=rwp;
        newoperation.type=teTagOperation::tagedit;
        newoperation.tag_ptr=in_ptr;
        for(int i = operations.size()-1;i>-1;--i)
            if(operations[i].tag_ptr.lock()==in_ptr&&operations[i].type==teTagOperation::tagedit_type){
                newoperation.prevCore=operations[i].nowCore;
                break;
            }
        newoperation.nowCore=in_now;
    }
    void addInsertOperation(std::shared_ptr<tetagcore> in_ptr,int id,teTagCore in_now){
        if(rwp==operations.size())
            operations.push_back({});
        teTagOperation&newoperation=operations[rwp++];
        end=rwp;
        newoperation.type=teTagOperation::taginsert;
        newoperation.idp=-1;
        newoperation.idn=id;
        newoperation.tag_ptr=in_ptr;
        newoperation.nowCore=in_now;
    }
    void addEraseOperation(std::shared_ptr<tetagcore> in_ptr,int id,teTagCore in_prev){
        if(rwp==operations.size())
            operations.push_back({});
        teTagOperation&newoperation=operations[rwp++];
        end=rwp;
        newoperation.type=teTagOperation::tagerase;
        newoperation.idp=id;
        newoperation.idn=-1;
        newoperation.tag_ptr=in_ptr;
        newoperation.prevCore=in_prev;
    }
    void addMoveOperation(std::shared_ptr<tetagcore> in_ptr,int idp,int idn){
        if(rwp==operations.size())
            operations.push_back({});
        teTagOperation&newoperation=operations[rwp++];
        end=rwp;
        newoperation.type=teTagOperation::tagmove;
        newoperation.idp=idp;
        newoperation.idn=idn;
        newoperation.tag_ptr=in_ptr;
    }
    void clear(){
        operations.clear();
        inip=0;
        rwp=0;
        end=0;
    }
    int previousEditOperation(std::shared_ptr<tetagcore>tag){
        int tmpp=rwp-1;
        while(tmpp>-1){
            if(operations[tmpp].tag_ptr.lock()==tag&&(operations[tmpp].type==teTagOperation::tagedit||operations[tmpp].type==teTagOperation::taginsert)){
                return tmpp;
            }
            --tmpp;
        }
        return tmpp;
    }
    void replaceTag(std::shared_ptr<tetagcore>prev_tag,std::shared_ptr<tetagcore>new_tag){
        for(teTagOperation&op:operations){
            if(op.tag_ptr.lock()==prev_tag)
                op.tag_ptr=new_tag;
        }
    }
    teTagOperation&take(){
        if(rwp<=inip)throw std::exception("there's no step to undo");
        return operations[--rwp];
    }
    teTagOperation&forward(){
        if(rwp>=end)throw std::exception("there's no step to redo");
        return operations[rwp++];
    }
};

struct teTagList:public QObject,public teObject{
    Q_OBJECT
    friend struct teTagListWidget;
    friend struct tePictureFile;
public:
    teTagList(){};
    bool receiveTagSignals=true;
    bool isTagsLoaded=false;
    bool isWidgetLoaded=false;
    bool isSaved=true;
    teTagList(const teTagList& in):
        tags(in.tags){}
    teTagList(teTagList&&in):
        tags(std::move(in.tags)){}

    /* When invoking member functions for addition, deletion, or modification,
     * nested calls to these addition/deletion/modification member functions may be triggered.
     * Each time these functions are called, the member mutex tagsMt is locked. Therefore,
     * the locked_for_edit member variable is used for counting.
     * When it is detected that the lock is being invoked by an addition/deletion/modification member function,
     * redundant locking is avoided to prevent deadlock issues caused by
     * nested calls to member functions within a single thread.
     */
    std::mutex tagsMt;
    bool inner_lock(){
        if(!locked_for_edit){
            tagsMt.lock();
            ++locked_for_edit;
            return true;
        }else return false;
    }
    void inner_unlock(){
        --locked_for_edit;
        tagsMt.unlock();
    }
    int initialize_push_back(std::shared_ptr<tetagcore>);
    int initialize_push_back(const QString&);
    int initialize_push_back(const std::string&in);
    void connectTag(std::shared_ptr<tetagcore>tag){
        tag->teConnect(teCallbackType::edit_with_layout,this,&teTagList::onTagEdited,tag,true);
    }
    void disconnectTag(std::shared_ptr<tetagcore>tag){
        tag->teDisconnect(this);
    }
    void onTagEdited(std::shared_ptr<tetagcore> tag,bool ifemit=true){
        isSaved=false;
        if(receiveTagSignals){
            operationlist.addEditOperation(tag,*tag);
        }
        else receiveTagSignals =true;
        if(ifemit){
            emit tagEdited(tag);
        }
    }
    void onTagErased(std::shared_ptr<tetagcore> tag,int id,bool ifemit=true){
        isSaved=false;
        if(receiveTagSignals){
            operationlist.addEraseOperation(tag,id,*tag);
        }
        else receiveTagSignals = true;
        if(ifemit){
            emit tagErased(tag);
            teemit(teCallbackType::edit);
        }
    }
    void onTagMoved(std::shared_ptr<tetagcore> tag,int prev,int now){
        isSaved=false;
        if(receiveTagSignals){
            operationlist.addMoveOperation(tag,prev,now);
        }
        else receiveTagSignals =true;
    }
    void onTagInserted(std::shared_ptr<tetagcore> tag,int pos,bool ifemit=true){
        isSaved=false;
        if(receiveTagSignals){
            operationlist.addInsertOperation(tag,pos,*tag);
        }
        else receiveTagSignals =true;
        if(ifemit){
            emit tagInserted(tag);
            teemit(teCallbackType::edit);
        }
    }
    void load(){
        if(isWidgetLoaded)return;
        for(std::shared_ptr<tetagcore> tag:tags){
            tag->load();
        }
        isWidgetLoaded=true;
    }
    void unload(){
        if(!isWidgetLoaded)return;
        for(std::shared_ptr<tetagcore> tag:tags){
            tag->unload();
        }
        isWidgetLoaded=false;
    }
    void clear(){
        tags.clear();
        operationlist.clear();
    }
    tetagcore& operator[](int index){
        return *tags[index];
    }
    bool remove_duplicate(std::shared_ptr<tetagcore> tag=nullptr,bool keepself=false){
        if(tag==nullptr&&keepself==true)
            telog("[teTagList::removeduplicate]:tag==nullptr&&keepself==true");
        int tagcount = tags.size();
        bool ret=false;
        if(tag!=nullptr){
            for(int i =0; i<tagcount;++i){
                if(*tags[i]==*tag&&tags[i]!=tag)
                {
                    if(keepself){
                        erase(i);
                        --i;
                        --tagcount;
                        ret=true;
                    }
                    else{
                        return true;
                    }
                }
            }
            return ret;
        }
        else{
            for(int i =0;i<tagcount;++i){
                for(int j = i+1 ;j<tagcount;++j){
                    if(*tags[i]==*tags[j]){
                        erase(j);
                        --j;
                        --tagcount;
                        ret = true;
                    }
                }
            }
            return ret;
        }
    }
    int insert(int pos,std::shared_ptr<tetagcore>tag,int removeDuplicate=1,bool ifSendSignal=true);

    void erase(int id){
        bool iflocked=inner_lock();
        std::shared_ptr<tetagcore>core = tags.takeAt(id);
        if(iflocked)inner_unlock();
        onTagErased(core,id,isTagsLoaded);
        core->unload();
        core->teDisconnect(this);
        core->teemit(teCallbackType::destroy,false);
    }
    void erase(std::shared_ptr<tetagcore>core){
        int index = tags.indexOf(core);
        if(index<0){
            telog("Can't find the pointer in taglist");
            return;
        }
        erase(tags.indexOf(core));
    }

    int edit(std::shared_ptr<tetagcore>core,QString text,int removeDuplicate=1,bool ifemit=true){
        bool iflocked=inner_lock();
        core->read(text);
        if(iflocked)inner_unlock();
        int ret=0;
        if(removeDuplicate==1&&remove_duplicate(core,false)){
            return -1;
        }else if(removeDuplicate==2&&remove_duplicate(core,true)){
            ret=-1;
        }
        onTagEdited(core,ifemit);
        if(core->type==teTagCore::deleteTag)
            ret=-1;
        return ret;
    }
    int edit(int index,QString text,int removeDuplicate=1,bool ifemit=true){
        bool iflocked=inner_lock();
        std::shared_ptr<tetagcore> core = tags.at(index);
        core->read(text);
        if(iflocked)inner_unlock();
        int ret=0;
        if(removeDuplicate==1&&remove_duplicate(core,false)){
            return -1;
        }else if(removeDuplicate==2&&remove_duplicate(core,true)){
            ret=-1;
        }
        onTagEdited(core,ifemit);
        return ret;
    }

    void move(int originPos,int newPos){
        bool iflocked=inner_lock();
        std::shared_ptr<tetagcore>taketag = tags.takeAt(originPos);
        // if(originPos<newPos)
        //     --newPos;
        tags.insert(newPos,taketag);
        if(iflocked)inner_unlock();
        onTagMoved(taketag,originPos,newPos);
    }
    void move(std::shared_ptr<tetagcore>tag,int newPos){
        int o = tags.indexOf(tag);
        if(o>-1){
            bool iflocked=inner_lock();
            tags.insert(newPos,tags.takeAt(o));
            if(iflocked)inner_unlock();
            onTagMoved(tag,o,newPos);
        }
        else
            telog("[teTagList::move]:could not find input tag in taglist");
    }

    int find(std::shared_ptr<tetagcore>in){
        return tags.indexOf(in);
    }
    virtual ~teTagList(){
        tags.clear();
        onDestroy();
    }
    QVector<std::shared_ptr<tetagcore>> getTags(){
        return tags;
    }
    QList<std::shared_ptr<tetagcore>>::Iterator begin(){
        return tags.begin();
    }
    QList<std::shared_ptr<tetagcore>>::Iterator end(){
        return tags.end();
    }
    QList<std::shared_ptr<tetagcore>>::const_iterator begin()const {
        return tags.begin();
    }
    QList<std::shared_ptr<tetagcore>>::const_iterator end()const {
        return tags.end();
    }
    size_t size() const{
        return tags.size();
    }
    std::shared_ptr<tetagcore>& back(){
        return tags.back();
    }
    std::shared_ptr<tetagcore>const& back()const{
        return tags.back();
    }
protected:
    QVector<std::shared_ptr<tetagcore>> tags;
    teOperationList operationlist;
    int firstoperations=0;
private:
    int locked_for_edit=0;
signals:
    void tagInserted(std::shared_ptr<tetagcore>);
    void tagEdited(std::shared_ptr<tetagcore>);
    void tagErased(std::shared_ptr<tetagcore>);
};

#endif // TETAG_H
