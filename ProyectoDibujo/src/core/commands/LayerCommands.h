#pragma once
#include <QString>
#include <QUndoCommand>
#include <QVariant>
#include <memory>

#include "core/Layer.h"

class Document;

class AddLayerCommand : public QUndoCommand {
public:
    AddLayerCommand(Document* doc, const QString& name);
    void undo() override;
    void redo() override;
private:
    Document* m_doc;
    std::unique_ptr<Layer> m_layer; // solo tiene valor mientras esta "deshecha"
    Layer::Id m_layerId;
    Layer::Id m_previousActive;
    int m_index = 0;
};

class RemoveLayerCommand : public QUndoCommand {
public:
    RemoveLayerCommand(Document* doc, Layer::Id id);
    void undo() override;
    void redo() override;
private:
    Document* m_doc;
    Layer::Id m_layerId;
    int m_index;
    std::unique_ptr<Layer> m_layer;
};

class MoveLayerCommand : public QUndoCommand {
public:
    MoveLayerCommand(Document* doc, Layer::Id id, int from, int to);
    void undo() override;
    void redo() override;
private:
    Document* m_doc;
    Layer::Id m_layerId;
    int m_from;
    int m_to;
};

// Nombre / visibilidad / opacidad. La opacidad se fusiona (mergeWith)
// para que arrastrar el slider sea un unico paso de undo.
class LayerPropertyCommand : public QUndoCommand {
public:
    enum class Property { Name, Visible, Opacity };

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
