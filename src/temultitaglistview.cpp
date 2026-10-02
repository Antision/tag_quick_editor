#include "temultitaglistview.h"

namespace {

/// Moves every link of `from` over to `to` and rewires the tag callbacks.
void moveLinkedTags(teSelectionTag*from,teSelectionTag*to){
    if(from->linked_tags.empty())
        return;
    const auto links = from->linked_tags;
    for(const auto&[tc,list]:links){
        tc->teDisconnect(from);
        to->link(tc,list);
    }
    from->linked_tags.clear();
}

}

void teSelectionTagModel::linkTagList(teTagList *in_list){
    if(linked_taglists.contains(in_list))
        return;
    linked_taglists.push_back(in_list);
    connect(in_list,&teTagList::tagInserted,this,[this, in_list](std::shared_ptr<teTag>newcore){
        if(tagInsertSuppression>0)
            linkNewTagcore(newcore,in_list);
    });
    in_list->teConnect(teCallbackType::destroy,this,&teSelectionTagModel::unlinkTagList,in_list);
}

void teSelectionTagModel::unlinkTagList(teTagList *in_list){
    if(in_list){
        const int index = linked_taglists.indexOf(in_list);
        if(index>=0)
            linked_taglists.removeAt(index);
        in_list->teDisconnect(this);
        disconnect(in_list,0,this,0);
    }else{
        const auto lists = linked_taglists;
        for(teTagList*list:lists){
            disconnect(list,0,this,0);
            list->teDisconnect(this);
        }
        linked_taglists.clear();
    }
}

bool teSelectionTagModel::setData(const QModelIndex &index, const QVariant &value, int role) {
    if (!index.isValid() || role != Qt::EditRole)
        return false;
    if (!value.canConvert<QString>())
        return false;

    const int row = index.row();
    if (row < 0 || row >= tags.size())
        return false;

    teSelectionTag* tag = tags[row];
    if (!tag)
        return false;

    tag->setText(value.toString());
    return true;
}

QVariant teSelectionTagModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= tags.size())
        return QVariant();
    // Only the roles the view and the delegate actually consume get the tag
    // pointer; everything else stays empty. The row is looked up instead of
    // trusting internalPointer(): a stale index (kept by a delegate or by the
    // selection model across row removals) used to hand out a dangling pointer.
    switch (role) {
    case Qt::DisplayRole:
    case Qt::EditRole:
    case Qt::ToolTipRole:
        return QVariant::fromValue(tags[index.row()]);
    default:
        return QVariant();
    }
}

teSelectionTag* teSelectionTagModel::findMultiTag(const QString& in_text, teSelectionTag* exclude) const {
    for (teSelectionTag* mt : tags)
        if (mt != exclude && mt->text == in_text)
            return mt;
    return nullptr;
}

teSelectionTag *teSelectionTagModel::getMultiTag(const QString &in_text, bool &isNewEntry, int row, bool ifselect){
    // The list is *not* kept sorted: the user may reorder rows freely, so a
    // binary search would look at the wrong half of the list. It also used the
    // lower_bound result as an insertion row, which scattered new rows.
    if (teSelectionTag* existing = findMultiTag(in_text)) {
        isNewEntry = false;
        return existing;
    }
    isNewEntry = true;
    return tagInsert(row > -1 ? row : tags.size(), in_text, ifselect);
}

void teSelectionTagModel::linkNewTagcore(std::shared_ptr<teTag> in_core, teTagList *in_list){
    bool isNewEntry = false;
    teSelectionTag* find_mt = getMultiTag(*in_core, isNewEntry);
    find_mt->link(in_core,in_list);
}

teSelectionTag *teSelectionTagModel::tagInsert(int row, std::shared_ptr<teTag> in_tag, bool select){
    if(row<0)row+=tags.size()+1;
    row = std::clamp(row,0,int(tags.size()));
    teSelectionTag*newmultitag =new teSelectionTag(this);
    beginInsertRows(QModelIndex(), row, row);
    tags.insert(row,newmultitag);
    connectTag(newmultitag);
    endInsertRows();
    if(select&&listview)
        listview->selectionModel()->select(index(row,0), QItemSelectionModel::ClearAndSelect);
    if(in_tag==nullptr){
        if(listview)
            listview->scrollTo(index(row,0));
        newmultitag->teConnect(teCallbackType::edit,this,&teSelectionTagModel::newTagTypeFinished,newmultitag);
        if(listview)
            listview->edit(index(row,0));
    }else{
        newmultitag->setText(*in_tag);
    }
    return newmultitag;
}

teSelectionTag *teSelectionTagModel::tagInsert(int row, const QString &text, bool select){
    if(row<0)row+=tags.size()+1;
    row = std::clamp(row,0,int(tags.size()));
    teSelectionTag*newmultitag = new teSelectionTag(text,this);
    beginInsertRows(QModelIndex(), row, row);
    tags.insert(row,newmultitag);
    endInsertRows();
    connectTag(newmultitag);
    if(select&&listview){
        listview->selectionModel()->select(index(row,0), QItemSelectionModel::ClearAndSelect);
        listview->scrollTo(index(row,0));
    }
    return newmultitag;
}

void teSelectionTagModel::setPos(int index){
    if(index<0||index>=tags.size()||tags.size()<2)
        return;

    const double pos = double(index)/double(tags.size()-1);
    const auto links = tags[index]->linked_tags;
    for(const auto&[tag,taglist]:links){
        const int nowpos = taglist->find(tag);
        if(nowpos<0)
            continue;
        int newpos = int(taglist->size()*pos);
        if(nowpos<newpos)
            --newpos;
        newpos = std::clamp(newpos,0,int(taglist->size())-1);
        taglist->move(nowpos,newpos);
    }
}

QMimeData *teSelectionTagModel::mimeData(const QModelIndexList &indexes) const {
    if (indexes.isEmpty()) return nullptr;
    QMimeData *mimeData = new QMimeData;
    QByteArray encodedData;
    QDataStream stream(&encodedData, QIODevice::WriteOnly);
    for (const QModelIndex &index : indexes) {
        if (index.isValid())
            stream << index.row();
    }
    mimeData->setData("application/x-qabstractitemmodeldatalist", encodedData);

    QStringList texts;
    for (const QModelIndex &index : indexes)
        if (index.row()>=0&&index.row()<tags.size())
            texts.append(tags[index.row()]->text);
    mimeData->setText(texts.join(qsl(", ")));
    return mimeData;
}

bool teSelectionTagModel::moveRows(const QModelIndex &sourceParent, int sourceRow, int count,
                                  const QModelIndex &destinationParent, int destinationRow) {
    if (sourceParent.isValid() || destinationParent.isValid())
        return false;
    if (sourceRow < 0 || destinationRow < 0 || sourceRow >= tags.size() ||
        destinationRow > tags.size() || count <= 0 || sourceRow + count > tags.size()) {
        return false;
    }
    if (destinationRow >= sourceRow && destinationRow < sourceRow + count) {
        return false;
    }

    beginMoveRows(sourceParent, sourceRow, sourceRow + count - 1, destinationParent,
                  sourceRow<destinationRow?destinationRow+count:destinationRow);
    QList<teSelectionTag*> tempTags;
    for (int i = 0; i < count; ++i) {
        tempTags.append(tags.takeAt(sourceRow));
    }
    for (int i = 0; i < tempTags.size(); ++i) {
        tags.insert(destinationRow + i, tempTags[i]);
    }
    endMoveRows();
    return true;
}

bool teSelectionTagModel::moveTags(const QList<int>& rows, int destination){
    if (rows.isEmpty() || tags.isEmpty())
        return false;

    QList<int> sorted = rows;
    std::sort(sorted.begin(), sorted.end());
    sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());

    QList<teSelectionTag*> moved;
    moved.reserve(sorted.size());
    // Take the rows out from the back so the remaining indices stay valid.
    for (int i = sorted.size()-1; i >= 0; --i) {
        const int row = sorted[i];
        if (row < 0 || row >= tags.size())
            continue;
        beginRemoveRows(QModelIndex(), row, row);
        moved.prepend(tags.takeAt(row));
        endRemoveRows();
    }
    if (moved.isEmpty())
        return false;

    // `destination` is an index in the list *before* the removals; shift it by
    // the number of removed rows that sat above it.
    int insertAt = destination;
    for (int row : sorted)
        if (row < destination)
            --insertAt;
    insertAt = std::clamp(insertAt, 0, int(tags.size()));

    beginInsertRows(QModelIndex(), insertAt, insertAt + moved.size() - 1);
    for (int i = 0; i < moved.size(); ++i)
        tags.insert(insertAt + i, moved[i]);
    endInsertRows();
    return true;
}

void teSelectionTagModel::newTagTypeFinished(teSelectionTag *in_tag){
    teDisconnect(in_tag,teCallbackType::edit);
    const int row = tags.indexOf(in_tag);
    if (in_tag->text.isEmpty() || insertToTaglist(in_tag, tags.size()>1&&row>=0?(double)row/(tags.size()-1):0)==nullptr){
        // An empty or unusable entry is dropped again.
        if (row >= 0 && tags.indexOf(in_tag) == row)
            tagErase(row);
        else
            tagErase(in_tag);
        return;
    }
    if (row >= 0)
        emit dataChanged(index(row,0),index(row,0),{Qt::EditRole});
}

void teSelectionTagModel::loadFiles(QList<tePictureFile *> in_filelist, bool ifclear){
    if(ifclear)
        clear();
    for(tePictureFile*f:in_filelist){
        teTagList&taglist = f->taglist;
        linkTagList(&taglist);
        // Copy the tag list: linking may create multi tags, and a multi tag can
        // (indirectly) touch the same tag list.
        const QVector<std::shared_ptr<teTag>> tagcores = taglist.getTags();
        for(std::shared_ptr<teTag>tc:tagcores){
            bool isNewEntry = false;
            teSelectionTag*findCore = getMultiTag(*tc,isNewEntry);
            findCore->link(tc,&taglist);
        }
    }
    if(listview&&!tags.isEmpty())
        listview->scrollTo(index(0,0));
}

void teSelectionTagModel::eraseFiles(QList<tePictureFile *> in_filelist){
    for(tePictureFile*f:in_filelist){
        teTagList& tl = f->taglist;
        // unlink_tags_in_list() may destroy multi tags, which mutates `tags`;
        // iterate over a snapshot and re-check membership.
        const QList<teSelectionTag*> snapshot = tags;
        for(teSelectionTag* mt : snapshot){
            if(tags.contains(mt))
                mt->unlink_tags_in_list(&tl);
        }
        unlinkTagList(&tl);
    }
}

teSelectionTag* teSelectionTagModel::insertToTaglist(teSelectionTag *in_tag, double pos){
    if(linked_taglists.isEmpty())
        return nullptr;

    // 1) Merging: if another multi tag already carries this text, the edit must
    //    end up as a single row (and a single tag per image), not as a duplicate.
    teSelectionTag* target = in_tag;
    if (teSelectionTag* other = findMultiTag(in_tag->text, in_tag)) {
        moveLinkedTags(other, in_tag);
        tagErase(other);
        target = in_tag;
    }

    // 2) Make sure every linked list holds exactly one tag with this text. The
    //    flag keeps tagInserted from re-entering this function for the tags we
    //    create here (they are linked explicitly below).
    --tagInsertSuppression;
    const QList<teTagList*> lists = linked_taglists;
    for(teTagList* list : lists){
        std::shared_ptr<teTag> existing;
        for(const std::shared_ptr<teTag>& tc : *list){
            if(QString(*tc)==target->text){
                existing = tc;
                break;
            }
        }
        if(!existing){
            existing = std::make_shared<teTag>(target->text);
            const int index = std::clamp(int(list->size()*pos),0,int(list->size()));
            if(list->insert(index,existing,1)!=0)
                existing.reset();
        }
        if(existing)
            target->link(existing,list);
    }
    ++tagInsertSuppression;

    emit listModified();
    return target;
}

void teSelectionTagModel::tagDestroy(int index){
    if(index<0||index>=tags.size())
        return;
    tags[index]->self_destroy();
    removeRows(index,1);
}

void teSelectionTagModel::tagDestroy(){
    const QModelIndexList indexes = listview?listview->selectionModel()->selectedRows():QModelIndexList{};
    if (indexes.isEmpty())
        return;
    QList<int> rows;
    rows.reserve(indexes.size());
    for (const QModelIndex& index : indexes)
        if (index.isValid())
            rows.append(index.row());
    std::sort(rows.begin(), rows.end(), std::greater<int>());
    rows.erase(std::unique(rows.begin(), rows.end()), rows.end());
    for (int row : rows)
        tagDestroy(row);
}

void teSelectionTagModel::clear(){
    if(tags.isEmpty()){
        unlinkTagList();
        return;
    }
    beginResetModel();
    const QList<teSelectionTag*> old = tags;
    tags.clear();
    endResetModel();
    for(teSelectionTag* mt : old){
        mt->clear();
        delete mt;
    }
    unlinkTagList();
}

bool teSelectionTagModel::removeRows(int row, int count, const QModelIndex &parent) {
    if(row<0||count<=0||row+count>tags.size())
        return false;
    QList<teSelectionTag*> victims;
    beginRemoveRows(parent, row, row+count-1);
    for(int i=0;i<count;++i)
        victims.append(tags.takeAt(row));
    endRemoveRows();
    // Deleting a multi tag runs its `ready_destroy`/Qt signals, which may call
    // back into this model, so the deletion is deferred by one event loop pass.
    QTimer::singleShot(0,[victims]{
        for(teSelectionTag*mtc:victims)
            delete mtc;
    });
    return true;
}

void teSelectionTag::link(std::shared_ptr<teTag> in_core, teTagList *in_list){
    const auto range = linked_tags.equal_range(in_core);
    for(auto it = range.first;it!=range.second;++it)
        if(it->second==in_list)
            return;                         // already linked
    linked_tags.insert({in_core,in_list});
    in_core->teConnect(teCallbackType::destroy,this,&teSelectionTag::unlink,in_core);
    in_core->teConnect(teCallbackType::edit,this,&teSelectionTag::re_read,in_core,in_list);
    in_core->teConnect(teCallbackType::edit_with_layout,this,&teSelectionTag::re_read,in_core,in_list);
}

void teSelectionTag::unlink(std::shared_ptr<teTag> in_core){
    linked_tags.erase(in_core);
    in_core->teDisconnect(this);
    if(linked_tags.empty()&&ifexecute)
        teemit(ready_destroy);
}

void teSelectionTag::setText(const QString &in_text){
    setText(QString(in_text));
}

void teSelectionTag::setText(QString &&in_text){
    if(text==in_text){
        teemit(teCallbackType::edit);
        return;
    }

    // The edited row stays where it is; an existing row with the same text is
    // merged into it so the multi tag list never shows the same tag twice.
    if(model){
        if(teSelectionTag* other = model->findMultiTag(in_text, this)){
            moveLinkedTags(other,this);
            model->tagErase(other);
        }
    }

    text=std::move(in_text);
    if(!linked_tags.empty())
        setCoreText();
    teemit(teCallbackType::edit);
}

void teSelectionTag::setCoreText(){
    re_read_switch=false;
    const auto tmp_linked_tags = linked_tags;
    for(const auto&[tc,tl]:tmp_linked_tags){
        // A previous iteration may have merged this core into another tag (a
        // rename erases duplicates), so re-check that it is still there before
        // renaming it again.
        if(tl->find(tc)<0)
            continue;
        tl->edit(tc,text,true);
    }
    re_read_switch=true;
}

void teSelectionTag::unlink_tags_in_list(teTagList *in_list){
    QList<std::shared_ptr<teTag>> victims;
    for(const auto&[tc,tl]:linked_tags)
        if(tl==in_list)
            victims.append(tc);
    for(const std::shared_ptr<teTag>& tc : victims)
        unlink(tc);
}

void teSelectionTag::clear(){
    const auto links = linked_tags;
    for(const auto&[tc,tl]:links)
        tc->teDisconnect(this);
    linked_tags.clear();
    text.clear();
}

void teSelectionTag::self_destroy(){
    ifexecute=false;
    while(!linked_tags.empty())
        linked_tags.begin()->second->erase(linked_tags.begin()->first);
    ifexecute=true;
    text.clear();
}

void teSelectionTag::re_read(std::shared_ptr<teTag>in,teTagList*in_list){
    if(!re_read_switch)
        return;
    if((QString)*in!=text){
        if(model)
            model->linkNewTagcore(in,in_list);
        unlink(in);
    }
}

teTagListDelegate::teTagListDelegate(QWidget *parent):parent(parent){
    lineedit=new suggestionLineEdit(suggestionBox,parent);
    lineedit->hide();
    connect(lineedit, &QLineEdit::editingFinished, this, [this]{
        emit commitData(lineedit);
        lineedit->stop();
    });
    lineedit->installEventFilter(this);
}

bool teTagListDelegate::eventFilter(QObject *watched, QEvent *e) {
    suggestionLineEdit* lineeditp = dynamic_cast<suggestionLineEdit*>(watched);
    if (e->type() == QEvent::FocusOut) {
        auto focusEvent = static_cast<QFocusEvent*>(e);
        if (focusEvent->reason() == Qt::FocusReason::OtherFocusReason ||
            focusEvent->reason() == Qt::FocusReason::ActiveWindowFocusReason) {
            lineeditp->setFocus();
            return true;
        }
        emit lineeditp->editingFinished();
        return true;
    } else if (e->type() == QEvent::KeyPress) {
        auto keyEvent = static_cast<QKeyEvent*>(e);
        if (keyEvent->key() == Qt::Key_Tab) {
            if (suggestionBox && suggestionBox->currentItem())
                lineeditp->setText(suggestionBox->currentItem()->data(Qt::UserRole).toString());
            suggestionBox->hide();
            return true;
        }
    }
    return QStyledItemDelegate::eventFilter(watched, e);
}

QWidget *teTagListDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const {
    Q_UNUSED(option);
    showEditor=true;
    auto* tagcore = index.data(Qt::DisplayRole).value<teSelectionTag*>();
    if(tagcore)
        lineedit->start(*tagcore);
    return lineedit;
}

void teTagListDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const {
    auto *lineEdit = qobject_cast<suggestionLineEdit *>(editor);
    if (lineEdit) {
        showEditor=false;
        model->setData(index, lineEdit->text(), Qt::EditRole);
        emit const_cast<teTagListDelegate *>(this)->closeEditor(editor, QAbstractItemDelegate::NoHint);
    }
}

void teTagListDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const {
    auto* tag = index.data(Qt::DisplayRole).value<teSelectionTag*>();
    if(!tag)
        return;

    painter->save();
    QColor borderColor;
    QColor bgColor;
    if (option.state & QStyle::State_Selected) {
        if (option.state & QStyle::State_MouseOver) {
            bgColor = QColor(21, 63, 34);
            borderColor = QColor(76, 230, 56);
        } else {
            bgColor = QColor(16, 43, 24);
            borderColor = QColor(61, 194, 43);
        }
    } else {
        bgColor = QColor(0, 0, 0);
        if (option.state & QStyle::State_MouseOver) {
            borderColor = QColor(132, 207, 205);
        } else {
            borderColor = QColor(41, 122, 122);
        }
    }
    const QRect rect = option.rect;
    painter->setPen(Qt::NoPen);
    painter->setBrush(bgColor);
    painter->drawRect(rect);

    painter->setPen(QPen(borderColor, 1));
    painter->drawRect(rect.adjusted(1, 1, -1, -1));

    painter->setPen(option.state & QStyle::State_Selected
                        ? QColor(0, 217, 109) : QColor(178, 136, 255));
    if(!tag->text.isEmpty()){
        // The inline editor is a child widget drawn on top of this cell, so the
        // text is painted unconditionally. Skipping it while editing made the
        // row look empty whenever the editor was closed without committing.
        painter->setPen(Qt::white);
        painter->setFont(QFont("Segoe UI", 15));
        painter->drawText(rect.adjusted(3, 0, 0, 0), Qt::AlignVCenter | Qt::AlignLeft, tag->text);
        painter->setPen(QColor(150, 150, 150));
        painter->setFont(QFont("Segoe UI", 12));
        painter->drawText(rect.adjusted(0, 0, -5, 0), Qt::AlignVCenter | Qt::AlignRight, QString::number(tag->linked_tags.size()));
    }
    painter->restore();
}

teSelectionTagListView::teSelectionTagListView(QWidget *parent) : QListView(parent) {
    model = new teSelectionTagModel(this);
    model->listview=this;
    setModel(model);
    setItemDelegate(&delegate);
    setEditTriggers(QAbstractItemView::DoubleClicked);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setDragDropMode(QAbstractItemView::InternalMove);
    setDefaultDropAction(Qt::MoveAction);
    setDragDropOverwriteMode(false);
    setDragEnabled(true);
    setAcceptDrops(true);
    setDropIndicatorShown(true);
    initializeMenu();
    setContextMenuPolicy(Qt::CustomContextMenu);
    setStyleSheet(liststyle.arg(0));
    connect(this, &QWidget::customContextMenuRequested, this, &teSelectionTagListView::showContextMenu);
}

void teSelectionTagListView::initializeMenu(){
    editAction = menu->addAction(QIcon(":/res/menu_edit.png"),"edit (F2/Ctrl+E)");
    deleteAction = menu->addAction(QIcon(":/res/menu_remove.png"),"delete (Ctrl+D/del)");
    insertAction = menu->addAction(QIcon(":/res/menu_add.png"),"insert above (Ctrl+W)");
    insertBelowAction = menu->addAction(QIcon(":/res/menu_add.png"),"insert below");
    cutAction = menu->addAction(QIcon(":/res/menu_cut.png"),"cut (Ctrl+X)");
    copyAction = menu->addAction(QIcon(":/res/menu_copy.png"),"copy (Ctrl+C)");
    pasteAction = menu->addAction(QIcon(":/res/menu_paste.png"),"paste (Ctrl+V)");
    setposAction = menu->addAction(QIcon(":/res/menu_pin.png"),"setpos");
    menu->setStyleSheet(qsl(
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
)"));
    menu->setAttribute(Qt::WA_TranslucentBackground);
    menu->setWindowFlags(menu->windowFlags() | Qt::FramelessWindowHint );
}

void teSelectionTagListView::dropEvent(QDropEvent *event) {
    const bool isMoveAction = (event->dropAction() == Qt::MoveAction ||
                               dragDropMode() == QAbstractItemView::InternalMove);
    if (!isMoveAction) {
        QAbstractItemView::dropEvent(event);
        return;
    }
    QPoint pos{event->position().toPoint().x(),event->position().toPoint().y()+delegate.tagheight/2};
    const QModelIndex targetIndex = indexAt(pos);
    const int dropRow = targetIndex.isValid() ? targetIndex.row() : model->rowCount();
    const QModelIndexList selectedIndexes = selectionModel()->selectedRows();
    if (selectedIndexes.isEmpty()) {
        event->ignore();
        return;
    }

    QList<int> rows;
    rows.reserve(selectedIndexes.size());
    for (const QModelIndex &index : selectedIndexes)
        rows.append(index.row());
    // Let the model do the row bookkeeping; doing it here with an accumulated
    // offset used to move the wrong rows when several rows were dragged.
    if (model->moveTags(rows, dropRow)) {
        std::sort(rows.begin(), rows.end());
        int first = dropRow;
        for (int row : rows)
            if (row < dropRow)
                --first;
        first = std::clamp(first, 0, model->rowCount()-1);
        selectionModel()->clearSelection();
        for (int i = 0; i < rows.size() && first + i < model->rowCount(); ++i)
            selectionModel()->select(model->index(first + i, 0), QItemSelectionModel::Select);
        event->accept();
    } else {
        event->ignore();
    }
}

void teSelectionTagListView::startDrag(Qt::DropActions supportedActions) {
    const QModelIndexList indexes = selectedIndexes();
    if (indexes.isEmpty())
        return;
    QList<teSelectionTag*>& tags = model->tags;
    QFont font("Segoe UI", 10);
    QFontMetrics fontMetrics(font);
    const QModelIndex index = *indexes.begin();
    if (index.row() < 0 || index.row() >= tags.size())
        return;
    const QString tagString = *tags.at(index.row());
    const int width = fontMetrics.horizontalAdvance(tagString) + 4;
    const int pixmapWidth = width + 8;
    const int pixmapHeight = fontMetrics.height() + 4;
    const int overlap = 4;
    const int totalHeightWithOverlap = pixmapHeight + (indexes.size() - 1) * overlap;
    QPixmap pixmap(pixmapWidth, totalHeightWithOverlap);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    const QColor backgroundColor(21, 63, 34);
    const QColor borderColor(76, 230, 56);

    int currentYOffset = (indexes.size() - 1) * overlap;
    for (int i = indexes.size() - 1; i >= 0; --i) {
        const QRect rect(0, currentYOffset, pixmapWidth, pixmapHeight);
        painter.setBrush(backgroundColor);
        painter.setPen(Qt::NoPen);
        painter.drawRect(rect);
        painter.setPen(borderColor);
        painter.drawRect(rect.adjusted(1, 1, -1, -1));
        if (i == 0) break;
        currentYOffset -= overlap;
    }

    painter.setFont(font);
    painter.setPen(Qt::white);
    const QRect textRect(4, (indexes.size() - 1) * overlap + 2, pixmapWidth - 8, pixmapHeight - 4);
    painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, tagString);
    painter.end();

    QMimeData *mimeData = model->mimeData(indexes);
    if (!mimeData)
        return;

    QDrag *drag = new QDrag(this);
    drag->setMimeData(mimeData);
    drag->setPixmap(pixmap);
    drag->setHotSpot(textRect.center());
    drag->exec(supportedActions, defaultDropAction());
}

void teSelectionTagListView::copyToClipBoard(bool ifcut){
    const QModelIndexList selectedIndexes = selectionModel()->selectedRows();
    if(selectedIndexes.isEmpty())
        return;

    QStringList texts;
    for(const QModelIndex&index:selectedIndexes){
        // data() no longer depends on the selection, so the selection must not
        // be reset while gathering the texts (it used to be cleared in the loop).
        if(auto* tagcore = index.data(Qt::DisplayRole).value<teSelectionTag*>())
            texts.append(tagcore->text);
    }
    if(texts.isEmpty())
        return;
    QApplication::clipboard()->setText(texts.join(qsl(", ")));

    if(ifcut){
        QList<int> rows;
        rows.reserve(selectedIndexes.size());
        for(const QModelIndex&index:selectedIndexes)
            rows.append(index.row());
        std::sort(rows.begin(),rows.end(),std::greater<int>());
        selectionModel()->clearSelection();
        for(int row:rows)
            model->tagDestroy(row);
    }
}

void teSelectionTagListView::paste(const QModelIndex &index){
    int row = index.isValid()?index.row():model->rowCount();
    const int modelTagCount = model->tags.size();

    const QString clipboardText = QApplication::clipboard()->text();
    const QStringList strlst = clipboardText.split(QLatin1Char(','),Qt::SkipEmptyParts);

    for(int i = strlst.count()-1;i>-1;--i){
        const QString text = strlst[i].trimmed();
        if(text.isEmpty())
            continue;
        bool isNewEntry = false;
        teSelectionTag* themultitag = model->getMultiTag(text,isNewEntry,row,true);
        model->insertToTaglist(themultitag,modelTagCount>1?(double)row/(modelTagCount-1):0);
    }
}

void teSelectionTagListView::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_W && event->modifiers() == Qt::ControlModifier) {
        if(!selectionModel()->selectedRows().empty())
            model->tagInsert(selectionModel()->selectedRows()[0].row(),std::shared_ptr<teTag>(nullptr),true);
        else
            model->tagInsert(0,std::shared_ptr<teTag>(nullptr),true);
    } else if (event->key() == Qt::Key_D && event->modifiers() == Qt::ControlModifier) {
        model->tagDestroy();
    } else if (event->key() == Qt::Key_Delete) {
        model->tagDestroy();
    } else if (event->key() == Qt::Key_Enter||event->key() == Qt::Key_Return) {
        if(auto selections = selectionModel()->selectedRows();!selections.empty())
            edit(selections[0]);
    } else if (event->key() == Qt::Key_E && event->modifiers() == Qt::ControlModifier) {
        if(auto selections = selectionModel()->selectedRows();!selections.empty())
            edit(selections[0]);
    } else if (event->key() == Qt::Key_X && event->modifiers() == Qt::ControlModifier) {
        copyToClipBoard(true);
    } else if (event->key() == Qt::Key_C && event->modifiers() == Qt::ControlModifier) {
        copyToClipBoard(false);
    } else if (event->key() == Qt::Key_V && event->modifiers() == Qt::ControlModifier) {
        if(auto selections = selectionModel()->selectedRows();!selections.empty())
            paste(selections.back());
    } else {
        QWidget::keyPressEvent(event);
    }
}

void teSelectionTagListView::showContextMenu(const QPoint &pos) {
    const QModelIndex index = indexAt(pos);
    if(index.isValid()&&!selectionModel()->isSelected(index))
        selectionModel()->select(index,QItemSelectionModel::ClearAndSelect);

    QAction*selectedAction = menu->exec(mapToGlobal(pos));

    if (selectedAction == deleteAction) {
        model->tagDestroy();
    } else if (selectedAction == insertAction) {
        model->tagInsert(index.isValid()?index.row():0);
    } else if (selectedAction == insertBelowAction) {
        model->tagInsert(index.isValid()?index.row()+1:model->rowCount());
    } else if (selectedAction == setposAction) {
        auto indexes = selectionModel()->selectedIndexes();
        std::reverse(indexes.begin(),indexes.end());
        for(QModelIndex& idx:indexes)
            if(idx.isValid())
                model->setPos(idx.row());
    } else if (selectedAction == copyAction) {
        copyToClipBoard(false);
    } else if (selectedAction == cutAction) {
        copyToClipBoard(true);
    } else if (selectedAction == pasteAction) {
        paste(index);
    } else if (selectedAction == editAction){
        if(index.isValid())
            edit(index);
    }
}
