#include "symbolpreviewwidget.h"

#include "symbolrenderer.h"

#include <QKeyEvent>
#include <QLineF>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPathStroker>

#include <algorithm>
#include <cmath>

SymbolPreviewWidget::SymbolPreviewWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(360, 280);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
}

void SymbolPreviewWidget::setArtwork(const QVector<SymbolPrimitive> &primitives)
{
    m_primitives = primitives;
    update();
}

void SymbolPreviewWidget::setOrigin(const QPointF &origin)
{
    m_origin = origin;
    update();
}

void SymbolPreviewWidget::setTerminals(const QVector<SymbolTerminal> &terminals)
{
    m_terminals = terminals;
    if (m_selectedTerminal >= m_terminals.size())
        m_selectedTerminal = m_terminals.size() - 1;
    update();
}

QPointF SymbolPreviewWidget::toScene(const QPointF &widgetPoint) const
{
    bool invertible = false;
    const QTransform inverse = m_sceneToWidget.inverted(&invertible);
    return invertible ? inverse.map(widgetPoint) : QPointF();
}

QPointF SymbolPreviewWidget::snapped(const QPointF &sourcePosition, bool free) const
{
    if (free)
        return sourcePosition;
    const QPointF offset = sourcePosition - m_origin;
    return m_origin + QPointF(std::round(offset.x() / 10.0) * 10.0,
                              std::round(offset.y() / 10.0) * 10.0);
}

int SymbolPreviewWidget::terminalAt(const QPointF &widgetPoint) const
{
    for (int i = m_terminals.size() - 1; i >= 0; --i) {
        const QPointF scenePosition = m_terminals[i].offset + m_origin;
        if (QLineF(m_sceneToWidget.map(scenePosition), widgetPoint).length() <= 9.0)
            return i;
    }
    return -1;
}

void SymbolPreviewWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), palette().base());
    painter.setPen(QPen(palette().mid().color(), 1));
    painter.drawRect(rect().adjusted(0, 0, -1, -1));

    LibrarySymbol artwork;
    artwork.primitives = m_primitives;
    QRectF bounds = SymbolRenderer::bounds(artwork);
    if (bounds.isEmpty())
        bounds = QRectF(-50, -37.5, 100, 75);
    bounds = bounds.united(QRectF(m_origin, QSizeF(0, 0)));
    if (bounds.width() < 10) bounds.adjust(-5, 0, 5, 0);
    if (bounds.height() < 10) bounds.adjust(0, -5, 0, 5);
    bounds = bounds.adjusted(-15, -15, 15, 15);
    const QRectF target = QRectF(rect()).adjusted(18, 18, -18, -18);
    const qreal scale = std::min(target.width() / bounds.width(),
                                 target.height() / bounds.height());
    m_sceneToWidget = QTransform(scale, 0, 0, scale,
        target.center().x() - scale * bounds.center().x(),
        target.center().y() - scale * bounds.center().y());

    painter.save();
    painter.setTransform(m_sceneToWidget);
    const qreal visibleLeft = toScene(QPointF(target.left(), target.top())).x();
    const qreal visibleRight = toScene(QPointF(target.right(), target.bottom())).x();
    const qreal visibleTop = toScene(QPointF(target.left(), target.top())).y();
    const qreal visibleBottom = toScene(QPointF(target.right(), target.bottom())).y();
    painter.setPen(QPen(QColor(225, 229, 234), 1.0 / scale));
    const int xStart = static_cast<int>(std::floor(visibleLeft / 10.0));
    const int xEnd = static_cast<int>(std::ceil(visibleRight / 10.0));
    const int yStart = static_cast<int>(std::floor(visibleTop / 10.0));
    const int yEnd = static_cast<int>(std::ceil(visibleBottom / 10.0));
    if (xEnd - xStart <= 160 && yEnd - yStart <= 160) {
        for (int x = xStart; x <= xEnd; ++x)
            painter.drawLine(QPointF(x * 10.0, visibleTop), QPointF(x * 10.0, visibleBottom));
        for (int y = yStart; y <= yEnd; ++y)
            painter.drawLine(QPointF(visibleLeft, y * 10.0), QPointF(visibleRight, y * 10.0));
    }

    const auto rendered = SymbolRenderer::primitives(artwork);
    for (const RenderedSymbolPrimitive &primitive : rendered) {
        painter.setPen(QPen(QColor(35, 45, 55), 1.0 / scale));
        painter.setBrush(primitive.filled ? QBrush(Qt::white) : QBrush(Qt::NoBrush));
        painter.drawPath(primitive.path);
    }

    const qreal markerRadius = 5.0 / scale;
    painter.setPen(QPen(QColor(180, 45, 45), 1.2 / scale));
    painter.setBrush(QColor(255, 235, 235));
    painter.drawEllipse(m_origin, markerRadius, markerRadius);
    painter.drawLine(m_origin + QPointF(-markerRadius * 1.8, 0),
                     m_origin + QPointF(markerRadius * 1.8, 0));
    painter.drawLine(m_origin + QPointF(0, -markerRadius * 1.8),
                     m_origin + QPointF(0, markerRadius * 1.8));

    for (int i = 0; i < m_terminals.size(); ++i) {
        const QPointF position = m_terminals[i].offset + m_origin;
        painter.setPen(QPen(i == m_selectedTerminal ? QColor(20, 90, 190) : QColor(35, 100, 170),
                            1.2 / scale));
        painter.setBrush(i == m_selectedTerminal ? QColor(195, 225, 255) : QColor(225, 240, 255));
        painter.drawEllipse(position, markerRadius, markerRadius);
        painter.drawLine(position + QPointF(-markerRadius * 0.6, 0),
                         position + QPointF(markerRadius * 0.6, 0));
        painter.drawLine(position + QPointF(0, -markerRadius * 0.6),
                         position + QPointF(0, markerRadius * 0.6));
    }
    painter.restore();

    painter.setPen(palette().text().color());
    for (int i = 0; i < m_terminals.size(); ++i) {
        const QPointF position = m_sceneToWidget.map(m_terminals[i].offset + m_origin);
        painter.drawText(position + QPointF(7, -5), m_terminals[i].name);
    }
    painter.drawText(QRect(8, 6, width() - 16, 18), Qt::AlignLeft,
                     tr("Click to add a terminal · Ctrl for free placement · drag the origin marker"));
}

void SymbolPreviewWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }
    setFocus();
    const QPointF widgetPosition = event->position();
    const int terminal = terminalAt(widgetPosition);
    if (terminal >= 0) {
        m_selectedTerminal = terminal;
        m_draggingTerminal = terminal;
        emit terminalSelected(terminal);
        update();
        event->accept();
        return;
    }
    const QPointF scenePosition = toScene(widgetPosition);
    if (QLineF(m_sceneToWidget.map(m_origin), widgetPosition).length() <= 9.0) {
        m_draggingOrigin = true;
        event->accept();
        return;
    }
    emit terminalAdded(snapped(scenePosition, event->modifiers().testFlag(Qt::ControlModifier)));
    event->accept();
}

void SymbolPreviewWidget::mouseMoveEvent(QMouseEvent *event)
{
    const QPointF scenePosition = toScene(event->position());
    if (m_draggingOrigin) {
        m_origin = scenePosition;
        emit originMoved(m_origin);
        update();
        event->accept();
    } else if (m_draggingTerminal >= 0 && m_draggingTerminal < m_terminals.size()) {
        const QPointF position = snapped(scenePosition,
            event->modifiers().testFlag(Qt::ControlModifier));
        emit terminalMoved(m_draggingTerminal, position);
        event->accept();
    } else {
        QWidget::mouseMoveEvent(event);
    }
}

void SymbolPreviewWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_draggingOrigin = false;
        m_draggingTerminal = -1;
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void SymbolPreviewWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Delete && m_selectedTerminal >= 0) {
        emit terminalRemoved(m_selectedTerminal);
        m_selectedTerminal = -1;
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}
