#include "schematicview.h"
#include "symbolmimedata.h"

#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMouseEvent>
#include <QApplication>
#include <QKeyEvent>
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
    setDragMode(QGraphicsView::NoDrag);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    setMouseTracking(true);
    setBackgroundBrush(Qt::white);
    // QAbstractScrollArea (QGraphicsView's base) defaults to Qt::NoFocus, so
    // without this a canvas click never gives the view keyboard focus and
    // Escape/Delete (handled by SchematicScene::keyPressEvent(), which only
    // ever receives events the view forwards) would silently do nothing.
    setFocusPolicy(Qt::StrongFocus);
    setAcceptDrops(true);
    viewport()->setAcceptDrops(true);
}

void SchematicView::setScene(QGraphicsScene *newScene)
{
    cancelSelectionBox();
    disconnect(m_boxResetConnection);
    QGraphicsView::setScene(newScene);
    if (auto *schematic = qobject_cast<SchematicScene *>(newScene))
        m_boxResetConnection = connect(schematic, &SchematicScene::selectionGestureReset,
                                      this, &SchematicView::cancelSelectionBox);
}

void SchematicView::cancelSelectionBox()
{
    const bool pending = m_boxPending;
    m_boxPending = m_boxActive = false;
    if (pending) {
        if (auto *schematic = qobject_cast<SchematicScene *>(scene()))
            schematic->restoreSelection(m_boxInitialSelection);
        viewport()->update();
    }
}

void SchematicView::updateSelectionBox(QPoint position)
{
    m_boxEnd = position;
    if (!m_boxActive && (position - m_boxStart).manhattanLength()
                           < QApplication::startDragDistance())
        return;
    m_boxActive = true;
    if (auto *schematic = qobject_cast<SchematicScene *>(scene()))
        schematic->selectInBox(QRectF(mapToScene(m_boxStart), mapToScene(m_boxEnd)).normalized(),
                               m_boxEnd.x() < m_boxStart.x(),
                               m_boxAdditive ? m_boxInitialSelection : SchematicScene::Selection{});
    viewport()->update();
}

void SchematicView::keyPressEvent(QKeyEvent *event)
{
    if (m_boxPending && event->key() == Qt::Key_Escape) {
        cancelSelectionBox();
        event->accept();
        return;
    }
    if (m_boxPending)
        cancelSelectionBox();
    QGraphicsView::keyPressEvent(event);
}

void SchematicView::focusOutEvent(QFocusEvent *event)
{
    cancelSelectionBox();
    if (auto *schematic = qobject_cast<SchematicScene *>(scene())) schematic->cancelMove();
    QGraphicsView::focusOutEvent(event);
}

void SchematicView::paintEvent(QPaintEvent *event)
{
    QGraphicsView::paintEvent(event);
    if (!m_boxActive)
        return;
    QPainter painter(viewport());
    const bool crossing = m_boxEnd.x() < m_boxStart.x();
    const QColor color = crossing ? QColor(0, 135, 65) : QColor(35, 95, 210);
    painter.setPen(QPen(color, 1, crossing ? Qt::DashLine : Qt::SolidLine));
    QColor fill = color;
    fill.setAlpha(30);
    painter.setBrush(fill);
    painter.drawRect(QRect(m_boxStart, m_boxEnd).normalized());
}

void SchematicView::dragEnterEvent(QDragEnterEvent *event)
{
    if (SymbolMimeData::read(event->mimeData())) {
        event->setDropAction(Qt::CopyAction);
        event->accept();
        return;
    }
    event->ignore();
}

void SchematicView::dragMoveEvent(QDragMoveEvent *event)
{
    if (SymbolMimeData::read(event->mimeData())) {
        event->setDropAction(Qt::CopyAction);
        event->accept();
        return;
    }
    event->ignore();
}

void SchematicView::dropEvent(QDropEvent *event)
{
    const std::optional<SymbolId> id = SymbolMimeData::read(event->mimeData());
    if (!id) {
        event->ignore();
        return;
    }

    const QPoint viewportPosition = event->position().toPoint();
    emit symbolDropped(*id, mapToScene(viewportPosition));
    setFocus(Qt::MouseFocusReason);
    event->setDropAction(Qt::CopyAction);
    event->accept();
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
    if (m_boxPending) {
        updateSelectionBox(event->pos());
        event->accept();
        return;
    }
    QGraphicsView::mouseMoveEvent(event);
}

void SchematicView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton) {
        cancelSelectionBox();
        if (auto *schematic = qobject_cast<SchematicScene *>(scene())) schematic->cancelMove();
        // Temporarily behave like left-button hand-drag panning without
        // disturbing custom left-button selection - QGraphicsView's own
        // ScrollHandDrag implementation only responds to the left button,
        // so a synthetic left-button press/release is forwarded while the
        // middle button is actually held.
        m_middleButtonPanning = true;
        m_panningWasInteractive = isInteractive();
        setInteractive(false);
        setDragMode(QGraphicsView::ScrollHandDrag);
        QMouseEvent fake(QEvent::MouseButtonPress, event->pos(), event->globalPosition(),
                         Qt::LeftButton, Qt::LeftButton, event->modifiers());
        QGraphicsView::mousePressEvent(&fake);
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton && !m_middleButtonPanning) {
        if (auto *schematic = qobject_cast<SchematicScene *>(scene())) {
            if (schematic->canStartSelectionBox(mapToScene(event->pos()))) {
                emit schematic->selectionInteractionStarted();
                setFocus(Qt::MouseFocusReason);
                m_boxInitialSelection = schematic->selection();
                m_boxStart = m_boxEnd = event->pos();
                m_boxAdditive = event->modifiers().testFlag(Qt::ControlModifier);
                m_boxPending = true;
                event->accept();
                return;
            }
        }
    }
    QGraphicsView::mousePressEvent(event);
}

void SchematicView::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_boxPending && event->button() == Qt::LeftButton) {
        updateSelectionBox(event->pos());
        if (!m_boxActive && !m_boxAdditive)
            scene()->clearSelection();
        m_boxPending = m_boxActive = false;
        viewport()->update();
        event->accept();
        return;
    }
    if (m_middleButtonPanning && event->button() == Qt::MiddleButton) {
        QMouseEvent fake(QEvent::MouseButtonRelease, event->pos(), event->globalPosition(),
                         Qt::LeftButton, Qt::NoButton, event->modifiers());
        QGraphicsView::mouseReleaseEvent(&fake);
        setDragMode(QGraphicsView::NoDrag);
        setInteractive(m_panningWasInteractive);
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
