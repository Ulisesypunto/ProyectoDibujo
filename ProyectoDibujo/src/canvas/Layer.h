#ifndef LAYER_H
#define LAYER_H

#include <QImage>
#include <QString>

struct Layer {
    QString name;
    QImage image;
    bool visible = true;
    float opacity = 1.0f;

    Layer(const QString &layerName, int width, int height)
        : name(layerName), image(width, height, QImage::Format_ARGB32) {
        image.fill(Qt::transparent); // Por defecto es totalmente transparente
    }
};

#endif // LAYER_H