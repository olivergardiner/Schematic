#include "documenteditcontroller.h"

#include <QUndoCommand>

#include <utility>

namespace {

class DocumentSnapshotCommand final : public QUndoCommand
{
public:
    DocumentSnapshotCommand(Document *document, Document before, Document after,
                            DocumentEditController::Selection beforeSelection,
                            DocumentEditController::Selection afterSelection,
                            DocumentEditController::StateRestored restored,
                            const QString &description)
        : m_document(document)
        , m_before(std::move(before))
        , m_after(std::move(after))
        , m_beforeSelection(std::move(beforeSelection))
        , m_afterSelection(std::move(afterSelection))
        , m_restored(std::move(restored))
    {
        setText(description);
    }

    void undo() override { restore(m_before, m_beforeSelection); }
    void redo() override { restore(m_after, m_afterSelection); }

private:
    void restore(const Document &snapshot,
                 const DocumentEditController::Selection &selection)
    {
        *m_document = snapshot;
        if (m_restored)
            m_restored(selection);
    }

    Document *m_document;
    Document m_before;
    Document m_after;
    DocumentEditController::Selection m_beforeSelection;
    DocumentEditController::Selection m_afterSelection;
    DocumentEditController::StateRestored m_restored;
};

} // namespace

DocumentEditController::DocumentEditController(Document *document, QObject *parent)
    : QObject(parent)
    , m_document(document)
{
}

bool DocumentEditController::commit(const QString &description,
                                    const EditOperation &operation,
                                    SelectionGetter afterSelectionGetter)
{
    if (!m_document || !operation)
        return false;

    const Document before = *m_document;
    const Selection beforeSelection = m_selectionGetter ? m_selectionGetter() : Selection{};
    if (!operation())
        return false;

    const Document after = *m_document;
    if (before.toJsonBytes() == after.toJsonBytes()) {
        *m_document = before;
        return false;
    }

    const Selection afterSelection = afterSelectionGetter
        ? afterSelectionGetter()
        : (m_selectionGetter ? m_selectionGetter() : Selection{});
    m_undoStack.push(new DocumentSnapshotCommand(m_document, before, after,
                                                 beforeSelection, afterSelection,
                                                 m_stateRestored, description));
    return true;
}

void DocumentEditController::setSelectionCallbacks(SelectionGetter getter,
                                                    StateRestored restored)
{
    m_selectionGetter = std::move(getter);
    m_stateRestored = std::move(restored);
}

void DocumentEditController::resetHistory()
{
    m_undoStack.clear();
    m_undoStack.setClean();
}
