#ifndef TEIMAGEWIDGET_H
#define TEIMAGEWIDGET_H

#include "pch.h"
#include "tepicturelistview.h"
#include "tesignalwidget.h"


class ImageDisplayWidget : public QWidget {
    Q_OBJECT
public:
    explicit ImageDisplayWidget(QWidget *parent = nullptr);

    void setImageList(const QVector<QString> &list);
    void setCurrentIndex(int idx);

    QString currentImagePath() const;
    void next();
    void prev();

signals:
    void nextImageRequested();
    void prevImageRequested();
    void requestDeleteCurrent();

protected:
    void paintEvent(QPaintEvent *) override;
    void wheelEvent(QWheelEvent *ev) override;
    void mousePressEvent(QMouseEvent *ev) override;
    void mouseMoveEvent(QMouseEvent *ev) override;
    void mouseReleaseEvent(QMouseEvent *ev) override;
    void resizeEvent(QResizeEvent *ev) override;
    void keyPressEvent(QKeyEvent *ev) override;

private:
    void loadImageAt(int idx);
    void adjustToFit();
    void ensureBounds();

private:
    QVector<QString> m_images;
    int m_curIndex = -1;

    QImage m_image;

    double m_scale = 1.0;
    QPointF m_offset {0.0, 0.0};   // 视口左上角对应的图像坐标
    QPoint  m_lastMouse;
    QPoint  m_mousePressPos;
    bool    m_dragging = false;
    int     m_clickThreshold = 4;
};

class teImageWidget : public teWidget {
    Q_OBJECT
public:
    explicit teImageWidget(QWidget *parent = nullptr);

    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void setImage(const QString &path);
signals:
    void nextImage();
    void prevImage();

private:
    QVBoxLayout *content_layout = nullptr;
    ImageDisplayWidget *imgView = nullptr;
    tePictureListView *pictureList = nullptr;
};
#endif // TEIMAGEWIDGET_H
