#include "teimagewidget.h"

ImageDisplayWidget::ImageDisplayWidget(QWidget *parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setAttribute(Qt::WA_OpaquePaintEvent);
}

void ImageDisplayWidget::setImageList(const QVector<QString> &list)
{
    QString prevPath = currentImagePath();
    m_images = list;

    if (m_images.isEmpty()) {
        m_curIndex = -1;
        m_image = QImage();
        update();
        return;
    }

    m_curIndex = 0;
    if (prevPath != currentImagePath()) {
        loadImageAt(m_curIndex);
    } else {
        m_scale = 1.0;
        adjustToFit();
        update();
    }
}

void ImageDisplayWidget::setCurrentIndex(int idx)
{
    if (idx < 0 || idx >= m_images.size()) return;

    if (m_curIndex != idx || currentImagePath() != m_images[idx]) {
        m_curIndex = idx;
        loadImageAt(idx);
    }
}

QString ImageDisplayWidget::currentImagePath() const
{
    if (m_curIndex >= 0 && m_curIndex < m_images.size())
        return m_images[m_curIndex];
    return {};
}

void ImageDisplayWidget::next()
{
    if (m_images.isEmpty()) return;
    int nextIdx = (m_curIndex + 1) % m_images.size();
    setCurrentIndex(nextIdx);
}

void ImageDisplayWidget::prev()
{
    if (m_images.isEmpty()) return;
    int prevIdx = (m_curIndex - 1 + m_images.size()) % m_images.size();
    setCurrentIndex(prevIdx);
}

void ImageDisplayWidget::loadImageAt(int idx)
{
    if (idx < 0 || idx >= m_images.size()) return;

    QImageReader reader(m_images[idx]);
    reader.setAutoTransform(true);
    QImage img = reader.read();
    m_image = img.isNull() ? QImage() : img;

    m_scale = 1.0;
    m_offset = {0.0, 0.0};
    adjustToFit();
    update();
}

void ImageDisplayWidget::adjustToFit()
{
    if (m_image.isNull() || width() <= 0 || height() <= 0) return;

    const double sx = width()  / double(m_image.width());
    const double sy = height() / double(m_image.height());
    m_scale = qMin(sx, sy);

    const double visibleW = width()  / m_scale;
    const double visibleH = height() / m_scale;

    // 让图片居中：m_offset 是“视口左上角”对应的图像坐标
    m_offset.setX((m_image.width()  - visibleW) / 2.0);
    m_offset.setY((m_image.height() - visibleH) / 2.0);

    ensureBounds();
}

void ImageDisplayWidget::ensureBounds()
{
    if (m_image.isNull()) return;

    const double srcW = width()  / m_scale;
    const double srcH = height() / m_scale;

    // 保证视口与图片有交集，同时允许缩小时出现居中偏移
    const double minX = qMin(0.0, double(m_image.width())  - srcW);
    const double maxX = qMax(0.0, double(m_image.width())  - srcW);
    const double minY = qMin(0.0, double(m_image.height()) - srcH);
    const double maxY = qMax(0.0, double(m_image.height()) - srcH);

    if (m_offset.x() < minX) m_offset.setX(minX);
    if (m_offset.x() > maxX) m_offset.setX(maxX);
    if (m_offset.y() < minY) m_offset.setY(minY);
    if (m_offset.y() > maxY) m_offset.setY(maxY);
}

void ImageDisplayWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), Qt::black);

    if (m_image.isNull()) {
        p.setPen(Qt::white);
        p.drawText(rect(), Qt::AlignCenter, QStringLiteral("No image"));
        return;
    }

    // 视口对应的图像源区域
    QRectF srcRect(m_offset, QSizeF(width() / m_scale, height() / m_scale));
    QRectF imgRect(QPointF(0, 0), QSizeF(m_image.size()));

    // 只绘制可见部分
    QRectF inter = srcRect.intersected(imgRect);
    if (inter.isEmpty()) return;

    // 计算它在 widget 上应该落到的位置
    const QRectF targetRect(
        (inter.left()   - srcRect.left()) * m_scale,
        (inter.top()    - srcRect.top())  * m_scale,
        inter.width()   * m_scale,
        inter.height()  * m_scale
        );

    // 只画这一块，不再整图缩放
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.drawImage(targetRect, m_image, inter);
}

void ImageDisplayWidget::wheelEvent(QWheelEvent *ev)
{
    if (m_image.isNull()) return;

    const QPointF mouse = ev->position();

    // 鼠标点对应的图像坐标，缩放前先记录
    const QPointF imageCoord = m_offset + QPointF(mouse.x() / m_scale, mouse.y() / m_scale);

    const double factor = (ev->angleDelta().y() > 0) ? 1.1 : 0.9;
    m_scale *= factor;
    if (m_scale < 0.02)  m_scale = 0.02;
    if (m_scale > 128.0) m_scale = 128.0;

    // 重新计算 offset，保证鼠标指向的图像点不漂移
    m_offset = imageCoord - QPointF(mouse.x() / m_scale, mouse.y() / m_scale);

    ensureBounds();
    update();
    ev->accept();
}

void ImageDisplayWidget::mousePressEvent(QMouseEvent *ev)
{
    if (ev->button() == Qt::LeftButton) {
        setFocus();
        m_lastMouse = ev->pos();
        m_mousePressPos = ev->pos();
        m_dragging = true;
        ev->accept();
        return;
    }

    if (ev->button() == Qt::RightButton) {
        emit requestDeleteCurrent();
        ev->accept();
        return;
    }

    QWidget::mousePressEvent(ev);
}

void ImageDisplayWidget::mouseMoveEvent(QMouseEvent *ev)
{
    if (m_dragging && !m_image.isNull()) {
        const QPoint delta = ev->pos() - m_lastMouse;
        m_lastMouse = ev->pos();

        // 鼠标拖动多少像素，图像坐标反向平移多少
        m_offset -= QPointF(delta.x() / m_scale, delta.y() / m_scale);

        ensureBounds();
        update();
        ev->accept();
        return;
    }

    QWidget::mouseMoveEvent(ev);
}

void ImageDisplayWidget::mouseReleaseEvent(QMouseEvent *ev)
{
    if (ev->button() == Qt::LeftButton) {
        m_dragging = false;

        const QPoint releasePos = ev->pos();
        const int dx = releasePos.x() - m_mousePressPos.x();
        const int dy = releasePos.y() - m_mousePressPos.y();

        // 没有发生明显拖拽，视为点击：切下一张
        if (qAbs(dx) <= m_clickThreshold && qAbs(dy) <= m_clickThreshold) {
            next();
        }

        ev->accept();
        return;
    }

    QWidget::mouseReleaseEvent(ev);
}

void ImageDisplayWidget::keyPressEvent(QKeyEvent *ev)
{
    if (ev->key() == Qt::Key_Left || ev->key() == Qt::Key_Up) {
        emit prevImageRequested();
        ev->accept();
        return;
    }

    if (ev->key() == Qt::Key_Right || ev->key() == Qt::Key_Down) {
        emit nextImageRequested();
        ev->accept();
        return;
    }

    QWidget::keyPressEvent(ev);
}

void ImageDisplayWidget::resizeEvent(QResizeEvent *ev)
{
    QWidget::resizeEvent(ev);
    adjustToFit();
    update();
}

teImageWidget::teImageWidget(QWidget *parent)
    : teWidget(parent)
{
    content_layout = new QVBoxLayout(content);
    content_layout->setContentsMargins(0, 0, 0, 0);
    content_layout->setSpacing(0);

    imgView = new ImageDisplayWidget(this);
    imgView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    content_layout->addWidget(imgView);
    connect(imgView,&ImageDisplayWidget::nextImageRequested,this,&teImageWidget::nextImage);
    connect(imgView,&ImageDisplayWidget::prevImageRequested,this,&teImageWidget::prevImage);
}
void teImageWidget::setImage(const QString &path)
{
    if (!imgView) return;
    imgView->setImageList({path});
    imgView->setCurrentIndex(0);
}
void teImageWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Right || event->key() == Qt::Key_Down) {
        emit nextImage();
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Left || event->key() == Qt::Key_Up) {
        emit prevImage();
        event->accept();
        return;
    }

    teWidget::keyPressEvent(event);
}

void teImageWidget::resizeEvent(QResizeEvent *event)
{
    teWidget::resizeEvent(event);
}
