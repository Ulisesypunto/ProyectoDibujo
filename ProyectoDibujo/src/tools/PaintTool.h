#pragma once
#include <QImage>
#include <QPainter>
#include <QPen>
#include <QRect>

#include "core/Layer.h"
#include "tools/Tool.h"
#include "tools/ToolManager.h"

// Base para herramientas que pintan pixeles en la capa activa (pincel, goma,
// futuros aerografo, lapiz...). Gestiona el ciclo del trazo y el historial:
// las subclases solo definen COMO es el lapiz.
class PaintTool : public Tool {
public:
    bool usesBrushSize() const override { return true; }

    void mousePress(const ToolEvent& event, ToolContext& ctx) final;
    void mouseMove(const ToolEvent& event, ToolContext& ctx) final;
    void mouseRelease(const ToolEvent& event, ToolContext& ctx) final;

protected:
    virtual QPen makePen(const ToolContext& ctx) const = 0;
    virtual QPainter::CompositionMode compositionMode() const { return QPainter::CompositionMode_SourceOver; }
    virtual QString strokeName() const = 0; // texto del historial
    virtual bool usesColor() const { return true; } // false en la goma

private:
    void drawSegment(const QPointF& from, const QPointF& to, ToolContext& ctx);

    bool m_active = false;
    Layer::Id m_layerId = Layer::InvalidId;
    QImage m_before; // copia de la capa al empezar el trazo
    QPointF m_last;
    QRect m_dirty;
};
