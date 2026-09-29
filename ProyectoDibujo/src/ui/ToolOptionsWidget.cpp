#include "ui/ToolOptionsWidget.h"
#include "tools/ToolManager.h"

#include <QColorDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>

ToolOptionsWidget::ToolOptionsWidget(ToolManager* tools, QWidget* parent)
    : QWidget(parent), m_tools(tools) {
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 0, 4, 0);

    layout->addWidget(new QLabel(tr("Color:"), this));
    m_colorButton = new QPushButton(this);
    m_colorButton->setFixedSize(28, 28);
    m_colorButton->setCursor(Qt::PointingHandCursor);
    layout->addWidget(m_colorButton);

    layout->addWidget(new QLabel(tr("Tamaño:"), this));
    m_slider = new QSlider(Qt::Horizontal, this);
    m_slider->setRange(ToolManager::MinSize, ToolManager::MaxSize);
    m_slider->setFixedWidth(120);
    m_spin = new QSpinBox(this);
    m_spin->setRange(ToolManager::MinSize, ToolManager::MaxSize);
    layout->addWidget(m_slider);
    layout->addWidget(m_spin);

    m_slider->setValue(m_tools->size());
    m_spin->setValue(m_tools->size());
    updateColorButton();

    // UI -> manager
    connect(m_colorButton, &QPushButton::clicked, this, &ToolOptionsWidget::chooseColor);
    connect(m_slider, &QSlider::valueChanged, m_tools, &ToolManager::setSize);
    connect(m_spin, QOverload<int>::of(&QSpinBox::valueChanged), m_tools, &ToolManager::setSize);

    // manager -> UI (tambien cuando cambia desde el gotero o un atajo)
    connect(m_tools, &ToolManager::colorChanged, this, [this] { updateColorButton(); });
    connect(m_tools, &ToolManager::sizeChanged, this, [this](int size) {
        const QSignalBlocker b1(m_slider);
        const QSignalBlocker b2(m_spin);
        m_slider->setValue(size);
        m_spin->setValue(size);
    });
}

void ToolOptionsWidget::updateColorButton() {
    m_colorButton->setStyleSheet(
        QStringLiteral("background-color: %1; border: 1px solid #666; border-radius: 4px;")
            .arg(m_tools->color().name()));
}

void ToolOptionsWidget::chooseColor() {
    const QColor c = QColorDialog::getColor(m_tools->color(), this, tr("Seleccionar color"));
    if (c.isValid()) m_tools->setColor(c);
}
