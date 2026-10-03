#pragma once
#include <QPainterPath>
#include <QString>
#include <QUndoCommand>

class Document;

class SelectionCommand : public QUndoCommand {
public:
    SelectionCommand(Document* doc, const QPainterPath& before, const QPainterPath& after,
                     const QString& text);
    void undo() override;
    void redo() override;

private:
    Document* m_doc;
    QPainterPath m_before;
    QPainterPath m_after;
};