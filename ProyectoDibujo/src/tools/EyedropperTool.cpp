#include "tools/EyedropperTool.h"
#include "core/Document.h"
#include "tools/ToolManager.h"

#include <QtMath>

void EyedropperTool::mousePress(const ToolEvent& event, ToolContext& ctx) { sample(event, ctx); }
void EyedropperTool::mouseMove(const ToolEvent& event, ToolContext& ctx) { sample(event, ctx); }

void EyedropperTool::sample(const ToolEvent& event, ToolContext& ctx) {
    const QPoint p(qFloor(event.pos.x()), qFloor(event.pos.y()));
    const QColor picked = ctx.document.colorAt(p);
    if (picked.isValid()) ctx.manager.setColor(picked);
}
