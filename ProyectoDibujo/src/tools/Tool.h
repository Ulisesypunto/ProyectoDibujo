#pragma once
#include <QKeySequence>
#include <QPointF>
#include <QString>
#include <QObject>

class Document;
class ToolManager;
class QPainter;
class QTransform;

// Evento ya convertido a coordenadas del DOCUMENTO (independiente de zoom/pan).
struct ToolEvent {
    QPointF pos;
    qreal pressure = 1.0; // reservado para tabletas Wacom
    qreal viewScale = 1.0; // pixeles de pantalla por pixel de documento (zoom)
    Qt::KeyboardModifiers modifiers;
};

// Todo lo que una herramienta puede tocar. Si mañana hace falta mas
// (seleccion, mascara...), se anade aqui sin cambiar la interfaz Tool.
struct ToolContext {
    Document& document;
    ToolManager& manager; // color, tamano y demas ajustes compartidos
};

// Interfaz base. Para crear una herramienta nueva: heredar de Tool (o de
// PaintTool si pinta pixeles) y registrarla en BuiltinTools.cpp.
class Tool {
public:
    virtual ~Tool() = default;

    virtual QString id() const = 0;            // identificador estable ("brush")
    virtual QString displayName() const = 0;
    virtual QKeySequence shortcut() const { return {}; }
    virtual Qt::CursorShape cursor() const { return Qt::CrossCursor; }
    virtual bool usesBrushSize() const { return false; } // muestra circulo de tamano
    virtual bool usesTolerance() const { return false; }
    virtual void paintOverlay(QPainter&, const QTransform& /*docToWidget*/) const {}

    // Ganchos opcionales (herramientas con estado, como Transformar)
    virtual void hover(const ToolEvent&, ToolContext&) {}        // raton sin pulsar
    virtual bool keyPress(int /*key*/, ToolContext&) { return false; } // true = consumida
    virtual void activated(ToolContext&) {}
    virtual void deactivated(ToolContext&) {}                    // dejar todo aplicado
    virtual void commit(ToolContext&) {}                         // aplicar lo pendiente
    virtual void contextChanged(ToolContext&) {}                 // cambio de capa/seleccion/pixeles

    virtual void mousePress(const ToolEvent& event, ToolContext& ctx) = 0;
    virtual void mouseMove(const ToolEvent& event, ToolContext& ctx) = 0;
    virtual void mouseRelease(const ToolEvent& event, ToolContext& ctx) = 0;
};
