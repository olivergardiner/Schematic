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

void SchematicView::drawBackground(QPainter *painter, const QRectF &rect)
{
    QGraphicsView::drawBackground(painter, rect);

    const qreal left = qFloor(rect.left() / kGridSpacingMinor) * kGridSpacingMinor;
    const qreal top  = qFloor(rect.top()  / kGridSpacingMinor) * kGridSpacingMinor;

    QVarLengthArray<QLineF, 256> minorLines;
    QVarLengthArray<QLineF, 64>  majorLines;

    for (qreal x = left; x < rect.right(); x += kGridSpacingMinor) {
        if (std::abs(std::fmod(x, qreal(kGridSpacingMajor))) < 0.5)
            majorLines.append(QLineF(x, rect.top(), x, rect.bottom()));
        else
            minorLines.append(QLineF(x, rect.top(), x, rect.bottom()));
    }
    for (qreal y = top; y < rect.bottom(); y += kGridSpacingMinor) {
        if (std::abs(std::fmod(y, qreal(kGridSpacingMajor))) < 0.5)
            majorLines.append(QLineF(rect.left(), y, rect.right(), y));
        else
            minorLines.append(QLineF(rect.left(), y, rect.right(), y));
    }

    painter->setPen(QPen(QColor(230, 230, 230), 0));
    painter->drawLines(minorLines.data(), minorLines.size());

    painter->setPen(QPen(QColor(200, 200, 200), 0));
    painter->drawLines(majorLines.data(), majorLines.size());
}
