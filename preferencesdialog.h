#ifndef PREFERENCESDIALOG_H
#define PREFERENCESDIALOG_H

#include <QDialog>
#include "document.h"
class QComboBox;
class QFontComboBox;
class QSpinBox;
class QDoubleSpinBox;
class QCheckBox;

class PreferencesDialog : public QDialog
{
    Q_OBJECT
public:
    explicit PreferencesDialog(QWidget *parent = nullptr);
    static qreal savedPropertyBlockGridSpacing();
    qreal propertyBlockGridSpacing() const;
    static bool savedCadStyleMouseWheel();
    bool cadStyleMouseWheel() const;
    static bool savedKeepPlacingSymbols();
    bool keepPlacingSymbols() const;
    void setDrawingOptions(const Document &document);
    DrawingVariation drawingVariation() const;
    int drawingStrokes() const;
    qreal drawingLooseness() const;
private:
    QFontComboBox *m_fontFamily;
    QSpinBox *m_labelFontSize;
    QSpinBox *m_noteFontSize;
    QComboBox *m_drawingVariation;
    QComboBox *m_drawingStrokes;
    QDoubleSpinBox *m_drawingLooseness;
    QWidget *m_handDrawnGroup;
    qreal m_initialLooseness = 1.0;
    qreal m_initialDisplayedLooseness = 100;
    QDoubleSpinBox *m_propertyGrid;
    QCheckBox *m_cadStyleMouseWheel;
    QCheckBox *m_keepPlacingSymbols;
};

#endif // PREFERENCESDIALOG_H
