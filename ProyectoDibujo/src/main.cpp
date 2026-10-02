#include <QApplication>
#include "ui/MainWindow.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("ProyectoDibujo");
    app.setOrganizationName("ProyectoDibujo");

    MainWindow window;
    window.show();
    return app.exec();
}
