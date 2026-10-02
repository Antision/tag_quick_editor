#ifndef LOGWINDOW_H
#define LOGWINDOW_H

#include <QWidget>

namespace Ui {
class LogWindow;
}

/**
 * @brief Simple read-only message console.
 *
 * Lines are fed through teSetLogTarget()/teLog() declared in func.h; the window
 * itself never needs to know who is logging.
 */
class LogWindow : public QWidget
{
    Q_OBJECT

public:
    explicit LogWindow(QWidget *parent = nullptr);
    ~LogWindow();

    /// Appends one line. Must be called on the GUI thread.
    void append(const QString& message);
    /// Removes every line.
    void clearLog();

private slots:
    void on_clearButton_clicked();

private:
    Ui::LogWindow *ui;
};

#endif // LOGWINDOW_H
