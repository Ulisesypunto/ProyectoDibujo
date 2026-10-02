#include "ui/MainWindow.h"

#include "core/Document.h"
#include "core/RecentFiles.h"
#include "core/io/KraFormat.h"
#include "tools/BuiltinTools.h"
#include "tools/ToolManager.h"
#include "ui/CanvasView.h"
#include "ui/ColorPickerPanel.h"
#include "ui/KeybindsDialog.h"
#include "ui/LayersPanel.h"
#include "ui/LobbyWidget.h"
#include "ui/ShortcutManager.h"
#include "ui/ToolOptionsWidget.h"
#include "ui/ToolsPanel.h"

#include <QAction>
#include <QClipboard>
#include <QCloseEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGuiApplication>
#include <QImageReader>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStatusBar>
#include <QUndoStack>
#include <QUrl>
#include <algorithm>

namespace {
constexpr int MaxDocumentSide = KraFormat::MaxSide;
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

// "*.png *.jpg ..." con todo lo que esta build de Qt sabe leer
QString imageGlobs() {
    QStringList globs;
    for (const QByteArray& f : QImageReader::supportedImageFormats())
        globs << QStringLiteral("*.") + QString::fromLatin1(f);
    return globs.join(QLatin1Char(' '));
}

bool isReadableImage(const QString& path) {
    return QImageReader::supportedImageFormats().contains(QFileInfo(path).suffix().toLower().toLatin1());
}

QImage readImage(const QString& path, QString* error) {
    QImageReader reader(path);
    reader.setAutoTransform(true); // respeta la orientacion EXIF
    QImage image = reader.read();
    if (image.isNull() && error) *error = reader.errorString();
    return image;
}
} // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setAcceptDrops(true);

    m_tools = new ToolManager(this);
    registerBuiltinTools(*m_tools);
    m_shortcuts = new ShortcutManager(this);
    m_recent = new RecentFiles(this);

    m_canvas = new CanvasView(m_tools, this);
    m_lobby = new LobbyWidget(m_recent, this);
    m_stack = new QStackedWidget(this);
    m_stack->addWidget(m_lobby);
    m_stack->addWidget(m_canvas);
    setCentralWidget(m_stack);
    setDockOptions(AllowTabbedDocks | AllowNestedDocks | AnimatedDocks);

    createActions();
    createDocks();       // antes que los menus: el menu Ver usa los docks
    registerShortcuts(); // despues de los docks: incluye las acciones de herramientas
    createMenus();

    auto* zoomLabel = new QLabel(this);
    statusBar()->addPermanentWidget(zoomLabel);
    connect(m_canvas, &CanvasView::zoomChanged, this, [zoomLabel](qreal z) {
        zoomLabel->setText(QStringLiteral("%1%  ").arg(qRound(z * 100)));
    });

    connect(m_lobby, &LobbyWidget::newRequested, this, &MainWindow::newDocument);
    connect(m_lobby, &LobbyWidget::openRequested, this, &MainWindow::openFile);
    connect(m_lobby, &LobbyWidget::importRequested, this, &MainWindow::importImages);
    connect(m_lobby, &LobbyWidget::openFileRequested, this, &MainWindow::requestOpen);
    connect(QGuiApplication::clipboard(), &QClipboard::dataChanged, this, &MainWindow::updateDocumentActions);

    setDocument(nullptr); // arranca en el lobby
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

    m_openAction = new QAction(tr("&Abrir..."), this);
    m_openAction->setShortcut(QKeySequence::Open);
    connect(m_openAction, &QAction::triggered, this, &MainWindow::openFile);

    m_saveAction = new QAction(tr("&Guardar"), this);
    m_saveAction->setShortcut(QKeySequence::Save);
    connect(m_saveAction, &QAction::triggered, this, [this] { saveDocument(); });

    m_saveAsAction = new QAction(tr("Guardar &como..."), this);
    m_saveAsAction->setShortcut(QKeySequence::SaveAs);
    connect(m_saveAsAction, &QAction::triggered, this, [this] { saveDocumentAs(); });

    m_exportAction = new QAction(tr("&Exportar imagen..."), this);
    m_exportAction->setShortcut(QKeySequence(Qt::ControlModifier | Qt::ShiftModifier | Qt::Key_E));
    connect(m_exportAction, &QAction::triggered, this, &MainWindow::exportImage);

    m_importAction = new QAction(tr("&Importar imagen como capa..."), this);
    m_importAction->setShortcut(QKeySequence(Qt::ControlModifier | Qt::ShiftModifier | Qt::Key_I));
    connect(m_importAction, &QAction::triggered, this, &MainWindow::importImages);

    m_closeAction = new QAction(tr("&Cerrar documento"), this);
    m_closeAction->setShortcut(QKeySequence::Close);
    connect(m_closeAction, &QAction::triggered, this, &MainWindow::closeDocument);

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

    m_copyAction = new QAction(tr("&Copiar capa"), this);
    m_copyAction->setShortcut(QKeySequence::Copy);
    connect(m_copyAction, &QAction::triggered, this, &MainWindow::copyLayer);

    m_pasteAction = new QAction(tr("&Pegar como capa nueva"), this);
    m_pasteAction->setShortcut(QKeySequence::Paste);
    connect(m_pasteAction, &QAction::triggered, this, &MainWindow::pasteImage);

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

    m_keybindsAction = new QAction(tr("&Atajos de teclado..."), this);
    connect(m_keybindsAction, &QAction::triggered, this, &MainWindow::showKeybinds);

    // Atajos globales: con paneles flotantes la ventana activa puede no ser esta.
    const auto actions = findChildren<QAction*>(QString(), Qt::FindDirectChildrenOnly);
    for (QAction* a : actions) a->setShortcutContext(Qt::ApplicationShortcut);
}

void MainWindow::registerShortcuts() {
    auto reg = [this](const char* id, const QString& label, QAction* action) {
        m_shortcuts->registerAction(QString::fromLatin1(id), label, action);
    };
    reg("file.new", tr("Nuevo documento"), m_newAction);
    reg("file.open", tr("Abrir"), m_openAction);
    reg("file.save", tr("Guardar"), m_saveAction);
    reg("file.saveAs", tr("Guardar como"), m_saveAsAction);
    reg("file.export", tr("Exportar imagen"), m_exportAction);
    reg("file.import", tr("Importar imagen como capa"), m_importAction);
    reg("file.close", tr("Cerrar documento"), m_closeAction);
    reg("file.quit", tr("Salir"), m_quitAction);
    reg("edit.undo", tr("Deshacer"), m_undoAction);
    reg("edit.redo", tr("Rehacer"), m_redoAction);
    reg("edit.copy", tr("Copiar capa"), m_copyAction);
    reg("edit.paste", tr("Pegar como capa nueva"), m_pasteAction);
    reg("view.zoomIn", tr("Acercar"), m_zoomInAction);
    reg("view.zoomOut", tr("Alejar"), m_zoomOutAction);
    reg("view.zoomFit", tr("Ajustar a la ventana"), m_zoomFitAction);
    reg("view.zoomActual", tr("Tamaño real (100%)"), m_zoomActualAction);
    reg("tool.sizeDown", tr("Reducir tamaño del pincel"), m_sizeDownAction);
    reg("tool.sizeUp", tr("Aumentar tamaño del pincel"), m_sizeUpAction);

    for (QAction* a : m_toolsPanel->toolActions()) {
        const QString toolId = a->data().toString();
        const Tool* tool = m_tools->tool(toolId);
        m_shortcuts->registerAction(QStringLiteral("tool.") + toolId,
                                    tr("Herramienta: %1").arg(tool ? tool->displayName() : toolId), a);
    }
    m_shortcuts->loadFromSettings(); // aplica lo que el usuario personalizo
}

void MainWindow::showKeybinds() {
    KeybindsDialog dialog(m_shortcuts, this);
    dialog.exec();
}

void MainWindow::createMenus() {
    QMenu* file = menuBar()->addMenu(tr("&Archivo"));
    file->addAction(m_newAction);
    file->addAction(m_openAction);
    QMenu* recent = file->addMenu(tr("Abrir &reciente"));
    connect(recent, &QMenu::aboutToShow, this, [this, recent] {
        recent->clear();
        for (const QString& path : m_recent->files())
            recent->addAction(QFileInfo(path).fileName(), this, [this, path] { requestOpen(path); });
        recent->setEnabled(!recent->isEmpty());
    });
    file->addSeparator();
    file->addAction(m_saveAction);
    file->addAction(m_saveAsAction);
    file->addAction(m_exportAction);
    file->addSeparator();
    file->addAction(m_importAction);
    file->addSeparator();
    file->addAction(m_closeAction);
    file->addAction(m_quitAction);

    QMenu* edit = menuBar()->addMenu(tr("&Editar"));
    edit->addAction(m_undoAction);
    edit->addAction(m_redoAction);
    edit->addSeparator();
    edit->addAction(m_copyAction);
    edit->addAction(m_pasteAction);

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

    QMenu* settings = menuBar()->addMenu(tr("A&justes"));
    settings->addAction(m_keybindsAction);
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
    m_docName.clear();
    m_copiedImage = QImage();

    m_canvas->setDocument(m_document.get());
    m_layersPanel->setDocument(m_document.get());
    m_stack->setCurrentWidget(m_document ? static_cast<QWidget*>(m_canvas) : static_cast<QWidget*>(m_lobby));

    if (m_document) {
        QUndoStack* stack = m_document->undoStack();
        connect(stack, &QUndoStack::indexChanged, this, &MainWindow::refreshUndoActions);
        connect(stack, &QUndoStack::cleanChanged, this, &MainWindow::updateWindowTitle);
    }
    refreshUndoActions();
    updateDocumentActions();
    updateWindowTitle();
}

void MainWindow::refreshUndoActions() {
    QUndoStack* stack = m_document ? m_document->undoStack() : nullptr;
    const bool canUndo = stack && stack->canUndo();
    const bool canRedo = stack && stack->canRedo();
    m_undoAction->setEnabled(canUndo);
    m_redoAction->setEnabled(canRedo);
    m_undoAction->setText(canUndo ? tr("&Deshacer %1").arg(stack->undoText()) : tr("&Deshacer"));
    m_redoAction->setText(canRedo ? tr("&Rehacer %1").arg(stack->redoText()) : tr("&Rehacer"));
}

void MainWindow::updateDocumentActions() {
    const bool has = m_document != nullptr;
    for (QAction* a : {m_saveAction, m_saveAsAction, m_exportAction, m_closeAction, m_copyAction})
        a->setEnabled(has);
    const QMimeData* mime = QGuiApplication::clipboard()->mimeData();
    m_pasteAction->setEnabled(has && mime && mime->hasImage());
}

void MainWindow::updateWindowTitle() {
    if (!m_document) {
        setWindowTitle(tr("Programa de Dibujo"));
        setWindowModified(false);
        return;
    }
    QString name = tr("Sin título");
    if (!m_currentFile.isEmpty()) name = QFileInfo(m_currentFile).fileName();
    else if (!m_docName.isEmpty()) name = m_docName;
    setWindowTitle(tr("%1[*] - Programa de Dibujo").arg(name));
    setWindowModified(!m_document->undoStack()->isClean());
}

bool MainWindow::maybeSave() {
    if (!m_document || m_document->undoStack()->isClean()) return true;
    const auto answer = QMessageBox::warning(this, tr("Cambios sin guardar"),
        tr("El dibujo tiene cambios sin guardar. ¿Quieres guardarlos?"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    if (answer == QMessageBox::Save) return saveDocument();
    return answer == QMessageBox::Discard;
}

void MainWindow::newDocument() {
    QSize size = m_document ? m_document->size() : QSize(1280, 720);
    if (!askDocumentSize(this, size)) return;
    if (!maybeSave()) return;
    setDocument(std::make_unique<Document>(size));
}

void MainWindow::closeDocument() {
    if (!m_document || !maybeSave()) return;
    setDocument(nullptr); // vuelve al lobby
}

// ---------------- abrir ----------------
void MainWindow::openFile() {
    const QString images = imageGlobs();
    const QString path = QFileDialog::getOpenFileName(this, tr("Abrir"), QString(),
        tr("Todos los soportados (*.kra *.krz %1);;Proyectos Krita (*.kra *.krz);;Imágenes (%1)").arg(images));
    if (path.isEmpty()) return;
    requestOpen(path);
}

void MainWindow::requestOpen(const QString& path) {
    if (!maybeSave()) return;
    openPath(path);
}

bool MainWindow::openPath(const QString& path) {
    if (!QFileInfo::exists(path)) {
        QMessageBox::warning(this, tr("Error"), tr("El archivo ya no existe:\n%1").arg(path));
        m_recent->remove(path);
        return false;
    }

    if (KraFormat::isProjectFile(path)) {
        KraFormat::LoadResult result = KraFormat::load(path);
        if (!result.document) {
            QMessageBox::warning(this, tr("Error"), tr("No se pudo abrir el proyecto:\n%1").arg(result.error));
            return false;
        }
        setDocument(std::move(result.document));
        m_currentFile = path;
        if (!result.warnings.isEmpty())
            QMessageBox::information(this, tr("Archivo abierto con avisos"), result.warnings.join(QLatin1Char('\n')));
    } else {
        QString error;
        const QImage image = readImage(path, &error);
        if (image.isNull()) {
            QMessageBox::warning(this, tr("Error"), tr("No se pudo abrir la imagen:\n%1").arg(error));
            return false;
        }
        QImage fitted = image;
        if (fitted.width() > MaxDocumentSide || fitted.height() > MaxDocumentSide)
            fitted = fitted.scaled(MaxDocumentSide, MaxDocumentSide, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        setDocument(Document::fromImage(fitted));
        m_docName = QFileInfo(path).completeBaseName();
    }
    m_recent->add(path, m_document->thumbnail(QSize(320, 220)));
    updateWindowTitle();
    return true;
}

// ---------------- guardar / exportar ----------------
bool MainWindow::saveDocument() {
    if (!m_document) return false;
    if (KraFormat::isProjectFile(m_currentFile)) return saveToPath(m_currentFile);
    return saveDocumentAs();
}

bool MainWindow::saveDocumentAs() {
    if (!m_document) return false;
    QString suggested = QStringLiteral("dibujo.kra");
    if (!m_currentFile.isEmpty()) suggested = m_currentFile;
    else if (!m_docName.isEmpty()) suggested = m_docName + QStringLiteral(".kra");

    QString selectedFilter;
    QString path = QFileDialog::getSaveFileName(this, tr("Guardar como"), suggested,
        tr("Proyecto Krita (*.kra);;Archivo Krita (*.krz)"), &selectedFilter);
    if (path.isEmpty()) return false;
    if (!KraFormat::isProjectFile(path))
        path += selectedFilter.contains(QStringLiteral("*.krz")) ? QStringLiteral(".krz") : QStringLiteral(".kra");
    return saveToPath(path);
}

bool MainWindow::saveToPath(const QString& path) {
    QString error;
    if (!KraFormat::save(*m_document, path, &error)) {
        QMessageBox::warning(this, tr("Error"), tr("No se pudo guardar:\n%1").arg(error));
        return false;
    }
    m_currentFile = path;
    m_docName.clear();
    m_document->undoStack()->setClean();
    m_recent->add(path, m_document->thumbnail(QSize(320, 220)));
    updateWindowTitle();
    statusBar()->showMessage(tr("Guardado: %1").arg(QFileInfo(path).fileName()), 3000);
    return true;
}

void MainWindow::exportImage() {
    if (!m_document) return;
    QString base = QStringLiteral("dibujo");
    if (!m_currentFile.isEmpty()) base = QFileInfo(m_currentFile).completeBaseName();
    else if (!m_docName.isEmpty()) base = m_docName;

    const QString path = QFileDialog::getSaveFileName(this, tr("Exportar imagen"),
        base + QStringLiteral(".png"), tr("PNG (*.png);;JPEG (*.jpg *.jpeg);;BMP (*.bmp)"));
    if (path.isEmpty()) return;
    if (!m_document->flatten().save(path)) {
        QMessageBox::warning(this, tr("Error"), tr("No se pudo exportar la imagen."));
        return;
    }
    statusBar()->showMessage(tr("Exportado: %1").arg(QFileInfo(path).fileName()), 3000);
}

// ---------------- importar / copiar / pegar ----------------
void MainWindow::importImages() {
    const QStringList paths = QFileDialog::getOpenFileNames(this, tr("Importar imágenes"), QString(),
        tr("Imágenes (%1)").arg(imageGlobs()));
    if (!paths.isEmpty()) importPaths(paths);
}

void MainWindow::importPaths(const QStringList& paths) {
    int imported = 0;
    for (const QString& path : paths) {
        QString error;
        QImage image = readImage(path, &error);
        if (image.isNull()) {
            QMessageBox::warning(this, tr("Error"),
                tr("No se pudo importar «%1»:\n%2").arg(QFileInfo(path).fileName(), error));
            continue;
        }
        const QString name = QFileInfo(path).completeBaseName();
        if (!m_document) { // sin documento: la primera imagen crea uno de su tamaño
            if (image.width() > MaxDocumentSide || image.height() > MaxDocumentSide)
                image = image.scaled(MaxDocumentSide, MaxDocumentSide, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            setDocument(Document::fromImage(image));
            m_docName = name;
            updateWindowTitle();
            ++imported;
        } else if (placeImage(image, name, tr("Importar imagen"), Placement::DocumentCenter)) {
            ++imported;
        }
    }
    if (imported > 0) statusBar()->showMessage(tr("%n imagen(es) importada(s)", nullptr, imported), 3000);
}

bool MainWindow::placeImage(QImage image, const QString& name, const QString& undoText,
                            Placement where, const QPoint& at) {
    if (!m_document || image.isNull()) return false;
    const QSize docSize = m_document->size();

    if (image.width() > docSize.width() || image.height() > docSize.height()) {
        QMessageBox box(QMessageBox::Question, tr("Imagen más grande que el lienzo"),
            tr("«%1» mide %2×%3 y el lienzo %4×%5.\n¿Qué quieres hacer?")
                .arg(name).arg(image.width()).arg(image.height()).arg(docSize.width()).arg(docSize.height()),
            QMessageBox::NoButton, this);
        QPushButton* fit = box.addButton(tr("Ajustar al lienzo"), QMessageBox::AcceptRole);
        QPushButton* crop = box.addButton(tr("Recortar"), QMessageBox::ActionRole);
        box.addButton(QMessageBox::Cancel);
        box.exec();
        if (box.clickedButton() == fit) {
            image = image.scaled(docSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            if (where == Placement::At) where = Placement::DocumentCenter;
        } else if (box.clickedButton() != crop) {
            return false;
        }
    }

    QPoint topLeft;
    switch (where) {
    case Placement::At:
        topLeft = at;
        break;
    case Placement::ViewCenter: {
        QPointF c = m_canvas->viewCenterInDocument();
        c.setX(qBound<qreal>(0.0, c.x(), docSize.width()));
        c.setY(qBound<qreal>(0.0, c.y(), docSize.height()));
        topLeft = QPoint(qRound(c.x() - image.width() / 2.0), qRound(c.y() - image.height() / 2.0));
        break;
    }
    case Placement::DocumentCenter:
        topLeft = QPoint((docSize.width() - image.width()) / 2, (docSize.height() - image.height()) / 2);
        break;
    }
    if (where != Placement::At) { // si cabe, que quede entera dentro del lienzo
        if (image.width() <= docSize.width())
            topLeft.setX(qBound(0, topLeft.x(), docSize.width() - image.width()));
        if (image.height() <= docSize.height())
            topLeft.setY(qBound(0, topLeft.y(), docSize.height() - image.height()));
    }

    m_document->addLayerWithImage(image, topLeft, name, undoText);
    return true;
}

void MainWindow::copyLayer() {
    if (!m_document) return;
    const Layer* layer = m_document->activeLayer();
    if (!layer) return;
    const QRect bounds = layer->contentBounds();
    if (bounds.isEmpty()) {
        statusBar()->showMessage(tr("La capa activa está vacía."), 3000);
        return;
    }
    // ARGB32 sin premultiplicar: es lo que esperan otras aplicaciones
    m_copiedImage = layer->image().copy(bounds).convertToFormat(QImage::Format_ARGB32);
    m_copiedPos = bounds.topLeft();
    QGuiApplication::clipboard()->setImage(m_copiedImage);
    statusBar()->showMessage(tr("Capa copiada (%1×%2 px).").arg(bounds.width()).arg(bounds.height()), 3000);
}

void MainWindow::pasteImage() {
    if (!m_document) return;
    QImage image = QGuiApplication::clipboard()->image();
    if (image.isNull()) {
        statusBar()->showMessage(tr("El portapapeles no contiene ninguna imagen."), 3000);
        return;
    }
    image = image.convertToFormat(QImage::Format_ARGB32);
    // Si es lo que acabamos de copiar aqui, se pega en su posicion original.
    if (!m_copiedImage.isNull() && image == m_copiedImage)
        placeImage(image, tr("Pegado"), tr("Pegar"), Placement::At, m_copiedPos);
    else
        placeImage(image, tr("Pegado"), tr("Pegar"), Placement::ViewCenter);
}

// ---------------- arrastrar y soltar ----------------
void MainWindow::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent* event) {
    QString project;
    QStringList images;
    for (const QUrl& url : event->mimeData()->urls()) {
        if (!url.isLocalFile()) continue;
        const QString path = url.toLocalFile();
        if (KraFormat::isProjectFile(path)) { if (project.isEmpty()) project = path; }
        else if (isReadableImage(path)) images << path;
    }
    if (!project.isEmpty()) requestOpen(project);
    else if (!images.isEmpty()) importPaths(images);
    event->acceptProposedAction();
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (!maybeSave()) { event->ignore(); return; }
    QSettings settings;
    settings.setValue("window/geometry", saveGeometry());
    settings.setValue("window/state", saveState(LayoutVersion));
    event->accept();
}
