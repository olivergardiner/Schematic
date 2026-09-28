#ifndef SCHEMATICVIEW_H
#define SCHEMATICVIEW_H

#include <QGraphicsView>

// SchematicView renders the SchematicScene and provides the interactive
// behaviour expected of a drawing canvas: a visible grid, mouse-wheel zoom,
// and rubber-band selection with hand-drag panning.
class SchematicView : public QGraphicsView
{
    Q_OBJECT

public:
    explicit SchematicView(QWidget *parent = nullptr);

    qreal zoomFactor() const { return m_zoomFactor; }

public slots:
    void zoomIn();
    void zoomOut();
    void zoomReset();
    void zoomToFit();

signals:
    void zoomFactorChanged(qreal factor);
    void mouseScenePositionChanged(const QPointF &scenePos);

protected:
    void wheelEvent(QWheelEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void drawBackground(QPainter *painter, const QRectF &rect) override;

private:
    void applyZoom(qreal factor);

    static constexpr qreal kZoomStep = 1.15;
    static constexpr qreal kMinZoom  = 0.05;
    static constexpr qreal kMaxZoom  = 20.0;
    static constexpr int   kGridSpacingMinor = 10;
    static constexpr int   kGridSpacingMajor = 100;

    qreal m_zoomFactor = 1.0;
};

#endif // SCHEMATICVIEW_H
