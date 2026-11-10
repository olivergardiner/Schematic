#include "mainwindow.h"
#include "propertiespanel.h"
#include "preferencesdialog.h"
#include "schematicsvgexporter.h"
#include "schematicscene.h"
#include "schematicview.h"

#include "wireendpoint.h"
#include "documentloadresult.h"
#include "symbollibrary.h"
#include "symbolrenderer.h"
#include "symbolmimedata.h"
#include "symbolimportdialog.h"

#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include <QAccessible>
#include <QMenuBar>
#include <QToolBar>
#include <QDockWidget>
#include <QStatusBar>
#include <QLabel>
#include <QToolButton>
#include <QDoubleSpinBox>
#include <QDrag>
#include <QMouseEvent>
#include <QMessageBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QCloseEvent>
#include <QAction>
#include <QActionGroup>
#include <QKeySequence>
#include <QIcon>
#include <QSaveFile>
#include <QFile>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QTextDocument>
#include <QTextCursor>
#include <QDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QInputDialog>
#include <QSignalBlocker>
#include <QLayout>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QScrollArea>
#include <QSplitter>
#include <QTreeWidget>
#include <QHeaderView>
#include <QTreeWidgetItem>
#include <QSettings>
#include <QStyle>
#include <QtMath>

#include <utility>
#include <cmath>

namespace {
// Decimal places shown by the grid-spacing spin box. The document stores
// the exact spacing (any finite positive value the file format accepts); the
// control only displays it at this precision, and loading never writes the
// displayed value back into the document.
constexpr int kGridSpinDecimals = 4;
constexpr int kMaxRecentFiles = 10;
#ifdef Q_OS_WIN
constexpr auto kPathCaseSensitivity = Qt::CaseInsensitive;
#else
constexpr auto kPathCaseSensitivity = Qt::CaseSensitive;
#endif

QString recentFilePath(const QString &path)
{
    const QFileInfo info(path);
    const QString canonical = info.canonicalFilePath();
    return QDir::cleanPath(canonical.isEmpty() ? info.absoluteFilePath() : canonical);
}

QStringList recentFiles()
{
    QStringList paths;
    const auto stored = QSettings().value("files/recentFiles").toStringList();
    for (const auto &entry : stored) {
        if (entry.isEmpty()) continue;
        const auto path = recentFilePath(entry);
        if (!paths.contains(path, kPathCaseSensitivity)) paths.append(path);
        if (paths.size() == kMaxRecentFiles) break;
    }
    return paths;
}

enum class TransformIcon { RotateLeft, RotateRight, FlipHorizontal, FlipVertical };

QIcon svgExportIcon()
{
    QPixmap pixmap(32,32);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(35,45,55), 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::white);
    painter.drawRoundedRect(QRectF(5,3,17,25), 1.5, 1.5);
    painter.setPen(QPen(QColor(35,110,160), 2.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawLine(QPointF(14,16), QPointF(29,16));
    painter.drawLine(QPointF(24,11), QPointF(29,16));
    painter.drawLine(QPointF(29,16), QPointF(24,21));
    return QIcon(pixmap);
}

QIcon textNoteIcon()
{
    QPixmap pixmap(32,32); pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap); painter.setRenderHint(QPainter::TextAntialiasing,true);
    QFont font; font.setPixelSize(25); font.setBold(true); painter.setFont(font);
    painter.setPen(QColor(35,45,55)); painter.drawText(pixmap.rect(),Qt::AlignCenter,QStringLiteral("T"));
    return QIcon(pixmap);
}

QIcon transformIcon(TransformIcon kind)
{
    const QString themeName = kind == TransformIcon::RotateLeft ? QStringLiteral("object-rotate-left")
        : kind == TransformIcon::RotateRight ? QStringLiteral("object-rotate-right")
        : kind == TransformIcon::FlipHorizontal ? QStringLiteral("object-flip-horizontal")
        : QStringLiteral("object-flip-vertical");
    const QIcon themed = QIcon::fromTheme(themeName);
    if (!themed.isNull()) return themed;
    QPixmap pixmap(32, 32);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    QPen pen(QColor(35, 45, 55), 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(QColor(35, 45, 55));

    if (kind == TransformIcon::FlipHorizontal || kind == TransformIcon::FlipVertical) {
        if (kind == TransformIcon::FlipVertical) {
            painter.translate(16, 16);
            painter.rotate(90);
            painter.translate(-16, -16);
        }
        painter.setPen(QPen(QColor(100, 110, 120), 1.5, Qt::DashLine));
        painter.drawLine(16, 5, 16, 27);
        painter.setPen(pen);
        painter.drawLine(14, 16, 5, 16);
        painter.drawPolygon(QPolygonF{QPointF(3, 16), QPointF(8, 12), QPointF(8, 20)});
        painter.drawLine(18, 16, 27, 16);
        painter.drawPolygon(QPolygonF{QPointF(29, 16), QPointF(24, 12), QPointF(24, 20)});
    } else {
        if (kind == TransformIcon::RotateLeft) {
            painter.translate(32, 0);
            painter.scale(-1, 1);
        }
        painter.setBrush(Qt::NoBrush);
        painter.drawArc(QRectF(6, 6, 20, 20), 90 * 16, -270 * 16);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(35, 45, 55));
        painter.drawPolygon(QPolygonF{QPointF(6, 10), QPointF(2, 17), QPointF(11, 17)});
    }
    return QIcon(pixmap);
}

QIcon historyIcon(bool redo)
{
    const QIcon themed = QIcon::fromTheme(redo ? QStringLiteral("edit-redo")
                                               : QStringLiteral("edit-undo"));
    if (!themed.isNull())
        return themed;

    QPixmap pixmap(32, 32);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const QColor color(35, 45, 55);
    painter.setPen(QPen(color, 2.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    const QRectF arc(8, 8, 16, 16);
    const int start = redo ? 135 : 45;
    const int sweep = redo ? -270 : 270;
    painter.drawArc(arc, start * 16, sweep * 16);

    const qreal angle = qDegreesToRadians(static_cast<qreal>(start + sweep));
    const QPointF center(16, 16);
    const QPointF tip(center.x() + 8 * qCos(angle), center.y() - 8 * qSin(angle));
    const qreal direction = redo ? -1.0 : 1.0;
    const QPointF tangent(-qSin(angle) * direction, -qCos(angle) * direction);
    const QPointF normal(-tangent.y(), tangent.x());
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    painter.drawPolygon(QPolygonF{tip,
                                  tip - tangent * 5.5 + normal * 3.5,
                                  tip - tangent * 5.5 - normal * 3.5});
    return QIcon(pixmap);
}

QIcon selectToolIcon()
{
    QPixmap pixmap(32, 32);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    QPolygonF pointer{QPointF(7, 4), QPointF(7, 25), QPointF(12, 20),
                      QPointF(16, 29), QPointF(20, 27), QPointF(16, 18),
                      QPointF(24, 18)};
    painter.setPen(QPen(Qt::white, 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(QColor(35, 45, 55));
    painter.drawPolygon(pointer);
    return QIcon(pixmap);
}

QIcon wireToolIcon()
{
    QPixmap pixmap(32, 32);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const QColor color(35, 45, 55);
    painter.setPen(QPen(color, 2.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawPolyline(QPolygonF{QPointF(5, 8), QPointF(16, 8), QPointF(16, 24),
                                  QPointF(27, 24)});
    painter.setBrush(Qt::white);
    painter.drawEllipse(QPointF(5, 8), 3, 3);
    painter.drawEllipse(QPointF(27, 24), 3, 3);
    return QIcon(pixmap);
}

QIcon gridIcon()
{
    QPixmap pixmap(20, 20);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(35, 45, 55), 1.4));
    painter.drawRect(QRect(3, 3, 14, 14));
    painter.drawLine(10, 3, 10, 17);
    painter.drawLine(3, 10, 17, 10);
    return QIcon(pixmap);
}

// Keep the same tile widgets and actions while the dock width changes.
class SymbolFlowLayout final : public QLayout
{
public:
    SymbolFlowLayout() { setContentsMargins(0, 0, 0, 0); setSpacing(4); }
    ~SymbolFlowLayout() override
    {
        while (auto *item = takeAt(0)) delete item;
    }

    void addItem(QLayoutItem *item) override { m_items.append(item); }
    int count() const override { return m_items.size(); }
    QLayoutItem *itemAt(int index) const override { return m_items.value(index, nullptr); }
    QLayoutItem *takeAt(int index) override
    {
        return index >= 0 && index < m_items.size() ? m_items.takeAt(index) : nullptr;
    }
    Qt::Orientations expandingDirections() const override { return Qt::Horizontal; }
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int width) const override { return arrange(QRect(0, 0, width, 0), false); }
    QSize sizeHint() const override { return minimumSize(); }
    QSize minimumSize() const override
    {
        QSize size;
        for (const auto *item : m_items) size = size.expandedTo(item->minimumSize());
        const auto margins = contentsMargins();
        return size + QSize(margins.left() + margins.right(), margins.top() + margins.bottom());
    }
    void setGeometry(const QRect &rect) override
    {
        QLayout::setGeometry(rect);
        arrange(rect, true);
    }

private:
    int arrange(const QRect &rect, bool apply) const
    {
        const auto margins = contentsMargins();
        const QRect available = rect.marginsRemoved(margins);
        int x = available.x(), y = available.y(), rowHeight = 0;
        for (auto *item : m_items) {
            const QSize size = item->sizeHint();
            if (rowHeight > 0 && x + size.width() > available.x() + available.width()) {
                x = available.x();
                y += rowHeight + spacing();
                rowHeight = 0;
            }
            if (apply) item->setGeometry(QRect(QPoint(x, y), size));
            x += size.width() + spacing();
            rowHeight = qMax(rowHeight, size.height());
        }
        return y + rowHeight - rect.y() + margins.bottom();
    }
    QVector<QLayoutItem *> m_items;
};

class SymbolTileButton final : public QToolButton
{
public:
    SymbolTileButton(QAction *action, SymbolId id, QWidget *parent)
        : QToolButton(parent)
        , m_id(std::move(id))
    {
        setDefaultAction(action);
    }

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton) {
            m_pressPosition = event->position().toPoint();
            m_dragStarted = false;
        }
        QToolButton::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (!m_dragStarted && (event->buttons() & Qt::LeftButton)
            && (event->position().toPoint() - m_pressPosition).manhattanLength()
                   >= QApplication::startDragDistance()) {
            m_dragStarted = true;
            setDown(false);

            QDrag drag(this);
            drag.setMimeData(SymbolMimeData::create(m_id));
            const QPixmap pixmap = icon().pixmap(iconSize());
            drag.setPixmap(pixmap);
            drag.setHotSpot(QPoint(pixmap.width() / 2, pixmap.height() / 2));
            drag.exec(Qt::CopyAction);
            setDown(false);
            event->accept();
            return;
        }
        QToolButton::mouseMoveEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (m_dragStarted) {
            m_dragStarted = false;
            setDown(false);
            event->accept();
            return;
        }
        QToolButton::mouseReleaseEvent(event);
    }

private:
    SymbolId m_id;
    QPoint m_pressPosition;
    bool m_dragStarted = false;
};
} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_scene(new SchematicScene(this))
    , m_view(new SchematicView(this))
{
    m_editController = new DocumentEditController(&m_document, this);
    m_scene->setPropertyBlockGridSpacing(PreferencesDialog::savedPropertyBlockGridSpacing());
    m_view->setCadStyleMouseWheel(PreferencesDialog::savedCadStyleMouseWheel());
    m_scene->setKeepPlacingSymbols(PreferencesDialog::savedKeepPlacingSymbols());
    const QStringList symbolLoadWarnings = m_symbolPackageStore.loadInto(&applicationSymbolLibrary());
    m_view->setScene(m_scene);
    setCentralWidget(m_view);

    createActions();
    createMenus();
    createToolBar();
    createSymbolDock();
    createPropertiesDock();
    createStatusBar();
    if (!symbolLoadWarnings.isEmpty())
        QMessageBox::warning(this, tr("Symbol Library"), symbolLoadWarnings.join(QLatin1Char('\n')));
    populateToolbarsMenu();
    restoreWindowLayout();

    connect(m_view, &SchematicView::zoomFactorChanged, this, &MainWindow::onZoomFactorChanged);
    connect(m_view, &SchematicView::mouseScenePositionChanged,
            this, &MainWindow::onMouseScenePositionChanged);
    connect(m_scene, &SchematicScene::statusMessage, this, &MainWindow::onSceneStatusMessage);
    connect(m_scene, &SchematicScene::componentDoubleClicked,
            this, &MainWindow::onEditComponentLabels);
    connect(m_scene, &SchematicScene::noteEditRequested, this, [this](NoteId) {
        m_propertiesDock->show(); m_propertiesDock->raise(); m_propertiesPanel->focusNote();
    });
    connect(m_scene, &SchematicScene::selectionChanged,
            this, &MainWindow::updateRenameActionEnabled);
    connect(m_scene, &SchematicScene::moveStateChanged,
            this, &MainWindow::updateRenameActionEnabled);
    connect(m_scene, &SchematicScene::editModeChanged, this,
            [this](SchematicScene::EditMode mode) {
                if (mode == SchematicScene::EditMode::Select && m_selectAction)
                    m_selectAction->setChecked(true);
                else if (mode == SchematicScene::EditMode::DrawWire && m_drawWireAction)
                    m_drawWireAction->setChecked(true);
                else if (mode == SchematicScene::EditMode::PlaceNote && m_noteAction)
                    m_noteAction->setChecked(true);
                updateRenameActionEnabled();
            });
    connect(m_view, &SchematicView::symbolDropped, this,
            [this](const SymbolId &id, const QPointF &position) {
                m_scene->placeComponentAt(id, position);
            });

    if (!restoreGeometry(QSettings().value(QStringLiteral("window/geometry")).toByteArray()))
        resize(1000, 700);
    updateWindowTitle();

    // m_document starts out empty (default-constructed); bind it to the
    // scene explicitly here rather than relying on some other call to do so
    // indirectly - see the m_document declaration comment in mainwindow.h.
    m_scene->bindDocument(&m_document);
    m_view->setGridSpacing(m_document.gridSpacing());
    m_scene->setEditCommitter([this](const QString &description,
                                     const std::function<bool()> &operation,
                                     std::function<DocumentSelection()> afterSelection) {
        return m_editController->commit(description, operation, std::move(afterSelection));
    });
    m_editController->setSelectionCallbacks(
        [this]() { return m_scene->selection(); },
        [this](const DocumentSelection &selection) {
            m_scene->restoreAfterDocumentChange(selection);
            syncGridControls();
            updateWindowTitle();
        });
    connect(m_editController->undoStack(), &QUndoStack::cleanChanged,
            this, &MainWindow::updateWindowTitle);
    connect(QApplication::clipboard(), &QClipboard::dataChanged, this, [this] {
        updateEditActions();
    });
    connect(qApp, &QApplication::focusChanged, this, [this](QWidget *, QWidget *now) {
        if (!qobject_cast<QMenu *>(now) && !QApplication::activePopupWidget()) {
            m_editTextTarget = now && isAncestorOf(now) ? qobject_cast<QLineEdit *>(now) : nullptr;
            m_editNoteTextTarget = now && isAncestorOf(now) ? qobject_cast<QPlainTextEdit *>(now) : nullptr;
        }
        if (m_editTextTarget) {
            connect(m_editTextTarget, &QLineEdit::textChanged, this, &MainWindow::updateEditActions, Qt::UniqueConnection);
            connect(m_editTextTarget, &QLineEdit::selectionChanged, this, &MainWindow::updateEditActions, Qt::UniqueConnection);
        }
        if (m_editNoteTextTarget) {
            connect(m_editNoteTextTarget, &QPlainTextEdit::textChanged, this, &MainWindow::updateEditActions, Qt::UniqueConnection);
            connect(m_editNoteTextTarget, &QPlainTextEdit::selectionChanged, this, &MainWindow::updateEditActions, Qt::UniqueConnection);
            connect(m_editNoteTextTarget, &QPlainTextEdit::undoAvailable, this, &MainWindow::updateEditActions, Qt::UniqueConnection);
            connect(m_editNoteTextTarget, &QPlainTextEdit::redoAvailable, this, &MainWindow::updateEditActions, Qt::UniqueConnection);
        }
        updateRenameActionEnabled();
    });
    updateRenameActionEnabled();
}

MainWindow::~MainWindow()
{
    disconnect(qApp, nullptr, this, nullptr);
    disconnect(QApplication::clipboard(), nullptr, this, nullptr);
    // QUndoStack emits cleanChanged when its commands are destroyed by
    // QObject child cleanup, after this window's members are gone. Do not
    // dispatch updateWindowTitle() into the partially destroyed window.
    disconnect(m_editController->undoStack(), nullptr, this, nullptr);
    // Stop deferred editor callbacks and detach the scene while the model
    // and controls are still alive, before QObject deletes child widgets.
    delete m_propertiesDock;
    m_propertiesPanel = nullptr;
    disconnect(m_scene, nullptr, this, nullptr);
    m_view->setScene(nullptr);
    m_scene->bindDocument(nullptr);
}

void MainWindow::createPropertiesDock()
{
    m_propertiesDock = new QDockWidget(tr("Properties"),this);
    m_propertiesDock->setObjectName("propertiesDock");
    m_propertiesPanel = new PropertiesPanel(&m_document,m_scene,m_editController,m_propertiesDock);
    m_propertiesDock->setWidget(m_propertiesPanel);
    m_propertiesDock->setMinimumWidth(330);
    addDockWidget(Qt::LeftDockWidgetArea,m_propertiesDock);
    if (auto *symbols = findChild<QDockWidget *>("symbolDock")) {
        splitDockWidget(symbols, m_propertiesDock, Qt::Vertical);
        resizeDocks({symbols, m_propertiesDock}, {500, 300}, Qt::Vertical);
    }
}

void MainWindow::createActions()
{
    // Actions are created inline in createMenus()/createToolBar(); this
    // function exists as a placeholder for when actions need to be shared
    // between more widgets (e.g. context menus) as the app grows.
}

void MainWindow::createMenus()
{
    auto *fileMenu = menuBar()->addMenu(tr("&File"));

    auto *newAction = fileMenu->addAction(tr("&New"), this, &MainWindow::onNew);
    newAction->setShortcut(QKeySequence::New);

    auto *openAction = fileMenu->addAction(tr("&Open..."), this, &MainWindow::onOpen);
    openAction->setShortcut(QKeySequence::Open);
    m_recentFilesMenu = fileMenu->addMenu(tr("Open &Recent"));
    m_recentFilesMenu->setObjectName("recentFilesMenu");
    for (int i = 0; i < kMaxRecentFiles; ++i) {
        auto *action = m_recentFilesMenu->addAction(QString());
        action->setObjectName(QStringLiteral("recentFileAction%1").arg(i + 1));
        action->setVisible(false);
        connect(action, &QAction::triggered, this, [this, action] {
            openDocumentFrom(action->data().toString());
        });
        m_recentFileActions.append(action);
    }
    m_recentFilesMenu->addSeparator();
    auto *clearRecent = m_recentFilesMenu->addAction(tr("Clear Recent Files"), this, [this] {
        QSettings().remove("files/recentFiles");
        updateRecentFilesMenu();
    });
    clearRecent->setObjectName("clearRecentFilesAction");
    connect(fileMenu, &QMenu::aboutToShow, this, &MainWindow::updateRecentFilesMenu);
    updateRecentFilesMenu();

    fileMenu->addAction(tr("Import Symbol..."), this, &MainWindow::onImportSymbol);

    fileMenu->addSeparator();

    auto *saveAction = fileMenu->addAction(tr("&Save"), this, &MainWindow::onSave);
    saveAction->setShortcut(QKeySequence::Save);

    auto *saveAsAction = fileMenu->addAction(tr("Save &As..."), this, &MainWindow::onSaveAs);
    saveAsAction->setShortcut(QKeySequence::SaveAs);

    fileMenu->addSeparator();
    m_svgExportAction = fileMenu->addAction(svgExportIcon(), tr("SVG &Export..."), this, &MainWindow::onSvgExport);
    m_svgExportAction->setObjectName("svgExportAction");
    m_svgExportAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));
    m_svgExportAction->setToolTip(tr("Export schematic as SVG (Ctrl+E)"));
    m_svgExportAction->setStatusTip(tr("Export the complete schematic as a vector image"));
    m_svgExportAction->setEnabled(false);
    fileMenu->addSeparator();

    auto *exitAction = fileMenu->addAction(tr("E&xit"), this, &QWidget::close);
    exitAction->setShortcut(QKeySequence::Quit);

    auto *editMenu = menuBar()->addMenu(tr("&Edit"));
    m_editMenu = editMenu;
    connect(editMenu, &QMenu::aboutToShow, this, &MainWindow::updateEditActions);
    m_undoAction = m_editController->undoStack()->createUndoAction(this, tr("&Undo"));
    m_undoAction->setObjectName("undoAction");
    m_undoAction->setShortcut(QKeySequence::Undo);
    m_undoAction->setIcon(historyIcon(false));
    m_undoAction->setToolTip(tr("Undo"));
    editMenu->addAction(m_undoAction);
    m_redoAction = m_editController->undoStack()->createRedoAction(this, tr("&Redo"));
    m_redoAction->setObjectName("redoAction");
    m_redoAction->setShortcut(QKeySequence::Redo);
    m_redoAction->setIcon(historyIcon(true));
    m_redoAction->setToolTip(tr("Redo"));
    editMenu->addAction(m_redoAction);
    disconnect(m_undoAction, nullptr, m_editController->undoStack(), nullptr);
    disconnect(m_redoAction, nullptr, m_editController->undoStack(), nullptr);
    connect(m_undoAction, &QAction::triggered, this, [this] {
        if (m_editNoteTextTarget) m_editNoteTextTarget->undo();
        else if (m_editTextTarget) m_editTextTarget->undo(); else m_editController->undoStack()->undo();
        updateEditActions();
    });
    connect(m_redoAction, &QAction::triggered, this, [this] {
        if (m_editNoteTextTarget) m_editNoteTextTarget->redo();
        else if (m_editTextTarget) m_editTextTarget->redo(); else m_editController->undoStack()->redo();
        updateEditActions();
    });

    editMenu->addSeparator();
    const auto clipboardAction = [this,editMenu](const QString &title, const QString &operation, QKeySequence::StandardKey key) {
        auto *action = editMenu->addAction(title, this, [this,operation] { clipboardEdit(operation); });
        action->setObjectName(operation + "Action"); action->setShortcuts(key); return action;
    };
    m_cutAction = clipboardAction(tr("Cu&t"), "cut", QKeySequence::Cut);
    m_copyAction = clipboardAction(tr("&Copy"), "copy", QKeySequence::Copy);
    m_pasteAction = clipboardAction(tr("&Paste"), "paste", QKeySequence::Paste);
    m_deleteAction = clipboardAction(tr("&Delete"), "delete", QKeySequence::Delete);
    m_noteAction = editMenu->addAction(textNoteIcon(),tr("Add Text Note"),this,[this] {
        m_scene->setEditMode(SchematicScene::EditMode::PlaceNote);
    });
    m_noteAction->setObjectName("addTextNoteAction"); m_noteAction->setCheckable(true);
    m_noteAction->setToolTip(tr("Add a wrapped text note"));
    editMenu->addSeparator();
    m_renameAction = editMenu->addAction(tr("&Rename..."), this,
                                         &MainWindow::onRenameSelectedComponent);
    m_renameAction->setShortcut(Qt::Key_F2);
    // Qt::WindowShortcut (the default for an action added to a menu, but
    // set explicitly for clarity) fires as long as this window - or any
    // descendant widget, including both m_view and the symbol dock added
    // in createSymbolDock() - has focus. Scoping this to m_view alone would
    // miss F2 presses while focus is in the dock, since the dock is a
    // sibling of m_view under QMainWindow, not a child of it.
    m_renameAction->setShortcutContext(Qt::WindowShortcut);
    m_renameAction->setEnabled(false); // Enabled only while exactly one component is selected.
    addAction(m_renameAction);

    editMenu->addSeparator();
    auto *preferencesAction = editMenu->addAction(tr("&Preferences…"), this, &MainWindow::onPreferences);
    preferencesAction->setObjectName("preferencesAction");
    preferencesAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Comma));

    auto *viewMenu = menuBar()->addMenu(tr("&View"));
    auto *zoomInAction = viewMenu->addAction(tr("Zoom &In"), m_view, &SchematicView::zoomIn);
    zoomInAction->setShortcut(QKeySequence::ZoomIn);
    auto *zoomOutAction = viewMenu->addAction(tr("Zoom &Out"), m_view, &SchematicView::zoomOut);
    zoomOutAction->setShortcut(QKeySequence::ZoomOut);
    viewMenu->addAction(tr("Zoom to &Fit"), m_view, &SchematicView::zoomToFit);
    auto *zoomResetAction = viewMenu->addAction(tr("&Reset Zoom"), m_view, &SchematicView::zoomReset);
    zoomResetAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));

    m_toolbarsMenu = viewMenu->addMenu(tr("Toolbars"));

    auto *helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(tr("&About"), this, &MainWindow::onAbout);
    helpMenu->addAction(tr("About &Qt"), qApp, &QApplication::aboutQt);
}

void MainWindow::rememberRecentFile(const QString &path)
{
    const QString absolute = recentFilePath(path);
    auto paths = recentFiles();
    for (auto it = paths.begin(); it != paths.end();) {
        if (it->compare(absolute, kPathCaseSensitivity) == 0) it = paths.erase(it);
        else ++it;
    }
    paths.prepend(absolute);
    while (paths.size() > kMaxRecentFiles) paths.removeLast();
    QSettings().setValue("files/recentFiles", paths);
    updateRecentFilesMenu();
}

void MainWindow::updateRecentFilesMenu()
{
    if (!m_recentFilesMenu) return;
    const auto paths = recentFiles();
    for (int i = 0; i < m_recentFileActions.size(); ++i) {
        auto *action = m_recentFileActions[i];
        const bool visible = i < paths.size();
        action->setVisible(visible);
        if (!visible) { action->setData(QVariant()); continue; }
        const QFileInfo info(paths[i]);
        const QString label = (info.fileName() + QStringLiteral(" — ")
            + QDir::toNativeSeparators(info.absolutePath())).replace(QLatin1Char('&'), QStringLiteral("&&"));
        const QString prefix = i < 9 ? QStringLiteral("&%1 ").arg(i + 1) : QStringLiteral("%1 ").arg(i + 1);
        action->setText(prefix + label);
        action->setData(paths[i]);
        action->setToolTip(QDir::toNativeSeparators(paths[i]));
        action->setStatusTip(action->toolTip());
    }
    m_recentFilesMenu->menuAction()->setEnabled(!paths.isEmpty());
}

void MainWindow::createToolBar()
{
    auto *fileToolBar = addToolBar(tr("File"));
    fileToolBar->setObjectName("fileToolBar");
    fileToolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    QAction *newAction = fileToolBar->addAction(style()->standardIcon(QStyle::SP_FileIcon),
                                                tr("New"), this, &MainWindow::onNew);
    newAction->setObjectName("newAction");
    newAction->setToolTip(tr("New schematic"));
    QAction *openAction = fileToolBar->addAction(style()->standardIcon(QStyle::SP_DirOpenIcon),
                                                 tr("Open"), this, &MainWindow::onOpen);
    openAction->setObjectName("openAction");
    openAction->setToolTip(tr("Open schematic"));
    QAction *saveAction = fileToolBar->addAction(style()->standardIcon(QStyle::SP_DialogSaveButton),
                                                 tr("Save"), this, &MainWindow::onSave);
    saveAction->setObjectName("saveAction");
    saveAction->setToolTip(tr("Save schematic"));
    fileToolBar->addSeparator();
    fileToolBar->addAction(m_svgExportAction);

    // m_modeGroup also gathers the per-symbol placement actions added in
    // createSymbolDock(), so Select/DrawWire/each-symbol-button are all
    // mutually exclusive - see the m_modeGroup declaration comment in
    // mainwindow.h.
    m_modeGroup = new QActionGroup(this);
    m_modeGroup->setExclusive(true);

    auto *toolToolBar = addToolBar(tr("Tools"));
    toolToolBar->setObjectName("toolsToolBar");
    toolToolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_selectAction = toolToolBar->addAction(selectToolIcon(), tr("Select"));
    m_selectAction->setObjectName("selectToolAction");
    m_selectAction->setToolTip(tr("Select and move components"));
    m_selectAction->setCheckable(true);
    m_selectAction->setChecked(true);
    m_modeGroup->addAction(m_selectAction);
    connect(m_selectAction, &QAction::triggered, this, &MainWindow::onSelectModeTriggered);

    m_drawWireAction = toolToolBar->addAction(wireToolIcon(), tr("Draw Wire"));
    m_drawWireAction->setObjectName("drawWireToolAction");
    m_drawWireAction->setToolTip(tr("Draw orthogonal wires between terminals"));
    m_drawWireAction->setCheckable(true);
    m_modeGroup->addAction(m_drawWireAction);
    toolToolBar->addAction(m_noteAction);
    m_modeGroup->addAction(m_noteAction);
    connect(m_drawWireAction, &QAction::triggered, this, &MainWindow::onDrawWireModeTriggered);

    auto *editToolBar = addToolBar(tr("Edit"));
    editToolBar->setObjectName("editToolBar");
    editToolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    editToolBar->addAction(m_undoAction);
    editToolBar->addAction(m_redoAction);
    editToolBar->addSeparator();

    m_rotateLeftAction = editToolBar->addAction(transformIcon(TransformIcon::RotateLeft), tr("Rotate Left"));
    m_rotateLeftAction->setObjectName("rotateLeftComponentAction");
    m_rotateLeftAction->setToolTip(tr("Rotate the selection 90 degrees counterclockwise"));
    m_rotateLeftAction->setStatusTip(m_rotateLeftAction->toolTip());
    connect(m_rotateLeftAction, &QAction::triggered, this, &MainWindow::onRotateLeftSelectedComponent);
    m_rotateAction = editToolBar->addAction(transformIcon(TransformIcon::RotateRight), tr("Rotate Right"));
    m_rotateAction->setObjectName("rotateComponentAction");
    m_rotateAction->setToolTip(tr("Rotate the selection 90 degrees clockwise"));
    m_rotateAction->setStatusTip(m_rotateAction->toolTip());
    connect(m_rotateAction, &QAction::triggered,
            this, &MainWindow::onRotateSelectedComponent);
    m_mirrorAction = editToolBar->addAction(transformIcon(TransformIcon::FlipHorizontal), tr("Flip Horizontal"));
    m_mirrorAction->setObjectName("mirrorComponentAction");
    m_mirrorAction->setToolTip(tr("Flip the selection left to right on the canvas"));
    m_mirrorAction->setStatusTip(m_mirrorAction->toolTip());
    connect(m_mirrorAction, &QAction::triggered,
            this, &MainWindow::onMirrorSelectedComponent);

    m_flipVerticalAction = editToolBar->addAction(transformIcon(TransformIcon::FlipVertical), tr("Flip Vertical"));
    m_flipVerticalAction->setObjectName("flipVerticalComponentAction");
    m_flipVerticalAction->setToolTip(tr("Flip the selection top to bottom on the canvas"));
    m_flipVerticalAction->setStatusTip(m_flipVerticalAction->toolTip());
    connect(m_flipVerticalAction, &QAction::triggered, this, &MainWindow::onFlipVerticalSelectedComponent);

    m_rotateLeftAction->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_R));
    m_rotateAction->setShortcut(QKeySequence(Qt::Key_R));
    m_mirrorAction->setShortcut(QKeySequence(Qt::Key_H));
    m_flipVerticalAction->setShortcut(QKeySequence(Qt::Key_V));
    for (auto *action : {m_rotateLeftAction, m_rotateAction, m_mirrorAction, m_flipVerticalAction}) {
        action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        m_view->addAction(action);
        action->setToolTip(action->toolTip() + " (" + action->shortcut().toString(QKeySequence::NativeText) + ")");
        m_editMenu->insertAction(m_renameAction, action);
        if (action == m_rotateAction) m_editMenu->insertSeparator(m_renameAction);
    }
    m_editMenu->insertSeparator(m_renameAction);

    auto *viewToolBar = addToolBar(tr("View"));
    viewToolBar->setObjectName("viewToolBar");
    viewToolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    auto *gridLabel = new QLabel(this);
    gridLabel->setPixmap(gridIcon().pixmap(16, 16));
    gridLabel->setAccessibleName(tr("Grid spacing"));
    gridLabel->setToolTip(tr("Grid spacing"));
    viewToolBar->addWidget(gridLabel);
    m_gridSpacingSpin = new QDoubleSpinBox(this);
    m_gridSpacingSpin->setRange(0.1, 1000.0);
    m_gridSpacingSpin->setDecimals(kGridSpinDecimals);
    m_gridSpacingSpin->setValue(m_document.gridSpacing());
    m_gridSpacingSpin->setToolTip(
        tr("Sets the snap increment and minor grid line spacing; major grid "
           "lines are drawn every 10 minor intervals."));
    connect(m_gridSpacingSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MainWindow::onGridSpacingChanged);
    viewToolBar->addWidget(m_gridSpacingSpin);
}

void MainWindow::restoreWindowLayout()
{
    const QSettings settings;
    const QByteArray state = settings.value(QStringLiteral("window/state")).toByteArray();
    if (!state.isEmpty())
        restoreState(state, 2);
}

void MainWindow::saveWindowLayout()
{
    QSettings settings;
    settings.setValue(QStringLiteral("window/geometry"), saveGeometry());
    settings.setValue(QStringLiteral("window/state"), saveState(2));
}

void MainWindow::populateToolbarsMenu()
{
    if (!m_toolbarsMenu)
        return;

    for (QToolBar *toolBar : findChildren<QToolBar *>())
        m_toolbarsMenu->addAction(toolBar->toggleViewAction());
    for (QDockWidget *dock : findChildren<QDockWidget *>())
        m_toolbarsMenu->addAction(dock->toggleViewAction());
}

void MainWindow::createSymbolDock()
{
    auto *dock = new QDockWidget(tr("Symbols"), this);
    dock->setObjectName("symbolDock");
    dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);

    auto *splitter = new QSplitter(Qt::Vertical, dock);
    m_categoryTree = new QTreeWidget(splitter);
    m_categoryTree->setHeaderHidden(true);
    m_categoryTree->setMinimumWidth(105);
    m_categoryTree->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_categoryTree->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    // Let long category paths determine the column width rather than clipping
    // them to the viewport, so horizontal scrolling is available when needed.
    m_categoryTree->header()->setStretchLastSection(false);
    m_categoryTree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_symbolScrollArea = new QScrollArea(splitter);
    m_symbolScrollArea->setWidgetResizable(true);
    m_symbolScrollArea->setMinimumWidth(88);
    m_symbolGridContent = new QWidget(m_symbolScrollArea);
    m_symbolSectionsLayout = new QVBoxLayout(m_symbolGridContent);
    m_symbolSectionsLayout->setContentsMargins(4, 4, 4, 4);
    m_symbolSectionsLayout->setSpacing(8);
    m_symbolScrollArea->setWidget(m_symbolGridContent);
    splitter->addWidget(m_categoryTree);
    splitter->addWidget(m_symbolScrollArea);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({250, 250});

    connect(m_categoryTree, &QTreeWidget::currentItemChanged, this,
            [this](QTreeWidgetItem *current, QTreeWidgetItem *) {
                if (current)
                    populateSymbolGrid(current->data(0, Qt::UserRole).toString());
            });
    connect(m_categoryTree, &QTreeWidget::collapsed, this, [this] {
        // Qt 6.11.1 leaves cached accessible rows at their expanded positions.
        // Stale sibling indexes can send Windows UI Automation into a cycle.
        QAccessibleTableModelChangeEvent event(m_categoryTree,
            QAccessibleTableModelChangeEvent::ModelReset);
        if (auto *accessible = QAccessible::queryAccessibleInterface(m_categoryTree)) {
            if (auto *table = accessible->tableInterface())
                table->modelChange(&event);
        }
        QAccessible::updateAccessibility(&event);
    });
    dock->setWidget(splitter);
    addDockWidget(Qt::LeftDockWidgetArea, dock);
    rebuildSymbolDockContents();
}

void MainWindow::rebuildSymbolDockContents()
{
    if (!m_categoryTree)
        return;
    QString selectedPath;
    if (m_categoryTree->currentItem())
        selectedPath = m_categoryTree->currentItem()->data(0, Qt::UserRole).toString();
    for (QAction *action : m_symbolActions) {
        m_modeGroup->removeAction(action);
        delete action;
    }
    m_symbolActions.clear();
    m_categoryItems.clear();
    m_categoryTree->clear();

    auto *root = new QTreeWidgetItem(m_categoryTree);
    root->setText(0, tr("All Symbols"));
    root->setData(0, Qt::UserRole, QString());
    root->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    m_categoryItems.insert(QString(), root);

    std::function<void(const QVector<SymbolCategory> &, QTreeWidgetItem *, const QString &)> populate;
    populate = [&](const QVector<SymbolCategory> &nodes, QTreeWidgetItem *parent, const QString &parentPath) {
        for (const auto &node : nodes) {
            const auto path = parentPath.isEmpty() ? node.name : parentPath + '/' + node.name;
            auto *category = new QTreeWidgetItem(parent);
            category->setText(0, node.name);
            category->setData(0, Qt::UserRole, path);
            category->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            m_categoryItems.insert(path, category);
            populate(node.children, category, path);
        }
    };
    populate(applicationSymbolLibrary().categoryTree(), root, {});

    for (const LibrarySymbol &symbol : applicationSymbolLibrary().symbols()) {
        if (!symbol.paletteVisible) continue;
        auto *action = new QAction(symbol.displayName, this);
        action->setCheckable(true);
        m_modeGroup->addAction(action);
        connect(action, &QAction::triggered, this, [this, id = symbol.id]() {
            onPlaceComponentKindChanged(id);
        });
        m_symbolActions.insert(symbol.id, action);
    }

    m_categoryTree->expandAll();
    QTreeWidgetItem *selection = m_categoryItems.value(selectedPath, nullptr);
    if (!selection && m_categoryTree->topLevelItemCount() > 0)
        selection = m_categoryTree->topLevelItem(0);
    if (selection) {
        m_categoryTree->setCurrentItem(selection);
        populateSymbolGrid(selection->data(0, Qt::UserRole).toString());
    }
}

void MainWindow::populateSymbolGrid(const QString &categoryPath)
{
    if (!m_symbolScrollArea)
        return;

    QWidget *oldContent = m_symbolScrollArea->takeWidget();
    delete oldContent;
    m_symbolGridContent = new QWidget(m_symbolScrollArea);
    m_symbolSectionsLayout = new QVBoxLayout(m_symbolGridContent);
    m_symbolSectionsLayout->setContentsMargins(4, 4, 4, 4);
    m_symbolSectionsLayout->setSpacing(8);

    struct PaletteSection
    {
        QString title;
        QVector<const LibrarySymbol *> symbols;
    };
    QVector<PaletteSection> sections;
    QHash<QString, int> sectionIndices;
    const QStringList selectedPath = categoryPath.isEmpty() ? QStringList{}
                                                           : libraryCategoryPath(categoryPath);
    const QString descendantPrefix = categoryPath + QLatin1Char('/');

    for (const auto *definition : applicationSymbolLibrary().paletteSymbols()) {
        const auto &symbol = *definition;
        if (!categoryPath.isEmpty() && symbol.category != categoryPath
            && !symbol.category.startsWith(descendantPrefix))
            continue;

        QString sectionTitle;
        if (symbol.category != categoryPath) {
            const QStringList symbolPath = libraryCategoryPath(symbol.category);
            if (symbolPath.size() > selectedPath.size())
                sectionTitle = symbolPath.at(selectedPath.size());
        }
        int sectionIndex = sectionIndices.value(sectionTitle, -1);
        if (sectionIndex < 0) {
            sectionIndex = sections.size();
            sectionIndices.insert(sectionTitle, sectionIndex);
            sections.append({sectionTitle, {}});
        }
        sections[sectionIndex].symbols.append(&symbol);
    }

    const QSize thumbnailSize(64, 48);
    for (const PaletteSection &section : sections) {
        if (!section.title.isEmpty()) {
            auto *heading = new QLabel(section.title, m_symbolGridContent);
            QFont headingFont = heading->font();
            headingFont.setBold(true);
            heading->setFont(headingFont);
            m_symbolSectionsLayout->addWidget(heading);
        }

        auto *grid = new SymbolFlowLayout;
        for (int i = 0; i < section.symbols.size(); ++i) {
            const LibrarySymbol &symbol = *section.symbols.at(i);
            QAction *action = m_symbolActions.value(symbol.id, nullptr);
            if (!action)
                continue;

            QPixmap thumbnail(thumbnailSize);
            thumbnail.fill(Qt::transparent);
            {
                QPainter painter(&thumbnail);
                SymbolRenderer::paintPreview(&painter, symbol,
                                             QRectF(QPointF(0, 0), thumbnailSize));
            }
            action->setIcon(QIcon(thumbnail));
            action->setToolTip(symbol.displayName);

            auto *button = new SymbolTileButton(action, symbol.id, m_symbolGridContent);
            button->setToolButtonStyle(Qt::ToolButtonIconOnly);
            button->setIconSize(thumbnailSize);
            button->setFixedSize(72, 56);
            button->setAccessibleName(symbol.displayName);
            button->setToolTip(symbol.displayName);
            grid->addWidget(button);
        }
        m_symbolSectionsLayout->addLayout(grid);
    }
    m_symbolSectionsLayout->addStretch(1);
    m_symbolScrollArea->setWidget(m_symbolGridContent);
}

void MainWindow::createStatusBar()
{
    m_positionLabel = new QLabel(tr("  "), this);
    m_zoomLabel = new QLabel(tr("Zoom: 100%"), this);

    statusBar()->addWidget(m_positionLabel, 1);
    statusBar()->addPermanentWidget(m_zoomLabel);
}

void MainWindow::updateWindowTitle()
{
    const QString name = m_currentFilePath.isEmpty() ? tr("Untitled") : m_currentFilePath;
    const bool modified = m_editController && !m_editController->undoStack()->isClean();
    setWindowTitle(QString("%1%2 - Schematic").arg(modified ? "*" : "", name));
}

void MainWindow::syncGridControls()
{
    // The spin box rounds to its displayed precision and clamps to its
    // range, so a loaded spacing outside that (still valid in the file) must
    // not travel back through valueChanged() into the document. Block the
    // signal while updating; the model keeps the exact loaded value and the
    // view uses it directly.
    {
        const QSignalBlocker blocker(m_gridSpacingSpin);
        m_gridSpacingSpin->setValue(m_document.gridSpacing());
    }
    m_view->setGridSpacing(m_document.gridSpacing());
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (maybeSave()) {
        saveWindowLayout();
        event->accept();
    } else {
        event->ignore();
    }
}

bool MainWindow::maybeSave()
{
    if (m_editController->undoStack()->isClean())
        return true;

    const auto result = QMessageBox::warning(this, tr("Schematic"),
        tr("The document has been modified.\nDo you want to save your changes?"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

    switch (result) {
    case QMessageBox::Save:
        return onSave();
    case QMessageBox::Cancel:
        return false;
    default:
        return true;
    }
}

void MainWindow::onNew()
{
    m_propertiesPanel->invalidate();
    if (!maybeSave())
        return;

    m_document = Document();
    m_scene->bindDocument(&m_document);
    m_editController->resetHistory();
    syncGridControls();
    m_currentFilePath.clear();
    updateWindowTitle();
}

void MainWindow::onOpen()
{
    m_propertiesPanel->invalidate();
    const QString path = QFileDialog::getOpenFileName(this, tr("Open Schematic"), QString(),
        tr("Schematic Files (*.schematic);;All Files (*)"));
    if (!path.isEmpty()) openDocumentFrom(path);
}

bool MainWindow::openDocumentFrom(const QString &path)
{
    m_propertiesPanel->invalidate();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, tr("Schematic"),
            tr("Could not open \"%1\" for reading: %2").arg(path, file.errorString()));
        return false;
    }
    DocumentLoadResult result = Document::fromJsonBytes(file.readAll());
    if (!result.document) {
        QMessageBox::warning(this, tr("Schematic"),
            tr("\"%1\" could not be loaded:\n%2").arg(path, result.errors.join('\n')));
        return false; // The current document is left completely unchanged.
    }

    if (!result.conflicts.isEmpty()) {
        QDialog dialog(this);
        dialog.setWindowTitle(tr("Review document symbols"));
        auto *layout = new QVBoxLayout(&dialog);
        layout->addWidget(new QLabel(tr("Choose which definition to use for each symbol."), &dialog));
        auto *form = new QFormLayout;
        layout->addLayout(form);
        QVector<QComboBox *> choices;
        for (const auto &conflict : result.conflicts) {
            auto *choice = new QComboBox(&dialog);
            choice->addItem(tr("Use embedded in this document"), 0);
            if (conflict.kind == SymbolConflictKind::Presentation) {
                choice->addItem(tr("Update library to embedded version"), 1);
                choice->addItem(tr("Use library version in this document"), 2);
            } else if (conflict.kind == SymbolConflictKind::Missing)
                choice->addItem(tr("Add embedded version to library"), 1);
            const QString detail = conflict.kind == SymbolConflictKind::Terminals ? tr(" (different terminals)")
                : conflict.kind == SymbolConflictKind::Missing ? tr(" (missing from library)") : tr(" (artwork or metadata differs)");
            form->addRow(conflict.embedded.id.value() + detail, choice);
            choices.append(choice);
        }
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        layout->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        if (dialog.exec() != QDialog::Accepted) return false;
        QVector<LibrarySymbol> updates;
        QStringList errors;
        for (int i = 0; i < choices.size(); ++i) {
            const auto &conflict = result.conflicts[i];
            const int choice = choices[i]->currentData().toInt();
            if (choice == 1) updates.append(conflict.embedded);
            if (choice == 2 && !result.document->useSymbol(*conflict.active, &errors)) break;
        }
        if (!errors.isEmpty() || !m_symbolPackageStore.saveEmbeddedBatch(updates, &errors, [this] { return maybeSave(); })) {
            if (!errors.isEmpty()) QMessageBox::warning(this, tr("Schematic"), errors.join('\n'));
            return false;
        }
        if (!updates.isEmpty()) rebuildSymbolDockContents();
    }
    if (result.conflicts.isEmpty() && !maybeSave()) return false;
    m_document = *result.document;
    m_scene->bindDocument(&m_document);
    m_editController->resetHistory();
    syncGridControls();
    m_currentFilePath = path;
    updateWindowTitle();

    if (!result.warnings.isEmpty())
        statusBar()->showMessage(result.warnings.join(QStringLiteral("; ")), 5000);
    rememberRecentFile(path);
    return true;
}

void MainWindow::onImportSymbol()
{
    SymbolImportDialog dialog(&m_symbolPackageStore, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    rebuildSymbolDockContents();
    statusBar()->showMessage(tr("Symbol imported into the library."), 5000);
}

bool MainWindow::writeDocumentTo(const QString &path)
{
    // QSaveFile writes to a temporary file alongside path and only
    // atomically renames it into place on a successful commit(), so a
    // write failure can never leave a partially-written or corrupted file
    // at path.
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, tr("Schematic"),
            tr("Could not open \"%1\" for writing: %2").arg(path, file.errorString()));
        return false;
    }
    const QByteArray bytes = m_document.toJsonBytes();
    const qint64 written = file.write(bytes);
    if (written != bytes.size()) {
        // A short or failed write must never be committed over the existing
        // file; discard the temporary file instead.
        const QString reason = file.errorString();
        file.cancelWriting();
        QMessageBox::warning(this, tr("Schematic"),
            tr("Could not save \"%1\": only %2 of %3 bytes could be written (%4)")
                .arg(path).arg(qMax<qint64>(written, 0)).arg(bytes.size()).arg(reason));
        return false;
    }
    if (!file.commit()) {
        QMessageBox::warning(this, tr("Schematic"),
            tr("Could not save \"%1\": %2").arg(path, file.errorString()));
        return false;
    }
    return true;
}

bool MainWindow::onSave()
{
    if (m_currentFilePath.isEmpty())
        return onSaveAs();

    if (!writeDocumentTo(m_currentFilePath))
        return false;
    rememberRecentFile(m_currentFilePath);
    m_editController->setClean();
    updateWindowTitle();
    return true;
}

bool MainWindow::onSaveAs()
{
    const QString path = QFileDialog::getSaveFileName(this, tr("Save Schematic As"), QString(),
                                                        tr("Schematic Files (*.schematic);;All Files (*)"));
    if (path.isEmpty())
        return false;

    // Only commit m_currentFilePath once the write actually succeeds - a
    // failed save must not change which file subsequent plain Save calls
    // target.
    if (!writeDocumentTo(path))
        return false;
    rememberRecentFile(path);
    m_currentFilePath = path;
    m_editController->setClean();
    updateWindowTitle();
    return true;
}

bool MainWindow::exportSvgTo(const QString &path)
{
    QString error;
    if (!SchematicSvgExporter::write(m_document, path, &error)) {
        QMessageBox::warning(this, tr("SVG Export"), tr("Could not export \"%1\": %2").arg(path, error));
        return false;
    }
    statusBar()->showMessage(tr("Exported SVG to %1").arg(path), 5000);
    return true;
}

void MainWindow::onSvgExport()
{
    const QString suggested = m_currentFilePath.isEmpty() ? tr("schematic.svg")
        : QFileInfo(m_currentFilePath).absolutePath() + QLatin1Char('/')
          + QFileInfo(m_currentFilePath).completeBaseName() + QStringLiteral(".svg");
    QFileDialog dialog(this, tr("SVG Export"));
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setNameFilter(tr("SVG Images (*.svg)"));
    dialog.setDefaultSuffix(QStringLiteral("svg"));
    dialog.selectFile(suggested);
    if (dialog.exec() == QDialog::Accepted && !dialog.selectedFiles().isEmpty())
        exportSvgTo(dialog.selectedFiles().first());
}

void MainWindow::onAbout()
{
    QMessageBox::about(this, tr("About Schematic"),
        tr("<h3>Schematic</h3>"
           "<p>A Qt-based schematic capture application focused on "
           "presentation rather than EDA.</p>"));
}

void MainWindow::onZoomFactorChanged(qreal factor)
{
    m_zoomLabel->setText(tr("Zoom: %1%").arg(qRound(factor * 100)));
}

void MainWindow::onMouseScenePositionChanged(const QPointF &scenePos)
{
    m_positionLabel->setText(tr("X: %1, Y: %2")
                                  .arg(scenePos.x(), 0, 'f', 1)
                                  .arg(scenePos.y(), 0, 'f', 1));
}

void MainWindow::onSceneStatusMessage(const QString &message)
{
    statusBar()->showMessage(message, 3000);
}

void MainWindow::onGridSpacingChanged(double spacing)
{
    const qreal previousSpacing = m_document.gridSpacing();
    if (qFuzzyCompare(previousSpacing, spacing))
        return;
    m_editController->commit(tr("Change grid spacing"), [this, spacing]() {
        return m_document.setGridSpacing(spacing);
    });
    m_view->setGridSpacing(m_document.gridSpacing());
}

void MainWindow::onSelectModeTriggered()
{
    m_scene->setEditMode(SchematicScene::EditMode::Select);
}

void MainWindow::onDrawWireModeTriggered()
{
    m_scene->setEditMode(SchematicScene::EditMode::DrawWire);
}

void MainWindow::onRotateSelectedComponent() { rotateSelectedComponent(90); }
void MainWindow::onRotateLeftSelectedComponent() { rotateSelectedComponent(-90); }
void MainWindow::onMirrorSelectedComponent() { flipSelectedComponent(FlipAxis::Horizontal); }
void MainWindow::onFlipVerticalSelectedComponent() { flipSelectedComponent(FlipAxis::Vertical); }

void MainWindow::rotateSelectedComponent(int degrees)
{
    m_scene->transformSelected(SelectionTransform::Kind::Rotate, degrees / 90);
}

void MainWindow::flipSelectedComponent(FlipAxis axis)
{
    m_scene->transformSelected(axis == FlipAxis::Horizontal
        ? SelectionTransform::Kind::FlipHorizontal : SelectionTransform::Kind::FlipVertical);
}

void MainWindow::onPlaceComponentKindChanged(const SymbolId &id)
{
    m_scene->setEditMode(SchematicScene::EditMode::PlaceComponent);
    m_scene->setPlaceComponentKind(id);
}

void MainWindow::onEditComponentLabels(ComponentId componentId)
{
    if (!m_document.component(componentId)) return;
    m_scene->cancelMove();
    m_scene->restoreSelection({{componentId}, {}});
    m_propertiesDock->show();
    m_propertiesDock->raise();
    m_propertiesPanel->focusProperty("value");
}

void MainWindow::onRenameSelectedComponent()
{
    if (!m_scene->singleSelectedComponent()) return;
    m_propertiesDock->show();
    m_propertiesDock->raise();
    m_propertiesPanel->focusProperty("reference");
}
void MainWindow::updateRenameActionEnabled()
{
    const bool oneComponentSelected = m_scene->singleSelectedComponent().has_value();
    const bool canTransform = !m_editTextTarget && !m_editNoteTextTarget && !m_scene->selection().isEmpty()
        && m_scene->editMode() == SchematicScene::EditMode::Select && !m_scene->moveInProgress();
    if (m_renameAction)
        m_renameAction->setEnabled(oneComponentSelected);
    if (m_rotateLeftAction) m_rotateLeftAction->setEnabled(canTransform);
    if (m_flipVerticalAction) m_flipVerticalAction->setEnabled(canTransform);
    if (m_rotateAction)
        m_rotateAction->setEnabled(canTransform);
    if (m_mirrorAction)
        m_mirrorAction->setEnabled(canTransform);
    updateEditActions();
}

namespace {
constexpr auto schematicClipboardMime = "application/x-schematic-fragment+json";
}

void MainWindow::updateEditActions()
{
    if (m_svgExportAction)
        m_svgExportAction->setEnabled(!m_document.components().isEmpty() || !m_document.wires().isEmpty() || !m_document.notes().isEmpty());
    if (!m_cutAction) return;
    const auto *mime = QApplication::clipboard()->mimeData();
    if (m_editNoteTextTarget) {
        const auto cursor = m_editNoteTextTarget->textCursor();
        const bool editable = !m_editNoteTextTarget->isReadOnly();
        m_undoAction->setEnabled(m_editNoteTextTarget->document()->isUndoAvailable());
        m_redoAction->setEnabled(m_editNoteTextTarget->document()->isRedoAvailable());
        m_copyAction->setEnabled(cursor.hasSelection()); m_cutAction->setEnabled(editable && cursor.hasSelection());
        m_pasteAction->setEnabled(editable && mime && mime->hasText());
        m_deleteAction->setEnabled(editable && (cursor.hasSelection() || !cursor.atEnd()));
        return;
    }
    m_undoAction->setEnabled(m_editTextTarget ? m_editTextTarget->isUndoAvailable() : m_editController->undoStack()->canUndo());
    m_redoAction->setEnabled(m_editTextTarget ? m_editTextTarget->isRedoAvailable() : m_editController->undoStack()->canRedo());
    if (m_editTextTarget) {
        const bool editable = !m_editTextTarget->isReadOnly();
        m_copyAction->setEnabled(m_editTextTarget->hasSelectedText());
        m_cutAction->setEnabled(editable && m_editTextTarget->hasSelectedText());
        m_pasteAction->setEnabled(editable && mime && mime->hasText());
        m_deleteAction->setEnabled(editable && (m_editTextTarget->hasSelectedText()
            || m_editTextTarget->cursorPosition() < m_editTextTarget->text().size()));
    } else {
        const bool ready = m_scene->editMode() == SchematicScene::EditMode::Select && !m_scene->moveInProgress();
        const bool selected = ready && !m_scene->selection().isEmpty();
        m_copyAction->setEnabled(selected); m_cutAction->setEnabled(selected);
        m_deleteAction->setEnabled(selected);
        m_pasteAction->setEnabled(ready && mime && mime->hasFormat(schematicClipboardMime));
    }
}

void MainWindow::clipboardEdit(const QString &operation)
{
    if (m_editNoteTextTarget) {
        if (operation == "copy") m_editNoteTextTarget->copy();
        else if (operation == "cut") m_editNoteTextTarget->cut();
        else if (operation == "paste") m_editNoteTextTarget->paste();
        else if (operation == "delete" && !m_editNoteTextTarget->isReadOnly()) {
            auto cursor = m_editNoteTextTarget->textCursor(); cursor.deleteChar(); m_editNoteTextTarget->setTextCursor(cursor);
        }
        updateEditActions(); return;
    }
    if (m_editTextTarget) {
        if (operation == "copy") m_editTextTarget->copy();
        else if (operation == "cut") m_editTextTarget->cut();
        else if (operation == "paste") m_editTextTarget->paste();
        else if (operation == "delete" && !m_editTextTarget->isReadOnly()) m_editTextTarget->del();
        updateEditActions(); return;
    }
    if (m_scene->editMode() != SchematicScene::EditMode::Select || m_scene->moveInProgress()) return;
    if (m_propertiesPanel) m_propertiesPanel->invalidate();
    if (operation == "delete") { m_scene->deleteSelection(); return; }
    auto *clipboard = QApplication::clipboard();
    const auto selection = m_scene->selection();
    QStringList errors;
    if (operation == "copy" || operation == "cut") {
        const auto bytes = m_document.copyFragment(selection.components, selection.wires, &errors, selection.notes);
        if (bytes.isEmpty()) { onSceneStatusMessage(errors.join('\n')); return; }
        auto *mime = new QMimeData; mime->setData(schematicClipboardMime, bytes); clipboard->setMimeData(mime);
        if (!clipboard->mimeData() || clipboard->mimeData()->data(schematicClipboardMime) != bytes) {
            onSceneStatusMessage(tr("Could not publish the schematic selection to the clipboard.")); return;
        }
        if (operation == "cut") m_editController->commit(tr("Cut selection"), [this,selection] {
            return m_document.removeSelection(selection.components, selection.wires, selection.notes);
        }, [] { return DocumentSelection{}; });
    } else if (operation == "paste") {
        const auto *mime = clipboard->mimeData();
        if (!mime || !mime->hasFormat(schematicClipboardMime)) return;
        const auto bytes = mime->data(schematicClipboardMime);
        const QPointF cursor = m_view->pastePosition();
        const qreal grid = m_document.gridSpacing();
        const QPointF target(std::round(cursor.x() / grid) * grid, std::round(cursor.y() / grid) * grid);
        DocumentSelection added;
        if (!m_editController->commit(tr("Paste selection"), [this,&added,&errors,bytes,target] {
            return m_document.pasteFragmentAt(bytes, target, &added.components, &added.wires, &errors, &added.notes);
        }, [&added] { return added; })) onSceneStatusMessage(errors.join('\n'));
    }
    updateEditActions();
}

void MainWindow::onPreferences()
{
    PreferencesDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        m_scene->setPropertyBlockGridSpacing(dialog.propertyBlockGridSpacing());
        m_view->setCadStyleMouseWheel(dialog.cadStyleMouseWheel());
        m_scene->setKeepPlacingSymbols(dialog.keepPlacingSymbols());
    }
}
