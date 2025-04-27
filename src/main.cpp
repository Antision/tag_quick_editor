#include "func.h"
#include "mainwindow.h"
#include"pch.h"
#include <QApplication>
#include"tesignalwidget.h"

using namespace std;
ants::ThreadPool thread_pool;

int main(int argc, char *argv[])
{
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
    thread_pool.terminate();
}
