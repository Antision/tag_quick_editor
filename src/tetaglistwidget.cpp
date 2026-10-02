#include "tetaglistwidget.h"
#include "teeditor.h"
#include "teeditorlist.h"
#include "tepicturefile.h"
#include "tetag.h"
#include "tetagdisplaywidget.h"
#include "tesignalwidget.h"
#include "func.h"
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

    // Magnified hover view. The two timers implement the usual hover dance:
    // appear after a short delay, and stay alive while the pointer travels from
    // the tag into the popup.
    tagDisplay = new teTagDisplayWidget(this);
    tagDisplayTimer = new QTimer(this);
    tagDisplayTimer->setSingleShot(true);
    connect(tagDisplayTimer,&QTimer::timeout,this,[this]{
        // A model/view list has no widget for the tag it hovers, so it parks a
        // core instead of a teTagBase.
        if(pendingDisplayCore)
            showTagDisplayCore(pendingDisplayCore);
        else
            showTagDisplay(pendingDisplayTag);
    });
    tagDisplayHideTimer = new QTimer(this);
    tagDisplayHideTimer->setSingleShot(true);
    connect(tagDisplayHideTimer,&QTimer::timeout,this,&teTagListWidgetBase::tagDisplayHideTick);
    connect(tagDisplay,&teTagDisplayWidget::pointerLeft,this,&teTagListWidgetBase::scheduleTagDisplayHide);
}

teTagListWidgetBase::~teTagListWidgetBase(){
    delete plaintextedit;
    delete tagDisplay;
}

void teTagListWidgetBase::scheduleTagDisplay(teTagBase* tag){
    if(!tag||!tag->core||!tagDisplay)
        return;
    pendingDisplayCore.reset();
    pendingDisplayTag = tag;
    if(tagDisplayHideTimer)
        tagDisplayHideTimer->stop();
    if(tagDisplay->isVisible())
        showTagDisplay(tag);            // moving from one tag to the next
    else
        tagDisplayTimer->start(350);
}

void teTagListWidgetBase::scheduleTagDisplayCore(std::shared_ptr<teTagCore> core){
    if(!core||!tagDisplay)
        return;
    pendingDisplayTag = nullptr;
    pendingDisplayCore = core;
    if(tagDisplayHideTimer)
        tagDisplayHideTimer->stop();
    if(tagDisplay->isVisible())
        showTagDisplayCore(core);
    else
        tagDisplayTimer->start(350);
}

void teTagListWidgetBase::showTagDisplay(teTagBase* tag){
    if(!tag||!tag->core||!tagDisplay)
        return;
    tagDisplay->showFor(tag,QCursor::pos());
}

void teTagListWidgetBase::showTagDisplayCore(std::shared_ptr<teTagCore> core){
    if(!core||!tagDisplay)
        return;
    tagDisplay->showForCore(core,QCursor::pos());
}

void teTagListWidgetBase::hideTagDisplay(){
    if(tagDisplayTimer)
        tagDisplayTimer->stop();
    if(tagDisplayHideTimer)
        tagDisplayHideTimer->stop();
    pendingDisplayTag = nullptr;
    pendingDisplayCore.reset();
    if(tagDisplay)
        tagDisplay->hideDisplay();
}

void teTagListWidgetBase::scheduleTagDisplayHide(){
    if(!tagDisplay||!tagDisplay->isVisible())
        return;
    tagDisplayHideTimer->start(180);
}

void teTagListWidgetBase::tagDisplayHideTick(){
    const QPoint pos = QCursor::pos();
    if(tagDisplay&&tagDisplay->isVisible()&&tagDisplay->frameGeometry().contains(pos))
        return;                         // the pointer is inside the popup
    // Enter/Leave for a parent/child pair arrives in an order that depends on
    // the platform, so rather than trusting the last event, look at what is
    // actually under the pointer.
    if(QWidget* under = QApplication::widgetAt(pos)){
        teTagBase* tag = qobject_cast<teTagBase*>(under);
        if(!tag){
            if(auto* word = qobject_cast<teWordBase*>(under))
                tag = qobject_cast<teTagBase*>(word->parentWidget());
        }
        if(tag&&isAncestorOf(tag)){
            showTagDisplay(tag);        // still over a tag of this list
            return;
        }
    }
    hideTagDisplay();
}

bool teTagListWidgetBase::eventFilter(QObject* watched,QEvent* event){
    // The filter is installed on the tags *and* on their words: moving from a
    // tag onto one of its words sends a Leave event to the tag, which must not
    // be mistaken for "the pointer left the tag".
    teTagBase* tag = qobject_cast<teTagBase*>(watched);
    if(!tag){
        if(auto* word = qobject_cast<teWordBase*>(watched))
            tag = qobject_cast<teTagBase*>(word->parentWidget());
    }
    if(!tag)
        return QWidget::eventFilter(watched,event);
    switch(event->type()){
    case QEvent::Enter:
        scheduleTagDisplay(tag);
        break;
    case QEvent::Leave:
        scheduleTagDisplayHide();
        break;
    case QEvent::MouseButtonPress:
        // The pointer is about to interact with the tag itself (selection,
        // dragging, inline editing), so the popup must get out of the way.
        hideTagDisplay();
        break;
    default:
        break;
    }
    return false;
}

void teTagListWidgetBase::installTagDisplayFilters(teTagBase* tag){
    if(!tag)
        return;
    tag->installEventFilter(this);
    const QList<teWordBase*> words = tag->findChildren<teWordBase*>();
    for(teWordBase* word : words)
        word->installEventFilter(this);
}


void teTagListWidgetBase::enterEvent(QEnterEvent *e){
    sc->setFocus();
}

size_t teTagListWidget::size() const{
    if(showing_list)
        return showing_list->size();
    return 0;
}

std::shared_ptr<teTagCore> teTagListWidget::currentCore() const
{
    if(!m_view)
        return nullptr;
    const QModelIndex index = m_view->currentIndex();
    if(!index.isValid())
        return nullptr;
    teTagCore* raw = index.data(teTagListModel::TagCoreRole).value<teTagCore*>();
    if(!raw)
        return nullptr;
    // Recover the owning shared_ptr through the tag list itself, which is the
    // only place that owns these tags.
    if(showing_list){
        for(const std::shared_ptr<teTagCore>& core:*showing_list)
            if(core.get()==raw)
                return core;
    }
    return nullptr;
}

void teTagListWidget::scrollToTop()
{
    if(m_view&&m_view->verticalScrollBar())
        m_view->verticalScrollBar()->setValue(0);
}

void teTagListWidget::selectAllRows()
{
    if(m_view&&m_model&&m_model->rowCount()>0)
        m_view->selectAll();
}

// --------------------------------------------------------------- selection

void teTagListWidget::setSelectCurrentCore(std::shared_ptr<teTagCore> core,bool ifclear)
{
    if(!m_model||!m_view)
        return;
    if(!core){
        // Same as setSelectCurrent(nullptr) in the widget based list: clear.
        // setCurrentIndex(invalid) is a no-op in Qt, so the current row is
        // cleared through the selection model.
        if(QItemSelectionModel* selection = m_view->selectionModel()){
            selection->clearCurrentIndex();
            selection->clearSelection();
        }
        return;
    }
    const int row = m_model->rowOf(core.get());
    if(row<0)
        return;                         // not in this list: leave the selection alone
    const QModelIndex index = m_model->index(row,0);
    if(ifclear)
        m_view->clearSelection();
    m_view->setCurrentIndex(index);
    m_view->selectionModel()->select(index,QItemSelectionModel::ClearAndSelect|QItemSelectionModel::Rows);
    m_view->scrollTo(index,QAbstractItemView::EnsureVisible);
}

void teTagListWidget::setSelectCore(std::shared_ptr<teTagCore> core)
{
    if(!m_model||!m_view)
        return;
    const int row = m_model->rowOf(core.get());
    if(row<0)
        return;
    m_view->selectionModel()->select(m_model->index(row,0),
                                     QItemSelectionModel::Select|QItemSelectionModel::Rows);
}

int teTagListWidget::setUnselectCore(std::shared_ptr<teTagCore> core)
{
    if(!m_model||!m_view)
        return 0;
    if(!core){
        const int count = int(m_view->selectionModel()->selectedRows().size());
        m_view->clearSelection();
        return count;
    }
    const int row = m_model->rowOf(core.get());
    if(row<0)
        return 0;
    m_view->selectionModel()->select(m_model->index(row,0),
                                     QItemSelectionModel::Deselect|QItemSelectionModel::Rows);
    return 1;
}

int teTagListWidget::setSelectRangeCore(std::shared_ptr<teTagCore> core,bool ifclear)
{
    if(!m_model||!m_view)
        return 0;
    const int row = m_model->rowOf(core.get());
    const QModelIndex current = m_view->currentIndex();
    if(row<0||!current.isValid())
        return 0;
    if(ifclear)
        m_view->clearSelection();
    // Same as the widget based list: select from the current tag to this one.
    const int first = std::min(current.row(),row);
    const int last  = std::max(current.row(),row);
    QItemSelection selection;
    selection.select(m_model->index(first,0),m_model->index(last,0));
    m_view->selectionModel()->select(selection,QItemSelectionModel::Select|QItemSelectionModel::Rows);
    return last-first+1;
}

bool teTagListWidget::isCoreSelected(std::shared_ptr<teTagCore> core) const
{
    if(!m_model||!m_view||!core)
        return false;
    const int row = m_model->rowOf(core.get());
    if(row<0)
        return false;
    return m_view->selectionModel()->isSelected(m_model->index(row,0));
}

void teTagListWidget::ensureCoreVisible(std::shared_ptr<teTagCore> core)
{
    if(!m_model||!m_view||!core)
        return;
    const int row = m_model->rowOf(core.get());
    if(row<0)
        return;
    m_view->scrollTo(m_model->index(row,0),QAbstractItemView::EnsureVisible);
}

int teTagListWidget::setSelectRange(teTagBase *in,bool ifclear){
    // Everything in this list is addressed by tag core; the widget based
    // implementation is only reachable from the editors' own tag lists.
    return setSelectRangeCore(in?in->core:nullptr,ifclear);
}

QString teTagListWidget::getSelectText(){
    QString result;
    if(!m_model||!m_view)
        return result;
    QModelIndexList rows = m_view->selectionModel()->selectedRows();
    std::sort(rows.begin(),rows.end(),[](const QModelIndex&a,const QModelIndex&b){return a.row()<b.row();});
    if(rows.isEmpty()&&m_view->currentIndex().isValid())
        rows.append(m_view->currentIndex());        // nothing selected: the current row
    for(const QModelIndex& index:rows){
        if(teTagCore* raw = index.data(teTagListModel::TagCoreRole).value<teTagCore*>())
            result += static_cast<QString>(*raw)+"\n";
    }
    return result;
}

bool teTagListWidgetBase::isSelected(teTagBase *in){
    if(select_current==in) return true;
    else if(select.find(in)!=select.end()) return true;
    else return false;
}

teTagBase* teTagListWidgetBase::widgetForCore(const std::shared_ptr<teTagCore>& core) const
{
    if(!core)
        return nullptr;
    // Fast path: the tag list is the owner of a core's widget.
    if(core->widget)
        return core->widget;
    // Slow path: a core may be shown by a widget that never registered itself
    // (that is how the editors' teRefTag works).
    if(!layout)
        return nullptr;
    for(int i=0;i<layout->count();++i){
        if(auto* tag = dynamic_cast<teTagBase*>(layout->itemAt(i)->widget())){
            if(tag->core==core)
                return tag;
        }
    }
    return nullptr;
}

void teTagListWidgetBase::setSelectCurrentCore(std::shared_ptr<teTagCore> core,bool ifclear){
    setSelectCurrent(widgetForCore(core),ifclear);
    ensureCoreVisible(core);
}

void teTagListWidgetBase::setSelectCore(std::shared_ptr<teTagCore> core){
    if(teTagBase* tag = widgetForCore(core))
        setSelect(tag);
}

int teTagListWidgetBase::setUnselectCore(std::shared_ptr<teTagCore> core){
    if(!core)
        return setUnselect(nullptr);
    if(teTagBase* tag = widgetForCore(core))
        return setUnselect(tag);
    return 0;
}

int teTagListWidgetBase::setSelectRangeCore(std::shared_ptr<teTagCore> core,bool ifclear){
    return setSelectRange(widgetForCore(core),ifclear);
}

bool teTagListWidgetBase::isCoreSelected(std::shared_ptr<teTagCore> core) const{
    teTagBase* tag = widgetForCore(core);
    if(!tag)
        return false;
    return select_current==tag||select.find(tag)!=select.end();
}

void teTagListWidgetBase::ensureCoreVisible(std::shared_ptr<teTagCore> core){
    teTagBase* tag = widgetForCore(core);
    if(!tag||!sc)
        return;
    sc->ensureWidgetVisible(tag);
    // The bound lambda keeps the timer from firing after this widget is gone.
    QTimer::singleShot(0,this,[this]{
        if(sc)
            sc->horizontalScrollBar()->setValue(0);
    });
}

void teTagListWidgetBase::tagEdit(teTagBase *tag, teWordBase *inw){
    hideTagDisplay();
    editingTag = tag;
    if(!tag||!tag->core)
        return;                             // unknown tag: nothing to edit
    editingCore.reset();
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
    // An editor control can start this without a widget behind the tag, so the
    // core is what actually identifies the tag being edited.
    if(editingTag)
        tagEdit(editingTag->core,in_str);
    else if(editingCore)
        tagEdit(editingCore,in_str);
    editingTag=nullptr;
    editingCore.reset();
}

void teTagListWidgetBase::tagEditCore(std::shared_ptr<teTagCore> core){
    tagEdit(widgetForCore(core));
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
    hideTagDisplay();
    if(!showing_list||!m_model||index<0||index>=int(showing_list->size())){
        telog(QString("[teTagListWidget::tagErase] index %1 out of range").arg(index));
        return;
    }
    // The view keeps the current row on a neighbour of the erased one.
    const int current = m_view->currentIndex().isValid()?m_view->currentIndex().row():-1;
    if(!m_model->eraseTag(index))
        return;
    const int rows = m_model->rowCount();
    if(rows>0&&(current==index||current<0)){
        const int next = index<rows?index:rows-1;
        setSelectCurrentCore(showing_list->shareAt(next));
    }
    scrollToTop();
}

void teTagListWidget::tagErase(std::shared_ptr<teTagCore>tag){
    if(!showing_list||!m_model)
        return;
    const int current = m_view->currentIndex().isValid()?m_view->currentIndex().row():-1;
    if(tag==nullptr&&m_model->rowCount()==0)
        return;
    if(tag==nullptr){
        // "erase the selection": the current row plus every selected row.
        QModelIndexList selected = m_view->selectionModel()->selectedRows();
        if(selected.isEmpty()&&current>=0)
            selected.append(m_model->index(current,0));
        QVector<int> indexes;
        indexes.reserve(selected.size());
        for(const QModelIndex& index:selected)
            indexes.append(index.row());
        std::sort(indexes.begin(),indexes.end(),std::greater<int>());
        QStringList erased;
        for(int row:indexes){
            if(row>=0&&row<int(showing_list->size())){
                erased.append(static_cast<QString>(*showing_list->shareAt(row)));
                m_model->eraseTag(row);
            }
        }
        if(!erased.isEmpty())
            telog(QString("[teTagListWidget] erased %1 tag(s): %2")
                      .arg(erased.size()).arg(erased.join(QStringLiteral(", "))));
        const int left = m_model->rowCount();
        if(left>0)
            setSelectCurrentCore(showing_list->shareAt(std::clamp(current<0?0:current-1,0,left-1)));
        scrollToTop();
        return;
    }
    const int pos = showing_list->find(tag);
    if(pos<0){
        telog("[teTagListWidget::tagErase]:tag not exist");
        return;
    }
    if(!m_model->eraseTag(pos))
        return;
    const int left = m_model->rowCount();
    if(left>0&&pos==current)
        setSelectCurrentCore(showing_list->shareAt(pos<left?pos:left-1));
    scrollToTop();
}


void teTagListWidget::tagEdit(std::shared_ptr<teTagCore>tag, QString text,int removeDuplicate,bool ifemit){
    if(!showing_list)
        return;
    if(showing_list->edit(tag,text,removeDuplicate,ifemit)==-1&&removeDuplicate==1)
        tagErase(tag);
}

void teTagListWidget::startEditingRow(int row)
{
    if(!m_model||row<0||row>=m_model->rowCount())
        return;
    const QModelIndex index = m_model->index(row,0);
    auto* raw = index.data(teTagListModel::TagCoreRole).value<teTagCore*>();
    if(raw&&raw->type==teTagCore::sentence){
        // Sentences are edited in the plain text window, like before.
        hideTagDisplay();
        plaintextedit->start(static_cast<QString>(*raw));
        plaintextedit->show();
        connect(plaintextedit,&teInputWidget::stringSignal,this,&teTagListWidgetBase::onPlainTextEditStop,Qt::DirectConnection);
        connect(plaintextedit,&teInputWidget::cancelSignal,this,[this]{
            disconnect(plaintextedit,&teInputWidget::stringSignal,0,0);
            disconnect(plaintextedit,&teInputWidget::cancelSignal,0,0);
        });
        return;
    }
    m_view->setCurrentIndex(index);
    m_view->edit(index);
}

void teTagListWidget::tagEditCore(std::shared_ptr<teTagCore> core)
{
    hideTagDisplay();
    if(!core||!m_model)
        return;
    const int row = m_model->rowOf(core.get());
    if(row>=0)
        startEditingRow(row);
}

void teTagListWidget::tagInsertAbove(bool edit,std::shared_ptr<teTagCore>newtag,int removeDuplicate){
    if(!showing_list||!m_model)
        return;
    if(newtag==nullptr){
        newtag=std::make_shared<tetagcore>();
        removeDuplicate=2;
    }
    const int current = m_view->currentIndex().isValid()?m_view->currentIndex().row():-1;
    const int row = current<0?0:current;
    if(m_model->insertTag(row,newtag,removeDuplicate)<0)
        return;
    setSelectCurrentCore(newtag);
    if(edit)
        startEditingRow(row);
}

void teTagListWidget::tagInsertBelow(bool edit,std::shared_ptr<teTagCore>newtag,int removeDuplicate){
    if(!showing_list||!m_model)
        return;
    if(newtag==nullptr){
        newtag=std::make_shared<tetagcore>();
        removeDuplicate=2;
    }
    const int current = m_view->currentIndex().isValid()?m_view->currentIndex().row():-1;
    const int row = current<0?m_model->rowCount():current+1;
    if(m_model->insertTag(row,newtag,removeDuplicate)<0)
        return;
    setSelectCurrentCore(newtag);
    if(edit)
        startEditingRow(row);
}

void teTagListWidget::keyPressEvent(QKeyEvent *event) {
    if(!showing_list) return;
    if (event->matches(QKeySequence::SelectAll)) {
        selectAllRows();
    } else if ((event->key() == Qt::Key_W && event->modifiers() == Qt::ControlModifier)||event->key() == Qt::Key_F4) {
        tagInsertAbove();
    } else if ((event->key() == Qt::Key_D && event->modifiers() == Qt::ControlModifier)||event->key() == Qt::Key_Delete) {
        tagErase();
        setFocus();
    } else if (event->key() == Qt::Key_Enter||event->key() == Qt::Key_Return) {
        if(!lineedit->isVisible())
            tagEditCore(currentCore());
    } else if ((event->key() == Qt::Key_E && event->modifiers() == Qt::ControlModifier)||event->key() == Qt::Key_F2) {
        tagEditCore(currentCore());
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

QString teTagListWidgetBase::getSelectText()
{
    QList<std::shared_ptr<tetagcore>> cores;

    if (select_current && select_current->core)
        cores.push_back(select_current->core);

    for (teTagBase* tw : select) {
        if (tw && tw->core)
            cores.push_back(tw->core);
    }
    return serializePieces(cores.size(), [&](int i) -> std::shared_ptr<tetagcore> {
        return cores[i];
    }, true);
}

void teTagListWidgetBase::paste()
{
    QString clipboardText = QApplication::clipboard()->text();
    const auto pieces = splitTextToPieces(clipboardText);

    for (const auto& piece : pieces) {
        if (!piece.text.isEmpty()) {
            tagInsertBelow(false, std::make_shared<tetagcore>(piece.text, nullptr, piece.sentence), 1);
        }
    }
}

void teTagListWidget::load(teTagList*newlist){
    hideTagDisplay();
    showing_list=newlist;
    if(!showing_list)
        return;
    // No teTagList::load() here any more: that is what built one widget per tag
    // (and the word widgets below it). The view draws the tags itself, so the
    // only thing that has to happen is telling the editors that asking for a
    // widget would be pointless.
    for(const std::shared_ptr<teTagCore>& core:*showing_list){
        if(core)
            core->ifViewOwned=true;
    }
    const int tmptmpSelectIndex=tmpSelectIndex;
    m_model->setTagList(showing_list);
    emit newlistloaded(newlist);

    if(tmptmpSelectIndex>-1&&m_model->rowCount()>0){
        const int row = std::min(tmptmpSelectIndex,m_model->rowCount()-1);
        setSelectCurrentCore(showing_list->shareAt(row));
    }
}

void teTagListWidget::loadFile(tePictureFile *f)
{
    if(!f)
        return;
    if(file==f&&showing_list==&f->taglist)
        return;                     // already showing this file
    clear();
    file=f;
    load(&f->taglist);
    f->teConnect(teCallbackType::destroy,this,&teTagListWidget::onFileDeleted,f);
}

void teTagListWidget::clear(teTagList *in){
    hideTagDisplay();
    if(lineedit->lineeditfocusflag){
        lineedit->stop();
    }
    lineedit->setParent(this);
    if(in==showing_list||in==nullptr){
        if(file){
            file->teDisconnect(this);
            file=nullptr;
        }
        if(m_model)
            m_model->setTagList(nullptr);
        if(showing_list){
            for(const std::shared_ptr<teTagCore>& core:*showing_list){
                if(core)
                    core->ifViewOwned=false;      // the editors may want widgets again
            }
            disconnect(showing_list,0,this,0);
            // Deliberately no teTagList::unload(): this list never loaded widgets,
            // and the pooled widgets belong to the editors' tag lists.
        }
        showing_list=nullptr;
        m_currentCoreBeforeReset.reset();
    }
}
void teTagListWidget::tagdroped(teTagBase *in_tag,int modifiers){
    // This list reorders through teTagListView/teTagListModel, so the widget
    // based drop path is unreachable here. It is still implemented because the
    // editors' tag lists share this base class.
    Q_UNUSED(in_tag);
    Q_UNUSED(modifiers);
}
void teTagListWidgetBase::connectTag(teTagBase*tagwidget){
    connect(tagwidget,&teTagBase::droped,this,&teTagListWidgetBase::tagdroped);
    connect(tagwidget,&teTagBase::rightButtonPress,this,&teTagListWidgetBase::onTagRightButtonClicked);
    connect(tagwidget,&teTagBase::leftButtonPress,this,&teTagListWidgetBase::onTagLeftButtonClicked);
    connect(tagwidget,&teTagBase::mouseDoubleClicked,this,static_cast<void(teTagListWidgetBase::*)(teTagBase*,teWordBase*)>(&teTagListWidgetBase::tagEdit),Qt::DirectConnection);
    // Drives the magnified hover view.
    installTagDisplayFilters(tagwidget);
}

void teTagListWidgetBase::disconnectTag(teTagBase *tagwidget){
    disconnect(tagwidget,0,this,0);
    teDisconnect(tagwidget->core.get());
}
teTagBase* teTagListWidget::taginsert(int index, std::shared_ptr<teTagCore>in_tag,int removeDuplicate,bool select){
    if(!showing_list||!m_model)
        return nullptr;
    while(index<0)
        index+=int(showing_list->size())+1;
    if(in_tag)
        in_tag->ifViewOwned=true;       // this list never builds a widget for it
    const int row = m_model->insertTag(index,in_tag,removeDuplicate);
    if(row<0){
        // The tag was never part of the list, so nothing may be recorded on the
        // undo stack for it. Rejected tags are destroyed like before, which is
        // what tells the editors that linked to it to let go.
        if(in_tag)
            in_tag->teemit(teCallbackType::destroy,false);
        return nullptr;
    }
    if(select)
        setSelectCurrentCore(in_tag);
    return nullptr;                     // no widget per tag any more
}
extern bool MergeSwitch;

namespace {
/// Undo/redo must not feed itself back onto the undo stack, and the auto-merge
/// of the editors must stay disabled while the operation is applied.
/// The previous code toggled two globals by hand in every branch; a single
/// early return or exception left them in the wrong state.
struct UndoRedoGuard{
    teTagList::ChangeSuppressor suppressor;
    bool savedMergeSwitch;
    explicit UndoRedoGuard(teTagList* list) : suppressor(list), savedMergeSwitch(MergeSwitch) {
        MergeSwitch = false;
    }
    ~UndoRedoGuard(){ MergeSwitch = savedMergeSwitch; }
    UndoRedoGuard(const UndoRedoGuard&) = delete;
    UndoRedoGuard& operator=(const UndoRedoGuard&) = delete;
};
}

void teTagListWidget::undo(){
    if(!showing_list)return;
    try{
        teTagOperation&lastOp = showing_list->operationlist.take();
        UndoRedoGuard guard(showing_list);
        switch(lastOp.type){
        case teTagOperation::taginsert:{
            std::shared_ptr<tetagcore>tagptr = lastOp.tag_ptr.lock();
            if(tagptr)
                tagErase(tagptr);
            break;
        }
        case teTagOperation::tagedit:{
            int previousOp =showing_list->operationlist.previousEditOperation(lastOp.tag_ptr.lock());
            if(previousOp<0)throw std::exception("[teTagListWidget::undo]:coult not find previous operation for edit operation");
            teTagOperation&lastEditOp = showing_list->operationlist.operations[previousOp];
            *lastOp.tag_ptr.lock()=lastEditOp.nowCore;
            break;
        }
        case teTagOperation::tagmove:{
            if(lastOp.idp>=showing_list->tags.size()){
                telog("[teTagListWidget::undo]:position in opereationList is out of range");
                lastOp.idp=showing_list->tags.size()-1;
            }
            if(lastOp.idn<0||lastOp.idn>=showing_list->tags.size()||lastOp.idp<0){
                telog("[teTagListWidget::undo]:position in opereationList is out of range");
                break;
            }
            if(showing_list->tags[lastOp.idn]!=lastOp.tag_ptr.lock()){
                telog("[teTagListWidget::undo]:position in opereationList is wrong");
                lastOp.idn=showing_list->find(lastOp.tag_ptr.lock());
                if(lastOp.idn<0)
                    break;
            }
            // Keep the selection on the tag the user had selected: the reorder
            // only moves rows, not the tag identities.
            const std::shared_ptr<teTagCore> currentTag = currentCore();
            showing_list->tags.insert(lastOp.idp,showing_list->tags.takeAt(lastOp.idn));
            // The order changed behind the model's back; the old code moved the
            // widget in the layout, the view has to be told instead.
            m_model->notifyExternalReorder();
            if(currentTag)
                setSelectCurrentCore(currentTag);
            break;
        }
        case teTagOperation::tagerase:{
            std::shared_ptr<teTagCore> newcore(new teTagCore{lastOp.prevCore});
            showing_list->operationlist.replaceTag(lastOp.tag_ptr.lock(),newcore);
            if(!taginsert(lastOp.idp,newcore,1)){
                telog(QString("[teTagListWidget::undo]:insert tag failed, inserting a new tagerase operation at %1").arg(lastOp.idp));
                showing_list->operationlist.addEraseOperation(newcore,lastOp.idp,lastOp.prevCore);
            }
            break;
        }
        default:break;
        }
    }catch(std::exception&){
        return;
    }
}
void teTagListWidget::redo(){
    if(!showing_list)return;
    try{
        teTagOperation&nextOp = showing_list->operationlist.forward();
        UndoRedoGuard guard(showing_list);
        switch(nextOp.type){
        case teTagOperation::taginsert:{
            std::shared_ptr<teTagCore> newcore(new teTagCore{nextOp.nowCore});
            showing_list->operationlist.replaceTag(nextOp.tag_ptr.lock(),newcore);
            if(!taginsert(nextOp.idn,newcore,1)){
                telog(QString("[teTagListWidget::redo]:insert tag failed at %1").arg(nextOp.idn));
                --showing_list->operationlist.rwp;
            }
            break;
        }
        case teTagOperation::tagedit:{
            *nextOp.tag_ptr.lock()=nextOp.nowCore;
            break;
        }
        case teTagOperation::tagmove:{
            if(nextOp.idn>=showing_list->tags.size()){
                telog("[teTagListWidget::redo]:position in opereationList is out of range");
                nextOp.idn=showing_list->tags.size()-1;
            }
            if(nextOp.idp<0||nextOp.idp>=showing_list->tags.size()||nextOp.idn<0){
                telog("[teTagListWidget::redo]:position in opereationList is out of range");
                break;
            }
            if(showing_list->tags[nextOp.idp]!=nextOp.tag_ptr.lock()){
                telog("[teTagListWidget::redo]:position in opereationList is wrong");
                nextOp.idp=showing_list->find(nextOp.tag_ptr.lock());
                if(nextOp.idp<0)
                    throw std::exception("[teTagListWidget::redo]:tag in opereationList is disappered");
            }
            const std::shared_ptr<teTagCore> currentTag = currentCore();
            showing_list->tags.insert(nextOp.idn,showing_list->tags.takeAt(nextOp.idp));
            m_model->notifyExternalReorder();
            if(currentTag)
                setSelectCurrentCore(currentTag);
            break;
        }
        case teTagOperation::tagerase:{
            tagErase(nextOp.tag_ptr.lock());
            break;
        }
        default:break;
        }
    }catch(std::exception&){
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
    // Editor controls may turn a tag into a copy of another one (a rename, a
    // colour swap, ...). Merging keeps the list free of duplicates; the old
    // code erased the tag twice here, which could take the index of a removed
    // row.
    if(showing_list&&tag&&showing_list->remove_duplicate(tag,true))
        telog("[teTagListWidget::onTagEdited] merged a duplicated tag");
}

// ---------------------------------------------------------------- the view

teTagListWidget::teTagListWidget(QWidget *parent)
    : teTagListWidgetBase(15,parent)
{
    // The widget based scroll area is kept (widgetpool is initialized with its
    // layout and the editors' lists still use it) but the tags are drawn by the
    // view from now on.
    if(sc)
        sc->hide();

    m_model = new teTagListModel(this);
    m_delegate = new teTagDelegate(this);
    m_view = new teTagListView(this);
    m_view->setModel(m_model);
    m_view->setItemDelegate(m_delegate);
    m_view->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_view->setEditTriggers(QAbstractItemView::DoubleClicked|QAbstractItemView::EditKeyPressed);
    m_view->setUniformItemSizes(true);
    m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_view->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_view->setContextMenuPolicy(Qt::CustomContextMenu);
    m_view->setDragEnabled(true);
    m_view->setAcceptDrops(true);
    m_view->setDropIndicatorShown(true);
    m_view->setDragDropMode(QAbstractItemView::InternalMove);
    m_view->setDefaultDropAction(Qt::MoveAction);
    m_view->setFrameShape(QFrame::NoFrame);
    m_view->setMouseTracking(true);
    m_view->viewport()->setMouseTracking(true);
    m_view->viewport()->installEventFilter(this);
    if(wlayout)
        wlayout->addWidget(m_view);

    connect(m_view,&QListView::doubleClicked,this,&teTagListWidget::onViewDoubleClicked);
    connect(m_view,&QListView::customContextMenuRequested,this,&teTagListWidget::onViewContextMenu);
    if(QItemSelectionModel* selection = m_view->selectionModel())
        connect(selection,&QItemSelectionModel::currentChanged,this,&teTagListWidget::onViewCurrentChanged);
    connect(m_model,&QAbstractItemModel::modelAboutToBeReset,this,[this]{
        m_currentCoreBeforeReset = currentCore();
    });
    connect(m_model,&QAbstractItemModel::modelReset,this,&teTagListWidget::onModelReset);
    // An empty row left behind by the inline editor disappears.
    connect(m_delegate,&QAbstractItemDelegate::closeEditor,this,[this](QWidget*,QAbstractItemDelegate::EndEditHint){
        finishRowEdit(m_view->currentIndex().row());
    });

    // The base class built the menu; here the actions have to work on rows.
    connect(editAction,&QAction::triggered,this,[this]{ tagEditCore(currentCore()); });
    connect(deleteAction,&QAction::triggered,this,&teTagListWidget::eraseSelectedRows);
    connect(insertAction,&QAction::triggered,this,[this]{ tagInsertAbove(true,nullptr); });
    connect(insertBelowAction,&QAction::triggered,this,[this]{ tagInsertBelow(true,nullptr); });
    connect(cutAction,&QAction::triggered,this,[this]{ cut(); });
    connect(copyAction,&QAction::triggered,this,[this]{ copy(); });
    connect(pasteAction,&QAction::triggered,this,[this]{ paste(); });
}

void teTagListView::dropEvent(QDropEvent* event)
{
    auto* tagModel = qobject_cast<teTagListModel*>(model());
    if(!tagModel){
        QListView::dropEvent(event);
        return;
    }
    QVector<int> rows;
    for(const QModelIndex& index:selectedIndexes())
        if(index.column()==0)
            rows.append(index.row());
    std::sort(rows.begin(),rows.end());
    if(rows.isEmpty()){
        event->ignore();
        return;
    }
    // One moveRows() call handles the whole block (and records it for undo);
    // the dragged rows must be contiguous for that, which a row selection is.
    const int sourceRow = rows.first();
    const int count = int(rows.size());
    for(int i=0;i<count;++i){
        if(rows[i]!=sourceRow+i){
            event->ignore();
            return;
        }
    }
    const QModelIndex target = indexAt(event->position().toPoint());
    int destination = target.isValid()?target.row():tagModel->rowCount();
    if(target.isValid()&&dropIndicatorPosition()==QAbstractItemView::BelowItem)
        ++destination;
    if(!tagModel->moveRows(QModelIndex(),sourceRow,count,QModelIndex(),destination)){
        event->ignore();
        return;
    }
    event->acceptProposedAction();
    const int first = sourceRow<destination?destination-count:destination;
    if(tagModel->rowCount()>0)
        setCurrentIndex(tagModel->index(std::clamp(first,0,tagModel->rowCount()-1),0));
}

void teTagListWidget::onViewDoubleClicked(const QModelIndex& index)
{
    if(index.isValid())
        startEditingRow(index.row());
}

void teTagListWidget::onViewCurrentChanged(const QModelIndex& current,const QModelIndex& previous)
{
    Q_UNUSED(previous);
    if(current.isValid())
        tmpSelectIndex=current.row();
}

void teTagListWidget::onModelReset()
{
    // Somebody changed the list behind the model's back (an editor inserted a
    // tag, a rename merged two tags). Rows were rebuilt, so the selection is
    // restored by tag core, which is what the editors address anyway.
    if(m_currentCoreBeforeReset){
        const int row = m_model->rowOf(m_currentCoreBeforeReset.get());
        if(row>=0)
            setSelectCurrentCore(m_currentCoreBeforeReset);
    }
    m_currentCoreBeforeReset.reset();
}

void teTagListWidget::eraseSelectedRows()
{
    if(!m_model)
        return;
    tagErase(nullptr);      // "erase the selection", shared with the editors' path
}
void teTagListWidget::finishRowEdit(int row)
{
    if(!m_model||row<0||row>=m_model->rowCount())
        return;
    teTagCore* raw = m_model->index(row,0).data(teTagListModel::TagCoreRole).value<teTagCore*>();
    if(raw&&static_cast<QString>(*raw).trimmed().isEmpty()){
        // An empty tag is not worth keeping: this is how the old inline editor
        // behaved for a freshly inserted tag.
        telog("[teTagListWidget] dropping an empty tag");
        m_model->eraseTag(row);
    }
}

void teTagListWidget::onViewContextMenu(const QPoint& pos)
{
    const QModelIndex index = m_view->indexAt(pos);
    if(index.isValid()&&!m_view->selectionModel()->isSelected(index))
        setSelectCurrentCore(showing_list&&index.row()<int(showing_list->size())
                                 ?showing_list->shareAt(index.row())
                                 :nullptr);
    menu->exec(m_view->viewport()->mapToGlobal(pos));
}

bool teTagListWidget::eventFilter(QObject* watched,QEvent* event)
{
    if(m_view&&watched==m_view->viewport()){
        switch(event->type()){
        case QEvent::MouseMove:{
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            const QModelIndex index = m_view->indexAt(mouseEvent->position().toPoint());
            if(index.isValid()){
                teTagCore* raw = index.data(teTagListModel::TagCoreRole).value<teTagCore*>();
                if(raw){
                    // The hover popup works on tag cores here: a row has no widget.
                    for(const std::shared_ptr<teTagCore>& core:*showing_list)
                        if(core.get()==raw){
                            scheduleTagDisplayCore(core);
                            break;
                        }
                }
            }else
                hideTagDisplay();
            break;
        }
        case QEvent::Leave:
            scheduleTagDisplayHide();
            break;
        default:
            break;
        }
    }
    return teTagListWidgetBase::eventFilter(watched,event);
}
