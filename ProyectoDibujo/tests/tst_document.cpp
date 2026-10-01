#include <QtTest>

#include "core/Document.h"
#include "tools/BuiltinTools.h"
#include "tools/ToolManager.h"

class DocumentTest : public QObject {
    Q_OBJECT
private slots:
    void startsWithOneLayer() {
        Document doc(QSize(32, 32));
        QCOMPARE(doc.layerCount(), 1);
        QVERIFY(doc.activeLayer() != nullptr);
    }

    void addLayerIsUndoable() {
        Document doc(QSize(32, 32));
        doc.addLayer();
        QCOMPARE(doc.layerCount(), 2);
        doc.undoStack()->undo();
        QCOMPARE(doc.layerCount(), 1);
        doc.undoStack()->redo();
        QCOMPARE(doc.layerCount(), 2);
    }

    void lastLayerCannotBeRemoved() {
        Document doc(QSize(32, 32));
        doc.removeLayer(doc.activeLayerId());
        QCOMPARE(doc.layerCount(), 1);
    }

    void removeAndUndoKeepsPixels() {
        Document doc(QSize(8, 8));
        doc.addLayer();
        Layer* top = doc.activeLayer();
        top->image().setPixelColor(1, 1, Qt::red);
        const Layer::Id id = top->id();
        doc.removeLayer(id);
        QCOMPARE(doc.layerCount(), 1);
        doc.undoStack()->undo();
        QCOMPARE(doc.layerCount(), 2);
        QCOMPARE(doc.layerById(id)->image().pixelColor(1, 1), QColor(Qt::red));
    }

    void brushStrokeUndoRedo() {
        Document doc(QSize(64, 64));
        ToolManager tools;
        registerBuiltinTools(tools);
        tools.setColor(Qt::red);
        tools.setSize(6);
        tools.setActiveTool("brush");

        ToolContext ctx{doc, tools};
        ToolEvent e;
        e.pos = QPointF(20, 20);
        tools.activeTool()->mousePress(e, ctx);
        e.pos = QPointF(30, 20);
        tools.activeTool()->mouseMove(e, ctx);
        tools.activeTool()->mouseRelease(e, ctx);

        QVERIFY(doc.activeLayer()->image().pixelColor(25, 20).alpha() > 0);
        doc.undoStack()->undo();
        QCOMPARE(doc.activeLayer()->image().pixelColor(25, 20).alpha(), 0);
        doc.undoStack()->redo();
        QVERIFY(doc.activeLayer()->image().pixelColor(25, 20).alpha() > 0);
    }

    void eyedropperReadsCompositeColor() {
        Document doc(QSize(16, 16));
        doc.activeLayer()->image().setPixelColor(3, 3, QColor(10, 20, 30));
        QCOMPARE(doc.colorAt(QPoint(3, 3)), QColor(10, 20, 30));
        QCOMPARE(doc.colorAt(QPoint(0, 0)), QColor(Qt::white)); // papel
    }
};

QTEST_GUILESS_MAIN(DocumentTest)
#include "tst_document.moc"
