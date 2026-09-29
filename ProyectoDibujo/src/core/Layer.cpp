#include "core/Layer.h"

Layer::Layer(Id id, const QString& name, const QSize& size)
    : m_id(id), m_name(name), m_image(size, QImage::Format_ARGB32_Premultiplied) {
    m_image.fill(Qt::transparent);
}
