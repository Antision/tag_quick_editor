#ifndef TASK_RUNNER_H
#define TASK_RUNNER_H

/**
 * @file task_runner.h
 * @brief The one place background work is started from, so that "close the
 *        window" can never leave a thread behind.
 *
 * The pool in main.cpp is global and its tasks are detached, which means
 * nothing joins them when the window goes away: main() waited for the pool
 * *after* the event loop had already returned, and a task that never finishes
 * (the autocomplete refresh loop, a network reply that never answered) simply
 * kept the process alive with no window - a zombie in the task manager that
 * could only be killed by hand.
 *
 * Every long-lived task now (a) carries a name, so whatever is still running at
 * exit can be named in the log, and (b) can see g_appShuttingDown, so it can
 * stop on its own before the widgets it touches are destroyed.
 */

#include "pch.h"

/// Defined in main.cpp, like the pool itself.
extern BS::thread_pool<> thread_pool;

/// Set as soon as the event loop is over (main.cpp / aboutToQuit). Long-lived
/// tasks must poll this in their loop condition and return when it is true.
extern std::atomic<bool> g_appShuttingDown;

/// Bookkeeping for teDetachTask(). Names are compared by content, not by address:
/// the same literal lives in several translation units and MSVC only merges them
/// with /GF (release), so pointer identity would silently stop matching in debug.
void teTaskStarted(const char* name);
void teTaskFinished(const char* name);
size_t teOutstandingTaskCount(const char* name);
/// One line into the application log, e.g. `autocomplete x1, thumbnail-decode x3`.
/// Does nothing when the pool is empty. Meant for the shutdown path.
void teLogOutstandingTasks();
/// Waits (at most `timeout`) for every task called `name` to return. Used before
/// destroying widgets that those tasks may still be touching.
void teWaitForTask(const char* name, std::chrono::milliseconds timeout);

/**
 * @brief thread_pool.detach_task() plus a name and a completion hook.
 *
 * The name is what makes the shutdown log readable; the hook is what allows
 * teWaitForTask() to tell "still decoding one image" (fine, it will end) from
 * "looping forever" (the bug this file exists for).
 */
template <typename F>
void teDetachTask(const char* name, F&& fn)
{
    teTaskStarted(name);
    thread_pool.detach_task([name, fn = std::forward<F>(fn)]() mutable {
        struct Finish {
            const char* name;
            ~Finish() { teTaskFinished(name); }
        } finish{name};
        fn();
    });
}

#endif // TASK_RUNNER_H
