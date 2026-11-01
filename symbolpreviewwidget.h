#ifndef SYMBOLPREVIEWWIDGET_H
#define SYMBOLPREVIEWWIDGET_H

#include "core/symbollibrary.h"

#include <QTransform>
#include <QWidget>

class SymbolPreviewWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SymbolPreviewWidget(QWidget *parent = nullptr);
    void setArtwork(const QVector<SymbolPrimitive> &primitives);
    void setOrigin(const QPointF &origin);
    void setTerminals(const QVector<SymbolTerminal> &terminals);
    int selectedTerminal() const { return m_selectedTerminal; }

signals:
    void terminalAdded(const QPointF &sourcePosition);
    void terminalMoved(int index, const QPointF &sourcePosition);
    void terminalRemoved(int index);
    void originMoved(const QPointF &sourcePosition);
    void terminalSelected(int index);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    QPointF toScene(const QPointF &widgetPoint) const;
    QPointF snapped(const QPointF &sourcePosition, bool free) const;
    int terminalAt(const QPointF &widgetPoint) const;

    QVector<SymbolPrimitive> m_primitives;
    QVector<SymbolTerminal> m_terminals;
    QPointF m_origin;
    QTransform m_sceneToWidget;
    int m_selectedTerminal = -1;
    int m_draggingTerminal = -1;
    bool m_draggingOrigin = false;
};

#endif // SYMBOLPREVIEWWIDGET_H
