#pragma once
#include <QPolygonF>
#include "tools/Tool.h"

// Lazo a mano alzada. Sin modificador: reemplaza la seleccion.
// Shift: suma · Alt: resta · Shift+Alt: interseca · clic sin arrastrar: deselecciona.
class LassoTool : public Tool {
public:
    QString id() const override { return QStringLiteral("lasso"); }
    QString displayName() const override { return QObject::tr("Lazo"); }
    QKeySequence shortcut() const override { return QKeySequence(Qt::Key_L); }

    void mousePress(const ToolEvent& event, ToolContext& ctx) override;
    void mouseMove(const ToolEvent& event, ToolContext& ctx) override;
    void mouseRelease(const ToolEvent& event, ToolContext& ctx) override;
    void paintOverlay(QPainter& painter, const QTransform& docToWidget) const override;

private:
    QPolygonF m_points;
    Qt::KeyboardModifiers m_mods;
    bool m_active = false;
};