#pragma once
#include <QByteArray>
#include <QMainWindow>
#include <memory>
#include <vector>

class Document;
class ToolManager;
class CanvasView;
class LayersPanel;
class ToolsPanel;
class QAction;
class QDockWidget;

// Ensambla la aplicacion: crea las piezas y las conecta. No contiene logica
// de dibujo ni de capas (esa vive en core/ y tools/).
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void createActions();
    void createMenus();
    void createDocks();
    void resetLayout();

    void setDocument(std::unique_ptr<Document> document);
    bool maybeSave();
    void newDocument();
    void openImage();
    bool saveImage();
    void updateWindowTitle();

    ToolManager* m_tools = nullptr;
    CanvasView* m_canvas = nullptr;
    LayersPanel* m_layersPanel = nullptr;
    ToolsPanel* m_toolsPanel = nullptr;
    std::unique_ptr<Document> m_document;
    std::vector<QDockWidget*> m_docks;
    QByteArray m_defaultState;

    QString m_currentFile;

    QAction* m_newAction = nullptr;
    QAction* m_openAction = nullptr;
    QAction* m_saveAction = nullptr;
    QAction* m_quitAction = nullptr;
    QAction* m_undoAction = nullptr;
    QAction* m_redoAction = nullptr;
    QAction* m_zoomInAction = nullptr;
    QAction* m_zoomOutAction = nullptr;
    QAction* m_zoomFitAction = nullptr;
    QAction* m_zoomActualAction = nullptr;
    QAction* m_sizeDownAction = nullptr;
    QAction* m_sizeUpAction = nullptr;
    QAction* m_resetLayoutAction = nullptr;
};