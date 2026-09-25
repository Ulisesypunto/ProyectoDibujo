#ifndef EYEDROPPERTOOL_H
#define EYEDROPPERTOOL_H

#include "ITool.h"
#include <QImage>
#include <functional>

class EyedropperTool : public ITool {
public:
    using ColorPickedCallback = std::function<void(const QColor&)>;

    explicit EyedropperTool(ColorPickedCallback callback);

    void mousePress(const QPoint& pos, QPainter& painter, QColor color, int size) override;
    void mouseMove(const QPoint& pos, QPainter& painter, QColor color, int size) override;
    void mouseRelease(const QPoint& pos, QPainter& painter, QColor color, int size) override;

    // Método para pasarle la referencia del Canvas/Imagen desde donde muestra el color
    void setCanvasImage(const QImage* img) { canvasImage = img; }

private:
    ColorPickedCallback onColorPicked;
    const QImage* canvasImage = nullptr;
    void sampleColor(const QPoint& pos);
};

#endif // EYEDROPPERTOOL_H