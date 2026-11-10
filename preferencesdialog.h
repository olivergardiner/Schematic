#ifndef PREFERENCESDIALOG_H
#define PREFERENCESDIALOG_H

#include <QDialog>
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
private:
    QDoubleSpinBox *m_propertyGrid;
    QCheckBox *m_cadStyleMouseWheel;
    QCheckBox *m_keepPlacingSymbols;
};

#endif // PREFERENCESDIALOG_H
