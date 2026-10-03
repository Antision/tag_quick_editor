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
}

teTagListWidgetBase::~teTagListWidgetBase(){
    delete plaintextedit;
}

size_t teTagListView::size() const{
    if(showing_list)
        return showing_list->size();
    return 0;
}

std::shared_ptr<teTag> teTagListView::currentCore() const
{
    if(!this)
        return nullptr;
    const QModelIndex index = this->currentIndex();
    if(!index.isValid())
        return nullptr;
    teTag* raw = index.data(teTagListModel::TagCoreRole).value<teTag*>();
    if(!raw)
        return nullptr;
    // Recover the owning shared_ptr through the tag list itself, which is the
    // only place that owns these tags.
    if(showing_list){
        for(const std::shared_ptr<teTag>& core:*showing_list)
            if(core.get()==raw)
                return core;
    }
    return nullptr;
}

void teTagListView::scrollToTop()
{
    if(this&&this->verticalScrollBar())
        this->verticalScrollBar()->setValue(0);
}

void teTagListView::selectAllRows()
{
    if(this&&m_model&&m_model->rowCount()>0)
        this->selectAll();
}

// --------------------------------------------------------------- selection

void teTagListView::setSelectCurrentCore(std::shared_ptr<teTag> core,bool ifclear)
{
    if(!m_model||!this)
        return;
    if(!core){
        // Same as setSelectCurrent(nullptr) in the widget based list: clear.
        // setCurrentIndex(invalid) is a no-op in Qt, so the current row is
        // cleared through the selection model.
        if(QItemSelectionModel* selection = this->selectionModel()){
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
        this->clearSelection();
    this->setCurrentIndex(index);
    this->selectionModel()->select(index,QItemSelectionModel::ClearAndSelect|QItemSelectionModel::Rows);
    this->scrollTo(index,QAbstractItemView::EnsureVisible);
}

void teTagListView::setSelectCore(std::shared_ptr<teTag> core)
{
    if(!m_model||!this)
        return;
    const int row = m_model->rowOf(core.get());
    if(row<0)
        return;
    this->selectionModel()->select(m_model->index(row,0),
                                     QItemSelectionModel::Select|QItemSelectionModel::Rows);
}

int teTagListView::setUnselectCore(std::shared_ptr<teTag> core)
{
    if(!m_model||!this)
        return 0;
    if(!core){
        const int count = int(this->selectionModel()->selectedRows().size());
        this->clearSelection();
        return count;
    }
    const int row = m_model->rowOf(core.get());
    if(row<0)
        return 0;
    this->selectionModel()->select(m_model->index(row,0),
                                     QItemSelectionModel::Deselect|QItemSelectionModel::Rows);
    return 1;
}

int teTagListView::setSelectRangeCore(std::shared_ptr<teTag> core,bool ifclear)
{
    if(!m_model||!this)
        return 0;
    const int row = m_model->rowOf(core.get());
    const QModelIndex current = this->currentIndex();
    if(row<0||!current.isValid())
        return 0;
    if(ifclear)
        this->clearSelection();
    // Same as the widget based list: select from the current tag to this one.
    const int first = std::min(current.row(),row);
    const int last  = std::max(current.row(),row);
    QItemSelection selection;
    selection.select(m_model->index(first,0),m_model->index(last,0));
    this->selectionModel()->select(selection,QItemSelectionModel::Select|QItemSelectionModel::Rows);
    return last-first+1;
}

bool teTagListView::isCoreSelected(std::shared_ptr<teTag> core) const
{
    if(!m_model||!this||!core)
        return false;
    const int row = m_model->rowOf(core.get());
    if(row<0)
        return false;
    return this->selectionModel()->isSelected(m_model->index(row,0));
}

void teTagListView::ensureCoreVisible(std::shared_ptr<teTag> core)
{
    if(!m_model||!this||!core)
        return;
    const int row = m_model->rowOf(core.get());
    if(row<0)
        return;
    this->scrollTo(m_model->index(row,0),QAbstractItemView::EnsureVisible);
}

QPoint teTagListView::tagDisplayAnchor(std::shared_ptr<teTag> core) const
{
    if(!m_model||!this||!core)
        return QCursor::pos();
    const int row = m_model->rowOf(core.get());
    if(row<0)
        return QCursor::pos();
    const QRect rect = this->visualRect(m_model->index(row,0));
    if(!rect.isValid()||rect.isEmpty())
        return QCursor::pos();
    // Right edge of the view, at the height of that row.
    return this->viewport()->mapToGlobal(QPoint(this->viewport()->width(),rect.center().y()));
}

QString teTagListView::getSelectText(){
    // The clipboard has to carry exactly what saving the caption would write:
    // tags joined with ", ", sentences on their own line, brackets kept glued to
    // their word. Joining the rows with newlines (as this used to do) loses all
    // of that. The widget based list already uses the same serializer.
    QVector<std::shared_ptr<teTag>> cores;
    if(m_model&&this){
        QModelIndexList rows = this->selectionModel()->selectedRows();
        std::sort(rows.begin(),rows.end(),[](const QModelIndex&a,const QModelIndex&b){return a.row()<b.row();});
        if(rows.isEmpty()&&this->currentIndex().isValid())
            rows.append(this->currentIndex());    // nothing selected: the current row
        cores.reserve(rows.size());
        for(const QModelIndex& index:rows){
            if(teTag* raw = index.data(teTagListModel::TagCoreRole).value<teTag*>())
                if(showing_list)
                    cores.push_back(showing_list->shareAt(index.row()));
        }
    }
    return serializePieces(cores.size(),[&](int i) -> std::shared_ptr<teTag> {
        return cores[i];
    },true);
}

bool teTagListWidgetBase::isSelected(teTagWidgetBase *in){
    if(select_current==in) return true;
    else if(select.find(in)!=select.end()) return true;
    else return false;
}

teTagWidgetBase* teTagListWidgetBase::widgetForCore(const std::shared_ptr<teTag>& core) const
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
        if(auto* tag = dynamic_cast<teTagWidgetBase*>(layout->itemAt(i)->widget())){
            if(tag->core==core)
                return tag;
        }
    }
    return nullptr;
}

teTagWidgetBase* teTagListWidgetBase::ensureWidgetFor(std::shared_ptr<teTag> core)
{
    if(!core)
        return nullptr;
    if(teTagWidgetBase* existing = widgetForCore(core))
        return existing;
    // Step 1 of decoupling tag and widget: the *list* is the entry point for "give
    // me the widget of this tag". The body still asks the tag to build it (the pool
    // lives there today); the next step moves that call here and the tag stops
    // knowing about widgets at all.
    core->load();
    return widgetForCore(core);
}

void teTagListWidgetBase::releaseWidgetFor(std::shared_ptr<teTag> core)
{
    if(!core)
        return;
    core->unload();                 // ditto: the pool side moves here next
}

void teTagListWidgetBase::setSelectCurrentCore(std::shared_ptr<teTag> core,bool ifclear){
    setSelectCurrent(widgetForCore(core),ifclear);
    ensureCoreVisible(core);
}

void teTagListWidgetBase::setSelectCore(std::shared_ptr<teTag> core){
    if(teTagWidgetBase* tag = widgetForCore(core))
        setSelect(tag);
}

int teTagListWidgetBase::setUnselectCore(std::shared_ptr<teTag> core){
    if(!core)
        return setUnselect(nullptr);
    if(teTagWidgetBase* tag = widgetForCore(core))
        return setUnselect(tag);
    return 0;
}

int teTagListWidgetBase::setSelectRangeCore(std::shared_ptr<teTag> core,bool ifclear){
    return setSelectRange(widgetForCore(core),ifclear);
}

bool teTagListWidgetBase::isCoreSelected(std::shared_ptr<teTag> core) const{
    teTagWidgetBase* tag = widgetForCore(core);
    if(!tag)
        return false;
    return select_current==tag||select.find(tag)!=select.end();
}

void teTagListWidgetBase::ensureCoreVisible(std::shared_ptr<teTag> core){
    teTagWidgetBase* tag = widgetForCore(core);
    if(!tag||!sc)
        return;
    sc->ensureWidgetVisible(tag);
    // The bound lambda keeps the timer from firing after this widget is gone.
    QTimer::singleShot(0,this,[this]{
        if(sc)
            sc->horizontalScrollBar()->setValue(0);
    });
}

void teTagListWidgetBase::tagEdit(teTagWidgetBase *tag, teWordWidgetBase *inw){
    editingTag = tag;
    if(!tag||!tag->core)
        return;                             // unknown tag: nothing to edit
    editingCore.reset();
    if(tag->core->type==teTag::sentence){
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
            for(teWord*&word:tag->core->words){
                if(inw!=(teWordWidgetBase*)word->widget){
                    maxsize+=word->text.size()+1;
                }else{
                    begin=maxsize;
                    maxsize+=word->text.size()+1;
                    count = word->text.size()+1;
                }
            }
        }else{
            for(teWord*&word:tag->core->words){
                maxsize+=word->text.size()+1;
            }
        }
        tagtext.reserve(maxsize);
        for(teWord*&word:tag->core->words){
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

void teTagListWidgetBase::tagEditCore(std::shared_ptr<teTag> core){
    tagEdit(widgetForCore(core));
}

void teTagListWidgetBase::onLineEditStop(){
    disconnect(lineedit,0,this,0);
    editingTag->layout->removeWidget(lineedit);
    tagEdit(editingTag->core,lineedit->text());
    editingTag=nullptr;
}

void teTagListWidgetBase::setSelectCurrent(teTagWidgetBase *tag, bool ifclear){
    if(ifclear){
        if(select_current!=nullptr&&tag!=select_current){
            select_current->setStyle(normal_style_enum);
        }
        if(!select.empty()){
            for(teTagWidgetBase*tp:select){
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
    pendingSelectionIndex = findWidgetIndexInLayout(layout,select_current);

    if(tag!=nullptr)
        tag->setStyle(select_current_style_enum);
}

void teTagListWidgetBase::setSelect(teTagWidgetBase *in){
    in->setStyle(select_style_enum);
    select.insert(in);
}

int teTagListWidgetBase::setUnselect(teTagWidgetBase *in){
    if(in==nullptr){
        if(select_current!=nullptr){
            select_current->setStyle(normal_style_enum);
            select_current=nullptr;
        }
        if(!select.empty()){
            for(teTagWidgetBase*tp:select){
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


void teTagListView::tagErase(int index){    hideTagDisplay();
    if(!showing_list||!m_model||index<0||index>=int(showing_list->size())){
        telog(QString("[teTagListView::tagErase] index %1 out of range").arg(index));
        return;
    }
    // The view keeps the current row on a neighbour of the erased one.
    const int current = this->currentIndex().isValid()?this->currentIndex().row():-1;
    if(!m_model->eraseTag(index))
        return;
    const int rows = m_model->rowCount();
    if(rows>0&&(current==index||current<0)){
        const int next = index<rows?index:rows-1;
        setSelectCurrentCore(showing_list->shareAt(next));
    }
    scrollToTop();
}

void teTagListView::tagErase(std::shared_ptr<teTag>tag){
    if(!showing_list||!m_model)
        return;
    const int current = this->currentIndex().isValid()?this->currentIndex().row():-1;
    if(tag==nullptr&&m_model->rowCount()==0)
        return;
    if(tag==nullptr){
        // "erase the selection": the current row plus every selected row.
        QModelIndexList selected = this->selectionModel()->selectedRows();
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
        telog("[teTagListView::tagErase]:tag not exist");
        return;
    }
    if(!m_model->eraseTag(pos))
        return;
    const int left = m_model->rowCount();
    if(left>0&&pos==current)
        setSelectCurrentCore(showing_list->shareAt(pos<left?pos:left-1));
    scrollToTop();
}


void teTagListView::retire(std::shared_ptr<teTag> tag)
{
    // The tag list owns the tags, so it owns retirement; this only forwards.
    if(showing_list)
        showing_list->retire(tag);
}

void teTagListView::tagEdit(std::shared_ptr<teTag>tag, QString text,int removeDuplicate,bool ifemit){
    if(!showing_list)
        return;
    if(showing_list->edit(tag,text,removeDuplicate,ifemit)==-1&&removeDuplicate==1)
        tagErase(tag);
}

void teTagListView::startEditingRow(int row)
{
    if(!m_model||row<0||row>=m_model->rowCount())
        return;
    const QModelIndex index = m_model->index(row,0);
    auto* raw = index.data(teTagListModel::TagCoreRole).value<teTag*>();
    if(raw&&raw->type==teTag::sentence){
        // Sentences are edited in the plain text window, like before.
        hideTagDisplay();
        // onPlainTextEditStop() (the confirm handler) needs to know which tag is
        // being edited; a row has no widget, so it is identified by its core.
        m_plainTextCore=showing_list?showing_list->shareAt(row):nullptr;
        plaintextedit->start(static_cast<QString>(*raw));
        plaintextedit->show();
        connect(plaintextedit,&teInputWidget::stringSignal,this,&teTagListView::onPlainTextEditStop,Qt::DirectConnection);
        connect(plaintextedit,&teInputWidget::cancelSignal,this,[this]{
            disconnect(plaintextedit,&teInputWidget::stringSignal,0,0);
            disconnect(plaintextedit,&teInputWidget::cancelSignal,0,0);
        });
        return;
    }
    this->setCurrentIndex(index);
    this->edit(index);
}

void teTagListView::tagEditCore(std::shared_ptr<teTag> core)
{
    hideTagDisplay();
    if(!core||!m_model)
        return;
    const int row = m_model->rowOf(core.get());
    if(row>=0)
        startEditingRow(row);
}

void teTagListView::adoptAsRow(std::shared_ptr<teTag> tag)
{
    if(!tag)
        return;
    // The editors build a real widget for a tag they are about to insert (the
    // controls call load() on it, and some of them ask every word for its widget
    // before the tag ever reaches this list). This list draws rows itself, so that
    // widget has to go: it was parked under the view and stayed visible - and
    // draggable - on top of the tag list. Doing it here catches every insertion
    // path, however late the flag is set.
    tag->ifViewOwned=true;
    for(teWord* word : tag->words){
        if(!word)
            continue;
        word->ifViewOwned=true;
        word->unload();
    }
    tag->unload();
}

void teTagListView::tagInsertAbove(bool edit,std::shared_ptr<teTag>newtag,int removeDuplicate){
    if(!showing_list||!m_model)
        return;
    if(newtag==nullptr){
        newtag=std::make_shared<teTag>();
        removeDuplicate=2;
    }
    adoptAsRow(newtag);
    const int current = this->currentIndex().isValid()?this->currentIndex().row():-1;
    const int row = current<0?0:current;
    if(m_model->insertTag(row,newtag,removeDuplicate)<0)
        return;
    // The editors may have merged the new tag into an existing one (they run from
    // the insertion's announcement), so find where it is now instead of trusting
    // the row we asked for.
    const int inserted = m_model->rowOf(newtag.get());
    if(inserted<0)
        return;                                 // merged away: nothing to select
    setSelectCurrentCore(newtag);
    if(edit)
        startEditingRow(inserted);
}

void teTagListView::tagInsertBelow(bool edit,std::shared_ptr<teTag>newtag,int removeDuplicate){
    if(!showing_list||!m_model)
        return;
    if(newtag==nullptr){
        newtag=std::make_shared<teTag>();
        removeDuplicate=2;
    }
    adoptAsRow(newtag);
    const int current = this->currentIndex().isValid()?this->currentIndex().row():-1;
    const int row = current<0?m_model->rowCount():current+1;
    if(m_model->insertTag(row,newtag,removeDuplicate)<0)
        return;
    const int inserted = m_model->rowOf(newtag.get());
    if(inserted<0)
        return;                                 // merged away: nothing to select
    setSelectCurrentCore(newtag);
    if(edit)
        startEditingRow(inserted);
}

void teTagListView::keyPressEvent(QKeyEvent *event) {
    if(!showing_list) return;
    if(handleTagDisplayEscape(event))
        return;
    if (event->matches(QKeySequence::SelectAll)) {
        selectAllRows();
    } else if ((event->key() == Qt::Key_W && event->modifiers() == Qt::ControlModifier)||event->key() == Qt::Key_F4) {
        tagInsertAbove();
    } else if ((event->key() == Qt::Key_D && event->modifiers() == Qt::ControlModifier)||event->key() == Qt::Key_Delete) {
        tagErase();
        if(this)
            this->setFocus();
    } else if (event->key() == Qt::Key_Enter||event->key() == Qt::Key_Return) {
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
        event->ignore();        // the view handles everything else
    }
}

void teTagListView::onFileDeleted(tePictureFile *obj){
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
    QList<std::shared_ptr<teTag>> cores;

    if (select_current && select_current->core)
        cores.push_back(select_current->core);

    for (teTagWidgetBase* tw : select) {
        if (tw && tw->core)
            cores.push_back(tw->core);
    }
    return serializePieces(cores.size(), [&](int i) -> std::shared_ptr<teTag> {
        return cores[i];
    }, true);
}

void teTagListWidgetBase::paste()
{
    QString clipboardText = QApplication::clipboard()->text();
    const auto pieces = splitTextToPieces(clipboardText);

    for (const auto& piece : pieces) {
        if (!piece.text.isEmpty()) {
            tagInsertBelow(false, std::make_shared<teTag>(piece.text, nullptr, piece.sentence), 1);
        }
    }
}

void teTagListView::load(teTagList*newlist){
    hideTagDisplay();
    showing_list=newlist;
    if(!showing_list)
        return;
    // No teTagList::load() here any more: that is what built one widget per tag
    // (and the word widgets below it). The view draws the tags itself, so the
    // only thing that has to happen is telling the editors that asking for a
    // widget would be pointless.
    for(const std::shared_ptr<teTag>& core:*showing_list){
        if(!core)
            continue;
        core->ifViewOwned=true;
        // The words carry the flag too: teWord::load() has to refuse as well, or
        // a control that scans the tag builds a widget per word (that is where the
        // loose "striped" / "side-tie" widgets over the list came from).
        for(teWord* word : core->words)
            if(word)
                word->ifViewOwned=true;
    }
    const int tmpSelectIndex=m_pendingSelectionIndex;
    m_model->setTagList(showing_list);
    emit newlistloaded(newlist);

    if(tmpSelectIndex>-1&&m_model->rowCount()>0){
        const int row = std::min(tmpSelectIndex,m_model->rowCount()-1);
        setSelectCurrentCore(showing_list->shareAt(row));
    }
}

void teTagListView::loadFile(tePictureFile *f)
{
    if(!f)
        return;
    if(file==f&&showing_list==&f->taglist)
        return;                     // already showing this file
    clear();
    file=f;
    load(&f->taglist);
    f->teConnect(teCallbackType::destroy,this,&teTagListView::onFileDeleted,f);
}

void teTagListView::clear(teTagList *in){
    hideTagDisplay();
    // Editing state of the view's inline editor belongs to the old list: end it,
    // otherwise its suggestion box outlives the picture (the editor is hidden by
    // Qt through a QWidget*, which used to leave the box on screen).
    if(m_delegate)
        m_delegate->stopEditing();
    if(in==showing_list||in==nullptr){
        if(file){
            file->teDisconnect(this);
            file=nullptr;
        }
        if(m_model)
            m_model->setTagList(nullptr);
        if(showing_list){
            for(const std::shared_ptr<teTag>& core:*showing_list){
                if(!core)
                    continue;
                core->ifViewOwned=false;      // the editors may want widgets again
                for(teWord* word : core->words)
                    if(word)
                        word->ifViewOwned=false;
            }
            disconnect(showing_list,0,this,0);
            // Deliberately no teTagList::unload(): this list never loaded widgets,
            // and the pooled widgets belong to the editors' tag lists.
        }
        showing_list=nullptr;
        m_currentCoreBeforeReset.reset();
    }
}
void teTagListWidgetBase::connectTag(teTagWidgetBase*tagwidget){
    connect(tagwidget,&teTagWidgetBase::droped,this,&teTagListWidgetBase::tagdroped);
    connect(tagwidget,&teTagWidgetBase::rightButtonPress,this,&teTagListWidgetBase::onTagRightButtonClicked);
    connect(tagwidget,&teTagWidgetBase::leftButtonPress,this,&teTagListWidgetBase::onTagLeftButtonClicked);
    connect(tagwidget,&teTagWidgetBase::mouseDoubleClicked,this,static_cast<void(teTagListWidgetBase::*)(teTagWidgetBase*,teWordWidgetBase*)>(&teTagListWidgetBase::tagEdit),Qt::DirectConnection);
    // Drives the magnified hover view.
}

void teTagListWidgetBase::disconnectTag(teTagWidgetBase *tagwidget){
    disconnect(tagwidget,0,this,0);
    teDisconnect(tagwidget->core.get());
}
teTagWidgetBase* teTagListView::taginsert(int index, std::shared_ptr<teTag>in_tag,int removeDuplicate,bool select){
    if(!showing_list||!m_model)
        return nullptr;
    while(index<0)
        index+=int(showing_list->size())+1;
    adoptAsRow(in_tag);                 // this list never builds a widget for it
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

void teTagListView::undo(){
    if(!showing_list)return;
    try{
        teTagOperation&lastOp = showing_list->operationlist.take();
        UndoRedoGuard guard(showing_list);
        switch(lastOp.type){
        case teTagOperation::taginsert:{
            std::shared_ptr<teTag>tagptr = lastOp.tag_ptr.lock();
            if(tagptr)
                tagErase(tagptr);
            break;
        }
        case teTagOperation::tagedit:{
            int previousOp =showing_list->operationlist.previousEditOperation(lastOp.tag_ptr.lock());
            if(previousOp<0)throw std::exception("[teTagListView::undo]:coult not find previous operation for edit operation");
            teTagOperation&lastEditOp = showing_list->operationlist.operations[previousOp];
            *lastOp.tag_ptr.lock()=lastEditOp.nowCore;
            break;
        }
        case teTagOperation::tagmove:{
            if(lastOp.idp>=showing_list->tags.size()){
                telog("[teTagListView::undo]:position in opereationList is out of range");
                lastOp.idp=showing_list->tags.size()-1;
            }
            if(lastOp.idn<0||lastOp.idn>=showing_list->tags.size()||lastOp.idp<0){
                telog("[teTagListView::undo]:position in opereationList is out of range");
                break;
            }
            if(showing_list->tags[lastOp.idn]!=lastOp.tag_ptr.lock()){
                telog("[teTagListView::undo]:position in opereationList is wrong");
                lastOp.idn=showing_list->find(lastOp.tag_ptr.lock());
                if(lastOp.idn<0)
                    break;
            }
            // Keep the selection on the tag the user had selected: the reorder
            // only moves rows, not the tag identities.
            const std::shared_ptr<teTag> currentTag = currentCore();
            showing_list->tags.insert(lastOp.idp,showing_list->tags.takeAt(lastOp.idn));
            // The order changed behind the model's back; the old code moved the
            // widget in the layout, the view has to be told instead.
            m_model->notifyExternalReorder();
            if(currentTag)
                setSelectCurrentCore(currentTag);
            break;
        }
        case teTagOperation::tagerase:{
            std::shared_ptr<teTag> newcore(new teTag{lastOp.prevCore});
            showing_list->operationlist.replaceTag(lastOp.tag_ptr.lock(),newcore);
            if(!taginsert(lastOp.idp,newcore,1)){
                telog(QString("[teTagListView::undo]:insert tag failed, inserting a new tagerase operation at %1").arg(lastOp.idp));
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
void teTagListView::redo(){
    if(!showing_list)return;
    try{
        teTagOperation&nextOp = showing_list->operationlist.forward();
        UndoRedoGuard guard(showing_list);
        switch(nextOp.type){
        case teTagOperation::taginsert:{
            std::shared_ptr<teTag> newcore(new teTag{nextOp.nowCore});
            showing_list->operationlist.replaceTag(nextOp.tag_ptr.lock(),newcore);
            if(!taginsert(nextOp.idn,newcore,1)){
                telog(QString("[teTagListView::redo]:insert tag failed at %1").arg(nextOp.idn));
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
                telog("[teTagListView::redo]:position in opereationList is out of range");
                nextOp.idn=showing_list->tags.size()-1;
            }
            if(nextOp.idp<0||nextOp.idp>=showing_list->tags.size()||nextOp.idn<0){
                telog("[teTagListView::redo]:position in opereationList is out of range");
                break;
            }
            if(showing_list->tags[nextOp.idp]!=nextOp.tag_ptr.lock()){
                telog("[teTagListView::redo]:position in opereationList is wrong");
                nextOp.idp=showing_list->find(nextOp.tag_ptr.lock());
                if(nextOp.idp<0)
                    throw std::exception("[teTagListView::redo]:tag in opereationList is disappered");
            }
            const std::shared_ptr<teTag> currentTag = currentCore();
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

void teTagListWidgetBase::onTagRightButtonClicked(teTagWidgetBase *tag, QPoint point, int modifiers){
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

void teTagListWidgetBase::onTagLeftButtonClicked(teTagWidgetBase *tag, QPoint point, int modifiers){
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
    // Clicking a tag is what opens the magnified popup (it used to appear after
    // hovering for a while, which got in the way of clicking and scrolling). The
    // widget based lists do not want it at all.
}

void teTagListView::onTagEdited(std::shared_ptr<teTag>tag){
    // Editor controls may turn a tag into a copy of another one (a rename, a
    // colour swap, ...). Merging keeps the list free of duplicates; the old
    // code erased the tag twice here, which could take the index of a removed
    // row.
    if(showing_list&&tag&&showing_list->remove_duplicate(tag,true))
        telog("[teTagListView::onTagEdited] merged a duplicated tag");
}

// ---------------------------------------------------------------- the view

teTagListView::teTagListView(QWidget* parent)
    : QListView(parent)
{
    createModelAndDelegate();
    createMenu();
    plaintextedit = new teInputWidget;
    // Magnified popup: it appears when a tag is clicked, not after a hover delay,
    // and stays while the pointer travels into it.
    tagDisplay = new teTagDisplayWidget(this);
    tagDisplayHideTimer = new QTimer(this);
    tagDisplayHideTimer->setSingleShot(true);
    connect(tagDisplayHideTimer,&QTimer::timeout,this,&teTagListView::tagDisplayHideTick);
    connect(tagDisplay,&teTagDisplayWidget::pointerLeft,this,&teTagListView::scheduleTagDisplayHide);
}

teTagListView::~teTagListView()
{
    clear();
    delete plaintextedit;
    plaintextedit = nullptr;
}

void teTagListView::createMenu()
{
    menu = new QMenu(this);
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

void teTagListView::copy()
{
    QApplication::clipboard()->setText(getSelectText());
}

void teTagListView::cut()
{
    copy();
    eraseSelectedRows();
}

void teTagListView::paste()
{
    const QString text = QApplication::clipboard()->text();
    const auto pieces = splitTextToPieces(text);
    for(const auto& piece : pieces)
        tagInsertBelow(false,std::make_shared<teTag>(piece.text,nullptr,piece.sentence),2);
}

void teTagListView::onPlainTextEditStop(QString in_str)
{
    disconnect(plaintextedit,&teInputWidget::stringSignal,0,0);
    disconnect(plaintextedit,&teInputWidget::cancelSignal,0,0);
    plaintextedit->hide();
    std::shared_ptr<teTag> core = m_plainTextCore;
    m_plainTextCore.reset();
    if(core)
        tagEdit(core,in_str);
}

// -- the magnified popup -------------------------------------------------------

void teTagListView::showTagDisplayCore(std::shared_ptr<teTag> core)
{
    if(!core||!tagDisplay)
        return;
    tagDisplay->showForCore(core,tagDisplayAnchor(core));
}

void teTagListView::followTagDisplayCore(std::shared_ptr<teTag> core)
{
    if(!tagDisplay||!tagDisplay->isVisible()||!core)
        return;
    if(tagDisplayHideTimer)
        tagDisplayHideTimer->stop();
    showTagDisplayCore(core);
}

void teTagListView::hideTagDisplay()
{
    if(tagDisplayHideTimer)
        tagDisplayHideTimer->stop();
    if(tagDisplay)
        tagDisplay->hideDisplay();
}

bool teTagListView::handleTagDisplayEscape(QKeyEvent* event)
{
    if(event&&event->key()==Qt::Key_Escape&&tagDisplay&&tagDisplay->isVisible()){
        hideTagDisplay();
        return true;
    }
    return false;
}

void teTagListView::scheduleTagDisplayHide()
{
    if(!tagDisplay||!tagDisplay->isVisible()||!tagDisplayHideTimer)
        return;
    tagDisplayHideTimer->start(180);
}

void teTagListView::tagDisplayHideTick()
{
    const QPoint pos = QCursor::pos();
    if(tagDisplay&&tagDisplay->isVisible()&&tagDisplay->frameGeometry().contains(pos))
        return;                         // the pointer is inside the popup
    hideTagDisplay();
}

void teTagListView::createModelAndDelegate()
{
    m_model = new teTagListModel(this);
    m_delegate = new teTagDelegate(this);
    setModel(m_model);
    setItemDelegate(m_delegate);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setEditTriggers(QAbstractItemView::DoubleClicked|QAbstractItemView::EditKeyPressed);
    setUniformItemSizes(true);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setContextMenuPolicy(Qt::CustomContextMenu);
    setDragEnabled(true);
    setAcceptDrops(true);
    setDropIndicatorShown(true);
    setDragDropMode(QAbstractItemView::InternalMove);
    setDefaultDropAction(Qt::MoveAction);
    setFrameShape(QFrame::NoFrame);
    setMouseTracking(true);
    viewport()->setMouseTracking(true);
    viewport()->installEventFilter(this);

    connect(this,&QListView::clicked,this,&teTagListView::onViewClicked);
    connect(this,&QListView::doubleClicked,this,&teTagListView::onViewDoubleClicked);
    connect(this,&QListView::customContextMenuRequested,this,&teTagListView::onViewContextMenu);
    if(QItemSelectionModel* selection = selectionModel())
        connect(selection,&QItemSelectionModel::currentChanged,this,&teTagListView::onViewCurrentChanged);
    connect(m_model,&QAbstractItemModel::modelAboutToBeReset,this,[this]{
        m_currentCoreBeforeReset = currentCore();
    });
    connect(m_model,&QAbstractItemModel::modelReset,this,&teTagListView::onModelReset);
    // An empty row left behind by the inline editor disappears.
    connect(m_delegate,&QAbstractItemDelegate::closeEditor,this,[this](QWidget*,QAbstractItemDelegate::EndEditHint){
        finishRowEdit(this->currentIndex().row());
    });

    // The base class built the menu; here the actions have to work on rows.
    connect(editAction,&QAction::triggered,this,[this]{ tagEditCore(currentCore()); });
    connect(deleteAction,&QAction::triggered,this,&teTagListView::eraseSelectedRows);
    connect(insertAction,&QAction::triggered,this,[this]{ tagInsertAbove(true,nullptr); });
    connect(insertBelowAction,&QAction::triggered,this,[this]{ tagInsertBelow(true,nullptr); });
    connect(cutAction,&QAction::triggered,this,[this]{ cut(); });
    connect(copyAction,&QAction::triggered,this,[this]{ copy(); });
    connect(pasteAction,&QAction::triggered,this,[this]{ paste(); });
}

void teTagListView::dropEvent(QDropEvent* event)
{
    if(!dropRowsAt(event->position().toPoint())){
        event->ignore();
        return;
    }
    event->accept();
}

bool teTagListView::dropRowsAt(const QPoint& viewportPos)
{
    auto* tagModel = qobject_cast<teTagListModel*>(model());
    if(!tagModel)
        return false;
    QVector<int> rows;
    for(const QModelIndex& index:selectionModel()->selectedRows())
        rows.append(index.row());
    if(rows.isEmpty())
        return false;
    // Bias the position by half a row: dropping above a row's middle inserts
    // before it, below inserts after it. That is exactly the rule Qt uses to draw
    // its drop indicator, so the drop lands where the line is drawn - including
    // between two tags.
    auto* tagDelegate = qobject_cast<teTagDelegate*>(itemDelegate());
    const int rowHeight = tagDelegate?tagDelegate->rowHeight():30;
    const QPoint pos(viewportPos.x(),viewportPos.y()+rowHeight/2);
    const QModelIndex target = indexAt(pos);
    const int dropRow = target.isValid()?target.row():tagModel->rowCount();
    if(!tagModel->moveTags(rows,dropRow))
        return false;
    // Keep the moved rows selected.
    sortRows(rows);
    selectionModel()->clearSelection();
    int first = dropRow;
    for(int row:rows)
        if(row<dropRow)
            --first;
    first = std::clamp(first,0,tagModel->rowCount()-1);
    for(int i=0;i<int(rows.size())&&first+i<tagModel->rowCount();++i)
        selectionModel()->select(tagModel->index(first+i,0),
                                 QItemSelectionModel::Select|QItemSelectionModel::Rows);
    setCurrentIndex(tagModel->index(first,0));
    return true;
}

void teTagListView::sortRows(QVector<int>& rows)
{
    std::sort(rows.begin(),rows.end());
    rows.erase(std::unique(rows.begin(),rows.end()),rows.end());
}

void teTagListView::startDrag(Qt::DropActions supportedActions)
{
    // Dragging a tag is no time for the popup: it hides as soon as the drag
    // starts (the user asked for exactly that).
    if(auto* list = qobject_cast<teTagListView*>(parentWidget()))
        list->hideTagDisplay();
    // Deliberately not QListView::startDrag(): that one calls clearOrRemove()
    // when the drag ends with MoveAction, which removes the dragged rows a
    // second time - after dropEvent() already moved them into place. That is
    // what made rows vanish (and, with the model out of sync, crash) while
    // dragging. The multi tag list draws its own pixmap for the same reason.
    auto* tagModel = qobject_cast<teTagListModel*>(model());
    const QModelIndexList indexes = selectionModel()->selectedRows();
    if(!tagModel||indexes.isEmpty())
        return;

    QFont font(QStringLiteral("Segoe UI"),10);
    const QFontMetrics metrics(font);
    const QString text = indexes.first().data(Qt::DisplayRole).toString();
    const int width = std::max(40,metrics.horizontalAdvance(text)+16);
    const int height = metrics.height()+8;
    QPixmap pixmap(width,height);
    pixmap.fill(Qt::transparent);
    {
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setBrush(QColor(21,63,34));
        painter.setPen(QColor(76,230,56));
        painter.drawRect(QRect(1,1,width-2,height-2));
        painter.setFont(font);
        painter.setPen(Qt::white);
        painter.drawText(QRect(7,0,width-14,height),Qt::AlignLeft|Qt::AlignVCenter,text);
    }

    QMimeData* mimeData = tagModel->mimeData(indexes);
    if(!mimeData)
        return;
    QDrag* drag = new QDrag(this);
    drag->setMimeData(mimeData);
    drag->setPixmap(pixmap);
    drag->setHotSpot(QPoint(8,height/2));
    drag->exec(supportedActions,defaultDropAction());
}

void teTagListView::onViewClicked(const QModelIndex& index)
{
    // The popup opens on a click (and on the selection it makes), not on hover.
    if(!index.isValid()||!showing_list)
        return;
    const int row=index.row();
    if(row<0||row>=int(showing_list->size()))
        return;
    showTagDisplayCore(showing_list->shareAt(row));
}

void teTagListView::onViewDoubleClicked(const QModelIndex& index)
{
    if(index.isValid())
        startEditingRow(index.row());
}

void teTagListView::onViewCurrentChanged(const QModelIndex& current,const QModelIndex& previous)
{
    Q_UNUSED(previous);
    if(current.isValid())
        m_pendingSelectionIndex=current.row();
}

void teTagListView::onModelReset()
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

void teTagListView::eraseSelectedRows()
{
    if(!m_model)
        return;
    tagErase(nullptr);      // "erase the selection", shared with the editors' path
}
void teTagListView::finishRowEdit(int row)
{
    if(!m_model||row<0||row>=m_model->rowCount())
        return;
    teTag* raw = m_model->index(row,0).data(teTagListModel::TagCoreRole).value<teTag*>();
    if(raw&&static_cast<QString>(*raw).trimmed().isEmpty()){
        // An empty tag is not worth keeping: this is how the old inline editor
        // behaved for a freshly inserted tag.
        telog("[teTagListWidget] dropping an empty tag");
        m_model->eraseTag(row);
    }
}

void teTagListView::onViewContextMenu(const QPoint& pos)
{
    const QModelIndex index = this->indexAt(pos);
    if(index.isValid()&&!this->selectionModel()->isSelected(index))
        setSelectCurrentCore(showing_list&&index.row()<int(showing_list->size())
                                 ?showing_list->shareAt(index.row())
                                 :nullptr);
    menu->exec(this->viewport()->mapToGlobal(pos));
}

bool teTagListView::eventFilter(QObject* watched,QEvent* event)
{
    if(this&&watched==this->viewport()){
        switch(event->type()){
        case QEvent::MouseMove:{
            // The popup follows the pointer from row to row while it is up; it is
            // a click (see the view's clicked() signal) that opens it.
            if(!tagDisplay||!tagDisplay->isVisible())
                break;
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            const QModelIndex index = this->indexAt(mouseEvent->position().toPoint());
            if(!index.isValid()){
                hideTagDisplay();
                break;
            }
            teTag* raw = index.data(teTagListModel::TagCoreRole).value<teTag*>();
            if(!raw)
                break;
            for(const std::shared_ptr<teTag>& core:*showing_list)
                if(core.get()==raw){
                    followTagDisplayCore(core);
                    break;
                }
            break;
        }
        case QEvent::Leave:
            scheduleTagDisplayHide();
            break;
        case QEvent::MouseButtonPress:{
            // A click on empty space below the tags closes the popup.
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            if(!this->indexAt(mouseEvent->position().toPoint()).isValid())
                hideTagDisplay();
            break;
        }
        default:
            break;
        }
    }
    return QObject::eventFilter(watched,event);
}
