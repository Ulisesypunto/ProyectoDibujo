#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidget>
#include <QPushButton>
#include <QDockWidget>
#include <QSlider>
#include <QSpinBox>
#include <QColorDialog>
#include <QShortcut>

#include "Canvas.h"
#include "BrushTool.h"
#include "EraserTool.h"
#include "EyedropperTool.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override = default;

private slots:
    void onAddLayerClicked();
    void onLayerSelected(int index);
    void onSelectColorClicked();

private:
    // Componentes principales
    Canvas *canvas;
    
    // Herramientas (instancias persistentes)
    BrushTool brushTool;
    EraserTool eraserTool;
    EyedropperTool eyedropperTool;

    // Componentes de la interfaz
    QListWidget *layersListWidget;
    QPushButton *addLayerButton;
    QPushButton *colorButton;
    QSlider *sizeSlider;
    QSpinBox *sizeSpinBox;

    // Estado local de la UI
    QColor currentColor = Qt::black;

    // Métodos de inicialización
    void createToolBar();
    void createLayersDock();
    void setupShortcuts();
    void updateLayersList();
    void updateColorButton();
};

#endif // MAINWINDOW_H