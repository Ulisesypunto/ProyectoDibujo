#include "ui/CanvasView.h"
#include "core/Document.h"
#include "tools/ToolManager.h"

#include <QEnterEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>
#include <cmath>

CanvasView::CanvasView(ToolManager* tools, QWidget* parent) : QWidget(parent), m_tools(tools) {
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_OpaquePaintEvent);
    m_hookedTool = m_tools->activeTool();
    connect(m_tools, &ToolManager::activeToolChanged, this, [this](Tool* tool) {
        switchToolHooks(tool);
        updateCursor();
        update();
    });
    connect(m_tools, &ToolManager::sizeChanged, this, [this] { update(); });
    updateCursor();
}

void CanvasView::setDocument(Document* document) {
    if (m_document && m_hookedTool) { // aplicar lo pendiente en el documento que sale
        ToolContext ctx{*m_document, *m_tools};
        m_hookedTool->deactivated(ctx);
    }
    if (m_document) m_document->disconnect(this);
    m_document = document;
    m_drawing = false;
    if (m_document) {
        connect(m_document, &Document::contentChanged, this, &CanvasView::onContentChanged);
        connect(m_document, &Document::layerListChanged, this, [this] { update(); });
        connect(m_document, &Document::layerPropertiesChanged, this, [this] { update(); });
        connect(m_document, &Document::selectionChanged, this, [this] {
            update();
            scheduleToolContextChanged();
        });
        connect(m_document, &Document::activeLayerChanged, this, [this] { scheduleToolContextChanged(); });
        if (m_hookedTool) {
            ToolContext ctx{*m_document, *m_tools};
            m_hookedTool->activated(ctx);
        }
    }
    m_fitPending = true;
    applyPendingFit();
    update();
}

// ---------------- transformacion de vista ----------------
QTransform CanvasView::viewTransform() const {
    QTransform t;
    if (m_mirrored) { t.translate(width(), 0); t.scale(-1, 1); }
    t.translate(m_origin.x(), m_origin.y());
    t.scale(m_zoom, m_zoom);
    return t;
}

QPointF CanvasView::toWidget(const QPointF& docPos) const { return viewTransform().map(docPos); }
QPointF CanvasView::toDocument(const QPointF& widgetPos) const {
    return viewTransform().inverted().map(widgetPos);
}

void CanvasView::shiftOrigin(const QPointF& d) {
    m_origin += QPointF(m_mirrored ? -d.x() : d.x(), d.y());
}

void CanvasView::setMirrored(bool mirrored) {
    if (m_mirrored == mirrored) return;
    m_mirrored = mirrored;
    update();
}

// ---------------- zoom / vista ----------------
void CanvasView::setZoom(qreal zoom, const QPointF& anchor) {
    zoom = qBound(MinZoom, zoom, MaxZoom);
    const QPointF docPoint = toDocument(anchor);
    m_zoom = zoom;
    shiftOrigin(anchor - toWidget(docPoint)); // el punto bajo el cursor se queda quieto
    m_fitPending = false;
    update();
    emit zoomChanged(m_zoom);
}

void CanvasView::zoomIn() { setZoom(m_zoom * 1.25, QPointF(rect().center())); }
void CanvasView::zoomOut() { setZoom(m_zoom / 1.25, QPointF(rect().center())); }

void CanvasView::zoomActualSize() { setZoom(1.0, QPointF(rect().center())); }

void CanvasView::zoomToFit() {
    if (!m_document) return;
    const qreal margin = 24;
    const QSizeF avail(width() - 2 * margin, height() - 2 * margin);
    const QSize doc = m_document->size();
    qreal z = qMin(avail.width() / doc.width(), avail.height() / doc.height());
    m_zoom = qBound(MinZoom, z, MaxZoom);
    m_origin = QPointF((width() - doc.width() * m_zoom) / 2.0,
                       (height() - doc.height() * m_zoom) / 2.0); // simetrico: vale con espejo
    m_fitPending = false;
    update();
    emit zoomChanged(m_zoom);
}

void CanvasView::applyPendingFit() {
    if (m_fitPending && m_document && isVisible() && width() > 50 && height() > 50) zoomToFit();
}

void CanvasView::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    applyPendingFit();
}

void CanvasView::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    applyPendingFit();
}

// ---------------- pintado ----------------
void CanvasView::paintEvent(QPaintEvent* event) {
    QPainter painter(this);
    painter.fillRect(event->rect(), QColor(58, 58, 62)); // fondo del area de trabajo
    if (!m_document) return;

    const QTransform view = viewTransform();
    const QRect docRect = m_document->rect();
    painter.save();
    painter.setTransform(view);
    painter.fillRect(docRect, Qt::white); // papel

    const QRectF visible = view.inverted().mapRect(QRectF(event->rect()));
    const QRect area = visible.toAlignedRect().adjusted(-1, -1, 1, 1) & docRect;
    painter.setRenderHint(QPainter::SmoothPixmapTransform, m_zoom < 1.0); // pixeles nitidos al ampliar
    m_document->render(painter, area);
    painter.restore();

    // Borde del documento
    painter.setPen(QColor(20, 20, 20));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(view.mapRect(QRectF(docRect)));

    // Contorno de la seleccion (blanco + negro discontinuo: visible sobre cualquier color)
    if (m_document->hasSelection()) {
        const QPainterPath outline = view.map(m_document->selection());
        painter.setRenderHint(QPainter::Antialiasing, false);
        painter.setPen(QPen(Qt::white, 1));
        painter.drawPath(outline);
        painter.setPen(QPen(Qt::black, 1, Qt::DashLine));
        painter.drawPath(outline);
    }

    Tool* tool = m_tools->activeTool();
    if (tool) tool->paintOverlay(painter, view); // vista previa del lazo, etc.

    // Circulo con el tamano del pincel (m_hover esta en coordenadas de widget: no se espeja)
    if (m_hasHover && !m_panning && !m_spaceDown && tool && tool->usesBrushSize()) {
        painter.setRenderHint(QPainter::Antialiasing, true);
        const qreal r = qMax<qreal>(1.0, m_tools->size() * m_zoom / 2.0);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(Qt::white, 1));
        painter.drawEllipse(m_hover, r + 1, r + 1);
        painter.setPen(QPen(Qt::black, 1));
        painter.drawEllipse(m_hover, r, r);
    }
}

void CanvasView::onContentChanged(const QRect& dirty) {
    const QRectF w = viewTransform().mapRect(QRectF(dirty));
    update(w.toAlignedRect().adjusted(-2, -2, 2, 2));
    scheduleToolContextChanged();
}

// ---------------- raton ----------------
static ToolEvent makeToolEvent(const QPointF& docPos, Qt::KeyboardModifiers mods, qreal viewScale) {
    ToolEvent e;
    e.pos = docPos;
    e.modifiers = mods;
    e.viewScale = viewScale;
    return e;
}

void CanvasView::mousePressEvent(QMouseEvent* event) {
    if (!m_document) return;

    const bool panButton = event->button() == Qt::MiddleButton ||
                           (event->button() == Qt::LeftButton && m_spaceDown);
    if (panButton) {
        m_panning = true;
        m_lastPanPos = event->position();
        updateCursor();
        return;
    }

    Tool* tool = m_tools->activeTool();
    if (event->button() == Qt::LeftButton && tool) {
        m_drawing = true;
        ToolContext ctx{*m_document, *m_tools};
        tool->mousePress(makeToolEvent(toDocument(event->position()), event->modifiers(), m_zoom), ctx);
        update();
    }
}

void CanvasView::mouseMoveEvent(QMouseEvent* event) {
    m_hover = event->position();
    m_hasHover = true;

    if (m_panning) {
        shiftOrigin(event->position() - m_lastPanPos);
        m_lastPanPos = event->position();
        m_fitPending = false;
    } else if (m_drawing && m_document && (event->buttons() & Qt::LeftButton)) {
        if (Tool* tool = m_tools->activeTool()) {
            ToolContext ctx{*m_document, *m_tools};
            tool->mouseMove(makeToolEvent(toDocument(event->position()), event->modifiers(), m_zoom), ctx);
        }
    } else if (m_document && !m_drawing) {
        if (Tool* tool = m_tools->activeTool()) {
            ToolContext ctx{*m_document, *m_tools};
            tool->hover(makeToolEvent(toDocument(event->position()), event->modifiers(), m_zoom), ctx);
            updateCursor();
        }
    }
    update();
}

void CanvasView::mouseReleaseEvent(QMouseEvent* event) {
    if (m_panning && (event->button() == Qt::MiddleButton || event->button() == Qt::LeftButton)) {
        m_panning = false;
        updateCursor();
        return;
    }
    if (m_drawing && event->button() == Qt::LeftButton) {
        m_drawing = false;
        if (m_document) {
            if (Tool* tool = m_tools->activeTool()) {
                ToolContext ctx{*m_document, *m_tools};
                tool->mouseRelease(makeToolEvent(toDocument(event->position()), event->modifiers(), m_zoom), ctx);
            }
        }
        update(); // limpia la vista previa del lazo
    }
}

void CanvasView::wheelEvent(QWheelEvent* event) {
    if (!m_document) return;
    const qreal factor = std::pow(1.0015, event->angleDelta().y());
    setZoom(m_zoom * factor, event->position());
    event->accept();
}

// ---------------- teclado / cursor ----------------
void CanvasView::keyPressEvent(QKeyEvent* event) {
    if (m_document) {
        if (Tool* tool = m_tools->activeTool()) {
            ToolContext ctx{*m_document, *m_tools};
            if (tool->keyPress(event->key(), ctx)) { update(); return; }
        }
    }
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        m_spaceDown = true;
        updateCursor();
        update();
        return;
    }
    QWidget::keyPressEvent(event);
}

void CanvasView::keyReleaseEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        m_spaceDown = false;
        updateCursor();
        update();
        return;
    }
    QWidget::keyReleaseEvent(event);
}

void CanvasView::enterEvent(QEnterEvent* event) {
    setFocus(); // para que Espacio (mano) funcione sin hacer click antes
    QWidget::enterEvent(event);
}

void CanvasView::leaveEvent(QEvent* event) {
    m_hasHover = false;
    update();
    QWidget::leaveEvent(event);
}

void CanvasView::updateCursor() {
    if (m_panning) setCursor(Qt::ClosedHandCursor);
    else if (m_spaceDown) setCursor(Qt::OpenHandCursor);
    else if (Tool* tool = m_tools->activeTool()) setCursor(tool->cursor());
    else setCursor(Qt::ArrowCursor);
}

// ---------------- ganchos de herramienta ----------------
void CanvasView::switchToolHooks(Tool* now) {
    if (m_document) {
        ToolContext ctx{*m_document, *m_tools};
        if (m_hookedTool && m_hookedTool != now) m_hookedTool->deactivated(ctx);
        if (now && now != m_hookedTool) now->activated(ctx);
    }
    m_hookedTool = now;
}

void CanvasView::commitTool() {
    if (!m_document) return;
    if (Tool* tool = m_tools->activeTool()) {
        ToolContext ctx{*m_document, *m_tools};
        tool->commit(ctx);
        update();
    }
}

// En cola: asi nunca se hace un push al undo dentro del redo de otro comando.
void CanvasView::scheduleToolContextChanged() {
    if (m_contextPending) return;
    m_contextPending = true;
    QMetaObject::invokeMethod(this, [this] {
        m_contextPending = false;
        notifyToolContextChanged();
    }, Qt::QueuedConnection);
}

void CanvasView::notifyToolContextChanged() {
    if (!m_document) return;
    if (Tool* tool = m_tools->activeTool()) {
        ToolContext ctx{*m_document, *m_tools};
        tool->contextChanged(ctx);
        update();
    }
}