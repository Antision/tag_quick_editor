#include "teimagewidget.h"

teImageScrollArea::teImageScrollArea(QWidget *parent)
    : QScrollArea(parent), scaleFactor(1.0) {
    setMouseTracking(true);
    setAlignment(Qt::AlignCenter);
    setWidgetResizable(true);
    imageLabel = new QLabel(this);
    imageLabel->setAlignment(Qt::AlignCenter);
    imageLabel->installEventFilter(this);
    setWidget(imageLabel);
}

void teImageScrollArea::setImage(const QString &path) {
    originalPixmap.load(path);
    if (originalPixmap.isNull()) {
        telog("[teImageScrollArea::setImage]:image is null");
        return;
    }
    adjustImageScaleToFit();
}

void teImageScrollArea::wheelEvent(QWheelEvent *event) {
    if (!originalPixmap.isNull()) {
        if (event->angleDelta().y() > 0) {
            scaleImage(1.1); // Zoom in
        } else {
            scaleImage(0.9); // Zoom out
        }
    }
}

void teImageScrollArea::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        lastMousePosition = event->pos();
    }
}

void teImageScrollArea::mouseMoveEvent(QMouseEvent *event) {
    if (event->buttons() & Qt::LeftButton && !originalPixmap.isNull()) {
        QPoint delta = event->pos() - lastMousePosition;
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        lastMousePosition = event->pos();
    }
}

void teImageScrollArea::scaleImage(double factor) {
    scaleFactor *= factor;
    QSize newSize = originalPixmap.size() * scaleFactor;
    if (newSize.width() > viewport()->width() ||
        newSize.height() > viewport()->height()) {
        QPoint mousePosInLabel = imageLabel->mapFromGlobal(QCursor::pos());
        int origin_mousepos_x = mousePosInLabel.x();
        int origin_mousepos_y = mousePosInLabel.y();
        double xrate = (double)origin_mousepos_x/imageLabel->width();
        double yrate = (double)origin_mousepos_y/imageLabel->height();
        imageLabel->setPixmap(originalPixmap.scaled(newSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        int dx = newSize.width()*xrate-origin_mousepos_x;
        int dy = newSize.height()*yrate-origin_mousepos_y;
        horizontalScrollBar()->setValue(horizontalScrollBar()->value()+dx);
        verticalScrollBar()->setValue(verticalScrollBar()->value()+dy);
    } else {
        adjustImageScaleToFit();
    }
}

teImageWidget::teImageWidget(QWidget *parent):teWidget(parent){
    content_layout = new QVBoxLayout(content);
    content->setLayout(content_layout);
    content_layout->setContentsMargins(0,0,0,0);
    imgArea = new teImageScrollArea(this);
    content_layout->addWidget(imgArea);
}

void teImageWidget::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Left||event->key() == Qt::Key_Up) {
        emit prevImage();
    } else if (event->key() == Qt::Key_Right||event->key() == Qt::Key_Down) {
        emit nextImage();
    }else if (event->key() == Qt::Key_R) {
        imgArea->adjustImageScaleToFit();
    }
}
