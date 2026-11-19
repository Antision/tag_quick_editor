#include "teeditorlist.h"
#include "teeditorcontrol.h"
#include "tetaglistwidget.h"
#include "testyle.h"
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
        container->setStyleSheet(teStyle::sheet(QStringLiteral("controls")));
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
        container->setStyleSheet(teStyle::sheet(QStringLiteral("controls")));
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

void teEditorList::connectTaglistWidget(teTagListView *in_listWidget){
    if(tagListWidget){
        tagListWidget->setEditorHost(nullptr);
        disconnect(tagListWidget,0,this,0);
    }
    if(!in_listWidget){telog("input taglist is nullptr");return;}
    tagListWidget=in_listWidget;
    for(teEditor*e:editorlist){
        e->setTagListWidget(in_listWidget);
    }
    // The view tells its host directly (TV-03). It used to be a pair of signals;
    // the difference only showed in who owned the connection - and the view must
    // not know this class once it moves into the shared library.
    in_listWidget->setEditorHost(this);
}

void teEditorList::readAll(teTagList *list){
    readList(list);
}

void teEditorList::clearAll(){
    unloadList();
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
    if(!connectedList)
        return;                     // nothing to read (a picture without a caption)
    // Reading a list is not editing it. The controls normalise what they read
    // (two mergeable clothes tags become one), and those merges must neither land
    // on the undo stack nor count as "the user changed something" - they were
    // undo steps before, so the first Ctrl+Z after opening a picture undid a merge
    // nobody had asked for.
    teTagList::ChangeSuppressor suppressor(connectedList);
    // Tags absorbed below are erased when this goes out of scope, not while the
    // editors are still reading the list.
    teTagList::RetireDeferrer retireDeferrer(connectedList);
    connect(connectedList,&teTagList::tagInserted,this,&teEditorList::onNewTagInserted,Qt::DirectConnection);
    connect(connectedList,&teTagList::tagEdited,this,&teEditorList::onTagEdited,Qt::DirectConnection);
    int tagsize=connectedList->size();
    QVector<std::shared_ptr<teTag>>temtaglist = connectedList->getTags();
    for(int i=0;i<tagsize;++i){
        std::shared_ptr<teTag> co=temtaglist[i];
        if(co->type!=teTag::tag)
            continue;
        for(teEditor*e:editorlist){
            // TagHandling::Consumed means "a control absorbed this tag" (it was
            // merged into another one). Removing it is the list's job, and this is
            // the only place that decides it - it used to be a deleteTag marker in
            // the tag's own type field that four layers each had to re-check.
            if(e->read(co)==TagHandling::Consumed&&connectedList){
                connectedList->retire(co);
                break;
            }
        }
    }
    // A control may also have marked a tag while reading without reporting it
    // (a merge marks the tag it absorbed from inside filter()), so hand every
    // marked tag over as well. Both paths end in teTagList::retire().
    for(const std::shared_ptr<teTag>& co : connectedList->getTags()){
        if(co->retired)
            connectedList->retire(co);
    }
    // Whatever the controls changed while reading is the state the first edit of
    // this picture starts from.
    if(connectedList)
        connectedList->syncUndoBaseline();
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

void teEditorList::onNewTagInserted(std::shared_ptr<teTag> in_tag){
    if(in_tag->type!=teTag::tag)
        return;
    for(teEditor*e:editorlist)
        if(e->read(in_tag)==TagHandling::Consumed){
            return;
        }
}

void teEditorList::onTagEdited(std::shared_ptr<teTag> in_tag){
    for(teEditor*e:editorlist){
        if(e->re_read(in_tag)==TagHandling::Consumed){
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


