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
    teWordWidgetBase(teWord&in):QLabel(in.text)
    {initialize();attachWord(&in);}
    teWordWidgetBase(const teWordWidgetBase&in):
        QLabel(in.core?in.core->text:QString())
    {initialize();attachWord(in.core);}
    teWordWidgetBase(teWordWidgetBase&&in)=delete;
    /**
     * @brief Makes this widget show `in` without ever keeping a stale pointer.
     *
     * A word widget used to hold a bare pointer, and a tag widget walks its word
     * widgets and dereferences exactly those pointers (disconnectWord() and
     * wordWidgetFor()). Words do die while their widgets are still around - a
     * tag re-read, the duplicate a merge drops, or the word list of a tag that
     * is cleared when the picture changes - and that dangling teWord is what
     * made the clothes editor crash while switching images.
     *
     * Listening to the word's destruction is what turns "core is a pointer that
     * may be stale" into "core is either null or alive", the invariant every one
     * of those call sites already assumes.
     */
    void attachWord(teWord* in){
        if(core!=in&&core)
            core->teDisconnect(this);       // show only one word at a time
        core=in;
        if(!core)
            return;
        // Re-armed every time, not only when the word changes: disconnectWord()
        // and give_back() both strip this connection, and the widget is then
        // handed out again for the very same word. Only this callback is
        // replaced, so the connections a specialised widget made for itself
        // (teRefWordWidget listens for `edit`) survive a re-arm.
        core->teDisconnect(this,teCallbackType::destroy);
        core->teConnect(teCallbackType::destroy,this,&teWordWidgetBase::onWordDestroyed);
    }
    /// The word this widget showed is gone: forget it instead of keeping a
    /// pointer into freed memory (see attachWord()).
    void onWordDestroyed(){ core=nullptr; }
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
        core=nullptr;       // the tag widget owns its word widgets, not the core
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
        attachWord(new teWord(input_string));
    }
    teWordWidget(QString&& input_string,QWidget* parent=nullptr):teWordWidget(input_string,parent){
        attachWord(new teWord(std::move(input_string)));
    }
    teWordWidget(teWord&in):teWordWidgetBase(in){
    }
    teWordWidget(const teWordWidgetBase&in):teWordWidgetBase(in){
    }

    teWordWidget(teWordWidgetBase&&in)=delete;
    virtual void readCore(teWord*in)override{
        teWordWidgetBase::readCore(in);
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
/**
 * @brief What an editor control did with a tag it was offered.
 *
 * These answers used to be plain `bool`s, and the same `true` meant "I claim this
 * tag" in one place and "this tag is gone, stop looking" in another - the callers
 * could not tell the two apart. Splitting them makes the contract explicit: only
 * Consumed tells the caller to stop.
 */
enum class TagHandling {
    /// Not this control's tag: keep asking the other controls.
    NotMine,
    /// This control claims the tag and shows it (its filter() said yes).
    Linked,
    /// The tag was retired while it was read (it was merged away). The list
    /// erases it, and no other control has to look at it any more.
    Consumed,
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
     * Gone: widgets belong to the list that shows them (teTagListWidgetBase keeps
     * the table). A tag is data plus signals; nothing here knows about widgets.
     */
    int weight=99;
    teTagType type = tag;
    teTag(){

    }
    teTag(bool iftmp){
        if(iftmp)info=QStringLiteral("tmp tagcore");
    }
    teTag(const QList<teWord*>in,teTagWidgetBase*child=nullptr);
    teTag(QList<teWord*>&&in,teTagWidgetBase*child=nullptr):words(std::move(in)){Q_UNUSED(child);}
    teTag(const QString &str,teTagWidgetBase*child=nullptr, bool forceSentence=false);
    teTag(const char* str,teTagWidgetBase*child=nullptr, bool forceSentence=false);
    teTag(const teTag&in):weight(in.weight),type(in.type){
        info=in.info;
        for(teWord*wc:in.words)
            words.push_back(new teWord{*wc});
    }
    teTag(teTag&&in);
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
     * @brief "This tag was merged once; do not merge it away by itself again."
     *
     * An automatic merge is something the user did not ask for, so undoing one has
     * to stick: the tags come back as they were, and the very next read would merge
     * them again on the spot - the undo would look like it did nothing.
     *
     * Both sides of a merge carry the mark, and a merge is skipped only when *both*
     * sides carry it. Touching either tag - a real edit, or entering the editor and
     * confirming without changing a word - clears its mark, so the pair merges again
     * the moment the user actually asks for it. The mark travels in the undo
     * snapshots, which is what makes it survive the undo it exists for.
     */
    bool mergeSuppressed=false;
    /// The words as plain text - the form the undo snapshots store.
    QStringList wordTexts() const{
        QStringList out;
        out.reserve(words.size());
        for(const teWord* word : words)
            if(word)
                out.append(word->text);
        return out;
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
class EditorControl;

class teTagWidgetBase: public QFrame,public teObject{
    Q_OBJECT
public:
    teTagWidgetBase(){ }
    virtual ~teTagWidgetBase();
    teTagWidgetBase(QWidget*parent):QFrame(parent){initialize();}
    teTagWidgetBase(std::shared_ptr<teTag>incore,QWidget*parent=nullptr);
    std::shared_ptr<teTag> core{};
    QHBoxLayout* layout=nullptr;

    std::multimap<EditorControl*,QWidget*>extra_widgets;
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
    void insertExtraWidgets(EditorControl*e,QWidget*w){
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
        // The word widget is this tag widget's child; the layout that holds it is
        // rebuilt from the core by edited_with_layout() below.
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
            ensureWordWidgetFor(wordcore,index);
        }
        if(ifSendSignal)core->edited_with_layout();
    }
    void insertWord(int index,teWord* inwc, bool ifconnect,bool ifSendSignal=true){
        while(index<0)index += core->words.size()+1;
        core->words.insert(index,inwc);
        if(ownsWordWidgets()){
            teWordWidgetBase* built = ensureWordWidgetFor(inwc,index);
            if(!ifconnect&&built)
                disconnect(built,0,this,0);
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
    /**
     * @brief The widget showing `core` among this tag's children, or nullptr.
     *
     * The tag widget owns its word widgets (they are its layout's children), so
     * this - not teWord::widget - is where "which widget shows this word" is
     * answered. teWord is data plus signals.
     */
    teWordWidgetBase* wordWidgetFor(teWord* core) const {
        if(!core)
            return nullptr;
        for(teWordWidgetBase* word : findChildren<teWordWidgetBase*>())
            if(word->core==core)
                return word;
        return nullptr;
    }
    /// Same, but builds the widget (and connects it) when there is none yet.
    /// Defined in tetag.cpp: the widget pool is declared after this class.
    teWordWidgetBase* ensureWordWidgetFor(teWord* core,int layoutIndex=-1);
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
            for(teWordWidgetBase* word : findChildren<teWordWidgetBase*>()){
                disconnect(word,0,this,0);
                if(word->core)
                    word->core->teDisconnect(this);
            }
        }else{
            disconnect(inw,0,this,0);
            inw->teDisconnect(this);
        }
    }
    /**
     * @brief Makes every word widget of this tag forget `in`.
     *
     * Must be called before a word is deleted while a widget may still be
     * showing it: disconnectWord() only drops the *connections*, so a widget
     * that was disconnected keeps the (soon to be dangling) pointer. Used by the
     * clothes editor, which drops the duplicate of a merged word.
     */
    void forgetWord(teWord* in){
        if(!in)
            return;
        for(teWordWidgetBase* word : findChildren<teWordWidgetBase*>())
            if(word->core==in)
                word->attachWord(nullptr);
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
/**
 * @brief One tag as the undo history writes it down.
 *
 * The words are stored one by one instead of as the tag's text: a tag is a list
 * of words and its text is that list joined with spaces, so re-parsing the text
 * would quietly normalise spacing and re-classify the tag. Capturing the words
 * makes a snapshot an exact copy, and it refers to nothing - no tag pointer, no
 * position - which is what lets a step stay valid after the tags it came from
 * are gone.
 */
struct teTagSnapshot {
    QStringList words;
    int type = 0;                   // teTag::teTagType
    int weight = 99;
    /// See teTag::mergeSuppressed: undo has to bring the mark back with the tag.
    bool mergeSuppressed = false;
    bool operator==(const teTagSnapshot& in) const {
        return type==in.type&&weight==in.weight
               &&mergeSuppressed==in.mergeSuppressed&&words==in.words;
    }
    bool operator!=(const teTagSnapshot& in) const { return !(*this==in); }
};
using teTagSnapshotList = QVector<teTagSnapshot>;

/**
 * @brief The undo history of one tag list: whole states, oldest first.
 *
 * The old history stored one record per tag operation (insert / edit / erase /
 * move) with a weak_ptr to the tag and the row it used to be at. One click is
 * rarely one operation - a merge is "edit this, erase that" - so undoing a click
 * took several undos, each of which could leave the list half merged; and the
 * row indexes stopped being true as soon as anything above them moved, which is
 * what the "position in operationList is wrong" fallbacks were papering over.
 *
 * A step here is a pair of complete states instead: what the list looked like
 * before the gesture and after it. Undo applies the first, redo the second, and
 * nothing has to be replayed or searched for. One gesture is one step, always.
 */
class teUndoHistory {
public:
    /// Steps kept. Far more than one picture needs, small enough that a whole
    /// history of a 60 tag list is a few hundred KiB.
    static constexpr int maxSteps = 100;
    struct Step {
        teTagSnapshotList before;
        teTagSnapshotList after;
    };
    bool canUndo() const { return cursor>0; }
    bool canRedo() const { return cursor<int(steps.size()); }
    /// Records one gesture. A step that changed nothing is not recorded, and a
    /// new step after an undo drops the redo branch behind it.
    void push(const teTagSnapshotList& before,const teTagSnapshotList& after){
        if(before==after)
            return;
        if(cursor<int(steps.size()))
            steps.resize(cursor);
        steps.append({before,after});
        if(int(steps.size())>maxSteps)
            steps.remove(0);
        cursor=int(steps.size());
    }
    const Step* stepToUndo() const { return canUndo()?&steps[cursor-1]:nullptr; }
    const Step* stepToRedo() const { return canRedo()?&steps[cursor]:nullptr; }
    void stepUndone(){ if(canUndo()) --cursor; }
    void stepRedone(){ if(canRedo()) ++cursor; }
    void clear(){ steps.clear(); cursor=0; }
    bool isEmpty() const { return steps.isEmpty(); }
    int size() const { return int(steps.size()); }
private:
    QVector<Step> steps;
    int cursor=0;
};

class teTagList:public QObject,public teObject{
    Q_OBJECT
    friend class teTagListView;
    friend class tePictureFile;
public:
    teTagList(){};
    bool isTagsLoaded=false;
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

    /**
     * @brief One gesture is one undo step, however many tags it touches.
     *
     * Every announcement below opens (or joins) a step and closes it again. The
     * step is not closed immediately: a merge is often only queued behind the
     * change that caused it (the model defers structural changes until its own
     * rows are consistent), so the commit waits for the event loop to go idle.
     * Two clicks are two events, so they are still two steps.
     */
    struct ChangeRecorder{
        teTagList* list;
        bool active;
        explicit ChangeRecorder(teTagList* l)
            :list(l),active(l&&l->recordingChanges()){
            if(active)
                list->beginChange();
        }
        ~ChangeRecorder(){ if(active) list->endChange(); }
        ChangeRecorder(const ChangeRecorder&)=delete;
        ChangeRecorder& operator=(const ChangeRecorder&)=delete;
    };

    void onTagEdited(std::shared_ptr<teTag> tag,bool ifemit=true){
        isSaved=false;
        ChangeRecorder recorder(this);
        if(ifemit)
            emit tagEdited(tag);
    }
    void onTagErased(std::shared_ptr<teTag> tag,int id,bool ifemit=true){
        Q_UNUSED(id);
        isSaved=false;
        ChangeRecorder recorder(this);
        if(ifemit){
            emit tagErased(tag);
            teemit(teCallbackType::edit);
        }
    }
    void onTagMoved(std::shared_ptr<teTag> tag,int prev,int now){
        Q_UNUSED(prev);
        Q_UNUSED(now);
        isSaved=false;
        ChangeRecorder recorder(this);
        emit tagMoved(tag);
    }
    void onTagInserted(std::shared_ptr<teTag> tag,int pos,bool ifemit=true){
        Q_UNUSED(pos);
        isSaved=false;
        ChangeRecorder recorder(this);
        if(ifemit){
            emit tagInserted(tag);
            teemit(teCallbackType::edit);
        }
    }

    /**
     * @brief The list content as a snapshot, in display order.
     *
     * `origins`, when given, receives the live tag each entry was taken from, in
     * the same order. The snapshot itself cannot hold them (it outlives the tags),
     * but while a step is open they are what tells the snapshot which entry a merge
     * has to mark - the words are useless for that, they are exactly what an edit
     * changes (see noteMergedForUndo).
     */
    teTagSnapshotList snapshot(QVector<teTag*>* origins=nullptr) const{
        teTagSnapshotList out;
        out.reserve(tags.size());
        if(origins)
            origins->clear();
        for(const std::shared_ptr<teTag>& tag : tags){
            if(!tag)
                continue;
            teTagSnapshot one;
            one.words=tag->wordTexts();
            one.type=int(tag->type);
            one.weight=tag->weight;
            one.mergeSuppressed=tag->mergeSuppressed;
            out.append(one);
            if(origins)
                origins->append(tag.get());
        }
        return out;
    }

    /**
     * @brief Marks the copies of two tags in the step being recorded.
     *
     * Called when an automatic merge happens: the snapshot was taken before the
     * merge, so the tags it will restore are unmarked unless they are marked here.
     *
     * The two tags are found by identity, not by their text: the merge runs inside
     * the gesture that changed one of them, so its words in the snapshot are the
     * pre-edit ones and no longer match what the tag says now.
     */
    void noteMergedForUndo(const teTag& first,const teTag& second){
        const int count = std::min(baselineTags.size(),stateBeforePendingChange.size());
        for(int i=0;i<count;++i){
            if(baselineTags[i]==&first||baselineTags[i]==&second)
                stateBeforePendingChange[i].mergeSuppressed=true;
        }
    }

    /**
     * @brief Puts the list in `order` without announcing anything.
     *
     * A permutation of the tags already there; the view tells its model to refresh
     * the rows afterwards. Used by undo/redo, which restores an order the model
     * cannot express as a single move.
     */
    void reorderTo(const QVector<std::shared_ptr<teTag>>& order){
        std::lock_guard<std::recursive_mutex> lg(tagsMt);
        if(order.size()!=tags.size())
            return;
        tags=order;
    }

    /// Takes the current content as the state the next step starts from.
    void syncUndoBaseline(){
        changePending=false;
        stateBeforePendingChange.clear();
        stateBeforePendingChange=snapshot(&baselineTags);
    }

    /// Opens a step, or joins the one already open.
    ///
    /// No snapshot is taken here: every hook runs *after* the change it reports,
    /// so the state before the gesture has to be the one kept from the last
    /// commit (see syncUndoBaseline).
    void beginChange(){
        ++changeDepth;
        changePending=true;
    }

    /// Closes it once the event that caused it has been fully handled.
    void endChange(){
        if(--changeDepth>0||destroying)
            return;
        if(!commitTimer){
            // A timer of its own instead of QTimer::singleShot(0, this, ...): the
            // singleShot overload keeps a weak reference to the context object, and
            // creating one while this list is being torn down trips Qt's
            // "QWeakPointer creation in a QObject being deleted" assert.
            commitTimer=new QTimer(this);
            commitTimer->setSingleShot(true);
            commitTimer->setInterval(0);
            connect(commitTimer,&QTimer::timeout,this,[this]{ commitChange(); });
        }
        commitTimer->start();
    }

    /**
     * @brief Turns the open step into a history entry.
     *
     * Called by the timer above, and directly by undo/redo so that a click
     * followed by Ctrl+Z in the same event still undoes the click.
     */
    void commitChange(){
        if(commitTimer)
            commitTimer->stop();
        if(changeDepth>0||!changePending)
            return;
        changePending=false;
        teTagSnapshotList after=snapshot();
        const bool changed = (after!=stateBeforePendingChange);
        if(changed)
            history.push(stateBeforePendingChange,after);
        // The baseline is the state the *next* step starts from, so it moves
        // whether or not this one was worth recording.
        stateBeforePendingChange=after;
    }

    /// True when there is something to undo (an open step counts: undo flushes it).
    bool canUndo() const { return changePending||history.canUndo(); }
    /// While a step is open there is nothing to redo: closing it drops the redo
    /// branch, so offering redo and then throwing the click away would be a lie.
    bool canRedo() const { return !changePending&&history.canRedo(); }
    void clear(){
        std::lock_guard<std::recursive_mutex> lg(tagsMt);
        tags.clear();
        history.clear();
        pendingRetire.clear();
        syncUndoBaseline();
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
    /**
     * @brief Takes the tag out of the list *without* announcing it.
     *
     * The undo step is recorded, but no signal and no callback runs, so the
     * caller can finish its own structural change first and then call
     * notifyErased(). See the comment in the implementation.
     */
    std::shared_ptr<teTag> takeErased(int id);
    /// Announces an erase that takeErased() already performed.
    void notifyErased(std::shared_ptr<teTag> core,int id,bool ifemit=true);
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
        // A tag being destroyed announces itself, and an announcement opens a step:
        // during teardown there is nothing left to record, and the timer must not be
        // re-armed on an object that is going away.
        destroying=true;
        if(commitTimer)
            commitTimer->stop();
        changeDepth=0;
        changePending=false;
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
    /**
     * @brief The undo history of this list.
     *
     * It belongs to the list, not to the view: switching to another picture and
     * back keeps this picture's history, and because a step stores states rather
     * than pointers it survives the tags being rebuilt.
     */
    teUndoHistory history;
    /// The content at the start of the step being recorded.
    teTagSnapshotList stateBeforePendingChange;
    /// The live tags those snapshot entries were taken from, same order. Only used
    /// to find a snapshot entry again while the step is open (a merge marks two of
    /// them); they are raw on purpose - nothing here dereferences them.
    QVector<teTag*> baselineTags;
    /// True while a step is open (see beginChange/commitChange).
    bool changePending=false;
    /// How many nested change notifications are running right now.
    int changeDepth=0;
    /// Closes the open step at the end of the current event (see endChange).
    QTimer* commitTimer=nullptr;
    /// Set while this list is being destroyed: nothing may schedule a commit then.
    bool destroying=false;
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
