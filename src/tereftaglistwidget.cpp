#include "tereftaglistwidget.h"
#include "teeditorcontrol.h"
#include "func.h"
bool teRefTagCmp(teRefTagWidget *a, teRefTagWidget *b){
    return a->core<b->core;
}

void teRefTagWidget::readCore(std::shared_ptr<teTag>in_core){
    if(core)
        teDisconnect(core.get());
    core = in_core;
    if(!core)
        return;
    // Deliberately no `core->widget=this` here: `core->widget` means "the widget
    // of the single image tag list", and the editors must not take it over (the
    // word plumbing of teTagBase installs the words of *that* list). The editors
    // resolve their own tag widget through teTagListWidgetBase::widgetForCore(),
    // which finds this widget in their layout.
    core->teConnect(teCallbackType::edit,qsl("teRefTag::readCore"),this,&teRefTagWidget::load);
    core->teConnect(teCallbackType::edit_with_layout,this,&teRefTagWidget::load);
    load();
}

void teRefTagWidget::load(){
    clearWordWidgets();
    int wordscount = core->words.count();
    for(int i=0;i<wordscount;++i){
        teRefWordWidget* newword = widgetpool_ref.getWord(core->words[i]);
        layout->insertWidget(i,newword);
        wordWidgets.insert(newword);
        connectWord(newword);
    }
}

void teRefTagWidget::clearWordWidgets(){
    for(teRefWordWidget*w:wordWidgets){
        widgetpool_ref.give_back(w);
    }
    wordWidgets.clear();
}

void teRefTagWidget::self_giveback(){
    clearWordWidgets();
    widgetpool_ref.give_back(this);
}

void teRefTagWidget::worddroped(teWordWidgetBase *in_word, int xpos){
    int wordcount = core->words.count();
    int in_id=-1;
    int i=0;
    for(;i<wordcount;++i){
        teWordWidgetBase*wordptr = dynamic_cast<teWordWidgetBase*>(layout->itemAt(i)->widget());
        if(wordptr->core!=in_word->core){
            if(xpos < wordptr->x()+wordptr->width()){
                break;
            }
        }else{
            in_id=i;
            for(i=wordcount-1;i>in_id;--i){
                wordptr = dynamic_cast<teWordWidgetBase*>(layout->itemAt(i)->widget());
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
    // The main tag list mirrors the change through its own widget - if it has
    // one (a tag of a file that is not on screen has none, and dereferencing
    // core->widget unchecked used to crash there).
    if(core->widget)
        core->widget->moveWordWidget(in_id,i);
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

int teRefTagListWidget::setSelectRange(teTagWidgetBase *in, bool ifclear){
    if(select_current==nullptr||in==nullptr||select_current==in) return -1;
    if(ifclear||!select.empty()){
        for(teTagWidgetBase*tp:select){
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
        setSelect((teTagWidgetBase*)layout->itemAt(i)->widget());
    }
    return 0;
}

void teRefTagListWidget::setSelectAll(){
    int tagcount=tags.size();
    if(tagcount==0)return;
    setSelectCurrent((teTagWidgetBase*)layout->itemAt(0)->widget());
    for(int i =1;i<tagcount;++i){
        setSelect((teTagWidgetBase*)layout->itemAt(i)->widget());
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

int teRefTagListWidget::findindex(std::shared_ptr<teTag> in_tag){
    int tagcount = tags.size();
    for(int i =0;i<tagcount;++i)
        if(((teTagWidgetBase*)layout->itemAt(i)->widget())->core==in_tag)
            return i;
    telog("couldn't find in_tag in tags");
    return -1;
}

int teRefTagListWidget::findindex(teTagWidgetBase *in_tag){
    int tagcount = tags.size();
    for(int i =0;i<tagcount;++i)
        if((teTagWidgetBase*)layout->itemAt(i)->widget()==in_tag)
            return i;
    telog("couldn't find in_tag in tags");
    return -1;
}

void teRefTagListWidget::tagErase(int index){
    if(index>tags.size()-1)telog("erase out of range");
    teRefTagWidget* tag = (teRefTagWidget*)layout->itemAt(index)->widget();
    if(isSelected(tag))
        setUnselect(tag);
    findAndErase(tag->core);
    widgetpool_ref.give_back(tag);
}

void teRefTagListWidget::tagErase(std::shared_ptr<teTag> tag){
    if(tag==nullptr&&select_current==nullptr)
        return;
    else if(tag==nullptr&&select_current!=nullptr&&!select.empty()){
        select_current->setStyle(teTagWidgetBase::normal);
        findAndErase(select_current->core);
        select_current=nullptr;
        for(auto t:select){
            t->setStyle(teTagWidgetBase::normal);
            findAndErase(t->core);
        }
        select.clear();
    }else{
        if(tag==nullptr)tag=select_current->core;
        int pos = findindex(tag);
        if(select_current&&tag==select_current->core){
            if(pos<tags.size()-1)
                setSelectCurrent((teTagWidgetBase*)layout->itemAt(pos+1)->widget());
            else if(pos>0)
                setSelectCurrent((teTagWidgetBase*)layout->itemAt(pos-1)->widget());
            else
                setUnselect(select_current);
        }
        if(pos>-1){
            findAndErase(tag);
        }else
            telog("[teRefTagListWidget::tagErase]:tag not exist");
    }
}

std::map<teRefTagWidget*,teTagList*,bool(*)(teRefTagWidget*,teRefTagWidget*)>::iterator teRefTagListWidget::findIterator(std::shared_ptr<teTag> tag){
    uint8_t tmpcmp[sizeof(teRefTagWidget)];
    teRefTagWidget*tmpcmp_p = reinterpret_cast<teRefTagWidget*>(tmpcmp);
    memcpy(&tmpcmp_p->core,&tag,sizeof(std::shared_ptr<teTag>));
    auto it = tags.lower_bound(tmpcmp_p);
    if(it==tags.end()||it->first->core!=tag){
        telog("[teRefTagListWidget::findAndErase]:Couldn't find tag in reftaglistwidget");
        return tags.end();
    }else
        return it;
}

void teRefTagListWidget::findAndErase(std::shared_ptr<teTag> in_core){
    auto it=findIterator(in_core);
    if(it!=tags.end()){
        widgetpool_ref.give_back(it->first);
        tags.erase(it);
    }else{
        telog("[teRefTagListWidget::findAndErase]:Couldn't find tag in reftaglistwidget");
    }
}

void teRefTagListWidget::Destroy(teRefTagWidget *tag){
    teTagList* parentlist = tags[tag];
    if(!parentTagListWidget||parentlist!=parentTagListWidget->showing_list){
        parentlist->erase(tag->core);
        tagErase(tag->core);
    }
    else
        parentTagListWidget->tagErase(tag->core);
}

void teRefTagListWidget::tagdroped(teTagWidgetBase *in_tag, int modifiers){
    if(!in_tag){layout->update();return;}
    int in_y=in_tag->y();
    int in_id=-1;
    int i=0;
    for(;i<tags.size();++i){
        teRefTagWidget*tagptr = (teRefTagWidget*)layout->itemAt(i)->widget();
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
        while(in_tag!=(teRefTagWidget*)layout->itemAt(in_id)->widget()){
            ++in_id;
        }
    }else --i;
    if(i==in_id){layout->update();return;}
    layout->insertItem(i,layout->takeAt(in_id));
}


void teRefTagListWidget::tagDestroy(int index){
    if(index>tags.size()-1)telog("[teRefTagListWidget::tagDestroy]erase index is out of range");
    teRefTagWidget* tag = (teRefTagWidget*)layout->itemAt(index)->widget();
    Destroy(tag);
}

void teRefTagListWidget::tagDestroy(std::shared_ptr<teTag> tag){
    teRefTagWidget* tagwidget=nullptr;
    if(std::map<teRefTagWidget*,teTagList*,bool(*)(teRefTagWidget*,teRefTagWidget*)>::iterator it=findIterator(tag);it!=tags.end()){
        tagwidget = findIterator(tag)->first;
    }
    if(tag==nullptr&&select_current==nullptr)
        return;
    else if(tag==nullptr&&select_current!=nullptr&&!select.empty()){
        Destroy((teRefTagWidget*)(select_current));
        for(auto t:select){
            Destroy((teRefTagWidget*)(t));
        }
    }else{
        if(tag==nullptr){
            tag=select_current->core;
            tagwidget=(teRefTagWidget*)(select_current);
        }
        Destroy((teRefTagWidget*)(tagwidget));
    }
}

#include "mainwindow.h"
extern QWidget*global_window;
void teRefTagListWidget::tagInsertAbove(bool edit, std::shared_ptr<teTag>newtag,int removeDuplicate){
    taginsert(edit,newtag,removeDuplicate);
}

void teRefTagListWidget::tagInsertBelow(bool edit, std::shared_ptr<teTag>newtag,int removeDuplicate){
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

void teRefTagListWidget::tagInsertAbove(bool edit, std::shared_ptr<teTag>newtag, teTagList *list,int removeDuplicate){
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

void teRefTagListWidget::tagEdit(std::shared_ptr<teTag> tag, QString text, int removeDuplicate, bool ifemit){
    if(parentTagListWidget)
        parentTagListWidget->tagEdit(tag,text,removeDuplicate,ifemit);
    else{
        telog("[teRefTagListWidget::tagEdit]:edit tag without parentTagListWidget set");
        tag->read(text,true);
    }
}
teTagWidgetBase* teRefTagListWidget::taginsert(int index,std::shared_ptr<teTag>in_tag,int removeDuplicate,bool select){
    if(parentTagListWidget)
        return taginsert(index,in_tag,parentTagListWidget->showing_list,select);
    else{
        telog("[teRefTagListWidget::taginsert]:No taglistwidget specified");
        MainWindow* mwptr = (MainWindow*)global_window;
        return taginsert(index,in_tag,mwptr->ui->taglist->showing_list,select);
    }
}


void teRefTagListWidget::paste()
{
    QString clipboardText = QApplication::clipboard()->text();
    const auto pieces = splitTextToPieces(clipboardText);

    if (parentTagListWidget) {
        for (const auto& piece : pieces)
            parentTagListWidget->tagInsertAbove(false, std::make_shared<teTag>(piece.text, nullptr, piece.sentence), 2);
    } else {
        telog("[teRefTagListWidget::taginsert]:No taglistwidget specified");
        MainWindow* mwptr = (MainWindow*)global_window;
        for (const auto& piece : pieces)
            mwptr->ui->taglist->tagInsertAbove(false, std::make_shared<teTag>(piece.text, nullptr, piece.sentence), 2);
    }
}

teTagWidgetBase *teRefTagListWidget::taginsert(int index,std::shared_ptr<teTag>in_tag,teTagList*in_list,int removeDuplicate,bool select){
    while(index<0)
        index+=tags.size()+1;
    teRefTagWidget*widget =(teRefTagWidget*)widgetpool_ref.getTag(in_tag);
    tags.insert({widget,in_list});
    layout->insertWidget(index,widget);
    connectTag(widget);
    // Same as the main tag list: the context object plus a QPointer, so a
    // recycled tag or a destroyed list cannot be touched from the callback.
    const QPointer<teRefTagWidget> guard(widget);
    QTimer::singleShot(0,this,[this,guard]{
        if(!sc)
            return;
        if(guard)
            sc->ensureWidgetVisible(guard);
        sc->horizontalScrollBar()->setValue(0);
    });
    if(select){
        setSelectCurrent(widget);
    }
    return widget;
}
