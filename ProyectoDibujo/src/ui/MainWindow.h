#pragma once
#include <QByteArray>
#include <QImage>
#include <QMainWindow>
#include <QPoint>
#include <memory>
#include <vector>

class Document;
class ToolManager;
class CanvasView;
class LayersPanel;
class ToolsPanel;
class LobbyWidget;
class RecentFiles;
class ShortcutManager;
class QAction;
class QDockWidget;
class QStackedWidget;
class QDragEnterEvent;
class QDropEvent;

// Ensambla la aplicacion: crea las piezas y las conecta. No contiene logica
// de dibujo ni de capas (esa vive en core/ y tools/).
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    enum class Placement { DocumentCenter, ViewCenter, At };

    void createActions();
    void createMenus();
    void createDocks();
    void registerShortcuts();
    void resetLayout();
    void showKeybinds();

    // documento / archivos
    void setDocument(std::unique_ptr<Document> document); // nullptr = volver al lobby
    bool maybeSave();
    void newDocument();
    void openFile();                        // dialogo
    void requestOpen(const QString& path);  // pregunta por cambios y abre
    bool openPath(const QString& path);     // abre sin preguntar
    bool saveDocument();
    bool saveDocumentAs();
    bool saveToPath(const QString& path);
    void exportImage();
    void closeDocument();
    void importImages();
    void importPaths(const QStringList& paths);
    bool placeImage(QImage image, const QString& name, const QString& undoText,
                    Placement where, const QPoint& at = QPoint());
    void copyLayer();
    void pasteImage();

    void refreshUndoActions();
    void updateDocumentActions();
    void updateWindowTitle();

    ToolManager* m_tools = nullptr;
    CanvasView* m_canvas = nullptr;
    LobbyWidget* m_lobby = nullptr;
    QStackedWidget* m_stack = nullptr;
    LayersPanel* m_layersPanel = nullptr;
    ToolsPanel* m_toolsPanel = nullptr;
    RecentFiles* m_recent = nullptr;
    ShortcutManager* m_shortcuts = nullptr;
    std::unique_ptr<Document> m_document;
    std::vector<QDockWidget*> m_docks;
    QByteArray m_defaultState;

    QString m_currentFile; // solo proyectos (.kra/.krz)
    QString m_docName;     // nombre mostrado si el documento viene de una imagen

    // ultimo Ctrl+C: permite "pegar en el mismo sitio" dentro de la app
    QImage m_copiedImage;
    QPoint m_copiedPos;

    QAction* m_newAction = nullptr;
    QAction* m_openAction = nullptr;
    QAction* m_saveAction = nullptr;
    QAction* m_saveAsAction = nullptr;
    QAction* m_exportAction = nullptr;
    QAction* m_importAction = nullptr;
    QAction* m_closeAction = nullptr;
    QAction* m_quitAction = nullptr;
    QAction* m_undoAction = nullptr;
    QAction* m_redoAction = nullptr;
    QAction* m_copyAction = nullptr;
    QAction* m_pasteAction = nullptr;
    QAction* m_zoomInAction = nullptr;
    QAction* m_zoomOutAction = nullptr;
    QAction* m_zoomFitAction = nullptr;
    QAction* m_zoomActualAction = nullptr;
    QAction* m_sizeDownAction = nullptr;
    QAction* m_sizeUpAction = nullptr;
    QAction* m_resetLayoutAction = nullptr;
    QAction* m_keybindsAction = nullptr;
    QAction* m_selectAllAction = nullptr;
    QAction* m_deselectAction = nullptr;
    QAction* m_invertSelectionAction = nullptr;
    QAction* m_mirrorAction = nullptr;
};
