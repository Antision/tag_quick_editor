#include "tereftaglistwidget.h"
#include "tagmarks.h"
#include "teeditorcontrol.h"
#include "mainwindow.h"
#include "func.h"

#include <QTimer>
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
    core->teConnect(teCallbackType::edit,this,&teRefTagWidget::load);
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
    // The pool hands out empty widgets first and readCore() assigns the core
    // later, so the constructor's setStyle() may have run before the tag was
    // known. Re-apply it here: this is also what keeps the colour in step with
    // edits/merges (the survivor's text changes without a new widget).
    //
    // The decoration is recomputed first: it is a function of the tag text, and
    // the tag's own edit callbacks (this one included) run *before* the list
    // announces tagEdited, so waiting for the editor would draw the old colours.
    // A widget built during read() got its core after the control asked for it,
    // which is why the colour used to appear only on the next click.
    if(core)
        refreshTagDecoration(core);
    setStyle(teTagStyle(currentStyle<0?normal:currentStyle));
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
    // The main tag list draws its rows itself (no widget per tag), and it mirrors
    // the word order through edited_with_layout() below; a widget based main list
    // would be asked through its own API rather than through the tag.
    core->edited_with_layout();
}



bool teRefTagListWidget::focusNextPrevChild(bool next)
{
    // A list that merges on Tab keeps it: letting the focus chain have it first is
    // why Tab did nothing while Shift+M worked.
    if(manualMergeSupported())
        return false;
    return teTagListWidgetBase::focusNextPrevChild(next);
}

void teRefTagListWidget::keyPressEvent(QKeyEvent *event) {
    if (event->matches(QKeySequence::SelectAll)) {
        setSelectAll();
    } else if ((event->key() == Qt::Key_D && event->modifiers() == Qt::ControlModifier)||event->key() == Qt::Key_Delete) {
        tagDestroy();
    } else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter
               || event->key() == Qt::Key_F3) {
        // Enter / F3 open the *widget* editor (the add widget becomes this tag's
        // temporary editor control); F2 and the context menu keep the inline
        // lineedit, so "change the words" and "change the tag" stay two things.
        if (select_current)
            (this->*static_cast<void(teTagListWidgetBase::*)(teTagWidgetBase*,teWordWidgetBase*)>(
                &teTagListWidgetBase::tagEdit))(select_current, nullptr);
    } else if ((event->key() == Qt::Key_E && event->modifiers() == Qt::ControlModifier)||event->key() == Qt::Key_F2) {
        teTagListWidgetBase::tagEdit();
    } else if (event->key() == Qt::Key_X && event->modifiers() == Qt::ControlModifier) {
        cut();
    } else if (event->key() == Qt::Key_C && event->modifiers() == Qt::ControlModifier) {
        copy();
    } else if (event->key() == Qt::Key_V && event->modifiers() == Qt::ControlModifier) {
        paste();
    } else if (event->key() == Qt::Key_W && event->modifiers() == Qt::ControlModifier) {
        // The same thing the "+" of this list does: the add widget for a new tag.
        addButtonClicked();
    } else if ((event->key() == Qt::Key_Tab
                || (event->key() == Qt::Key_M && event->modifiers() == Qt::ShiftModifier))
               && manualMergeSupported()) {
        // 手动合并(H&E / clothes):多选按列表顺序链式合并,单选级联。
        // 只有实现了合并的列表才吃这两个键,别的列表的 Tab 照常做焦点切换。
        manualMerge();
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
            // The widget is about to be recycled: it must not stay in the
            // selection (see forgetSelection()).
            forgetSelection(reftag);
            reftag->clearWordWidgets();
            widgetpool_ref.give_back(reftag);
        }
        tags.clear();
    }else{
        telog("TagRefList currently does not support clearing specified tags");
    }
    updateAdaptiveHeight();
}

int teRefTagListWidget::findindex(std::shared_ptr<teTag> in_tag){
    // Walk the layout, not `tags.size()`: the map and the layout can disagree
    // while widgets are being recycled, and itemAt(i) is then a spacer or null.
    // Dereferencing it blind was one of the ways this list crashed on a switch.
    if(!layout||!in_tag)
        return -1;
    for(int i=0;i<layout->count();++i){
        auto* widget=dynamic_cast<teTagWidgetBase*>(layout->itemAt(i)->widget());
        if(widget&&widget->core==in_tag)
            return i;
    }
    telog("couldn't find in_tag in tags");
    return -1;
}

int teRefTagListWidget::findindex(teTagWidgetBase *in_tag){
    if(!layout||!in_tag)
        return -1;
    for(int i=0;i<layout->count();++i)
        if(layout->itemAt(i)->widget()==in_tag)
            return i;
    telog("couldn't find in_tag in tags");
    return -1;
}

void teRefTagListWidget::tagErase(int index){
    if(!layout||index<0||index>=layout->count()){
        telog("erase out of range");
        return;
    }
    auto* tag=dynamic_cast<teRefTagWidget*>(layout->itemAt(index)->widget());
    if(!tag)
        return;
    if(isSelected(tag))
        setUnselect(tag);
    // findAndErase() already hands the widget back to the pool and drops the map
    // entry; the extra give_back() here only looked harmless.
    findAndErase(tag->core);
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

void teRefTagListWidget::updateAdaptiveHeight()
{
    // The widgets inserted in this turn are not shown yet, and an unshown tag
    // reports its pre-stylesheet size hint (25px vs 30px once it is laid out).
    // Measure on the next turn, and coalesce: inserting ten tags is one update.
    if (m_heightUpdatePending)
        return;
    m_heightUpdatePending = true;
    QTimer::singleShot(0, this, [this] {
        m_heightUpdatePending = false;
        applyAdaptiveHeight();
    });
}

void teRefTagListWidget::applyAdaptiveHeight()
{
    if (!sc || !layout)
        return;
    int widgets = 0;
    int rowHeight = 0;
    for (int i = 0; i < layout->count(); ++i) {
        QWidget* widget = layout->itemAt(i)->widget();
        if (!widget)
            continue;                   // the bottom spacer
        ++widgets;
        // sizeHint, not height(): a widget the layout stretched (or one that has
        // not been laid out yet) would otherwise decide the row height.
        if (rowHeight <= 0)
            rowHeight = qMax(widget->sizeHint().height(),
                             widget->minimumSizeHint().height());
    }
    if (rowHeight <= 0)
        rowHeight = fontMetrics().height() + 10;
    // 0 条也留一行:6px 的空条读起来像被压扁的边框,而列表始终要能接住拖进来的 tag。
    // 高度规则(2026-11-24 用户定):
    //   最多 4 行,行外层留 10px;不足 4 条时按实际条数 × 行高 + 10;
    //   0 条时只占一行、不加那 10px(空列表不该比一个 tag 还高)。
    const int rows = qMin(qMax(widgets, 1), 4);
    const int wanted = rows * rowHeight + (widgets == 0 ? 0 : 10);
    // Both halves matter: the scroll area is what shows the rows, but the control
    // itself is what the editor row lays out. Fixing only the scroll area left the
    // row at the height it got while the list was empty - the visible height lagged
    // one tag behind until the page was rebuilt by switching images.
    sc->setFixedHeight(wanted);
    // The control also carries the title + "+/-" row above the scroll area; a
    // fixed height of just `wanted` would take those pixels away from the tags.
    int chrome = 0;
    if (wlayout) {
        for (int i = 0; i < wlayout->count(); ++i) {
            QLayoutItem* item = wlayout->itemAt(i);
            if (!item || item->widget() == sc)
                continue;
            QWidget* widget = item->widget();
            if (widget && widget->isHidden())
                continue;
            chrome += item->sizeHint().height();
        }
        const QMargins margins = wlayout->contentsMargins();
        chrome += margins.top() + margins.bottom();
    }
    setFixedHeight(wanted + chrome);
    updateGeometry();
    // Ask the editor page to lay out again. The control is one widget in a row,
    // and a row inside a page that was built while this list was empty kept the
    // old geometry until the page was rebuilt (switching images "fixed" it).
    for (QWidget* parent = parentWidget(); parent; parent = parent->parentWidget()) {
        parent->updateGeometry();
        if (QLayout* layout = parent->layout())
            layout->activate();
        if (qobject_cast<QScrollArea*>(parent))
            break;
    }
}

void teRefTagListWidget::showEvent(QShowEvent* event)
{
    teTagListWidgetBase::showEvent(event);
    // Widgets added while the page was hidden reported their pre-stylesheet size;
    // measure again now that they are on screen.
    updateAdaptiveHeight();
}

void teRefTagListWidget::findAndErase(std::shared_ptr<teTag> in_core){
    auto it=findIterator(in_core);
    if(it!=tags.end()){
        // Before the widget is parked/deleted: a stale selection entry is what
        // crashed the next clear() (the widget may already be a recycled or a
        // deleted object by then).
        forgetSelection(it->first);
        widgetpool_ref.give_back(it->first);
        tags.erase(it);
        updateAdaptiveHeight();
    }else{
        telog("[teRefTagListWidget::findAndErase]:Couldn't find tag in reftaglistwidget");
    }
}

void teRefTagListWidget::Destroy(teRefTagWidget *tag){
    if(!tag)
        return;
    auto it=tags.find(tag);
    if(it==tags.end())
        return;                 // 已经不在:它的 destroy 回调收走过它
    teTagList* parentlist = it->second;
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
        // Snapshot: destroying the current one runs the destroy callback, which
        // forgets widgets (and selection entries) synchronously - walking the
        // live set while it mutates was undefined behaviour.
        const std::set<teTagWidgetBase*> chosen=select;
        Destroy((teRefTagWidget*)(select_current));
        for(auto t:chosen){
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

extern QWidget*global_window;
void teRefTagListWidget::tagInsertAbove(bool edit, std::shared_ptr<teTag>newtag,int removeDuplicate){
    if(!newtag){
        // 右键菜单的 "insert above":打开新增窗口,由它把 tag 写到当前 tag 上方。
        // 旧代码把 edit(bool) 当成 index 传给 taginsert(),还带着空 tag —— 直接崩。
        if(edit)
            insertRequested(false);
        return;
    }
    taginsert(-1,newtag,removeDuplicate);
}

void teRefTagListWidget::tagInsertBelow(bool edit, std::shared_ptr<teTag>newtag,int removeDuplicate){
    if(newtag==nullptr){
        if(edit)
            insertRequested(true);
        else
            telog("can't insert a reftag with nullptr tagcore");
        return;
    }
    MainWindow* mwptr = (MainWindow*)global_window;
    // ui->taglist is the view now; the controller behind it owns the tag list.
    TagListView* mainTagList = mwptr->ui->taglist;
    if(select_current==nullptr){
        taginsert(mainTagList->showing_list->size(),newtag,mainTagList->showing_list,removeDuplicate);
    }else{
        taginsert(findindex(select_current->core)+1,newtag,mainTagList->showing_list,removeDuplicate);
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
            parentTagListWidget->tagInsertAbove(false, std::make_shared<teTag>(piece.text, piece.sentence), 2);
    } else {
        telog("[teRefTagListWidget::taginsert]:No taglistwidget specified");
        MainWindow* mwptr = (MainWindow*)global_window;
        for (const auto& piece : pieces)
            mwptr->ui->taglist->tagInsertAbove(false, std::make_shared<teTag>(piece.text, piece.sentence), 2);
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
    updateAdaptiveHeight();
    return widget;
}
