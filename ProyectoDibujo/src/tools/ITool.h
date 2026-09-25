#ifndef ITOOL_H
#define ITOOL_H

#include <QPoint>
#include <QColor>

class QPainter;

class ITool {
public:
    virtual ~ITool() = default;

    // Métodos que ejecutará el lienzo según eventos del ratón
    virtual void mousePress(const QPoint& pos, QPainter& painter, QColor color, int size) = 0;
    virtual void mouseMove(const QPoint& pos, QPainter& painter, QColor color, int size) = 0;
    virtual void mouseRelease(const QPoint& pos, QPainter& painter, QColor color, int size) = 0;
};

#endif // ITOOL_H