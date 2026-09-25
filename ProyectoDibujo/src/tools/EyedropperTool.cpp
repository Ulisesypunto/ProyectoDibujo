#include "EyedropperTool.h"
#include <QPainter>

EyedropperTool::EyedropperTool(ColorPickedCallback callback) 
    : onColorPicked(callback) {}

void EyedropperTool::sampleColor(const QPoint& pos) {
    if (canvasImage && canvasImage->rect().contains(pos)) {
        QColor picked = canvasImage->pixelColor(pos);
        if (onColorPicked && picked.isValid()) {
            onColorPicked(picked);
        }
    }
}

void EyedropperTool::mousePress(const QPoint& pos, QPainter& painter, QColor color, int size) {
    Q_UNUSED(painter); Q_UNUSED(color); Q_UNUSED(size);
    sampleColor(pos);
}

void EyedropperTool::mouseMove(const QPoint& pos, QPainter& painter, QColor color, int size) {
    Q_UNUSED(painter); Q_UNUSED(color); Q_UNUSED(size);
    sampleColor(pos);
}

void EyedropperTool::mouseRelease(const QPoint& pos, QPainter& painter, QColor color, int size) {
    Q_UNUSED(pos); Q_UNUSED(painter); Q_UNUSED(color); Q_UNUSED(size);
}