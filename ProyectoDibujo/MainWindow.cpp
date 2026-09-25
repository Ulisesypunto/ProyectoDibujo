#include "MainWindow.h"
#include <QDockWidget>
#include <QLabel>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent) {
    setWindowTitle("Krota");
    resize(1280, 720);
    setupUI();
}

MainWindow::~MainWindow() {}

void MainWindow::setupUI() {
    // Espacio reservado para el Lienzo principal
    QLabel *canvasPlaceholder = new QLabel("Lienzo de dibujo (Próximamente)", this);
    canvasPlaceholder->setAlignment(Qt::AlignCenter);
    setCentralWidget(canvasPlaceholder);

    // Panel lateral para capas (Dock Widget)
    QDockWidget *layersDock = new QDockWidget("Capas", this);
    layersDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    QLabel *layersPlaceholder = new QLabel("Panel de Capas", layersDock);
    layersDock->setWidget(layersPlaceholder);
    addDockWidget(Qt::RightDockWidgetArea, layersDock);
}