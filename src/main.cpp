#include "func.h"
#include "mainwindow.h"
#include"pch.h"
#include "suggestionsource.h"
#include "dedupenv.h"   // common/dedup:去重窗口的提示音
#include "audio.h"
#include <QApplication>
#include"tesignalwidget.h"
#include"task_runner.h"

#include <cstdlib>

using namespace std;
namespace {
/// The program's tag completion source. It starts empty and answers nothing
/// until load_tags() filled `ctags`; installing it here is what keeps the editor
/// widget free of the index (TV-03).
CtagsSuggestionSource g_tagSuggestions;
}

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
    setSuggestionSource(&g_tagSuggestions);
    load_config();
    // 去重窗口(common/dedup)不再直接调某个程序的音频层:这里把共用提示音接上,
    // 四条规定与 gi 相同(单对 / 批量 / 错误 / 一条都没移走)。
    dedupenv::setSoundHandler([](DedupSound sound) {
        switch (sound) {
        case DedupSound::SingleAction:
            appaudio::playSound(appaudio::Sound::SingleAction);
            break;
        case DedupSound::BatchAction:
            appaudio::playSound(appaudio::Sound::BatchAction);
            break;
        case DedupSound::Error:
            appaudio::playSound(appaudio::Sound::Error);
            break;
        case DedupSound::NothingRemoved:
            appaudio::playSound(appaudio::Sound::NothingRemoved);
            break;
        }
    });
    // config.json is the user's last word on tag icons/colours; the registry only
    // applies them, so it has to know them before the first tag is read.
    applyTagDecorations();
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
