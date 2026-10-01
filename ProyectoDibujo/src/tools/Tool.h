#pragma once
#include <QKeySequence>
#include <QPointF>
#include <QString>

class Document;
class ToolManager;

// Evento ya convertido a coordenadas del DOCUMENTO (independiente de zoom/pan).
struct ToolEvent {
    QPointF pos;
    qreal pressure = 1.0; // reservado para tabletas Wacom
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

    virtual void mousePress(const ToolEvent& event, ToolContext& ctx) = 0;
    virtual void mouseMove(const ToolEvent& event, ToolContext& ctx) = 0;
    virtual void mouseRelease(const ToolEvent& event, ToolContext& ctx) = 0;
};
