#include "tools/ToolManager.h"

ToolManager::ToolManager(QObject* parent) : QObject(parent) {}

void ToolManager::registerTool(std::unique_ptr<Tool> tool) {
    m_tools.push_back(std::move(tool));
    if (!m_active) m_active = m_tools.back().get(); // la primera es la activa por defecto
}

Tool* ToolManager::tool(const QString& id) const {
    for (const auto& t : m_tools)
        if (t->id() == id) return t.get();
    return nullptr;
}

std::vector<Tool*> ToolManager::tools() const {
    std::vector<Tool*> out;
    out.reserve(m_tools.size());
    for (const auto& t : m_tools) out.push_back(t.get());
    return out;
}

void ToolManager::setActiveTool(const QString& id) {
    Tool* t = tool(id);
    if (!t || t == m_active) return;
    m_active = t;
    emit activeToolChanged(t);
}

void ToolManager::setColor(const QColor& color) {
    if (!color.isValid() || color == m_color) return;
    m_color = color;
    emit colorChanged(m_color);
}

void ToolManager::setSize(int size) {
    size = qBound(MinSize, size, MaxSize);
    if (size == m_size) return;
    m_size = size;
    emit sizeChanged(m_size);
}

void ToolManager::setTolerance(int tolerance) {
    tolerance = qBound(0, tolerance, MaxTolerance);
    if (tolerance == m_tolerance) return;
    m_tolerance = tolerance;
    emit toleranceChanged(m_tolerance);
}