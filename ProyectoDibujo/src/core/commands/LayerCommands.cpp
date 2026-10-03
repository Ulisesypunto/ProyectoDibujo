// LayerCommands.cpp
#include "core/commands/LayerCommands.h"
#include "core/Document.h"

// ---------- AddLayerCommand ----------
AddLayerCommand::AddLayerCommand(Document* doc, const QString& name)
    : AddLayerCommand(doc, std::make_unique<Layer>(doc->allocateLayerId(), name, doc->size())) {}

AddLayerCommand::AddLayerCommand(Document* doc, std::unique_ptr<Layer> layer)
    : m_doc(doc), m_layer(std::move(layer)), m_layerId(m_layer->id()),
      m_previousActive(doc->activeLayerId()) {
    const Layer* active = doc->layerById(m_previousActive);
    if (active && active->isGroup() && !m_layer->isGroup()) {
        // carpeta activa: la capa nueva entra DENTRO, arriba del todo
        m_parent = active->id();
        m_index = doc->topIndexInGroup(m_parent) + 1;
    } else if (active) {
        m_parent = active->parentId(); // hermana, justo encima de la activa
        m_index = doc->indexOfLayer(m_previousActive) + 1;
    } else {
        m_index = doc->layerCount();
    }
    m_layer->setParentId(m_parent);
    setText(m_layer->isGroup() ? QObject::tr("Añadir carpeta") : QObject::tr("Añadir capa"));
}

void AddLayerCommand::redo() {
    if (Layer* p = m_doc->layerById(m_parent)) p->setExpanded(true); // antes: la UI se reconstruye al insertar
    m_doc->insertLayer(m_index, std::move(m_layer));
    m_doc->setActiveLayer(m_layerId);
}

void AddLayerCommand::undo() {
    m_layer = m_doc->takeLayer(m_index);
    m_doc->setActiveLayer(m_previousActive);
}

// ---------- RemoveLayerCommand ----------
RemoveLayerCommand::RemoveLayerCommand(Document* doc, Layer::Id id) : m_doc(doc), m_layerId(id) {
    for (int i = 0; i < doc->layerCount(); ++i) {
        const Layer::Id lid = doc->layerAt(i)->id();
        if (lid == id || doc->isDescendantOf(lid, id)) m_entries.push_back(Entry{i, nullptr});
    }
    setText(doc->layerById(id) && doc->layerById(id)->isGroup() ? QObject::tr("Eliminar carpeta")
                                                                 : QObject::tr("Eliminar capa"));
}

void RemoveLayerCommand::redo() {
    for (auto it = m_entries.rbegin(); it != m_entries.rend(); ++it) // de arriba a abajo: indices validos
        it->layer = m_doc->takeLayer(it->index);
}

void RemoveLayerCommand::undo() {
    for (Entry& e : m_entries) m_doc->insertLayer(e.index, std::move(e.layer)); // ascendente
    m_doc->setActiveLayer(m_layerId);
}

// ---------- MoveLayerCommand ----------
MoveLayerCommand::MoveLayerCommand(Document* doc, Layer::Id id, int fromIndex, Layer::Id fromParent,
                                   int toIndex, Layer::Id toParent)
    : m_doc(doc), m_layerId(id), m_fromIndex(fromIndex), m_fromParent(fromParent),
      m_toIndex(toIndex), m_toParent(toParent) {
    setText(QObject::tr("Mover capa"));
}

void MoveLayerCommand::redo() { m_doc->relocateLayer(m_layerId, m_toIndex, m_toParent); }
void MoveLayerCommand::undo() { m_doc->relocateLayer(m_layerId, m_fromIndex, m_fromParent); }

// ---------- LayerPropertyCommand ----------
LayerPropertyCommand::LayerPropertyCommand(Document* doc, Layer::Id id, Property property,
                                           const QVariant& oldValue, const QVariant& newValue)
    : m_doc(doc), m_layerId(id), m_property(property), m_old(oldValue), m_new(newValue) {
    switch (property) {
    case Property::Name:    setText(QObject::tr("Renombrar capa")); break;
    case Property::Visible: setText(QObject::tr("Mostrar/ocultar capa")); break;
    case Property::Opacity: setText(QObject::tr("Cambiar opacidad")); break;
    case Property::Clipped: setText(QObject::tr("Bloquear alpha")); break;
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
    case Property::Clipped: layer->setClipped(value.toBool()); break;
    }
    m_doc->notifyLayerPropertiesChanged(m_layerId);
}