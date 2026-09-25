#ifndef ERASERTOOL_H
#define ERASERTOOL_H

#include "ITool.h"

class EraserTool : public ITool {
public:
    EraserTool();
    void mousePress(const QPoint& pos, QPainter& painter, QColor color, int size) override;
    void mouseMove(const QPoint& pos, QPainter& painter, QColor color, int size) override;
    void mouseRelease(const QPoint& pos, QPainter& painter, QColor color, int size) override;

private:
    QPoint lastPoint;
};

#endif // ERASERTOOL_H