#pragma once
#include <QColor>
#include <QRectF>
#include <QWidget>

// Anillo de tono + cuadrado de saturacion/brillo (como el selector de Krita).
class ColorWheelWidget : public QWidget {
    Q_OBJECT
public:
    explicit ColorWheelWidget(QWidget* parent = nullptr);

    QColor color() const;
    void setColor(const QColor& color); // no emite colorPicked

    QSize sizeHint() const override { return {200, 200}; }

signals:
    void colorPicked(const QColor& color); // solo cuando el usuario interactua

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    enum class Drag { None, Hue, Square };
    struct Geometry {
        QPointF center;
        qreal outerR;
        qreal innerR;
        QRectF square;
    };
    Geometry geometry() const;
    void pick(const QPointF& pos);

    float m_h = 0.f, m_s = 0.f, m_v = 0.f; // 0..1
    Drag m_drag = Drag::None;
};