#pragma once
#include <QImage>
#include <QString>

// Una capa de pixeles. No es copiable a proposito (una imagen puede pesar
// varios MB): el Document las guarda por puntero unico y estable.
class Layer {
public:
    using Id = int;
    static constexpr Id InvalidId = -1;

    Layer(Id id, const QString& name, const QSize& size);
    Layer(const Layer&) = delete;
    Layer& operator=(const Layer&) = delete;

    Id id() const { return m_id; }

    const QString& name() const { return m_name; }
    void setName(const QString& name) { m_name = name; }

    bool isVisible() const { return m_visible; }
    void setVisible(bool visible) { m_visible = visible; }

    qreal opacity() const { return m_opacity; }
    void setOpacity(qreal opacity) { m_opacity = qBound<qreal>(0.0, opacity, 1.0); }

    QImage& image() { return m_image; }
    const QImage& image() const { return m_image; }
    QRect rect() const { return m_image.rect(); }
    QRect contentBounds() const; // caja mínima con píxeles no transparentes

private:
    Id m_id;
    QString m_name;
    bool m_visible = true;
    qreal m_opacity = 1.0;
    QImage m_image; // ARGB32 premultiplicado (mas rapido al componer)
};
