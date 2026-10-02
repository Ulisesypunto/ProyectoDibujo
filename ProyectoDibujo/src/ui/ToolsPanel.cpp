#include "ui/ToolsPanel.h"
#include "tools/ToolManager.h"

#include <QAction>
#include <QActionGroup>
#include <QToolButton>
#include <QVBoxLayout>

ToolsPanel::ToolsPanel(ToolManager* tools, QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    auto* group = new QActionGroup(this);
    group->setExclusive(true);

    for (Tool* tool : tools->tools()) {
        auto* action = new QAction(tool->displayName(), group);
        action->setCheckable(true);
        action->setChecked(tool == tools->activeTool());
        action->setShortcut(tool->shortcut());
        action->setShortcutContext(Qt::ApplicationShortcut); // funciona aunque un panel flotante tenga el foco
        action->setData(tool->id());
        connect(action, &QAction::triggered, this, [tools, id = tool->id()] { tools->setActiveTool(id); });
        m_actions.append(action);

        // El texto "Pincel (B)" sigue al atajo, que ahora el usuario puede cambiar.
        auto updateText = [action, name = tool->displayName()] {
            const QString key = action->shortcut().toString(QKeySequence::NativeText);
            const QString text = key.isEmpty() ? name : ToolsPanel::tr("%1 (%2)").arg(name, key);
            if (action->text() != text) action->setText(text); // evita bucle con changed()
        };
        updateText();
        connect(action, &QAction::changed, action, updateText);

        auto* button = new QToolButton(this);
        button->setDefaultAction(action);
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        button->setMinimumHeight(28);
        layout->addWidget(button);
    }
    layout->addStretch(1);

    connect(tools, &ToolManager::activeToolChanged, this, [group](Tool* active) {
        for (QAction* a : group->actions())
            if (a->data().toString() == active->id()) a->setChecked(true);
    });
}
