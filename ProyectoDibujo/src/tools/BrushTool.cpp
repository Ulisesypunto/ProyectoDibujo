#include "BrushTool.h"
#include <QPainter>

BrushTool::BrushTool() : lastPoint(0, 0) {}

void BrushTool::mousePress(const QPoint& pos, QPainter& painter, QColor color, int size) {
    lastPoint = pos;
    
    // Dibuja un punto inicial al hacer click
    painter.setPen(QPen(color, size, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawPoint(pos);
}

void BrushTool::mouseMove(const QPoint& pos, QPainter& painter, QColor color, int size) {
    // Unimos el punto anterior con el actual para formar un trazo continuo
    painter.setPen(QPen(color, size, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawLine(lastPoint, pos);
    lastPoint = pos;
}

void BrushTool::mouseRelease(const QPoint& pos, QPainter& painter, QColor color, int size) {
    Q_UNUSED(pos);
    Q_UNUSED(painter);
    Q_UNUSED(color);
    Q_UNUSED(size);
}