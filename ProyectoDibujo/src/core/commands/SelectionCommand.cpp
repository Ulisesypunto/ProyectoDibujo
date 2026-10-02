#include "core/commands/SelectionCommand.h"
#include "core/Document.h"

SelectionCommand::SelectionCommand(Document* doc, const QPainterPath& before,
                                   const QPainterPath& after, const QString& text)
    : m_doc(doc), m_before(before), m_after(after) {
    setText(text);
}

void SelectionCommand::undo() { m_doc->applySelection(m_before); }
void SelectionCommand::redo() { m_doc->applySelection(m_after); }