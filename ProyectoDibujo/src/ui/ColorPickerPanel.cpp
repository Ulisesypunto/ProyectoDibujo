#include "ui/ColorPickerPanel.h"
#include "tools/ToolManager.h"
#include "ui/ColorHistoryWidget.h"
#include "ui/ColorWheelWidget.h"

#include <QHBoxLayout>

ColorPickerPanel::ColorPickerPanel(ToolManager* tools, QWidget* parent) : QWidget(parent) {
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    auto* wheel = new ColorWheelWidget(this);
    auto* history = new ColorHistoryWidget(this);
    layout->addWidget(wheel, 1);
    layout->addWidget(history, 0, Qt::AlignTop);

    wheel->setColor(tools->color());

    // UI -> manager
    connect(wheel, &ColorWheelWidget::colorPicked, tools, &ToolManager::setColor);
    connect(history, &ColorHistoryWidget::colorClicked, tools, &ToolManager::setColor);
    // manager -> UI (gotero, historial...)
    connect(tools, &ToolManager::colorChanged, wheel, &ColorWheelWidget::setColor);
    connect(tools, &ToolManager::colorUsed, history, &ColorHistoryWidget::addColor);
}