#include "teeditorlist.h"
#include "tagmarks.h"
#include "teeditor.h"

#include <QApplication>
#include <QDrag>
#include <QDropEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QTimer>

#include <algorithm>
#include "teeditorcontrol.h"
#include "tetaglistwidget.h"
#include "teeditorhelpers.h"
#include "testyle.h"
#include "pch.h"

teEditorList::~teEditorList()
{
    // The view keeps a raw TagEditorHost pointer and calls clearAll() from its
    // own destructor. When the window destroys the editor list before the tag
    // list (child order is not something we control) that call reached freed
    // memory - the crash on shutdown. Let go of the view first.
    if(tagListWidget)
        tagListWidget->setEditorHost(nullptr);
    // The title buttons and the page containers carry our event filter. Remove
    // it before the widgets go away: a destruction-time event used to land in
    // eventFilter() with a page list that was already being torn down.
    for(teEditor* editor:editorlist){
        if(auto* standard=dynamic_cast<teEditor_standard*>(editor))
            standard->editor_switch->removeEventFilter(this);
    }
    if(editorLayout){
        for(QScrollArea* page:editorLayout->pages){
            if(!page)
                continue;
            page->removeEventFilter(this);
            if(page->viewport())
                page->viewport()->removeEventFilter(this);
            if(page->widget())
                page->widget()->removeEventFilter(this);
        }
        // The page list lives in the layout struct, which outlives this widget.
        // Left alone it would hand a dangling QScrollArea* to whoever builds the
        // next window (the regression check builds several).
        editorLayout->pages.clear();
    }
    for(teEditor* editor:editorlist)
        delete editor;
    editorlist.clear();
}

void teEditorList::loadEditors(const QVector<teEditor*>& input_vector_ptr,editorListLayout*in_listpage){
    editorLayout = in_listpage;
    // The pages belong to *this* editor list (they are children of pageSplitter).
    // Anything still in the struct is left over from a previous window and its
    // widgets are gone; appending to it would hand a dangling pointer to every
    // reader of the page list.
    editorLayout->pages.clear();
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
        container->setStyleSheet(teStyle::controls());
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
    installEditorDrag();
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
        container->setStyleSheet(teStyle::controls());
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
    installEditorDrag();
}

void teEditorList::connectTaglistWidget(TagListView *in_listWidget){
    if(tagListWidget){
        tagListWidget->setEditorHost(nullptr);
        disconnect(tagListWidget,0,this,0);
    }
    if(!in_listWidget){
        telog("input taglist is nullptr");
        tagListWidget=nullptr;      // do not keep a pointer we just detached from
        return;
    }
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

bool teEditorList::mergeEnabled() const
{
    return MergeSwitch;
}

void teEditorList::refreshDecorations()
{
    for(teEditor* editor:editorlist){
        if(!editor)
            continue;
        for(EditorControl* control:editor->controls){
            if(!control)
                continue;
            for(const std::shared_ptr<teTag>& tag:control->linked_tags){
                refreshTagDecoration(tag);
                if(auto* widget=control->tagWidgetFor(tag))
                    widget->refreshStyle();
            }
        }
    }
}

void teEditorList::setMergeEnabled(bool enabled)
{
    MergeSwitch=enabled;
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
    // The text changed, and the decoration is a function of it. Doing it here
    // covers every control, including the ones that handle their own edits and
    // never go through EditorControl::re_read().
    refreshTagDecoration(in_tag);
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

namespace {
constexpr auto kEditorDragMime = "application/x-tq-editor-name";
}

void teEditorList::installEditorDrag()
{
    for(teEditor* editor:editorlist){
        auto* standard=dynamic_cast<teEditor_standard*>(editor);
        if(!standard)
            continue;
        if(standard->editor_switch->property("tqEditorDrag").toBool())
            continue;
        standard->editor_switch->setProperty("tqEditorDrag",true);
        standard->editor_switch->installEventFilter(this);
        standard->editor_switch->setToolTip(QStringLiteral("Drag to another column"));
    }
    for(QScrollArea* page:editorLayout->pages){
        page->setAcceptDrops(true);
        page->viewport()->setAcceptDrops(true);
        page->widget()->setAcceptDrops(true);
        for(QWidget* widget:{static_cast<QWidget*>(page),page->viewport(),page->widget()}){
            if(widget->property("tqEditorDrop").toBool())
                continue;
            widget->setProperty("tqEditorDrop",true);
            widget->installEventFilter(this);
        }
    }
}

bool teEditorList::eventFilter(QObject* watched,QEvent* event)
{
    // Source: the title button of a page.
    for(teEditor* editor:editorlist){
        auto* standard=dynamic_cast<teEditor_standard*>(editor);
        if(!standard||standard->editor_switch!=watched)
            continue;
        if(event->type()==QEvent::MouseButtonPress){
            auto* mouse=static_cast<QMouseEvent*>(event);
            if(mouse->button()==Qt::LeftButton){
                m_editorDragStart=mouse->globalPosition().toPoint();
                m_editorDragName=editor->name;
            }
        }else if(event->type()==QEvent::MouseMove&&!m_editorDragName.isEmpty()){
            auto* mouse=static_cast<QMouseEvent*>(event);
            if(!(mouse->buttons()&Qt::LeftButton))
                return false;
            if((mouse->globalPosition().toPoint()-m_editorDragStart).manhattanLength()
                <QApplication::startDragDistance())
                return false;
            auto* drag=new QDrag(this);
            drag->setPixmap(editor->grab());
            auto* mime=new QMimeData;
            mime->setData(kEditorDragMime,m_editorDragName.toUtf8());
            drag->setMimeData(mime);
            drag->exec(Qt::MoveAction);
            m_editorDragName.clear();
            return true;
        }
        return false;
    }

    // Target: a page column (scroll area, its viewport or its container).
    int pageIndex=-1;
    for(int i=0;i<editorLayout->pages.size();++i){
        QScrollArea* page=editorLayout->pages[i];
        if(watched==page||watched==page->viewport()||watched==page->widget()){
            pageIndex=i;
            break;
        }
    }
    if(pageIndex<0)
        return false;

    const auto hasEditor=[&](const QMimeData* mime){
        return mime&&mime->hasFormat(kEditorDragMime);
    };
    if(event->type()==QEvent::DragEnter||event->type()==QEvent::DragMove){
        auto* drop=static_cast<QDropEvent*>(event);
        if(hasEditor(drop->mimeData())){
            drop->setDropAction(Qt::MoveAction);
            drop->accept();
            return true;
        }
        return false;
    }
    if(event->type()==QEvent::Drop){
        auto* drop=static_cast<QDropEvent*>(event);
        if(!hasEditor(drop->mimeData()))
            return false;
        const QString name=QString::fromUtf8(drop->mimeData()->data(kEditorDragMime));
        if(name.isEmpty())
            return false;
        QWidget* container=editorLayout->pages[pageIndex]->widget();
        // The drop event may be delivered to the viewport or to the scroll area,
        // neither of which shares the container's origin. Using the raw event
        // position compared that against widget geometry in another coordinate
        // system, which shifted the row by one: A dropped on B's lower half used
        // to land after C. Map from the receiving widget into the container.
        QPoint pos=drop->position().toPoint();
        if(auto* receiver=qobject_cast<QWidget*>(watched))
            pos=container->mapFrom(receiver,pos);
        QVector<QPair<QString,QRect>> anchors;
        if(QLayout* layout=container->layout()){
            for(int i=0;i<layout->count();++i){
                auto* editorWidget=dynamic_cast<teEditor*>(layout->itemAt(i)->widget());
                if(editorWidget)
                    anchors.append({editorWidget->name,editorWidget->geometry()});
            }
        }
        // See editorDropRow(): the dragged editor is not an anchor, so the row
        // is already the index to insert at after it has been removed.
        int row=editorDropRow(anchors,name,pos.y());
        for(QStringList& page:editorLayout->editors)
            page.removeAll(name);
        if(pageIndex>=editorLayout->editors.size())
            return false;
        row=std::max(0,std::min(row,int(editorLayout->editors[pageIndex].size())));
        editorLayout->editors[pageIndex].insert(row,name);
        drop->setDropAction(Qt::MoveAction);
        drop->accept();
        // Rebuild after the drop event returns: setEditorsToPages() reparents and
        // may delete empty pages.
        QTimer::singleShot(0,this,[this]{ setEditorsToPages(); });
        return true;
    }
    return false;
}
