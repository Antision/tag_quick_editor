#include "temultitaglistview.h"


void moveLinkedTags(teMultiTagCore*from,teMultiTagCore*to){
    if(!from->linked_tags.empty()){
        to->linked_tags.insert(from->linked_tags.begin(),from->linked_tags.end());
        for(auto&[tc,list]:from->linked_tags){
            tc->teConnect(teCallbackType::destroy,to,&teMultiTagCore::unlink,tc);
            tc->teConnect(teCallbackType::edit,to,&teMultiTagCore::re_read,tc,list);
            tc->teConnect(teCallbackType::edit_with_layout,to,&teMultiTagCore::re_read,tc,list);
            tc->teDisconnect(from);
        }
        from->linked_tags.clear();
    }
}
void teMultiTagListModel::linkTagList(teTagList *in_list){
    linked_taglists.push_back(in_list);
    connect(in_list,&teTagList::tagInserted,this,[this, in_list](std::shared_ptr<tetagcore>newcore){
        if(ifrecivenewtagcoreinsertsignal>0)
            linkNewTagcore(newcore,in_list);
    });
    in_list->teConnect(teCallbackType::destroy,this,&teMultiTagListModel::unlinkTagList,in_list);
}

void teMultiTagListModel::unlinkTagList(teTagList *in_list){
    if(in_list){
        linked_taglists.removeAt(linked_taglists.indexOf(in_list));
        in_list->teDisconnect(this);
        disconnect(in_list,0,this,0);
    }else{
        for(teTagList*list:linked_taglists){
            disconnect(list,0,this,0);
            list->teDisconnect(this);
        }
        linked_taglists.clear();
    }
}

bool teMultiTagListModel::setData(const QModelIndex &index, const QVariant &value, int role) {
    if (!index.isValid() || role != Qt::EditRole) {
        return false;
    }
    if (!value.canConvert<QString>()) {
        return false;
    }
    int row = index.row();
    if (row < 0 || row >= tags.size()) {
        return false;
    }

    teMultiTagCore* tag = tags[row];
    if (!tag) {
        return false;
    }
    tag->setText(value.toString());
    emit dataChanged(index, index, {role});
    return true;
}

QVariant teMultiTagListModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= tags.size()) return QVariant();
    const auto tag = static_cast<teMultiTagCore*>(index.internalPointer());
    return QVariant::fromValue(tag); // For simplicity, display first word.
}

teMultiTagCore *teMultiTagListModel::getMultiTag(const QString &in_text, bool &ifnew, int row, bool ifselect){
    teMultiTagCore*find_mt=nullptr;
    QList<teMultiTagCore*>::Iterator it;
    it = std::lower_bound(tags.begin(), tags.end(), in_text, multitagTextCmp);
    if (it != tags.end()&&(*it)->text==in_text) {
        find_mt= *it;
    }else
        for (teMultiTagCore* mt : tags) {
            if (mt->text == in_text) {
                find_mt= mt;
                break;
            }
        }
    if(!find_mt){
        ifnew=true;
        find_mt=tagInsert((row>-1?row:(it-tags.begin())),in_text,ifselect);
    }else
        ifnew = false;
    return find_mt;
}

void teMultiTagListModel::linkNewTagcore(std::shared_ptr<tetagcore> in_core, teTagList *in_list, QItemSelectionModel *selectionModel){
    QString in_text = *in_core;
    bool ifnew;
    teMultiTagCore*find_mt=getMultiTag(in_text,ifnew);
    find_mt->link(in_core,in_list);
}

teMultiTagCore *teMultiTagListModel::tagInsert(int row, std::shared_ptr<tetagcore> in_tag, bool select){
    if(row<0)row+=tags.size()+1;
    teMultiTagCore*newmultitag =new teMultiTagCore(this);
    beginInsertRows(QModelIndex(), row, row);
    tags.insert(row,newmultitag);
    connectTag(newmultitag);
    endInsertRows();
    listview->selectionModel()->reset();
    if(select){
        listview->selectionModel()->select(index(row,0), QItemSelectionModel::Select);
    }
    if(in_tag==nullptr){
        listview->scrollTo(index(row,0));
        newmultitag->teConnect(teCallbackType::edit,this,&teMultiTagListModel::newTagTypeFinished,newmultitag,row);
        listview->edit(index(row,0));
    }else{
        newmultitag->setText(*in_tag);
    }
    return newmultitag;
}

teMultiTagCore *teMultiTagListModel::tagInsert(int row, const QString &text, bool select){
    teMultiTagCore*newmultitag = new teMultiTagCore(text,this);
    beginInsertRows(QModelIndex(), row, row);
    tags.insert(row,newmultitag);
    endInsertRows();
    connectTag(newmultitag);
    listview->scrollTo(index(row,0));
    if(select)
        listview->selectionModel()->select(index(row,0), QItemSelectionModel::Select);
    return newmultitag;
}

void teMultiTagListModel::setPos(int index){
    if(tags.empty())return;

    double pos = (double)index/(tags.size()-1);
    for(auto[tag,taglist]:tags[index]->linked_tags){
        int nowpos = taglist->find(tag);
        int newpos = pos*taglist->size();
        if(nowpos<newpos)
            --newpos;
        taglist->move(nowpos,newpos);
    }
}

QMimeData *teMultiTagListModel::mimeData(const QModelIndexList &indexes) const {
    if (indexes.isEmpty()) return nullptr;
    QMimeData *mimeData = new QMimeData;
    QByteArray encodedData;
    QDataStream stream(&encodedData, QIODevice::WriteOnly);
    for (const QModelIndex &index : indexes) {
        if (index.isValid()) {
            stream << index.row();
        }
    }
    mimeData->setData("application/x-qabstractitemmodeldatalist", encodedData);
    QString finalStr;
    for (const QModelIndex &index : indexes) {
        finalStr.append(tags[index.row()]->text);
        finalStr.append(qsl(", "));
    }
    finalStr.chop(2);
    mimeData->setText(finalStr);
    return mimeData;
}

bool teMultiTagListModel::moveRows(const QModelIndex &sourceParent, int sourceRow, int count, const QModelIndex &destinationParent, int destinationRow) {
    if (sourceRow < 0 || destinationRow < 0 || sourceRow >= tags.size() ||
        destinationRow > tags.size() || count <= 0 || sourceRow + count > tags.size()) {
        return false;
    }
    if (destinationRow >= sourceRow && destinationRow < sourceRow + count) {
        return false;
    }

    beginMoveRows(sourceParent, sourceRow, sourceRow + count - 1, destinationParent, sourceRow<destinationRow?destinationRow+count:destinationRow);
    QList<teMultiTagCore*> tempTags;
    for (int i = 0; i < count; ++i) {
        tempTags.append(tags.takeAt(sourceRow));
    }
    for (int i = 0; i < tempTags.size(); ++i) {
        tags.insert(destinationRow + i, tempTags[i]);
    }
    endMoveRows();
    return true;
}

void teMultiTagListModel::newTagTypeFinished(teMultiTagCore *in_tag, int row){
    in_tag->teDisconnect(this,teCallbackType::edit);
    if(in_tag->text.isEmpty()||insertToTaglist(in_tag,tags.size()>1?(double)row/(tags.size()-1):0)){
        tagErase(tags.indexOf(in_tag));
        return;
    }else
        emit dataChanged(index(row,0),index(row,0),{Qt::EditRole});
}

void teMultiTagListModel::loadFiles(QList<tePictureFile *> in_filelist, bool ifclear){
    if(ifclear)
        clear();
    for(tePictureFile*f:in_filelist){
        teTagList&taglist = f->taglist;
        linkTagList(&taglist);
        for(std::shared_ptr<tetagcore>tc:taglist){
            bool ifnew;
            teMultiTagCore*findCore = getMultiTag(*tc,ifnew);
            findCore->link(tc,&taglist);
        }
    }
    listview->scrollTo(index(0,0));
}

void teMultiTagListModel::eraseFiles(QList<tePictureFile *> in_filelist){
    std::map<QString,teMultiTagCore*>multitags;
    for(teMultiTagCore*mt:tags){
        multitags.insert({*mt,mt});
    }
    for(tePictureFile*f:in_filelist){
        teTagList&tl = f->taglist;
        for(std::shared_ptr<tetagcore>tc:tl){
            auto it = multitags.find(*tc);
            if(it!=multitags.end()){
                it->second->unlink(tc);
            }
        }
        unlinkTagList(&f->taglist);
    }
}

teMultiTagCore* teMultiTagListModel::insertToTaglist(teMultiTagCore *in_tag, double pos){
    --ifrecivenewtagcoreinsertsignal;
    int taglist_count_minus_one=linked_taglists.size()-1;
    if(taglist_count_minus_one==-1)return (teMultiTagCore *)(-1);
    int i=0;
    QString in_tag_text = in_tag->text;
    std::shared_ptr<tetagcore>origin=std::make_shared<tetagcore>(in_tag_text);
    teMultiTagCore* repeattag=nullptr;
    for(;i<taglist_count_minus_one;++i){
        std::shared_ptr<tetagcore>tmpcpy = std::make_shared<tetagcore>(*origin);
        if(repeattag){
            if(linked_taglists[i]->insert(linked_taglists[i]->size()*pos,tmpcpy,1)==0){
                repeattag->link(tmpcpy,linked_taglists[i]);
            }
        }
        else if(linked_taglists[i]->insert(linked_taglists[i]->size()*pos,tmpcpy,1)<0){
            for(teMultiTagCore*mt:tags){
                if((QString)*mt==in_tag_text&&mt!=in_tag){
                    repeattag=mt;break;
                }
            }
            if(!repeattag)
                telog("[teMultitagListWidget::insert_to_taglist]this tag is repeated but can't find the tag in multitaglist");
            else
                moveLinkedTags(in_tag,repeattag);
        }
        else
            in_tag->link(tmpcpy,linked_taglists[i]);
    }
    if(linked_taglists[i]->insert(linked_taglists[i]->size()*pos,origin,1)<0&&!repeattag){
        for(teMultiTagCore*mt:tags){
            if((QString)*mt==in_tag_text&&mt!=in_tag){
                repeattag=mt;
                break;
            }
        }
        if(!repeattag)telog("[teMultitagListWidget::insert_to_taglist]this tag is repeating but can't find the tag in multitaglist");
        else moveLinkedTags(in_tag,repeattag);
    }else{
        (repeattag?repeattag:in_tag)->link(origin,linked_taglists[i]);
    }
    ++ifrecivenewtagcoreinsertsignal;

    emit listModified();
    if(repeattag)return repeattag;
    else return nullptr;
}

void teMultiTagListModel::tagDestroy(){
    QList<QModelIndex> indexes = listview->selectionModel()->selectedIndexes();
    if (indexes.isEmpty())
        return;
    QList<int> rows;
    for (const QModelIndex& index : indexes) {
        if (index.isValid())
            rows.append(index.row());
    }
    std::sort(rows.begin(), rows.end(), std::greater<int>());

    listview->selectionModel()->clear();
    for (int row : rows) {
        beginRemoveRows(QModelIndex(), row, row);
        tagDestroy(row);
        endRemoveRows();
    }
}

void teMultiTagListModel::clear(){
    if(tags.empty())return;
    beginRemoveRows(QModelIndex(),0,tags.size()-1);
    for(teMultiTagCore*mt:tags){
        mt->clear();
        delete mt;
    }
    tags.clear();
    unlinkTagList();
    endRemoveRows();
}

bool teMultiTagListModel::removeRows(int row, int count, const QModelIndex &parent) {
    int d=row+count;
    if (row < 0 || d > tags.size())
        return false;
    QVector<teMultiTagCore*>deleteMultiTags;
    beginRemoveRows(parent, row, d - 1);
    for (int i = row; i < d; ++i){
        deleteMultiTags.append(tags.takeAt(i));
    }
    QTimer::singleShot(0,[deleteMultiTags]{
        for(teMultiTagCore*mtc:deleteMultiTags)
            delete mtc;
    });
    endRemoveRows();
    return true;
}

void teMultiTagCore::link(std::shared_ptr<tetagcore> in_core, teTagList *in_list){
    linked_tags.insert({in_core,in_list});
    in_core->teConnect(teCallbackType::destroy,this,&teMultiTagCore::unlink,in_core);
    in_core->teConnect(teCallbackType::edit,this,&teMultiTagCore::re_read,in_core,in_list);
    in_core->teConnect(teCallbackType::edit_with_layout,this,&teMultiTagCore::re_read,in_core,in_list);
}

void teMultiTagCore::unlink(std::shared_ptr<tetagcore> in_core){
    linked_tags.erase(in_core);
    in_core->teDisconnect(this);
    if(linked_tags.empty()&&ifexecute)
        teemit(ready_destroy);
}

void teMultiTagCore::setText(const QString &in_text){
    text=in_text;
    if(!linked_tags.empty())
        setCoreText();
    teemit(teCallbackType::edit);
}

void teMultiTagCore::setText(QString &&in_text){
    text=std::move(in_text);
    if(!linked_tags.empty())
        setCoreText();
    teemit(teCallbackType::edit);
}

void teMultiTagCore::setCoreText(){
    re_read_switch=false;
    auto tmp_linked_tags = linked_tags;
    for(auto&[tc,tl]:tmp_linked_tags)
        tl->edit(tc,text,true);
    re_read_switch=true;
}

void teMultiTagCore::unlink_tags_in_list(teTagList *in_list){
    auto it = linked_tags.begin();
    for(;it!=linked_tags.end();){
        if(it->second!=in_list)
            ++it;
        else{
            unlink(it->first);
            it=linked_tags.erase(it);
        }
    }
}

void teMultiTagCore::link_tags_in_list(teTagList *in_list){
    for(std::shared_ptr<tetagcore>tc:*in_list)
        if((QString)*tc==text)
            link(tc,in_list);
}

void teMultiTagCore::clear(){
    for(auto&[tc,tl]:linked_tags)
        tc->teDisconnect(this);
    linked_tags.clear();
    text.clear();
}

void teMultiTagCore::self_destroy(){
    ifexecute=false;
    while(!linked_tags.empty())
        linked_tags.begin()->second->erase(linked_tags.begin()->first);
    ifexecute=true;
    text.clear();
}

void teMultiTagCore::re_read(std::shared_ptr<tetagcore>in,teTagList*in_list){
    if(!re_read_switch)return;
    if((QString)*in!=text){
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
    lineedit->installEventFilter(const_cast<teTagListDelegate*>(this));
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
            if (suggestionBox && suggestionBox->currentItem()) {
                lineeditp->setText(suggestionBox->currentItem()->data(Qt::UserRole).toString());
            }
            suggestionBox->hide();
            return true;
        }
    }
    return QStyledItemDelegate::eventFilter(watched, e);
}

QWidget *teTagListDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const {
    editing_row = index.row();
    showEditor=true;
    teMultiTagCore*tagcore = qvariant_cast<teMultiTagCore*>(index.data(Qt::EditRole));
    lineedit->start(*tagcore);
    return lineedit;
}

void teTagListDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const {
    suggestionLineEdit *lineEdit = qobject_cast<suggestionLineEdit *>(editor);
    if (lineEdit) {
        editing_row = -1;
        showEditor=false;
        model->setData(index, ((suggestionLineEdit*)editor)->text(), Qt::EditRole);
        emit const_cast<teTagListDelegate *>(this)->closeEditor(editor, QAbstractItemDelegate::NoHint);
    }
}

void teTagListDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const {
    painter->save();
    const auto tag = static_cast<teMultiTagCore*>(index.internalPointer());
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
    QRect rect = option.rect;
    painter->setPen(Qt::NoPen);
    painter->setBrush(bgColor);
    painter->drawRect(rect);

    painter->setPen(QPen(borderColor, 1));
    painter->drawRect(rect.adjusted(1, 1, -1, -1));

    painter->setPen(option.state & QStyle::State_Selected
                        ? QColor(0, 217, 109) : QColor(178, 136, 255));
    if(!tag->text.isEmpty()&& index.row()!=editing_row){
        painter->setPen(Qt::white);
        painter->setFont(QFont("Segoe UI", 15));
        painter->drawText(rect.adjusted(3, 0, 0, 0), Qt::AlignVCenter | Qt::AlignLeft, tag->text);
        painter->setPen(QColor(150, 150, 150));
        painter->setFont(QFont("Segoe UI", 12));
        painter->drawText(rect.adjusted(0, 0, -5, 0), Qt::AlignVCenter | Qt::AlignRight, QString::number(tag->linked_tags.size()));
    }
    painter->restore();
}

teMultitagListView::teMultitagListView(QWidget *parent) {
    model = new teMultiTagListModel(this);
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
    // Context menu for edit, delete, and more
    initializeMenu();
    setContextMenuPolicy(Qt::CustomContextMenu);
    setStyleSheet(liststyle.arg(0));
    connect(this, &QWidget::customContextMenuRequested, this, &teMultitagListView::showContextMenu);
}

void teMultitagListView::initializeMenu(){
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

void teMultitagListView::dropEvent(QDropEvent *event) {
    const bool isMoveAction = (event->dropAction() == Qt::MoveAction ||
                               dragDropMode() == QAbstractItemView::InternalMove);
    if (!isMoveAction) {
        QAbstractItemView::dropEvent(event);
        return;
    }
    QPoint Pos{event->position().toPoint().x(),event->position().toPoint().y()+delegate.tagheight/2};
    QModelIndex targetIndex = indexAt(Pos);
    int dropRow = targetIndex.isValid() ? targetIndex.row() : model->rowCount();
    QModelIndexList selectedIndexes = selectionModel()->selectedIndexes();
    if (selectedIndexes.isEmpty()) {
        event->ignore();
        return;
    }
    static auto cmpfunc = [](const QModelIndex&a,const QModelIndex&b)->bool{
        return a.row()<b.row();
    };
    std::sort(selectedIndexes.begin(), selectedIndexes.end(),cmpfunc);
    int offset = 0;
    for (const QModelIndex &index : selectedIndexes) {
        int sourceRow = index.row()<dropRow?index.row() + offset:index.row();
        model->moveRow(QModelIndex(), sourceRow, QModelIndex(),sourceRow < dropRow?dropRow-1: dropRow);
        if (sourceRow < dropRow) {
            --offset;
        }
    }
    event->accept();
}

void teMultitagListView::startDrag(Qt::DropActions supportedActions) {
    QModelIndexList indexes = selectedIndexes();
    if (indexes.isEmpty())
        return;
    QList<teMultiTagCore*>& tags = model->tags;
    QFont font("Segoe UI", 10);
    QFontMetrics fontMetrics(font);
    QString tagString;
    int maxWidth = 0;
    int totalHeight = 0;

    QModelIndex index = *indexes.begin();
    tagString = *(tags.at(index.row()));
    int width = fontMetrics.horizontalAdvance(tagString) + 4;
    maxWidth = qMax(maxWidth, width);
    totalHeight += fontMetrics.height() + 4;
    int pixmapWidth = maxWidth + 8;
    int pixmapHeight = fontMetrics.height() + 4;
    const int overlap = 4;
    int totalHeightWithOverlap = totalHeight + (indexes.size() - 1) * overlap;
    QPixmap pixmap(pixmapWidth, totalHeightWithOverlap);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    QColor backgroundColor(21, 63, 34);
    QColor borderColor(76, 230, 56);

    int currentYOffset = (indexes.size() - 1) * overlap;
    for (int i = indexes.size() - 1; i >= 0; --i) {
        QRect rect(0, currentYOffset, pixmapWidth, pixmapHeight);
        painter.setBrush(backgroundColor);
        painter.setPen(Qt::NoPen);
        painter.drawRect(rect);

        painter.setPen(borderColor);
        painter.drawRect(rect.adjusted(1, 1, -1, -1));

        if (i == 0) break;
        currentYOffset -= overlap;
    }

    currentYOffset = (indexes.size() - 1) * overlap;
    painter.setFont(font);
    painter.setPen(Qt::white);
    QRect rect(4, 2, pixmapWidth - 8, pixmapHeight - 4);
    painter.drawText(rect, Qt::AlignLeft | Qt::AlignVCenter, tagString);
    currentYOffset += pixmapHeight;

    painter.end();

    QMimeData *mimeData = model->mimeData(indexes);
    if (!mimeData)
        return;

    QDrag *drag = new QDrag(this);
    drag->setMimeData(mimeData);
    drag->setPixmap(pixmap);
    drag->setHotSpot(rect.center());
    Qt::DropAction action = drag->exec(supportedActions, defaultDropAction());
    if (action == Qt::MoveAction) {
        return;
    }
}

void teMultitagListView::copyToClipBoard(bool ifcut){
    QString clipBoardText;
    QModelIndexList selectedIndexes = selectionModel()->selectedIndexes();

    for(const QModelIndex&index:selectedIndexes){
        auto* tagcore = qvariant_cast<teMultiTagCore*>(model->data(index,Qt::DisplayRole));
        clipBoardText.append(tagcore->text);
        clipBoardText.append(", ");
        selectionModel()->reset();
    }
    if(clipBoardText.isEmpty()) return;
    clipBoardText.chop(2);
    QApplication::clipboard()->setText(clipBoardText);

    if(ifcut){
        static auto cmpfunc = [](const QModelIndex&a,const QModelIndex&b)->bool{
            return a.row()>b.row();
        };
        std::sort(selectedIndexes.begin(), selectedIndexes.end(),cmpfunc);
        for(QModelIndex& index:selectedIndexes)
            model->tagDestroy(index.row());
    }
}

void teMultitagListView::paste(const QModelIndex &index){
    selectionModel()->clear();
    int row = index.row();
    int modelTagCount = model->tags.size();
    bool ifnew;

    QString clipboardText = QApplication::clipboard()->text();
    QStringList strlst = clipboardText.split(",");

    for(int i =strlst.count()-1;i>-1;--i){
        QString& str = strlst[i];
        teMultiTagCore* themultitag = model->getMultiTag(str.trimmed(),ifnew,row,true);
        if(ifnew)
            model->insertToTaglist(themultitag,modelTagCount>1?(double)row/(modelTagCount-1):0);
    }
}

void teMultitagListView::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_W && event->modifiers() == Qt::ControlModifier) {
        model->tagInsert(selectionModel()->selectedRows()[0].row(),std::shared_ptr<tetagcore>(nullptr),true);
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

void teMultitagListView::showContextMenu(const QPoint &pos) {
    QAction*selectedAction = menu->exec(mapToGlobal(pos));
    QModelIndex index = indexAt(pos);

    if (selectedAction == deleteAction) {
        model->tagDestroy();
    } else if (selectedAction == insertAction) {
        model->tagInsert(index.row());
    } else if (selectedAction == insertBelowAction) {
        model->tagInsert(index.row()+1);
    } else if (selectedAction == setposAction) {
        auto indexes = selectionModel()->selectedIndexes();
        std::reverse(indexes.begin(),indexes.end());
        for(QModelIndex& idx:indexes)
            model->setPos(idx.row());
    } else if (selectedAction == copyAction) {
        copyToClipBoard(false);
    } else if (selectedAction == cutAction) {
        copyToClipBoard(true);
    } else if (selectedAction == pasteAction) {
        paste(index);
    } else if (selectedAction == editAction){
        edit(index);
    }
}
