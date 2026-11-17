#include "task_runner.h"

std::atomic<bool> g_appShuttingDown{false};

namespace {

struct Entry {
    std::string name;
    size_t count;
};

struct Registry {
    std::mutex mtx;
    std::vector<Entry> entries;
};

/// Deliberately leaked: worker threads may still be finishing while the static
/// objects of this program are being destroyed, and a detached task must never
/// find its bookkeeping already gone.
Registry& registry()
{
    static Registry* instance = new Registry;
    return *instance;
}

} // namespace

void teTaskStarted(const char* name)
{
    Registry& r = registry();
    std::lock_guard<std::mutex> lock(r.mtx);
    for (Entry& entry : r.entries) {
        if (entry.name == name) {
            ++entry.count;
            return;
        }
    }
    r.entries.push_back({name, 1});
}

void teTaskFinished(const char* name)
{
    Registry& r = registry();
    std::lock_guard<std::mutex> lock(r.mtx);
    for (auto it = r.entries.begin(); it != r.entries.end(); ++it) {
        if (it->name == name) {
            if (--it->count == 0)
                r.entries.erase(it);
            return;
        }
    }
}

size_t teOutstandingTaskCount(const char* name)
{
    Registry& r = registry();
    std::lock_guard<std::mutex> lock(r.mtx);
    for (const Entry& entry : r.entries) {
        if (entry.name == name)
            return entry.count;
    }
    return 0;
}

void teLogOutstandingTasks()
{
    QString summary;
    {
        Registry& r = registry();
        std::lock_guard<std::mutex> lock(r.mtx);
        for (const Entry& entry : r.entries) {
            if (!summary.isEmpty())
                summary += QStringLiteral(", ");
            summary += QStringLiteral("%1 x%2").arg(QString::fromStdString(entry.name)).arg(entry.count);
        }
    }
    if (!summary.isEmpty())
        teLog(QStringLiteral("[exit] background task(s) still running: ") + summary);
}

void teWaitForTask(const char* name, std::chrono::milliseconds timeout)
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (teOutstandingTaskCount(name) > 0) {
        if (std::chrono::steady_clock::now() >= deadline)
            return;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}
