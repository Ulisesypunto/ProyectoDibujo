#pragma once
#include <QColor>
#include <QObject>
#include <QRect>
#include <QSize>
#include <QUndoStack>
#include <memory>
#include <vector>

#include "core/Layer.h"

class QPainter;

// Modelo del dibujo: tamano fijo, lista de capas e historial (undo/redo).
// No conoce la UI ni las herramientas: solo emite senales cuando cambia.
class Document : public QObject {
    Q_OBJECT
public:
    explicit Document(const QSize& size, QObject* parent = nullptr);
    static std::unique_ptr<Document> fromImage(const QImage& image);

    QSize size() const { return m_size; }
    QRect rect() const { return QRect(QPoint(0, 0), m_size); }
    QUndoStack* undoStack() { return &m_undoStack; }

    // --- Consulta de capas (indice 0 = fondo, ultimo = arriba) ---
    int layerCount() const { return int(m_layers.size()); }
    Layer* layerAt(int index) const;
    Layer* layerById(Layer::Id id) const;
    int indexOfLayer(Layer::Id id) const;

    Layer* activeLayer() const { return layerById(m_activeLayerId); }
    Layer::Id activeLayerId() const { return m_activeLayerId; }
    void setActiveLayer(Layer::Id id);

    // --- Operaciones de alto nivel (todas deshacibles) ---
    void addLayer(const QString& name = QString());
    void removeLayer(Layer::Id id);            // siempre queda al menos 1 capa
    void moveLayer(Layer::Id id, int delta);   // +1 sube, -1 baja
    void setLayerName(Layer::Id id, const QString& name);
    void setLayerVisible(Layer::Id id, bool visible);
    void setLayerOpacity(Layer::Id id, qreal opacity);

    // --- Render ---
    void render(QPainter& painter, const QRect& area) const; // solo capas, sin papel
    QImage flatten(bool withPaper = true) const;            // (antes: flatten() const)
    QImage thumbnail(const QSize& maxSize) const;
    Layer* addLayerNoHistory(const QString& name);          // para cargar archivos
    void addLayerWithImage(const QImage& image, const QPoint& topLeft,
                        const QString& name, const QString& undoText);
    QColor colorAt(const QPoint& pos) const;                  // color compuesto de un pixel

    // --- Bajo nivel: los usan los QUndoCommand y las herramientas ---
    Layer::Id allocateLayerId() { return m_nextLayerId++; }
    void insertLayer(int index, std::unique_ptr<Layer> layer);
    std::unique_ptr<Layer> takeLayer(int index);
    void reorderLayer(int from, int to);
    void notifyContentChanged(const QRect& dirty);
    void notifyLayerPropertiesChanged(Layer::Id id);

signals:
    void layerListChanged();
    void activeLayerChanged(Layer::Id id);
    void layerPropertiesChanged(Layer::Id id);
    void contentChanged(const QRect& dirty);

private:
    QSize m_size;
    std::vector<std::unique_ptr<Layer>> m_layers;
    Layer::Id m_activeLayerId = Layer::InvalidId;
    Layer::Id m_nextLayerId = 1;
    QUndoStack m_undoStack;
};
