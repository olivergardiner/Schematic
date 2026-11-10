#include "propertiespanel.h"
#include "schematicscene.h"
#include "documenteditcontroller.h"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QToolButton>
#include <QButtonGroup>
#include <QColorDialog>
#include <QMenu>
#include <QPainter>
#include <QStandardItemModel>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QDoubleSpinBox>
#include <QFocusEvent>
#include <QPointer>
#include <QPushButton>
#include <QSet>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>

namespace {
QWidget *alignmentButtons(QWidget *parent, const QString &name,
                          std::optional<PropertyJustification> current,
                          std::function<void(PropertyJustification)> apply)
{
    auto *widget = new QWidget(parent); widget->setObjectName(name);
    auto *layout = new QHBoxLayout(widget); layout->setContentsMargins(0,0,0,0); layout->setSpacing(2);
    auto *group = new QButtonGroup(widget); group->setExclusive(true);
    const QStringList names{"Left","Middle","Right"};
    const QStringList labels{QObject::tr("Align left"),QObject::tr("Align centre"),QObject::tr("Align right")};
    for (int i=0;i<3;++i) {
        const auto value = i == 0 ? PropertyJustification::Left : i == 1 ? PropertyJustification::Middle : PropertyJustification::Right;
        QPixmap pixmap(24,24); pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap); painter.setPen(QPen(widget->palette().color(QPalette::WindowText),2));
        const int lengths[]{18,12,18,10};
        for(int row=0;row<4;++row) {
            const int x=i==0 ? 3 : i==1 ? (24-lengths[row])/2 : 21-lengths[row];
            painter.drawLine(x,5+row*5,x+lengths[row],5+row*5);
        }
        painter.end();
        auto *button = new QToolButton(widget); button->setObjectName(name+names[i]);
        button->setIcon(QIcon(pixmap)); button->setIconSize({24,24}); button->setCheckable(true);
        button->setToolTip(labels[i]); button->setAccessibleName(labels[i]);
        group->addButton(button); button->setChecked(current && *current==value); layout->addWidget(button);
        QObject::connect(button,&QToolButton::clicked,widget,[apply,value] { apply(value); });
    }
    layout->addStretch(); return widget;
}

QIcon colourIcon(const QString &colour)
{
    QPixmap pixmap(24,24); pixmap.fill(Qt::transparent); QPainter painter(&pixmap);
    painter.setPen(Qt::gray); painter.setBrush(colour=="none" ? QBrush(Qt::white) : QBrush(QColor(colour)));
    painter.drawRect(3,3,17,17);
    if(colour=="none") { painter.setPen(QPen(Qt::red,2)); painter.drawLine(4,19,19,4); }
    return QIcon(pixmap);
}

class NoteTextEdit : public QPlainTextEdit
{
public:
    using QPlainTextEdit::QPlainTextEdit;
    std::function<bool()> commit;
    std::function<void()> cancel;
protected:
    void focusOutEvent(QFocusEvent *event) override {
        QPlainTextEdit::focusOutEvent(event);
        const auto callback = commit;
        if (callback) callback();
    }
    bool event(QEvent *event) override {
        if (event->type() == QEvent::ShortcutOverride && static_cast<QKeyEvent *>(event)->key() == Qt::Key_Escape) {
            event->accept(); return true;
        }
        return QPlainTextEdit::event(event);
    }
    void keyPressEvent(QKeyEvent *event) override {
        if (event->key() == Qt::Key_Escape) {
            if (cancel) cancel(); event->accept(); return;
        }
        if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
            && event->modifiers().testFlag(Qt::ControlModifier)) {
            const auto callback = commit;
            if (callback) callback(); event->accept(); return;
        }
        QPlainTextEdit::keyPressEvent(event);
    }
};

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
    m_noteControls = new QWidget(this);
    auto *noteLayout = new QVBoxLayout(m_noteControls);
    noteLayout->setContentsMargins(0,0,0,0);
    noteLayout->addWidget(new QLabel(tr("Text (Ctrl+Enter to apply):"), m_noteControls));
    m_noteText = new NoteTextEdit(m_noteControls);
    m_noteText->setObjectName("noteTextEditor");
    m_noteText->setMinimumHeight(100);
    noteLayout->addWidget(m_noteText);
    auto *noteForm = new QFormLayout;
    m_noteWidth = new QDoubleSpinBox(m_noteControls);
    m_noteWidth->setObjectName("noteWrapWidth");
    m_noteWidth->setRange(kMinimumNoteWidth,kMaximumNoteWidth);
    m_noteWidth->setDecimals(2); m_noteWidth->setSuffix(tr(" units"));
    noteForm->addRow(tr("Wrapping width:"),m_noteWidth);
    noteLayout->addLayout(noteForm);
    m_noteAppearance = new QFormLayout;
    noteLayout->addLayout(m_noteAppearance);
    connect(m_noteText, &QPlainTextEdit::textChanged, this, [this] {
        if (!m_refreshing) m_noteTextDirty = true;
    });
    connect(m_noteWidth, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this] {
        if (!m_refreshing) m_noteWidthDirty = true;
    });
    connect(m_noteWidth, &QDoubleSpinBox::editingFinished, this, [this] {
        const auto callback = static_cast<NoteTextEdit *>(m_noteText)->commit;
        if (callback) callback();
    });
    m_summary = new QLabel(this); m_summary->setWordWrap(true); layout->addWidget(m_summary);
    layout->addWidget(m_noteControls);
    m_layoutControls = new QFormLayout;
    layout->addLayout(m_layoutControls);
    m_appearanceControls = new QFormLayout;
    layout->addLayout(m_appearanceControls);
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

PropertiesPanel::~PropertiesPanel()
{
    ++m_epoch;
    auto *editor = static_cast<NoteTextEdit *>(m_noteText);
    editor->commit = {}; editor->cancel = {};
    disconnect(m_noteWidth,nullptr,this,nullptr);
    m_finishEdit = {};
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
    if (refreshNote()) { m_refreshing = false; return; }
    const auto targets = m_scene->selection().components;
    const bool enabled = !targets.isEmpty() && !m_scene->moveInProgress();
    refreshLayouts(targets);
    refreshJustification(targets);
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

bool PropertiesPanel::refreshNote()
{
    const auto selection = m_scene->selection();
    const bool single = selection.notes.size() == 1 && selection.components.isEmpty() && selection.wires.isEmpty();
    m_noteControls->setVisible(single);
    m_table->setVisible(!single);
    for (auto *button : {m_add,m_rename,m_remove,m_resetPosition}) button->setVisible(!single);
    auto *editor = static_cast<NoteTextEdit *>(m_noteText);
    editor->commit = {}; editor->cancel = {};
    if (!single) { m_focusNote = false; return false; }
    m_focusProperty.clear();
    refreshLayouts({}); refreshJustification({});
    const auto *note = m_document->note(selection.notes.first());
    if (!note) return false;
    m_summary->setText(tr("Text note")); m_error->clear();
    m_noteControls->setEnabled(!m_scene->moveInProgress());
    m_noteText->setPlainText(note->text);
    m_noteWidth->setValue(note->width);
    m_noteTextDirty = false; m_noteWidthDirty = false;
    const auto id = note->id;
    const auto epoch = m_epoch;
    editor->commit = [this,id,epoch] {
        if (epoch != m_epoch || m_refreshing || m_scene->moveInProgress()) return true;
        const auto selected = m_scene->selection();
        if (selected.notes != QVector<NoteId>{id} || !selected.components.isEmpty() || !selected.wires.isEmpty()) return true;
        const auto *current = m_document->note(id);
        if (!current) return true;
        // Display normalization (newlines or decimal precision) is not an
        // edit. Preserve exact stored values unless that control was edited.
        const auto text = m_noteTextDirty ? m_noteText->toPlainText() : current->text;
        const auto width = m_noteWidthDirty ? m_noteWidth->value() : current->width;
        if (text == current->text && width == current->width) return true;
        const auto position = current->position;
        const bool result = m_edits->commit(tr("Edit text note"), [this,id,position,text,width] {
            return m_document->setNote(id,position,text,width);
        });
        if (!result) m_error->setText(tr("The note text or width is invalid."));
        return result;
    };
    editor->cancel = [this,id] {
        if (const auto *note = m_document->note(id)) {
            m_noteText->setPlainText(note->text); m_noteWidth->setValue(note->width);
        }
        m_noteTextDirty = false; m_noteWidthDirty = false;
        m_error->clear();
    };
    m_finishEdit = editor->commit;
    while(m_noteAppearance->rowCount()) m_noteAppearance->removeRow(0);
    m_noteAppearance->addRow(tr("Justification:"),alignmentButtons(m_noteControls,"noteJustification",note->justification,
        [this,id](PropertyJustification value) { changeNoteAppearance(id,value); }));
    for(int target=1;target<=2;++target) {
        const auto colour = target==1 ? note->fillColor : note->outlineColor;
        auto *button = new QToolButton(m_noteControls);
        button->setObjectName(target==1 ? "noteFillColour" : "noteOutlineColour");
        button->setText(target==1 ? tr("Fill…") : tr("Outline…"));
        button->setAccessibleName(button->text()); button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        button->setIcon(colourIcon(colour)); button->setToolTip(colour=="none" ? tr("None") : colour);
        button->setPopupMode(QToolButton::MenuButtonPopup);
        auto *menu = new QMenu(button);
        auto *clear = menu->addAction(tr("None"));
        clear->setObjectName(target==1 ? "clearNoteFill" : "clearNoteOutline");
        connect(clear,&QAction::triggered,this,[this,id,target] { changeNoteAppearance(id,{},target,true); });
        button->setMenu(menu);
        connect(button,&QToolButton::clicked,this,[this,id,target] { changeNoteAppearance(id,{},target); });
        m_noteAppearance->addRow(target==1 ? tr("Fill colour:") : tr("Outline colour:"),button);
    }
    if (m_focusNote) {
        m_focusNote = false; m_noteText->setFocus(); m_noteText->selectAll();
    }
    return true;
}

void PropertiesPanel::focusNote()
{
    m_focusNote = true;
    invalidate();
}

void PropertiesPanel::changeNoteAppearance(NoteId id, std::optional<PropertyJustification> justification,
                                         int colourTarget, bool clear)
{
    const auto matches = [this,id] {
        const auto selection=m_scene->selection();
        return !m_scene->moveInProgress() && selection.notes==QVector<NoteId>{id}
            && selection.components.isEmpty() && selection.wires.isEmpty();
    };
    if(!matches() || !finishTextEdit()) return;
    const auto *note=m_document->note(id); if(!note) return;
    QString colour="none";
    if(colourTarget && !clear) {
        const auto previous=colourTarget==1 ? note->fillColor : note->outlineColor;
        const auto epoch=m_epoch;
        const auto selected=QColorDialog::getColor(previous=="none" ? QColor(Qt::white) : QColor(previous),this,
            colourTarget==1 ? tr("Note fill colour") : tr("Note outline colour"),QColorDialog::ShowAlphaChannel);
        if(!selected.isValid() || epoch!=m_epoch || !matches()) return;
        colour=selected.name(QColor::HexArgb);
    }
    note=m_document->note(id); if(!note || !matches()) return;
    const auto alignment=justification.value_or(note->justification);
    const auto fill=colourTarget==1 ? colour : note->fillColor;
    const auto outline=colourTarget==2 ? colour : note->outlineColor;
    m_edits->commit(tr("Change note appearance"),[this,id,alignment,fill,outline] {
        return m_document->setNoteAppearance(id,alignment,fill,outline);
    });
    invalidate();
}

void PropertiesPanel::refreshJustification(const QVector<ComponentId> &targets)
{
    while (m_appearanceControls->rowCount()) m_appearanceControls->removeRow(0);
    if (targets.isEmpty()) return;
    const auto *first = m_document->component(targets.first());
    if (!first) return;
    const auto value = first->propertyBlockJustification();
    bool mixed = false;
    for (auto id : targets)
        if (const auto *component = m_document->component(id))
            mixed |= component->propertyBlockJustification() != value;
    const auto epoch = m_epoch;
    auto *buttons = alignmentButtons(this,"propertyBlockJustification",
        mixed ? std::optional<PropertyJustification>{} : std::optional<PropertyJustification>{value},
        [this,targets,epoch,value,mixed](PropertyJustification choice) {
            if(epoch!=m_epoch || m_scene->moveInProgress() || m_scene->selection().components!=targets) return;
            if(!finishTextEdit()) {
                auto *group=findChild<QWidget *>("propertyBlockJustification")->findChild<QButtonGroup *>();
                group->setExclusive(false);
                const QString suffix=value==PropertyJustification::Left ? "Left" : value==PropertyJustification::Middle ? "Middle" : "Right";
                for(auto *button : group->buttons()) button->setChecked(!mixed && button->objectName().endsWith(suffix));
                group->setExclusive(true); return;
            }
            m_edits->commit(tr("Change property justification"),[&] {
                return m_document->setPropertyBlockJustification(targets,choice);
            });
            invalidate();
        });
    buttons->setEnabled(!m_scene->moveInProgress());
    m_appearanceControls->addRow(tr("Justification:"),buttons);
}

void PropertiesPanel::refreshLayouts(const QVector<ComponentId> &targets)
{
    while (m_layoutControls->rowCount()) m_layoutControls->removeRow(0);
    if (targets.size() != 1) return;
    const auto *component = m_document->component(targets.first());
    const auto *symbol = component ? m_document->symbol(component->symbolId()) : nullptr;
    if (!symbol) return;
    const auto epoch = m_epoch;
    for (const auto &control : symbol->layoutControls) {
        auto *combo = new QComboBox(this);
        combo->setObjectName("symbolLayout_" + control.key);
        combo->setAccessibleName(control.label);
        combo->setToolTip(tr("Positions refer to the symbol's normal pose, before rotation or flipping."));
        combo->setEnabled(!m_scene->moveInProgress());
        int current = -1;
        for (const auto &choice : control.choices) {
            combo->addItem(choice.label, choice.key);
            const int row = combo->count() - 1;
            if (choice.key == control.current) current = row;
            if (!m_document->canSetSymbolLayout(component->id(), control.key, choice.key))
                if (auto *model = qobject_cast<QStandardItemModel *>(combo->model())) {
                    model->item(row)->setEnabled(false);
                    model->item(row)->setToolTip(tr("Install the compatible layout alternative to use this option."));
                }
        }
        combo->setCurrentIndex(current);
        m_layoutControls->addRow(control.label, combo);
        connect(combo, qOverload<int>(&QComboBox::activated), this,
                [this, combo, id = component->id(), key = control.key, epoch, current](int index) {
            if (epoch != m_epoch || m_scene->moveInProgress() || m_scene->selectedComponentIds() != QVector<ComponentId>{id}) return;
            const QString choice = combo->itemData(index).toString();
            if (!finishTextEdit()) { QSignalBlocker block(combo); combo->setCurrentIndex(current); return; }
            QStringList errors;
            bool valid = false;
            const bool committed = m_edits->commit(tr("Change symbol layout"), [&] {
                valid = m_document->setSymbolLayout(id, key, choice, &errors);
                return valid;
            });
            if (!committed && !valid) {
                QSignalBlocker block(combo); combo->setCurrentIndex(current);
                m_error->setText(errors.join('\n'));
            } else { m_error->clear(); invalidate(); }
        });
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
