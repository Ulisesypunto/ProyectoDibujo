#include "core/commands/LayerCommands.h"
#include "core/Document.h"

// ---------- AddLayerCommand ----------
AddLayerCommand::AddLayerCommand(Document* doc, const QString& name)
    : m_doc(doc),
      m_layer(std::make_unique<Layer>(doc->allocateLayerId(), name, doc->size())),
      m_layerId(m_layer->id()),
      m_previousActive(doc->activeLayerId()) {
    const int activeIndex = doc->indexOfLayer(m_previousActive);
    m_index = activeIndex >= 0 ? activeIndex + 1 : doc->layerCount(); // encima de la activa
    setText(QObject::tr("Añadir capa"));
}

void AddLayerCommand::redo() {
    m_doc->insertLayer(m_index, std::move(m_layer));
    m_doc->setActiveLayer(m_layerId);
}

void AddLayerCommand::undo() {
    m_layer = m_doc->takeLayer(m_index);
    m_doc->setActiveLayer(m_previousActive);
}

// ---------- RemoveLayerCommand ----------
RemoveLayerCommand::RemoveLayerCommand(Document* doc, Layer::Id id)
    : m_doc(doc), m_layerId(id), m_index(doc->indexOfLayer(id)) {
    setText(QObject::tr("Eliminar capa"));
}

void RemoveLayerCommand::redo() { m_layer = m_doc->takeLayer(m_index); }

void RemoveLayerCommand::undo() {
    m_doc->insertLayer(m_index, std::move(m_layer));
    m_doc->setActiveLayer(m_layerId);
}

// ---------- MoveLayerCommand ----------
MoveLayerCommand::MoveLayerCommand(Document* doc, Layer::Id id, int from, int to)
    : m_doc(doc), m_layerId(id), m_from(from), m_to(to) {
    setText(QObject::tr("Mover capa"));
}

void MoveLayerCommand::redo() { m_doc->reorderLayer(m_from, m_to); }
void MoveLayerCommand::undo() { m_doc->reorderLayer(m_to, m_from); }

// ---------- LayerPropertyCommand ----------
LayerPropertyCommand::LayerPropertyCommand(Document* doc, Layer::Id id, Property property,
                                           const QVariant& oldValue, const QVariant& newValue)
    : m_doc(doc), m_layerId(id), m_property(property), m_old(oldValue), m_new(newValue) {
    switch (property) {
    case Property::Name:    setText(QObject::tr("Renombrar capa")); break;
    case Property::Visible: setText(QObject::tr("Mostrar/ocultar capa")); break;
    case Property::Opacity: setText(QObject::tr("Cambiar opacidad")); break;
    }
}

bool LayerPropertyCommand::mergeWith(const QUndoCommand* other) {
    const auto* o = static_cast<const LayerPropertyCommand*>(other);
    if (o->m_layerId != m_layerId || o->m_property != m_property) return false;
    m_new = o->m_new;
    return true;
}

void LayerPropertyCommand::apply(const QVariant& value) {
    Layer* layer = m_doc->layerById(m_layerId);
    if (!layer) return;
    switch (m_property) {
    case Property::Name:    layer->setName(value.toString()); break;
    case Property::Visible: layer->setVisible(value.toBool()); break;
    case Property::Opacity: layer->setOpacity(value.toReal()); break;
    }
    m_doc->notifyLayerPropertiesChanged(m_layerId);
}
