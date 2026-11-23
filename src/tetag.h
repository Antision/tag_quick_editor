#ifndef TETAG_H
#define TETAG_H
#include"pch.h"
// 数据层(teWord / teTag / teTagList / 快照 / 切分)已经搬进 common/tagview;
// 这个头只剩 tqe 自己的 per-tag 控件实现。
#include"tag.h"

class teWordWidget;
class teTagWidget;
class teTagWidgetBase;
class obj_callback_function_base;
class teInputWidget;
class EditorControl;

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
    /// Text colour for this word (white, or black on a white-dominant tag).
    void setTextColor(const QColor& color);
    QColor textColor=Qt::white;
    void applyWordSheet();
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
        if(ifnotify)
            notify = teCollectCallbacks(teCallbackType::extraWidget_removed);
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
    /**
     * @brief 把焦点交给承载这个 tag 的列表(teTagListWidgetBase)。
     *
     * 列表是 ClickFocus,tag 控件本身是 NoFocus:键盘快捷键(Ctrl+W / Ctrl+X /
     * Enter / F2 ...)要落在列表的 keyPressEvent 上,必须先让列表拿到焦点。
     * 旧代码对 tag 控件调 setFocus()——打在 NoFocus 上什么都不发生,列表的
     * 快捷键从此按了没反应。
     */
    void focusOwningList();
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
    /// Base sheet + the mark colours, so an unchanged style is not re-parsed.
    QString currentSheet;
    /// Colour part of the sheet for `core` (empty when it has no marks).
    QString markStyleSheet() const;
    /// Re-applies the current style (after the decoration changed).
    void refreshStyle(){ setStyle(teTagStyle(currentStyle<0?normal:currentStyle)); }
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
#endif // TETAG_H
