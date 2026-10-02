#include <QtTest>
#include <QTemporaryDir>

#include "core/Document.h"
#include "core/io/KraFormat.h"

class KraTest : public QObject {
    Q_OBJECT
private slots:
    void roundTripKeepsLayersAndPixels() {
        Document doc(QSize(100, 70)); // 2x2 teselas de 64 px
        doc.activeLayer()->image().setPixelColor(70, 65, QColor(10, 200, 30)); // tesela (1,1)
        doc.addLayer(QStringLiteral("Arriba"));
        doc.activeLayer()->image().setPixelColor(1, 1, Qt::red);
        doc.setLayerOpacity(doc.activeLayerId(), 0.5);
        doc.setLayerVisible(doc.layerAt(0)->id(), false);

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("t.kra"));
        QString error;
        QVERIFY2(KraFormat::save(doc, path, &error), qPrintable(error));

        KraFormat::LoadResult result = KraFormat::load(path);
        QVERIFY2(static_cast<bool>(result.document), qPrintable(result.error));
        Document& loaded = *result.document;
        QCOMPARE(loaded.size(), doc.size());
        QCOMPARE(loaded.layerCount(), 2);
        QCOMPARE(loaded.layerAt(1)->name(), QStringLiteral("Arriba"));
        QVERIFY(!loaded.layerAt(0)->isVisible());
        QVERIFY(qAbs(loaded.layerAt(1)->opacity() - 0.5) < 0.01);
        QCOMPARE(loaded.layerAt(0)->image().pixelColor(70, 65), QColor(10, 200, 30));
        QCOMPARE(loaded.layerAt(1)->image().pixelColor(1, 1), QColor(Qt::red));
        QCOMPARE(loaded.layerAt(1)->image().pixelColor(50, 50).alpha(), 0);
    }

    void rejectsNonKraFiles() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QFile f(dir.filePath(QStringLiteral("x.kra")));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("no soy un zip");
        f.close();
        const KraFormat::LoadResult result = KraFormat::load(f.fileName());
        QVERIFY(!result.document);
        QVERIFY(!result.error.isEmpty());
    }

    void addLayerWithImageIsUndoable() {
        Document doc(QSize(20, 20));
        QImage img(4, 4, QImage::Format_ARGB32);
        img.fill(Qt::blue);
        doc.addLayerWithImage(img, QPoint(5, 5), QStringLiteral("x"), QStringLiteral("Pegar"));
        QCOMPARE(doc.layerCount(), 2);
        QCOMPARE(doc.activeLayer()->image().pixelColor(6, 6), QColor(Qt::blue));
        QCOMPARE(doc.activeLayer()->image().pixelColor(0, 0).alpha(), 0);
        doc.undoStack()->undo();
        QCOMPARE(doc.layerCount(), 1);
        doc.undoStack()->redo();
        QCOMPARE(doc.layerCount(), 2);
    }
};

QTEST_GUILESS_MAIN(KraTest)
#include "tst_kra.moc"
