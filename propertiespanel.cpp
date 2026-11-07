#include "propertiespanel.h"
#include "schematicscene.h"
#include "documenteditcontroller.h"
#include <QApplication>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QSet>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>

namespace {
class PropertyTextEdit : public QLineEdit
{
public:
    using QLineEdit::QLineEdit;
    bool dirty = false;
    QString initial;
    std::function<void()> cancelled;
protected:
    bool event(QEvent *event) override {
        if (event->type() == QEvent::ShortcutOverride) {
            auto *key = static_cast<QKeyEvent *>(event);
            if (key->matches(QKeySequence::Undo) || key->matches(QKeySequence::Redo)
                || key->matches(QKeySequence::SelectAll) || key->key() == Qt::Key_Delete
                || key->key() == Qt::Key_Backspace) { event->accept(); return true; }
        }
        return QLineEdit::event(event);
    }
    void keyPressEvent(QKeyEvent *event) override {
        if (event->key() == Qt::Key_Escape) {
            dirty = false; setText(initial); if (cancelled) cancelled(); event->accept(); return;
        }
        QLineEdit::keyPressEvent(event);
    }
};
class VisibilityCheck : public QCheckBox
{
protected:
    bool hitButton(const QPoint &point) const override { return rect().contains(point); }
    void nextCheckState() override { setCheckState(checkState() == Qt::Checked ? Qt::Unchecked : Qt::Checked); }
};
}

PropertiesPanel::PropertiesPanel(Document *document, SchematicScene *scene, DocumentEditController *edits, QWidget *parent)
    : QWidget(parent), m_document(document), m_scene(scene), m_edits(edits)
{
    setObjectName("propertiesPanel");
    auto *layout = new QVBoxLayout(this);
    m_summary = new QLabel(this); m_summary->setWordWrap(true); layout->addWidget(m_summary);
    m_table = new QTableWidget(0,3,this); m_table->setObjectName("propertiesTable");
    m_table->setHorizontalHeaderLabels({tr("Property"), tr("Value"), tr("Visible")});
    m_table->verticalHeader()->hide();
    m_table->horizontalHeader()->setSectionResizeMode(0,QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1,QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(2,QHeaderView::ResizeToContents);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_table);
    auto *buttons = new QHBoxLayout;
    m_add = new QPushButton(tr("Add…"),this); m_add->setObjectName("addPropertyButton");
    m_rename = new QPushButton(tr("Rename…"),this); m_rename->setObjectName("renamePropertyButton");
    m_remove = new QPushButton(tr("Remove"),this); m_remove->setObjectName("removePropertyButton");
    buttons->addWidget(m_add); buttons->addWidget(m_rename); buttons->addWidget(m_remove); layout->addLayout(buttons);
    m_resetPosition = new QPushButton(tr("Reset property position"), this);
    m_resetPosition->setObjectName("resetPropertyPositionButton");
    layout->addWidget(m_resetPosition);
    connect(m_resetPosition, &QPushButton::clicked, this, [this] {
        if (finishTextEdit()) m_scene->resetPropertyPositions(m_scene->selectedComponentIds());
    });
    m_error = new QLabel(this); m_error->setObjectName("propertyError"); m_error->setWordWrap(true);
    m_error->setStyleSheet("color: #b03030;"); layout->addWidget(m_error);
    connect(m_table,&QTableWidget::currentCellChanged,this,[this] { updateButtons(); });
    connect(m_add,&QPushButton::clicked,this,&PropertiesPanel::addProperty);
    connect(m_rename,&QPushButton::clicked,this,&PropertiesPanel::renameProperty);
    connect(m_remove,&QPushButton::clicked,this,[this] {
        if (!finishTextEdit()) return;
        const int row = m_table->currentRow();
        if (row < 2) return;
        apply(m_scene->selectedComponentIds(), {{PropertyEdit::Kind::Remove, m_table->item(row,0)->data(Qt::UserRole).toString()}});
    });
    connect(scene,&SchematicScene::selectionChanged,this,&PropertiesPanel::invalidate);
    connect(scene,&SchematicScene::selectionInteractionStarted,this,&PropertiesPanel::invalidate);
    connect(scene,&SchematicScene::selectionGestureReset,this,&PropertiesPanel::invalidate);
    connect(scene,&SchematicScene::moveStateChanged,this,&PropertiesPanel::invalidate);
    invalidate();
}

void PropertiesPanel::invalidate()
{
    ++m_epoch;
    if (m_refreshPending) return;
    m_refreshPending = true;
    QTimer::singleShot(0,this,[this] { m_refreshPending = false; refresh(); });
}

void PropertiesPanel::refresh()
{
    m_refreshing = true;
    const auto targets = m_scene->selection().components;
    const bool enabled = !targets.isEmpty() && !m_scene->moveInProgress();
    m_table->setEnabled(enabled); m_add->setEnabled(enabled);
    m_error->clear();
    if (targets.isEmpty()) m_summary->setText(m_scene->selection().wires.isEmpty()
        ? tr("Select a component to edit its properties.") : tr("The selection has no component properties."));
    else if (targets.size() == 1) {
        const auto *component = m_document->component(targets.first());
        const auto *symbol = component ? m_document->symbol(component->symbolId()) : nullptr;
        m_summary->setText(symbol ? symbol->displayName : tr("Component"));
    } else m_summary->setText(tr("%1 components selected").arg(targets.size()));
    if (!m_scene->selection().wires.isEmpty() && !targets.isEmpty())
        m_summary->setText(m_summary->text() + tr(" · Wires are not property targets."));
    QStringList keys;
    for (auto id : targets) if (const auto *component = m_document->component(id))
        for (const auto &p : component->properties()) if (!keys.contains(p.id)) keys.append(p.id);
    keys.removeAll("reference"); keys.removeAll("value"); std::sort(keys.begin(),keys.end());
    if (!targets.isEmpty()) keys = QStringList{"reference","value"} + keys;
    // Refresh runs outside cell signals. Remove obsolete editors now rather
    // than leaving same-named widgets awaiting deferred deletion.
    for (int row = 0; row < m_table->rowCount(); ++row) {
        for (int column = 1; column < 3; ++column) {
            auto *widget = m_table->cellWidget(row,column);
            m_table->removeCellWidget(row,column);
            delete widget;
        }
    }
    m_table->setRowCount(0); m_table->setRowCount(keys.size());
    const quint64 epoch = m_epoch;
    for (int row = 0; row < keys.size(); ++row) {
        const QString key = keys[row];
        QSet<QString> values, names; int present = 0, visible = 0;
        for (auto id : targets) if (const auto *component = m_document->component(id)) {
            if (const auto *p = component->property(key)) { ++present; visible += p->visible; values.insert(p->value); names.insert(p->name); }
        }
        const QString name = key == "reference" ? tr("Reference") : key == "value" ? tr("Value")
                           : names.size() == 1 ? *names.constBegin() : key;
        auto *label = new QTableWidgetItem(name); label->setData(Qt::UserRole,key);
        label->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        const QString help = present != targets.size()
            ? tr("Present on %1 of %2 components. Editing creates it on missing components; their values start empty and visibility starts hidden.").arg(present).arg(targets.size())
            : names.size() > 1 ? tr("Different names: %1").arg(QStringList(names.values()).join(", ")) : QString{};
        if (present != targets.size()) label->setText(name + tr(" (%1/%2)").arg(present).arg(targets.size()));
        label->setToolTip(help); m_table->setItem(row,0,label);
        auto *editor = new PropertyTextEdit(m_table); editor->setObjectName("propertyValue_" + key);
        if (present != targets.size()) editor->setPlaceholderText(tr("[Not set on all]"));
        else if (values.size() > 1) editor->setPlaceholderText(tr("[Multiple values]"));
        else if (!values.isEmpty()) editor->setText(*values.constBegin());
        editor->initial = editor->text();
        editor->cancelled = [this] { m_error->clear(); };
        editor->setReadOnly(key == "reference" && targets.size() > 1);
        editor->setToolTip(editor->isReadOnly() ? tr("References must be unique. Select one component to rename it.") : help);
        m_table->setCellWidget(row,1,editor);
        const QPointer<PropertyTextEdit> guard(editor);
        const auto commit = [this,guard,targets,key,name,epoch]() -> bool {
            if (!guard || !guard->dirty || epoch != m_epoch || m_scene->moveInProgress()) return true;
            if (apply(targets, {{PropertyEdit::Kind::SetValue,key,key == "reference" || key == "value" ? QString{} : name,guard->text()}})) {
                if (guard) guard->dirty = false;
                m_focusProperty = key;
                return true;
            }
            return false;
        };
        connect(editor,&QLineEdit::textEdited,this,[this,editor,row,commit] {
            editor->dirty = true; m_table->setCurrentCell(row,1); m_finishEdit = commit;
        });
        connect(editor,&QLineEdit::returnPressed,this,commit);
        connect(editor,&QLineEdit::editingFinished,this,[this,commit] {
            QTimer::singleShot(0,this,[commit] {
                // Opening a menu temporarily takes focus; keep editing the
                // same text rather than committing/replacing its widget.
                if (!QApplication::activePopupWidget()) commit();
            });
        });
        auto *check = new VisibilityCheck; check->setObjectName("propertyVisible_" + key);
        check->setAccessibleName(tr("Show %1 on drawing").arg(name)); check->setTristate(true);
        check->setCheckState(present == targets.size() && visible == targets.size() ? Qt::Checked
                            : present == targets.size() && !visible ? Qt::Unchecked : Qt::PartiallyChecked);
        const auto initialVisibility = check->checkState();
        check->setToolTip(help); m_table->setCellWidget(row,2,check);
        connect(check,&QCheckBox::clicked,this,[this,check,targets,key,name,row,epoch,initialVisibility] {
            if (epoch != m_epoch) return;
            if (!finishTextEdit()) {
                QSignalBlocker block(check);
                // Revert the attempted click while retaining the invalid text.
                check->setCheckState(initialVisibility);
                return;
            }
            m_table->setCurrentCell(row,2);
            apply(targets, {{PropertyEdit::Kind::SetVisible,key,key == "reference" || key == "value" ? QString{} : name,{},check->checkState() == Qt::Checked}});
        });
    }
    m_refreshing = false; updateButtons();
    if (!m_focusProperty.isEmpty()) {
        const QString focus = m_focusProperty; m_focusProperty.clear();
        for (int row = 0; row < m_table->rowCount(); ++row) {
            if (m_table->item(row,0)->data(Qt::UserRole).toString() == focus) {
                m_table->setCurrentCell(row,1);
                if (auto *editor = qobject_cast<QLineEdit *>(m_table->cellWidget(row,1))) { editor->setFocus(); editor->selectAll(); }
            }
        }
    }
}

bool PropertiesPanel::apply(const QVector<ComponentId> &targets, const QVector<PropertyEdit> &edits)
{
    auto current = m_scene->selection().components; auto requested = targets;
    std::sort(requested.begin(),requested.end());
    if (requested != current || m_scene->moveInProgress()) return false;
    QStringList errors; bool valid = false;
    const bool committed = m_edits->commit(tr("Edit component properties"),[&] {
        valid = m_document->editProperties(targets,edits,&errors); return valid;
    });
    if (!committed && !valid) { m_error->setText(errors.join('\n')); return false; }
    m_error->clear(); invalidate(); return true;
}

void PropertiesPanel::updateButtons()
{
    if (m_refreshing) return;
    bool hasCustomPosition = false;
    for (auto id : m_scene->selectedComponentIds())
        if (const auto *component = m_document->component(id))
            hasCustomPosition |= component->propertyBlockOffset().has_value();
    m_resetPosition->setEnabled(m_table->isEnabled() && hasCustomPosition);
    const bool custom = m_table->isEnabled() && m_table->currentRow() >= 2;
    m_remove->setEnabled(custom);
    m_rename->setEnabled(custom && m_scene->selection().components.size() == 1);
}

void PropertiesPanel::focusProperty(const QString &id)
{
    if (!finishTextEdit()) return;
    window()->activateWindow();
    m_focusProperty = id; invalidate();
}

void PropertiesPanel::addProperty()
{
    if (!finishTextEdit()) return;
    const auto targets = m_scene->selection().components;
    const auto epoch = m_epoch;
    QDialog dialog(this); dialog.setWindowTitle(tr("Add Property"));
    auto *layout = new QFormLayout(&dialog);
    auto *name = new QLineEdit(&dialog); name->setObjectName("newPropertyName");
    auto *value = new QLineEdit(&dialog); value->setObjectName("newPropertyValue");
    auto *visible = new QCheckBox(tr("Show on drawing"),&dialog);
    layout->addRow(tr("Name:"),name); layout->addRow(tr("Value:"),value); layout->addRow(visible);
    auto *problem = new QLabel(&dialog); problem->setWordWrap(true); layout->addRow(problem);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,&dialog);
    layout->addRow(buttons); connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    while (dialog.exec() == QDialog::Accepted) {
        if (epoch != m_epoch) return;
        if (apply(targets, {{PropertyEdit::Kind::Add,propertyIdFromName(name->text()),name->text(),value->text(),visible->isChecked()}})) return;
        problem->setText(m_error->text());
    }
}

void PropertiesPanel::renameProperty()
{
    if (!finishTextEdit()) return;
    const int row = m_table->currentRow(); if (row < 2) return;
    const auto targets = m_scene->selection().components;
    if (targets.size() != 1) return;
    const QString key = m_table->item(row,0)->data(Qt::UserRole).toString();
    const quint64 epoch = m_epoch; bool ok = false;
    const auto *p = m_document->component(targets.first())->property(key);
    const auto name = QInputDialog::getText(this,tr("Rename Property"),tr("Name:"),QLineEdit::Normal,p->name,&ok);
    if (ok && epoch == m_epoch) apply(targets, {{PropertyEdit::Kind::Rename,key,name}});
}

bool PropertiesPanel::finishTextEdit()
{
    const auto finish = m_finishEdit;
    m_finishEdit = {};
    if (finish && !finish()) { m_finishEdit = finish; return false; }
    return true;
}
