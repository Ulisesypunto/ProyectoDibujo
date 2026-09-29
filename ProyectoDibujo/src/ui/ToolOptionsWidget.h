#pragma once
#include <QWidget>

class QPushButton;
class QSlider;
class QSpinBox;
class ToolManager;

// Selector de color + tamano. Solo habla con el ToolManager.
class ToolOptionsWidget : public QWidget {
    Q_OBJECT
public:
    explicit ToolOptionsWidget(ToolManager* tools, QWidget* parent = nullptr);

private:
    void updateColorButton();
    void chooseColor();

    ToolManager* m_tools;
    QPushButton* m_colorButton;
    QSlider* m_slider;
    QSpinBox* m_spin;
};
