#include "canvas/ThumbnailView.h"

#include "canvas/CanvasView.h"

#include <QEvent>
#include <QMouseEvent>
#include <QPainter>

namespace {

constexpr int Padding = 6;
constexpr int Inset = 12;
// Rebuilding the small picture waits for a pause in drawing.
constexpr int RebuildDelay = 120;

} // namespace

ThumbnailView::ThumbnailView(CanvasView *canvas)
    : QWidget(canvas)
    , m_canvas(canvas)
    , m_viewport(canvas->viewport())
{
    setObjectName(QStringLiteral("canvasView.thumbnail"));
    setAccessibleName(tr("Thumbnail"));
    setToolTip(tr("Thumbnail: click to show that part of the picture"));
    setCursor(Qt::PointingHandCursor);
    m_rebuild.setSingleShot(true);
    m_rebuild.setInterval(RebuildDelay);
    connect(&m_rebuild, &QTimer::timeout, this, &ThumbnailView::rebuild);
    connect(canvas, &CanvasView::pictureChanged, this, [this] {
        if (isVisible()) {
            m_rebuild.start();
        }
    });
    connect(canvas, &CanvasView::viewChanged, this, qOverload<>(&QWidget::update));
    m_viewport->installEventFilter(this);
    hide();
}

QRectF ThumbnailView::pictureRect() const
{
    return QRectF(QPointF(Padding, Padding), QSizeF(m_picture.size()));
}

void ThumbnailView::rebuild()
{
    const QImage &picture = m_canvas->picture();
    m_picture = picture.scaled(MaximumPicture, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    setFixedSize(m_picture.size() + QSize(2 * Padding, 2 * Padding));
    reposition();
    update();
}

void ThumbnailView::reposition()
{
    if (!m_viewport) {
        return;
    }
    const QRect area = m_viewport->geometry();
    move(area.right() - width() - Inset + 1, area.top() + Inset);
    raise();
}

void ThumbnailView::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    rebuild();
}

bool ThumbnailView::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_viewport && (event->type() == QEvent::Resize || event->type() == QEvent::Move)) {
        reposition();
    }
    return QWidget::eventFilter(watched, event);
}

void ThumbnailView::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    // A rounded card like the side panel's.
    QColor border = palette().color(QPalette::WindowText);
    border.setAlphaF(0.12);
    painter.setPen(QPen(border, 1));
    painter.setBrush(palette().color(QPalette::Window));
    painter.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 6, 6);

    const QRectF target = pictureRect();
    painter.fillRect(target, Qt::white);
    painter.drawImage(target.topLeft(), m_picture);

    // The part of the picture the canvas shows, when it does not show all of it.
    const QSize document = m_canvas->picture().size();
    const QRectF shown = m_canvas->visibleDocumentRect();
    if (document.isEmpty() || (shown.width() >= document.width() && shown.height() >= document.height())) {
        return;
    }
    const qreal scale = target.width() / document.width();
    const QRectF outline(target.topLeft() + shown.topLeft() * scale, shown.size() * scale);
    // A white edge under the accent keeps the outline visible on any picture.
    const QRectF frame = outline.intersected(target).adjusted(1.5, 1.5, -1.5, -1.5);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(Qt::white, 3));
    painter.drawRect(frame);
    painter.setPen(QPen(palette().color(QPalette::Highlight).darker(130), 1.5));
    painter.drawRect(frame);
}

void ThumbnailView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        moveViewTo(event->position());
    }
}

void ThumbnailView::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton) {
        moveViewTo(event->position());
    }
}

void ThumbnailView::moveViewTo(QPointF position)
{
    const QRectF target = pictureRect();
    if (target.isEmpty()) {
        return;
    }
    const qreal scale = m_canvas->picture().width() / target.width();
    m_canvas->centerOn((position - target.topLeft()) * scale);
}
