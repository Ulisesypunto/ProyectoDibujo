#pragma once
#include <Qobject>
#include <Qstring>
#include "tools/Tool.h"

// Toma el color COMPUESTO (todas las capas visibles) bajo el cursor.
class EyedropperTool : public Tool {
public:
    QString id() const override { return QStringLiteral("eyedropper"); }
    QString displayName() const override { return QObject::tr("Gotero"); }
    QKeySequence shortcut() const override { return QKeySequence(Qt::Key_I); }

    void mousePress(const ToolEvent& event, ToolContext& ctx) override;
    void mouseMove(const ToolEvent& event, ToolContext& ctx) override;
    void mouseRelease(const ToolEvent&, ToolContext&) override {}

private:
    void sample(const ToolEvent& event, ToolContext& ctx);
};
