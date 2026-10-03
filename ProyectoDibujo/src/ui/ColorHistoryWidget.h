#pragma once
#include <QColor>
#include <QMouseEvent>
#include <QPainter>
#include <QWidget>
#include <vector>

// Cuadricula pequeña con los ultimos colores usados (el mas reciente arriba-izquierda).
class ColorHistoryWidget : public QWidget {
    Q_OBJECT
public:
    static constexpr int Columns = 1;  // tira vertical como en Krita
    static constexpr int Rows = 14;
    static constexpr int Cell = 20;

    explicit ColorHistoryWidget(QWidget* parent = nullptr) : QWidget(parent) {
        setFixedSize(Columns * Cell, Rows * Cell);
        setCursor(Qt::PointingHandCursor);
    }

public slots:
    void addColor(const QColor& color) {
        if (!m_colors.empty() && m_colors.front().rgb() == color.rgb()) return;
        for (auto it = m_colors.begin(); it != m_colors.end(); ++it)
            if (it->rgb() == color.rgb()) { m_colors.erase(it); break; }
        m_colors.insert(m_colors.begin(), color);
        if (int(m_colors.size()) > Columns * Rows) m_colors.pop_back();
        update();
    }

signals:
    void colorClicked(const QColor& color);

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        for (int i = 0; i < Columns * Rows; ++i) {
            const QRect r((i % Columns) * Cell, (i / Columns) * Cell, Cell, Cell);
            if (i < int(m_colors.size())) {
                p.fillRect(r.adjusted(1, 1, -1, -1), m_colors[size_t(i)]);
                p.setPen(QColor(20, 20, 20));
            } else {
                p.setPen(QColor(90, 90, 90));
            }
            p.drawRect(r.adjusted(1, 1, -2, -2));
        }
    }
    void mousePressEvent(QMouseEvent* e) override {
        if (e->button() != Qt::LeftButton) return;
        const int col = int(e->position().x()) / Cell;
        const int row = int(e->position().y()) / Cell;
        const int i = row * Columns + col;
        if (col < Columns && i >= 0 && i < int(m_colors.size())) emit colorClicked(m_colors[size_t(i)]);
    }

private:
    std::vector<QColor> m_colors;
};