#include "MainWindow.h"

#include <QApplication>
#include <QIcon>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    QApplication::setApplicationName("trad-auto-ds4-controller");
    QApplication::setApplicationDisplayName("Trad Auto-DS4 Controller");
    QApplication::setOrganizationName("TRAD");
    QApplication::setOrganizationDomain("github.com/KidDzhasi");
    QApplication::setApplicationVersion("1.0.0");

    // Ensures Wayland / GNOME Shell matches the window with packaging/trad-auto-ds4-controller.desktop
    QGuiApplication::setDesktopFileName("trad-auto-ds4-controller");
    app.setWindowIcon(QIcon(":/assets/trad-auto-ds4-controller.svg"));

    MainWindow window;
    window.show();

    return app.exec();
}
