#pragma once
#include <QWidget>

class QSlider;
class QSpinBox;
class ToolManager;

// Tamaño del pincel. Solo habla con el ToolManager.
class ToolOptionsWidget : public QWidget {
    Q_OBJECT
public:
    explicit ToolOptionsWidget(ToolManager* tools, QWidget* parent = nullptr);

private:
    ToolManager* m_tools;
    QSlider* m_slider;
    QSpinBox* m_spin;
};