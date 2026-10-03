#ifndef SUGGESTIONLINEEDIT_H
#define SUGGESTIONLINEEDIT_H
#include"pch.h"
class teTagWidget;
extern const QString suggestionLineEditStyle;
extern QString lineeditstyle;
extern QWidget* global_window;
class suggestionLineEdit:public QLineEdit{

    Q_OBJECT
public:
    QListWidget*suggestionBox;
    QWidget*containerParent=nullptr;
    QString previous;
    unsigned int l=0;unsigned int r=ctags.size();
    std::atomic<bool> lineeditTextUpdateFlag=false;
    std::atomic<bool> lineeditfocusflag=false;
    std::pair<size_t, size_t> prefixRange;
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
        QLineEdit::hideEvent(event);
    }
    void moveSuggestionBox();

    void onSuggestionClicked(QListWidgetItem* item);
public slots:
    void updateSuggestionBox(getSuggestionTagsArray_type HighestFrenquencyCtags);
    void start(const QString& in);
    void stop();
    void resetContainer();
public:
    bool event(QEvent* e) override;
    void keyPressEvent(QKeyEvent* event) override;
    ~suggestionLineEdit(){
        stop();
        delete suggestionBox;
    }

};

#endif // SUGGESTIONLINEEDIT_H
