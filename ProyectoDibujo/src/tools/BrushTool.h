#ifndef BRUSHTOOL_H
#define BRUSHTOOL_H

#include "ITool.h"

class BrushTool : public ITool {
public:
    BrushTool();
    void mousePress(const QPoint& pos, QPainter& painter, QColor color, int size) override;
    void mouseMove(const QPoint& pos, QPainter& painter, QColor color, int size) override;
    void mouseRelease(const QPoint& pos, QPainter& painter, QColor color, int size) override;

private:
    QPoint lastPoint;
};

#endif // BRUSHTOOL_H