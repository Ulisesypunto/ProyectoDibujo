#include "tools/LassoTool.h"
#include "core/Document.h"

#include <QLineF>
#include <QPainter>
#include <QPainterPath>
#include <QTransform>

void LassoTool::mousePress(const ToolEvent& event, ToolContext&) {
    m_active = true;
    m_mods = event.modifiers;
    m_points.clear();
    m_points << event.pos;
}

void LassoTool::mouseMove(const ToolEvent& event, ToolContext&) {
    if (!m_active) return;
    if (QLineF(m_points.last(), event.pos).length() >= 1.0) m_points << event.pos;
}

void LassoTool::mouseRelease(const ToolEvent&, ToolContext& ctx) {
    if (!m_active) return;
    m_active = false;
    const QPolygonF pts = m_points;
    m_points.clear();

    const bool add = m_mods & Qt::ShiftModifier;
    const bool sub = m_mods & Qt::AltModifier;

    if (pts.size() < 3) {
        if (!add && !sub) ctx.document.clearSelection(); // clic suelto = deseleccionar
        return;
    }

    QPainterPath lasso;
    lasso.setFillRule(Qt::WindingFill);
    lasso.addPolygon(pts);
    lasso.closeSubpath();

    const QPainterPath current = ctx.document.selection();
    QPainterPath result = lasso;
    if (add && sub) result = current.intersected(lasso);
    else if (sub) result = current.subtracted(lasso);
    else if (add && !current.isEmpty()) result = current.united(lasso);

    ctx.document.setSelection(result, QObject::tr("Lazo"));
}

void LassoTool::paintOverlay(QPainter& p, const QTransform& docToWidget) const {
    if (!m_active || m_points.size() < 2) return;
    const QPolygonF poly = docToWidget.map(m_points);
    p.setRenderHint(QPainter::Antialiasing, false);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(Qt::white, 1));
    p.drawPolyline(poly);
    p.setPen(QPen(Qt::black, 1, Qt::DashLine));
    p.drawPolyline(poly);
    p.drawLine(poly.first(), poly.last()); // linea de cierre
}