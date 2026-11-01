#ifndef SYMBOLIMPORTDIALOG_H
#define SYMBOLIMPORTDIALOG_H

#include "core/symbollibrary.h"

#include <QDialog>

class QCheckBox;
class QDoubleSpinBox;
class QLineEdit;
class QListWidget;
class QPushButton;
class QTableWidget;
class SymbolPackageStore;
class SymbolPreviewWidget;

class SymbolImportDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SymbolImportDialog(SymbolPackageStore *store, QWidget *parent = nullptr);

private slots:
    void chooseSource();
    void updateOrigin();
    void terminalAdded(const QPointF &sourcePosition);
    void terminalMoved(int index, const QPointF &sourcePosition);
    void terminalRemoved(int index);
    void terminalTableChanged(int row, int column);
    void moveTerminalUp();
    void moveTerminalDown();
    void importSymbol();

private:
    void loadSource(const QString &path);
    void updatePreview();
    void updateTerminalTable();
    void updateMessages();
    void updateImportEnabled();
    QByteArray makeFinalSidecar() const;

    SymbolPackageStore *m_store = nullptr;
    QLineEdit *m_sourceEdit = nullptr;
    QLineEdit *m_idEdit = nullptr;
    QLineEdit *m_nameEdit = nullptr;
    QLineEdit *m_categoryEdit = nullptr;
    QLineEdit *m_prefixEdit = nullptr;
    QCheckBox *m_showsReferenceCheck = nullptr;
    QDoubleSpinBox *m_originX = nullptr;
    QDoubleSpinBox *m_originY = nullptr;
    QTableWidget *m_terminalTable = nullptr;
    QListWidget *m_messages = nullptr;
    SymbolPreviewWidget *m_preview = nullptr;
    QVector<SymbolPrimitive> m_primitives;
    QVector<SymbolTerminal> m_rawTerminals;
    QPointF m_origin;
    QByteArray m_sourceSvg;
    QByteArray m_sourceSidecar;
    QString m_sourceSidecarPath;
    QStringList m_importerWarnings;
    QStringList m_importerErrors;
    bool m_updatingTable = false;
    QPushButton *m_importButton = nullptr;
};

#endif // SYMBOLIMPORTDIALOG_H
