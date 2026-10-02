#include "core/Document.h"
#include "core/commands/LayerCommands.h"

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
    if (layerCount() <= 1 || !layerById(id)) return;
    m_undoStack.push(new RemoveLayerCommand(this, id));
}

void Document::moveLayer(Layer::Id id, int delta) {
    const int from = indexOfLayer(id);
    const int to = from + delta;
    if (from < 0 || to < 0 || to >= layerCount() || from == to) return;
    m_undoStack.push(new MoveLayerCommand(this, id, from, to));
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

// ---------------- render ----------------
void Document::render(QPainter& painter, const QRect& area) const {
    const QRect a = area & rect();
    if (a.isEmpty()) return;
    const qreal previousOpacity = painter.opacity();
    for (const auto& layer : m_layers) {
        if (!layer->isVisible()) continue;
        painter.setOpacity(previousOpacity * layer->opacity());
        painter.drawImage(a.topLeft(), layer->image(), a);
    }
    painter.setOpacity(previousOpacity);
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

Layer* Document::addLayerNoHistory(const QString& name) {
    auto layer = std::make_unique<Layer>(allocateLayerId(), name, m_size);
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

void Document::reorderLayer(int from, int to) {
    if (from == to || from < 0 || to < 0 || from >= layerCount() || to >= layerCount()) return;
    std::unique_ptr<Layer> moved = std::move(m_layers[size_t(from)]);
    m_layers.erase(m_layers.begin() + from);
    m_layers.insert(m_layers.begin() + to, std::move(moved));
    emit layerListChanged();
    emit contentChanged(rect());
}

void Document::notifyContentChanged(const QRect& dirty) { emit contentChanged(dirty); }

void Document::notifyLayerPropertiesChanged(Layer::Id id) {
    emit layerPropertiesChanged(id);
    emit contentChanged(rect());
}
