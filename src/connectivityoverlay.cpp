#include "connectivityoverlay.h"

#include "mainwindow.h"
#include "teeditor.h"
#include "teeditorcontrol.h"
#include "teeditorlist.h"

#include "taglistmodel.h"
#include "taglistview.h"

#include <QApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollArea>
#include <QScrollBar>
#include <QSplitter>
#include <QTimer>

#include <array>
#include <set>

namespace {
/// 0.8 over an editor column, 0.2 while only the tag list is under the pointer.
constexpr int kColumnAlpha=204;
constexpr int kTagRowAlpha=51;
constexpr int kDotRadius=2;
constexpr int kMinIntervalMs=16;

/// "Attach once" marker for the widgets that need a hook (see attachPage).
constexpr const char* kAttachedProperty="tqLinkAttached";

bool sameWeakTag(const std::weak_ptr<teTag>& first,const std::weak_ptr<teTag>& second)
{
    return !first.owner_before(second)&&!second.owner_before(first);
}
}

ConnectivityOverlay::ConnectivityOverlay(MainWindow* window,TagListView* tagView,teEditorList* editors)
    : QWidget(window),m_window(window),m_tagView(tagView),m_editors(editors)
{
    setObjectName(QStringLiteral("connectivityOverlay"));
    // A sheet, not a control: clicks, hover and the wheel all belong to whatever
    // is underneath (LINK-05: "must not affect hit testing or selection").
    setAttribute(Qt::WA_TransparentForMouseEvents,true);
    setAttribute(Qt::WA_NoSystemBackground,true);
    setFocusPolicy(Qt::NoFocus);

    m_timer=new QTimer(this);
    m_timer->setSingleShot(true);
    m_timer->setInterval(kMinIntervalMs);
    connect(m_timer,&QTimer::timeout,this,[this]{
        attachPages();
        const QRect oldGeometry=geometry();
        const QRect previous=segmentsBounds();
        reposition();
        refreshSegments();
        if(geometry()!=oldGeometry)
            update();               // the sheet moved: nothing of the old frame applies
        else
            repaintArea(previous);
    });

    if(QApplication::instance())
        QApplication::instance()->installEventFilter(this);
    if(m_window){
        m_window->installEventFilter(this);
        if(m_window->splitter){
            m_window->splitter->installEventFilter(this);
            connect(m_window->splitter,&QSplitter::splitterMoved,this,&ConnectivityOverlay::schedule);
        }
    }
    if(m_editors&&m_editors->pageSplitter){
        m_editors->pageSplitter->installEventFilter(this);
        connect(m_editors->pageSplitter,&QSplitter::splitterMoved,this,&ConnectivityOverlay::schedule);
    }
    if(m_tagView){
        m_tagView->installEventFilter(this);
        if(m_tagView->viewport())
            m_tagView->viewport()->installEventFilter(this);
        if(auto* bar=m_tagView->verticalScrollBar())
            connect(bar,&QScrollBar::valueChanged,this,&ConnectivityOverlay::schedule);
        attachModel(m_tagView->tagModel());
    }
    attachPages();
    reposition();
    raise();
}

void ConnectivityOverlay::attachModel(TagListModel* model)
{
    if(!model||model->property(kAttachedProperty).toBool())
        return;
    model->setProperty(kAttachedProperty,true);
    // Any structural or content change moves rows around; all of them end in one
    // repaint through the timer.
    const auto changed=[this]{ schedule(); };
    connect(model,&QAbstractItemModel::modelReset,this,changed);
    connect(model,&QAbstractItemModel::layoutChanged,this,changed);
    connect(model,&QAbstractItemModel::rowsInserted,this,changed);
    connect(model,&QAbstractItemModel::rowsRemoved,this,changed);
    connect(model,&QAbstractItemModel::rowsMoved,this,changed);
    connect(model,&QAbstractItemModel::dataChanged,this,changed);
}

void ConnectivityOverlay::attachPage(QWidget* page)
{
    if(!page||page->property(kAttachedProperty).toBool())
        return;
    page->setProperty(kAttachedProperty,true);
    // A page column scrolls, moves and gets new editors; watching the scroll area
    // (and its container) is enough - widgets inside report their own resize
    // through the application filter.
    page->installEventFilter(this);
    if(auto* scroll=qobject_cast<QScrollArea*>(page)){
        if(scroll->viewport())
            scroll->viewport()->installEventFilter(this);
        if(scroll->widget())
            scroll->widget()->installEventFilter(this);
        if(auto* bar=scroll->verticalScrollBar())
            connect(bar,&QScrollBar::valueChanged,this,&ConnectivityOverlay::schedule);
    }
}

void ConnectivityOverlay::attachPages()
{
    if(!m_editors||!m_editors->editorLayout)
        return;
    for(QWidget* page:m_editors->editorLayout->pages)
        attachPage(page);
}

int ConnectivityOverlay::pageCount() const
{
    if(!m_editors||!m_editors->editorLayout)
        return 0;
    return int(m_editors->editorLayout->pages.size());
}

void ConnectivityOverlay::reposition()
{
    if(!m_window||!m_window->splitter)
        return;
    const QRect area(m_window->splitter->mapTo(m_window,QPoint(0,0)),
                     m_window->splitter->size());
    if(geometry()!=area){
        setGeometry(area);
        // Siblings are painted in creation order; a page column or the tag list
        // must never end up above the lines.
        raise();
    }
}

void ConnectivityOverlay::schedule()
{
    if(!m_timer->isActive())
        m_timer->start();
}

void ConnectivityOverlay::updateHoverAt(const QPoint& globalPos)
{
    // The sheet has to sit on the splitter before anything is measured against it:
    // the first hover can arrive before the layout has settled.
    const QRect oldGeometry=geometry();
    reposition();
    const bool moved=geometry()!=oldGeometry;

    Hover next=Hover::None;
    int nextPage=-1;
    std::weak_ptr<teTag> nextTag;

    for(int i=0;i<pageCount();++i){
        QWidget* page=m_editors->editorLayout->pages[i];
        if(!page||!page->isVisible())
            continue;
        if(QRect(page->mapToGlobal(QPoint(0,0)),page->size()).contains(globalPos)){
            next=Hover::Column;
            nextPage=i;
            break;
        }
    }

    if(next==Hover::None&&m_tagView&&m_tagView->isVisible()&&m_tagView->viewport()
        &&m_tagView->viewport()->rect().contains(m_tagView->viewport()->mapFromGlobal(globalPos))){
        const QModelIndex index=m_tagView->indexAt(m_tagView->viewport()->mapFromGlobal(globalPos));
        TagListModel* model=m_tagView->tagModel();
        // Only a row carries a tag; the empty area below the last row draws
        // nothing (the pointer is "on the list" but there is no set to show).
        if(index.isValid()&&model&&m_tagView->showing_list){
            if(teTag* raw=model->tagAt(index.row())){
                for(const std::shared_ptr<teTag>& tag:m_tagView->showing_list->getTags())
                    if(tag.get()==raw){
                        next=Hover::TagRow;
                        nextTag=tag;
                        break;
                    }
            }
        }
    }

    const bool changed=next!=m_hover||nextPage!=m_hoverPage
                        ||(next==Hover::TagRow&&!sameWeakTag(nextTag,m_hoverTag));
    if(!changed&&!moved)
        return;
    const QRect previous=segmentsBounds();
    m_hover=next;
    m_hoverPage=nextPage;
    m_hoverTag=nextTag;
    refreshSegments();
    if(moved)
        update();       // the sheet moved: the old frame is not where the lines are
    else
        repaintArea(previous);
}

void ConnectivityOverlay::refreshSegments()
{
    m_segments.clear();
    if(m_hover!=Hover::None)
        collectSegments(m_segments);
    m_lastSegments=int(m_segments.size());
}

QRect ConnectivityOverlay::segmentsBounds() const
{
    QRect bounds;
    for(const LinkSegment& segment:m_segments)
        bounds=bounds.united(QRect(segment.from,segment.to).normalized());
    return bounds;
}

void ConnectivityOverlay::repaintArea(const QRect& previous)
{
    // A line to a row scrolled out of the list can reach far outside the sheet;
    // only the part on the sheet is ever painted, and only that part may make the
    // widgets underneath repaint.
    const QRect dirty=previous.united(segmentsBounds()).intersected(rect());
    if(dirty.isNull())
        return;
    // The dots stick out by their radius, and antialiasing adds a pixel around
    // the line itself.
    update(dirty.adjusted(-kDotRadius-2,-kDotRadius-2,kDotRadius+2,kDotRadius+2));
}

bool ConnectivityOverlay::eventFilter(QObject* watched,QEvent* event)
{
    switch(event->type()){
    case QEvent::MouseMove:{
        if(auto* mouse=static_cast<QMouseEvent*>(event))
            updateHoverAt(mouse->globalPosition().toPoint());
        break;
    }
    case QEvent::Leave:
        // Leaving the window is the only "the pointer is gone" notification that
        // reaches an application filter (the sheet itself never sees the mouse).
        if(watched==m_window&&m_hover!=Hover::None){
            const QRect previous=segmentsBounds();
            m_hover=Hover::None;
            m_hoverPage=-1;
            m_hoverTag.reset();
            refreshSegments();
            repaintArea(previous);
        }
        break;
    case QEvent::Resize:
    case QEvent::Move:
    case QEvent::Show:
    case QEvent::Hide:
    case QEvent::LayoutRequest:
    case QEvent::Scroll:
    case QEvent::Wheel:
        schedule();
        break;
    default:
        break;
    }
    return QWidget::eventFilter(watched,event);
}

void ConnectivityOverlay::collectPageLinks(int pageIndex,TagListModel* model,
                                           const QPoint& origin,QVector<LinkSegment>& out,
                                           const teTag* onlyTag) const
{
    if(pageIndex<0||pageIndex>=pageCount())
        return;
    QScrollArea* page=m_editors->editorLayout->pages[pageIndex];
    if(!page||!page->isVisible()||!page->viewport())
        return;
    QWidget* container=page->widget();
    QLayout* layout=container?container->layout():nullptr;
    if(!layout)
        return;

    for(int i=0;i<layout->count();++i){
        auto* editor=dynamic_cast<teEditor*>(layout->itemAt(i)->widget());
        if(!editor||!editor->isVisible())
            continue;
        for(EditorControl* control:editor->controls){
            auto* widget=dynamic_cast<QWidget*>(control);
            // A control inside a folded editor or a closed sub page is still
            // linked, but it is not on screen: no line may start there.
            if(!widget||!widget->isVisible())
                continue;
            const QRect controlRect=control->visualRect();
            if(controlRect.isEmpty())
                continue;
            for(const std::shared_ptr<teTag>& tag:control->linked_tags){
                if(!tag||(onlyTag&&tag.get()!=onlyTag))
                    continue;
                // A claim can outlive the rule that made it (a control is told
                // about the edit, but a link that was never cleaned up keeps the
                // tag). Such a line would end at the control's corner while the
                // control does not own the tag any more, which is the "line to a
                // fixed spot" bug. Ask the control again.
                if(!control->filter(tag))
                    continue;
                const int row=model->rowOf(tag.get());
                if(row<0)
                    continue;
                const QRect rowRect=m_tagView->visualRect(model->index(row,0));
                if(rowRect.isEmpty())
                    continue;
                // Start at the chip showing this tag when the control draws one
                // per tag (the editors' ref lists do): the list panel itself is
                // not what the tag is attached to, and a line to its corner is a
                // line to nothing in particular.
                QRect from=controlRect;
                if(teTagWidgetBase* chip=control->tagWidgetFor(tag)){
                    auto* chipWidget=dynamic_cast<QWidget*>(chip);
                    if(chipWidget&&chipWidget->isVisible())
                        from=QRect(chipWidget->mapToGlobal(QPoint(0,0)),chipWidget->size());
                }
                LinkSegment segment;
                // No clipping: a row scrolled out of the list still has a real
                // position, and the line is meant to leave the window to reach
                // it (only the part inside the sheet is drawn).
                if(!linkEndpoints(from,
                                  QRect(m_tagView->viewport()->mapToGlobal(rowRect.topLeft()),
                                        rowRect.size()),
                                  &segment.from,&segment.to,kLinkInset))
                    continue;
                segment.visible=true;
                segment.from-=origin;
                segment.to-=origin;
                out.append(segment);
            }
        }
    }
}

void ConnectivityOverlay::collectSegments(QVector<LinkSegment>& out) const
{
    if(m_hover==Hover::None||!m_tagView||!m_tagView->isVisible()||!m_tagView->tagModel()
        ||!m_tagView->viewport()||!m_tagView->showing_list)
        return;
    TagListModel* model=m_tagView->tagModel();
    const QPoint origin=mapToGlobal(QPoint(0,0));

    if(m_hover==Hover::Column){
        collectPageLinks(m_hoverPage,model,origin,out,nullptr);
    }else{
        // Over the tag list: the set of controls that capture exactly this tag
        // ("the same-name set"), across every column.
        const std::shared_ptr<teTag> tag=m_hoverTag.lock();
        if(!tag)
            return;
        for(int i=0;i<pageCount();++i)
            collectPageLinks(i,model,origin,out,tag.get());
    }

    // Two controls can end on the same chip (a colour button row and the ref list
    // both claim the tag); drawing the same line twice only makes it brighter.
    std::set<std::array<int,4>> seen;
    QVector<LinkSegment> unique;
    unique.reserve(out.size());
    for(const LinkSegment& segment:out){
        if(seen.insert({segment.from.x(),segment.from.y(),
                        segment.to.x(),segment.to.y()}).second)
            unique.append(segment);
    }
    out=std::move(unique);
}

void ConnectivityOverlay::paintEvent(QPaintEvent*)
{
    if(m_segments.isEmpty())
        return;

    const int alpha=m_hover==Hover::Column?kColumnAlpha:kTagRowAlpha;
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing,true);
    painter.setBrush(Qt::NoBrush);

    // One colour per line, cycling through linkColor() in drawing order. With the
    // tag list and the first column only a scrollbar apart, the lines leave in
    // nearly the same direction and the colour is what tells them apart.
    for(int i=0;i<int(m_segments.size());++i){
        QColor color=linkColor(i);
        color.setAlpha(alpha);
        QPen pen(color);
        pen.setWidthF(1.0);
        pen.setDashPattern({4.0,3.0});
        painter.setPen(pen);
        painter.drawLine(m_segments[i].from,m_segments[i].to);
    }

    // The dots are white and sit inside the border (LINK-03). They fade with the
    // line, otherwise the 20% state would show bright dots on faint lines.
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(255,255,255,alpha));
    for(const LinkSegment& segment:m_segments){
        painter.drawEllipse(segment.from,kDotRadius,kDotRadius);
        painter.drawEllipse(segment.to,kDotRadius,kDotRadius);
    }
}
