#include "qflowlayout.h"
// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

//! [1]
QFlowLayout::QFlowLayout(QWidget *parent, int margin, int hSpacing, int vSpacing)
    : QLayout(parent), m_hSpace(hSpacing), m_vSpace(vSpacing)
{
    setContentsMargins(margin, margin, margin, margin);
}

QFlowLayout::QFlowLayout(int margin, int hSpacing, int vSpacing)
    : m_hSpace(hSpacing), m_vSpace(vSpacing)
{
    setContentsMargins(margin, margin, margin, margin);
}
//! [1]

//! [2]
QFlowLayout::~QFlowLayout()
{
    QLayoutItem *item;
    while ((item = takeAt(0)))
        delete item;
}
//! [2]

//! [3]
void QFlowLayout::addItem(QLayoutItem *item)
{
    itemList.append(item);
}

void QFlowLayout::insertWidget(int index, QWidget *wid){
    if (index > itemList.size()) {
        index = itemList.size();
    }else if(index < 0){
        index+=itemList.size()+1;
    }

    if (QWidget* pw = parentWidget()) {
        wid->setParent(pw);
        wid->show();
    }

    QLayoutItem* item = new QWidgetItem(wid);
    item->setAlignment(alignment());

    itemList.insert(index, item);

    invalidate();
    if (parentWidget()) {
        parentWidget()->updateGeometry();
    }
}


//! [3]

//! [4]
int QFlowLayout::horizontalSpacing() const
{
    if (m_hSpace >= 0) {
        return m_hSpace;
    } else {
        return smartSpacing(QStyle::PM_LayoutHorizontalSpacing);
    }
}

int QFlowLayout::verticalSpacing() const
{
    if (m_vSpace >= 0) {
        return m_vSpace;
    } else {
        return smartSpacing(QStyle::PM_LayoutVerticalSpacing);
    }
}
//! [4]

//! [5]
int QFlowLayout::count() const
{
    return itemList.size();
}

QLayoutItem *QFlowLayout::itemAt(int index) const
{
    return itemList.value(index);
}

QLayoutItem *QFlowLayout::takeAt(int index)
{
    if (index >= 0 && index < itemList.size())
        return itemList.takeAt(index);
    return nullptr;
}
//! [5]

//! [6]
Qt::Orientations QFlowLayout::expandingDirections() const
{
    return { };
}
//! [6]

//! [7]
bool QFlowLayout::hasHeightForWidth() const
{
    return true;
}

int QFlowLayout::heightForWidth(int width) const
{
    int height = doLayout(QRect(0, 0, width, 0), true);
    return height;
}
//! [7]

//! [8]
void QFlowLayout::setGeometry(const QRect &rect)
{
    QLayout::setGeometry(rect);
    doLayout(rect, false);
}

QSize QFlowLayout::sizeHint() const
{
    return minimumSize();
}

QSize QFlowLayout::minimumSize() const
{
    QSize size;
    for (const QLayoutItem *item : std::as_const(itemList))
        size = size.expandedTo(item->minimumSize());

    const QMargins margins = contentsMargins();
    size += QSize(margins.left() + margins.right(), margins.top() + margins.bottom());
    return size;
}
//! [8]

//! [9]
int QFlowLayout::doLayout(const QRect &rect, bool testOnly) const
{
    int left, top, right, bottom;
    getContentsMargins(&left, &top, &right, &bottom);
    QRect effectiveRect = rect.adjusted(+left, +top, -right, -bottom);
    int x = effectiveRect.x();
    int y = effectiveRect.y();
    int lineHeight = 0;
    //! [9]

    //! [10]
    for (QLayoutItem *item : std::as_const(itemList)) {
        const QWidget *wid = item->widget();
        int spaceX = horizontalSpacing();
        if (spaceX == -1)
            spaceX = wid->style()->layoutSpacing(
                QSizePolicy::PushButton, QSizePolicy::PushButton, Qt::Horizontal);
        int spaceY = verticalSpacing();
        if (spaceY == -1)
            spaceY = wid->style()->layoutSpacing(
                QSizePolicy::PushButton, QSizePolicy::PushButton, Qt::Vertical);
        //! [10]
        //! [11]
        int nextX = x + item->sizeHint().width() + spaceX;
        if (nextX - spaceX > effectiveRect.right() && lineHeight > 0) {
            x = effectiveRect.x();
            y = y + lineHeight + spaceY;
            nextX = x + item->sizeHint().width() + spaceX;
            lineHeight = 0;
        }

        if (!testOnly)
            item->setGeometry(QRect(QPoint(x, y), item->sizeHint()));

        x = nextX;
        lineHeight = qMax(lineHeight, item->sizeHint().height());
    }
    return y + lineHeight - rect.y() + bottom;
}
//! [11]
//! [12]
int QFlowLayout::smartSpacing(QStyle::PixelMetric pm) const
{
    QObject *parent = this->parent();
    if (!parent) {
        return -1;
    } else if (parent->isWidgetType()) {
        QWidget *pw = static_cast<QWidget *>(parent);
        return pw->style()->pixelMetric(pm, nullptr, pw);
    } else {
        return static_cast<QLayout *>(parent)->spacing();
    }
}
//! [12]

bool QFlowLayout::moveItem(int from, int to)
{
    if (from < 0 || from >= itemList.size() || to < 0 || to >= itemList.size() || from == to)
        return false;
    itemList.move(from, to);
    invalidate();
    if (QWidget* pw = parentWidget())
        pw->updateGeometry();
    return true;
}

int QFlowLayout::indexAt(const QPoint& pos) const
{
    for (int i = 0; i < itemList.size(); ++i) {
        if (itemList[i]->geometry().contains(pos))
            return i;
    }
    return -1;
}

int QFlowLayout::insertIndexAt(const QPoint& pos) const
{
    if (itemList.isEmpty())
        return -1;

    const int hovered = indexAt(pos);
    if (hovered >= 0)
        return hovered;

    // Past the edge of a row: pick the item whose rectangle is closest, so that
    // dropping below the last row still lands on the last item.
    int nearest = itemList.size() - 1;
    int bestDistance = std::numeric_limits<int>::max();
    for (int i = 0; i < itemList.size(); ++i) {
        const QRect geometry = itemList[i]->geometry();
        const int dy = pos.y() < geometry.top() ? geometry.top() - pos.y()
                       : (pos.y() > geometry.bottom() ? pos.y() - geometry.bottom() : 0);
        const int dx = pos.x() < geometry.left() ? geometry.left() - pos.x()
                       : (pos.x() > geometry.right() ? pos.x() - geometry.right() : 0);
        // Rows dominate: a hit one row away is closer than a far-away cell of
        // the current row.
        const int distance = dy * 1000 + dx;
        if (distance < bestDistance) {
            bestDistance = distance;
            nearest = i;
        }
    }
    return nearest;
}

QFlowLayoutReorderer::QFlowLayoutReorderer(QFlowLayout* layout,QWidget* container,QObject* parent)
    : QObject(parent), m_layout(layout), m_container(container)
{
}

void QFlowLayoutReorderer::attach(QWidget* widget)
{
    if (widget)
        widget->installEventFilter(this);
}

void QFlowLayoutReorderer::beginDrag(const QPoint& globalPos)
{
    m_dragging = true;
    m_dragged = m_candidate;
    m_draggedIndex = -1;
    for (int i = 0; i < m_layout->count(); ++i) {
        if (m_layout->itemAt(i)->widget() == m_dragged) {
            m_draggedIndex = i;
            break;
        }
    }
    if (m_draggedIndex < 0) {
        m_dragging = false;
        return;
    }
    m_dragged->setCursor(Qt::ClosedHandCursor);
    m_dragged->raise();
    updateDrag(globalPos);
}

void QFlowLayoutReorderer::updateDrag(const QPoint& globalPos)
{
    if (!m_dragged || m_draggedIndex < 0)
        return;
    const QPoint local = m_container->mapFromGlobal(globalPos);
    int target = m_layout->insertIndexAt(local);
    if (target < 0)
        return;
    target = std::max(target, m_firstMovableIndex);
    if (target == m_draggedIndex)
        return;
    if (m_layout->moveItem(m_draggedIndex, target))
        m_draggedIndex = target;
}

void QFlowLayoutReorderer::endDrag()
{
    if (m_dragged) {
        m_dragged->unsetCursor();
        m_dragged->update();
    }
    const bool wasDragging = m_dragging;
    m_dragging = false;
    m_dragged = nullptr;
    m_draggedIndex = -1;
    m_candidate = nullptr;
    if (wasDragging)
        emit reordered();
}

bool QFlowLayoutReorderer::eventFilter(QObject* watched,QEvent* event)
{
    switch (event->type()) {
    case QEvent::MouseButtonPress: {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() != Qt::LeftButton)
            return false;
        m_candidate = qobject_cast<QWidget*>(watched);
        m_pressGlobal = mouseEvent->globalPosition().toPoint();
        return false;       // the widget keeps its normal press handling
    }
    case QEvent::MouseMove: {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if (!m_candidate || !(mouseEvent->buttons() & Qt::LeftButton))
            return false;
        if (!m_dragging) {
            const int distance = (mouseEvent->globalPosition().toPoint() - m_pressGlobal).manhattanLength();
            if (distance < QApplication::startDragDistance())
                return false;
            beginDrag(mouseEvent->globalPosition().toPoint());
            if (!m_dragging)
                return false;
        }
        updateDrag(mouseEvent->globalPosition().toPoint());
        return true;        // consumed: this is a drag, not a click
    }
    case QEvent::MouseButtonRelease: {
        if (m_dragging) {
            endDrag();
            // Swallowing the release is what prevents the "clicked" signal, so
            // a drag never also triggers the click action of the widget.
            return true;
        }
        m_candidate = nullptr;
        return false;
    }
    case QEvent::Hide:
    case QEvent::Destroy:
        if (watched == m_candidate || watched == m_dragged) {
            m_candidate = nullptr;
            m_dragged = nullptr;
            m_dragging = false;
            m_draggedIndex = -1;
        }
        return false;
    default:
        return false;
    }
}

