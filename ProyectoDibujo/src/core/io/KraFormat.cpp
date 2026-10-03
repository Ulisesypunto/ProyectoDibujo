#include "core/io/KraFormat.h"

#include <QBuffer>
#include <QFile>
#include <QFileInfo>
#include <QPainter>
#include <QRegularExpression>
#include <QUuid>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <cstring>
#include <functional>

// API privada de Qt (Qt6::GuiPrivate). Aislada aqui: si algun dia se cambia por
// miniz/libzip solo hay que tocar este archivo.
#include <private/qzipreader_p.h>
#include <private/qzipwriter_p.h>

namespace {

constexpr int TileSide = 64;
constexpr int PixelSize = 4; // RGBA 8 bits (en memoria B,G,R,A, sin premultiplicar)

// ---------------- LZF (descompresion, algoritmo de liblzf) ----------------
bool lzfDecompress(const uchar* in, int inLen, uchar* out, int outLen) {
    const uchar* ip = in;
    const uchar* const iend = in + inLen;
    uchar* op = out;
    uchar* const oend = out + outLen;

    while (ip < iend) {
        unsigned ctrl = *ip++;
        if (ctrl < 32) { // tira de literales
            ++ctrl;
            if (op + ctrl > oend || ip + ctrl > iend) return false;
            std::memcpy(op, ip, ctrl);
            ip += ctrl;
            op += ctrl;
        } else {         // referencia hacia atras
            unsigned len = ctrl >> 5;
            const uchar* ref = op - ((ctrl & 0x1f) << 8) - 1;
            if (len == 7) {
                if (ip >= iend) return false;
                len += *ip++;
            }
            if (ip >= iend) return false;
            ref -= *ip++;
            len += 2;
            if (op + len > oend || ref < out) return false;
            while (len--) *op++ = *ref++; // copia byte a byte (puede solaparse)
        }
    }
    return op == oend;
}

// ---------------- lectura ----------------
struct LayerInfo {
    QString name;
    QString file;
    int x = 0;
    int y = 0;
    qreal opacity = 1.0;
    bool visible = true;
    bool isGroup = false;
    bool clipped = false;
    bool expanded = true;
    int parent = -1;   // indice en ParsedMainDoc::layers (-1 = raiz)
    QImage image;
    bool ok = false;
};

struct ParsedMainDoc {
    QString imageName;
    QSize size;
    std::vector<LayerInfo> layers; // preorden del XML: en cada nivel la primera es la de ARRIBA
    int skippedNodes = 0;
};

bool parseMainDoc(const QByteArray& xml, ParsedMainDoc& out, QString& error) {
    QXmlStreamReader reader(xml);
    std::vector<int> parents; // pila: padre que tendran los <layer> anidados
    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.isEndElement()) {
            if (reader.name() == QLatin1String("layer") && !parents.empty()) parents.pop_back();
            continue;
        }
        if (!reader.isStartElement()) continue;
        const QXmlStreamAttributes a = reader.attributes();
        auto attr = [&a](const char* n) { return a.value(QLatin1String(n)).toString(); };

        if (reader.name() == QLatin1String("IMAGE")) {
            out.imageName = attr("name");
            out.size = QSize(attr("width").toInt(), attr("height").toInt());
        } else if (reader.name() == QLatin1String("layer")) {
            const int parent = parents.empty() ? -1 : parents.back();
            const QString type = attr("nodetype");
            const bool group = type == QLatin1String("grouplayer");
            if (!group && type != QLatin1String("paintlayer")) {
                ++out.skippedNodes;
                parents.push_back(parent); // mantiene la pila equilibrada
                continue;
            }
            LayerInfo info;
            info.name = attr("name");
            info.file = attr("filename");
            info.x = attr("x").toInt();
            info.y = attr("y").toInt();
            info.opacity = attr("opacity").isEmpty() ? 1.0 : qBound(0.0, attr("opacity").toInt() / 255.0, 1.0);
            info.visible = attr("visible") != QLatin1String("0");
            info.isGroup = group;
            info.clipped = attr("inheritalpha") == QLatin1String("1");
            info.expanded = attr("collapsed") != QLatin1String("1");
            info.parent = parent;
            out.layers.push_back(info);
            parents.push_back(group ? int(out.layers.size()) - 1 : parent);
        }
    }
    if (reader.hasError()) { error = reader.errorString(); return false; }
    return true;
}

// Decodifica un fichero de capa de Krita (teselas de 64x64 en planos B,G,R,A).
// Escribe en 'target' (ARGB32 NO premultiplicado, del tamano del documento).
bool decodeLayer(const QByteArray& data, const QPoint& offset, QImage& target) {
    int pos = 0;
    auto readLine = [&](QByteArray& line) {
        const int end = data.indexOf('\n', pos);
        if (end < 0) return false;
        line = data.mid(pos, end - pos);
        pos = end + 1;
        return true;
    };

    int tileW = TileSide, tileH = TileSide, pixelSize = PixelSize, tileCount = -1;
    QByteArray line;
    while (tileCount < 0) {
        if (!readLine(line)) return false;
        const QList<QByteArray> kv = line.split(' ');
        if (kv.size() != 2) return false;
        const int v = kv[1].toInt();
        if (kv[0] == "TILEWIDTH") tileW = v;
        else if (kv[0] == "TILEHEIGHT") tileH = v;
        else if (kv[0] == "PIXELSIZE") pixelSize = v;
        else if (kv[0] == "DATA") tileCount = v;
    }
    if (pixelSize != PixelSize || tileW <= 0 || tileH <= 0 || tileW > 1024 || tileH > 1024) return false;

    const int tilePixels = tileW * tileH;
    const int tileBytes = tilePixels * pixelSize;
    QByteArray planar(tileBytes, 0);
    uchar* dst = reinterpret_cast<uchar*>(planar.data());

    for (int i = 0; i < tileCount; ++i) {
        if (!readLine(line)) return false;
        const QList<QByteArray> f = line.split(',');
        if (f.size() != 4 || f[2] != "LZF") return false;
        const int tx = f[0].toInt(), ty = f[1].toInt(), size = f[3].toInt();
        if (size < 1 || pos + size > data.size()) return false;

        const uchar* chunk = reinterpret_cast<const uchar*>(data.constData()) + pos;
        pos += size;
        if (chunk[0] == 0) { // sin comprimir
            if (size - 1 != tileBytes) return false;
            std::memcpy(dst, chunk + 1, size_t(tileBytes));
        } else if (!lzfDecompress(chunk + 1, size - 1, dst, tileBytes)) {
            return false;
        }

        const uchar* b = dst;
        const uchar* g = b + tilePixels;
        const uchar* r = g + tilePixels;
        const uchar* al = r + tilePixels;
        for (int row = 0; row < tileH; ++row) {
            const int py = offset.y() + ty + row;
            if (py < 0 || py >= target.height()) continue;
            QRgb* scan = reinterpret_cast<QRgb*>(target.scanLine(py));
            for (int col = 0; col < tileW; ++col) {
                const int px = offset.x() + tx + col;
                if (px < 0 || px >= target.width()) continue;
                const int idx = row * tileW + col;
                scan[px] = qRgba(r[idx], g[idx], b[idx], al[idx]);
            }
        }
    }
    return true;
}

// ---------------- escritura ----------------
QByteArray encodeLayer(const QImage& argb) { // ARGB32 no premultiplicado
    const int W = argb.width(), H = argb.height();
    constexpr int tilePixels = TileSide * TileSide;
    constexpr int tileBytes = tilePixels * PixelSize;

    QByteArray body;
    QByteArray planar(tileBytes, 0);
    uchar* p = reinterpret_cast<uchar*>(planar.data());
    int count = 0;

    for (int ty = 0; ty < H; ty += TileSide) {
        for (int tx = 0; tx < W; tx += TileSide) {
            std::memset(p, 0, size_t(tileBytes));
            bool any = false;
            for (int y = 0; y < TileSide && ty + y < H; ++y) {
                const QRgb* scan = reinterpret_cast<const QRgb*>(argb.constScanLine(ty + y));
                for (int x = 0; x < TileSide && tx + x < W; ++x) {
                    const QRgb px = scan[tx + x];
                    if (qAlpha(px) == 0) continue;
                    any = true;
                    const int idx = y * TileSide + x;
                    p[idx] = uchar(qBlue(px));
                    p[tilePixels + idx] = uchar(qGreen(px));
                    p[2 * tilePixels + idx] = uchar(qRed(px));
                    p[3 * tilePixels + idx] = uchar(qAlpha(px));
                }
            }
            if (!any) continue; // tesela vacia = pixel por defecto (transparente)
            ++count;
            body += QByteArray::number(tx) + ',' + QByteArray::number(ty) + ",LZF," +
                    QByteArray::number(tileBytes + 1) + '\n';
            body.append('\0'); // 0 = datos sin comprimir (el ZIP ya los comprime)
            body.append(planar);
        }
    }
    const QByteArray header = "VERSION 2\nTILEWIDTH 64\nTILEHEIGHT 64\nPIXELSIZE 4\nDATA " +
                              QByteArray::number(count) + '\n';
    return header + body;
}

QByteArray toPng(const QImage& image) {
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return bytes;
}

void writeLayerNodes(QXmlStreamWriter& w, const Document& doc, Layer::Id parent) {
    for (int i = doc.layerCount() - 1; i >= 0; --i) { // Krita: la primera es la de arriba
        const Layer* l = doc.layerAt(i);
        if (l->parentId() != parent) continue;
        const bool group = l->isGroup();
        if (group) w.writeStartElement(QStringLiteral("layer"));
        else w.writeEmptyElement(QStringLiteral("layer"));
        w.writeAttribute(QStringLiteral("name"), l->name());
        w.writeAttribute(QStringLiteral("opacity"), QString::number(qRound(l->opacity() * 255)));
        w.writeAttribute(QStringLiteral("visible"), l->isVisible() ? QStringLiteral("1") : QStringLiteral("0"));
        w.writeAttribute(QStringLiteral("locked"), QStringLiteral("0"));
        w.writeAttribute(QStringLiteral("collapsed"), (group && !l->isExpanded()) ? QStringLiteral("1") : QStringLiteral("0"));
        w.writeAttribute(QStringLiteral("filename"), QStringLiteral("layer%1").arg(i + 2));
        w.writeAttribute(QStringLiteral("nodetype"), group ? QStringLiteral("grouplayer") : QStringLiteral("paintlayer"));
        if (group) w.writeAttribute(QStringLiteral("passthrough"), QStringLiteral("0"));
        w.writeAttribute(QStringLiteral("compositeop"), QStringLiteral("normal"));
        w.writeAttribute(QStringLiteral("colorspacename"), QStringLiteral("RGBA"));
        w.writeAttribute(QStringLiteral("channelflags"), QString());
        w.writeAttribute(QStringLiteral("colorlabel"), QStringLiteral("0"));
        w.writeAttribute(QStringLiteral("x"), QStringLiteral("0"));
        w.writeAttribute(QStringLiteral("y"), QStringLiteral("0"));
        w.writeAttribute(QStringLiteral("uuid"), QUuid::createUuid().toString());
        w.writeAttribute(QStringLiteral("intimeline"), QStringLiteral("0"));
        w.writeAttribute(QStringLiteral("onionskin"), QStringLiteral("0"));
        w.writeAttribute(QStringLiteral("inheritalpha"), l->isClipped() ? QStringLiteral("1") : QStringLiteral("0")); // atributo propio
        if (group) {
            w.writeStartElement(QStringLiteral("layers"));
            writeLayerNodes(w, doc, l->id());
            w.writeEndElement(); // layers
            w.writeEndElement(); // layer
        }
    }
}

QByteArray buildMainDoc(const Document& doc, const QString& name) {
    QByteArray out;
    QXmlStreamWriter w(&out);
    w.setAutoFormatting(true);
    w.writeStartDocument();
    w.writeDTD(QStringLiteral(
        "<!DOCTYPE DOC PUBLIC '-//KDE//DTD krita 2.0//EN' 'http://www.calligra.org/DTD/krita-2.0.dtd'>"));
    w.writeStartElement(QStringLiteral("DOC"));
    w.writeDefaultNamespace(QStringLiteral("http://www.calligra.org/DTD/krita"));
    w.writeAttribute(QStringLiteral("kritaVersion"), QStringLiteral("5.2.0"));
    w.writeAttribute(QStringLiteral("editor"), QStringLiteral("Krita"));
    w.writeAttribute(QStringLiteral("syntaxVersion"), QStringLiteral("2.0"));

    w.writeStartElement(QStringLiteral("IMAGE"));
    w.writeAttribute(QStringLiteral("mime"), QStringLiteral("application/x-kra"));
    w.writeAttribute(QStringLiteral("name"), name);
    w.writeAttribute(QStringLiteral("width"), QString::number(doc.size().width()));
    w.writeAttribute(QStringLiteral("height"), QString::number(doc.size().height()));
    w.writeAttribute(QStringLiteral("colorspacename"), QStringLiteral("RGBA"));
    w.writeAttribute(QStringLiteral("profile"), QStringLiteral("sRGB-elle-V2-srgbtrc.icc"));
    w.writeAttribute(QStringLiteral("description"), QString());
    w.writeAttribute(QStringLiteral("x-res"), QStringLiteral("100"));
    w.writeAttribute(QStringLiteral("y-res"), QStringLiteral("100"));

    w.writeStartElement(QStringLiteral("layers"));
    w.writeEndElement(); // layers
    w.writeEndElement(); // IMAGE
    w.writeEndElement(); // DOC
    w.writeEndDocument();
    w.writeStartElement(QStringLiteral("layers"));
    writeLayerNodes(w, doc, Layer::InvalidId);
    w.writeEndElement(); // layers
    return out;
}

} // namespace

namespace KraFormat {

bool isProjectFile(const QString& path) {
    const QString s = QFileInfo(path).suffix().toLower();
    return s == QLatin1String("kra") || s == QLatin1String("krz");
}

LoadResult load(const QString& path) {
    LoadResult res;
    QZipReader zip(path);
    if (!zip.isReadable()) {
        res.error = QObject::tr("No se pudo abrir el archivo (no es un ZIP valido).");
        return res;
    }
    if (!zip.fileData(QStringLiteral("mimetype")).contains("krita")) {
        res.error = QObject::tr("El archivo no es un proyecto de Krita.");
        return res;
    }

    ParsedMainDoc parsed;
    QString xmlError;
    if (!parseMainDoc(zip.fileData(QStringLiteral("maindoc.xml")), parsed, xmlError)) {
        res.error = QObject::tr("maindoc.xml danado: %1").arg(xmlError);
        return res;
    }
    const QSize size = parsed.size;
    if (size.width() < 1 || size.height() < 1 || size.width() > MaxSide || size.height() > MaxSide) {
        res.error = QObject::tr("Tamano de imagen no valido o demasiado grande (%1x%2, maximo %3).")
                        .arg(size.width()).arg(size.height()).arg(MaxSide);
        return res;
    }
    if (parsed.skippedNodes > 0)
        res.warnings << QObject::tr("Se omitieron %1 elemento(s) no soportado(s) (mascaras, capas vectoriales, filtros...).")
                            .arg(parsed.skippedNodes);

        bool anyPaint = false;
    for (LayerInfo& info : parsed.layers) {
        if (info.isGroup) { info.ok = true; continue; }
        const QByteArray data = zip.fileData(parsed.imageName + QStringLiteral("/layers/") + info.file);
        QImage img(size, QImage::Format_ARGB32);
        img.fill(Qt::transparent);
        if (data.isEmpty() || !decodeLayer(data, QPoint(info.x, info.y), img)) {
            res.warnings << QObject::tr("No se pudo leer la capa «%1» (formato de pixel no soportado).").arg(info.name);
            continue;
        }
        info.image = std::move(img);
        info.ok = anyPaint = true;
    }

    auto doc = std::make_unique<Document>(size);
    if (!anyPaint) {
        // Plan B: la imagen ya compuesta que guarda Krita
        QImage merged;
        merged.loadFromData(zip.fileData(QStringLiteral("mergedimage.png")), "PNG");
        if (merged.isNull()) {
            res.error = QObject::tr("El archivo no contiene capas legibles ni imagen compuesta.");
            return res;
        }
        QPainter p(&doc->layerAt(0)->image());
        p.setCompositionMode(QPainter::CompositionMode_Source);
        p.drawImage(0, 0, merged);
        p.end();
        doc->layerAt(0)->setName(QObject::tr("Imagen"));
        res.warnings << QObject::tr("Se cargo la imagen compuesta (sin capas).");
    } else {
        // Hermanas de abajo a arriba, recursivo por carpetas
        std::function<void(int, Layer::Id)> build = [&](int parentIdx, Layer::Id parentId) {
            for (int i = int(parsed.layers.size()) - 1; i >= 0; --i) {
                LayerInfo& info = parsed.layers[size_t(i)];
                if (info.parent != parentIdx || !info.ok) continue;
                Layer* layer = doc->addLayerNoHistory(info.name, parentId, info.isGroup);
                layer->setOpacity(info.opacity);
                layer->setVisible(info.visible);
                layer->setClipped(info.clipped);
                if (info.isGroup) {
                    layer->setExpanded(info.expanded);
                    build(i, layer->id());
                } else {
                    layer->image() = info.image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
                }
            }
        };
        build(-1, Layer::InvalidId);
        doc->takeLayer(0); // la capa vacia que crea Document al construirse
        for (int i = doc->layerCount() - 1; i >= 0; --i)
            if (!doc->layerAt(i)->isGroup()) { doc->setActiveLayer(doc->layerAt(i)->id()); break; }
    }
    res.document = std::move(doc);
    return res;
}

bool save(const Document& doc, const QString& path, QString* error) {
    auto fail = [error](const QString& msg) {
        if (error) *error = msg;
        return false;
    };

    QString name = QFileInfo(path).completeBaseName();
    name.replace(QRegularExpression(QStringLiteral("[/\\\\:*?\"<>|]")), QStringLiteral("_"));
    if (name.isEmpty()) name = QStringLiteral("Unnamed");

    const QString tmp = path + QStringLiteral(".part"); // no destruir el original si algo falla
    {
        QZipWriter zip(tmp);
        if (!zip.isWritable()) return fail(QObject::tr("No se puede escribir en «%1».").arg(path));

        // 'mimetype' primero y sin comprimir (como exige el formato)
        zip.setCompressionPolicy(QZipWriter::NeverCompress);
        zip.addFile(QStringLiteral("mimetype"), QByteArray("application/x-krita"));
        zip.setCompressionPolicy(QZipWriter::AutoCompress);

        zip.addFile(QStringLiteral("maindoc.xml"), buildMainDoc(doc, name));
        zip.addFile(QStringLiteral("documentinfo.xml"),
            QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                           "<document-info xmlns=\"http://www.calligra.org/DTD/document-info\">\n"
                           " <about><title>%1</title></about>\n"
                           " <author><full-name></full-name></author>\n"
                           "</document-info>\n").arg(name.toHtmlEscaped()).toUtf8());

        for (int i = 0; i < doc.layerCount(); ++i) {
            if (doc.layerAt(i)->isGroup()) continue;
            const QString base = QStringLiteral("%1/layers/layer%2").arg(name).arg(i + 2);
            zip.addFile(base, encodeLayer(doc.layerAt(i)->image().convertToFormat(QImage::Format_ARGB32)));
            zip.addFile(base + QStringLiteral(".defaultpixel"), QByteArray(4, '\0'));
        }
        zip.addFile(QStringLiteral("mergedimage.png"), toPng(doc.flatten(false)));
        zip.addFile(QStringLiteral("preview.png"), toPng(doc.thumbnail(QSize(256, 256))));
        zip.close();
        if (zip.status() != QZipWriter::NoError) {
            QFile::remove(tmp);
            return fail(QObject::tr("Error al escribir el archivo."));
        }
    }
    QFile::remove(path);
    if (!QFile::rename(tmp, path)) {
        QFile::remove(tmp);
        return fail(QObject::tr("No se pudo reemplazar «%1».").arg(path));
    }
    return true;
}

} // namespace KraFormat
