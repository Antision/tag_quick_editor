#include "suggestionlineedit.h"

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

void suggestionLineEdit::updateSuggestionBox(getSuggestionTagsArray_type HighestFrenquencyCtags){
    suggestionBox->clear();
    QPoint pos = this->mapToGlobal(QPoint{0,height()});
    suggestionBox->move(pos);
    suggestionBox->setMinimumWidth(width());
    if (HighestFrenquencyCtags.empty()) {
        suggestionBox->hide();
        return;
    }
    suggestionBox->setSortingEnabled(false);
    unsigned int HighestFrenquencyCtagsSize = HighestFrenquencyCtags.size();
    for (unsigned int i=0;i<HighestFrenquencyCtagsSize; ++i) {
        auto[frequency,index] = HighestFrenquencyCtags.top();
        HighestFrenquencyCtags.pop();
        ctag&tag = ctags[index];
        QString displayText,dataText;
        if (tag.ref_pos != 0xffffffffu) {
            dataText = QString(ctags[tag.ref_pos].text);
            displayText = QString("%1 -> %2 (%3)").arg(tag.text).arg(dataText).arg(tag.frequency);
        } else {
            dataText = QString(tag.text);
            displayText = QString("%1 (%2)").arg(dataText).arg(tag.frequency);
        }
        QListWidgetItem* item = new QListWidgetItem(displayText);
        item->setData(Qt::UserRole, QString(dataText));
        suggestionBox->insertItem(0,item);
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
    previous=QStringLiteral("");
    setText(in);
    lineeditTextUpdateFlag=true;
    lineeditfocusflag=true;
    thread_pool.add([this]{
        while(lineeditfocusflag==true){
            if(lineeditTextUpdateFlag==true){
                lineeditTextUpdateFlag=false;
                QString newtext=text();
                if(newtext.isEmpty())
                {
                    QMetaObject::invokeMethod(suggestionBox,"hide");
                    previous="";continue;
                }
                if(newtext.back()==' '){
                    newtext.chop(1);
                }
                auto[diffstr,diffcharCount] = compareStrings(previous,newtext);
                if(diffcharCount>0||previous.isEmpty()){
                    auto[newl,newr] = find_prefix_range(ctags,newtext.toLatin1().data());
                    l=newl,r=newr;
                }else if(diffstr[0]!='\0'){
                    auto[newl,newr] = refine_range(ctags,previous.toLatin1().data(),l,r,diffstr,diffcharCount);
                    l=newl,r=newr;
                }
                delete diffstr;
                previous = newtext;
                auto HighestFrenquencyCtags = getSuggestionTagsArray(ctags,l,r,50);
                QMetaObject::invokeMethod(this,"updateSuggestionBox",std::move(HighestFrenquencyCtags));
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
    lineEditUpdateMutexCV.notify_one();
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
font: 12pt "Segoe UI";)");

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
    font: 12pt "Segoe UI";
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
