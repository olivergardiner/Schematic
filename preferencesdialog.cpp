#include "preferencesdialog.h"
#include "schematicfont.h"

#include <QDialogButtonBox>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QSettings>
#include <QCheckBox>
#include <QLabel>
#include <QComboBox>
#include <QGroupBox>
#include <QFontComboBox>
#include <QSpinBox>
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
    resize(520, 480);

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
    auto *fonts = new QGroupBox(tr("Schematic text - all drawings"), appearance);
    auto *fontForm = new QFormLayout(fonts);
    m_fontFamily = new QFontComboBox(fonts);
    m_fontFamily->setObjectName("schematicFontFamily");
    m_fontFamily->setCurrentFont(QFont(SchematicFont::family()));
    fontForm->addRow(tr("Font:"), m_fontFamily);
    m_labelFontSize = new QSpinBox(fonts);
    m_labelFontSize->setObjectName("labelFontSize");
    m_labelFontSize->setRange(4,72); m_labelFontSize->setSuffix(tr(" units"));
    m_labelFontSize->setValue(SchematicFont::labels().pixelSize());
    fontForm->addRow(tr("Label size:"), m_labelFontSize);
    m_noteFontSize = new QSpinBox(fonts);
    m_noteFontSize->setObjectName("noteFontSize");
    m_noteFontSize->setRange(4,72); m_noteFontSize->setSuffix(tr(" units"));
    m_noteFontSize->setValue(SchematicFont::notes().pixelSize());
    fontForm->addRow(tr("Note size:"), m_noteFontSize);
    auto *preview = new QLabel(tr("R1  100k    C1  47n    Schematic note"), fonts);
    preview->setObjectName("schematicFontPreview");
    const auto updatePreview = [this, preview] {
        QFont font = m_fontFamily->currentFont(); font.setPixelSize(18); preview->setFont(font);
    };
    connect(m_fontFamily, &QFontComboBox::currentFontChanged, this, [updatePreview](const QFont &) { updatePreview(); });
    updatePreview(); fontForm->addRow(preview);
    form->addRow(fonts);
    auto *handDrawn = new QGroupBox(tr("Hand-drawn style — current schematic"), appearance);
    m_handDrawnGroup = handDrawn;
    handDrawn->setEnabled(false);
    handDrawn->setObjectName("handDrawnPreferencesGroup");
    auto *drawingForm = new QFormLayout(handDrawn);
    m_drawingVariation = new QComboBox(handDrawn);
    m_drawingVariation->setObjectName("drawingVariationCombo");
    m_drawingVariation->addItems({tr("Individual"), tr("Consistent")});
    m_drawingVariation->setToolTip(tr("Consistent repeats the same hand-drawn artwork for each instance of a symbol"));
    drawingForm->addRow(tr("Variation:"), m_drawingVariation);
    m_drawingStrokes = new QComboBox(handDrawn);
    m_drawingStrokes->setObjectName("drawingStrokesCombo");
    m_drawingStrokes->addItems({tr("Single"), tr("Double")});
    drawingForm->addRow(tr("Strokes:"), m_drawingStrokes);
    m_drawingLooseness = new QDoubleSpinBox(handDrawn);
    m_drawingLooseness->setObjectName("drawingLooseness");
    m_drawingLooseness->setRange(0,300); m_drawingLooseness->setDecimals(0);
    m_drawingLooseness->setSingleStep(10); m_drawingLooseness->setSuffix(tr(" %"));
    m_drawingLooseness->setValue(100);
    m_drawingLooseness->setToolTip(tr("0% keeps exact geometry; 100% is the original hand-drawn look; 300% gives the loosest strokes"));
    drawingForm->addRow(tr("Looseness:"), m_drawingLooseness);
    form->addRow(handDrawn);
    tabs->addTab(appearance, tr("Appearance"));
    layout->addWidget(tabs);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        QSettings().setValue("appearance/schematicFontFamily", m_fontFamily->currentFont().family());
        QSettings().setValue("appearance/labelFontSize", m_labelFontSize->value());
        QSettings().setValue("appearance/noteFontSize", m_noteFontSize->value());
        QSettings().setValue("appearance/propertyBlockGridSpacing", propertyBlockGridSpacing());
        QSettings().setValue("general/cadStyleMouseWheel", cadStyleMouseWheel());
        QSettings().setValue("general/keepPlacingSymbols", keepPlacingSymbols());
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

void PreferencesDialog::setDrawingOptions(const Document &document)
{
    m_drawingVariation->setCurrentIndex(document.drawingVariation() == DrawingVariation::Consistent ? 1 : 0);
    m_drawingStrokes->setCurrentIndex(document.drawingStrokes()-1);
    m_initialLooseness = document.drawingLooseness();
    m_drawingLooseness->setValue(m_initialLooseness*100);
    m_initialDisplayedLooseness = m_drawingLooseness->value();
    m_handDrawnGroup->setEnabled(document.drawingStyle() == DrawingStyle::HandDrawn);
}

DrawingVariation PreferencesDialog::drawingVariation() const
{
    return m_drawingVariation->currentIndex() == 1 ? DrawingVariation::Consistent : DrawingVariation::Individual;
}
int PreferencesDialog::drawingStrokes() const { return m_drawingStrokes->currentIndex()+1; }
qreal PreferencesDialog::drawingLooseness() const
{
    return m_drawingLooseness->value() == m_initialDisplayedLooseness
        ? m_initialLooseness : m_drawingLooseness->value()/100;
}
