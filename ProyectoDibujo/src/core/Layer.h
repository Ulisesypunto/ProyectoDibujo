// Layer.h
#pragma once
#include <QImage>
#include <QString>

class Layer {
public:
    using Id = int;
    static constexpr Id InvalidId = -1;

    Layer(Id id, const QString& name, const QSize& size, bool isGroup = false);
    Layer(const Layer&) = delete;
    Layer& operator=(const Layer&) = delete;

    Id id() const { return m_id; }
    bool isGroup() const { return m_isGroup; }          // carpeta: no tiene pixeles

    Id parentId() const { return m_parent; }            // InvalidId = raiz
    void setParentId(Id id) { m_parent = id; }

    const QString& name() const { return m_name; }
    void setName(const QString& name) { m_name = name; }

    bool isVisible() const { return m_visible; }
    void setVisible(bool visible) { m_visible = visible; }

    qreal opacity() const { return m_opacity; }
    void setOpacity(qreal opacity) { m_opacity = qBound<qreal>(0.0, opacity, 1.0); }

    // Limita la capa a la zona opaca de lo que hay debajo (en su mismo nivel)
    bool isClipped() const { return m_clipped; }
    void setClipped(bool clipped) { m_clipped = clipped; }

    // Solo UI (carpeta abierta/cerrada); no entra en el historial
    bool isExpanded() const { return m_expanded; }
    void setExpanded(bool e) { m_expanded = e; }

    QImage& image() { return m_image; }
    const QImage& image() const { return m_image; }
    QRect rect() const { return m_image.rect(); }
    QRect contentBounds() const;

private:
    Id m_id;
    QString m_name;
    bool m_visible = true;
    qreal m_opacity = 1.0;
    QImage m_image; // ARGB32 premultiplicado (nula en las carpetas)
    bool m_isGroup;
    Id m_parent = InvalidId;
    bool m_clipped = false;
    bool m_expanded = true;
};