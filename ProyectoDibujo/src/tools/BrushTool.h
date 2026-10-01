#pragma once
#include "tools/PaintTool.h"

class BrushTool : public PaintTool {
public:
    QString id() const override { return QStringLiteral("brush"); }
    QString displayName() const override { return QObject::tr("Pincel"); }
    QKeySequence shortcut() const override { return QKeySequence(Qt::Key_B); }

protected:
    QPen makePen(const ToolContext& ctx) const override {
        return QPen(ctx.manager.color(), ctx.manager.size(), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    }
    QString strokeName() const override { return QObject::tr("Trazo de pincel"); }
};
