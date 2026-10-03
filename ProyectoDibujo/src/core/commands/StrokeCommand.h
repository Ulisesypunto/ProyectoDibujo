#pragma once
#include <QImage>
#include <QRect>
#include <QUndoCommand>

#include "core/Layer.h"

class Document;

// Guarda SOLO la region modificada (antes/despues), no toda la capa.
// Sustituye al antiguo "copiar todas las capas en cada trazo".
class StrokeCommand : public QUndoCommand {
public:
    StrokeCommand(Document* doc, Layer::Id layerId, const QRect& rect,
                  const QImage& before, const QImage& after, const QString& text);
    void undo() override { apply(m_before); }
    void redo() override;

private:
    void apply(const QImage& region);

    Document* m_doc;
    Layer::Id m_layerId;
    QRect m_rect;
    QImage m_before;
    QImage m_after;
    bool m_firstRedo = true; // el trazo ya esta dibujado cuando se hace push
};
