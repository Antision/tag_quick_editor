#ifndef LOGWINDOW_H
#define LOGWINDOW_H

#include <QWidget>

namespace Ui {
class LogWindow;
}

class LogWindow : public QWidget
{
    Q_OBJECT

public:
    explicit LogWindow(QWidget *parent = nullptr);
    ~LogWindow();

private slots:
    void on_clearButton_clicked();

private:
    Ui::LogWindow *ui;
};

#endif // LOGWINDOW_H
