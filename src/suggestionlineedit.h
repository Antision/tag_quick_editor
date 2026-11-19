#ifndef SUGGESTIONLINEEDIT_H
#define SUGGESTIONLINEEDIT_H
#include "tagviewcontracts.h"
#include <QLineEdit>
#include <QListWidget>
#include <atomic>
#include <condition_variable>
#include <mutex>
class teTagWidget;
extern const QString suggestionLineEditStyle;
extern QString lineeditstyle;
extern QWidget* global_window;
class suggestionLineEdit:public QLineEdit{

    Q_OBJECT
public:
    QListWidget*suggestionBox;
    QWidget*containerParent=nullptr;
    std::atomic<bool> lineeditTextUpdateFlag=false;
    std::atomic<bool> lineeditfocusflag=false;
    /// Incremented by start(): a worker from an older edit sees a different value
    /// and returns, so the widget never feeds two background threads at once.
    std::atomic<unsigned> workerGeneration{0};
    std::mutex lineEditUpdateCheckThreadMutex;
    std::condition_variable_any lineEditUpdateMutexCV;

    suggestionLineEdit(QListWidget*suggestionBox,QWidget*parent=nullptr);
    /**
     * @brief Hides the suggestion box whenever the editor is hidden.
     *
     * QWidget::hide() is not virtual, so a hide() declared here would only be
     * called when the pointer is a suggestionLineEdit - Qt hides the editor
     * through a QWidget* when editing ends, which left the suggestion box
     * floating on screen (even after switching pictures). hideEvent() is virtual
     * and catches every path.
     */
    void hideEvent(QHideEvent* event) override {
        suggestionBox->hide();
        // Hiding the editor ends the edit. The refresh worker below has to stop
        // here: it reads this widget from another thread, and a worker that only
        // ever stops through stop() would keep a pool thread (and a dangling
        // `this`) for the rest of the session - a tag edit that ended because the
        // window was closed used to be one of them.
        lineeditfocusflag = false;
        lineEditUpdateMutexCV.notify_all();
        QLineEdit::hideEvent(event);
    }
    void moveSuggestionBox();

    void onSuggestionClicked(QListWidgetItem* item);
public slots:
    void start(const QString& in);
    void stop();
    void resetContainer();
private:
    /// Fills the box with what the source returned (GUI thread).
    void applySuggestions(const QVector<SuggestionItem>& items);
public:
    bool event(QEvent* e) override;
    void keyPressEvent(QKeyEvent* event) override;
    ~suggestionLineEdit(){
        stop();
        delete suggestionBox;
    }

};

#endif // SUGGESTIONLINEEDIT_H
