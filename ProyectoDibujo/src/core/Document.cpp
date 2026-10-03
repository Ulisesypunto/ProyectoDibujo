#include "core/Document.h"
#include "core/commands/LayerCommands.h"
#include "core/commands/SelectionCommand.h"

#include <QPainter>

Document::Document(const QSize& size, QObject* parent)
    : QObject(parent), m_size(size.expandedTo(QSize(1, 1))), m_undoStack(this) {
    m_undoStack.setUndoLimit(200);

    // Capa inicial (no forma parte del historial)
    const Layer::Id id = allocateLayerId();
    m_layers.push_back(std::make_unique<Layer>(id, tr("Capa %1").arg(id), m_size));
    m_activeLayerId = id;
}

std::unique_ptr<Document> Document::fromImage(const QImage& image) {
    auto doc = std::make_unique<Document>(image.size());
    doc->layerAt(0)->image() = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    doc->layerAt(0)->setName(tr("Imagen"));
    return doc;
}

Layer* Document::layerAt(int index) const {
    if (index < 0 || index >= layerCount()) return nullptr;
    return m_layers[size_t(index)].get();
}

int Document::indexOfLayer(Layer::Id id) const {
    for (int i = 0; i < layerCount(); ++i)
        if (m_layers[size_t(i)]->id() == id) return i;
    return -1;
}

Layer* Document::layerById(Layer::Id id) const { return layerAt(indexOfLayer(id)); }

void Document::setActiveLayer(Layer::Id id) {
    if (id == m_activeLayerId || !layerById(id)) return;
    m_activeLayerId = id;
    emit activeLayerChanged(id);
}

// ---------------- operaciones con historial ----------------
void Document::addLayer(const QString& name) {
    const QString finalName = name.isEmpty() ? tr("Capa %1").arg(m_nextLayerId) : name;
    m_undoStack.push(new AddLayerCommand(this, finalName));
}

void Document::removeLayer(Layer::Id id) {
    if (!layerById(id)) return;
    int remainingPaint = 0; // siempre debe quedar al menos 1 capa de pintura
    for (const auto& l : m_layers)
        if (!l->isGroup() && l->id() != id && !isDescendantOf(l->id(), id)) ++remainingPaint;
    if (remainingPaint == 0) return;
    m_undoStack.push(new RemoveLayerCommand(this, id));
}

void Document::addGroup(const QString& name) {
    const QString n = name.isEmpty() ? tr("Carpeta %1").arg(m_nextLayerId) : name;
    m_undoStack.push(new AddLayerCommand(this, std::make_unique<Layer>(allocateLayerId(), n, m_size, true)));
}

void Document::setLayerClipped(Layer::Id id, bool clipped) {
    Layer* l = layerById(id);
    if (!l || l->isClipped() == clipped) return;
    m_undoStack.push(new LayerPropertyCommand(this, id, LayerPropertyCommand::Property::Clipped,
                                              l->isClipped(), clipped));
}

void Document::setLayerName(Layer::Id id, const QString& name) {
    Layer* l = layerById(id);
    if (!l || l->name() == name || name.isEmpty()) return;
    m_undoStack.push(new LayerPropertyCommand(this, id, LayerPropertyCommand::Property::Name,
                                              l->name(), name));
}

void Document::setLayerVisible(Layer::Id id, bool visible) {
    Layer* l = layerById(id);
    if (!l || l->isVisible() == visible) return;
    m_undoStack.push(new LayerPropertyCommand(this, id, LayerPropertyCommand::Property::Visible,
                                              l->isVisible(), visible));
}

void Document::setLayerOpacity(Layer::Id id, qreal opacity) {
    Layer* l = layerById(id);
    opacity = qBound<qreal>(0.0, opacity, 1.0);
    if (!l || qFuzzyCompare(l->opacity() + 1.0, opacity + 1.0)) return;
    m_undoStack.push(new LayerPropertyCommand(this, id, LayerPropertyCommand::Property::Opacity,
                                              l->opacity(), opacity));
}

// ---- jerarquia ----
bool Document::isDescendantOf(Layer::Id id, Layer::Id ancestor) const {
    const Layer* l = layerById(id);
    for (int guard = layerCount() + 1; l && guard > 0; --guard) {
        const Layer::Id p = l->parentId();
        if (p == Layer::InvalidId) return false;
        if (p == ancestor) return true;
        l = layerById(p);
    }
    return false;
}

int Document::topIndexInGroup(Layer::Id group) const {
    int top = indexOfLayer(group);
    for (int i = 0; i < layerCount(); ++i)
        if (m_layers[size_t(i)]->parentId() == group) top = qMax(top, i);
    return top;
}

// +1 sube, -1 baja (entre hermanas de la misma carpeta)
void Document::moveLayer(Layer::Id id, int delta) {
    const int from = indexOfLayer(id);
    if (from < 0 || delta == 0) return;
    const Layer::Id parent = m_layers[size_t(from)]->parentId();
    const int step = delta > 0 ? 1 : -1;
    for (int j = from + step; j >= 0 && j < layerCount(); j += step)
        if (m_layers[size_t(j)]->parentId() == parent) { moveLayerTo(id, j, parent); return; }
}

// 'index' = posicion de insercion en la lista SIN la capa movida
void Document::moveLayerTo(Layer::Id id, int index, Layer::Id parent) {
    const int from = indexOfLayer(id);
    Layer* l = layerAt(from);
    if (!l) return;
    if (parent != Layer::InvalidId) {
        const Layer* p = layerById(parent);
        if (!p || !p->isGroup() || parent == id || isDescendantOf(parent, id)) return; // no meter una carpeta en si misma
    }
    index = qBound(0, index, layerCount() - 1);
    if (from == index && l->parentId() == parent) return;
    m_undoStack.push(new MoveLayerCommand(this, id, from, l->parentId(), index, parent));
}

void Document::moveLayerRelative(Layer::Id id, Layer::Id target, LayerDrop where) {
    const int from = indexOfLayer(id), t = indexOfLayer(target);
    if (from < 0 || t < 0 || id == target) return;
    const Layer* tl = m_layers[size_t(t)].get();
    if (where == LayerDrop::Into && !tl->isGroup()) where = LayerDrop::Above;

    const int tAfter = t - (from < t ? 1 : 0); // indice del destino tras quitar la capa
    Layer::Id parent = tl->parentId();
    int index = tAfter + 1; // Above (en pantalla "encima" = indice mayor)
    if (where == LayerDrop::Below) index = tAfter;
    else if (where == LayerDrop::Into) {
        parent = target;
        int top = -1;
        for (int i = 0; i < layerCount(); ++i)
            if (i != from && m_layers[size_t(i)]->parentId() == target) top = i - (from < i ? 1 : 0);
        index = top >= 0 ? top + 1 : tAfter;
    }
    moveLayerTo(id, index, parent);
}

void Document::relocateLayer(Layer::Id id, int index, Layer::Id parent) {
    const int from = indexOfLayer(id);
    if (from < 0) return;
    std::unique_ptr<Layer> l = std::move(m_layers[size_t(from)]);
    m_layers.erase(m_layers.begin() + from);
    l->setParentId(parent);
    if (Layer* p = layerById(parent)) p->setExpanded(true);
    index = qBound(0, index, layerCount());
    m_layers.insert(m_layers.begin() + index, std::move(l));
    emit layerListChanged();
    emit contentChanged(rect());
}

// ---------------- render ----------------
void Document::render(QPainter& painter, const QRect& area) const {
    const QRect a = area & rect();
    if (a.isEmpty()) return;

    bool simple = true; // sin carpetas ni recortes: camino rapido de siempre
    for (const auto& l : m_layers)
        if (l->isGroup() || l->isClipped()) { simple = false; break; }

    if (simple) {
        const qreal previousOpacity = painter.opacity();
        for (const auto& layer : m_layers) {
            if (!layer->isVisible()) continue;
            painter.setOpacity(previousOpacity * layer->opacity());
            painter.drawImage(a.topLeft(), layer->image(), a);
        }
        painter.setOpacity(previousOpacity);
        return;
    }
    painter.drawImage(a.topLeft(), composeChildren(Layer::InvalidId, a));
}

// Compone (sin papel) los hijos directos de 'parent' en la zona 'a'.
QImage Document::composeChildren(Layer::Id parent, const QRect& a) const {
    QImage acc(a.size(), QImage::Format_ARGB32_Premultiplied);
    acc.fill(Qt::transparent);
    for (const auto& l : m_layers) {
        if (l->parentId() != parent || !l->isVisible()) continue;

        if (!l->isGroup() && !l->isClipped()) {
            QPainter p(&acc);
            p.setOpacity(l->opacity());
            p.drawImage(QPoint(0, 0), l->image(), a);
            continue;
        }
        QImage content = l->isGroup() ? composeChildren(l->id(), a) : l->image().copy(a);
        if (l->isClipped()) { // "bloquear alpha": solo donde ya hay algo debajo
            QPainter cp(&content);
            cp.setCompositionMode(QPainter::CompositionMode_DestinationIn);
            cp.drawImage(QPoint(0, 0), acc);
        }
        QPainter p(&acc);
        p.setOpacity(l->opacity());
        p.drawImage(QPoint(0, 0), content);
    }
    return acc;
}

QImage Document::layerPreview(Layer::Id id, const QSize& maxSize) const {
    const Layer* l = layerById(id);
    if (!l) return QImage();
    const QImage full = l->isGroup() ? composeChildren(id, rect()) : l->image();
    const QSize target = m_size.scaled(maxSize, Qt::KeepAspectRatio);
    QImage out(target, QImage::Format_ARGB32_Premultiplied);
    QPainter p(&out);
    constexpr int cell = 8; // damero de transparencia
    for (int y = 0; y < target.height(); y += cell)
        for (int x = 0; x < target.width(); x += cell)
            p.fillRect(x, y, cell, cell, ((x / cell + y / cell) % 2) ? QColor(190, 190, 190) : QColor(255, 255, 255));
    p.drawImage(QPoint(0, 0), full.scaled(target, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
    return out;
}

QImage Document::flatten(bool withPaper) const {
    QImage out(m_size, QImage::Format_ARGB32_Premultiplied);
    out.fill(withPaper ? QColor(Qt::white) : QColor(Qt::transparent));
    QPainter painter(&out);
    render(painter, rect());
    return out;
}

QImage Document::thumbnail(const QSize& maxSize) const {
    const QSize target = m_size.scaled(maxSize, Qt::KeepAspectRatio);
    QImage out(target, QImage::Format_ARGB32_Premultiplied);
    out.fill(Qt::white);
    QPainter p(&out);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.scale(qreal(target.width()) / m_size.width(), qreal(target.height()) / m_size.height());
    render(p, rect());
    return out;
}

Layer* Document::addLayerNoHistory(const QString& name, Layer::Id parent, bool group) {
    auto layer = std::make_unique<Layer>(allocateLayerId(), name, m_size, group);
    layer->setParentId(parent);
    Layer* raw = layer.get();
    insertLayer(layerCount(), std::move(layer));
    return raw;
}

void Document::addLayerWithImage(const QImage& image, const QPoint& topLeft,
                                 const QString& name, const QString& undoText) {
    auto layer = std::make_unique<Layer>(
        allocateLayerId(), name.isEmpty() ? tr("Capa %1").arg(m_nextLayerId) : name, m_size);
    {
        QPainter p(&layer->image());
        p.setCompositionMode(QPainter::CompositionMode_Source);
        p.drawImage(topLeft, image);
    }
    auto* cmd = new AddLayerCommand(this, std::move(layer));
    if (!undoText.isEmpty()) cmd->setText(undoText);
    m_undoStack.push(cmd);
}

QColor Document::colorAt(const QPoint& pos) const {
    if (!rect().contains(pos)) return QColor();
    QImage pixel(1, 1, QImage::Format_ARGB32_Premultiplied);
    pixel.fill(Qt::white);
    QPainter painter(&pixel);
    painter.translate(-pos);
    render(painter, QRect(pos, QSize(1, 1)));
    painter.end();
    return pixel.pixelColor(0, 0);
}

// ---------------- bajo nivel ----------------
void Document::insertLayer(int index, std::unique_ptr<Layer> layer) {
    index = qBound(0, index, layerCount());
    const Layer::Id id = layer->id();
    const bool hadActive = layerById(m_activeLayerId) != nullptr;
    m_layers.insert(m_layers.begin() + index, std::move(layer));
    emit layerListChanged();
    emit contentChanged(rect());
    if (!hadActive) {
        m_activeLayerId = Layer::InvalidId; // fuerza la emision en setActiveLayer
        setActiveLayer(id);
    }
}

std::unique_ptr<Layer> Document::takeLayer(int index) {
    std::unique_ptr<Layer> taken = std::move(m_layers[size_t(index)]);
    m_layers.erase(m_layers.begin() + index);

    const bool wasActive = taken->id() == m_activeLayerId;
    if (wasActive) {
        const Layer* next = layerAt(qMin(index, layerCount() - 1));
        m_activeLayerId = next ? next->id() : Layer::InvalidId;
    }
    emit layerListChanged();
    if (wasActive) emit activeLayerChanged(m_activeLayerId);
    emit contentChanged(rect());
    return taken;
}

void Document::notifyContentChanged(const QRect& dirty) { emit contentChanged(dirty); }

void Document::notifyLayerPropertiesChanged(Layer::Id id) {
    emit layerPropertiesChanged(id);
    emit contentChanged(rect());
}

// ---------------- seleccion ----------------
void Document::setSelection(const QPainterPath& path, const QString& undoText) {
    QPainterPath canvas;
    canvas.addRect(rect());
    QPainterPath p = path.intersected(canvas).simplified();
    if (p.boundingRect().isEmpty()) p = QPainterPath();
    if (p == m_selection) return;
    m_undoStack.push(new SelectionCommand(this, m_selection, p,
                                          undoText.isEmpty() ? tr("Selección") : undoText));
}

void Document::selectAll() {
    QPainterPath p;
    p.addRect(rect());
    setSelection(p, tr("Seleccionar todo"));
}

void Document::clearSelection() { setSelection(QPainterPath(), tr("Deseleccionar")); }

void Document::invertSelection() {
    if (!hasSelection()) { selectAll(); return; }
    QPainterPath all;
    all.addRect(rect());
    setSelection(all.subtracted(m_selection), tr("Invertir selección"));
}

void Document::applySelection(const QPainterPath& path) {
    m_selection = path;
    emit selectionChanged();
}