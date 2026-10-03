#ifndef TETAG_H
#define TETAG_H
#include"pch.h"
#include"suggestionlineedit.h"

class teWordWidget;
class teTagWidgetBase;
class obj_callback_function_base;

class teWord:public teObject{
public:
    QString text;
    teWordWidget*widget=nullptr;
    /**
     * @brief True while a model/view based tag list shows this word.
     *
     * Such a list draws its rows through a delegate, so load() must not build a
     * widget for the word: the pooled widget ended up parked under the view and
     * was visible (and draggable) on top of the tag list. Set together with the
     * tag's own flag by teTagListView::load().
     */
    bool ifViewOwned=false;
    teWord(){}
    teWord(const QString& input_string){
        text=input_string;
    }
    teWord(QString&& input_string){
        text=std::move(input_string);
    }
    teWord(const teWord& in_core){
        text = in_core.text;
    }
    teWordWidget* load();
    void unload();
    /// Makes sure the word is displayed, i.e. that it owns a widget.
    /// See teTagCore::ensureWidget().
    void ensureWidget(){
        if(!widget)
            load();
    }
    bool operator==(const teWord& in) const{
        return text == in.text;
    }
    bool operator==(const QString& in) const{
        return text == in;
    }
    operator QString(){
        return text;
    }
    ~teWord();
};

class teWordWidgetBase:public QLabel,public teObject{
    Q_OBJECT
public:
    teWord*core=nullptr;
    teWordWidgetBase(QWidget*parent=nullptr):QLabel(parent){initialize();}
    teWordWidgetBase(const QString& input_string,QWidget* parent=nullptr):
        QLabel(input_string,parent){initialize();}
    teWordWidgetBase(QString&& input_string,QWidget* parent=nullptr):
        QLabel(input_string,parent)
    {initialize();}
    teWordWidgetBase(teWord&in):QLabel(in.text),core(&in)
    {initialize();}
    teWordWidgetBase(const teWordWidgetBase&in):QLabel(in.core->text),core(in.core)
    {initialize();}
    teWordWidgetBase(teWordWidgetBase&&in)=delete;
    virtual void setText(const QString&text){
        if(!core)telog("[teRefWord::setText]:no core specified for this refword");
        core->text=text;
        QLabel::setText(text);
    }
    void initialize();
    virtual void readCore(teWord*in);
    bool event(QEvent*e)override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event)override;
    bool isDragging=false;
    int start_x=0;
    int mousePosInWidget=0;
    /// Set by the widget pool while this widget waits to be reused. It is the
    /// guard against registering the same widget twice (which would hand it out
    /// to two owners at once).
    bool inWidgetPool=false;
    bool operator==(const teWordWidgetBase& in) const{
        return core->text == in.core->text;
    }
    bool operator==(const QString& in) const{
        return core->text == in;
    }
    teWordWidgetBase& operator=(QString&&in){
        core->text = std::move(in);
        QLabel::setText(core->text);
        return *this;
    }
    teWordWidgetBase& operator=(const teWordWidgetBase&in){
        core->text = in.core->text;
        QLabel::setText(core->text);
        return *this;
    }
    teWordWidgetBase& operator=(teWordWidgetBase&&in){
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
    teWord& get_core(){
        return *core;
    }
    virtual ~teWordWidgetBase(){
        if(core!=nullptr){
            core->widget=nullptr;
            core=nullptr;
        }
    }
signals:
    void mouseDoubleClicked(teWordWidgetBase*);
    void droped(teWordWidgetBase*,int xpos);
};

class teWordWidget:public teWordWidgetBase{
public:
    teWordWidget(QWidget*parent=nullptr):teWordWidgetBase(parent){}
    ~teWordWidget(){

    }
    teWordWidget(const QString& input_string,QWidget* parent=nullptr):teWordWidgetBase(input_string,parent){
        core=new teWord(input_string);
        core->widget=this;
    }
    teWordWidget(QString&& input_string,QWidget* parent=nullptr):teWordWidget(input_string,parent){
        core=new teWord(std::move(input_string));
        core->widget=this;
    }
    teWordWidget(teWord&in):teWordWidgetBase(in){
        in.widget=this;
    }
    teWordWidget(const teWordWidgetBase&in):teWordWidgetBase(in){
        core->widget=this;
    }

    teWordWidget(teWordWidgetBase&&in)=delete;
    virtual void readCore(teWord*in)override{
        teWordWidgetBase::readCore(in);
        in->widget=this;
    }

};

class teRefWordWidget:public teWordWidgetBase{
public:
    teRefWordWidget(QWidget*parent=nullptr):teWordWidgetBase(parent){initialize();}
    teRefWordWidget(const QString& input_string,QWidget* parent=nullptr)=delete;
    teRefWordWidget(QString&& input_string,QWidget* parent=nullptr)=delete;
    teRefWordWidget(teWord&in_word):teWordWidgetBase(in_word){
        in_word.teConnect(teCallbackType::edit,this,&teRefWordWidget::readCore,&in_word);
        initialize();
    }
    teRefWordWidget(const teWordWidgetBase&in):teWordWidgetBase(in){initialize();}
    teRefWordWidget(teWordWidgetBase&&in)=delete;
    void initialize(){
        setMaximumHeight(20);
    }
};
class teTag:public teObject,public std::enable_shared_from_this<teTag>{
public:
    enum teTagType{
        tag=0,
        sentence,
        other
    };

    QList<teWord*> words;

    /**
     * @brief The widget that shows this tag, if any.
     *
     * The type used to be teTag (the widget of the single image tag list). That
     * list draws its tags through a delegate now and owns no widget per tag, so
     * the only widget a tag has is the one an editor created for it - hence the
     * common base class. Everything the editors call through this pointer
     * (setText, insertWord, destroyWord, disconnectWord, layout) is a teTagWidgetBase
     * member.
     */

    teTagWidgetBase* widget=nullptr;
    int weight=99;
    teTagType type = tag;
    teTag(){

    }
    teTag(bool iftmp){
        if(iftmp)info=QStringLiteral("tmp tagcore");
    }
    teTag(const QList<teWord*>in,teTagWidgetBase*child=nullptr);
    teTag(QList<teWord*>&&in,teTagWidgetBase*child=nullptr):words(std::move(in)),widget(child){}
    teTag(const QString &str,teTagWidgetBase*child=nullptr, bool forceSentence=false);
    teTag(const char* str,teTagWidgetBase*child=nullptr, bool forceSentence=false);
    teTag(const teTag&in):weight(in.weight),type(in.type){
        info=in.info;
        for(teWord*wc:in.words)
            words.push_back(new teWord{*wc});
    }
    teTag(teTag&&in);
    void load();
    void unload();
    /**
     * @brief True while a model/view based tag list shows this tag.
     *
     * Such a list draws its rows through a delegate and owns no widget per tag,
     * so ensureWidget() must not build the widgets the model replaced. A core
     * belongs to exactly one tag list, so a single flag is enough.
     */
    bool ifViewOwned=false;
    /**
     * @brief "This tag is on its way out."
     *
     * The editors merge one tag into another ("underwear" into "panties") and the
     * merged-away tag has to disappear. That used to be expressed by writing
     * teTag::deleteTag into `type`, i.e. the tag's *kind* was abused as a deletion
     * signal, and every layer then had to test for it again. It is an explicit
     * flag now, and teTagList::retire() is the one place that sets it and erases
     * the tag from its list.
     */
    bool retired=false;
    /**
     * @brief Makes sure the tag is displayed, i.e. that it owns a widget.
     *
     * This is the single place that has to change when a tag list stops owning
     * a widget per tag (a model/view based list shows tags through a delegate
     * instead), which is why the callers no longer test `widget` themselves.
     */
    void ensureWidget(){
        if(ifViewOwned)
            return;
        if(!widget)
            load();
    }
    void read(const QString &str,bool ifclear=true, bool forceSentence=false);
    teWord* takeWordAt(int index,bool ifSendSignal=true);
    QList<teWord*>::iterator begin(){
        return words.begin();
    }
    QList<teWord*>::iterator end(){
        return words.end();
    }
    QList<teWord*>::const_iterator begin() const{
        return words.begin();
    }
    QList<teWord*>::const_iterator end() const {
        return words.end();
    }
    void clear(){
        for(auto word:words)
            delete word;
        words.clear();
    }
    bool contains(const QString&str)const{
        for(const teWord*wc:words)
            if(wc->text==str)
                return true;
        return false;
    }
    teTag& operator=(const teTag&in);
    bool operator== (const teTag&in)const;
    operator QString() const;
    void edited(){
        teemit(teCallbackType::edit);
    };
    void edited_with_layout(){
        teemit(teCallbackType::edit_with_layout);
    }
    ~teTag();
};
class teInputWidget;
class teEditorControl;

class teTagWidgetBase: public QFrame,public teObject{
    Q_OBJECT
public:
    teTagWidgetBase(){ }
    virtual ~teTagWidgetBase();
    teTagWidgetBase(QWidget*parent):QFrame(parent){initialize();}
    teTagWidgetBase(std::shared_ptr<teTag>incore,QWidget*parent=nullptr);
    std::shared_ptr<teTag> core{};
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
        std::vector<teCallbackPtr> notify;
        if(ifnotify){
            std::lock_guard<std::recursive_mutex> lg(teCallbackMutex());
            for(auto&[obj,func]:linked_callback_call)
                if(func&&func->type==teCallbackType::extraWidget_removed&&func->connected)
                    notify.push_back(func);
        }
        // Invoke before disconnecting: disconnecting first would mark the
        // callbacks as dead and they would never run.
        for(teCallbackPtr&f:notify)
            if(f->connected)(*f)();
        teDisconnect(nullptr,teCallbackType::extraWidget_removed);
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

    enum teTagStyle{
        normal,
        select,
        select_current,
        multi,
        multi_select,
        multi_select_current,
    };
    void setStyle(teTagStyle in);
    virtual void readCore(std::shared_ptr<teTag>in_core)=0;
    virtual void setText(const QString& str){
        if(core==nullptr){
            core = std::make_shared<teTag>(str);
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
    teWord* takeWordAt(int index,bool ifSendSignal=true){
        while(index<0)index += core->words.size();
        teWord* wc = core->words.takeAt(index);
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
        teWord*wordcore = new teWord{std::move(string)};
        core->words.insert(index,wordcore);
        // Only a list that owns a widget per word (the widget based tag list)
        // needs the word widget here. The model/view based list and the editors'
        // tag lists rebuild their words from the core, which is what
        // edited_with_layout() below makes them do.
        if(ownsWordWidgets()){
            wordcore->load();
            layout->insertWidget(index,wordcore->widget);
            connectWord(wordcore->widget);
        }
        if(ifSendSignal)core->edited_with_layout();
    }
    void insertWord(int index,teWord* inwc, bool ifconnect,bool ifSendSignal=true){
        while(index<0)index += core->words.size()+1;
        core->words.insert(index,inwc);
        if(ownsWordWidgets()){
            if(!inwc->widget)
                inwc->load();
            layout->insertWidget(index,inwc->widget);
            if(ifconnect)
                connectWord(inwc->widget);
        }
        if(ifSendSignal)core->edited_with_layout();
    }
    /**
     * @brief True when this tag's list owns a widget per word.
     *
     * The widget based tag list does; a model/view based list draws the words
     * itself, and the editors' tag lists build their words from the tag core.
     */
    virtual bool ownsWordWidgets() const { return true; }
    void connectWord(teWordWidgetBase*inw){
        connect(inw,&teWordWidgetBase::droped,this,&teTagWidgetBase::worddroped);
        connect(inw,&teWordWidgetBase::mouseDoubleClicked,this,[this](teWordWidgetBase*inw){
            emit mouseDoubleClicked(this,inw);},Qt::DirectConnection
                );
    }
    /**
     * @brief Moves the widget of the word at `from` to layout position `to`.
     *
     * Kept as a named operation so the widget layout stays an implementation
     * detail of this class instead of being reached into from the editors.
     */
    void moveWordWidget(int from,int to){
        if(!layout)
            return;
        if(from<0||to<0||from>=layout->count()||to>=layout->count())
            return;
        layout->insertItem(to,layout->takeAt(from));
    }
    void disconnectWord(teWordWidgetBase*inw=nullptr){
        if(!inw){
            for(teWord*wc:core->words)
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
    teTagWidgetBase& operator=(const QString& input_string);
    teTagWidgetBase &operator=(const teTagWidgetBase &in);
    bool operator== (const teTagWidget&in)const;
    teTag& getcore()const{return *core;}
    virtual operator QString() const {return *core;}
    int fit_goodness(const QString& find_word,int*return_index=nullptr,int*return_questionable_index=nullptr) const;
    QList<teWord*>::iterator begin(){return core->words.begin();}
    QList<teWord*>::iterator end(){return core->words.end();}
    QList<teWord*>::const_iterator begin() const{return core->words.begin();}
    QList<teWord*>::const_iterator end() const {return core->words.end();}
    bool isDragging=false;
    int start_y=0;
    int mousePosInWidget=0;
    /// Set by the widget pool while this widget waits to be reused.
    bool inWidgetPool=false;
    /// The sheet currently applied by setStyle(); -1 means "none yet".
    int currentStyle=-1;
signals:
    void rightButtonPress(teTagWidgetBase*,QPoint,int);
    void leftButtonPress(teTagWidgetBase*,QPoint,int);
    void droped(teTagWidgetBase*,int);
    void mouseDoubleClicked(teTagWidgetBase*,teWordWidgetBase*);
public slots:
    virtual void worddroped(teWordWidgetBase*in_word,int xpos);
};

class teTagWidget:public teTagWidgetBase
{
    Q_OBJECT
public:
    teTagWidget(QWidget*parent=nullptr):teTagWidgetBase(parent){setStyle(teTagWidget::normal);}
    teTagWidget(const QString &str,QWidget*parent=nullptr);
    teTagWidget(std::shared_ptr<teTag>incore,QWidget*parent=nullptr);
    virtual void readCore(std::shared_ptr<teTag>in_core)override;
    void load()override;
};
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
    std::weak_ptr<teTag> tag_ptr;
    teOperationType type;
    teTag prevCore{true};
    teTag nowCore{true};
    int idp=-1;
    int idn=-1;
};

class teOperationList{
public:
    QVector<teTagOperation>operations;
    int inip=0;
    int rwp=0;
    int end=0;
    void addEditOperation(std::shared_ptr<teTag> in_ptr,teTag in_now){
        if(rwp==operations.size())
            operations.push_back({});
        teTagOperation&newoperation=operations[rwp++];
        end=rwp;
        newoperation.type=teTagOperation::tagedit;
        newoperation.tag_ptr=in_ptr;
        newoperation.nowCore=in_now;
    }
    void addInsertOperation(std::shared_ptr<teTag> in_ptr,int id,teTag in_now){
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
    void addEraseOperation(std::shared_ptr<teTag> in_ptr,int id,teTag in_prev){
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
    void addMoveOperation(std::shared_ptr<teTag> in_ptr,int idp,int idn){
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
    int previousEditOperation(std::shared_ptr<teTag>tag){
        int tmpp=rwp-1;
        while(tmpp>-1){
            if(operations[tmpp].tag_ptr.lock()==tag&&(operations[tmpp].type==teTagOperation::tagedit||operations[tmpp].type==teTagOperation::taginsert)){
                return tmpp;
            }
            --tmpp;
        }
        return tmpp;
    }
    void replaceTag(std::shared_ptr<teTag>prev_tag,std::shared_ptr<teTag>new_tag){
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

class teTagList:public QObject,public teObject{
    Q_OBJECT
    friend class teTagListView;
    friend class tePictureFile;
public:
    teTagList(){};
    bool isTagsLoaded=false;
    bool isWidgetLoaded=false;
    bool isSaved=true;
    teTagList(const teTagList& in):
        tags(in.tags){}
    teTagList(teTagList&&in):
        tags(std::move(in.tags)){}

    /* Adding, deleting or modifying a tag triggers nested modifications (a
     * rename can erase a duplicate, which erases the tag's widget, ...).
     * A recursive mutex makes that re-entrancy safe without the old
     * `locked_for_edit` counting hack, which was neither re-entrant nor
     * thread safe.
     */
    std::recursive_mutex tagsMt;
    /// While this is > 0 modifications are not pushed onto the undo stack.
    /// Prefer ChangeSuppressor over touching it directly.
    int signalSuppression=0;
    /// Tags marked for removal by retire() and not erased yet.
    std::vector<std::shared_ptr<teTag>> pendingRetire;
    /// While > 0 retire() only queues (see RetireDeferrer).
    int retireDeferDepth=0;

    /// RAII guard suppressing undo recording for its lifetime.
    struct ChangeSuppressor{
        teTagList* list;
        explicit ChangeSuppressor(teTagList* l):list(l){ if(list) ++list->signalSuppression; }
        ~ChangeSuppressor(){ if(list) --list->signalSuppression; }
        ChangeSuppressor(const ChangeSuppressor&)=delete;
        ChangeSuppressor& operator=(const ChangeSuppressor&)=delete;
    };

    int initialize_push_back(std::shared_ptr<teTag>);
    int initialize_push_back(const QString&, bool forceSentence);
    int initialize_push_back(const std::string&in, bool forceSentence);
    void connectTag(std::shared_ptr<teTag>tag){
        tag->teConnect(teCallbackType::edit_with_layout,this,&teTagList::onTagEdited,tag,true);
    }
    void disconnectTag(std::shared_ptr<teTag>tag){
        tag->teDisconnect(this);
    }
    bool recordingChanges() const { return signalSuppression<=0; }

    void onTagEdited(std::shared_ptr<teTag> tag,bool ifemit=true){
        isSaved=false;
        if(recordingChanges())
            operationlist.addEditOperation(tag,*tag);
        if(ifemit)
            emit tagEdited(tag);
    }
    void onTagErased(std::shared_ptr<teTag> tag,int id,bool ifemit=true){
        isSaved=false;
        if(recordingChanges())
            operationlist.addEraseOperation(tag,id,*tag);
        if(ifemit){
            emit tagErased(tag);
            teemit(teCallbackType::edit);
        }
    }
    void onTagMoved(std::shared_ptr<teTag> tag,int prev,int now){
        isSaved=false;
        if(recordingChanges())
            operationlist.addMoveOperation(tag,prev,now);
        emit tagMoved(tag);
    }
    void onTagInserted(std::shared_ptr<teTag> tag,int pos,bool ifemit=true){
        isSaved=false;
        if(recordingChanges())
            operationlist.addInsertOperation(tag,pos,*tag);
        if(ifemit){
            emit tagInserted(tag);
            teemit(teCallbackType::edit);
        }
    }
    void load(){
        if(isWidgetLoaded)return;
        for(std::shared_ptr<teTag> tag:tags){
            tag->load();
        }
        isWidgetLoaded=true;
    }
    void unload(){
        if(!isWidgetLoaded)return;
        for(std::shared_ptr<teTag> tag:tags){
            tag->unload();
        }
        isWidgetLoaded=false;
    }
    void clear(){
        std::lock_guard<std::recursive_mutex> lg(tagsMt);
        tags.clear();
        operationlist.clear();
    }
    /// Unchecked access; `at()` is the bounds-checked variant.
    teTag& operator[](int index){
        return *tags[index];
    }
    /// Bounds-checked access. Returns nullptr when `index` is out of range.
    teTag* at(int index){
        if(index<0||index>=tags.size())
            return nullptr;
        return tags[index].get();
    }
    /// Bounds-checked access that keeps the owning shared_ptr alive.
    std::shared_ptr<teTag> shareAt(int index) const{
        if(index<0||index>=tags.size())
            return nullptr;
        return tags[index];
    }
    /**
     * @brief Takes the tag at `index` out of the list.
     *
     * No signal and no undo record: the caller (a model) owns the view
     * notifications and calls onTagMoved() once the row is back in place. Used
     * for dragging several rows at once, which is not a contiguous block move.
     */
    std::shared_ptr<teTag> takeTagOut(int index){
        if(index<0||index>=tags.size())
            return nullptr;
        return tags.takeAt(index);
    }
    /// Puts a tag back (same contract as takeTagOut()).
    void insertTagIn(int index,std::shared_ptr<teTag> tag){
        if(!tag)
            return;
        index=std::clamp(index,0,int(tags.size()));
        tags.insert(index,tag);
    }

    /// Removes duplicates.
    ///  - `tag == nullptr`: removes every duplicate pair in the list.
    ///  - `keepself == false`: returns true if an identical *other* tag exists
    ///    (nothing is removed).
    ///  - `keepself == true`: erases every *other* tag identical to `tag` and
    ///    returns true when something was erased.
    bool remove_duplicate(std::shared_ptr<teTag> tag=nullptr,bool keepself=false);

    int insert(int pos,std::shared_ptr<teTag>tag,int removeDuplicate=1,bool ifSendSignal=true);

    /// Removes and destroys the tag at `id`. Out-of-range ids are ignored.
    void erase(int id);
    void erase(std::shared_ptr<teTag>core){
        const int index = tags.indexOf(core);
        if(index<0){
            telog("Can't find the pointer in taglist");
            return;
        }
        erase(index);
    }
    /**
     * @brief The single way to say "this tag is going away".
     *
     * Marks the tag (teTag::retired) and erases it, so the two steps cannot drift
     * apart: a tag that is marked but not erased stayed in the list, and one that
     * is erased without being marked came back through the editor that owned it.
     * Every hardcoded `type = teTag::deleteTag` site is meant to call this.
     *
     * While a RetireDeferrer is alive the erase is only queued: removing a tag
     * from inside the loop that is reading the list pulled the ground out from
     * under the editor that was still working on it (that is why the old code
     * marked the tag and erased it after the loop).
     */
    void retire(std::shared_ptr<teTag> tag){
        if(!tag)
            return;
        // Idempotent on purpose: a control may have marked the tag itself (a merge
        // marks the tag it absorbed from inside filter()), and the caller that
        // walks the list afterwards must be able to hand it over again.
        tag->retired=true;
        if(tags.indexOf(tag)<0)
            return;                             // already erased
        if(std::find(pendingRetire.begin(),pendingRetire.end(),tag)==pendingRetire.end())
            pendingRetire.push_back(tag);
        if(retireDeferDepth==0)
            purgeRetired();
    }
    /// Erases every queued tag. Called by retire() and by RetireDeferrer.
    void purgeRetired(){
        // The erases below run the editors' callbacks, which may retire more tags:
        // work off a copy and loop until nothing is left.
        while(!pendingRetire.empty()){
            const std::vector<std::shared_ptr<teTag>> batch = pendingRetire;
            pendingRetire.clear();
            for(const std::shared_ptr<teTag>& tag : batch){
                const int pos = tags.indexOf(tag);
                if(pos>=0)
                    erase(pos);
            }
        }
    }
    /// True when the tag is marked as going away (or is already gone).
    static bool isRetired(const std::shared_ptr<teTag>& tag){
        return !tag||tag->retired;
    }
    /// Queues retire() for its lifetime and flushes once at the end.
    struct RetireDeferrer{
        teTagList* list;
        explicit RetireDeferrer(teTagList* in) : list(in){ if(list) ++list->retireDeferDepth; }
        ~RetireDeferrer(){
            if(list&&--list->retireDeferDepth==0)
                list->purgeRetired();
        }
        RetireDeferrer(const RetireDeferrer&)=delete;
        RetireDeferrer& operator=(const RetireDeferrer&)=delete;
    };

    /// Renames `core` (or the tag at `index`).
    /// `removeDuplicate == 1` merges: an existing tag with the same text is
    /// erased so the list never ends up with two identical tags.
    /// Returns -1 when the tag was absorbed (see teTagList::retire), 0 otherwise.
    int edit(std::shared_ptr<teTag>core,QString text,int removeDuplicate=1,bool ifemit=true);
    int edit(int index,QString text,int removeDuplicate=1,bool ifemit=true);

    void move(int originPos,int newPos);
    void move(std::shared_ptr<teTag>tag,int newPos);
    /**
     * @brief Moves `count` tags starting at `from` to `insertAt`.
     *
     * `insertAt` is an index in the list *after* the block was taken out, which
     * is what a model needs for QAbstractItemModel::moveRows(). Records the move
     * for undo/redo and emits tagMoved(), but does not announce anything to a
     * view - the caller owns the begin/endMoveRows() pair.
     */
    void reorderBlock(int from,int count,int insertAt);
    QString toText();
    int find(std::shared_ptr<teTag>in){
        return tags.indexOf(in);
    }
    virtual ~teTagList(){
        tags.clear();
        onDestroy();
    }
    QVector<std::shared_ptr<teTag>> getTags(){
        return tags;
    }
    QList<std::shared_ptr<teTag>>::Iterator begin(){
        return tags.begin();
    }
    QList<std::shared_ptr<teTag>>::Iterator end(){
        return tags.end();
    }
    QList<std::shared_ptr<teTag>>::const_iterator begin()const {
        return tags.begin();
    }
    QList<std::shared_ptr<teTag>>::const_iterator end()const {
        return tags.end();
    }
    size_t size() const{
        return tags.size();
    }
    std::shared_ptr<teTag>& back(){
        return tags.back();
    }
    std::shared_ptr<teTag>const& back()const{
        return tags.back();
    }
protected:
    QVector<std::shared_ptr<teTag>> tags;
    teOperationList operationlist;
signals:
    void tagInserted(std::shared_ptr<teTag>);
    void tagEdited(std::shared_ptr<teTag>);
    void tagErased(std::shared_ptr<teTag>);
    /// Emitted whenever the order changed (used by the model based tag list,
    /// which cannot see the reorder any other way).
    void tagMoved(std::shared_ptr<teTag>);
};


// [tag / sentence] -> 文本
template <class Getter>
QString serializePieces(int count, Getter getter, bool includeSentence = true)
{
    QString out;

    bool havePrev = false;
    bool prevSentence = false;

    for (int i = 0; i < count; ++i) {
        std::shared_ptr<teTag> tag = getter(i);
        if (!tag)
            continue;

        const bool curSentence = (tag->type == teTag::sentence);

        if (!includeSentence && curSentence)
            continue;

        QString curText = static_cast<QString>(*tag);

        // sentence 强制补句号
        if (curSentence) {
            curText = curText.trimmed();
            if (!curText.endsWith('.'))
                curText += '.';
        }

        // 处理前一个元素与当前元素之间的分隔
        if (havePrev) {

            // tag -> tag
            if (!prevSentence && !curSentence) {
                out += ", ";
            }

            // tag -> sentence
            else if (!prevSentence && curSentence) {
                out += "\n";
            }

            // sentence -> tag
            else if (prevSentence && !curSentence) {
                out += "\n";
            }

            // sentence -> sentence
            else {
                out += " ";
            }
        }

        out += curText;

        havePrev = true;
        prevSentence = curSentence;
    }

    return out;
}

// 文本 -> [tag / sentence]
QList<ParsedPiece> splitTextToPieces(const QString& raw);
#endif // TETAG_H
