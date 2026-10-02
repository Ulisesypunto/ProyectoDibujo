#pragma once
#include <QWidget>

class QLabel;
class QListWidget;
class RecentFiles;

// Pantalla de inicio: acciones rapidas + cuadricula de dibujos recientes.
// Solo emite senales; MainWindow decide que hacer.
class LobbyWidget : public QWidget {
    Q_OBJECT
public:
    explicit LobbyWidget(RecentFiles* recent, QWidget* parent = nullptr);

signals:
    void newRequested();
    void openRequested();
    void importRequested();
    void openFileRequested(const QString& path);

private:
    void refresh();

    RecentFiles* m_recent;
    QListWidget* m_list;
    QLabel* m_empty;
};
