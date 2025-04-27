#include "teeditorcontrol.h"
#include "tetag.h"
#include "tetaglistwidget.h"

bool teEditorControl::re_read(std::shared_ptr<tetagcore>tag){
    if(!filter(tag)){
        if(tag->type==teTagCore::deleteTag){
            taglistwidget->tagErase(tag);
            return true;
        }
        unlink(tag);
    }
}

bool teEditorControl::read(std::shared_ptr<tetagcore>tag){
    bool iffilt = filter(tag);
    if(tag->type==teTagCore::deleteTag){
        taglistwidget->tagErase(tag);
        return true;
    }
    if(iffilt){
        if(tag->widget==nullptr)tag->load();
        link(tag);
        if(tag->type==teTagCore::deleteTag){
            taglistwidget->tagErase(tag);
            return true;
        }
    }
    return false;
}

void teEditorControl::link(std::shared_ptr<tetagcore>in_tag){
    in_tag->teConnect(teCallbackType::destroy,this,&teEditorControl::unlink,in_tag);
    in_tag->teConnect(teCallbackType::edit,qsl("teEditorControl::link"),this,&teEditorControl::re_read,in_tag);
    in_tag->teConnect(teCallbackType::edit_with_layout,this,&teEditorControl::re_read,in_tag);
    linked_tags.insert(in_tag);
    refreshState();
}

void teEditorControl::unlink(std::shared_ptr<tetagcore>in_tag){
    in_tag->teDisconnect(this);
    linked_tags.erase(in_tag);
    if(ifrefreshState)
        refreshState();
}

bool teEditorControl::linked(std::shared_ptr<tetagcore>tag){
    return linked_tags.find(tag)!=linked_tags.end();
}

void teEditorControl::setTaglistwidget(teTagListWidget *in_taglistwidget){
    taglistwidget=in_taglistwidget;
}

void teEditorControl::edited(){
    for(auto&[obj,func]:linked_callback_call)
        if(func->type==teCallbackType::edit)
            (*func)();
}
teTagComboBox::teTagComboBox(QList<QPair<QString,QStringList>>&& string_datas,QString&&in_default_text, QWidget *parent, QString *styleSheet)
    :QComboBox(parent),default_text(std::move(in_default_text)){
    addItem(default_text,QStringList{});
    for(QPair<QString,QStringList>& pair:string_datas){
        addItem(std::move(pair.first),pair.second);
        QStringList& tagstrlist = pair.second;
        int tagstrlist_size = tagstrlist.size();
        for(int i =0;i<tagstrlist_size;++i)
            captureList.insert(tagstrlist.takeAt(0));
    }
    if(styleSheet)
        setStyleSheet(*styleSheet);
    connect(this,&teTagComboBox::currentIndexChanged,this,&teTagComboBox::onIndexChanged,Qt::DirectConnection);
}

bool teTagComboBox::filter(std::shared_ptr<tetagcore>tag){
    if(captureList.find((QString)(*tag))!=captureList.end())
        return true;
    else
        return false;
}

void teTagComboBox::clear(){
    setStyleSheet("color:white;");
    for(std::shared_ptr<tetagcore>tag:linked_tags){
        tag->teDisconnect(this);
    }
    --excute;
    setItemText(0,default_text);
    setItemData(0,QStringList{});
    reset();
    linked_tags.clear();
    ++excute;
}

void teTagComboBox::reset(){
    --excute;
    setCurrentIndex(0);
    ++excute;
}

void teTagComboBox::onIndexChanged(int index){
    if(!taglistwidget->showing_list)return;
    if(excute<1) return;
    QStringList stringlist = itemData(index,Qt::UserRole).toStringList();

    auto linked_tags_tmp = linked_tags;
    auto it =linked_tags_tmp.begin();
    for (;it!=linked_tags_tmp.end();++it) {
        std::shared_ptr<tetagcore> tag = *it;
        QString tagString = static_cast<QString>(*tag);
        if (!stringlist.contains(tagString)) {
            taglistwidget->tagErase(tag);
        }
    }
    for (const QString& tagstring : stringlist) {
        auto found = std::find_if(linked_tags.begin(), linked_tags.end(),
                                  [&tagstring](std::shared_ptr<tetagcore> tag) {
                                      return static_cast<QString>(*tag) == tagstring;
                                  });
        if (found == linked_tags.end()) {
            taglistwidget->tagInsertAbove(false, std::make_shared<tetagcore>(tagstring));
        }
    }
    edited();
}

void teTagComboBox::refreshState(){
    QStringList tagstrings;
    for(std::shared_ptr<tetagcore>tag:linked_tags){
        tagstrings.push_back(*tag);
    }
    QString itemtext="";
    if(!tagstrings.empty())
    {
        for(QString& str:tagstrings){
            itemtext+=str+',';
        }
        itemtext.chop(1);
        setItemText(0,std::move(QString(itemtext)));
        setItemData(0,std::move(tagstrings));
        --excute;
        setCurrentIndex(0);
        ++excute;
        setStyleSheet("color:#07f680;");
    }else{
        setStyleSheet("color:white;");
        setItemText(0,default_text);
        setItemData(0,QStringList{});
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
            QLabel* lb = (QLabel*)widget;
            final_string.append(data);
            if(space)
                final_string.append(QStringLiteral(" "));
        }
    }
    if(atLeastOneButtonTriggered){
        final_string.chop(1);
        if(linked_tags.empty()){
            std::shared_ptr<tetagcore>newtagcore = std::make_shared<tetagcore>(final_string);
            newtagcore->load();
            link(newtagcore);
            taglistwidget->tagInsertAbove(false, newtagcore);
        }else{
            while(linked_tags.size()>1){
                taglistwidget->tagErase(*linked_tags.begin());
            }
            (*linked_tags.begin())->widget->setText(final_string);
        }
    }else{
        while(!linked_tags.empty())
            taglistwidget->tagErase(*linked_tags.begin());
        return;
    }
    ifrefreshState=true;
    refreshState();
    edited();
}
void teTagButtonGroup::clear(){
    setStyleSheet("color:white;");
    for(std::shared_ptr<tetagcore>tag:linked_tags){
        tag->teDisconnect(this);
    }
    linked_tags.clear();
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
bool teTagButtonGroup::filter(std::shared_ptr<tetagcore>tag){
    if(defaultFiltStrings.find(*tag)!=defaultFiltStrings.end()){
        return true;
    }else return false;
}

void teTagButtonGroup::refreshState(){
    for(auto&[widget,data,ifbutton,ifspace,ifvariablePos]:allwidgets){
        if(!ifbutton)goto nextbutton;
        for(std::shared_ptr<tetagcore>tag:linked_tags){
            if(ifspace){
                for(tewordcore*word:*tag){
                    if(*word==data){
                        ((QPushButton*)widget)->setChecked(true);
                        goto nextbutton;
                    }
                }
            }else{
                for(tewordcore*word:*tag){
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
        std::shared_ptr<tetagcore>newtagcore = std::make_shared<tetagcore>(tagstr);
        newtagcore->load();
        link(newtagcore);
        taglistwidget->tagInsertAbove(false, newtagcore);
    }else
        taglistwidget->tagEdit(*linked_tags.begin(),tagstr);
    refreshState();
    edited();
}

void teTagLineedit::clear(){
    setStyleSheet("color:white;");
    for(std::shared_ptr<tetagcore>tag:linked_tags){
        tag->teDisconnect(this);
    }
    reset();
    linked_tags.clear();
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
    setStyleSheet("color:white;");
    for(std::shared_ptr<tetagcore>tag:linked_tags){
        tag->teDisconnect(this);
    }
    linked_tags.clear();
    reset();
}

void teTagCheckBox::addString(QList<QString> in_strings){
    strings.append(in_strings);
}

void teTagCheckBox::clearString(){
    strings.clear();
}


void teTagCheckBox::select(){
    --excute;
    setChecked(true);
    ++excute;
}

void teTagCheckBox::unselect(){
    --excute;
    setChecked(false);
    ++excute;
}

bool teTagCheckBox::filter(std::shared_ptr<tetagcore>tag){
    if(strings.indexOf(QString(*tag))>-1){
        return true;
    }return false;
}

void teTagCheckBox::onStateChanged(bool state){
    if(!taglistwidget->showing_list)return;
    if(excute>0){
        if(!state){
            while(linked_tags.size()>0){
                taglistwidget->tagErase((*linked_tags.begin()));
            }
        }
        else{
            for(QString&string:strings){
                taglistwidget->tagInsertAbove(false,std::make_shared<tetagcore>(string));
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

void teTagCheckBoxPlus::link2(std::shared_ptr<tetagcore>in_tag){
    in_tag->teConnect(teCallbackType::destroy,this,&teTagCheckBoxPlus::unlink2,in_tag);
    in_tag->teConnect(teCallbackType::edit,qsl("teTagCheckBoxPlus::link2"),this,&teTagCheckBoxPlus::re_read,in_tag,2);
    in_tag->teConnect(teCallbackType::edit_with_layout,this,&teTagCheckBoxPlus::re_read,in_tag,2);
    second_tags.insert(in_tag);
}

void teTagCheckBoxPlus::unlink2(std::shared_ptr<tetagcore>tag){
    tag->teDisconnect(this);
    second_tags.erase(tag);
    refreshState();
}

bool teTagCheckBoxPlus::read(std::shared_ptr<tetagcore>tag){
    if(filter(tag)){
        if(tag->widget==nullptr)tag->load();
        link(tag);
    }else if(filter2(tag)){
        if(tag->widget==nullptr)tag->load();
        link2(tag);
        if(isChecked()){
            onStateChanged(true);
        }
    }
    if(tag->type==teTagCore::deleteTag){
        taglistwidget->tagErase(tag);
        return true;
    }
    return false;
}

bool teTagCheckBoxPlus::re_read(std::shared_ptr<tetagcore>tag, int taggroup){
    if(taggroup==1&&!filter(tag)){
        unlink(tag);
    }
    else if(taggroup==2&&!filter2(tag)){
        unlink2(tag);
    }
    if(tag->type==teTagCore::deleteTag){
        taglistwidget->tagErase(tag);
        return true;
    }
}

void teTagCheckBoxPlus::clear(){
    setStyleSheet("color:white;");
    for(std::shared_ptr<tetagcore>tag:linked_tags){
        tag->teDisconnect(this);
    }
    linked_tags.clear();
    for(std::shared_ptr<tetagcore>tag:second_tags){
        tag->teDisconnect(this);
    }
    second_tags.clear();
    reset();
}


teTagListControl::teTagListControl(colorsWidget *in_onEdit_widget, teTagListWidget *parentlist, QWidget *parent, QString *styleSheet)
    : teRefTagListWidget(parentlist,parent),onEdit_widget(in_onEdit_widget){
    connect(onEdit_widget,&teSignalWidget::stringSignal,this,&teTagListControl::reciveWidgetSignal,Qt::DirectConnection);
    connect(onEdit_widget,&teSignalWidget::destroySignal,this,&teTagListControl::reciveDestroySignal,Qt::DirectConnection);
    connect(onEdit_widget,&teSignalWidget::cancelSignal,this,&teTagListControl::reciveCancelSignal,Qt::DirectConnection);

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
    buttonLayout->addStretch(0);
    buttonLayout->addWidget(addButton);
    buttonLayout->addWidget(removeButton);
    wlayout->insertLayout(0,buttonLayout);
    connect(addButton, &QPushButton::clicked, this, &teTagListControl::onAddButtonClicked,Qt::DirectConnection);
    connect(removeButton, &QPushButton::clicked, this,&teTagListControl::onSubButtonClicked,Qt::DirectConnection);
}

void teTagListControl::link(std::shared_ptr<tetagcore> in_tag){
    in_tag->teConnect(teCallbackType::destroy,this,&teEditorControl::unlink,in_tag);
    linked_tags.insert(in_tag);
    taginsert(-1,in_tag,taglistwidget->showing_list);
}

void teTagListControl::unlink(std::shared_ptr<tetagcore> in_tag){
    teDisconnect(in_tag.get());
    linked_tags.erase(in_tag);
    tagErase(in_tag);
}

void teTagListControl::onAddButtonClicked(){
    ifedit=false;
    onEdit_widget->show();
    onEdit_widget->move(QCursor::pos().x()-onEdit_widget->width(),QCursor::pos().y()-onEdit_widget->height()/2);
}

void teTagListControl::reciveWidgetSignal(QString data, bool ifadd){
    if(ifedit){
        ifedit=false;
        if(select_current){
            taglistwidget->tagEdit(select_current->core,data);
        }
    }else{
        taglistwidget->tagInsertAbove(false,std::make_shared<tetagcore>(data),true);
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

void teTagListControl::tagEdit(teTagBase *tag, teWordBase *word){
    ifedit=true;
    onEdit_widget->input_and_show(tag->core);
    onEdit_widget->move(QCursor::pos().x()-onEdit_widget->width(),QCursor::pos().y()-onEdit_widget->height()/2);
}

void teTagListControl::setSelectCurrent(teTagBase *in, bool ifclear){
    teRefTagListWidget::setSelectCurrent(in);
    teTag* coreWidget = in->core->widget;
    taglistwidget->setSelectCurrent(coreWidget);
    QTimer::singleShot(0,[this]{
        taglistwidget->sc->horizontalScrollBar()->setValue(0);
    });
    taglistwidget->sc->ensureWidgetVisible(coreWidget);
}

void teTagListControl::setSelect(teTagBase *in){
    teRefTagListWidget::setSelect(in);
    teTag* coreWidget = in->core->widget;
    taglistwidget->setSelect(coreWidget);
    QTimer::singleShot(0,[this]{
        taglistwidget->sc->horizontalScrollBar()->setValue(0);
    });
    taglistwidget->sc->ensureWidgetVisible(coreWidget);
}

int teTagListControl::setUnselect(teTagBase *in){
    teRefTagListWidget::setUnselect(in);
    if(in){
        teTag* coreWidget = in->core->widget;
        if(coreWidget)
            taglistwidget->setUnselect(coreWidget);
    }
    return 1;
}

void teTagListControl::clear(){
    std::set<std::shared_ptr<tetagcore>>tmplinked_tags = linked_tags;
    for(std::shared_ptr<tetagcore>tag:tmplinked_tags){
        unlink(tag);
    }
    linked_tags.clear();
    reset();
}
