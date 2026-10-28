#include "widgets/FullScreenView.h"

#include <QApplication>
#include <QKeyEvent>
#include <QPainter>

FullScreenView::FullScreenView(const QImage &picture, QWidget *parent)
    : QWidget(parent, Qt::Window)
    , m_picture(picture)
{
    setObjectName(QStringLiteral("fullScreenView"));
    setAttribute(Qt::WA_DeleteOnClose);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setWindowTitle(QApplication::applicationDisplayName());
    setAccessibleName(tr("Full screen picture"));
    setAccessibleDescription(tr("Press any key or click to return"));
}

void FullScreenView::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(rect(), Qt::black);
    // Actual size, centred; pictures larger than the screen are shrunk to fit.
    QSizeF size = m_picture.size();
    const bool shrink = size.width() > width() || size.height() > height();
    if (shrink) {
        size.scale(QSizeF(this->size()), Qt::KeepAspectRatio);
    }
    QRectF target(QPointF(), size);
    target.moveCenter(QRectF(rect()).center());
    painter.setRenderHint(QPainter::SmoothPixmapTransform, shrink);
    painter.drawImage(target, m_picture);
}

void FullScreenView::keyPressEvent(QKeyEvent *event)
{
    event->accept();
    close();
}

void FullScreenView::mousePressEvent(QMouseEvent *event)
{
    event->accept();
    close();
}

void FullScreenView::closeEvent(QCloseEvent *event)
{
    Q_EMIT closed();
    QWidget::closeEvent(event);
}
