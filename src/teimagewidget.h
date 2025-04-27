#ifndef TEIMAGEWIDGET_H
#define TEIMAGEWIDGET_H

#include "pch.h"
#include "tepicturelistview.h"
#include "tesignalwidget.h"

class teImageScrollArea : public QScrollArea {
    Q_OBJECT

public:
    explicit teImageScrollArea(QWidget *parent = nullptr);
    bool eventFilter(QObject *watched, QEvent *e)override{
        if(e->type()==QEvent::Wheel){
            wheelEvent(dynamic_cast<QWheelEvent *>(e));
            return true;
        }
        return false;
    }
    // Set image path
    void setImage(const QString &path);

    void wheelEvent(QWheelEvent *event) override;

    void mousePressEvent(QMouseEvent *event) override;

    void mouseMoveEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent*event) override{
        event->ignore();
    }
    QLabel *imageLabel;
    QPixmap originalPixmap;
    double scaleFactor;
    QPoint lastMousePosition;
    void scaleImage(double factor);
    void adjustImageScaleToFit() {
        scaleFactor = qMin((double)viewport()->width()/originalPixmap.width(),(double)viewport()->height()/originalPixmap.height());
        imageLabel->setPixmap(originalPixmap.scaled(viewport()->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }

};

class teImageWidget:public teWidget{
    Q_OBJECT
public:
    QVBoxLayout* content_layout;
    teImageScrollArea* imgArea;
    tePictureListView* pictureList;
    teImageWidget(QWidget*parent=nullptr);
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event)override{
        if(imgArea->width()>imgArea->imageLabel->width()&&imgArea->height()>imgArea->imageLabel->height())
            imgArea->adjustImageScaleToFit();
        return QWidget::resizeEvent(event);
    }
signals:
    void nextImage(); // Signal for next image
    void prevImage(); // Signal for previous image
};
#endif // TEIMAGEWIDGET_H
