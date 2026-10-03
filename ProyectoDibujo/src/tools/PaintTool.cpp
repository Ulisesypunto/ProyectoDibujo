#include "tools/PaintTool.h"
#include "core/Document.h"
#include "core/commands/StrokeCommand.h"

void PaintTool::mousePress(const ToolEvent& event, ToolContext& ctx) {
    Layer* layer = ctx.document.activeLayer();
    if (!layer || !layer->isVisible() || layer->isGroup()) return;
    if (usesColor()) ctx.manager.noteColorUsed();
    
    m_active = true;
    m_layerId = layer->id();
    m_before = layer->image().copy(); // copia real (evita dudas con la comparticion implicita)
    m_dirty = QRect();
    m_last = event.pos;
    drawSegment(m_last, m_last, ctx);
}

void PaintTool::mouseMove(const ToolEvent& event, ToolContext& ctx) {
    if (!m_active) return;
    drawSegment(m_last, event.pos, ctx);
    m_last = event.pos;
}

void PaintTool::mouseRelease(const ToolEvent&, ToolContext& ctx) {
    if (!m_active) return;
    m_active = false;

    Layer* layer = ctx.document.layerById(m_layerId);
    if (layer && !m_dirty.isEmpty()) {
        // Solo se guarda la region tocada: undo barato en memoria.
        ctx.document.undoStack()->push(new StrokeCommand(
            &ctx.document, m_layerId, m_dirty,
            m_before.copy(m_dirty), layer->image().copy(m_dirty), strokeName()));
    }
    m_before = QImage();
    m_dirty = QRect();
}

void PaintTool::drawSegment(const QPointF& from, const QPointF& to, ToolContext& ctx) {
    Layer* layer = ctx.document.layerById(m_layerId);
    if (!layer) return;

    {
        QPainter painter(&layer->image());
        painter.setRenderHint(QPainter::Antialiasing, true);
        if (ctx.document.hasSelection()) painter.setClipPath(ctx.document.selection());
        painter.setCompositionMode(compositionMode());
        painter.setPen(makePen(ctx));
        if (from == to) painter.drawPoint(from);
        else painter.drawLine(from, to);
    }

    const qreal r = ctx.manager.size() / 2.0 + 2.0;
    const QRect dirty = QRectF(from, to).normalized().adjusted(-r, -r, r, r)
                            .toAlignedRect() & layer->rect();
    m_dirty |= dirty;
    ctx.document.notifyContentChanged(dirty);
}
