#include "HttpServer.h"

#include <QApplication>


int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    HttpServer w;
    w.show();
    return a.exec();
}
