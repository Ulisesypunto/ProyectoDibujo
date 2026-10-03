#pragma once
#include "tools/Tool.h"

// Cubeta: rellena la zona contigua de color parecido en la capa ACTIVA.
// Respeta la seleccion y la tolerancia del ToolManager.
class FillTool : public Tool {
public:
    QString id() const override { return QStringLiteral("fill"); }
    QString displayName() const override { return QObject::tr("Cubeta"); }
    QKeySequence shortcut() const override { return QKeySequence(Qt::Key_G); }
    bool usesTolerance() const override { return true; }

    void mousePress(const ToolEvent& event, ToolContext& ctx) override;
    void mouseMove(const ToolEvent&, ToolContext&) override {}
    void mouseRelease(const ToolEvent&, ToolContext&) override {}
};