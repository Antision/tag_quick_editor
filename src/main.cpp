#include "func.h"
#include "mainwindow.h"
#include"pch.h"
#include <QApplication>
#include"tesignalwidget.h"
#include"task_runner.h"

#include <cstdlib>

using namespace std;
BS::thread_pool<> thread_pool;

int main(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QApplication a(argc, argv);
    // Long-lived workers (the autocomplete refresh loop) watch this flag: once the
    // event loop is over they have to return, because the widgets they read are
    // about to be destroyed. Set from aboutToQuit so that every exit path gets it,
    // including a close the window manager sends behind our back.
    QObject::connect(&a, &QCoreApplication::aboutToQuit, [] { g_appShuttingDown = true; });
    // 全局字体:内置 Ubuntu,Light 作主字重(许可文本随字体编进 exe,见 :/UFL.txt)
    {
        QFont f(appFontFamily());
        if (!f.family().isEmpty()) {
            f.setWeight(QFont::Light);
            a.setFont(f);
        }
    }
    QLoggingCategory::setFilterRules(QStringLiteral("qt.gui.imageio=false"));
    load_config();
    // Heap, not stack: the window has to be destroyed *before* the pool is drained
    // (its destructor is what cancels the queued thumbnail decodes), but only once
    // no worker is touching it any more. See the shutdown block below.
    MainWindow* w = new MainWindow;
    w->show();
    w->loadState();
    w->checkForUpdate();
    const int exitCode = a.exec();

    // ---- shutdown ----
    // Nothing from here on may keep the process alive. A closed window with a
    // process that keeps running is exactly the bug this block exists for.
    g_appShuttingDown = true;

    // The autocomplete workers only look at their flag once per refresh slice, so
    // give them that slice. They are the only tasks that never end on their own.
    teWaitForTask("autocomplete", std::chrono::milliseconds(600));
    if (teOutstandingTaskCount("autocomplete") > 0) {
        // One is still inside a parse: its editor must not be deleted while it
        // reads it. Everything is on disk already, so leave without destroying
        // the widgets instead of racing them.
        teLogOutstandingTasks();
        save_config();
        std::_Exit(exitCode);
    }

    // Ask the queued thumbnail decodes to return, then let the pool drain *before*
    // any widget is destroyed. Draining first is what the old shutdown did and it
    // is the only order in which no worker can be touching a widget during its
    // destructor; the difference now is that the wait has a deadline.
    w->cancelPendingWork();
    const bool poolIdle = thread_pool.wait_for(std::chrono::seconds(2));

    // Must happen while the window exists: it reads global_window for the splitter
    // sizes and the geometry (see save_config).
    save_config();

    if (!poolIdle) {
        // Something is blocked in a syscall we cannot cancel (a network reply that
        // never came back). Everything the user could lose is written, so end the
        // process here rather than deleting widgets a live task may still read.
        teLogOutstandingTasks();
        std::_Exit(exitCode);
    }

    // Pool is empty: destroying the window can no longer race any worker, and its
    // own destructor saves the state that only exists in the widgets (editors,
    // layout) as the last thing before the event loop object goes away.
    delete w;
    return exitCode;
}
