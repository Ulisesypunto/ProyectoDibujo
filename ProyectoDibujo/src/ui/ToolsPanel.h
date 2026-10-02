#pragma once
#include <QList>
#include <QWidget>

class QAction;
class ToolManager;

// Botones de herramientas generados desde el ToolManager (una herramienta
// nueva aparece aqui sola, con su atajo).
class ToolsPanel : public QWidget {
    Q_OBJECT
public:
    explicit ToolsPanel(ToolManager* tools, QWidget* parent = nullptr);
    const QList<QAction*>& toolActions() const { return m_actions; }

private:
    QList<QAction*> m_actions;
};