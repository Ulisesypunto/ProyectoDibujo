#include "EraserTool.h"
#include <QPainter>

EraserTool::EraserTool() : lastPoint(0, 0) {}

void EraserTool::mousePress(const QPoint& pos, QPainter& painter, QColor color, int size) {
    Q_UNUSED(color);
    lastPoint = pos;

    // Activa el modo para BORRAR PÍXELES (volviendo transparente la zona)
    painter.setCompositionMode(QPainter::CompositionMode_Clear);
    painter.setPen(QPen(Qt::transparent, size, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawPoint(pos);
}

void EraserTool::mouseMove(const QPoint& pos, QPainter& painter, QColor color, int size) {
    Q_UNUSED(color);

    painter.setCompositionMode(QPainter::CompositionMode_Clear);
    painter.setPen(QPen(Qt::transparent, size, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawLine(lastPoint, pos);
    lastPoint = pos;
}

void EraserTool::mouseRelease(const QPoint& pos, QPainter& painter, QColor color, int size) {
    Q_UNUSED(pos);
    Q_UNUSED(painter);
    Q_UNUSED(color);
    Q_UNUSED(size);
}