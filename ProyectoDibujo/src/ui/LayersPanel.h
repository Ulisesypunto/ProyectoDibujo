// LayersPanel.h
#pragma once
#include <QPointer>
#include <QWidget>

#include "core/Document.h"
#include "core/Layer.h"

class LayerTreeWidget;
class QTreeWidgetItem;
class QSlider;
class QLabel;

// Panel de capas: arbol con carpetas, arrastrar para ordenar/mover a carpetas,
// visibilidad, renombrar (doble clic), boton alpha, opacidad y miniatura al
// pasar el raton. Todo va por Document (=> undo/redo).
class LayersPanel : public QWidget {
    Q_OBJECT
public:
    explicit LayersPanel(QWidget* parent = nullptr);
    void setDocument(Document* document);

protected:
    bool eventFilter(QObject* obj, QEvent* event) override; // tooltip con miniatura

private:
    void rebuildList();
    void addChildren(QTreeWidgetItem* parentItem, Layer::Id parentId);
    void syncOpacitySlider();
    void onActiveLayerChanged(Layer::Id id);
    void onLayerPropertiesChanged(Layer::Id id);
    void onCurrentItemChanged(QTreeWidgetItem* current);
    void onItemChanged(QTreeWidgetItem* item, int column);
    void onOpacityChanged(int value);
    void onLayerDropped(int draggedId, int targetId, int where);
    QTreeWidgetItem* itemForLayer(Layer::Id id) const;
    Layer::Id currentLayerId() const;

    QPointer<Document> m_document;
    LayerTreeWidget* m_tree;
    QSlider* m_opacity;
    QLabel* m_opacityLabel;
    bool m_updating = false;
};