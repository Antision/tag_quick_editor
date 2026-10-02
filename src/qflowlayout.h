#ifndef QFLOWLAYOUT_H
#define QFLOWLAYOUT_H

// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#include <QLayout>
#include <QObject>
#include <QRect>
#include <QStyle>
#include <functional>

//! [0]
class QFlowLayout : public QLayout
{
public:
    explicit QFlowLayout(QWidget *parent, int margin = -1, int hSpacing = -1, int vSpacing = -1);
    explicit QFlowLayout(int margin = -1, int hSpacing = -1, int vSpacing = -1);
    ~QFlowLayout();

    void addItem(QLayoutItem *item) override;
    void insertWidget(int index,QWidget *wid);

    int horizontalSpacing() const;
    int verticalSpacing() const;
    void setHorizontalSpacing(int hs){m_hSpace = hs;};
    void setVerticalSpacing(int vs) {m_vSpace = vs;};
    Qt::Orientations expandingDirections() const override;
    bool hasHeightForWidth() const override;
    int heightForWidth(int) const override;
    int count() const override;
    QLayoutItem *itemAt(int index) const override;
    QSize minimumSize() const override;
    void setGeometry(const QRect &rect) override;
    QSize sizeHint() const override;
    QLayoutItem *takeAt(int index) override;

    /// Moves the item at `from` to index `to` (both in layout order).
    /// Returns false when either index is out of range.
    bool moveItem(int from,int to);

    /// Index of the item whose geometry contains `pos` (parent coordinates).
    /// Returns -1 when the position is not over any item.
    int indexAt(const QPoint& pos) const;

    /// Index a widget dropped at `pos` should be inserted at: the index of the
    /// item under the cursor, or the last index for a position past the end.
    /// Returns -1 only when the layout is empty.
    int insertIndexAt(const QPoint& pos) const;

private:
    int doLayout(const QRect &rect, bool testOnly) const;
    int smartSpacing(QStyle::PixelMetric pm) const;

    QList<QLayoutItem *> itemList;
    int m_hSpace;
    int m_vSpace;
};
//! [0]

/**
 * @brief Drag & drop reordering for the widgets managed by a QFlowLayout.
 *
 * Install it as an event filter on every reorderable child. A press/release
 * without movement is left alone, so the child keeps its normal click
 * behaviour; as soon as the pointer moves past QApplication::startDragDistance()
 * the widget is dragged and the layout items are shuffled live. The release of
 * a drag is swallowed, which is what keeps a "click to remove" chip from also
 * reacting to a drag.
 *
 * The owner is notified through reordered() once the drag finished, so it can
 * persist the new order.
 */
class QFlowLayoutReorderer : public QObject
{
    Q_OBJECT
public:
    explicit QFlowLayoutReorderer(QFlowLayout* layout,QWidget* container,QObject* parent=nullptr);

    /// Installs the reorderer as an event filter on `widget`.
    void attach(QWidget* widget);

    /// Indices below this one are never used as a drop target (e.g. a permanent
    /// line edit living at index 0).
    void setFirstMovableIndex(int index){ m_firstMovableIndex = index; }
    int firstMovableIndex() const { return m_firstMovableIndex; }

    bool isDragging() const { return m_dragging; }

signals:
    /// The user finished dragging a widget to a new position.
    void reordered();

protected:
    bool eventFilter(QObject* watched,QEvent* event) override;

private:
    void beginDrag(const QPoint& globalPos);
    void updateDrag(const QPoint& globalPos);
    void endDrag();

    QFlowLayout* m_layout;
    QWidget* m_container;
    QWidget* m_candidate = nullptr;
    QWidget* m_dragged = nullptr;
    QPoint m_pressGlobal;
    int m_draggedIndex = -1;
    int m_firstMovableIndex = 0;
    bool m_dragging = false;
};

#endif // QFLOWLAYOUT_H
