#pragma once
#include <QImage>
#include <QPainterPath>
#include <QPointF>
#include <QRect>
#include <QRectF>
#include <QTransform>

#include "core/Layer.h"
#include "tools/Tool.h"

// Transformacion libre estilo Krita: mover (dentro), rotar (fuera), escalar
// (tiradores), inclinar (Ctrl + tirador de lado) y eje de rotacion movible.
// Es "flotante": hasta confirmar (Enter / cambiar de herramienta) se parte
// siempre de la imagen original, asi que no se acumula perdida de calidad.
class TransformTool : public Tool {
public:
    QString id() const override { return QStringLiteral("transform"); }
    QString displayName() const override { return QObject::tr("Transformar"); }
    QKeySequence shortcut() const override { return QKeySequence(Qt::Key_T); }
    Qt::CursorShape cursor() const override { return m_cursor; }

    void mousePress(const ToolEvent& event, ToolContext& ctx) override;
    void mouseMove(const ToolEvent& event, ToolContext& ctx) override;
    void mouseRelease(const ToolEvent& event, ToolContext& ctx) override;
    void hover(const ToolEvent& event, ToolContext& ctx) override;
    bool keyPress(int key, ToolContext& ctx) override;
    void activated(ToolContext& ctx) override;
    void deactivated(ToolContext& ctx) override;
    void commit(ToolContext& ctx) override;
    void contextChanged(ToolContext& ctx) override;
    void paintOverlay(QPainter& painter, const QTransform& docToWidget) const override;

private:
    enum class Mode { None, Move, Rotate, Pivot, Scale, Shear };
    struct Hit {
        Mode mode = Mode::Rotate;
        int handle = -1;
    };

    // Tiradores: 0..3 esquinas (TL,TR,BR,BL), 4..7 lados (T,R,B,L)
    static QPointF handleUnit(int i);
    QPointF boxPoint(const QPointF& unit) const; // (0..1, 0..1) -> punto de la caja original
    QPointF handlePos(int i) const { return m_matrix.map(boxPoint(handleUnit(i))); }
    Hit hitTest(const QPointF& pos, qreal viewScale, Qt::KeyboardModifiers mods) const;

    void resetBox(ToolContext& ctx);       // calcula la caja (sin tocar pixeles)
    bool beginFloating(ToolContext& ctx);  // "levanta" lo seleccionado
    void render(ToolContext& ctx);
    void cancel(ToolContext& ctx);
    void clearState();

    // --- caja / estado ---
    bool m_valid = false;      // hay algo que transformar
    bool m_floating = false;   // ya se levanto el contenido
    bool m_changed = false;    // hay pixeles modificados sin confirmar
    bool m_busy = false;
    Layer::Id m_layerId = Layer::InvalidId;
    bool m_hasSel = false;
    QPainterPath m_selBefore, m_selNow;
    QRect m_liftRect;          // zona levantada (enteros)
    QRectF m_box;              // caja ORIGINAL (la transformacion se aplica encima)
    QImage m_before;           // capa completa al levantar
    QImage m_lifted;           // contenido levantado (solo liftRect)
    QRect m_dirty;
    QTransform m_matrix;       // transformacion total acumulada
    QPointF m_pivot;           // eje de rotacion (coordenadas del documento)

    // --- gesto en curso ---
    Mode m_mode = Mode::None;
    int m_handle = -1;
    QPointF m_press, m_pivot0;
    QTransform m_matrix0;

    Qt::CursorShape m_cursor = Qt::ArrowCursor;
};