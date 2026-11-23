#include "teeditorcontrol.h"
#include "tagmarks.h"
#include "tetag.h"
#include "tetaglistwidget.h"

TagHandling EditorControl::re_read(std::shared_ptr<teTag>tag){
    if(!filter(tag)){
        if(tag->retired){
            if(taglistwidget)
                taglistwidget->retire(tag);
            return TagHandling::Consumed;
        }
        unlink(tag);
        return TagHandling::NotMine;
    }
    // The text changed (a merge, an edit): keep the decoration in step. The
    // decoration page skips claimed tags, so this is the only writer here.
    refreshTagDecoration(tag);
    if(auto* widget=tagWidgetFor(tag))
        widget->refreshStyle();
    return TagHandling::Linked;
}

TagHandling EditorControl::read(std::shared_ptr<teTag>tag){
    const bool iffilt = filter(tag);
    if(tag->retired){
        if(taglistwidget)
            taglistwidget->retire(tag);
        return TagHandling::Consumed;
    }
    if(iffilt){
        // Write the decoration before the tag widget is created/linked: the
        // widget reads it while building its sheet, so a tag switched to for the
        // first time is coloured immediately instead of on the second visit.
        ensureTagDecoration(tag);
        if(taglistwidget)
            taglistwidget->ensureWidgetFor(tag);
        link(tag);
        if(tag->retired){
            if(taglistwidget)
                taglistwidget->retire(tag);
            return TagHandling::Consumed;
        }
        return TagHandling::Linked;
    }
    return TagHandling::NotMine;
}

void EditorControl::link(std::shared_ptr<teTag>in_tag){
    // A control that claims a tag owns its look too. The decoration page only
    // handles tags nobody claimed, so without this a "blonde hair" tag would be
    // skipped by the decoration control and lose its colour entirely.
    ensureTagDecoration(in_tag);
    in_tag->teConnect(teCallbackType::destroy,this,&EditorControl::unlink,in_tag);
    in_tag->teConnect(teCallbackType::edit,this,&EditorControl::re_read,in_tag);
    in_tag->teConnect(teCallbackType::edit_with_layout,this,&EditorControl::re_read,in_tag);
    linked_tags.insert(in_tag);
    applyMarks();               // MARK-02 A: the claim carries the look
    refreshState();
}

void EditorControl::applyMarks(){
    if(mark.isEmpty())
        return;
    const QString owner=controlId();
    if(owner.isEmpty())
        return;                 // a control without a name owns no mark
    markOwner=owner;
    // No local copy: setMark() on an unchanged value is a no-op (see teTag), so
    // re-linking a tag does not send it through every control again.
    for(const std::shared_ptr<teTag>& tag:linked_tags)
        if(tag)
            tag->setMark(owner,mark);
}

QString EditorControl::controlId() const{
    // The widget's object name, when there is one: the secondary menus are named
    // after it and the pages already use it for the QSS.
    if(const auto* widget=dynamic_cast<const QWidget*>(this))
        return widget->objectName();
    return QString();
}

QRect EditorControl::visualRect() const{
    // Every control is added to a page, but the mixin cannot assume which widget
    // that is - a control that is drawn inside another one overrides this.
    if(const auto* widget=dynamic_cast<const QWidget*>(this))
        return QRect(widget->mapToGlobal(QPoint(0,0)),widget->size());
    return QRect();
}

void EditorControl::unlink(std::shared_ptr<teTag>in_tag){
    dropClaim(in_tag,linked_tags);
}

void EditorControl::dropClaim(std::shared_ptr<teTag>in_tag,std::set<std::shared_ptr<teTag>>&tags){
    if(!in_tag)
        return;
    // Disconnect first: clearing the mark announces an edit, and this control
    // must not hear its own farewell (a re_read here would re-link the tag).
    in_tag->teDisconnect(this);
    if(!markOwner.isEmpty())
        in_tag->clearMark(markOwner);
    tags.erase(in_tag);
    if(ifrefreshState)
        refreshState();
}

bool EditorControl::linked(std::shared_ptr<teTag>tag){
    return linked_tags.find(tag)!=linked_tags.end();
}

void EditorControl::setTaglistwidget(TagListView *in_taglistwidget){
    taglistwidget=in_taglistwidget;
}

teTagWidgetBase* EditorControl::tagWidgetFor(std::shared_ptr<teTag> core) const
{
    // Every editor control is also a tag list (it shows its linked tags as
    // widgets), so the lookup of the tag list base class applies.
    auto* list = dynamic_cast<teTagListWidgetBase*>(const_cast<EditorControl*>(this));
    return list?list->widgetForCore(core):nullptr;
}

void EditorControl::edited(){
    teemit(teCallbackType::edit);
}

void EditorControl::dropAll(std::set<std::shared_ptr<teTag>>&tags,teObject*owner){
    // One at a time, with the tag held alive by a local shared_ptr: a disconnect
    // must not run on a tag whose last reference the erase just dropped.
    while(!tags.empty()){
        std::shared_ptr<teTag> tag = *tags.begin();
        tags.erase(tags.begin());
        tag->teDisconnect(owner);
    }
}

void EditorControl::markMerged(teTag& survivor,teTag& absorbed){
    survivor.mergeSuppressed=true;
    absorbed.mergeSuppressed=true;
    if(taglistwidget&&taglistwidget->showing_list)
        taglistwidget->showing_list->noteMergedForUndo(survivor,absorbed);
}

void EditorControl::clearLinks(){
    // Same order as dropClaim(), per tag: the mark goes with the claim, and the
    // tag is held by a local shared_ptr while both happen.
    while(!linked_tags.empty()){
        std::shared_ptr<teTag> tag=*linked_tags.begin();
        linked_tags.erase(linked_tags.begin());
        tag->teDisconnect(this);
        if(!markOwner.isEmpty())
            tag->clearMark(markOwner);
    }
}

teTagButtonGroup::teTagButtonGroup(const QVector<in_item>& in_buttons, QWidget *parent, QString *styleSheet_button, QString *styleSheet_label)
    :QWidget(parent),group(new QButtonGroup(this)){
    layout = new QHBoxLayout(this);
    int button_id=0;
    group->setExclusive(false);
    for(auto&[text,data,ifbutton,ifspace,ifvp]:in_buttons){
        QWidget* w=nullptr;
        if(ifbutton){
            QPushButton* btn = new QPushButton(text);
            w = btn;
            btn->setCheckable(true);
            if(styleSheet_button) btn->setStyleSheet(*styleSheet_button);
            group->addButton(btn,button_id++);
            buttons.push_back(btn);
        }else {
            QLabel*lbl = new QLabel(text,this);
            w= (QWidget*)lbl;
            if(styleSheet_label) lbl->setStyleSheet(*styleSheet_button);
            lbl->setAlignment(Qt::AlignHCenter|Qt::AlignVCenter);
        }
        allwidgets.push_back({w,data,ifbutton,ifspace,ifvp});
        layout->addWidget(w);
    }
    layout->setContentsMargins(0,0,0,0);
    layout->setSpacing(0);
    // No delimiter inside a group: its buttons are one control (`open | closed |
    // mouth` is one meaning, not three). Separators go *between* groups - see
    // teEditor_standard::addRowTo and colorsWidget::addSectionToContent.
    connect(group, &QButtonGroup::idClicked, this, &teTagButtonGroup::onClicked,Qt::DirectConnection);
    getDefaultFiltStrings();
}

void teTagButtonGroup::reset(){
    for(QPushButton* btn:buttons){
        btn->setChecked(false);
    }
}

void teTagButtonGroup::onClicked(int id){

    if(!taglistwidget->showing_list)return;
    // -- 多人放宽:每个按钮各自成 tag(见 ButtonRuleSet 上方的通用规则)。
    if(multiTagMode()){
        ifrefreshState=false;
        onClickedMulti(id);
        ifrefreshState=true;
        refreshState();
        edited();
        return;
    }
    // -- 顺从 tag list:列表里已有两条以上本组 tag 时,不再整组重写成一个
    //    词串。点灭的按钮只删自己的 tag;点亮的按钮恢复互斥——其余按钮
    //    (及其 tag)一起灭掉,然后走正常的单 tag 组合路径。
    if(linked_tags.size()>1&&id>=0&&id<buttons.size()){
        const bool turnedOn=buttons[id]->isChecked();
        ifrefreshState=false;
        if(!turnedOn){
            eraseTagsForButton(id);
            ifrefreshState=true;
            refreshState();
            edited();
            return;
        }
        while(!linked_tags.empty())
            taglistwidget->tagErase(*linked_tags.begin());
        for(int other=0;other<buttons.size();++other)
            if(other!=id&&buttons[other]->isChecked())
                buttons[other]->setChecked(false);
    }
    reform(id);
    bool atLeastOneButtonTriggered=false;
    ifrefreshState=false;
    QString final_string="";
    for(auto&[widget,data,button,space,variablePos]:allwidgets){
        if(button){
            QPushButton* btn = (QPushButton*)widget;
            if(btn->isChecked()){
                atLeastOneButtonTriggered=true;
                final_string.append(data);
                if(space)
                    final_string.append(QStringLiteral(" "));
            }
        }else{
            final_string.append(data);
            if(space)
                final_string.append(QStringLiteral(" "));
        }
    }
    if(atLeastOneButtonTriggered){
        final_string.chop(1);
        if(linked_tags.empty()){
            std::shared_ptr<teTag>newtagcore = std::make_shared<teTag>(final_string);
            link(newtagcore);
            taglistwidget->tagInsertAbove(false, newtagcore);
        }else{
            while(linked_tags.size()>1){
                taglistwidget->tagErase(*linked_tags.begin());
            }
            // A button group is not a tag list, so it has no widget of its own:
            // the composed text goes straight to the tag core, and the lists
            // showing that tag pick it up through the core's edit signal. (This
            // used to write to core->widget - the single image tag list's widget,
            // which does not exist any more.)
            (*linked_tags.begin())->read(final_string,true);
        }
    }else{
        while(!linked_tags.empty())
            taglistwidget->tagErase(*linked_tags.begin());
        // 恢复这个开关再返回:漏掉它会让控件从此不再 refreshState
        // (按钮停在旧状态,后续 unlink 也不再刷新)。
        ifrefreshState=true;
        refreshState();
        edited();
        return;
    }
    ifrefreshState=true;
    refreshState();
    edited();
}
void teTagButtonGroup::clear(){
    resetPolicy();                              // the subclass's own state first
    setStyleSheetOnce(this,"color:white;");
    clearLinks();
    reset();
}
void teTagButtonGroup::getDefaultFiltStrings(){

    QList<item> movableItems, fixedItems;
    QVector<int> movableIndexes;
    int widgetCount = allwidgets.size();
    for (int i =0 ;i<widgetCount;++i) {
        item w = allwidgets[i];
        if (w.ifvariablePos){
            movableItems.append(w);
            movableIndexes.append(i);
        }else
            fixedItems.append(w);
    }

    std::sort(movableItems.begin(), movableItems.end(), [](const item& a, const item& b) {
        return a.data < b.data;
    });

    do {
        QList<item> combined;
        combined.reserve(allwidgets.size());
        combined.append(fixedItems);
        {
            int insertIndex=-1;
            for(int i:movableIndexes)
                combined.insert(i,movableItems[++insertIndex]);
        }

        int btnCount = combined.size();
        for (int mask = 0; mask < (1 << btnCount); ++mask) {
            QString composedText;
            bool addSpace = false;

            for (int i = 0; i < btnCount; ++i) {
                bool active = combined[i].ifbutton ? (mask & (1 << i)) : true;

                if (active) {
                    if (addSpace) {
                        composedText += " ";
                    }
                    composedText += combined[i].data;
                    addSpace = combined[i].ifspace;
                }
            }

            if (!composedText.isEmpty()) {
                defaultFiltStrings.insert(composedText);
            }
        }
    } while (std::next_permutation(movableItems.begin(), movableItems.end(),
                                   [](const item& a, const item& b) { return a.data < b.data; }));
}
bool teTagButtonGroup::filter(std::shared_ptr<teTag>tag) const{
    if(defaultFiltStrings.find(*tag)!=defaultFiltStrings.end()){
        return true;
    }else return false;
}

bool teTagButtonGroup::multiTagMode() const
{
    return false;
}

QString teTagButtonGroup::tagTextForButton(int buttonId) const
{
    // onClicked() 的同一套组合规则,只是"只点亮这一个按钮":ifbutton==false
    // 的项是共享词,照旧附在后面("open mouth" / "cat ears" / "mature male")。
    QString text;
    for(int i=0;i<int(allwidgets.size());++i){
        const item& entry=allwidgets[i];
        if(entry.ifbutton&&i!=buttonId)
            continue;
        text.append(entry.data);
        if(entry.ifspace)
            text.append(QLatin1Char(' '));
    }
    while(text.endsWith(QLatin1Char(' ')))
        text.chop(1);
    return text;
}

void teTagButtonGroup::insertTagForButton(int buttonId)
{
    const QString text=tagTextForButton(buttonId);
    if(text.isEmpty()||!taglistwidget||!taglistwidget->showing_list)
        return;
    // 同文 tag 已经在列表里:不再插入(link 一个被列表拒收的 tag 会让
    // 按钮亮在一条看不见的 tag 上)。
    for(const std::shared_ptr<teTag>& tag:taglistwidget->showing_list->getTags())
        if(tag&&!tag->retired&&static_cast<QString>(*tag)==text)
            return;
    std::shared_ptr<teTag> newtag=std::make_shared<teTag>(text);
    link(newtag);
    taglistwidget->tagInsertAbove(false,newtag);
}

void teTagButtonGroup::eraseTagsForButton(int buttonId)
{
    const QString text=tagTextForButton(buttonId);
    if(text.isEmpty()||!taglistwidget)
        return;
    // A copy: erasing a tag destroys it, and the destroy callback unlinks it -
    // looping on the live set would mutate what it walks.
    const std::set<std::shared_ptr<teTag>> claimed=linked_tags;
    for(const std::shared_ptr<teTag>& tag:claimed)
        if(tag&&static_cast<QString>(*tag)==text)
            taglistwidget->tagErase(tag);
}

void teTagButtonGroup::onClickedMulti(int buttonId)
{
    if(buttonId<0||buttonId>=buttons.size())
        return;
    if(buttons[buttonId]->isChecked())
        insertTagForButton(buttonId);
    else
        eraseTagsForButton(buttonId);
}

void teTagButtonGroup::refreshState(){
    for(auto&[widget,data,ifbutton,ifspace,ifvariablePos]:allwidgets){
        if(!ifbutton)goto nextbutton;
        for(std::shared_ptr<teTag>tag:linked_tags){
            if(ifspace){
                for(teWord*word:*tag){
                    if(*word==data){
                        ((QPushButton*)widget)->setChecked(true);
                        goto nextbutton;
                    }
                }
            }else{
                for(teWord*word:*tag){
                    if(word->text.size()>data.size()&&word->text.indexOf(data)==0){
                        QString suffix = word->text.last(word->text.size()-data.size());
                        for(auto&[widget2,data2,ifbutton2,ifspace2,ifvariablePos2]:allwidgets)
                        {
                            if(ifspace2){
                                if(data2==suffix){
                                    ((QPushButton*)widget)->setChecked(true);
                                    goto nextbutton;
                                }
                            }else continue;
                        }
                        continue;
                    }
                }
            }
        }
        ((QPushButton*)widget)->setChecked(false);
    nextbutton:;
    }
}



teTagLineedit::teTagLineedit(QString &&text):label(new QLabel{std::move(text)}){
    layout->addWidget(label);
    layout->addWidget(lineedit);
    layout->setContentsMargins(0,0,0,0);
    layout->setSpacing(1);
    connect(lineedit,&QLineEdit::textEdited,this,&teTagLineedit::onEditingFinished,Qt::DirectConnection);
}

void teTagLineedit::onEditingFinished(){
    if(!taglistwidget->showing_list)return;
    QString tagstr="";
    tagstr.reserve(lineedit->text().size()+label->text().size()+1);
    QStringList l = lineedit->text().split(' ',Qt::SkipEmptyParts);
    for(QString&str:l){
        tagstr.append(std::move(str));
        tagstr.append(' ');
    }
    if(label->text().isEmpty())tagstr.removeLast();
    else tagstr.append(label->text());
    if (linked_tags.empty()) {
        std::shared_ptr<teTag>newtagcore = std::make_shared<teTag>(tagstr);
        link(newtagcore);
        taglistwidget->tagInsertAbove(false, newtagcore);
    }else
        taglistwidget->tagEdit(*linked_tags.begin(),tagstr);
    refreshState();
    edited();
}

void teTagLineedit::clear(){
    setStyleSheetOnce(this,"color:white;");
    clearLinks();
    reset();
    edited();
}

void teTagLineedit::reset(){
    lineedit->clear();
}

teTagCheckBox::teTagCheckBox(QList<QString>&& in_strings, QWidget *parent, QString *stylesheet):QPushButton(*in_strings.begin(),parent){
    connect(this, &QPushButton::toggled,this,&teTagCheckBox::onStateChanged,Qt::DirectConnection);
    setCheckable(true);
    if(stylesheet!=nullptr){
        setStyleSheet(*stylesheet);
    }
    if(in_strings.size()>1)
        in_strings.remove(0);
    strings.append(std::move(in_strings));

}
void teTagCheckBox::reset(){
    unselect();
}
void teTagCheckBox::clear(){
    setStyleSheetOnce(this,"color:white;");
    clearLinks();
    reset();
}

void teTagCheckBox::addString(QList<QString> in_strings){
    strings.append(in_strings);
}

void teTagCheckBox::clearString(){
    strings.clear();
}


void teTagCheckBox::select(){
    UpdateGuard guard(excute);
    setChecked(true);
}

void teTagCheckBox::unselect(){
    UpdateGuard guard(excute);
    setChecked(false);
}

bool teTagCheckBox::filter(std::shared_ptr<teTag>tag) const{
    if(strings.indexOf(QString(*tag))>-1){
        return true;
    }return false;
}

void teTagCheckBox::eraseOwnedTags(){
    if(!taglistwidget||!taglistwidget->showing_list)
        return;
    // A copy: erasing a tag destroys it, and its destroy callback removes it from
    // linked_tags - looping over the live set would mutate what it walks.
    const std::set<std::shared_ptr<teTag>> owned = linked_tags;
    for(const std::shared_ptr<teTag>& tag : owned)
        if(taglistwidget->showing_list->find(tag)>=0)
            taglistwidget->tagErase(tag);
    // Anything left was never shown in the list, so no erase will announce it.
    if(!linked_tags.empty())
        clearLinks();
}

void teTagCheckBox::onStateChanged(bool state){
    if(!taglistwidget->showing_list)return;
    if(excute>0){
        if(!state){
            eraseOwnedTags();
        }
        else{
            for(QString&string:strings){
                taglistwidget->tagInsertAbove(false,std::make_shared<teTag>(string));
            }
        }
    }
    edited();
}

void teTagCheckBox::refreshState(){
    if(linked_tags.empty()){
        unselect();
    }
    else{
        select();
    }
}

void teTagCheckBoxPlus::linkCounted(std::shared_ptr<teTag>in_tag){
    in_tag->teConnect(teCallbackType::destroy,this,&teTagCheckBoxPlus::unlinkCounted,in_tag);
    in_tag->teConnect(teCallbackType::edit,this,&teTagCheckBoxPlus::re_readCounted,in_tag);
    in_tag->teConnect(teCallbackType::edit_with_layout,this,&teTagCheckBoxPlus::re_readCounted,in_tag);
    counted_tags.insert(in_tag);
}

void teTagCheckBoxPlus::unlinkCounted(std::shared_ptr<teTag>tag){
    dropClaim(tag,counted_tags);
    // The box says "multiple <nouns>" *because* it counts tags ("2girls"). When the
    // last count goes away there is nothing left to be multiple about, so the tag it
    // wrote goes with them - clearing the count field (the number pad's 0) has to
    // take "multiple girls" with it, not leave it claiming an empty picture.
    if(counted_tags.empty()&&!linked_tags.empty())
        eraseOwnedTags();
}

TagHandling teTagCheckBoxPlus::read(std::shared_ptr<teTag>tag){
    if(!tag)
        return TagHandling::NotMine;

    bool claimed = false;
    if(filter(tag)){
        if(taglistwidget)
            taglistwidget->ensureWidgetFor(tag);
        link(tag);
        claimed = true;
    }else if(filter2(tag)){
        if(taglistwidget)
            taglistwidget->ensureWidgetFor(tag);
        linkCounted(tag);
        claimed = true;
        // A counted tag only matters while the box is ticked: it is the list of
        // "how many there are", which is what ticking the box writes out.
        if(isChecked())
            onStateChanged(true);
    }
    if(tag->retired){
        if(taglistwidget)
            taglistwidget->retire(tag);
        return TagHandling::Consumed;
    }
    return claimed?TagHandling::Linked:TagHandling::NotMine;
}

void teTagCheckBoxPlus::re_readOwned(std::shared_ptr<teTag>tag){
    if(!filter(tag))
        unlink(tag);
    if(tag->retired&&taglistwidget)
        taglistwidget->retire(tag);
}

void teTagCheckBoxPlus::re_readCounted(std::shared_ptr<teTag>tag){
    if(!filter2(tag))
        unlinkCounted(tag);
    if(tag->retired&&taglistwidget)
        taglistwidget->retire(tag);
}

void teTagCheckBoxPlus::clear(){
    setStyleSheetOnce(this,"color:white;");
    clearLinks();
    EditorControl::dropAll(counted_tags,this);
    reset();
}


teTagListControl::teTagListControl(colorsWidget *in_onEdit_widget, TagListView *parentlist, QWidget *parent, QString *styleSheet,QString title)
    : teRefTagListWidget(parentlist,parent),onEdit_widget(in_onEdit_widget){
    // link() needs the main tag list to reach the teTagList it mirrors; it is
    // known here already, so it can never be left unset.
    setTaglistwidget(parentlist);
    connect(onEdit_widget,&colorsWidget::stringSignal,this,&teTagListControl::reciveWidgetSignal,Qt::DirectConnection);
    connect(onEdit_widget,&colorsWidget::destroySignal,this,&teTagListControl::reciveDestroySignal,Qt::DirectConnection);
    connect(onEdit_widget,&colorsWidget::cancelSignal,this,&teTagListControl::reciveCancelSignal,Qt::DirectConnection);

    if(styleSheet!=nullptr)
        setStyleSheet(*styleSheet);
    QPushButton* addButton = new QPushButton("+");
    QPushButton* removeButton = new QPushButton("-");
    addButton->setStyleSheet(R"(QPushButton{color:#09f580;border:1px solid #09f580;}
QPushButton:hover{background-color:rgba(9,245,128,100);})");
    addButton->setFixedSize(15,15);
    removeButton->setStyleSheet(R"(QPushButton{color:#f50935;border:1px solid #f50935;}
QPushButton:hover{background-color:rgba(245,9,53,100);})");
    removeButton->setFixedSize(15,15);
    setStyleSheet(R"(QLabel[type="title"]{
font:italic 14px;color:rgb(200,200,200);
})");
    if(!title.isEmpty()){
        QLabel*titleLabel= new QLabel{title};
        titleLabel->setProperty("type","title");
        buttonLayout->addWidget(titleLabel);
    }
    buttonLayout->addStretch(0);
    buttonLayout->addWidget(addButton);
    buttonLayout->addWidget(removeButton);
    wlayout->insertLayout(0,buttonLayout);
    connect(addButton, &QPushButton::clicked, this, &teTagListControl::onAddButtonClicked,Qt::DirectConnection);
    connect(removeButton, &QPushButton::clicked, this,&teTagListControl::onSubButtonClicked,Qt::DirectConnection);
}

void teTagListControl::link(std::shared_ptr<teTag> in_tag){
    in_tag->teConnect(teCallbackType::destroy,this,&EditorControl::unlink,in_tag);
    linked_tags.insert(in_tag);
    // The main list is the one that owns the tag; the editor only mirrors it.
    if(taglistwidget)
        taginsert(-1,in_tag,taglistwidget->showing_list);
    else
        taginsert(-1,in_tag);
}

void teTagListControl::unlink(std::shared_ptr<teTag> in_tag){
    // One call for two sets: the control's claim and the tag widget this list
    // built for the tag both subscribe from the same teObject (the virtual base),
    // so disconnecting this object from the tag drops both.
    teDisconnect(in_tag.get());
    linked_tags.erase(in_tag);
    tagErase(in_tag);
}

void teTagListControl::onAddButtonClicked(){
    ifedit=false;
    m_insertBelow=false;
    // A new tag is not related to anything: no button may be lit.
    onEdit_widget->startNewTag();
    onEdit_widget->showNearCursor();
}

void teTagListControl::insertRequested(bool below){
    // 右键 insert above / below:和 "+" 同一个窗口,只是记住插到哪一侧。
    ifedit=false;
    m_insertBelow=below;
    onEdit_widget->startNewTag();
    onEdit_widget->showNearCursor();
}

void teTagListControl::reciveWidgetSignal(QString data, bool ifadd){
    if(ifedit){
        ifedit=false;
        if(select_current){
            taglistwidget->tagEdit(select_current->core,data);
        }
    }else{
        auto tag=std::make_shared<teTag>(data);
        const bool below=m_insertBelow;
        m_insertBelow=false;
        if(below)
            taglistwidget->tagInsertBelow(false,tag,true);
        else
            taglistwidget->tagInsertAbove(false,tag,true);
    }
}

void teTagListControl::reciveDestroySignal(){
    if(ifedit){
        ifedit=false;
        if(select_current){
            taglistwidget->tagErase(select_current->core);
        }
    }
}

void teTagListControl::tagEdit(teTagWidgetBase *tag, teWordWidgetBase *word){
    ifedit=true;
    m_insertBelow=false;
    onEdit_widget->input_and_show(tag->core);
}

// The three overrides below mirror the editor's selection into the main tag
// list. They address the tag by its core now: the editor only knows cores, and
// a core does not necessarily own a widget (which is what the old
// `in->core->widget` assumed - and it dereferenced it unchecked).
void teTagListControl::setSelectCurrent(teTagWidgetBase *in, bool ifclear){
    teRefTagListWidget::setSelectCurrent(in);
    if(!taglistwidget)
        return;
    taglistwidget->setSelectCurrentCore(in?in->core:nullptr,ifclear);
}

void teTagListControl::setSelect(teTagWidgetBase *in){
    teRefTagListWidget::setSelect(in);
    if(taglistwidget&&in)
        taglistwidget->setSelectCore(in->core);
}

int teTagListControl::setUnselect(teTagWidgetBase *in){
    teRefTagListWidget::setUnselect(in);
    if(taglistwidget&&in)
        taglistwidget->setUnselectCore(in->core);
    return 1;
}

bool teTagListControl::filter(std::shared_ptr<teTag> tag) const
{
    if(!tag||tag->words.isEmpty())
        return false;
    if(!vocabulary.isEmpty()){
        if(vocabulary.contains(*tag->words.back()))
            return true;
        if(vocabulary.contains(static_cast<QString>(*tag)))
            return true;
    }
    return accept?accept(*tag):false;
}

void teTagListControl::clear(){
    resetPolicy();                              // the subclass's own state first
    std::set<std::shared_ptr<teTag>>tmplinked_tags = linked_tags;
    for(std::shared_ptr<teTag>tag:tmplinked_tags){
        unlink(tag);
    }
    linked_tags.clear();
    reset();
}
