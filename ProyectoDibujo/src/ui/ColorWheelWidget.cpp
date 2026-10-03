#include "ui/ColorWheelWidget.h"

#include <QConicalGradient>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <algorithm>
#include <cmath>

namespace { constexpr qreal Pi = 3.14159265358979323846; }

ColorWheelWidget::ColorWheelWidget(QWidget* parent) : QWidget(parent) {
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumSize(140, 140);
    setCursor(Qt::CrossCursor);
}

QColor ColorWheelWidget::color() const {
    return QColor::fromHsvF(m_h, m_s, m_v).toRgb();
}

void ColorWheelWidget::setColor(const QColor& c) {
    if (!c.isValid() || c.rgb() == color().rgb()) return;
    const float h = c.hsvHueF();
    if (h >= 0.f) m_h = h; // en grises el tono no esta definido: conservamos el anterior
    m_s = c.hsvSaturationF();
    m_v = c.valueF();
    update();
}

ColorWheelWidget::Geometry ColorWheelWidget::geometry() const {
    Geometry g;
    const qreal side = qMin(width(), height());
    g.center = QPointF(width() / 2.0, height() / 2.0);
    g.outerR = side / 2.0 - 2.0;
    const qreal ringW = qMax<qreal>(10.0, g.outerR * 0.16);
    g.innerR = g.outerR - ringW;
    const qreal half = (g.innerR - 3.0) / std::sqrt(2.0); // cuadrado inscrito en el anillo
    g.square = QRectF(g.center.x() - half, g.center.y() - half, 2 * half, 2 * half);
    return g;
}

void ColorWheelWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const Geometry g = geometry();
    const qreal ringW = g.outerR - g.innerR;

    // Anillo de tonos (el gradiente conico de Qt gira en sentido antihorario desde las 3h)
    QConicalGradient cg(g.center, 0);
    for (int i = 0; i <= 6; ++i) cg.setColorAt(i / 6.0, QColor::fromHsvF(i / 6.0f, 1.f, 1.f));
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QBrush(cg), ringW));
    p.drawEllipse(g.center, g.outerR - ringW / 2, g.outerR - ringW / 2);

    // Cuadrado: tono puro + blanco (izq->der) + negro (arriba->abajo)
    p.setPen(Qt::NoPen);
    p.fillRect(g.square, QColor::fromHsvF(m_h, 1.f, 1.f));
    QLinearGradient white(g.square.left(), 0, g.square.right(), 0);
    white.setColorAt(0, Qt::white);
    white.setColorAt(1, QColor(255, 255, 255, 0));
    p.fillRect(g.square, white);
    QLinearGradient black(0, g.square.top(), 0, g.square.bottom());
    black.setColorAt(0, QColor(0, 0, 0, 0));
    black.setColorAt(1, Qt::black);
    p.fillRect(g.square, black);

    // Marcadores
    auto marker = [&p](const QPointF& pt) {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(Qt::black, 3));
        p.drawEllipse(pt, 5.5, 5.5);
        p.setPen(QPen(Qt::white, 1.5));
        p.drawEllipse(pt, 5.5, 5.5);
    };
    const qreal a = m_h * 2 * Pi;
    const qreal rm = g.outerR - ringW / 2;
    marker(QPointF(g.center.x() + rm * std::cos(a), g.center.y() - rm * std::sin(a)));
    marker(QPointF(g.square.left() + m_s * g.square.width(),
                   g.square.top() + (1.0 - m_v) * g.square.height()));
}

void ColorWheelWidget::pick(const QPointF& pos) {
    const Geometry g = geometry();
    if (m_drag == Drag::Hue) {
        const QPointF d = pos - g.center;
        qreal a = std::atan2(-d.y(), d.x()) / (2 * Pi);
        if (a < 0) a += 1.0;
        m_h = float(a);
    } else if (m_drag == Drag::Square) {
        m_s = float(std::clamp((pos.x() - g.square.left()) / g.square.width(), 0.0, 1.0));
        m_v = float(1.0 - std::clamp((pos.y() - g.square.top()) / g.square.height(), 0.0, 1.0));
    } else {
        return;
    }
    update();
    emit colorPicked(color());
}

void ColorWheelWidget::mousePressEvent(QMouseEvent* e) {
    if (e->button() != Qt::LeftButton) return;
    const Geometry g = geometry();
    const QPointF pos = e->position();
    const QPointF d = pos - g.center;
    const qreal dist = std::hypot(d.x(), d.y());
    if (g.square.contains(pos)) m_drag = Drag::Square;
    else if (dist >= g.innerR - 2 && dist <= g.outerR + 2) m_drag = Drag::Hue;
    else { m_drag = Drag::None; return; }
    pick(pos);
}

void ColorWheelWidget::mouseMoveEvent(QMouseEvent* e) {
    if (m_drag != Drag::None && (e->buttons() & Qt::LeftButton)) pick(e->position());
}

void ColorWheelWidget::mouseReleaseEvent(QMouseEvent*) { m_drag = Drag::None; }