#include "logwindow.h"
#include "ui_logwindow.h"

namespace {
/// Keeps the console bounded so a long-running session cannot eat all memory.
constexpr int kMaxLogLines = 5000;
}

LogWindow::LogWindow(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::LogWindow)
{
    ui->setupUi(this);
    ui->logTextEdit->setReadOnly(true);
    ui->logTextEdit->setMaximumBlockCount(kMaxLogLines);
}

LogWindow::~LogWindow()
{
    delete ui;
}

void LogWindow::append(const QString& message)
{
    ui->logTextEdit->appendPlainText(message);
}

void LogWindow::clearLog()
{
    ui->logTextEdit->clear();
}

void LogWindow::on_clearButton_clicked()
{
    clearLog();
}
