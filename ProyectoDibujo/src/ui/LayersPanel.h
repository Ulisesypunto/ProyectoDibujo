#pragma once
#include <QPointer>
#include <QWidget>

#include "core/Document.h"
#include "core/Layer.h"

class QListWidget;
class QListWidgetItem;
class QSlider;
class QLabel;

// Panel de capas: lista (la de arriba primero), visibilidad, renombrar,
// opacidad, añadir/borrar/mover. Todo va por Document (=> undo/redo gratis).
class LayersPanel : public QWidget {
    Q_OBJECT
public:
    explicit LayersPanel(QWidget* parent = nullptr);
    void setDocument(Document* document);

private:
    void rebuildList();
    void syncOpacitySlider();
    void onActiveLayerChanged(Layer::Id id);
    void onLayerPropertiesChanged(Layer::Id id);
    void onCurrentRowChanged(int row);
    void onItemChanged(QListWidgetItem* item);
    void onOpacityChanged(int value);
    int rowForLayer(Layer::Id id) const;
    Layer::Id currentLayerId() const;

    QPointer<Document> m_document;
    QListWidget* m_list;
    QSlider* m_opacity;
    QLabel* m_opacityLabel;
    bool m_updating = false; // evita bucles UI <-> modelo
};
