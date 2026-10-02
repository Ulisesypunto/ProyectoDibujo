#include "ui/ToolOptionsWidget.h"
#include "tools/ToolManager.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>

ToolOptionsWidget::ToolOptionsWidget(ToolManager* tools, QWidget* parent)
    : QWidget(parent), m_tools(tools) {
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(6, 2, 6, 2);

    layout->addWidget(new QLabel(tr("Tamaño:"), this));
    m_slider = new QSlider(Qt::Horizontal, this);
    m_slider->setRange(ToolManager::MinSize, ToolManager::MaxSize);
    m_slider->setMinimumWidth(100);
    m_spin = new QSpinBox(this);
    m_spin->setRange(ToolManager::MinSize, ToolManager::MaxSize);
    layout->addWidget(m_slider, 1);
    layout->addWidget(m_spin);

    m_slider->setValue(m_tools->size());
    m_spin->setValue(m_tools->size());

    connect(m_slider, &QSlider::valueChanged, m_tools, &ToolManager::setSize);
    connect(m_spin, QOverload<int>::of(&QSpinBox::valueChanged), m_tools, &ToolManager::setSize);
    connect(m_tools, &ToolManager::sizeChanged, this, [this](int size) {
        const QSignalBlocker b1(m_slider);
        const QSignalBlocker b2(m_spin);
        m_slider->setValue(size);
        m_spin->setValue(size);
    });
}