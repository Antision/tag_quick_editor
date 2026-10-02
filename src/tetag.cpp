#include "tetag.h"
#include "teeditorcontrol.h"
teTagWidget::teTagWidget(const QString &str,QWidget*parent):
    teTagWidgetBase(std::make_shared<teTag>(str),parent)
{
    core->widget=this;
    load();
    setStyle(teTagWidget::normal);
}

void teTagWidget::readCore(std::shared_ptr<teTag>in_core){
    if((core!=nullptr)&&core!=in_core){
        core->widget=nullptr;
    }
    core = in_core;
    in_core->teConnect(teCallbackType::edit,this,&teTagWidget::load);
    in_core->widget=this;

    if(in_core->type==teTag::sentence)
        setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);
    load();
}

teTagWidget::teTagWidget(std::shared_ptr<teTag>incore,QWidget*parent):teTagWidgetBase(incore,parent){
    if(incore->widget&&incore->widget!=this){
        widgetpool.give_back(incore->widget);
    }
    incore->widget=this;
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
    if(core!=nullptr){
        core->widget=nullptr;
        core.reset();
    }
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
        teWordWidgetBase*wordptr = core->words[i]->widget;
        if(wordptr!=in_word){
            if(xpos < wordptr->x()+wordptr->width()){
                break;
            }
        }else{
            in_id=i;
            for(i=wordcount-1;i>in_id;--i){
                wordptr = core->words[i]->widget;
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
        while(in_word!=core->words[in_id]->widget){
            ++in_id;
        }
    }else --i;
    core->words.insert(i,core->words.takeAt(in_id));
    layout->insertItem(i,layout->takeAt(in_id));
    core->edited_with_layout();
}
int teTagList::initialize_push_back(const QString& str, bool forceSentence)
{
    return initialize_push_back(std::make_shared<teTag>(str, nullptr, forceSentence));
}

int teTagList::initialize_push_back(const std::string &in, bool forceSentence)
{
    return initialize_push_back(QString::fromStdString(in), forceSentence);
}

int teTagList::initialize_push_back(std::shared_ptr<teTag> tag)
{
    if (remove_duplicate(tag, false)) {
        return -1;
    } else {
        tags.append(tag);
        connectTag(tag);
        operationlist.addEditOperation(tag, *tag);
        ++operationlist.inip;
        return tags.size() - 1;
    }
}

bool teTagList::remove_duplicate(std::shared_ptr<teTag> tag, bool keepself)
{
    std::lock_guard<std::recursive_mutex> lg(tagsMt);

    if (tag == nullptr && keepself == true)
        telog("[teTagList::remove_duplicate]:tag==nullptr&&keepself==true");

    bool ret = false;
    if (tag != nullptr) {
        for (int i = 0; i < tags.size(); ) {
            if (*tags[i] == *tag && tags[i] != tag) {
                if (keepself) {
                    erase(i);           // erase() already shrinks the list
                    ret = true;
                    continue;
                }
                return true;
            }
            ++i;
        }
        return ret;
    }

    for (int i = 0; i < tags.size(); ++i) {
        for (int j = i + 1; j < tags.size(); ) {
            if (*tags[i] == *tags[j]) {
                erase(j);
                ret = true;
                continue;
            }
            ++j;
        }
    }
    return ret;
}

void teTagList::erase(int id)
{
    std::lock_guard<std::recursive_mutex> lg(tagsMt);
    if (id < 0 || id >= tags.size()) {
        telog(QString("[teTagList::erase] index %1 out of range (size %2)").arg(id).arg(tags.size()));
        return;
    }
    std::shared_ptr<teTag> core = tags.takeAt(id);
    onTagErased(core, id, isTagsLoaded);
    core->unload();
    core->teDisconnect(this);
    core->teemit(teCallbackType::destroy, false);
}

int teTagList::edit(std::shared_ptr<teTag> core, QString text, int removeDuplicate, bool ifemit)
{
    if (!core)
        return -1;

    {
        std::lock_guard<std::recursive_mutex> lg(tagsMt);
        core->read(text);
    }

    if (removeDuplicate == 1 || removeDuplicate == 2) {
        // Merging: the tag the user just renamed wins, every identical tag is
        // erased. Returning early here (as the old code did) is exactly what
        // left two identical tags in the list.
        remove_duplicate(core, true);
    }

    onTagEdited(core, ifemit);
    return core->type == teTag::deleteTag ? -1 : 0;
}

int teTagList::edit(int index, QString text, int removeDuplicate, bool ifemit)
{
    std::shared_ptr<teTag> core;
    {
        std::lock_guard<std::recursive_mutex> lg(tagsMt);
        if (index < 0 || index >= tags.size()) {
            telog(QString("[teTagList::edit] index %1 out of range (size %2)").arg(index).arg(tags.size()));
            return -1;
        }
        core = tags.at(index);
    }
    return edit(core, text, removeDuplicate, ifemit);
}

int teTagList::insert(int pos, std::shared_ptr<teTag> tag, int removeDuplicate, bool ifSendSignal)
{
    if (!tag)
        return -1;
    if (removeDuplicate == 1 && remove_duplicate(tag, false))
        return -1;
    if (tag->type == teTag::deleteTag)
        return -1;

    connectTag(tag);
    {
        std::lock_guard<std::recursive_mutex> lg(tagsMt);
        pos = std::clamp(pos, 0, int(tags.size()));
        tags.insert(pos, tag);
    }
    if (removeDuplicate == 2)
        remove_duplicate(tag, true);

    // Announce the insertion *after* the list is consistent; the old code
    // emitted before inserting, so a slot that inspected the list saw the tag
    // missing (and a rejected deleteTag was announced as well).
    onTagInserted(tag, pos, ifSendSignal);
    return 0;
}

void teTagList::move(int originPos, int newPos)
{
    std::shared_ptr<teTag> taketag;
    {
        std::lock_guard<std::recursive_mutex> lg(tagsMt);
        if (originPos < 0 || originPos >= tags.size()) {
            telog(QString("[teTagList::move] origin %1 out of range (size %2)").arg(originPos).arg(tags.size()));
            return;
        }
        taketag = tags.takeAt(originPos);
        newPos = std::clamp(newPos, 0, int(tags.size()));
        tags.insert(newPos, taketag);
    }
    onTagMoved(taketag, originPos, newPos);
}

void teTagList::move(std::shared_ptr<teTag> tag, int newPos)
{
    const int o = tags.indexOf(tag);
    if (o < 0) {
        telog("[teTagList::move]:could not find input tag in taglist");
        return;
    }
    move(o, newPos);
}

void teTagList::reorderBlock(int from, int count, int insertAt)
{
    if (count <= 0)
        return;

    QVector<std::shared_ptr<teTag>> block;
    {
        std::lock_guard<std::recursive_mutex> lg(tagsMt);
        if (from < 0 || from + count > int(tags.size())) {
            telog(QString("[teTagList::reorderBlock] block %1+%2 out of range (size %3)")
                      .arg(from).arg(count).arg(tags.size()));
            return;
        }
        block.reserve(count);
        for (int i = 0; i < count; ++i)
            block.append(tags.takeAt(from));
        insertAt = std::clamp(insertAt, 0, int(tags.size()));
        for (int i = 0; i < count; ++i)
            tags.insert(insertAt + i, block[i]);
    }

    isSaved = false;
    for (int i = 0; i < count; ++i) {
        if (recordingChanges())
            operationlist.addMoveOperation(block[i], from + i, insertAt + i);
    }
    emit tagMoved(block.first());
}

QString teTagList::toText()
{
    return serializePieces(size(), [this](int i) -> std::shared_ptr<teTag> {
        return tags[i];
    }, true);
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
    core=in;
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
    return ((teTagWidgetBase*)parent())->mouseMoveEvent(event);
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

teTag::teTag(const QList<teWord *> in, teTagWidgetBase *child):widget(child){
    for(teWord*w:in)
        words.push_back(new teWord(w->text));
}

teTag::teTag(const QString &str, teTagWidgetBase *child, bool forceSentence):widget(child){
    // The flag used to be dropped here, so a tag built as a sentence came out as
    // a plain tag and was then edited in the wrong window.
    read(str,true,forceSentence);
}

teTag::teTag(const char *str, teTagWidgetBase *child, bool forceSentence):widget(child){
    read(QString(str),true,forceSentence);
}

teTag::teTag(teTag &&in):words(std::move(in.words)),widget(in.widget){
    info=in.info;
    if(in.widget!=nullptr)
        in.widget->core.reset();
    in.widget=nullptr;
}

void teTag::load(){
    if(widget==nullptr)
        widget = (teTagWidget*)widgetpool.getTag(shared_from_this());
    else
        widget->load();
}

teTag &teTag::operator=(const teTag &in){
    if(!words.empty()){
        for(teWord*wc:words)
            delete wc;
        words.clear();
    }
    for(teWord*wc:in.words)
        words.push_back(new teWord{*wc});
    weight=in.weight;
    edited();
    return *this;
}

void teTag::read(const QString &str, bool ifclear, bool forceSentence)
{
    if (ifclear)
        clear();

    QString text = str.trimmed();

    // Force sentence mode: used for the result of openpath / splitTextToPieces
    if (forceSentence) {
        if (!text.endsWith('.'))
            text += '.';

        words.push_back(new teWord(text));
        type = sentence;

        if (widget)
            widget->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);

        if (ifclear)
            edited();
        return;
    }

    // Manually adding a tag: if it ends with a period, treat it as a sentence
    if (text.endsWith('.')) {
        words.push_back(new teWord(text));
        type = sentence;

        if (widget)
            widget->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);

        if (ifclear)
            edited();
        return;
    }

    // Logic for normal tags
    QStringList result;
    QStringList wordlist;

    if (text.indexOf("(") != -1 || text.indexOf(")") != -1) {
        QRegularExpression regex(R"(\\\(|\(|\\\)|\))");
        int lastIndex = 0;
        QRegularExpressionMatchIterator it = regex.globalMatch(text);
        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();
            int start = match.capturedStart();
            if (start > lastIndex)
                result.append(text.mid(lastIndex, start - lastIndex));
            result.append(match.captured());
            lastIndex = match.capturedEnd();
        }
        if (lastIndex < text.length())
            result.append(text.mid(lastIndex));
    } else {
        result = {text};
    }

    for (QString s : result)
        wordlist.append(s.split(" ", Qt::SkipEmptyParts));

    for (QString &tmpword : wordlist)
        words.push_back(new teWord(std::move(tmpword)));

    type = tag;

    if (ifclear)
        edited();
}

teWord *teTag::takeWordAt(int index, bool ifSendSignal){
    while(index<0)index += words.size();
    teWord* wc = words.takeAt(index);
    if(wc->widget){
        wc->widget->teDisconnect();
        if(this->widget)
            QApplication::disconnect(wc->widget,0,this->widget,0);
    }
    if(ifSendSignal)
        edited_with_layout();
    return wc;
}

bool teTag::operator==(const teTag &in) const{
    if(this->words.size()!=in.words.size()) return false;
    else{
        int size = words.size();
        for(int i =0;i<size;++i){
            if(words[i]->text!=in.words[i]->text)
                return false;
        }
        return true;
    }
}

teTag::operator QString() const
{
    QString out;

    auto isOpenParen = [](const QString& s) {
        return s == "(" || s == R"(\()";
    };

    auto isCloseParen = [](const QString& s) {
        return s == ")" || s == R"(\))";
    };

    QString prev;
    bool first = true;

    for (teWord* word : words) {
        if (!word)
            continue;

        const QString cur = word->text;

        if (first) {
            out += cur;
            prev = cur;
            first = false;
            continue;
        }

        // 唯一不加空格的两种情况：
        // 1. 左括号后
        // 2. 右括号前
        if (!isOpenParen(prev) && !isCloseParen(cur))
            out += ' ';

        out += cur;
        prev = cur;
    }

    return out;
}

teTag::~teTag(){
    onDestroy();
    clear();
    unload();
}

void teTag::unload(){
    if(widget!=nullptr){
        widgetpool.give_back(widget);
        widget=nullptr;
    }
    for(teWord*w:words)
        w->unload();
}
teWordWidget* teWord::load(){
    if(!widget)
        widget = widgetpool.getWord(this);
    return widget;
}

void teWord::unload(){
    if(widget!=nullptr){
        widgetpool.give_back(widget);
        widget=nullptr;
    }
}
teWord::~teWord(){
    unload();
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



QList<ParsedPiece> splitTextToPieces(const QString& raw)
{
    QList<ParsedPiece> out;

    QString text = raw;
    text.replace("\r\n", "\n");
    text.replace('\r', '\n');

    const QStringList lines =
        text.split('\n', Qt::KeepEmptyParts);

    // 判断当前位置的 "..." 是否属于连续三个点。
    // 只要这里是 "...", 就绝不能把其中任何一个 '.' 当成句号。
    auto isEllipsis = [](const QString& line, int pos) -> bool {
        return pos >= 0 &&
               pos + 3 <= line.size() &&
               line.mid(pos, 3) == "...";
    };

    // 找真正的句号。
    //
    // 例如：
    //   "abc."       -> 找到
    //   "d..."       -> 找不到
    //   "..."        -> 找不到
    //   "abc...,def" -> 找不到
    auto findSentenceDot =
        [&](const QString& line, int start) -> int
    {
        int pos = start;

        while (pos < line.size()) {
            if (line[pos] != '.') {
                ++pos;
                continue;
            }

            // "...": 整组三个点跳过去
            if (isEllipsis(line, pos)) {
                pos += 3;
                continue;
            }

            // 单独的 '.' 才是真正的句号
            return pos;
        }

        return -1;
    };

    for (QString line : lines) {
        line = line.trimmed();

        if (line.isEmpty())
            continue;

        /*
         * 先判断这一行有没有真正的句号。
         *
         * 没有真正句号：
         *     完全按照普通 tag 处理。
         *
         * 例如：
         *     ...,a,b,d...,...
         *
         * 得到：
         *     ...
         *     a
         *     b
         *     d...
         *     ...
         */
        const int firstDot =
            findSentenceDot(line, 0);

        if (firstDot < 0) {
            const QStringList parts =
                line.split(',', Qt::SkipEmptyParts);

            for (QString part : parts) {
                part = part.trimmed();

                if (!part.isEmpty())
                    out.push_back({part, false});
            }

            continue;
        }

        /*
         * 这一行存在真正的句号，
         * 因此按照 sentence 解析。
         *
         * 注意：
         * sentence 内部的逗号全部保留，
         * 不能再用 ',' 拆分。
         */
        int start = 0;

        while (start < line.size()) {
            const int dot =
                findSentenceDot(line, start);

            if (dot < 0) {
                QString tail =
                    line.mid(start).trimmed();

                if (!tail.isEmpty()) {
                    const QStringList parts =
                        tail.split(',', Qt::SkipEmptyParts);

                    for (QString part : parts) {
                        part = part.trimmed();

                        if (!part.isEmpty())
                            out.push_back({part, false});
                    }
                }

                break;
            }

            QString piece =
                line.mid(start, dot - start + 1).trimmed();

            if (!piece.isEmpty())
                out.push_back({piece, true});

            start = dot + 1;

            while (start < line.size() &&
                   line[start].isSpace()) {
                ++start;
            }
        }
    }

    return out;
}
