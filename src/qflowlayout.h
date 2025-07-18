#ifndef QFLOWLAYOUT_H
#define QFLOWLAYOUT_H

// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#include <QLayout>
#include <QRect>
#include <QStyle>
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

private:
    int doLayout(const QRect &rect, bool testOnly) const;
    int smartSpacing(QStyle::PixelMetric pm) const;

    QList<QLayoutItem *> itemList;
    int m_hSpace;
    int m_vSpace;
};
//! [0]

#endif // QFLOWLAYOUT_H
