#include "core/Layer.h"

Layer::Layer(Id id, const QString& name, const QSize& size)
    : m_id(id), m_name(name), m_image(size, QImage::Format_ARGB32_Premultiplied) {
    m_image.fill(Qt::transparent);
}
QRect Layer::contentBounds() const {
    int minX = m_image.width(), minY = m_image.height(), maxX = -1, maxY = -1;
    for (int y = 0; y < m_image.height(); ++y) {
        const QRgb* line = reinterpret_cast<const QRgb*>(m_image.constScanLine(y));
        for (int x = 0; x < m_image.width(); ++x) {
            if (qAlpha(line[x]) == 0) continue;
            minX = qMin(minX, x); maxX = qMax(maxX, x);
            minY = qMin(minY, y); maxY = qMax(maxY, y);
        }
    }
    return maxX < 0 ? QRect() : QRect(QPoint(minX, minY), QPoint(maxX, maxY));
}