#include "tools/FillTool.h"
#include "core/Document.h"
#include "core/commands/StrokeCommand.h"
#include "tools/ToolManager.h"

#include <QImage>
#include <QPainter>
#include <QtMath>
#include <cstdlib>
#include <vector>

void FillTool::mousePress(const ToolEvent& event, ToolContext& ctx) {
    Document& doc = ctx.document;
    Layer* layer = doc.activeLayer();
    if (!layer || !layer->isVisible()) return;

    QImage& img = layer->image(); // ARGB32 premultiplicado
    const int W = img.width(), H = img.height();
    const QPoint seed(qFloor(event.pos.x()), qFloor(event.pos.y()));
    if (!img.rect().contains(seed)) return;

    // Mascara de la seleccion (solo del tamano de su caja, para ahorrar memoria)
    const bool hasSel = doc.hasSelection();
    QImage mask;
    QPoint maskOrigin;
    if (hasSel) {
        const QRect box = doc.selection().boundingRect().toAlignedRect() & img.rect();
        if (box.isEmpty()) return;
        mask = QImage(box.size(), QImage::Format_ARGB32_Premultiplied);
        mask.fill(Qt::transparent);
        QPainter mp(&mask);
        mp.translate(-box.topLeft());
        mp.fillPath(doc.selection(), Qt::white);
        mp.end();
        maskOrigin = box.topLeft();
    }
    auto selected = [&](int x, int y) {
        if (!hasSel) return true;
        const int mx = x - maskOrigin.x(), my = y - maskOrigin.y();
        if (mx < 0 || my < 0 || mx >= mask.width() || my >= mask.height()) return false;
        return qAlpha(reinterpret_cast<const QRgb*>(mask.constScanLine(my))[mx]) > 127;
    };
    if (!selected(seed.x(), seed.y())) return;

    const QImage& cimg = img;
    auto at = [&](int x, int y) { return reinterpret_cast<const QRgb*>(cimg.constScanLine(y))[x]; };

    const QRgb target = at(seed.x(), seed.y());
    const QRgb fill = qPremultiply(ctx.manager.color().rgba());
    if (target == fill) return;

    const int tol = ctx.manager.tolerance();
    auto similar = [&](int x, int y) {
        const QRgb c = at(x, y);
        return std::abs(qAlpha(c) - qAlpha(target)) <= tol && std::abs(qRed(c) - qRed(target)) <= tol &&
               std::abs(qGreen(c) - qGreen(target)) <= tol && std::abs(qBlue(c) - qBlue(target)) <= tol;
    };

    // ---- Fase 1: calcular la region (sin tocar la imagen) con relleno por lineas ----
    std::vector<uchar> mark(size_t(W) * size_t(H), 0);
    auto canFill = [&](int x, int y) { return !mark[size_t(y) * W + x] && similar(x, y) && selected(x, y); };

    std::vector<QPoint> stack{seed};
    int minX = W, minY = H, maxX = -1, maxY = -1;
    while (!stack.empty()) {
        const QPoint p = stack.back();
        stack.pop_back();
        const int y = p.y();
        if (!canFill(p.x(), y)) continue;

        int l = p.x(), r = p.x();
        while (l > 0 && canFill(l - 1, y)) --l;
        while (r < W - 1 && canFill(r + 1, y)) ++r;
        for (int x = l; x <= r; ++x) mark[size_t(y) * W + x] = 1;
        minX = qMin(minX, l); maxX = qMax(maxX, r);
        minY = qMin(minY, y); maxY = qMax(maxY, y);

        for (int ny : {y - 1, y + 1}) {
            if (ny < 0 || ny >= H) continue;
            bool inSpan = false;
            for (int x = l; x <= r; ++x) {
                const bool good = canFill(x, ny);
                if (good && !inSpan) stack.emplace_back(x, ny);
                inSpan = good;
            }
        }
    }
    if (maxX < 0) return;

    // ---- Fase 2: aplicar + undo (solo la caja tocada) ----
    const QRect bounds(QPoint(minX, minY), QPoint(maxX, maxY));
    const QImage before = img.copy(bounds);
    for (int y = minY; y <= maxY; ++y) {
        QRgb* row = reinterpret_cast<QRgb*>(img.scanLine(y));
        for (int x = minX; x <= maxX; ++x)
            if (mark[size_t(y) * W + x]) row[x] = fill;
    }
    ctx.manager.noteColorUsed();
    doc.notifyContentChanged(bounds);
    doc.undoStack()->push(new StrokeCommand(&doc, layer->id(), bounds, before, img.copy(bounds),
                                            QObject::tr("Relleno")));
}