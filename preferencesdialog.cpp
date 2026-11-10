#include "preferencesdialog.h"

#include <QDialogButtonBox>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QSettings>
#include <QCheckBox>
#include <QLabel>
#include <cmath>

qreal PreferencesDialog::savedPropertyBlockGridSpacing()
{
    bool ok = false;
    const auto value = QSettings().value("appearance/propertyBlockGridSpacing", 2.0).toDouble(&ok);
    return ok && std::isfinite(value) && value >= 0.1 && value <= 1000.0 ? value : 2.0;
}

qreal PreferencesDialog::propertyBlockGridSpacing() const { return m_propertyGrid->value(); }

bool PreferencesDialog::savedCadStyleMouseWheel()
{
    return QSettings().value("general/cadStyleMouseWheel", true).toBool();
}

bool PreferencesDialog::cadStyleMouseWheel() const { return m_cadStyleMouseWheel->isChecked(); }

bool PreferencesDialog::savedKeepPlacingSymbols()
{
    return QSettings().value("general/keepPlacingSymbols", false).toBool();
}

bool PreferencesDialog::keepPlacingSymbols() const { return m_keepPlacingSymbols->isChecked(); }

PreferencesDialog::PreferencesDialog(QWidget *parent)
    : QDialog(parent)
{
    setObjectName("preferencesDialog");
    setWindowTitle(tr("Preferences"));
    resize(480, 320);

    auto *layout = new QVBoxLayout(this);
    auto *tabs = new QTabWidget(this);
    tabs->setObjectName("preferencesTabs");
    auto *general = new QWidget(tabs);
    general->setObjectName("generalPreferencesPage");
    auto *generalLayout = new QVBoxLayout(general);
    m_cadStyleMouseWheel = new QCheckBox(tr("CAD-style mouse wheel"), general);
    m_cadStyleMouseWheel->setObjectName("cadStyleMouseWheel");
    m_cadStyleMouseWheel->setChecked(savedCadStyleMouseWheel());
    generalLayout->addWidget(m_cadStyleMouseWheel);
    auto *wheelHelp = new QLabel(tr("When enabled, the wheel zooms and Ctrl+wheel scrolls vertically. "
                                    "When disabled, the wheel scrolls and Ctrl+wheel zooms."), general);
    wheelHelp->setWordWrap(true);
    generalLayout->addWidget(wheelHelp);
    m_keepPlacingSymbols = new QCheckBox(tr("Keep placing symbols after placement"), general);
    m_keepPlacingSymbols->setObjectName("keepPlacingSymbols");
    m_keepPlacingSymbols->setChecked(savedKeepPlacingSymbols());
    generalLayout->addWidget(m_keepPlacingSymbols);
    generalLayout->addStretch();
    tabs->addTab(general, tr("General"));
    auto *appearance = new QWidget(tabs);
    appearance->setObjectName("appearancePreferencesPage");
    auto *form = new QFormLayout(appearance);
    m_propertyGrid = new QDoubleSpinBox(appearance);
    m_propertyGrid->setObjectName("propertyBlockGridSpacing");
    m_propertyGrid->setRange(0.1, 1000.0);
    m_propertyGrid->setDecimals(2);
    m_propertyGrid->setSuffix(tr(" units"));
    m_propertyGrid->setValue(savedPropertyBlockGridSpacing());
    form->addRow(tr("Property block grid spacing:"), m_propertyGrid);
    tabs->addTab(appearance, tr("Appearance"));
    layout->addWidget(tabs);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        QSettings().setValue("appearance/propertyBlockGridSpacing", propertyBlockGridSpacing());
        QSettings().setValue("general/cadStyleMouseWheel", cadStyleMouseWheel());
        QSettings().setValue("general/keepPlacingSymbols", keepPlacingSymbols());
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}
