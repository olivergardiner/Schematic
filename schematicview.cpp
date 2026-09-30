#include "schematicview.h"

#include <QMouseEvent>
#include <QWheelEvent>
#include <QPainter>
#include <QScrollBar>
#include <QtMath>
#include <cmath>

SchematicView::SchematicView(QWidget *parent)
    : QGraphicsView(parent)
{
    setRenderHint(QPainter::Antialiasing, true);
    setRenderHint(QPainter::TextAntialiasing, true);
    setRenderHint(QPainter::SmoothPixmapTransform, true);
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    setDragMode(QGraphicsView::RubberBandDrag);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    setMouseTracking(true);
    setBackgroundBrush(Qt::white);
    // QAbstractScrollArea (QGraphicsView's base) defaults to Qt::NoFocus, so
    // without this a canvas click never gives the view keyboard focus and
    // Escape/Delete (handled by SchematicScene::keyPressEvent(), which only
    // ever receives events the view forwards) would silently do nothing.
    setFocusPolicy(Qt::StrongFocus);
}

void SchematicView::applyZoom(qreal factor)
{
    const qreal newZoom = qBound(kMinZoom, m_zoomFactor * factor, kMaxZoom);
    if (qFuzzyCompare(newZoom, m_zoomFactor))
        return;

    const qreal effectiveFactor = newZoom / m_zoomFactor;
    scale(effectiveFactor, effectiveFactor);
    m_zoomFactor = newZoom;
    emit zoomFactorChanged(m_zoomFactor);
}

void SchematicView::zoomIn()
{
    applyZoom(kZoomStep);
}

void SchematicView::zoomOut()
{
    applyZoom(1.0 / kZoomStep);
}

void SchematicView::zoomReset()
{
    resetTransform();
    m_zoomFactor = 1.0;
    emit zoomFactorChanged(m_zoomFactor);
}

void SchematicView::zoomToFit()
{
    if (!scene())
        return;

    const QRectF target = scene()->itemsBoundingRect();
    if (target.isEmpty())
        return;

    fitInView(target, Qt::KeepAspectRatio);
    m_zoomFactor = transform().m11();
    emit zoomFactorChanged(m_zoomFactor);
}

void SchematicView::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        if (event->angleDelta().y() > 0)
            zoomIn();
        else if (event->angleDelta().y() < 0)
            zoomOut();
        event->accept();
        return;
    }

    QGraphicsView::wheelEvent(event);
}

void SchematicView::mouseMoveEvent(QMouseEvent *event)
{
    emit mouseScenePositionChanged(mapToScene(event->pos()));
    QGraphicsView::mouseMoveEvent(event);
}

void SchematicView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton) {
        // Temporarily behave like left-button hand-drag panning without
        // disturbing the default RubberBandDrag mode used for left-button
        // rubber-band selection (see the constructor) - QGraphicsView's own
        // ScrollHandDrag implementation only responds to the left button,
        // so a synthetic left-button press/release is forwarded while the
        // middle button is actually held.
        m_middleButtonPanning = true;
        setDragMode(QGraphicsView::ScrollHandDrag);
        QMouseEvent fake(QEvent::MouseButtonPress, event->pos(), event->globalPosition(),
                         Qt::LeftButton, Qt::LeftButton, event->modifiers());
        QGraphicsView::mousePressEvent(&fake);
        event->accept();
        return;
    }
    QGraphicsView::mousePressEvent(event);
}

void SchematicView::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_middleButtonPanning && event->button() == Qt::MiddleButton) {
        QMouseEvent fake(QEvent::MouseButtonRelease, event->pos(), event->globalPosition(),
                         Qt::LeftButton, Qt::NoButton, event->modifiers());
        QGraphicsView::mouseReleaseEvent(&fake);
        setDragMode(QGraphicsView::RubberBandDrag);
        m_middleButtonPanning = false;
        event->accept();
        return;
    }
    QGraphicsView::mouseReleaseEvent(event);
}

void SchematicView::setGridSpacing(qreal spacing)
{
    if (!std::isfinite(spacing) || spacing <= 0.0)
        return;
    if (qFuzzyCompare(spacing, m_gridSpacingMinor))
        return;
    m_gridSpacingMinor = spacing;
    viewport()->update();
}

void SchematicView::drawBackground(QPainter *painter, const QRectF &rect)
{
    QGraphicsView::drawBackground(painter, rect);

    const qreal minor = m_gridSpacingMinor;
    const qreal major = minor * kMajorGridRatio;
    const qreal left = qFloor(rect.left() / minor) * minor;
    const qreal top  = qFloor(rect.top()  / minor) * minor;

    QVarLengthArray<QLineF, 256> minorLines;
    QVarLengthArray<QLineF, 64>  majorLines;

    for (qreal x = left; x < rect.right(); x += minor) {
        if (std::abs(std::fmod(x, major)) < minor * 0.05)
            majorLines.append(QLineF(x, rect.top(), x, rect.bottom()));
        else
            minorLines.append(QLineF(x, rect.top(), x, rect.bottom()));
    }
    for (qreal y = top; y < rect.bottom(); y += minor) {
        if (std::abs(std::fmod(y, major)) < minor * 0.05)
            majorLines.append(QLineF(rect.left(), y, rect.right(), y));
        else
            minorLines.append(QLineF(rect.left(), y, rect.right(), y));
    }

    painter->setPen(QPen(QColor(230, 230, 230), 0));
    painter->drawLines(minorLines.data(), minorLines.size());

    painter->setPen(QPen(QColor(200, 200, 200), 0));
    painter->drawLines(majorLines.data(), majorLines.size());
}
