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
    connect(m_tools, &ToolManager::activeToolChanged, this, [this] { updateCursor(); update(); });
    connect(m_tools, &ToolManager::sizeChanged, this, [this] { update(); });
    updateCursor();
}

void CanvasView::setDocument(Document* document) {
    if (m_document) m_document->disconnect(this);
    m_document = document;
    m_drawing = false;
    if (m_document) {
        connect(m_document, &Document::contentChanged, this, &CanvasView::onContentChanged);
        connect(m_document, &Document::layerListChanged, this, [this] { update(); });
        connect(m_document, &Document::layerPropertiesChanged, this, [this] { update(); });
    }
    m_fitPending = true;
    applyPendingFit();
    update();
}

// ---------------- zoom / vista ----------------
void CanvasView::setZoom(qreal zoom, const QPointF& anchor) {
    zoom = qBound(MinZoom, zoom, MaxZoom);
    const QPointF docPoint = toDocument(anchor);
    m_zoom = zoom;
    m_origin = anchor - docPoint * m_zoom; // el punto bajo el cursor se queda quieto
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
                       (height() - doc.height() * m_zoom) / 2.0);
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

    const QRect docRect = m_document->rect();
    painter.save();
    painter.translate(m_origin);
    painter.scale(m_zoom, m_zoom);
    painter.fillRect(docRect, Qt::white); // papel

    const QRectF visible = painter.transform().inverted().mapRect(QRectF(event->rect()));
    const QRect area = visible.toAlignedRect().adjusted(-1, -1, 1, 1) & docRect;
    painter.setRenderHint(QPainter::SmoothPixmapTransform, m_zoom < 1.0); // pixeles nitidos al ampliar
    m_document->render(painter, area);
    painter.restore();

    // Borde del documento
    painter.setPen(QColor(20, 20, 20));
    painter.drawRect(QRectF(m_origin, QSizeF(docRect.size()) * m_zoom));

    // Circulo con el tamano del pincel
    Tool* tool = m_tools->activeTool();
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
    const QRectF w(toWidget(dirty.topLeft()), QSizeF(dirty.size()) * m_zoom);
    update(w.toAlignedRect().adjusted(-2, -2, 2, 2));
}

// ---------------- raton ----------------
static ToolEvent makeToolEvent(const QPointF& docPos, Qt::KeyboardModifiers mods) {
    ToolEvent e;
    e.pos = docPos;
    e.modifiers = mods;
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
        tool->mousePress(makeToolEvent(toDocument(event->position()), event->modifiers()), ctx);
    }
}

void CanvasView::mouseMoveEvent(QMouseEvent* event) {
    m_hover = event->position();
    m_hasHover = true;

    if (m_panning) {
        m_origin += event->position() - m_lastPanPos;
        m_lastPanPos = event->position();
        m_fitPending = false;
    } else if (m_drawing && m_document && (event->buttons() & Qt::LeftButton)) {
        if (Tool* tool = m_tools->activeTool()) {
            ToolContext ctx{*m_document, *m_tools};
            tool->mouseMove(makeToolEvent(toDocument(event->position()), event->modifiers()), ctx);
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
                tool->mouseRelease(makeToolEvent(toDocument(event->position()), event->modifiers()), ctx);
            }
        }
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
