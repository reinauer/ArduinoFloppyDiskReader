#include "MainWindow.h"

#include <QApplication>
#include <QIcon>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("RobSmithDev");
    QCoreApplication::setOrganizationDomain("robsmithdev.co.uk");
    QCoreApplication::setApplicationName("DrawBridge");
    QCoreApplication::setApplicationVersion("2.8.8");
    app.setWindowIcon(QIcon(":/drawbridge/app.ico"));

    MainWindow window;
    window.show();
    return app.exec();
}
