#include "teeditor.h"
#include "tetag.h"
#include "teeditorcontrol.h"
#include "testyle.h"
teEditor::teEditor(teTagListView*in_taglistwidget,const QString& name,QWidget *parent):QFrame(parent),name(name),taglistwidget(in_taglistwidget){
    setObjectName(name);
}

void teEditor::reset(){
    for(EditorControl*control:controls)
        control->reset();
}

void teEditor::registerControl(EditorControl* control){
    if(!control)
        return;
    // The control reads tags from this list and mirrors its selection into it;
    // setting it here means a row can never forget it.
    control->setTaglistwidget(taglistwidget);
    controls.push_back(control);
}

TagHandling teEditor::read(std::shared_ptr<teTag>tag){
    // Only Consumed stops the scan: several controls may claim the same tag (a
    // colour control and a length control, say), and a control that links it
    // without consuming it must not keep the others from seeing it.
    TagHandling result=TagHandling::NotMine;
    for(EditorControl*ctrl_ptr:controls){
        const TagHandling handled=ctrl_ptr->read(tag);
        if(handled==TagHandling::Consumed)
            return TagHandling::Consumed;
        if(handled==TagHandling::Linked)
            result=TagHandling::Linked;
    }
    return result;
}
TagHandling teEditor::re_read(std::shared_ptr<teTag>tag){
    if(tag->retired)
        return TagHandling::Consumed;
    if(taglistwidget)
        taglistwidget->ensureWidgetFor(tag);
    // Only controls that do *not* have the tag are offered it: the ones that do
    // re-check it through the tag's own edit callbacks (EditorControl::link
    // connects re_read for that), so calling re_read here as well would re-read
    // every linked tag twice.
    for(EditorControl*ctrl_ptr:controls){
        if(!ctrl_ptr->linked(tag)){
            if(ctrl_ptr->read(tag)==TagHandling::Consumed)
                return TagHandling::Consumed;
        }else if(tag->retired){
            return TagHandling::Consumed;
        }
    }
    return TagHandling::NotMine;
}
void teEditor::clear(){
    for(EditorControl*ctrl_ptr:controls){
        ctrl_ptr->clear();
    }
    if(taglist){
        disconnect(taglist,0,this,0);
        taglist->teDisconnect(this);
        taglist=nullptr;
    }
}

void teEditor::setTagListWidget(teTagListView *listwidget){
    taglistwidget = listwidget;
    for(EditorControl*ec:controls){
        ec->setTaglistwidget(listwidget);
    }
}

teEditor_standard::teEditor_standard(teTagListView*in_taglistwidget,const QString &name, const QString &accent, QWidget *parent):teEditor(in_taglistwidget,name,parent){
    editor_switch->setObjectName("editor_switch");
    // The accent is read by a property selector in the sheet, so it has to be set
    // before the sheet is applied (Qt resolves property selectors while polishing).
    if(!accent.isEmpty())
        editor_switch->setProperty("accent",accent);
    setStyleSheet(teStyle::sheet(QStringLiteral("editor")));
    editor_switch->setText(name);
    editor_switch->setCheckable(true);
    editor_switch->setChecked(true);
    titleLayout->addWidget(editor_switch);
    back_to_main_page->setText(QStringLiteral("↑"));
    back_to_main_page->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
    back_to_main_page->setMinimumSize(30,25);
    back_to_main_page->setStyleSheet(QStringLiteral(R"(
QPushButton{font-size:16pt;font-style:bold;padding:0px;margin:0px;}
QPushButton:!hover{border:2px solid #4c399e;}
QPushButton:hover{border:2px solid #ad5dff;})"));
    back_to_main_page->hide();
    connect(back_to_main_page,&QPushButton::clicked,this,&teEditor_standard::showMainPage,Qt::DirectConnection);
    titleLayout->addWidget(back_to_main_page);
    mainLayout->addLayout(titleLayout);
    mainLayout->addWidget(content);
    contentLayout->setContentsMargins(0,0,0,0);
    contentLayout->setSpacing(0);
    mainLayout->setSpacing(1);
    mainLayout->setContentsMargins(1,1,1,1);
    connect(editor_switch,&QPushButton::clicked,this,&teEditor_standard::onSwitchToggled,Qt::DirectConnection);
}

void teEditor_standard::addRowTo(QBoxLayout* layout, teEditorRow row){
    if(!layout)
        return;
    if(!row.title.isEmpty()){
        auto* title = new QLabel(row.title);
        title->setProperty("type","title");
        title->setStyleSheet(QStringLiteral("font:italic 14px;color:rgb(200,200,200);"));
        layout->addWidget(title);
    }
    auto* line = new QHBoxLayout;
    line->setSpacing(1);
    line->setContentsMargins(0,0,0,0);
    if(!row.lead.isEmpty()){
        line->addWidget(new QLabel(row.lead));
    }
    bool hasWidget = !row.lead.isEmpty();
    for(EditorControl* control : row.controls){
        if(!control)
            continue;
        registerControl(control);
        if(auto* widget = dynamic_cast<QWidget*>(control)){
            line->addWidget(widget);
            hasWidget = true;
        }
    }
    if(hasWidget)
        layout->addLayout(line);
    else
        delete line;                    // a control that is not a widget has no row
}

void teEditor_standard::addSubPage(QPushButton* trigger, QWidget* panel){
    if(!panel)
        return;
    subPages.push_back(panel);
    mainLayout->addWidget(panel);
    panel->hide();
    if(trigger)
        connect(trigger,&QPushButton::clicked,this,[this,panel]{showSubPage(panel);},Qt::DirectConnection);
}

void teEditor_standard::showSubPage(QWidget* panel){
    if(!panel||!subPages.contains(panel))
        return;
    fold();
    back_to_main_page->show();
    panel->show();
    openPage = panel;
}

void teEditor_standard::showMainPage(){
    if(openPage)
        openPage->hide();
    openPage = nullptr;
    back_to_main_page->hide();
    unfold();
}

void teEditor_standard::onSwitchToggled(bool checked){
    ifrun = checked;
    if(checked)
        unfold();
    else
        fold();
}

void teEditor_standard::fold(){
    content->hide();
    for(QWidget* page : subPages)
        page->hide();
    back_to_main_page->hide();
}

void teEditor_standard::unfold(){
    content->show();
}
