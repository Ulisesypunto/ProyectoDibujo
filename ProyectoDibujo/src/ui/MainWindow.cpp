#include "MainWindow.h"
#include <QToolBar>
#include <QAction>
#include <QVBoxLayout>
#include <QLabel>
#include <QKeySequence>
#include <QImage>

MainWindow::MainWindow(QWidget *parent) 
    : QMainWindow(parent),
      // Inicializar EyedropperTool pasándole la lambda para actualizar el color cuando seleccione un píxel
      eyedropperTool([this](const QColor &pickedColor) {
          currentColor = pickedColor;
          canvas->setPenColor(currentColor);
          updateColorButton();
      }) 
{
    // 1. Instanciar el Lienzo principal
    canvas = new Canvas(this);
    setCentralWidget(canvas);

    // Conectar la imagen completa del canvas a la pipeta/gotero
    eyedropperTool.setCanvasImage(canvas->getCompositeImagePointer());

    // 2. Establecer la herramienta inicial (Pincel)
    canvas->setTool(&brushTool);

    // 3. Construir la Interfaz de Usuario
    createToolBar();
    createLayersDock();
    setupShortcuts();

    // 4. Sincronizar el panel con las capas iniciales
    updateLayersList();

    resize(1100, 750);
    setWindowTitle("Programa de Dibujo");
}

void MainWindow::createToolBar() {
    QToolBar *toolbar = addToolBar("Herramientas");
    toolbar->setMovable(false);

    // --- ACCIONES DE HERRAMIENTAS ---
    QAction *brushAction = toolbar->addAction("Pincel (B)");
    connect(brushAction, &QAction::triggered, this, [this]() {
        canvas->setTool(&brushTool);
    });

    QAction *eraserAction = toolbar->addAction("Goma (E)");
    connect(eraserAction, &QAction::triggered, this, [this]() {
        canvas->setTool(&eraserTool);
    });

    QAction *eyedropperAction = toolbar->addAction("Gotero (I)");
    connect(eyedropperAction, &QAction::triggered, this, [this]() {
        canvas->setTool(&eyedropperTool);
    });

    toolbar->addSeparator();

    // --- SELECTOR DE COLOR ---
    toolbar->addWidget(new QLabel(" Color: ", this));
    colorButton = new QPushButton(this);
    colorButton->setFixedSize(28, 28);
    colorButton->setCursor(Qt::PointingHandCursor);
    updateColorButton();
    connect(colorButton, &QPushButton::clicked, this, &MainWindow::onSelectColorClicked);
    toolbar->addWidget(colorButton);

    toolbar->addSeparator();

    // --- CONTROL DE TAMAÑO ---
    toolbar->addWidget(new QLabel(" Tamaño: ", this));

    sizeSlider = new QSlider(Qt::Horizontal, this);
    sizeSlider->setRange(1, 100);
    sizeSlider->setValue(5);
    sizeSlider->setFixedWidth(110);

    sizeSpinBox = new QSpinBox(this);
    sizeSpinBox->setRange(1, 100);
    sizeSpinBox->setValue(5);

    // Sincronizar Slider y SpinBox entre sí
    connect(sizeSlider, &QSlider::valueChanged, sizeSpinBox, &QSpinBox::setValue);
    connect(sizeSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), sizeSlider, &QSlider::setValue);

    // Emitir tamaño al Canvas
    connect(sizeSlider, &QSlider::valueChanged, this, [this](int newSize) {
        canvas->setPenSize(newSize);
    });

    toolbar->addWidget(sizeSlider);
    toolbar->addWidget(sizeSpinBox);
}

void MainWindow::createLayersDock() {
    QDockWidget *dock = new QDockWidget("Capas", this);
    dock->setAllowedAreas(Qt::RightDockWidgetArea | Qt::LeftDockWidgetArea);

    // 🔒 SIN LA CRUZ DE CIERRE (Mantiene la solapa siempre activa)
    dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);

    QWidget *dockWidgetContents = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(dockWidgetContents);

    layersListWidget = new QListWidget(this);
    addLayerButton = new QPushButton("+ Nueva Capa", this);

    layout->addWidget(layersListWidget);
    layout->addWidget(addLayerButton);

    dock->setWidget(dockWidgetContents);
    addDockWidget(Qt::RightDockWidgetArea, dock);

    // Conexiones del panel de capas
    connect(addLayerButton, &QPushButton::clicked, this, &MainWindow::onAddLayerClicked);
    connect(layersListWidget, &QListWidget::currentRowChanged, this, &MainWindow::onLayerSelected);
}

void MainWindow::setupShortcuts() {
    // Tecla 'B': Pincel
    QShortcut *brushShortcut = new QShortcut(QKeySequence(Qt::Key_B), this);
    connect(brushShortcut, &QShortcut::activated, this, [this]() {
        canvas->setTool(&brushTool);
    });

    // Tecla 'E': Goma
    QShortcut *eraserShortcut = new QShortcut(QKeySequence(Qt::Key_E), this);
    connect(eraserShortcut, &QShortcut::activated, this, [this]() {
        canvas->setTool(&eraserTool);
    });

    // Tecla 'I': Pipeta / Gotero
    QShortcut *eyedropperShortcut = new QShortcut(QKeySequence(Qt::Key_I), this);
    connect(eyedropperShortcut, &QShortcut::activated, this, [this]() {
        canvas->setTool(&eyedropperTool);
    });

    // Disminuir tamaño de pincel con '['
    QShortcut *decreaseSize = new QShortcut(QKeySequence(Qt::Key_BracketLeft), this);
    connect(decreaseSize, &QShortcut::activated, this, [this]() {
        int current = sizeSlider->value();
        if (current > 1) sizeSlider->setValue(current - 1);
    });

    // Aumentar tamaño de pincel con ']'
    QShortcut *increaseSize = new QShortcut(QKeySequence(Qt::Key_BracketRight), this);
    connect(increaseSize, &QShortcut::activated, this, [this]() {
        int current = sizeSlider->value();
        if (current < 100) sizeSlider->setValue(current + 1);
    });

    // Ctrl+Z: Deshacer
    QShortcut *undoShortcut = new QShortcut(QKeySequence(Qt::ControlModifier | Qt::Key_Z), this);
    connect(undoShortcut, &QShortcut::activated, canvas, &Canvas::undo);

    // Ctrl+Y o Ctrl+Shift+Z: Rehacer
    QShortcut *redoShortcut = new QShortcut(QKeySequence(Qt::ControlModifier | Qt::ShiftModifier | Qt::Key_Z), this);
    connect(redoShortcut, &QShortcut::activated, canvas, &Canvas::redo);
}

void MainWindow::onAddLayerClicked() {
    int count = layersListWidget->count() + 1;
    canvas->addLayer(QString("Capa %1").arg(count));
    updateLayersList();
}

void MainWindow::onLayerSelected(int index) {
    if (index >= 0) {
        // En la UI mostramos la capa superior arriba de todo, invertimos el índice para el Canvas
        int actualIndex = layersListWidget->count() - 1 - index;
        canvas->setActiveLayer(actualIndex);
    }
}

void MainWindow::onSelectColorClicked() {
    QColor newColor = QColorDialog::getColor(currentColor, this, "Seleccionar Color");
    if (newColor.isValid()) {
        currentColor = newColor;
        canvas->setPenColor(currentColor);
        updateColorButton();
    }
}

void MainWindow::updateColorButton() {
    QString style = QString("background-color: %1; border: 1px solid #666; border-radius: 4px;")
                        .arg(currentColor.name());
    colorButton->setStyleSheet(style);
}

void MainWindow::updateLayersList() {
    layersListWidget->clear();
    
    // Rellenar en orden inverso para que la capa más alta aparezca arriba en la lista
    for (int i = canvas->getLayersCount() - 1; i >= 0; --i) {
        layersListWidget->addItem(canvas->getLayerName(i));
    }

    if (layersListWidget->count() > 0) {
        layersListWidget->setCurrentRow(0);
    }
}