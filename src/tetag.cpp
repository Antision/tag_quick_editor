#include "tetag.h"
#include "teeditorcontrol.h"
teTag::teTag(const QString &str,QWidget*parent):
    teTagBase(std::make_shared<tetagcore>(str),parent)
{
    core->widget=this;
    load();
    setStyle(teTag::normal);
}

void teTag::readCore(std::shared_ptr<tetagcore>in_core){
    if((core!=nullptr)&&core!=in_core){
        core->widget=nullptr;
    }
    core = in_core;
    in_core->teConnect(teCallbackType::edit,this,&teTag::load);
    in_core->widget=this;

    if(in_core->type==teTagCore::sentence)
        setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);
    load();
}

teTag::teTag(std::shared_ptr<tetagcore>incore,QWidget*parent):teTagBase(incore,parent){
    if(incore->widget&&incore->widget!=this){
        widgetpool.give_back(incore->widget);
    }
    incore->widget=this;
    load();
    setStyle(teTag::normal);
}
teTagBase::teTagBase(std::shared_ptr<tetagcore>incore, QWidget *parent){
    core=incore;
    initialize();
}

void teTagBase::clearWordWidgets(){
    for(teWordBase*word:findChildren<teWordBase*>()){
        widgetpool.give_back(word);
    }
}

void teTagBase::takeWordWidgets(){
    int layoutItemCount = layout->count();
    for(int i=0;i<layoutItemCount-1;++i){
        layout->takeAt(0);
    }
}

void teTagBase::initialize(){
    setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Fixed);
    layout = new QHBoxLayout(this);
    QSpacerItem* rightspacer = new QSpacerItem(0,0,QSizePolicy::Expanding,QSizePolicy::Fixed);
    layout->addSpacerItem(rightspacer);
    layout->setContentsMargins(2,4,0,4);
}

extern QString tetag_selectCurrentStyle;
extern QString tetag_selectStyle;
extern QString tetag_normalStyle;
extern QString tetagMulti_selectCurrentStyle;
extern QString tetagMulti_selectStyle;
extern QString tetagMulti_normalStyle;
void teTagBase::setStyle(tetagStyle in){
    switch(in){
    case normal:setStyleSheet(tetag_normalStyle);break;
    case select:setStyleSheet(tetag_selectStyle);break;
    case select_current:setStyleSheet(tetag_selectCurrentStyle);break;
    case multi:setStyleSheet(tetagMulti_normalStyle);break;
    case multi_select:setStyleSheet(tetagMulti_selectStyle);break;
    case multi_select_current:setStyleSheet(tetagMulti_selectCurrentStyle);break;

    default:telog("unknown tetag style");
    }
}

teTagBase::~teTagBase(){
    for(teWordBase*word:findChildren<teWordBase*>()){
        word->setParent(this->parentWidget());
    }
    if(core!=nullptr){
        core->widget=nullptr;
        core.reset();
    }
}

teTagBase& teTagBase::operator=(const QString& input_string){
    setText(input_string);
    return *this;
};

teTagBase &teTagBase::operator=(const teTagBase &in){
    setText(in);
    core->weight=in.core->weight;
    core->type=in.core->type;
    return *this;
}

bool teTagBase::operator==(const teTag &in) const{
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

int teTagBase::fit_goodness(const QString &find_word, int *return_index, int *return_questionable_index) const{
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

void teTag::load(){
    static int tetagLoadNum=0;
    int wordscount = core->words.count();
    clearWordWidgets();
    for(int i=0;i<wordscount;++i){
        teWordBase* newword = widgetpool.getWord(core->words[i]);
        layout->insertWidget(i,newword);
        connectWord(newword);
    }
    if(wordscount==0){
        tewordcore* newwordcore = new tewordcore(QStringLiteral(""));
        teWordBase* newword = widgetpool.getWord(newwordcore);
        core->words.push_back(newword->core);
        layout->insertWidget(0,newword);
        connectWord(newword);
    }
}


void teTagBase::mouseMoveEvent(QMouseEvent *event) {
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
void teTagBase::mousePressEvent(QMouseEvent *event) {
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
void teTagBase::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        emit droped(this,event->modifiers());
    }
}
void teTagBase::worddroped(teWordBase *in_word, int xpos){
    int wordcount = core->words.count();
    int in_id=-1;
    int i=0;
    for(;i<wordcount;++i){
        teWordBase*wordptr = core->words[i]->widget;
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
    return initialize_push_back(std::make_shared<tetagcore>(str, nullptr, forceSentence));
}

int teTagList::initialize_push_back(const std::string &in, bool forceSentence)
{
    return initialize_push_back(QString::fromStdString(in), forceSentence);
}

int teTagList::initialize_push_back(std::shared_ptr<tetagcore> tag)
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
    return -1;
}
int teTagList::insert(int pos, std::shared_ptr<tetagcore> tag, int removeDuplicate, bool ifSendSignal){
    if(removeDuplicate==1&&remove_duplicate(tag,false)){
        return -1;
    }
    connectTag(tag);
    onTagInserted(tag,pos,ifSendSignal);
    if(tag->type==teTagCore::deleteTag){
        disconnectTag(tag);
        return -1;
    }
    if(removeDuplicate==2){
        bool iflocked=inner_lock();
        tags.insert(pos,tag);
        if(iflocked)inner_unlock();
        return remove_duplicate(tag,true);
    }else if(removeDuplicate==1){
        if(remove_duplicate(tag,false)){
            disconnectTag(tag);
            return -1;
        }else{
            bool iflocked=false;
            if(!locked_for_edit){
                tagsMt.lock();
                iflocked=true;
                ++locked_for_edit;
            }
            tags.insert(pos,tag);
            if(iflocked){
                --locked_for_edit;
                tagsMt.unlock();
            }
        }
    }else if(removeDuplicate==0){
        std::lock_guard<std::mutex>lg(tagsMt);
        tags.insert(pos,tag);
    }
    return 0;
}

QString teTagList::toText()
{
    return serializePieces(size(), [this](int i) -> std::shared_ptr<tetagcore> {
        return tags[i];
    }, true);
}


void teWordBase::initialize(){
    setContentsMargins(0,0,0,0);
    setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
    setMaximumHeight(27);
    setStyleSheet(QStringLiteral(R"(
teWordBase{
    color: white;
}
QWidget{
    background-color:transparent;
}
teWordBase:hover{
    border:1px solid #21ffbd;
    padding:-1px;
}
teWordBase:!hover{
    border:1px solid transparent;
    padding:-1px;
}
)"));
}

void teWordBase::readCore(tewordcore *in){
    core=in;
    QLabel::setText(*in);
}

bool teWordBase::event(QEvent *e){
    if(e->type()==QEvent::Type::MouseButtonDblClick){
        mouseDoubleClickEvent((QMouseEvent*)e);
        return true;
    }else QLabel::event(e);
}

void teWordBase::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        start_x = event->pos().x();
        mousePosInWidget = start_x;
        isDragging = false;
        raise();
    }
    return ((teTagBase*)parent())->mousePressEvent(event);
}

void teWordBase::mouseMoveEvent(QMouseEvent *event) {
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
    return ((teTagBase*)parent())->mouseMoveEvent(event);
}

void teWordBase::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        isDragging = false;
    }
    emit droped(this,mapToParent(event->pos()).x());
    return ((teTagBase*)parent())->mouseReleaseEvent(event);
}

void teWordBase::mouseDoubleClickEvent(QMouseEvent *event){
    emit mouseDoubleClicked(this);
}

teTagCore::teTagCore(const QList<teWordCore *> in, teTag *child):widget(child){
    for(tewordcore*w:in)
        words.push_back(new teWordCore(w->text));
}

teTagCore::teTagCore(const QString &str, teTag *child, bool forceSentence):widget(child){
    read(str);
}

teTagCore::teTagCore(const char *str, teTag *child, bool forceSentence):widget(child){
    read(QString(str));
}

teTagCore::teTagCore(teTagCore &&in):words(std::move(in.words)),widget(in.widget){
    info=in.info;
    if(in.widget!=nullptr)
        in.widget->core.reset();
    in.widget=nullptr;
}

void teTagCore::load(){
    if(widget==nullptr)
        widget = (tetag*)widgetpool.getTag(shared_from_this());
    else
        widget->load();
}

teTagCore &teTagCore::operator=(const teTagCore &in){
    if(!words.empty()){
        for(tewordcore*wc:words)
            delete wc;
        words.clear();
    }
    for(tewordcore*wc:in.words)
        words.push_back(new tewordcore{*wc});
    weight=in.weight;
    edited();
    return *this;
}

void teTagCore::read(const QString &str, bool ifclear, bool forceSentence)
{
    if (ifclear)
        clear();

    QString text = str.trimmed();

    // Force sentence mode: used for the result of openpath / splitTextToPieces
    if (forceSentence) {
        if (!text.endsWith('.'))
            text += '.';

        words.push_back(new teWordCore(text));
        type = sentence;

        if (widget)
            widget->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);

        if (ifclear)
            edited();
        return;
    }

    // Manually adding a tag: if it ends with a period, treat it as a sentence
    if (text.endsWith('.')) {
        words.push_back(new teWordCore(text));
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
        words.push_back(new teWordCore(std::move(tmpword)));

    type = tag;

    if (ifclear)
        edited();
}

teWordCore *teTagCore::takeWordAt(int index, bool ifSendSignal){
    while(index<0)index += words.size();
    teWordCore* wc = words.takeAt(index);
    if(wc->widget){
        wc->widget->teDisconnect();
        if(this->widget)
            QApplication::disconnect(wc->widget,0,this->widget,0);
    }
    if(ifSendSignal)
        edited_with_layout();
    return wc;
}

bool teTagCore::operator==(const teTagCore &in) const{
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

teTagCore::operator QString() const
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

    for (tewordcore* word : words) {
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

teTagCore::~teTagCore(){
    onDestroy();
    clear();
    unload();
}

void teTagCore::unload(){
    if(widget!=nullptr){
        widgetpool.give_back(widget);
        widget=nullptr;
    }
    for(tewordcore*w:words)
        w->unload();
}
teWord* teWordCore::load(){
    if(!widget)
        widget = widgetpool.getWord(this);
    return widget;
}

void teWordCore::unload(){
    if(widget!=nullptr){
        widgetpool.give_back(widget);
        widget=nullptr;
    }
}
teWordCore::~teWordCore(){
    unload();
}

QString tetag_normalStyle(QStringLiteral(R"(
teTagBase:!hover{
    border:1px solid #43367b;
    background-color:black;
}
teTagBase:hover{
    border:1px solid #b288ff;
    background-color:black;
}
)"));
QString tetag_selectStyle(QStringLiteral(R"(
teTagBase:!hover{
    border:1px solid #3e5e4b;
    background-color:#173d3e;
}
teTagBase:hover{
    border:1px solid #2a6b45;
    background-color:#173d3e;
}
)"));
QString tetag_selectCurrentStyle(QStringLiteral(R"(
teTagBase:!hover{
    border:1px solid #109452;
    background-color:#1a5759;
}
teTagBase:hover{
    border:1px solid #00d96d;
    background-color:#1a5759;
}
)"));

QString tetagMulti_normalStyle(QStringLiteral(R"(
teTagBase:!hover{
    border:1px solid #90922a;
    background-color:black;
}
teTagBase:hover{
    border:1px solid #e9ec47;
    background-color:black;
}
)"));
QString tetagMulti_selectStyle(QStringLiteral(R"(
teTagBase:!hover{
    border:1px solid #3dc22b;
    background-color:#102b18;
}
teTagBase:hover{
    border:1px solid #4ce638;
    background-color:#153f22;
}
)"));
QString tetagMulti_selectCurrentStyle(QStringLiteral(R"(
teTagBase:!hover{
    border:1px solid #58d347;
    background-color:#206836;
}
teTagBase:hover{
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
