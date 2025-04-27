#include "tereftaglistwidget.h"
#include "teeditorcontrol.h"

bool teRefTagCmp(tereftag *a, tereftag *b){
    return a->core<b->core;
}

void teRefTag::readCore(std::shared_ptr<tetagcore>in_core){
    if(core)
        teDisconnect(core.get());
    core = in_core;
    core->teConnect(teCallbackType::edit,qsl("teRefTag::readCore"),this,&teRefTag::load);
    core->teConnect(teCallbackType::edit_with_layout,this,&teRefTag::load);
    load();
}

void teRefTag::load(){
    clearWordWidgets();
    int wordscount = core->words.count();
    for(int i=0;i<wordscount;++i){
        terefword* newword = widgetpool_ref.getWord(core->words[i]);
        layout->insertWidget(i,newword);
        wordWidgets.insert(newword);
        connectWord(newword);
    }
}

void teRefTag::clearWordWidgets(){
    for(terefword*w:wordWidgets){
        widgetpool_ref.give_back(w);
    }
    wordWidgets.clear();
}

void teRefTag::self_giveback(){
    clearWordWidgets();
    widgetpool_ref.give_back(this);
}

void teRefTag::worddroped(teWordBase *in_word, int xpos){
    int wordcount = core->words.count();
    int in_id=-1;
    int i=0;
    for(;i<wordcount;++i){
        teWordBase*wordptr = dynamic_cast<teWordBase*>(layout->itemAt(i)->widget());
        if(wordptr->core!=in_word->core){
            if(xpos < wordptr->x()+wordptr->width()){
                break;
            }
        }else{
            in_id=i;
            for(i=wordcount-1;i>in_id;--i){
                wordptr = dynamic_cast<teWordBase*>(layout->itemAt(i)->widget());
                if(xpos > wordptr->x()){
                    ++i;
                    goto words_loop_end;
                }
            }
            layout->update();
            return;
        }
    }
words_loop_end:
    if(in_id==-1){
        in_id = i+1;
        while(in_word->core!=core->words[in_id]){
            ++in_id;
        }
    }else --i;
    core->words.insert(i,core->words.takeAt(in_id));
    core->widget->layout->insertItem(i,core->widget->layout->takeAt(in_id));
    core->edited_with_layout();
}



void teRefTagListWidget::keyPressEvent(QKeyEvent *event) {
    if (event->matches(QKeySequence::SelectAll)) {
        setSelectAll();
    } else if ((event->key() == Qt::Key_D && event->modifiers() == Qt::ControlModifier)||event->key() == Qt::Key_Delete) {
        tagDestroy();
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

int teRefTagListWidget::setSelectRange(teTagBase *in, bool ifclear){
    if(select_current==nullptr||in==nullptr||select_current==in) return -1;
    if(ifclear||!select.empty()){
        for(teTagBase*tp:select){
            tp->setStyle(normal_style_enum);
        }
        select.clear();
    }
    int in_index,select_current_index;
    for(int i =0;i<tags.size();++i){
        QWidget*w = layout->itemAt(i)->widget();
        if(w==in)in_index=i;
        else if(w==select_current)select_current_index=i;
    }
    int direction =(in_index<select_current_index?1:-1);
    for(int i = in_index;i!=select_current_index;i+=direction){
        setSelect((teTagBase*)layout->itemAt(i)->widget());
    }
    return 0;
}

void teRefTagListWidget::setSelectAll(){
    int tagcount=tags.size();
    if(tagcount==0)return;
    setSelectCurrent((teTagBase*)layout->itemAt(0)->widget());
    for(int i =1;i<tagcount;++i){
        setSelect((teTagBase*)layout->itemAt(i)->widget());
    }
    return;
}

void teRefTagListWidget::clear(teTagList *in){
    if(lineedit->lineeditfocusflag){
        lineedit->stop();
    }
    lineedit->setParent(this);
    if(in==nullptr){
        setUnselect();
        for(auto&[reftag,list]:tags){
            reftag->clearWordWidgets();
            widgetpool_ref.give_back(reftag);
        }
        tags.clear();
    }else{
        telog("TagRefList currently does not support clearing specified tags");
    }
}

int teRefTagListWidget::findindex(std::shared_ptr<tetagcore> in_tag){
    int tagcount = tags.size();
    for(int i =0;i<tagcount;++i)
        if(((tetagbase*)layout->itemAt(i)->widget())->core==in_tag)
            return i;
    telog("couldn't find in_tag in tags");
    return -1;
}

int teRefTagListWidget::findindex(tetagbase *in_tag){
    int tagcount = tags.size();
    for(int i =0;i<tagcount;++i)
        if((tetagbase*)layout->itemAt(i)->widget()==in_tag)
            return i;
    telog("couldn't find in_tag in tags");
    return -1;
}

void teRefTagListWidget::tagErase(int index){
    if(index>tags.size()-1)telog("erase out of range");
    teRefTag* tag = (teRefTag*)layout->itemAt(index)->widget();
    if(isSelected(tag))
        setUnselect(tag);
    findAndErase(tag->core);
    widgetpool_ref.give_back(tag);
}

void teRefTagListWidget::tagErase(std::shared_ptr<tetagcore> tag){
    if(tag==nullptr&&select_current==nullptr)
        return;
    else if(tag==nullptr&&select_current!=nullptr&&!select.empty()){
        select_current->setStyle(teTagBase::normal);
        findAndErase(select_current->core);
        select_current=nullptr;
        for(auto t:select){
            t->setStyle(teTagBase::normal);
            findAndErase(t->core);
        }
        select.clear();
    }else{
        if(tag==nullptr)tag=select_current->core;
        int pos = findindex(tag);
        if(select_current&&tag==select_current->core){
            if(pos<tags.size()-1)
                setSelectCurrent((tetagbase*)layout->itemAt(pos+1)->widget());
            else if(pos>0)
                setSelectCurrent((tetagbase*)layout->itemAt(pos-1)->widget());
            else
                setUnselect(select_current);
        }
        if(pos>-1){
            findAndErase(tag);
        }else
            telog("[teRefTagListWidget::tagErase]:tag not exist");
    }
}

std::map<teRefTag*,teTagList*,bool(*)(tereftag*,tereftag*)>::iterator teRefTagListWidget::findIterator(std::shared_ptr<tetagcore> tag){
    uint8_t tmpcmp[sizeof(teRefTag)];
    teRefTag*tmpcmp_p = reinterpret_cast<teRefTag*>(tmpcmp);
    memcpy(&tmpcmp_p->core,&tag,sizeof(std::shared_ptr<tetagcore>));
    auto it = tags.lower_bound(tmpcmp_p);
    if(it==tags.end()||it->first->core!=tag){
        telog("[teRefTagListWidget::findAndErase]:Couldn't find tag in reftaglistwidget");
        return tags.end();
    }else
        return it;
}

void teRefTagListWidget::findAndErase(std::shared_ptr<teTagCore> in_core){
    auto it=findIterator(in_core);
    if(it!=tags.end()){
        widgetpool_ref.give_back(it->first);
        tags.erase(it);
    }else{
        telog("[teRefTagListWidget::findAndErase]:Couldn't find tag in reftaglistwidget");
    }
}

void teRefTagListWidget::Destroy(teRefTag *tag){
    teTagList* parentlist = tags[tag];
    if(!parentTagListWidget||parentlist!=parentTagListWidget->showing_list){
        parentlist->erase(tag->core);
        tagErase(tag->core);
    }
    else
        parentTagListWidget->tagErase(tag->core);
}

void teRefTagListWidget::tagdroped(teTagBase *in_tag, int modifiers){
    if(!in_tag){layout->update();return;}
    int in_y=in_tag->y();
    int in_id=-1;
    int i=0;
    for(;i<tags.size();++i){
        teRefTag*tagptr = (teRefTag*)layout->itemAt(i)->widget();
        if(tagptr!=in_tag){
            if(in_y < tagptr->y()){
                if(in_id!=-1&&i==in_id+1){
                    layout->update();
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
        while(in_tag!=(teRefTag*)layout->itemAt(in_id)->widget()){
            ++in_id;
        }
    }else --i;
    if(i==in_id){layout->update();return;}
    layout->insertItem(i,layout->takeAt(in_id));
}


void teRefTagListWidget::tagDestroy(int index){
    if(index>tags.size()-1)telog("[teRefTagListWidget::tagDestroy]erase index is out of range");
    teRefTag* tag = (teRefTag*)layout->itemAt(index)->widget();
    Destroy(tag);
}

void teRefTagListWidget::tagDestroy(std::shared_ptr<tetagcore> tag){
    teRefTag* tagwidget=nullptr;
    if(std::map<teRefTag*,teTagList*,bool(*)(tereftag*,tereftag*)>::iterator it=findIterator(tag);it!=tags.end()){
        tagwidget = findIterator(tag)->first;
    }
    if(tag==nullptr&&select_current==nullptr)
        return;
    else if(tag==nullptr&&select_current!=nullptr&&!select.empty()){
        Destroy((teRefTag*)(select_current));
        for(auto t:select){
            Destroy((teRefTag*)(t));
        }
    }else{
        if(tag==nullptr){
            tag=select_current->core;
            tagwidget=(teRefTag*)(select_current);
        }
        Destroy((teRefTag*)(tagwidget));
    }
}

#include "mainwindow.h"
extern QWidget*global_window;
void teRefTagListWidget::tagInsertAbove(bool edit, std::shared_ptr<tetagcore>newtag,int removeDuplicate){
    taginsert(edit,newtag,removeDuplicate);
}

void teRefTagListWidget::tagInsertBelow(bool edit, std::shared_ptr<tetagcore>newtag,int removeDuplicate){
    MainWindow* mwptr = (MainWindow*)global_window;
    if(newtag==nullptr){
        telog("can't insert a reftag with nullptr tagcore");
        return;
    }
    if(select_current==nullptr){
        taginsert(mwptr->ui->taglist->showing_list->size(),newtag,mwptr->ui->taglist->showing_list,removeDuplicate);
    }else{
        taginsert(findindex(select_current->core)+1,newtag,mwptr->ui->taglist->showing_list,removeDuplicate);
    }
    if(edit)
        teTagListWidgetBase::tagEdit();
}

void teRefTagListWidget::tagInsertAbove(bool edit, std::shared_ptr<tetagcore>newtag, teTagList *list,int removeDuplicate){
    if(newtag==nullptr){
        telog("can't insert a reftag with nullptr tagcore");
        return;
    }
    if(select_current==nullptr){
        taginsert(0,newtag,list,removeDuplicate);
    }else{
        taginsert(findindex(select_current->core),newtag,list,removeDuplicate);
    }
    if(edit)
        teTagListWidgetBase::tagEdit();
}

void teRefTagListWidget::tagEdit(std::shared_ptr<teTagCore> tag, QString text, int removeDuplicate, bool ifemit){
    if(parentTagListWidget)
        parentTagListWidget->tagEdit(tag,text,removeDuplicate,ifemit);
    else{
        telog("[teRefTagListWidget::tagEdit]:edit tag without parentTagListWidget set");
        tag->read(text,true);
    }
}
teTagBase* teRefTagListWidget::taginsert(int index,std::shared_ptr<tetagcore>in_tag,int removeDuplicate,bool select){
    if(parentTagListWidget)
        return taginsert(index,in_tag,parentTagListWidget->showing_list,select);
    else{
        telog("[teRefTagListWidget::taginsert]:No taglistwidget specified");
        MainWindow* mwptr = (MainWindow*)global_window;
        return taginsert(index,in_tag,mwptr->ui->taglist->showing_list,select);
    }
}


void teRefTagListWidget::paste(){
    QString clipboardText = QApplication::clipboard()->text();
    QStringList strlst = clipboardText.split(",");
    if(parentTagListWidget){
        for(QString& s:strlst)
            parentTagListWidget->tagInsertAbove(false,std::make_shared<tetagcore>(s.trimmed()),2);
    }else{
        telog("[teRefTagListWidget::taginsert]:No taglistwidget specified");
        MainWindow* mwptr = (MainWindow*)global_window;
        for(QString& s:strlst)
            mwptr->ui->taglist->tagInsertAbove(false,std::make_shared<tetagcore>(s.trimmed()),2);
    }
}

teTagBase *teRefTagListWidget::taginsert(int index,std::shared_ptr<tetagcore>in_tag,teTagList*in_list,int removeDuplicate,bool select){
    while(index<0)
        index+=tags.size()+1;
    tereftag*widget =(tereftag*)widgetpool_ref.getTag(in_tag);
    tags.insert({widget,in_list});
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
