#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QApplication::setOrganizationName("Perspective");
    QApplication::setApplicationName("Schematic");

    MainWindow w;
    w.show();
    return QCoreApplication::exec();
}
