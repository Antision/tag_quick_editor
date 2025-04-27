#include "tetaglistwidget.h"
#include "teeditor.h"
#include "teeditorlist.h"
#include "tepicturefile.h"
#include "tetag.h"
#include "tesignalwidget.h"
QStringList ClipBoard;

teTagListWidgetBase::teTagListWidgetBase(int wordsize,QWidget *parent): QWidget{parent}{
    lineedit=new suggestionLineEdit(suggestionBox,this);
    plaintextedit = new teInputWidget;
    QSizePolicy sp =QSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
    sp.setHorizontalStretch(1);
    sp.setVerticalStretch(1);
    lineedit->setSizePolicy(sp);

    wlayout = new QVBoxLayout(this);
    wlayout->setSpacing(0);
    wlayout->setContentsMargins(0,0,0,0);
    sc = new QScrollArea(this);
    sc->setStyleSheet(liststyle.arg(wordsize));
    sc->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
    sc->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    sc->setContentsMargins(0,0,0,0);
    wlayout->addWidget(sc);
    QWidget * container = new QWidget(sc);
    layout = new QVBoxLayout(container);
    layout->setSpacing(0);
    layout->setContentsMargins(1,1,1,1);
    sc->setWidget(container);
    sc->setWidgetResizable(true);
    container->setLayout(layout);
    suggestionBox->hide();
    QSpacerItem* bottomspacer = new QSpacerItem(0,0,QSizePolicy::Fixed,QSizePolicy::Expanding);
    layout->insertSpacerItem(-1,bottomspacer);
    lineedit->hide();
    editAction = menu->addAction(QIcon(":/res/menu_edit.png"),"edit (F2/Ctrl+E)");
    deleteAction = menu->addAction(QIcon(":/res/menu_remove.png"),"delete (Ctrl+D/del)");
    insertAction = menu->addAction(QIcon(":/res/menu_add.png"),"insert above (Ctrl+W)");
    insertBelowAction = menu->addAction(QIcon(":/res/menu_add.png"),"insert below");
    cutAction = menu->addAction(QIcon(":/res/menu_cut.png"),"cut (Ctrl+X)");
    copyAction = menu->addAction(QIcon(":/res/menu_copy.png"),"copy (Ctrl+C)");
    pasteAction = menu->addAction(QIcon(":/res/menu_paste.png"),"paste (Ctrl+V)");
    menu->setStyleSheet(
        R"(QMenu {
        background-color: transparent;
        font: 14px "Segoe UI";
        color:#8dfda7;
        border-radius:3px;
    }
    QMenu::item {
        background-color: rgb(27,29,37);
        padding: 3px;
        border-radius:2px;
        margin: 0px;
        border: 1px solid #8b9ac6;
    }
    QMenu::item:selected {background-color: #3d258f;}
)");
    menu->setAttribute(Qt::WA_TranslucentBackground);
    menu->setWindowFlags(menu->windowFlags() | Qt::FramelessWindowHint);
}

teTagListWidgetBase::~teTagListWidgetBase(){delete plaintextedit;}

teTagListWidget::teTagListWidget(QWidget *parent):teTagListWidgetBase(15,parent){
}

void teTagListWidgetBase::enterEvent(QEnterEvent *e){
    sc->setFocus();
}

size_t teTagListWidget::size() const{
    return showing_list->size();
}

bool teTagListWidgetBase::isSelected(teTagBase *in){
    if(select_current==in) return true;
    else if(select.find(in)!=select.end()) return true;
    else return false;
}

void teTagListWidgetBase::tagEdit(teTagBase *tag, teWordBase *inw){
    editingTag = tag;
    if(tag->core->type==teTagCore::sentence){
        plaintextedit->start(*tag->core);
        plaintextedit->show();
        connect(plaintextedit,&teInputWidget::stringSignal,this,&teTagListWidgetBase::onPlainTextEditStop,Qt::DirectConnection);
        connect(plaintextedit,&teInputWidget::cancelSignal,this,[this]{
            disconnect(plaintextedit,&teInputWidget::stringSignal,0,0);
            disconnect(plaintextedit,&teInputWidget::cancelSignal,0,0);
        });
    }else{
        int maxsize=0;
        int begin=0;
        int count=0;
        QString tagtext="";
        if(inw!=nullptr){
            for(tewordcore*&word:tag->core->words){
                if(inw!=(teWordBase*)word->widget){
                    maxsize+=word->text.size()+1;
                }else{
                    begin=maxsize;
                    maxsize+=word->text.size()+1;
                    count = word->text.size()+1;
                }
            }
        }else{
            for(tewordcore*&word:tag->core->words){
                maxsize+=word->text.size()+1;
            }
        }
        tagtext.reserve(maxsize);
        for(tewordcore*&word:tag->core->words){
            word->widget->hide();
            tagtext.append(word->text);
            tagtext.append(' ');
        }
        lineedit->start(tagtext);
        tag->layout->insertWidget(0,lineedit);
        connect(lineedit,&suggestionLineEdit::editingFinished,this,&teTagListWidgetBase::onLineEditStop,Qt::DirectConnection);
        lineedit->containerParent=tag;
        lineedit->show();
        lineedit->moveSuggestionBox();
        lineedit->setFocus();
        inw==nullptr?lineedit->selectAll():lineedit->setSelection(begin,count);
    }
}

void teTagListWidgetBase::onPlainTextEditStop(QString in_str){
    disconnect(plaintextedit,0,this,0);
    tagEdit(editingTag->core,in_str);
    editingTag=nullptr;
}

void teTagListWidgetBase::onLineEditStop(){
    disconnect(lineedit,0,this,0);
    editingTag->layout->removeWidget(lineedit);
    tagEdit(editingTag->core,lineedit->text());
    editingTag=nullptr;
}

void teTagListWidgetBase::setSelectCurrent(teTagBase *tag, bool ifclear){
    if(ifclear){
        if(select_current!=nullptr&&tag!=select_current){
            select_current->setStyle(normal_style_enum);
        }
        if(!select.empty()){
            for(teTagBase*tp:select){
                tp->setStyle(normal_style_enum);
            }
            select.clear();
        }
    }else{
        if(select_current==tag)return;
        if(select.find(tag)!=select.end()){
            select.erase(tag);
        }
        if(select_current!=nullptr){
            select_current->setStyle(select_style_enum);
            select.insert(select_current);
        }
    }
    select_current=tag;
    tmpSelectIndex = findWidgetIndexInLayout(layout,select_current);

    if(tag!=nullptr)
        tag->setStyle(select_current_style_enum);
}

int teTagListWidget::setSelectRange(teTagBase *in,bool ifclear){
    if(select_current==nullptr||in==nullptr||select_current==in) return -1;
    if(ifclear||!select.empty()){
        for(teTagBase*tp:select){
            tp->setStyle(normal_style_enum);
        }
        select.clear();
    }
    for(int i = 0;i<showing_list->size();++i){
        teTagBase*thistag = (*showing_list)[i].widget;
        if(thistag==in){
            for(int j=i;j<showing_list->size();++j){
                thistag=(*showing_list)[j].widget;
                if(thistag==select_current){
                    for(int k=i;k<j;++k){
                        select.insert((*showing_list)[k].widget);
                        (*showing_list)[k].widget->setStyle(select_style_enum);
                    }break;
                }
            }break;
        }else if(thistag==select_current){
            for(int j=i;j<showing_list->size();++j){
                thistag=(*showing_list)[j].widget;
                if(thistag==in){
                    for(int k=i+1;k<=j;++k){
                        select.insert((*showing_list)[k].widget);
                        (*showing_list)[k].widget->setStyle(select_style_enum);
                    }break;
                }
            }break;
        }
    }
    return 0;
}

void teTagListWidgetBase::setSelect(teTagBase *in){
    in->setStyle(select_style_enum);
    select.insert(in);
}

int teTagListWidgetBase::setUnselect(teTagBase *in){
    if(in==nullptr){
        if(select_current!=nullptr){
            select_current->setStyle(normal_style_enum);
            select_current=nullptr;
        }
        if(!select.empty()){
            for(teTagBase*tp:select){
                tp->setStyle(normal_style_enum);
            }
            select.clear();
        }
        return 0;
    }else if(in==select_current){
        in->setStyle(normal_style_enum);
        if(!select.empty()){
            select_current=*select.begin();
            select.erase(select_current);
            select_current->setStyle(select_current_style_enum);
        }else
            select_current=nullptr;
        return 0;
    }else if(select.find(in)!=select.end()){
        in->setStyle(normal_style_enum);
        select.erase(in);
        return 0;
    }
    return 0;
}


void teTagListWidget::tagErase(int index){
    if((*showing_list)[index].widget==select_current){
        if(index<showing_list->size()-1)
            setSelectCurrent((*showing_list)[index+1].widget);
        else if(index>0)
            setSelectCurrent((*showing_list)[index-1].widget);
        else
            setUnselect(select_current);
    }
    else if(isSelected((*showing_list)[index].widget))
        setUnselect((*showing_list)[index].widget);
    showing_list->erase(index);
    QTimer::singleShot(0,[=, this](){
        sc->horizontalScrollBar()->setValue(0);
    });
}

void teTagListWidget::tagErase(std::shared_ptr<teTagCore>tag){
    if(tag==nullptr&&select_current==nullptr)
        return;
    else if(tag==nullptr&&select_current!=nullptr&&!select.empty()){
        showing_list->erase(select_current->core);
        select_current=nullptr;
        for(auto t:select)
            showing_list->erase(t->core);
        select.clear();
    }else{
        if(tag==nullptr)tag=select_current->core;
        int pos = showing_list->find(tag);
        if(select_current&&tag->widget==select_current){
            if(pos<showing_list->size()-1)
                setSelectCurrent((*showing_list)[pos+1].widget);
            else if(pos>0)
                setSelectCurrent((*showing_list)[pos-1].widget);
            else
                setUnselect(select_current);
        }
        if(pos>-1){
            if(isSelected(tag->widget))
                setUnselect(tag->widget);
            showing_list->erase(tag);
        }else
            telog("[teTagListWidget::tagErase]:tag not exist");
    }
    QTimer::singleShot(0,[=, this](){
        sc->horizontalScrollBar()->setValue(0);
    });
}


void teTagListWidget::tagEdit(std::shared_ptr<teTagCore>tag, QString text,int removeDuplicate,bool ifemit){
    if(showing_list->edit(tag,text,removeDuplicate,ifemit)==-1&&removeDuplicate==1)
        tagErase(tag);
}

void teTagListWidget::tagInsertAbove(bool edit,std::shared_ptr<teTagCore>newtag,int removeDuplicate){
    if(!showing_list)return;
    if(newtag==nullptr){
        newtag.reset(new tetagcore{});
        removeDuplicate=2;
    }
    if(select_current==nullptr){
        taginsert(0,newtag,removeDuplicate);
    }else{
        taginsert((*showing_list).find(select_current->core),newtag,removeDuplicate);
    }
    if(edit)
        teTagListWidgetBase::tagEdit();
}
void teTagListWidget::tagInsertBelow(bool edit,std::shared_ptr<teTagCore>newtag,int removeDuplicate){
    if(!showing_list)return;
    if(newtag==nullptr){
        newtag.reset(new tetagcore{});
        removeDuplicate=2;
    }
    if(select_current==nullptr){
        taginsert(showing_list->size(),newtag,removeDuplicate);
    }else{
        taginsert((*showing_list).find(select_current->core)+1,newtag,removeDuplicate);
    }
    if(edit)
        teTagListWidgetBase::tagEdit();
}
void teTagListWidget::keyPressEvent(QKeyEvent *event) {
    if(!showing_list) return;
    if (event->matches(QKeySequence::SelectAll)) {
        setSelectCurrent((*showing_list->begin())->widget);
        setSelectRange((showing_list->back())->widget);
    } else if ((event->key() == Qt::Key_W && event->modifiers() == Qt::ControlModifier)||event->key() == Qt::Key_F4) {
        tagInsertAbove();
    } else if ((event->key() == Qt::Key_D && event->modifiers() == Qt::ControlModifier)||event->key() == Qt::Key_Delete) {
        tagErase();
        setFocus();
    } else if (event->key() == Qt::Key_Delete) {
        tagErase();
        setFocus();
    } else if (event->key() == Qt::Key_Enter||event->key() == Qt::Key_Return) {
        if(!lineedit->isVisible())
            teTagListWidgetBase::tagEdit();
    } else if ((event->key() == Qt::Key_E && event->modifiers() == Qt::ControlModifier)||event->key() == Qt::Key_F2) {
        teTagListWidgetBase::tagEdit();
    } else if (event->key() == Qt::Key_X && event->modifiers() == Qt::ControlModifier) {
        cut();
    } else if (event->key() == Qt::Key_C && event->modifiers() == Qt::ControlModifier) {
        copy();
    } else if (event->key() == Qt::Key_V && event->modifiers() == Qt::ControlModifier) {
        paste();
    } else {
        QWidget::keyPressEvent(event);
    }
}

void teTagListWidget::onFileDeleted(tePictureFile *obj){
    if(obj==file){
        clear();
        file=nullptr;
        showing_list=nullptr;
        emit showinglistDestroyed();
    }
}
void teTagListWidgetBase::tagEdit(){
    if(select_current!=nullptr)
        tagEdit(select_current);
}
void teTagListWidgetBase::focusOutEvent(QFocusEvent *e){
    setUnselect();
    return QWidget::focusOutEvent(e);
}

QString teTagListWidgetBase::getSelectText(){
    QString finalStr;
    if(!select_current)return finalStr;
    finalStr.append(*select_current);
    if(select.empty())return finalStr;
    finalStr.append(", ");
    for(teTagBase*tw:select){
        finalStr.append(*tw->core);
        finalStr.append(", ");
    }
    finalStr.chop(2);
    return finalStr;
}

void teTagListWidgetBase::paste(){
    QString clipboardText = QApplication::clipboard()->text();
    QStringList strlst = clipboardText.split(",");
    for(QString& s:strlst)
        tagInsertBelow(false,std::make_shared<tetagcore>(s.trimmed()),1);
}

void teTagListWidget::load(teTagList*newlist){
    showing_list=newlist;
    showing_list->load();
    int tmptmpSelectIndex=tmpSelectIndex;
    int size = showing_list->size();
    for(int i =0;i<size;){
        teTag*tagwidget = (*showing_list)[i].widget;
        layout->insertWidget(i,tagwidget);
        tagwidget->show();
        tagwidget->setStyle(teTagBase::normal);
        connectTag(tagwidget);
        if(++i%4==0)
            QApplication::processEvents();
    }
    emit newlistloaded(newlist);

    if(tmptmpSelectIndex>-1&&showing_list->size()>0){
        if(showing_list->size()-1<tmptmpSelectIndex)
            tmptmpSelectIndex=showing_list->size()-1;
        setSelectCurrent((*showing_list)[tmptmpSelectIndex].widget);
    }
    QApplication::processEvents();
}

void teTagListWidget::loadFile(tePictureFile *f)
{
    static bool lock=false;
    if(!lock)
        lock=true;
    else return;
    QCoreApplication::processEvents();
    clear();
    file=f;
    load(&f->taglist);
    f->teConnect(teCallbackType::destroy,this,&teTagListWidget::onFileDeleted,f);
    lock=false;
}
void teTagListWidgetBase::connectTag(teTagBase*tagwidget){
    connect(tagwidget,&teTagBase::droped,this,&teTagListWidgetBase::tagdroped);
    connect(tagwidget,&teTagBase::rightButtonPress,this,&teTagListWidgetBase::onTagRightButtonClicked);
    connect(tagwidget,&teTagBase::leftButtonPress,this,&teTagListWidgetBase::onTagLeftButtonClicked);
    connect(tagwidget,&teTagBase::mouseDoubleClicked,this,static_cast<void(teTagListWidgetBase::*)(teTagBase*,teWordBase*)>(&teTagListWidgetBase::tagEdit),Qt::DirectConnection);
}

void teTagListWidgetBase::disconnectTag(teTagBase *tagwidget){
    disconnect(tagwidget,0,this,0);
    teDisconnect(tagwidget->core.get());
}
void teTagListWidget::clear(teTagList *in){
    if(lineedit->lineeditfocusflag){
        lineedit->stop();
    }
    lineedit->setParent(this);
    if(in==showing_list||in==nullptr){
        file=nullptr;
        setUnselect();
        if(showing_list){
            for(std::shared_ptr<teTagCore>c:*showing_list){
                if(c->widget)
                    disconnectTag(c->widget);
            }
            disconnect(showing_list,0,this,0);
            showing_list->unload();
        }
        showing_list=nullptr;
        widgetpool.realloc();
        widgetpool_ref.realloc();
    }
}
void teTagListWidget::tagdroped(teTagBase *in_tag,int modifiers){
    if(!in_tag){layout->update();return;}
    int in_y=in_tag->y();
    int in_id=-1;
    int i=0;
    QVector<std::shared_ptr<teTagCore>>&tags=showing_list->tags;
    for(;i<showing_list->size();++i){
        tetag*tagptr = tags[i]->widget;
        if(tagptr!=in_tag){
            if(in_y < tagptr->y()){
                if(in_id!=-1&&i==in_id+1){
                    layout->update();
                    if(modifiers&Qt::ControlModifier){
                        if(select_current)
                            setSelect(in_tag);
                        else
                            setSelectCurrent(in_tag);
                    }else if(modifiers&Qt::ShiftModifier){

                    }else{
                        setSelectCurrent(in_tag,true);
                    }
                    return;
                }
                break;
            }
        }else{
            in_id=i;
        }
    }
    if(in_id==-1){
        in_id = i+1;
        while(in_tag!=tags[in_id]->widget){
            ++in_id;
        }
    }else --i;
    if(i==in_id){layout->update();return;}
    showing_list->onTagMoved(tags[in_id],in_id,i);
    tags.insert(i,tags.takeAt(in_id));
    layout->insertItem(i,layout->takeAt(in_id));

    QVector<int>abovetags;
    for(int j = i-1;j>-1;--j){
        if(isSelected(tags[j]->widget)){
            abovetags.push_back(j);
        }
    }
    int pos=i-1;
    for(int n:abovetags){
        showing_list->move(n,pos);
        layout->insertItem(pos,layout->takeAt(n));
        --pos;
    }
    QVector<int>belowtags;
    for(int j = i+1;j<tags.size();++j){
        if(isSelected(tags[j]->widget)){
            belowtags.push_back(j);
        }
    }
    pos=i+1;
    for(int n:belowtags){
        showing_list->move(n,pos);
        layout->insertItem(pos,layout->takeAt(n));
        ++pos;
    }
}

teTagBase* teTagListWidget::taginsert(int index, std::shared_ptr<teTagCore>in_tag,int removeDuplicate,bool select){
    while(index<0)
        index+=showing_list->size()+1;
    if(showing_list->insert(index,in_tag,removeDuplicate)==-1){
        showing_list->onTagErased(in_tag,index);
        in_tag->teemit(teCallbackType::destroy,false);
        if(in_tag->widget)
            widgetpool.give_back(in_tag->widget);
        return nullptr;
    }
    if(in_tag->widget==nullptr)
        in_tag->load();
    tetag*widget = in_tag->widget;
    layout->insertWidget(index,widget);
    connectTag(widget);
    QTimer::singleShot(0,[this, widget]{
        sc->ensureWidgetVisible(widget);
        sc->horizontalScrollBar()->setValue(0);
    });
    if(select){
        setSelectCurrent(widget);
    }
    return widget;
}
extern bool MergeSwitch;

void teTagListWidget::undo(){
    if(!showing_list)return;
    try{
        teTagOperation&lastOp = showing_list->operationlist.take();
        switch(lastOp.type){
        case teTagOperation::taginsert:{
            MergeSwitch=false;
            showing_list->receiveTagSignals=false;
            std::shared_ptr<tetagcore>tagptr = lastOp.tag_ptr.lock();
            if(tagptr)
                tagErase(tagptr);
            showing_list->receiveTagSignals=true;
            MergeSwitch=true;
            break;
        }
        case teTagOperation::tagedit:{
            MergeSwitch=false;
            int previousOp =showing_list->operationlist.previousEditOperation(lastOp.tag_ptr.lock());
            if(previousOp<0)throw std::exception("[teTagListWidget::undo]:coult not find previous operation for edit operation");
            teTagOperation&lastEditOp = showing_list->operationlist.operations[previousOp];
            showing_list->receiveTagSignals=false;
            *lastOp.tag_ptr.lock()=lastEditOp.nowCore;
            showing_list->receiveTagSignals=true;
            MergeSwitch=true;
            break;
        }
        case teTagOperation::tagmove:{
            MergeSwitch=false;
            if(lastOp.idp>=showing_list->tags.size()){
                telog("[teTagListWidget::undo]:position in opereationList is out of range");
                lastOp.idp=showing_list->tags.size()-1;
            }
            if(showing_list->tags[lastOp.idn]!=lastOp.tag_ptr.lock()){
                telog("[teTagListWidget::undo]:position in opereationList is wrong");
                lastOp.idn=showing_list->find(lastOp.tag_ptr.lock());
            }
            showing_list->tags.insert(lastOp.idp,showing_list->tags.takeAt(lastOp.idn));
            layout->insertItem(lastOp.idp,layout->takeAt(lastOp.idn));
            MergeSwitch=true;
            break;
        }
        case teTagOperation::tagerase:{
            MergeSwitch=false;
            showing_list->receiveTagSignals=false;
            qDebug()<<"[teTagListWidget::undo]:undo tagerase operation "<<lastOp.tag_ptr.lock().get()<<lastOp.idp<<lastOp.prevCore;
            std::shared_ptr<teTagCore> newcore(new teTagCore{lastOp.prevCore});
            showing_list->operationlist.replaceTag(lastOp.tag_ptr.lock(),newcore);
            if(!taginsert(lastOp.idp,newcore,1)){
                qDebug()<<"[teTagListWidget::undo]:insert tag failed,insert a new tagerase operation "<<newcore.get()<<lastOp.idp<<lastOp.prevCore;
                showing_list->operationlist.addEraseOperation(newcore,lastOp.idp,lastOp.prevCore);
            }
            showing_list->receiveTagSignals=true;
            MergeSwitch=true;
            break;
        }
        default:break;
        }
    }catch(std::exception){
        return;
    }
}
void teTagListWidget::redo(){
    try{
        teTagOperation&nextOp = showing_list->operationlist.forward();
        switch(nextOp.type){
        case teTagOperation::taginsert:{
            MergeSwitch=false;
            showing_list->receiveTagSignals=false;
            std::shared_ptr<teTagCore> newcore(new teTagCore{nextOp.nowCore});
            showing_list->operationlist.replaceTag(nextOp.tag_ptr.lock(),newcore);
            if(!taginsert(nextOp.idn,newcore,1)){
                qDebug()<<"[teTagListWidget::redo]:insert tag failed,insert a new tagerase operation "<<newcore.get()<<nextOp.idn<<nextOp.nowCore;
                --showing_list->operationlist.rwp;
            }
            showing_list->receiveTagSignals=true;
            MergeSwitch=true;
            break;
        }
        case teTagOperation::tagedit:{
            MergeSwitch=false;
            showing_list->receiveTagSignals=false;
            *nextOp.tag_ptr.lock()=nextOp.nowCore;
            showing_list->receiveTagSignals=true;
            MergeSwitch=true;
            break;
        }
        case teTagOperation::tagmove:{
            MergeSwitch=false;
            if(nextOp.idn>=showing_list->tags.size()){
                telog("[teTagListWidget::redo]:position in opereationList is out of range");
                nextOp.idn=showing_list->tags.size()-1;
            }
            if(showing_list->tags[nextOp.idp]!=nextOp.tag_ptr.lock()){
                telog("[teTagListWidget::redo]:position in opereationList is wrong");
                nextOp.idp=showing_list->find(nextOp.tag_ptr.lock());
                if(nextOp.idp<0)
                    throw std::exception("[teTagListWidget::redo]:tag in opereationList is disappered");
            }
            showing_list->tags.insert(nextOp.idn,showing_list->tags.takeAt(nextOp.idp));
            layout->insertItem(nextOp.idn,layout->takeAt(nextOp.idp));
            MergeSwitch=true;
            break;
        }
        case teTagOperation::tagerase:{
            MergeSwitch=false;
            showing_list->receiveTagSignals=false;
            tagErase(nextOp.tag_ptr.lock());
            showing_list->receiveTagSignals=true;
            MergeSwitch=true;
            break;
        }
        default:break;
        }
    }catch(std::exception){
        return;
    }
}

void teTagListWidgetBase::onTagRightButtonClicked(teTagBase *tag, QPoint point, int modifiers){
    if(tag)
        setSelectCurrent(tag,false);
    menu->show();
    QAction *action = menu->exec(QCursor::pos());
    if(action==editAction){
        tagEdit();
    }else if(action == deleteAction){
        tagErase();
    }else if(action==insertAction){
        tagInsertAbove(true,nullptr);
    }else if(action==insertBelowAction){
        tagInsertBelow(true,nullptr);
    }else if(action==cutAction){
        cut();
    }else if(action==copyAction){
        copy();
    }else if(action==pasteAction){
        paste();
    }

}

void teTagListWidgetBase::onTagLeftButtonClicked(teTagBase *tag, QPoint point, int modifiers){
    if(modifiers&Qt::ShiftModifier&&select_current!=nullptr){
        if(modifiers&Qt::ControlModifier)
            setSelectRange(tag,false);
        else
            setSelectRange(tag);
    }else if(modifiers&Qt::ControlModifier&&select_current!=nullptr){
        if((select.find(tag)==select.end())&&tag!=select_current)
            setSelect(tag);
        else
            setUnselect(tag);
    }else{
        if(!isSelected(tag))
            setSelectCurrent(tag,true);
    }
}

void teTagListWidget::onTagEdited(std::shared_ptr<teTagCore>tag){
    if(showing_list->remove_duplicate(tag,false)){
        tagErase(tag);
        showing_list->erase(tag);
        tag->deleteLater();
        setUnselect(tag->widget);
        return;
    }
    for(teEditor*e:*editorlist){
        QTimer::singleShot(0,[this, tag]{
            sc->ensureWidgetVisible(tag->widget);
            sc->horizontalScrollBar()->setValue(0);
        });
        setSelectCurrent(tag->widget,true);
        if(e->re_read(tag)){
            break;
        }
    }
}
