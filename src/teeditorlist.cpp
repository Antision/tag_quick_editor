#include "teeditorlist.h"
#include "teeditorcontrol.h"
#include "tetaglistwidget.h"
#include "pch.h"

void teEditorList::loadEditors(const QVector<teEditor*>& input_vector_ptr,editorListLayout*in_listpage){
    editorLayout = in_listpage;
    setContentsMargins(0,0,0,0);
    int pagecount = editorLayout->editors.size();
    if(pagecount==0)
        ++pagecount;
    layout->setContentsMargins(0,0,0,0);
    layout->setSpacing(0);
    setLayout(layout);

    for(int i =0;i<pagecount;++i){
        QScrollArea* newscrollarea = new QScrollArea;
        editorLayout->pages.push_back(newscrollarea);
        pageSplitter->addWidget(newscrollarea);
        QWidget* container = new QWidget;
        newscrollarea->setWidget(container);
        newscrollarea->setWidgetResizable(true);
        newscrollarea->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
        newscrollarea->setStyleSheet(editorListScrollAreaStyle);
        QVBoxLayout* containerLayout=new QVBoxLayout;
        container->setStyleSheet(editorliststyle);
        container->setLayout(containerLayout);
        containerLayout->setContentsMargins(0,0,0,0);
        containerLayout->setSpacing(0);
    }
    layout->addWidget(pageSplitter);

    QList<int> pageWidth;
    int totalWidth = this->width();
    for(int i =0;i<pagecount;++i){
        pageWidth.push_back(totalWidth/pagecount);
    }
    pageSplitter->setSizes(pageWidth);

    if(!input_vector_ptr.empty()){
        editorlist=std::move(input_vector_ptr);
        QVector<teEditor*> tmpeditorlist = editorlist;
        for(int i=0;i<editorLayout->editors.size();++i){
            int pageEditorCount = editorLayout->editors[i].size();
            for(int j =0;j<pageEditorCount;++j){
                for(auto it = tmpeditorlist.begin();it!=tmpeditorlist.end();++it)
                    if(editorLayout->editors[i][j]==((*it)->name)){
                        editorLayout->pages[i]->widget()->layout()->addWidget(*it);
                        tmpeditorlist.erase(it);
                        break;
                    }
            }
        }
        for(teEditor*e:tmpeditorlist){
            editorLayout->pages[0]->widget()->layout()->addWidget(e);
        }
        for(QScrollArea*page:editorLayout->pages){
            ((QVBoxLayout*)(page->widget()->layout()))->addItem(new QSpacerItem(0,0,QSizePolicy::Fixed,QSizePolicy::Expanding));
        }
    }
}

void teEditorList::setEditorsToPages(){
    int pagecount = editorLayout->pages.size();
    int newPageCount = editorLayout->editors.size();
    for(int i = 0;i<newPageCount;++i){
        if(editorLayout->editors[i].empty()){
            editorLayout->editors.removeAt(i);
            --i;
            --newPageCount;
        }
    }
    for(int i = 0;i<pagecount;++i){
        int layoutItemCount = editorLayout->pages[i]->widget()->layout()->count();
        for(int j =0;j<layoutItemCount;++j){
            QLayoutItem*item = editorLayout->pages[i]->widget()->layout()->takeAt(0);
            if(item->spacerItem())
                delete item->spacerItem();
            else if(item->widget()){
                item->widget()->setParent(nullptr);
            }
        }
    }
    for(int i = pagecount;i>newPageCount;--i){
        delete editorLayout->pages.back();
        editorLayout->pages.pop_back();
    }
    for(int i = pagecount;i<newPageCount;++i){
        QScrollArea*newscrollarea = new QScrollArea;
        QWidget* container = new QWidget;
        newscrollarea->setWidget(container);
        newscrollarea->setWidgetResizable(true);
        newscrollarea->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
        newscrollarea->setStyleSheet(editorListScrollAreaStyle);

        QVBoxLayout* containerLayout=new QVBoxLayout;
        container->setStyleSheet(editorliststyle);
        container->setLayout(containerLayout);
        containerLayout->setContentsMargins(0,0,0,0);
        containerLayout->setSpacing(0);

        editorLayout->pages.push_back(newscrollarea);
    }
    if(pagecount!=newPageCount){
        QList<int>tmpsizes;
        for(int i =0;i<editorLayout->pages.size();++i){
            pageSplitter->addWidget(editorLayout->pages[i]);
            tmpsizes.push_back(1);
        }
        pageSplitter->setSizes(tmpsizes);
    }
    QVector<teEditor*> tmpeditorlist = editorlist;
    for(int i=0;i<editorLayout->editors.size();++i){
        int pageEditorCount = editorLayout->editors[i].size();
        for(int j =0;j<pageEditorCount;++j){
            for(auto it = tmpeditorlist.begin();it!=tmpeditorlist.end();++it)
                if(editorLayout->editors[i][j]==(*it)->name){
                    editorLayout->pages[i]->widget()->layout()->addWidget(*it);
                    tmpeditorlist.erase(it);
                    break;
                }
        }
    }
    for(teEditor*e:tmpeditorlist){
        editorLayout->pages[0]->widget()->layout()->addWidget(e);
    }
    for(QScrollArea*page:editorLayout->pages){
        ((QVBoxLayout*)(page->widget()->layout()))->addItem(new QSpacerItem(0,0,QSizePolicy::Fixed,QSizePolicy::Expanding));
    }
}

void teEditorList::connectTaglistWidget(teTagListWidget *in_listWidget){
    if(tagListWidget!=nullptr) disconnect(tagListWidget,0,this,0);
    if(!in_listWidget){telog("input taglist is nullptr");return;}
    tagListWidget=in_listWidget;
    for(teEditor*e:editorlist){
        e->setTagListWidget(in_listWidget);
    }
    connect(in_listWidget,&teTagListWidget::newlistloaded,this,&teEditorList::readList,Qt::DirectConnection);
    connect(in_listWidget,&teTagListWidget::showinglistDestroyed,this,&teEditorList::unloadList,Qt::DirectConnection);
}

void teEditorList::onNewTaglistLoaded(){
    for(teEditor*e:editorlist)
        e->clear();
    if(tagListWidget->showing_list){
        disconnect(tagListWidget->showing_list,0,this,0);
    }
}

void teEditorList::readList(teTagList *input_taglist){
    unloadList();
    if(input_taglist)
        connectedList = input_taglist;
    connect(connectedList,&teTagList::tagInserted,this,&teEditorList::onNewTagInserted,Qt::DirectConnection);
    connect(connectedList,&teTagList::tagEdited,this,&teEditorList::onTagEdited,Qt::DirectConnection);
    int tagsize=connectedList->size();
    QVector<std::shared_ptr<tetagcore>>temtaglist = connectedList->getTags();
    for(int i=0;i<tagsize;++i){
        for(teEditor*e:editorlist){
            std::shared_ptr<tetagcore> co=temtaglist[i];
            if(e->read(co)){
                co->type=teTagCore::deleteTag;
                break;
            }
        }
    }
    temtaglist = connectedList->getTags();
    for(std::shared_ptr<tetagcore> co:temtaglist){
        if(co->type==teTagCore::deleteTag)
            tagListWidget->tagErase(co);
    }
}

void teEditorList::unloadList(){
    for(teEditor*e:editorlist){
        e->clear();
    }
    if(connectedList){
        disconnect(connectedList,0,this,0);
        connectedList=nullptr;
    }
}

void teEditorList::onNewTagInserted(std::shared_ptr<tetagcore> in_tag){
    for(teEditor*e:editorlist)
        if(e->read(in_tag)){
            return;
        }
}

void teEditorList::onTagEdited(std::shared_ptr<tetagcore> in_tag){
    for(teEditor*e:editorlist){
        if(e->re_read(in_tag)){
            return;
        }
    }
}
const QString editorListScrollAreaStyle = qsl(R"(QScrollArea{border:1px solid #4c399e;}
QScrollArea:hover{
    border-color: #6045d6;
}
QScrollArea:!hover{
    border-color: #4c399e;
}
QScrollBar{background:#5421ff;}
QScrollBar::vertical{margin: 12px 0px 12px 0px;border:0px solid #5421ff;width:10px;}
QScrollBar::horizontal{margin: 0px 12px 0px 12px;border:0px solid #5421ff;height:10px;}
QScrollBar::handle:!pressed{background:#7751f3;border:1px solid #c5a9f7;}
QScrollBar::handle:pressed{background:#c5a9f7;}

QScrollBar::handle:vertical{min-height:20px;}
QScrollBar::handle:vertical{min-width:20px;}
QScrollBar::up-arrow:vertical{
    border-image: url(":/res/scrollbar_uparrow.png");
    width:10px;height:10px;
}
QScrollBar::down-arrow:vertical{
    border-image: url(":/res/scrollbar_downarrow.png");
    width:10px;height:10px;
}

QScrollBar::left-arrow:horizontal {
    border-image: url(":/res/scrollbar_leftarrow.png");
    width:10px;height:10px;
}
QScrollBar::right-arrow:horizontal {
    border-image: url(":/res/scrollbar_rightarrow.png");
    width:10px;height:10px;
}
QScrollBar::sub-page,QScrollBar::add-page {
    background-color: #6d6292;
}
QScrollBar::sub-line:vertical {
    subcontrol-position: top;
    subcontrol-origin: margin;
    background-color: #6644d6;
    border: 0px solid #e7dfff;
    height:12px;
    width:10px;
}
QScrollBar::add-line:vertical {
    subcontrol-position: bottom;
    subcontrol-origin: margin;
    background-color: #6644d6;
    border: 0px solid #e7dfff;
    height: 12px;
    width:10px;
}

QScrollBar::sub-line:horizontal {
    subcontrol-position: left;
    subcontrol-origin: margin;
    background-color: #6644d6;
    border: 0px solid #e7dfff;
    height:10px;
    width:12px;
}
QScrollBar::add-line:horizontal {
    subcontrol-position: right;
    subcontrol-origin: margin;
    background-color: #6644d6;
    border: 0px solid #e7dfff;
    height: 10px;
    width:12px;
})");
const QString editorliststyle = qsl(R"(
QWidget{
background-color: rgb(27, 27, 27);
}
*{
    font: 12pt "Segoe UI";
    color:white;
}
QScrollArea{border:1px solid #4c399e;}
QScrollArea:hover{border:1px solid #ad5dff;}
QScrollBar{background:rgba(0,0,0,210);}
QScrollBar::vertical{margin: 0px 0px 0px 0px;border:0px solid #5421ff;width:5px;}
QScrollBar::horizontal{margin: 0px 0px 0px 0px;border:0px solid #5421ff;height:5px;}
QScrollBar::handle{
color:#00ff00
}
QScrollBar::handle:!pressed{background:rgba(255,255,255,60);}
QScrollBar::handle:pressed{background:rgba(255,255,255,150);}

QScrollBar::handle:vertical{min-height:20px; border-radius: 2px;}
QScrollBar::handle:horizontal{min-width:20px;border-radius: 2px;}

QScrollBar::sub-page,QScrollBar::add-page {
    background-color: None;
}
QScrollBar::add-line,QScrollBar::sub-line{
border:0px solid black;
width:0px;
height:0px;
}
QScrollBar::sub-page,QScrollBar::add-page {
    background-color: #05191f;
}
QPushButton{
    outline: 0px solid black;
    border: 1px solid #4c399e;
    background-color:transparent;
    padding: 0px;
}
QPushButton:hover{
    border: 1px solid #a858f2;
    background-color:rgba(177,100,255,60);
}
QPushButton:checked{
    border: 1px solid #12b594;
    background-color:#00592c;
}
QPushButton:checked:hover{
    border: 1px solid #12f3e0;
    background-color:#00a452;
}
QCheckBox{
    outline: 0px solid black;
    border: 1px solid #4c399e;
}
QCheckBox:hover{
    border: 1px solid #a858f2;
    background-color:rgba(177,100,255,60);
}
QLineEdit{
    outline: 0px solid black;
    border: 1px solid #4c399e;
}
QComboBox{
    border: 1px solid #4c399e;
}
QComboBox:hover{
    border: 1px solid #a858f2;
    background-color:rgba(177,100,255,60);
}
QAbstractItemView{
    font:10pt "Segoe UI";
    outline: 0px solid black;
    border: 1px solid #4c399e;
    selection-background-color: #3aaf74;
}
QAbstractItemView::item{
    height: 25px;
    border:1px solid #57487a;
    background-color: #171717;
}
QAbstractItemView::item:hover{
    border:1px solid #4a25a4;
    background-color: #4c4a5d;
}

)");

