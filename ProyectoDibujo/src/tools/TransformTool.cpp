#include "tools/TransformTool.h"
#include "core/Document.h"
#include "core/commands/SelectionCommand.h"
#include "core/commands/StrokeCommand.h"
#include "tools/ToolManager.h"

#include <QLineF>
#include <QPainter>
#include <QPolygonF>
#include <QUndoStack>
#include <cmath>

namespace {
constexpr qreal Pi = 3.14159265358979323846;

qreal clampScale(qreal s) {
    if (std::fabs(s) < 0.01) s = s < 0 ? -0.01 : 0.01;
    return qBound<qreal>(-64.0, s, 64.0);
}

qreal ratio(qreal cur, qreal start, qreal anchor) {
    const qreal d = start - anchor;
    return std::fabs(d) < 1e-6 ? 1.0 : (cur - anchor) / d;
}

Qt::CursorShape handleCursor(int h, bool shear) {
    if (shear) return (h == 4 || h == 6) ? Qt::SizeHorCursor : Qt::SizeVerCursor;
    switch (h) {
    case 0: case 2: return Qt::SizeFDiagCursor;
    case 1: case 3: return Qt::SizeBDiagCursor;
    case 4: case 6: return Qt::SizeVerCursor;
    default:        return Qt::SizeHorCursor;
    }
}
} // namespace

// ---------------- geometria ----------------
QPointF TransformTool::handleUnit(int i) {
    static const QPointF u[8] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}, {0.5, 0}, {1, 0.5}, {0.5, 1}, {0, 0.5}};
    return u[i];
}

QPointF TransformTool::boxPoint(const QPointF& u) const {
    return m_box.topLeft() + QPointF(u.x() * m_box.width(), u.y() * m_box.height());
}

TransformTool::Hit TransformTool::hitTest(const QPointF& pos, qreal viewScale,
                                          Qt::KeyboardModifiers mods) const {
    const qreal tol = 8.0 / qMax<qreal>(viewScale, 1e-6);
    if (QLineF(pos, m_pivot).length() <= tol * 1.3) return {Mode::Pivot, -1};
    for (int i = 0; i < 8; ++i) {
        if (QLineF(pos, handlePos(i)).length() > tol) continue;
        const bool shear = i >= 4 && (mods & Qt::ControlModifier);
        return {shear ? Mode::Shear : Mode::Scale, i};
    }
    if (m_matrix.map(QPolygonF(m_box)).containsPoint(pos, Qt::OddEvenFill)) return {Mode::Move, -1};
    return {Mode::Rotate, -1};
}

// ---------------- ciclo de vida ----------------
void TransformTool::clearState() {
    m_valid = m_floating = m_changed = false;
    m_mode = Mode::None;
    m_matrix = QTransform();
    m_before = QImage();
    m_lifted = QImage();
    m_selBefore = m_selNow = QPainterPath();
    m_layerId = Layer::InvalidId;
}

void TransformTool::resetBox(ToolContext& ctx) {
    clearState();
    Document& doc = ctx.document;
    Layer* layer = doc.activeLayer();
    if (!layer || !layer->isVisible() || layer->isGroup()) return;

    m_hasSel = doc.hasSelection();
    const QRect r = m_hasSel ? (doc.selection().boundingRect().toAlignedRect() & layer->rect())
                             : layer->contentBounds();
    if (r.isEmpty()) return;

    m_layerId = layer->id();
    m_liftRect = r;
    m_box = QRectF(r);
    m_pivot = m_box.center();
    m_selBefore = doc.selection();
    m_valid = true;
}

bool TransformTool::beginFloating(ToolContext& ctx) {
    Layer* layer = ctx.document.layerById(m_layerId);
    if (!layer) return false;
    m_before = layer->image().copy();
    m_lifted = QImage(m_liftRect.size(), QImage::Format_ARGB32_Premultiplied);
    m_lifted.fill(Qt::transparent);
    {
        QPainter p(&m_lifted);
        p.translate(-m_liftRect.topLeft());
        if (m_hasSel) p.setClipPath(m_selBefore);
        p.drawImage(m_liftRect.topLeft(), m_before, m_liftRect);
    }
    m_dirty = m_liftRect;
    m_selNow = m_selBefore;
    m_floating = true;
    return true;
}

void TransformTool::activated(ToolContext& ctx) { resetBox(ctx); }

void TransformTool::deactivated(ToolContext& ctx) {
    commit(ctx);
    clearState();
}

void TransformTool::contextChanged(ToolContext& ctx) {
    if (m_busy || m_mode != Mode::None) return;
    if (m_floating) {
        // Si cambio la capa activa o alguien toco la seleccion, aplicamos lo pendiente.
        const bool sameLayer = ctx.document.activeLayerId() == m_layerId;
        const bool sameSel = !m_hasSel || ctx.document.selection() == m_selNow;
        if (!sameLayer || !sameSel) commit(ctx);
        return;
    }
    resetBox(ctx); // seleccion / pixeles / capa distintos: recalcular la caja
}

void TransformTool::commit(ToolContext& ctx) {
    if (m_floating && m_changed) {
        Document& doc = ctx.document;
        if (Layer* layer = doc.layerById(m_layerId)) {
            m_busy = true;
            const QRect r = m_dirty & layer->rect();
            QUndoStack* stack = doc.undoStack();
            stack->beginMacro(displayName()); // pixeles + seleccion = un solo paso
            stack->push(new StrokeCommand(&doc, m_layerId, r, m_before.copy(r),
                                          layer->image().copy(r), displayName()));
            if (m_hasSel && doc.selection() == m_selNow)
                stack->push(new SelectionCommand(&doc, m_selBefore, m_selNow, displayName()));
            stack->endMacro();
            m_busy = false;
        }
    }
    resetBox(ctx);
}

void TransformTool::cancel(ToolContext& ctx) {
    if (m_floating && m_changed) {
        if (Layer* layer = ctx.document.layerById(m_layerId)) {
            m_busy = true;
            {
                QPainter p(&layer->image());
                p.setCompositionMode(QPainter::CompositionMode_Source);
                p.drawImage(m_dirty.topLeft(), m_before, m_dirty);
            }
            if (m_hasSel) ctx.document.applySelection(m_selBefore);
            ctx.document.notifyContentChanged(m_dirty);
            m_busy = false;
        }
    }
    resetBox(ctx);
}

bool TransformTool::keyPress(int key, ToolContext& ctx) {
    if (!m_floating) return false;
    if (key == Qt::Key_Return || key == Qt::Key_Enter) { commit(ctx); return true; }
    if (key == Qt::Key_Escape) { cancel(ctx); return true; }
    return false;
}

// ---------------- raton ----------------
void TransformTool::hover(const ToolEvent& event, ToolContext&) {
    if (!m_valid) { m_cursor = Qt::ArrowCursor; return; }
    const Hit h = hitTest(event.pos, event.viewScale, event.modifiers);
    switch (h.mode) {
    case Mode::Move:   m_cursor = Qt::SizeAllCursor; break;
    case Mode::Pivot:  m_cursor = Qt::PointingHandCursor; break;
    case Mode::Scale:  m_cursor = handleCursor(h.handle, false); break;
    case Mode::Shear:  m_cursor = handleCursor(h.handle, true); break;
    default:           m_cursor = Qt::CrossCursor; break; // rotar
    }
}

void TransformTool::mousePress(const ToolEvent& event, ToolContext& ctx) {
    m_mode = Mode::None;
    if (!m_valid) return;
    const Hit hit = hitTest(event.pos, event.viewScale, event.modifiers);
    if (hit.mode != Mode::Pivot && !m_floating && !beginFloating(ctx)) return;
    m_mode = hit.mode;
    m_handle = hit.handle;
    m_press = event.pos;
    m_matrix0 = m_matrix;
    m_pivot0 = m_pivot;
}

void TransformTool::mouseMove(const ToolEvent& event, ToolContext& ctx) {
    if (m_mode == Mode::None) return;
    const QPointF P = event.pos;
    const Qt::KeyboardModifiers mods = event.modifiers;
    QTransform newM = m_matrix0;

    switch (m_mode) {
    case Mode::Pivot:
        m_pivot = P;
        return;

    case Mode::Move: {
        qreal dx = std::round(P.x() - m_press.x()), dy = std::round(P.y() - m_press.y());
        if (mods & Qt::ShiftModifier) { if (std::fabs(dx) >= std::fabs(dy)) dy = 0; else dx = 0; }
        newM = m_matrix0 * QTransform::fromTranslate(dx, dy);
        break;
    }

    case Mode::Rotate: {
        const qreal a0 = std::atan2(m_press.y() - m_pivot0.y(), m_press.x() - m_pivot0.x());
        const qreal a1 = std::atan2(P.y() - m_pivot0.y(), P.x() - m_pivot0.x());
        qreal deg = (a1 - a0) * 180.0 / Pi;
        if (mods & Qt::ShiftModifier) deg = std::round(deg / 15.0) * 15.0;
        const QTransform D = QTransform().translate(m_pivot0.x(), m_pivot0.y()).rotate(deg)
                                         .translate(-m_pivot0.x(), -m_pivot0.y());
        newM = m_matrix0 * D;
        break;
    }

    case Mode::Scale: {
        // Se trabaja en el espacio de la caja (antes de m_matrix0): escala a lo largo de sus ejes.
        const QTransform inv0 = m_matrix0.inverted();
        const QPointF q0 = inv0.map(m_press), q = inv0.map(P);
        const QPointF u = handleUnit(m_handle);
        const QPointF anchorU = (mods & Qt::AltModifier) ? QPointF(0.5, 0.5)
                                                         : QPointF(1.0 - u.x(), 1.0 - u.y());
        const QPointF a = boxPoint(anchorU);
        qreal sx = (u.x() == 0.5) ? 1.0 : ratio(q.x(), q0.x(), a.x());
        qreal sy = (u.y() == 0.5) ? 1.0 : ratio(q.y(), q0.y(), a.y());
        if (mods & Qt::ShiftModifier) { // proporcional
            if (u.x() == 0.5) sx = sy;
            else if (u.y() == 0.5) sy = sx;
            else sx = sy = (std::fabs(sx) >= std::fabs(sy)) ? sx : sy;
        }
        const QTransform S = QTransform().translate(a.x(), a.y())
                                         .scale(clampScale(sx), clampScale(sy))
                                         .translate(-a.x(), -a.y());
        newM = S * m_matrix0;
        break;
    }

    case Mode::Shear: {
        const QTransform inv0 = m_matrix0.inverted();
        const QPointF q0 = inv0.map(m_press), q = inv0.map(P);
        const QPointF u = handleUnit(m_handle);
        const QPointF h = boxPoint(u), a = boxPoint(QPointF(1.0 - u.x(), 1.0 - u.y()));
        QTransform S;
        if (u.x() == 0.5) { // lado superior/inferior: desplaza en X
            const qreal dist = h.y() - a.y();
            if (std::fabs(dist) < 1e-6) return;
            const qreal k = (q.x() - q0.x()) / dist;
            S = QTransform(1, 0, k, 1, -k * a.y(), 0);
        } else {            // lado izquierdo/derecho: desplaza en Y
            const qreal dist = h.x() - a.x();
            if (std::fabs(dist) < 1e-6) return;
            const qreal k = (q.y() - q0.y()) / dist;
            S = QTransform(1, k, 0, 1, 0, -k * a.x());
        }
        newM = S * m_matrix0;
        break;
    }
    default:
        return;
    }

    if (!newM.isInvertible()) return;
    // El eje de rotacion acompana al contenido (salvo cuando lo arrastras tu)
    m_pivot = newM.map(m_matrix0.inverted().map(m_pivot0));
    m_matrix = newM;
    render(ctx);
}

void TransformTool::mouseRelease(const ToolEvent&, ToolContext&) { m_mode = Mode::None; }

// ---------------- pixeles ----------------
void TransformTool::render(ToolContext& ctx) {
    Layer* layer = ctx.document.layerById(m_layerId);
    if (!layer) return;
    m_busy = true;

    const QRect dest = m_matrix.mapRect(QRectF(m_liftRect)).toAlignedRect().adjusted(-2, -2, 2, 2)
                       & layer->rect();
    m_dirty |= dest;
    {
        QPainter p(&layer->image());
        // 1) restaurar la zona desde el original
        p.setCompositionMode(QPainter::CompositionMode_Source);
        p.drawImage(m_dirty.topLeft(), m_before, m_dirty);
        // 2) vaciar el hueco que deja lo levantado
        if (m_hasSel) p.setClipPath(m_selBefore);
        p.setCompositionMode(QPainter::CompositionMode_Clear);
        p.fillRect(m_liftRect, Qt::transparent);
        // 3) dibujar lo levantado, transformado
        p.setClipRect(m_dirty);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        p.setTransform(m_matrix);
        p.drawImage(m_liftRect.topLeft(), m_lifted);
    }
    if (m_hasSel) { // la seleccion acompana (sin undo hasta confirmar)
        m_selNow = m_matrix.map(m_selBefore);
        ctx.document.applySelection(m_selNow);
    }
    m_changed = true;
    ctx.document.notifyContentChanged(m_dirty);
    m_busy = false;
}

// ---------------- superposicion ----------------
void TransformTool::paintOverlay(QPainter& painter, const QTransform& docToWidget) const {
    if (!m_valid) return;
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setBrush(Qt::NoBrush);

    const QPolygonF poly = docToWidget.map(m_matrix.map(QPolygonF(m_box)));
    painter.setPen(QPen(Qt::white, 1));
    painter.drawPolygon(poly);
    painter.setPen(QPen(Qt::black, 1, Qt::DashLine));
    painter.drawPolygon(poly);

    painter.setPen(QPen(Qt::black, 1));
    painter.setBrush(Qt::white);
    for (int i = 0; i < 8; ++i) {
        const QPointF c = docToWidget.map(handlePos(i));
        painter.drawRect(QRectF(c.x() - 4, c.y() - 4, 8, 8));
    }

    // Eje de rotacion
    const QPointF pc = docToWidget.map(m_pivot);
    painter.setBrush(QColor(255, 255, 255, 90));
    painter.setPen(QPen(Qt::black, 1.5));
    painter.drawEllipse(pc, 7, 7);
    painter.drawLine(QPointF(pc.x() - 10, pc.y()), QPointF(pc.x() + 10, pc.y()));
    painter.drawLine(QPointF(pc.x(), pc.y() - 10), QPointF(pc.x(), pc.y() + 10));
    painter.restore();
}