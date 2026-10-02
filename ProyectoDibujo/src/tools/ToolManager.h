#pragma once
#include <QColor>
#include <QObject>
#include <memory>
#include <vector>

#include "tools/Tool.h"

// Registro de herramientas + ajustes compartidos (color, tamano).
// La UI y el lienzo hablan con esta clase, nunca entre si.
class ToolManager : public QObject {
    Q_OBJECT
public:
    static constexpr int MinSize = 1;
    static constexpr int MaxSize = 200;
    static constexpr int MaxTolerance = 255;

    explicit ToolManager(QObject* parent = nullptr);

    void registerTool(std::unique_ptr<Tool> tool);
    Tool* tool(const QString& id) const;
    std::vector<Tool*> tools() const; // en orden de registro

    Tool* activeTool() const { return m_active; }
    void setActiveTool(const QString& id);

    QColor color() const { return m_color; }
    void setColor(const QColor& color);

    int size() const { return m_size; }
    void setSize(int size);

    // Lo llaman las herramientas que pintan con color (alimenta el historial)
    void noteColorUsed() { emit colorUsed(m_color); }

    int tolerance() const { return m_tolerance; }
    void setTolerance(int tolerance);

signals:
    void activeToolChanged(Tool* tool);
    void colorChanged(const QColor& color);
    void colorUsed(const QColor& color);
    void sizeChanged(int size);
    void toleranceChanged(int tolerance);

private:
    std::vector<std::unique_ptr<Tool>> m_tools;
    Tool* m_active = nullptr;
    QColor m_color = Qt::black;
    int m_size = 5;
    int m_tolerance = 32;
};