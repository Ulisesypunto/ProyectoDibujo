#pragma once
#include <QWidget>

class ToolManager;

// Panel de color: rueda + cuadrado SV + historial. Solo habla con el ToolManager.
class ColorPickerPanel : public QWidget {
    Q_OBJECT
public:
    explicit ColorPickerPanel(ToolManager* tools, QWidget* parent = nullptr);
};