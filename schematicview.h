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

    // Sets both the snap increment and the minor grid-line spacing to
    // spacing (scene units); major grid lines are drawn every 10 minor
    // intervals so the grid stays readable. Ignored if spacing
    // is not finite and positive.
    void setGridSpacing(qreal spacing);

signals:
    void zoomFactorChanged(qreal factor);
    void mouseScenePositionChanged(const QPointF &scenePos);

protected:
    void wheelEvent(QWheelEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void drawBackground(QPainter *painter, const QRectF &rect) override;

private:
    void applyZoom(qreal factor);

    static constexpr qreal kZoomStep = 1.15;
    static constexpr qreal kMinZoom  = 0.05;
    static constexpr qreal kMaxZoom  = 20.0;
    // Major grid lines are drawn every this-many minor intervals - see
    // setGridSpacing().
    static constexpr int kMajorGridRatio = 10;

    qreal m_zoomFactor = 1.0;
    qreal m_gridSpacingMinor = 10.0;

    // Middle-button panning temporarily swaps to ScrollHandDrag (which
    // requires a fake left-button press per QGraphicsView's own documented
    // hand-drag implementation) without disturbing the default
    // RubberBandDrag mode used for left-button rubber-band selection, or
    // the existing Ctrl+wheel zoom.
    bool m_middleButtonPanning = false;
};

#endif // SCHEMATICVIEW_H
