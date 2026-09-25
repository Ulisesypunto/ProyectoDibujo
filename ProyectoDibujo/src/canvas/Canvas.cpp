#include "Canvas.h"
#include <QPainter>
#include <QMouseEvent>

Canvas::Canvas(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_StaticContents);
    // Crear la primera capa por defecto al iniciar
    addLayer("Capa 1");
}

void Canvas::addLayer(const QString &name) {
    int w = width() > 0 ? width() : 800;
    int h = height() > 0 ? height() : 600;
    layers.append(Layer(name, w, h));
    currentLayerIndex = layers.size() - 1;
    update();
}

void Canvas::setActiveLayer(int index) {
    if (index >= 0 && index < layers.size()) {
        currentLayerIndex = index;
    }
}

void Canvas::setTool(ITool* tool) { currentTool = tool; }
void Canvas::setPenColor(const QColor &color) { currentColor = color; }
void Canvas::setPenSize(int size) { currentSize = size; }

void Canvas::resizeEvent(QResizeEvent *event) {
    for (auto &layer : layers) {
        if (width() > layer.image.width() || height() > layer.image.height()) {
            int newW = qMax(width() + 128, layer.image.width());
            int newH = qMax(height() + 128, layer.image.height());

            QImage newImage(newW, newH, QImage::Format_ARGB32);
            newImage.fill(Qt::transparent);

            QPainter painter(&newImage);
            painter.drawImage(QPoint(0, 0), layer.image);
            layer.image = newImage;
        }
    }
    QWidget::resizeEvent(event);
}

void Canvas::paintEvent(QPaintEvent *event) {
    QPainter painter(this);
    QRect dirtyRect = event->rect();

    // 1. Dibujar el fondo base (el "papel" blanco)
    painter.fillRect(dirtyRect, canvasBackgroundColor);

    // 2. Dibujar cada capa superpuesta en orden
    for (const auto &layer : layers) {
        if (layer.visible) {
            painter.setOpacity(layer.opacity);
            painter.drawImage(dirtyRect, layer.image, dirtyRect);
        }
    }
}

void Canvas::saveState() {
    // Guardar copia actual de todas las capas
    undoStack.push(layers);

    // Limitar el tamaño máximo del historial
    if (undoStack.size() > maxUndoSteps) {
        undoStack.removeFirst();
    }

    // Cada vez que el usuario dibuja algo nuevo, la pila de redo se invalida
    redoStack.clear();
}

void Canvas::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton && currentTool && !layers.isEmpty()) {
        // 1. Guardamos una foto del lienzo ANTES de que la herramienta modifique nada
        saveState();

        QPainter painter(&layers[currentLayerIndex].image);
        currentTool->mousePress(event->pos(), painter, currentColor, currentSize);
        update();
    }
}

void Canvas::mouseMoveEvent(QMouseEvent *event) {
    if ((event->buttons() & Qt::LeftButton) && currentTool && !layers.isEmpty()) {
        // NO guardamos estado aquí para no saturar la pila con micro-líneas
        QPainter painter(&layers[currentLayerIndex].image);
        currentTool->mouseMove(event->pos(), painter, currentColor, currentSize);
        update();
    }
}

void Canvas::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton && currentTool && !layers.isEmpty()) {
        QPainter painter(&layers[currentLayerIndex].image);
        currentTool->mouseRelease(event->pos(), painter, currentColor, currentSize);
        update();
        // El trazo completo ya se dibujó en la QImage activa.
        // El estado guardado en mousePress ahora sirve perfectamente como punto de retorno (Undo).
    }
}

// Devuelve la imagen de la capa activa o la imagen combinada
const QImage* Canvas::getCompositeImagePointer() const {
    if (!layers.isEmpty() && currentLayerIndex < layers.size()) {
        return &layers[currentLayerIndex].image;
    }
    return nullptr;
}

void Canvas::undo() {
    if (undoStack.isEmpty()) return;

    // Pasar el estado actual a la pila de redo
    redoStack.push(layers);

    // Recuperar el estado anterior
    layers = undoStack.pop();

    // Asegurar que el índice activo no quede fuera de rango
    if (currentLayerIndex >= layers.size()) {
        currentLayerIndex = qMax(0, layers.size() - 1);
    }

    update();
}

void Canvas::redo() {
    if (redoStack.isEmpty()) return;

    // Guardar el estado actual en undo antes de rehacer
    undoStack.push(layers);

    // Recuperar el estado de redo
    layers = redoStack.pop();

    if (currentLayerIndex >= layers.size()) {
        currentLayerIndex = qMax(0, layers.size() - 1);
    }

    update();
}