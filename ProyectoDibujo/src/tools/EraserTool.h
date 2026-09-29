#pragma once
#include "tools/PaintTool.h"

class EraserTool : public PaintTool {
public:
    QString id() const override { return QStringLiteral("eraser"); }
    QString displayName() const override { return QObject::tr("Goma"); }
    QKeySequence shortcut() const override { return QKeySequence(Qt::Key_E); }

protected:
    // Clear vuelve transparentes los pixeles (el color de la pluma no importa)
    QPainter::CompositionMode compositionMode() const override { return QPainter::CompositionMode_Clear; }
    QPen makePen(const ToolContext& ctx) const override {
        return QPen(Qt::black, ctx.manager.size(), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    }
    QString strokeName() const override { return QObject::tr("Borrado"); }
};
