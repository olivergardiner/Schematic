#include "symbolimportdialog.h"

#include "core/symbolsvg.h"
#include "symbolpackagestore.h"
#include "symbolpreviewwidget.h"
#include "symbolrenderer.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileInfo>
#include <QFileDialog>
#include <QFormLayout>
#include <QGridLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLineF>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QPainterPathStroker>

#include <cmath>

namespace {

QDoubleSpinBox *coordinateBox(QWidget *parent)
{
    auto *box = new QDoubleSpinBox(parent);
    box->setDecimals(4);
    box->setRange(-1000000000.0, 1000000000.0);
    box->setSingleStep(1.0);
    return box;
}

QString jsonString(const QJsonObject &object, const QString &key)
{
    return object.value(key).isString() ? object.value(key).toString() : QString();
}

} // namespace

SymbolImportDialog::SymbolImportDialog(SymbolPackageStore *store, QWidget *parent)
    : QDialog(parent), m_store(store)
{
    setWindowTitle(tr("Import Symbol"));
    resize(1050, 720);

    auto *root = new QVBoxLayout(this);
    auto *sourceRow = new QHBoxLayout;
    m_sourceEdit = new QLineEdit(this);
    m_sourceEdit->setReadOnly(true);
    auto *browse = new QPushButton(tr("Choose..."), this);
    sourceRow->addWidget(new QLabel(tr("Source:"), this));
    sourceRow->addWidget(m_sourceEdit, 1);
    sourceRow->addWidget(browse);
    root->addLayout(sourceRow);

    auto *body = new QHBoxLayout;
    auto *left = new QWidget(this);
    auto *leftLayout = new QVBoxLayout(left);
    auto *metadata = new QFormLayout;
    m_idEdit = new QLineEdit(this);
    m_nameEdit = new QLineEdit(this);
    m_categoryEdit = new QLineEdit(this);
    m_categoryEdit->setPlaceholderText(tr("e.g. Semiconductors/Transistors"));
    m_prefixEdit = new QLineEdit(this);
    m_showsReferenceCheck = new QCheckBox(tr("Show reference label"), this);
    metadata->addRow(tr("ID:"), m_idEdit);
    metadata->addRow(tr("Name:"), m_nameEdit);
    metadata->addRow(tr("Category:"), m_categoryEdit);
    metadata->addRow(tr("Reference prefix:"), m_prefixEdit);
    metadata->addRow(QString(), m_showsReferenceCheck);
    leftLayout->addLayout(metadata);

    auto *originRow = new QGridLayout;
    m_originX = coordinateBox(this);
    m_originY = coordinateBox(this);
    originRow->addWidget(new QLabel(tr("Origin X:"), this), 0, 0);
    originRow->addWidget(m_originX, 0, 1);
    originRow->addWidget(new QLabel(tr("Origin Y:"), this), 0, 2);
    originRow->addWidget(m_originY, 0, 3);
    leftLayout->addLayout(originRow);

    m_terminalTable = new QTableWidget(0, 4, this);
    m_terminalTable->setHorizontalHeaderLabels({tr("#"), tr("Name"), tr("SVG X"), tr("SVG Y")});
    m_terminalTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_terminalTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_terminalTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_terminalTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    leftLayout->addWidget(new QLabel(tr("Terminals (placement order is terminal index):"), this));
    leftLayout->addWidget(m_terminalTable, 1);

    auto *terminalButtons = new QHBoxLayout;
    auto *upButton = new QPushButton(tr("Move Up"), this);
    auto *downButton = new QPushButton(tr("Move Down"), this);
    auto *removeButton = new QPushButton(tr("Remove"), this);
    terminalButtons->addWidget(upButton);
    terminalButtons->addWidget(downButton);
    terminalButtons->addWidget(removeButton);
    leftLayout->addLayout(terminalButtons);
    body->addWidget(left, 1);

    auto *right = new QVBoxLayout;
    m_preview = new SymbolPreviewWidget(this);
    right->addWidget(m_preview, 1);
    right->addWidget(new QLabel(tr("Warnings and errors:"), this));
    m_messages = new QListWidget(this);
    m_messages->setMaximumHeight(125);
    right->addWidget(m_messages);
    body->addLayout(right, 2);
    root->addLayout(body, 1);

    auto *buttons = new QDialogButtonBox(this);
    auto *importButton = buttons->addButton(tr("Import"), QDialogButtonBox::AcceptRole);
    auto *cancelButton = buttons->addButton(QDialogButtonBox::Cancel);
    root->addWidget(buttons);
    importButton->setObjectName(QStringLiteral("importSymbolButton"));
    importButton->setEnabled(false);

    connect(browse, &QPushButton::clicked, this, &SymbolImportDialog::chooseSource);
    connect(m_originX, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            &SymbolImportDialog::updateOrigin);
    connect(m_originY, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            &SymbolImportDialog::updateOrigin);
    connect(m_terminalTable, &QTableWidget::cellChanged,
            this, &SymbolImportDialog::terminalTableChanged);
    connect(upButton, &QPushButton::clicked, this, &SymbolImportDialog::moveTerminalUp);
    connect(downButton, &QPushButton::clicked, this, &SymbolImportDialog::moveTerminalDown);
    connect(removeButton, &QPushButton::clicked, this, [this]() {
        terminalRemoved(m_terminalTable->currentRow());
    });
    connect(m_preview, &SymbolPreviewWidget::terminalAdded,
            this, &SymbolImportDialog::terminalAdded);
    connect(m_preview, &SymbolPreviewWidget::terminalMoved,
            this, &SymbolImportDialog::terminalMoved);
    connect(m_preview, &SymbolPreviewWidget::terminalRemoved,
            this, &SymbolImportDialog::terminalRemoved);
    connect(m_preview, &SymbolPreviewWidget::originMoved, this,
            [this](const QPointF &origin) {
        m_origin = origin;
        const QSignalBlocker blockX(m_originX), blockY(m_originY);
        m_originX->setValue(origin.x());
        m_originY->setValue(origin.y());
        updatePreview();
    });
    connect(m_preview, &SymbolPreviewWidget::terminalSelected, this,
            [this](int row) { m_terminalTable->selectRow(row); });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(importButton, &QPushButton::clicked, this, &SymbolImportDialog::importSymbol);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    for (QLineEdit *edit : {m_idEdit, m_nameEdit, m_categoryEdit, m_prefixEdit})
        connect(edit, &QLineEdit::textChanged, this, &SymbolImportDialog::updateImportEnabled);
    connect(m_showsReferenceCheck, &QCheckBox::toggled,
            this, &SymbolImportDialog::updateImportEnabled);

    m_importButton = importButton;
    updateMessages();
}

void SymbolImportDialog::chooseSource()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Choose Symbol Source"), {},
        tr("Symbol sidecar or SVG (*.json *.svg);;JSON sidecar (*.json);;SVG artwork (*.svg)"));
    if (!path.isEmpty())
        loadSource(path);
}

void SymbolImportDialog::loadSource(const QString &path)
{
    m_sourceEdit->setText(path);
    m_sourceSidecarPath.clear();
    m_sourceSidecar.clear();
    m_sourceSvg.clear();
    m_primitives.clear();
    m_rawTerminals.clear();
    m_origin = {};
    m_importerWarnings.clear();
    m_importerErrors.clear();
    m_idEdit->clear();
    m_nameEdit->clear();
    m_categoryEdit->clear();
    m_prefixEdit->clear();
    m_showsReferenceCheck->setChecked(false);

    QFile source(path);
    if (!source.open(QIODevice::ReadOnly)) {
        m_importerErrors << tr("Could not read %1: %2").arg(path, source.errorString());
        updatePreview();
        return;
    }
    const QByteArray sourceBytes = source.readAll();
    if (path.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive)) {
        m_sourceSidecarPath = path;
        m_sourceSidecar = sourceBytes;
        QString svgPath;
        QStringList pathErrors;
        if (!SymbolSvgImporter::resolveSidecarSvgPath(sourceBytes,
                QFileInfo(path).absolutePath(), &svgPath, &pathErrors)) {
            m_importerErrors.append(pathErrors);
        } else {
            QFile svgFile(svgPath);
            if (!svgFile.open(QIODevice::ReadOnly))
                m_importerErrors << tr("Could not read referenced SVG %1: %2")
                                        .arg(svgPath, svgFile.errorString());
            else
                m_sourceSvg = svgFile.readAll();
        }
        QJsonParseError jsonError;
        const QJsonDocument sidecarDoc = QJsonDocument::fromJson(sourceBytes, &jsonError);
        if (jsonError.error != QJsonParseError::NoError || !sidecarDoc.isObject()) {
            m_importerErrors << tr("Invalid sidecar JSON: %1").arg(jsonError.errorString());
        } else {
            const QJsonObject object = sidecarDoc.object();
            m_idEdit->setText(jsonString(object, QStringLiteral("id")));
            m_nameEdit->setText(jsonString(object, QStringLiteral("name")));
            m_categoryEdit->setText(jsonString(object, QStringLiteral("category")));
            m_prefixEdit->setText(jsonString(object, QStringLiteral("referencePrefix")));
            if (object.value(QStringLiteral("showsReference")).isBool())
                m_showsReferenceCheck->setChecked(object.value(QStringLiteral("showsReference")).toBool());
            if (object.value(QStringLiteral("origin")).isObject()) {
                const QJsonObject origin = object.value(QStringLiteral("origin")).toObject();
                if (origin.value(QStringLiteral("x")).isDouble()
                    && origin.value(QStringLiteral("y")).isDouble())
                    m_origin = {origin.value(QStringLiteral("x")).toDouble(),
                                origin.value(QStringLiteral("y")).toDouble()};
                else
                    m_importerErrors << tr("Sidecar origin must contain numeric x and y values.");
            }
            if (object.value(QStringLiteral("terminals")).isArray()) {
                const QJsonArray terminals = object.value(QStringLiteral("terminals")).toArray();
                for (int i = 0; i < terminals.size(); ++i) {
                    if (!terminals[i].isObject()) {
                        m_importerErrors << tr("Terminal %1 must be an object.").arg(i + 1);
                        continue;
                    }
                    const QJsonObject terminal = terminals[i].toObject();
                    if (!terminal.value(QStringLiteral("x")).isDouble()
                        || !terminal.value(QStringLiteral("y")).isDouble()) {
                        m_importerErrors << tr("Terminal %1 needs numeric x and y values.").arg(i + 1);
                        continue;
                    }
                    const QString name = terminal.value(QStringLiteral("name")).isString()
                        ? terminal.value(QStringLiteral("name")).toString()
                        : QString::number(i + 1);
                    m_rawTerminals.append({name, QPointF(terminal.value(QStringLiteral("x")).toDouble(),
                                                          terminal.value(QStringLiteral("y")).toDouble())});
                }
            } else {
                m_importerErrors << tr("Sidecar terminals must be an array.");
            }
        }
    } else if (path.endsWith(QStringLiteral(".svg"), Qt::CaseInsensitive)) {
        m_sourceSvg = sourceBytes;
    } else {
        m_importerErrors << tr("Choose a JSON sidecar or an SVG file.");
    }

    if (!m_sourceSvg.isEmpty()) {
        const SymbolSvgArtworkResult artwork = SymbolSvgImporter::importArtwork(m_sourceSvg);
        m_primitives = artwork.primitives;
        m_importerWarnings = artwork.warnings;
        m_importerErrors.append(artwork.errors);
    }
    {
        const QSignalBlocker blockX(m_originX), blockY(m_originY);
        m_originX->setValue(m_origin.x());
        m_originY->setValue(m_origin.y());
    }
    updatePreview();
}

void SymbolImportDialog::updateOrigin()
{
    m_origin = QPointF(m_originX->value(), m_originY->value());
    updatePreview();
}

void SymbolImportDialog::terminalAdded(const QPointF &sourcePosition)
{
    m_rawTerminals.append({QString::number(m_rawTerminals.size() + 1), sourcePosition});
    updatePreview();
    m_terminalTable->selectRow(m_rawTerminals.size() - 1);
}

void SymbolImportDialog::terminalMoved(int index, const QPointF &sourcePosition)
{
    if (index < 0 || index >= m_rawTerminals.size())
        return;
    m_rawTerminals[index].offset = sourcePosition;
    updatePreview();
    updateTerminalTable();
}

void SymbolImportDialog::terminalRemoved(int index)
{
    if (index < 0 || index >= m_rawTerminals.size())
        return;
    m_rawTerminals.removeAt(index);
    updatePreview();
    if (!m_rawTerminals.isEmpty())
        m_terminalTable->selectRow(std::min(index, static_cast<int>(m_rawTerminals.size()) - 1));
}

void SymbolImportDialog::terminalTableChanged(int row, int column)
{
    if (m_updatingTable || row < 0 || row >= m_rawTerminals.size())
        return;
    if (column == 1) {
        m_rawTerminals[row].name = m_terminalTable->item(row, column)->text();
    } else if (column == 2 || column == 3) {
        bool ok = false;
        const qreal value = m_terminalTable->item(row, column)->text().toDouble(&ok);
        if (!ok || !std::isfinite(value)) {
            updateTerminalTable();
            return;
        }
        if (column == 2) m_rawTerminals[row].offset.setX(value);
        else m_rawTerminals[row].offset.setY(value);
    }
    updatePreview();
}

void SymbolImportDialog::moveTerminalUp()
{
    const int row = m_terminalTable->currentRow();
    if (row <= 0 || row >= m_rawTerminals.size()) return;
    m_rawTerminals.swapItemsAt(row, row - 1);
    updatePreview();
    m_terminalTable->selectRow(row - 1);
}

void SymbolImportDialog::moveTerminalDown()
{
    const int row = m_terminalTable->currentRow();
    if (row < 0 || row + 1 >= m_rawTerminals.size()) return;
    m_rawTerminals.swapItemsAt(row, row + 1);
    updatePreview();
    m_terminalTable->selectRow(row + 1);
}

void SymbolImportDialog::updatePreview()
{
    m_preview->setArtwork(m_primitives);
    m_preview->setOrigin(m_origin);
    QVector<SymbolTerminal> relativeTerminals;
    relativeTerminals.reserve(m_rawTerminals.size());
    for (const SymbolTerminal &terminal : m_rawTerminals)
        relativeTerminals.append({terminal.name, terminal.offset - m_origin});
    m_preview->setTerminals(relativeTerminals);
    updateTerminalTable();
    updateMessages();
    updateImportEnabled();
}

void SymbolImportDialog::updateTerminalTable()
{
    m_updatingTable = true;
    const QSignalBlocker blocker(m_terminalTable);
    m_terminalTable->setRowCount(m_rawTerminals.size());
    for (int i = 0; i < m_rawTerminals.size(); ++i) {
        const SymbolTerminal &terminal = m_rawTerminals[i];
        auto *index = new QTableWidgetItem(QString::number(i + 1));
        index->setFlags(index->flags() & ~Qt::ItemIsEditable);
        m_terminalTable->setItem(i, 0, index);
        m_terminalTable->setItem(i, 1, new QTableWidgetItem(terminal.name));
        m_terminalTable->setItem(i, 2, new QTableWidgetItem(QString::number(terminal.offset.x(), 'g', 12)));
        m_terminalTable->setItem(i, 3, new QTableWidgetItem(QString::number(terminal.offset.y(), 'g', 12)));
    }
    m_updatingTable = false;
}

void SymbolImportDialog::updateMessages()
{
    m_messages->clear();
    for (const QString &error : m_importerErrors)
        m_messages->addItem(tr("Error: %1").arg(error));
    for (const QString &warning : m_importerWarnings)
        m_messages->addItem(tr("Warning: %1").arg(warning));

    LibrarySymbol artwork;
    artwork.primitives = m_primitives;
    const auto rendered = SymbolRenderer::primitives(artwork);
    QPainterPathStroker stroker;
    stroker.setWidth(4.0);
    QVector<QPainterPath> closeToGeometry;
    for (const RenderedSymbolPrimitive &primitive : rendered) {
        QPainterPath shape = stroker.createStroke(primitive.path);
        if (primitive.filled)
            shape = shape.united(primitive.path);
        closeToGeometry.append(shape);
    }
    for (int i = 0; i < m_rawTerminals.size(); ++i) {
        const QPointF offset = m_rawTerminals[i].offset - m_origin;
        const qreal gx = std::round(offset.x() / 10.0) * 10.0;
        const qreal gy = std::round(offset.y() / 10.0) * 10.0;
        if (std::abs(offset.x() - gx) > 1e-6 || std::abs(offset.y() - gy) > 1e-6)
            m_messages->addItem(tr("Warning: terminal %1 is off the 10-unit grid.").arg(i + 1));
        bool nearArtwork = false;
        for (const QPainterPath &shape : closeToGeometry)
            nearArtwork = nearArtwork || shape.contains(offset);
        if (!nearArtwork)
            m_messages->addItem(tr("Warning: terminal %1 is more than 2 units from symbol artwork.").arg(i + 1));
    }
}

void SymbolImportDialog::updateImportEnabled()
{
    if (!m_importButton)
        return;
    LibrarySymbol candidate;
    candidate.id = SymbolId(m_idEdit->text().trimmed());
    candidate.displayName = m_nameEdit->text().trimmed();
    candidate.category = m_categoryEdit->text().trimmed();
    candidate.referencePrefix = m_prefixEdit->text().trimmed();
    candidate.showsReference = m_showsReferenceCheck->isChecked();
    candidate.primitives = m_primitives;
    for (const SymbolTerminal &terminal : m_rawTerminals)
        candidate.terminals.append({terminal.name.trimmed(), terminal.offset - m_origin});
    static const QRegularExpression validId(QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._-]*$"));
    m_importButton->setEnabled(m_store && !m_sourceSvg.isEmpty()
        && m_importerErrors.isEmpty() && validId.match(candidate.id.value()).hasMatch()
        && validateLibrarySymbol(candidate).isEmpty());
}

QByteArray SymbolImportDialog::makeFinalSidecar() const
{
    QJsonArray terminals;
    for (const SymbolTerminal &terminal : m_rawTerminals) {
        terminals.append(QJsonObject{{QStringLiteral("name"), terminal.name.trimmed()},
                                     {QStringLiteral("x"), terminal.offset.x()},
                                     {QStringLiteral("y"), terminal.offset.y()}});
    }
    const QJsonObject sidecar{{QStringLiteral("id"), m_idEdit->text().trimmed()},
        {QStringLiteral("name"), m_nameEdit->text().trimmed()},
        {QStringLiteral("category"), m_categoryEdit->text().trimmed()},
        {QStringLiteral("referencePrefix"), m_prefixEdit->text().trimmed()},
        {QStringLiteral("showsReference"), m_showsReferenceCheck->isChecked()},
        {QStringLiteral("svg"), m_idEdit->text().trimmed() + QStringLiteral(".svg")},
        {QStringLiteral("origin"), QJsonObject{{QStringLiteral("x"), m_origin.x()},
                                               {QStringLiteral("y"), m_origin.y()}}},
        {QStringLiteral("terminals"), terminals}};
    return QJsonDocument(sidecar).toJson(QJsonDocument::Indented);
}

void SymbolImportDialog::importSymbol()
{
    const QByteArray sidecar = makeFinalSidecar();
    const SymbolSvgImportResult imported = SymbolSvgImporter::importSymbol(
        sidecar, m_sourceSvg, QStringLiteral("/"));
    if (!imported.symbol) {
        m_importerErrors = imported.errors;
        updateMessages();
        updateImportEnabled();
        return;
    }
    const LibrarySymbol *existing = applicationSymbolLibrary().find(imported.symbol->id);
    if (existing) {
        if (!m_store->contains(imported.symbol->id)) {
            QMessageBox::warning(this, tr("Symbol ID Conflict"),
                tr("The ID %1 belongs to a packaged symbol and cannot be replaced.")
                    .arg(imported.symbol->id.value()));
            return;
        }
        if (existing->terminals.size() != imported.symbol->terminals.size()) {
            QMessageBox::warning(this, tr("Incompatible Symbol Change"),
                tr("A saved symbol's terminal count, order, names, and positions cannot change under the same ID."));
            return;
        }
        for (int i = 0; i < existing->terminals.size(); ++i) {
            if (existing->terminals[i].name != imported.symbol->terminals[i].name
                || existing->terminals[i].offset != imported.symbol->terminals[i].offset) {
                QMessageBox::warning(this, tr("Incompatible Symbol Change"),
                    tr("A saved symbol's terminal count, order, names, and positions cannot change under the same ID."));
                return;
            }
        }
        if (QMessageBox::question(this, tr("Replace Symbol"),
                tr("Replace the existing writable symbol %1?").arg(imported.symbol->id.value()),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
            return;
    }

    QStringList errors;
    if (!m_store->save(*imported.symbol, m_sourceSvg, sidecar, &errors)) {
        QMessageBox::warning(this, tr("Import Failed"), errors.join(QLatin1Char('\n')));
        return;
    }
    accept();
}

