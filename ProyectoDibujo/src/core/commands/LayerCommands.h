// LayerCommands.h
#pragma once
#include <QString>
#include <QUndoCommand>
#include <QVariant>
#include <memory>
#include <vector>

#include "core/Layer.h"

class Document;

class AddLayerCommand : public QUndoCommand {
public:
    AddLayerCommand(Document* doc, const QString& name);
    AddLayerCommand(Document* doc, std::unique_ptr<Layer> layer);
    void undo() override;
    void redo() override;
private:
    Document* m_doc;
    std::unique_ptr<Layer> m_layer;
    Layer::Id m_layerId;
    Layer::Id m_previousActive;
    Layer::Id m_parent = Layer::InvalidId;
    int m_index = 0;
};

// Borra la capa y, si es carpeta, todo su contenido.
class RemoveLayerCommand : public QUndoCommand {
public:
    RemoveLayerCommand(Document* doc, Layer::Id id);
    void undo() override;
    void redo() override;
private:
    struct Entry { int index; std::unique_ptr<Layer> layer; };
    Document* m_doc;
    Layer::Id m_layerId;
    std::vector<Entry> m_entries; // indices ascendentes
};

// Cambia posicion y/o carpeta. Los indices son "posicion de insercion tras
// quitar la capa de la lista".
class MoveLayerCommand : public QUndoCommand {
public:
    MoveLayerCommand(Document* doc, Layer::Id id, int fromIndex, Layer::Id fromParent,
                     int toIndex, Layer::Id toParent);
    void undo() override;
    void redo() override;
private:
    Document* m_doc;
    Layer::Id m_layerId;
    int m_fromIndex;
    Layer::Id m_fromParent;
    int m_toIndex;
    Layer::Id m_toParent;
};

class LayerPropertyCommand : public QUndoCommand {
public:
    enum class Property { Name, Visible, Opacity, Clipped };

    LayerPropertyCommand(Document* doc, Layer::Id id, Property property,
                         const QVariant& oldValue, const QVariant& newValue);
    void undo() override { apply(m_old); }
    void redo() override { apply(m_new); }
    int id() const override { return m_property == Property::Opacity ? 1001 : -1; }
    bool mergeWith(const QUndoCommand* other) override;
private:
    void apply(const QVariant& value);

    Document* m_doc;
    Layer::Id m_layerId;
    Property m_property;
    QVariant m_old;
    QVariant m_new;
};