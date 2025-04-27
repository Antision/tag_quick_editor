#include "teeditor.h"
#include "tetag.h"
#include "teeditorcontrol.h"
teEditor::teEditor(teTagListWidget*in_taglistwidget,const QString& name,QWidget *parent):QFrame(parent),name(name),taglistwidget(in_taglistwidget){
    setObjectName(name);
}

void teEditor::reset(){
    for(teEditorControl*control:controls)
        control->reset();
}

bool teEditor::read(std::shared_ptr<tetagcore>tag){
    for(teEditorControl*ctrl_ptr:controls){
        if(ctrl_ptr->read(tag)){
            return true;
        }
    }
    return false;
}
bool teEditor::re_read(std::shared_ptr<tetagcore>tag){
    if(tag->type==teTagCore::deleteTag)
        return true;
    if(!tag->widget)tag->load();
    for(teEditorControl*ctrl_ptr:controls){
        if(!ctrl_ptr->linked(tag)){
            if(ctrl_ptr->read(tag)){
                return true;
            }
        }else if(tag->type==teTagCore::deleteTag){
            return true;
        }
    }
    return false;
}
void teEditor::clear(){
    for(teEditorControl*ctrl_ptr:controls){
        ctrl_ptr->clear();
    }
    if(taglist){
        disconnect(taglist,0,this,0);
        taglist->teDisconnect(this);
        taglist=nullptr;
    }
}

void teEditor::setTagListWidget(teTagListWidget *listwidget){
    taglistwidget = listwidget;
    for(teEditorControl*ec:controls){
        ec->setTaglistwidget(listwidget);
    }
}

teEditor_standard::teEditor_standard(teTagListWidget*in_taglistwidget,const QString &name, QString *styleSheet, QWidget *parent):teEditor(in_taglistwidget,name,parent){
    editor_switch->setObjectName("editor_switch");
    editor_switch->setText(name);
    editor_switch->setCheckable(true);
    editor_switch->setChecked(true);
    titleLayout->addWidget(editor_switch);
    back_to_main_page->setText(QStringLiteral("↑"));
    back_to_main_page->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
    back_to_main_page->setMinimumSize(30,25);
    back_to_main_page->setStyleSheet(QStringLiteral(R"(
QPushButton{font:bold 16pt "Segoe UI";padding:0px;margin:0px;}
QPushButton:!hover{border:2px solid #4c399e;}
QPushButton:hover{border:2px solid #ad5dff;})"));
    back_to_main_page->hide();
    connect(back_to_main_page,&QPushButton::clicked,this,&teEditor_standard::showInterface,Qt::DirectConnection);
    titleLayout->addWidget(back_to_main_page);
    mainLayout->addLayout(titleLayout);
    mainLayout->addLayout(contentLayout);
    mainLayout->setSpacing(1);
    mainLayout->setContentsMargins(1,1,1,1);
    if(styleSheet)
        setStyleSheet(*styleSheet);
    connect(editor_switch,&QPushButton::clicked,this,&teEditor_standard::onSwitchToggled,Qt::DirectConnection);
}
void setLayoutWidgetsVisible(QLayout* layout, bool visible) {
    if (!layout) return;

    for (int i = 0; i < layout->count(); ++i) {
        QLayoutItem* item = layout->itemAt(i);
        if (!item) continue;

        if (QWidget* widget = item->widget()) {
            if (visible) {
                widget->show();
            } else {
                widget->hide();
            }
        }

        if (QLayout* childLayout = item->layout()) {
            setLayoutWidgetsVisible(childLayout, visible);
        }
    }
}
void teEditor_standard::showInterface(int index){
    if(index){
        fold();
        back_to_main_page->show();
        extraInterfaces[index-1]->show();
        showing_interface=index;
    }else{
        unfold();
        back_to_main_page->hide();
        extraInterfaces[showing_interface-1]->hide();
        showing_interface=0;
    }
}

void teEditor_standard::onSwitchToggled(bool checked){
    if(checked){
        unfold();
        ifrun=true;
    }
    else if(!checked){
        fold();
        ifrun=false;
    }
}

void teEditor_standard::fold(){
    setLayoutWidgetsVisible(contentLayout,false);
    for(QWidget*w:extraInterfaces)
        w->hide();
    back_to_main_page->hide();
}

void teEditor_standard::unfold(){
    setLayoutWidgetsVisible(contentLayout,true);
}
