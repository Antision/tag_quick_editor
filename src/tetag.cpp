#include "tetag.h"
#include "teeditorcontrol.h"
teTagWidget::teTagWidget(const QString &str,QWidget*parent):
    teTagWidgetBase(std::make_shared<teTag>(str),parent)
{
    load();
    setStyle(teTagWidget::normal);
}

void teTagWidget::readCore(std::shared_ptr<teTag>in_core){
    core = in_core;
    in_core->teConnect(teCallbackType::edit,this,&teTagWidget::load);
    in_core->teConnect(teCallbackType::edit_with_layout,this,&teTagWidget::load);

    if(in_core->type==teTag::sentence)
        setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);
    load();
}

teTagWidget::teTagWidget(std::shared_ptr<teTag>incore,QWidget*parent):teTagWidgetBase(incore,parent){
    load();
    setStyle(teTagWidget::normal);
}
teTagWidgetBase::teTagWidgetBase(std::shared_ptr<teTag>incore, QWidget *parent){
    core=incore;
    initialize();
}

void teTagWidgetBase::clearWordWidgets(){
    for(teWordWidgetBase*word:findChildren<teWordWidgetBase*>()){
        widgetpool.give_back(word);
    }
}

void teTagWidgetBase::takeWordWidgets(){
    int layoutItemCount = layout->count();
    for(int i=0;i<layoutItemCount-1;++i){
        layout->takeAt(0);
    }
}

teWordWidgetBase* teTagWidgetBase::ensureWordWidgetFor(teWord* core,int layoutIndex)
{
    if(!core)
        return nullptr;
    if(teWordWidgetBase* existing = wordWidgetFor(core)){
        // Reused rather than rebuilt: re-arm the "forget me if I die" callback,
        // because disconnectWord() stripped it (see teWordWidgetBase::attachWord).
        existing->attachWord(core);
        return existing;
    }
    teWordWidgetBase* built = widgetpool.getWord(core);
    if(layout)
        layout->insertWidget(layoutIndex<0?layout->count()-1:layoutIndex,built);
    connectWord(built);
    return built;
}

void teTagWidgetBase::initialize(){
    setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Fixed);
    layout = new QHBoxLayout(this);
    QSpacerItem* rightspacer = new QSpacerItem(0,0,QSizePolicy::Expanding,QSizePolicy::Fixed);
    layout->addSpacerItem(rightspacer);
    layout->setContentsMargins(2,4,0,4);
}

extern QString teTag_selectCurrentStyle;
extern QString teTag_selectStyle;
extern QString teTag_normalStyle;
extern QString teTagMulti_selectCurrentStyle;
extern QString teTagMulti_selectStyle;
extern QString teTagMulti_normalStyle;
void teTagWidgetBase::setStyle(teTagStyle in){
    // Qt re-parses a sheet on every setStyleSheet() call (about 3 ms for these
    // small widgets), and the tag list sets the style of every tag on every
    // load, so re-applying the sheet that is already in place is skipped.
    if(currentStyle==int(in))
        return;
    currentStyle=int(in);
    switch(in){
    case normal:setStyleSheet(teTag_normalStyle);break;
    case select:setStyleSheet(teTag_selectStyle);break;
    case select_current:setStyleSheet(teTag_selectCurrentStyle);break;
    case multi:setStyleSheet(teTagMulti_normalStyle);break;
    case multi_select:setStyleSheet(teTagMulti_selectStyle);break;
    case multi_select_current:setStyleSheet(teTagMulti_selectCurrentStyle);break;

    default:telog("unknown teTag style");
    }
}

teTagWidgetBase::~teTagWidgetBase(){
    for(teWordWidgetBase*word:findChildren<teWordWidgetBase*>()){
        word->setParent(this->parentWidget());
    }
    core.reset();               // the list owns the widget table, not the tag
}

teTagWidgetBase& teTagWidgetBase::operator=(const QString& input_string){
    setText(input_string);
    return *this;
};

teTagWidgetBase &teTagWidgetBase::operator=(const teTagWidgetBase &in){
    setText(in);
    core->weight=in.core->weight;
    core->type=in.core->type;
    return *this;
}

bool teTagWidgetBase::operator==(const teTagWidget &in) const{
    if(core->words.size()!=in.core->words.size()) return false;
    else{
        int size = core->words.size();
        for(int i =0;i<size;++i){
            if(core->words[i]->text!=in.core->words[i]->text)
                return false;
        }
        return true;
    }
}

int teTagWidgetBase::fit_goodness(const QString &find_word, int *return_index, int *return_questionable_index) const{
    int&& size=core->words.size();
    int index=-1;
    int questionable_index=-1;
    for(int i=0;i<size;++i){
        const QString& word=*core->words[i];
        if(word.length()<find_word.length())continue;
        else if(word==find_word)index=i;
        else if(word.lastIndexOf(find_word)==word.length()-find_word.length())questionable_index=i;
    }
    if(return_index!=nullptr)*return_index=index;
    if(return_questionable_index!=nullptr)*return_questionable_index=questionable_index;

    if(index==-1&&questionable_index==-1) return-2;
    else if(((index!=-1&&index+1<size)
              ||(index==-1))
             &&questionable_index+1==size
             )return 0;
    else if(index!=-1&&index+1<size) return -1;
    else if(index!=-1&&index+1==size) return 1;
    else return -2;
}

void teTagWidget::load(){
    static int teTagLoadNum=0;
    int wordscount = core->words.count();
    clearWordWidgets();
    for(int i=0;i<wordscount;++i){
        teWordWidgetBase* newword = widgetpool.getWord(core->words[i]);
        layout->insertWidget(i,newword);
        connectWord(newword);
    }
    if(wordscount==0){
        teWord* newwordcore = new teWord(QStringLiteral(""));
        teWordWidgetBase* newword = widgetpool.getWord(newwordcore);
        core->words.push_back(newword->core);
        layout->insertWidget(0,newword);
        connectWord(newword);
    }
}


void teTagWidgetBase::mouseMoveEvent(QMouseEvent *event) {
    if (event->buttons() & Qt::LeftButton) {
        extern QWidget* global_window;
        if (!global_window->rect().contains(global_window->mapFromGlobal(event->globalPosition()).toPoint())){
            QDrag *drag = new QDrag(this);
            QMimeData *mimeData = new QMimeData();
            mimeData->setText(operator QString());
            drag->setMimeData(mimeData);
            drag->exec();
            emit droped(nullptr,0);
            return;
        }
        int current_y = event->pos().y();
        int distance = current_y - start_y;
        if (!isDragging) {
            isDragging = true;
        }
        if (isDragging) {
            move(mapToParent(QPoint{0,distance}));
        }
    }
}
void teTagWidgetBase::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        start_y = event->pos().y();
        mousePosInWidget = start_y;
        isDragging = false;
        raise();
        setFocus();
        emit leftButtonPress(this,event->pos(),event->modifiers());
    }
    else if(event->button() == Qt::RightButton){
        emit rightButtonPress(this,event->pos(),event->modifiers());
    }
}
void teTagWidgetBase::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        // Only a real drag reorders the list. Emitting this for a plain click
        // made tagdroped() re-apply the selection, so ctrl+clicking a tag that
        // was already selected deselected it on press and selected it again on
        // release - the tag could never be removed from a multi selection.
        if(!isDragging)
            return;
        isDragging = false;
        emit droped(this,event->modifiers());
    }
}
void teTagWidgetBase::worddroped(teWordWidgetBase *in_word, int xpos){
    int wordcount = core->words.count();
    int in_id=-1;
    int i=0;
    for(;i<wordcount;++i){
        teWordWidgetBase*wordptr = wordWidgetFor(core->words[i]);
        if(wordptr!=in_word){
            if(xpos < wordptr->x()+wordptr->width()){
                break;
            }
        }else{
            in_id=i;
            for(i=wordcount-1;i>in_id;--i){
                wordptr = wordWidgetFor(core->words[i]);
                if(xpos > wordptr->x()){
                    ++i;
                    goto words_loop_end;
                }
            }
            layout->update();
            return;
        }
    }
words_loop_end:
    if(in_id==-1){
        in_id = i+1;
        while(in_word!=wordWidgetFor(core->words[in_id])){
            ++in_id;
        }
    }else --i;
    core->words.insert(i,core->words.takeAt(in_id));
    layout->insertItem(i,layout->takeAt(in_id));
    core->edited_with_layout();
}

void teWordWidgetBase::initialize(){
    setContentsMargins(0,0,0,0);
    setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
    setMaximumHeight(27);
    setStyleSheet(QStringLiteral(R"(
teWordWidgetBase{
    color: white;
}
QWidget{
    background-color:transparent;
}
teWordWidgetBase:hover{
    border:1px solid #21ffbd;
    padding:-1px;
}
teWordWidgetBase:!hover{
    border:1px solid transparent;
    padding:-1px;
}
)"));
}

void teWordWidgetBase::readCore(teWord *in){
    // attachWord() - not a plain assignment - so the widget also forgets `in`
    // again if the word is deleted while it is still being shown; see tetag.h.
    attachWord(in);
    QLabel::setText(*in);
}

bool teWordWidgetBase::event(QEvent *e){
    if(e->type()==QEvent::Type::MouseButtonDblClick){
        mouseDoubleClickEvent((QMouseEvent*)e);
        return true;
    }else QLabel::event(e);
}

void teWordWidgetBase::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        start_x = event->pos().x();
        mousePosInWidget = start_x;
        isDragging = false;
        raise();
    }
    return ((teTagWidgetBase*)parent())->mousePressEvent(event);
}

void teWordWidgetBase::mouseMoveEvent(QMouseEvent *event) {
    if (event->buttons() & Qt::LeftButton) {
        int current_x = event->pos().x();
        int distance = current_x - start_x;
        if (!isDragging) {
            isDragging = true;
        }
        if (isDragging) {
            move(mapToParent(QPoint{distance,0}));
        }
    }
    // Only a tag may own a word: the cast used to be unconditional, so a word
    // whose parent was gone (a pooled widget that was never re-parented) crashed
    // here instead of doing nothing.
    if (auto* tagParent = qobject_cast<teTagWidgetBase*>(parent()))
        tagParent->mouseMoveEvent(event);
}

void teWordWidgetBase::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        isDragging = false;
    }
    emit droped(this,mapToParent(event->pos()).x());
    return ((teTagWidgetBase*)parent())->mouseReleaseEvent(event);
}

void teWordWidgetBase::mouseDoubleClickEvent(QMouseEvent *event){
    emit mouseDoubleClicked(this);
}


QString teTag_normalStyle(QStringLiteral(R"(
teTagWidgetBase:!hover{
    border:1px solid #43367b;
    background-color:black;
}
teTagWidgetBase:hover{
    border:1px solid #b288ff;
    background-color:black;
}
)"));
QString teTag_selectStyle(QStringLiteral(R"(
teTagWidgetBase:!hover{
    border:1px solid #3e5e4b;
    background-color:#173d3e;
}
teTagWidgetBase:hover{
    border:1px solid #2a6b45;
    background-color:#173d3e;
}
)"));
QString teTag_selectCurrentStyle(QStringLiteral(R"(
teTagWidgetBase:!hover{
    border:1px solid #109452;
    background-color:#1a5759;
}
teTagWidgetBase:hover{
    border:1px solid #00d96d;
    background-color:#1a5759;
}
)"));

QString teTagMulti_normalStyle(QStringLiteral(R"(
teTagWidgetBase:!hover{
    border:1px solid #90922a;
    background-color:black;
}
teTagWidgetBase:hover{
    border:1px solid #e9ec47;
    background-color:black;
}
)"));
QString teTagMulti_selectStyle(QStringLiteral(R"(
teTagWidgetBase:!hover{
    border:1px solid #3dc22b;
    background-color:#102b18;
}
teTagWidgetBase:hover{
    border:1px solid #4ce638;
    background-color:#153f22;
}
)"));
QString teTagMulti_selectCurrentStyle(QStringLiteral(R"(
teTagWidgetBase:!hover{
    border:1px solid #58d347;
    background-color:#206836;
}
teTagWidgetBase:hover{
    border:1px solid #69f057;
    background-color:#247f40;
}
)"));



