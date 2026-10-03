#include "core/commands/StrokeCommand.h"
#include "core/Document.h"

#include <QPainter>

StrokeCommand::StrokeCommand(Document* doc, Layer::Id layerId, const QRect& rect,
                             const QImage& before, const QImage& after, const QString& text)
    : m_doc(doc), m_layerId(layerId), m_rect(rect), m_before(before), m_after(after) {
    setText(text);
}

void StrokeCommand::redo() {
    if (m_firstRedo) { m_firstRedo = false; return; }
    apply(m_after);
}

void StrokeCommand::apply(const QImage& region) {
    Layer* layer = m_doc->layerById(m_layerId);
    if (!layer) return;
    QPainter painter(&layer->image());
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.drawImage(m_rect.topLeft(), region);
    painter.end();
    m_doc->notifyContentChanged(m_rect);
}
