#ifndef CANVAS_H
#define CANVAS_H

#include <QWidget>
#include <QVector>
#include <memory>
#include "Layer.h"
#include "ITool.h"
#include <QStack>

class Canvas : public QWidget {
    Q_OBJECT

public:
    explicit Canvas(QWidget *parent = nullptr);

    void setTool(ITool* tool);
    void setPenColor(const QColor &color);
    void setPenSize(int size);

    // Gestión de Capas
    void addLayer(const QString &name = "Nueva Capa");
    void setActiveLayer(int index);
    int activeLayerIndex() const { return currentLayerIndex; }

    int getLayersCount() const { return layers.size(); }
    QString getLayerName(int index) const { 
        if (index >= 0 && index < layers.size()) return layers[index].name;
        return "";
    }

    // Devuelve el puntero a la imagen que combina todas las capas (para la pipeta/gotero)
    const QImage* getCompositeImagePointer() const;
    
    // Métodos para deshacer y rehacer
    void undo();
    void redo();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    QVector<Layer> layers;
    int currentLayerIndex = 0;

    ITool* currentTool = nullptr;
    QColor currentColor = Qt::black;
    int currentSize = 5;

    QColor canvasBackgroundColor = Qt::white; // Fondo base (papel)

    // --- PILAS DE HISTORIAL PARA UNDO / REDO ---
    QStack<QVector<Layer>> undoStack;
    QStack<QVector<Layer>> redoStack;
    const int maxUndoSteps = 20; // Límite de pasos guardados para cuidar la memoria

    void saveState();
};

#endif // CANVAS_H