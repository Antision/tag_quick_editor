#include "suggestionlineedit.h"
#include "task_runner.h"

suggestionLineEdit::suggestionLineEdit(QListWidget *suggestionBox, QWidget *parent):QLineEdit(parent),suggestionBox(suggestionBox){
    connect(suggestionBox, &QListWidget::itemClicked, this, &suggestionLineEdit::onSuggestionClicked,Qt::DirectConnection);
    connect(this, &suggestionLineEdit::textEdited,this, [this]{
        lineeditTextUpdateFlag=true;
        lineEditUpdateMutexCV.notify_one();
    },Qt::DirectConnection);
    suggestionBox->setWindowFlags(Qt::FramelessWindowHint | Qt::Tool );
    suggestionBox->setStyleSheet(suggestionLineEditStyle);
    setStyleSheet(lineeditstyle);
}

void suggestionLineEdit::moveSuggestionBox(){
    suggestionBox->show();
    QPoint pos = this->mapToGlobal(QPoint{0,height()});
    suggestionBox->setMaximumSize(width(),250);
    suggestionBox->move(pos.x(),pos.y()+height());
}

void suggestionLineEdit::onSuggestionClicked(QListWidgetItem *item){
    setText(item->data(Qt::UserRole).toString());
    emit editingFinished();
    stop();
}

void suggestionLineEdit::applySuggestions(const QVector<SuggestionItem>& items){
    suggestionBox->clear();
    QPoint pos = this->mapToGlobal(QPoint{0,height()});
    suggestionBox->move(pos);
    suggestionBox->setMinimumWidth(width());
    if (items.isEmpty()) {
        suggestionBox->hide();
        return;
    }
    suggestionBox->setSortingEnabled(false);
    // The source returns the most frequent hit first; each row goes on top, so
    // the best match ends up at the top of the visible list (as it always did).
    for (const SuggestionItem& item : items) {
        QListWidgetItem* row = new QListWidgetItem(item.display);
        row->setData(Qt::UserRole,item.text);
        suggestionBox->insertItem(0,row);
    }
    suggestionBox->setCurrentRow(0);
    suggestionBox->raise();
    suggestionBox->show();
    int totalHeight = 5;
    for (int i = 0; i < suggestionBox->count(); ++i) {
        totalHeight += suggestionBox->sizeHintForRow(i);
    }
    totalHeight = qMin(totalHeight, suggestionBox->maximumHeight());
    suggestionBox->resize(width(),totalHeight);
}

void suggestionLineEdit::start(const QString &in){
    setText(in);
    lineeditTextUpdateFlag=true;
    lineeditfocusflag=true;
    // The editor widget is reused for every tag, so a worker started by an
    // earlier edit may still be inside its sleep slice. The generation makes it
    // retire instead of running beside the new one.
    const unsigned generation = ++workerGeneration;
    SuggestionSource* source = suggestionSource();
    if (!source) {
        // No index in this program (gallery_info): there is nothing to search,
        // so no worker is started at all. The box just stays hidden.
        suggestionBox->hide();
        return;
    }
    teDetachTask("autocomplete", [this, generation, source]{
        while(lineeditfocusflag==true && !g_appShuttingDown
              && generation == workerGeneration.load()){
            if(lineeditTextUpdateFlag==true){
                lineeditTextUpdateFlag=false;
                QString newtext=text();
                if(newtext.isEmpty())
                {
                    QMetaObject::invokeMethod(this,[this]{ suggestionBox->hide(); },Qt::QueuedConnection);
                    continue;
                }
                if(newtext.back()==' '){
                    newtext.chop(1);
                }
                const QVector<SuggestionItem> items = source->suggest(newtext,50);
                QMetaObject::invokeMethod(this,[this,items]{
                    applySuggestions(items);
                },Qt::QueuedConnection);
            }
            lineEditUpdateCheckThreadMutex.lock();
            if(lineeditTextUpdateFlag==false)
                lineEditUpdateMutexCV.wait_for(lineEditUpdateCheckThreadMutex,std::chrono::milliseconds(300));
            lineEditUpdateCheckThreadMutex.unlock();
        }
    });
}

void suggestionLineEdit::stop(){
    suggestionBox->hide();
    QTimer::singleShot(0, this, &suggestionLineEdit::hide);
    lineeditfocusflag=false;
    lineEditUpdateMutexCV.notify_all();
    containerParent=nullptr;
    return;
}

void suggestionLineEdit::resetContainer(){
    emit editingFinished();
    if(containerParent)
        disconnect(this,0,containerParent,0);
    containerParent=nullptr;
}

bool suggestionLineEdit::event(QEvent *e) {
    if (e->type() == QEvent::FocusOut) {
        auto focusEvent = static_cast<QFocusEvent*>(e);
        if (focusEvent->reason() == Qt::FocusReason::OtherFocusReason ||
            focusEvent->reason() == Qt::FocusReason::ActiveWindowFocusReason||
            focusEvent->reason() == Qt::TabFocusReason) {
            setFocus();
            return true;
        }
        emit editingFinished();
        stop();
    } else if (e->type() == QEvent::KeyPress) {
        auto keyEvent = static_cast<QKeyEvent*>(e);
        if (keyEvent->key() == Qt::Key_Tab) {
            if (suggestionBox && suggestionBox->currentItem()) {
                setText(suggestionBox->currentItem()->data(Qt::UserRole).toString());
            }
            suggestionBox->hide();
            return true;
        }else if (keyEvent->key() == Qt::Key_Return) {
            emit editingFinished();
            stop();
            return true;
        }
    }
    return QLineEdit::event(e);
}

void suggestionLineEdit::keyPressEvent(QKeyEvent *event) {
    if (suggestionBox->isVisible()) {
        switch (event->key()) {
        case Qt::Key_Down:
            suggestionBox->setCurrentRow(suggestionBox->currentRow() + 1);
            return;
        case Qt::Key_Up:
            suggestionBox->setCurrentRow(suggestionBox->currentRow() - 1);
            return;
        case Qt::Key_Escape:
            suggestionBox->hide();
            return;
        default:
            break;
        }
    }
    QLineEdit::keyPressEvent(event);
}


QString lineeditstyle = QStringLiteral(R"(background:rgba(0,0,0,135);
border:white dashed 1px;
color:white;
font-size:12pt;)");

const QString suggestionLineEditStyle(R"(
QListWidget{border:1px solid #4c399e;}
QScrollBar{background:rgba(0,0,0,210);}
QScrollBar::vertical{margin: 0px 0px 0px 0px;border:0px solid #5421ff;width:5px;}
QScrollBar::horizontal{margin: 0px 0px 0px 0px;border:0px solid #5421ff;height:5px;}
QScrollBar::handle{
color:#00ff00
}
QScrollBar::handle:!pressed{background:rgba(255,255,255,60);}
QScrollBar::handle:pressed{background:rgba(255,255,255,150);}

QScrollBar::handle:vertical{min-height:20px; border-radius: 2px;}
QScrollBar::handle:horizontal{min-width:20px;border-radius: 2px;}

QScrollBar::sub-page,QScrollBar::add-page {
    background-color: None;
}
QScrollBar::add-line,QScrollBar::sub-line{
border:0px solid black;
width:0px;
height:0px;
}

QListView {
    font-size:12pt;
    show-decoration-selected: 1;
    background-color:rgb(20,20,20);
    color:white;
}

QListView::item:alternate {
    background: #EEEEEE;
}

QListView::item:selected {
    color:white;
    border: 1px solid #3c2e5c;
}

QListView::item:selected:!active {
    color:black;
    background: #8e80aa;
}

QListView::item:selected:active {
    background: #a396c2;
}

QListView::item:hover {
    background: #2d185e;
}
)");
