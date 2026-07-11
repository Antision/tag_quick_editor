#include "func.h"
#include "mainwindow.h"
#include"pch.h"
#include <QApplication>
#include"tesignalwidget.h"

using namespace std;
BS::thread_pool<> thread_pool;

int main(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QApplication a(argc, argv);
    QLoggingCategory::setFilterRules(QStringLiteral("qt.gui.imageio=false"));
    load_config();
    MainWindow w;
    w.show();
    w.loadState();
    CreateAutoSaveThread(&w);
    w.checkForUpdate();
    a.exec();
    save_config();
}
