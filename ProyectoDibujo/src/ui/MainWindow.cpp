#include "ui/MainWindow.h"

#include "core/Document.h"
#include "tools/BuiltinTools.h"
#include "tools/ToolManager.h"
#include "ui/CanvasView.h"
#include "ui/ColorPickerPanel.h"
#include "ui/LayersPanel.h"
#include "ui/ToolOptionsWidget.h"
#include "ui/ToolsPanel.h"

#include <QAction>
#include <QCloseEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QSettings>
#include <QSpinBox>
#include <QStatusBar>
#include <QUndoStack>
#include <algorithm>

namespace {
constexpr int MaxDocumentSide = 8192;
constexpr int LayoutVersion = 1;

bool askDocumentSize(QWidget* parent, QSize& size) {
    QDialog dialog(parent);
    dialog.setWindowTitle(QObject::tr("Nuevo documento"));
    auto* form = new QFormLayout(&dialog);

    auto* width = new QSpinBox(&dialog);
    width->setRange(1, MaxDocumentSide);
    width->setValue(size.width());
    auto* height = new QSpinBox(&dialog);
    height->setRange(1, MaxDocumentSide);
    height->setValue(size.height());
    form->addRow(QObject::tr("Ancho (px):"), width);
    form->addRow(QObject::tr("Alto (px):"), height);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    form->addRow(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) return false;
    size = QSize(width->value(), height->value());
    return true;
}
} // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    m_tools = new ToolManager(this);
    registerBuiltinTools(*m_tools);

    m_canvas = new CanvasView(m_tools, this);
    setCentralWidget(m_canvas);
    setDockOptions(AllowTabbedDocks | AllowNestedDocks | AnimatedDocks);

    createActions();
    createDocks();   // antes que los menus: el menu Ver usa los docks
    createMenus();

    auto* zoomLabel = new QLabel(this);
    statusBar()->addPermanentWidget(zoomLabel);
    connect(m_canvas, &CanvasView::zoomChanged, this, [zoomLabel](qreal z) {
        zoomLabel->setText(QStringLiteral("%1%  ").arg(qRound(z * 100)));
    });

    setDocument(std::make_unique<Document>(QSize(1280, 720)));
    resize(1200, 800);

    m_defaultState = saveState(LayoutVersion); // disposicion de fabrica
    QSettings settings;
    restoreGeometry(settings.value("window/geometry").toByteArray());
    restoreState(settings.value("window/state").toByteArray(), LayoutVersion);
}

MainWindow::~MainWindow() = default;

// ---------------- acciones / menus ----------------
void MainWindow::createActions() {
    m_newAction = new QAction(tr("&Nuevo..."), this);
    m_newAction->setShortcut(QKeySequence::New);
    connect(m_newAction, &QAction::triggered, this, &MainWindow::newDocument);

    m_openAction = new QAction(tr("&Abrir imagen..."), this);
    m_openAction->setShortcut(QKeySequence::Open);
    connect(m_openAction, &QAction::triggered, this, &MainWindow::openImage);

    m_saveAction = new QAction(tr("&Guardar imagen..."), this);
    m_saveAction->setShortcut(QKeySequence::Save);
    connect(m_saveAction, &QAction::triggered, this, [this] { saveImage(); });

    m_quitAction = new QAction(tr("&Salir"), this);
    m_quitAction->setShortcut(QKeySequence::Quit);
    connect(m_quitAction, &QAction::triggered, this, &QWidget::close);

    m_undoAction = new QAction(tr("&Deshacer"), this);
    m_undoAction->setShortcut(QKeySequence::Undo);
    connect(m_undoAction, &QAction::triggered, this, [this] { if (m_document) m_document->undoStack()->undo(); });

    m_redoAction = new QAction(tr("&Rehacer"), this);
    m_redoAction->setShortcuts({QKeySequence::Redo,
                                QKeySequence(Qt::ControlModifier | Qt::ShiftModifier | Qt::Key_Z)});
    connect(m_redoAction, &QAction::triggered, this, [this] { if (m_document) m_document->undoStack()->redo(); });

    m_zoomInAction = new QAction(tr("Acercar"), this);
    m_zoomInAction->setShortcut(QKeySequence::ZoomIn);
    connect(m_zoomInAction, &QAction::triggered, m_canvas, &CanvasView::zoomIn);

    m_zoomOutAction = new QAction(tr("Alejar"), this);
    m_zoomOutAction->setShortcut(QKeySequence::ZoomOut);
    connect(m_zoomOutAction, &QAction::triggered, m_canvas, &CanvasView::zoomOut);

    m_zoomFitAction = new QAction(tr("Ajustar a la ventana"), this);
    m_zoomFitAction->setShortcut(QKeySequence(Qt::ControlModifier | Qt::Key_0));
    connect(m_zoomFitAction, &QAction::triggered, m_canvas, &CanvasView::zoomToFit);

    m_zoomActualAction = new QAction(tr("Tamaño real (100%)"), this);
    m_zoomActualAction->setShortcut(QKeySequence(Qt::ControlModifier | Qt::Key_1));
    connect(m_zoomActualAction, &QAction::triggered, m_canvas, &CanvasView::zoomActualSize);

    // Atajos '[' y ']' para el tamaño (sin menu, solo teclado)
    m_sizeDownAction = new QAction(this);
    m_sizeDownAction->setShortcut(QKeySequence(Qt::Key_BracketLeft));
    connect(m_sizeDownAction, &QAction::triggered, this, [this] {
        m_tools->setSize(m_tools->size() - std::max(1, m_tools->size() / 10));
    });
    m_sizeUpAction = new QAction(this);
    m_sizeUpAction->setShortcut(QKeySequence(Qt::Key_BracketRight));
    connect(m_sizeUpAction, &QAction::triggered, this, [this] {
        m_tools->setSize(m_tools->size() + std::max(1, m_tools->size() / 10));
    });
    addAction(m_sizeDownAction);
    addAction(m_sizeUpAction);

    m_resetLayoutAction = new QAction(tr("Restaurar disposición"), this);
    connect(m_resetLayoutAction, &QAction::triggered, this, &MainWindow::resetLayout);

    // Atajos globales: con paneles flotantes la ventana activa puede no ser esta.
    const auto actions = findChildren<QAction*>(QString(), Qt::FindDirectChildrenOnly);
    for (QAction* a : actions) a->setShortcutContext(Qt::ApplicationShortcut);
}

void MainWindow::createMenus() {
    QMenu* file = menuBar()->addMenu(tr("&Archivo"));
    file->addAction(m_newAction);
    file->addAction(m_openAction);
    file->addAction(m_saveAction);
    file->addSeparator();
    file->addAction(m_quitAction);

    QMenu* edit = menuBar()->addMenu(tr("&Editar"));
    edit->addAction(m_undoAction);
    edit->addAction(m_redoAction);

    QMenu* view = menuBar()->addMenu(tr("&Ver"));
    view->addAction(m_zoomInAction);
    view->addAction(m_zoomOutAction);
    view->addAction(m_zoomFitAction);
    view->addAction(m_zoomActualAction);
    view->addSeparator();
    QMenu* panels = view->addMenu(tr("Paneles"));
    for (QDockWidget* dock : m_docks) panels->addAction(dock->toggleViewAction());
    panels->addSeparator();
    panels->addAction(m_resetLayoutAction);
}

void MainWindow::createDocks() {
    auto addDock = [this](const QString& title, const QString& name, QWidget* content,
                          Qt::DockWidgetArea area) {
        auto* dock = new QDockWidget(title, this);
        dock->setObjectName(name); // necesario para saveState/restoreState
        dock->setAllowedAreas(Qt::AllDockWidgetAreas);
        dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable |
                          QDockWidget::DockWidgetClosable);
        dock->setWidget(content);
        addDockWidget(area, dock);
        m_docks.push_back(dock);
    };

    m_toolsPanel = new ToolsPanel(m_tools, this);
    // Las acciones viven en la ventana principal para que el atajo siga activo
    // aunque el panel este cerrado.
    for (QAction* a : m_toolsPanel->toolActions()) addAction(a);

    m_layersPanel = new LayersPanel(this);

    addDock(tr("Herramientas"), "dockTools", m_toolsPanel, Qt::LeftDockWidgetArea);
    addDock(tr("Opciones"), "dockOptions", new ToolOptionsWidget(m_tools, this), Qt::TopDockWidgetArea);
    addDock(tr("Color"), "dockColor", new ColorPickerPanel(m_tools, this), Qt::RightDockWidgetArea);
    addDock(tr("Capas"), "dockLayers", m_layersPanel, Qt::RightDockWidgetArea);
}

void MainWindow::resetLayout() {
    restoreState(m_defaultState, LayoutVersion);
    for (QDockWidget* dock : m_docks) dock->show();
}

// ---------------- documento ----------------
void MainWindow::setDocument(std::unique_ptr<Document> document) {
    std::unique_ptr<Document> previous = std::move(m_document); // se destruye al salir
    m_document = std::move(document);
    m_currentFile.clear();

    m_canvas->setDocument(m_document.get());
    m_layersPanel->setDocument(m_document.get());

    QUndoStack* stack = m_document->undoStack();
    auto refreshUndoActions = [this, stack] {
        m_undoAction->setEnabled(stack->canUndo());
        m_redoAction->setEnabled(stack->canRedo());
        m_undoAction->setText(stack->canUndo() ? tr("&Deshacer %1").arg(stack->undoText()) : tr("&Deshacer"));
        m_redoAction->setText(stack->canRedo() ? tr("&Rehacer %1").arg(stack->redoText()) : tr("&Rehacer"));
    };
    connect(stack, &QUndoStack::indexChanged, this, refreshUndoActions);
    connect(stack, &QUndoStack::cleanChanged, this, [this] { updateWindowTitle(); });
    refreshUndoActions();
    updateWindowTitle();
}

void MainWindow::updateWindowTitle() {
    const QString name = m_currentFile.isEmpty() ? tr("Sin título") : QFileInfo(m_currentFile).fileName();
    setWindowTitle(tr("%1[*] - Programa de Dibujo").arg(name));
    setWindowModified(m_document && !m_document->undoStack()->isClean());
}

bool MainWindow::maybeSave() {
    if (!m_document || m_document->undoStack()->isClean()) return true;
    const auto answer = QMessageBox::warning(this, tr("Cambios sin guardar"),
        tr("El dibujo tiene cambios sin guardar. ¿Quieres guardarlos?"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    if (answer == QMessageBox::Save) return saveImage();
    return answer == QMessageBox::Discard;
}

void MainWindow::newDocument() {
    if (!maybeSave()) return;
    QSize size = m_document ? m_document->size() : QSize(1280, 720);
    if (!askDocumentSize(this, size)) return;
    setDocument(std::make_unique<Document>(size));
}

void MainWindow::openImage() {
    if (!maybeSave()) return;
    const QString path = QFileDialog::getOpenFileName(this, tr("Abrir imagen"), QString(),
        tr("Imágenes (*.png *.jpg *.jpeg *.bmp)"));
    if (path.isEmpty()) return;

    QImage image;
    if (!image.load(path) || image.isNull()) {
        QMessageBox::warning(this, tr("Error"), tr("No se pudo abrir la imagen."));
        return;
    }
    setDocument(Document::fromImage(image));
    m_currentFile = path;
    updateWindowTitle();
}

bool MainWindow::saveImage() {
    if (!m_document) return false;
    const QString path = QFileDialog::getSaveFileName(this, tr("Guardar imagen"),
        m_currentFile.isEmpty() ? QStringLiteral("dibujo.png") : m_currentFile,
        tr("PNG (*.png);;JPEG (*.jpg *.jpeg);;BMP (*.bmp)"));
    if (path.isEmpty()) return false;

    if (!m_document->flatten().save(path)) {
        QMessageBox::warning(this, tr("Error"), tr("No se pudo guardar la imagen."));
        return false;
    }
    m_currentFile = path;
    m_document->undoStack()->setClean();
    updateWindowTitle();
    return true;
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (!maybeSave()) { event->ignore(); return; }
    QSettings settings;
    settings.setValue("window/geometry", saveGeometry());
    settings.setValue("window/state", saveState(LayoutVersion));
    event->accept();
}