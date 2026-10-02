#include "tetaglistmodel.h"

namespace {

/// Colours of one tag widget state, taken from the sheets teTagBase::setStyle()
/// applies to the widget based list (teTagBase::normal / select / select_current).
struct TagColors{
    QColor border;
    QColor background;
    QColor hoverBorder;
};

TagColors colorsFor(bool selected,bool current,bool hovered)
{
    TagColors colors;
    if(current){
        colors.border      = QColor(0x10,0x94,0x52);
        colors.hoverBorder = QColor(0x00,0xd9,0x6d);
        colors.background  = QColor(0x1a,0x57,0x59);
    }else if(selected){
        colors.border      = QColor(0x3e,0x5e,0x4b);
        colors.hoverBorder = QColor(0x2a,0x6b,0x45);
        colors.background  = QColor(0x17,0x3d,0x3e);
    }else{
        colors.border      = QColor(0x43,0x36,0x7b);
        colors.hoverBorder = QColor(0xb2,0x88,0xff);
        colors.background  = QColor(0x00,0x00,0x00);
    }
    Q_UNUSED(hovered);
    return colors;
}

}

// ---------------------------------------------------------------- teTagListModel

teTagListModel::teTagListModel(QObject* parent):QAbstractListModel(parent)
{
}

teTagListModel::~teTagListModel()
{
    disconnectList();
}

void teTagListModel::connectList(teTagList* list)
{
    if(!list)
        return;
    connect(list,&teTagList::tagInserted,this,&teTagListModel::onListInserted);
    connect(list,&teTagList::tagErased,this,&teTagListModel::onListErased);
    connect(list,&teTagList::tagEdited,this,&teTagListModel::onListEdited);
    connect(list,&teTagList::tagMoved,this,[this]{
        // Ignore our own moves: the signal arrives between beginMoveRows() and
        // endMoveRows(), and announcing a layout change there corrupts the
        // view's bookkeeping.
        if(m_selfMutation)
            return;
        notifyExternalReorder();
    });
}

void teTagListModel::disconnectList()
{
    if(!m_list)
        return;
    disconnect(m_list,0,this,0);
    m_list=nullptr;
}

void teTagListModel::setTagList(teTagList* list)
{
    if(m_list==list)
        return;
    beginResetModel();
    disconnectList();
    m_list=list;
    connectList(m_list);
    endResetModel();
}

int teTagListModel::rowCount(const QModelIndex& parent) const
{
    if(parent.isValid()||!m_list)
        return 0;
    return int(m_list->size());
}

QVariant teTagListModel::data(const QModelIndex& index,int role) const
{
    if(!m_list||!index.isValid()||index.row()<0||index.row()>=int(m_list->size()))
        return QVariant();
    // The row is looked up instead of trusting a stored pointer: a stale index
    // (kept by a delegate or by the selection model) must not hand out a
    // dangling tag.
    teTag* tag = m_list->at(index.row());
    if(!tag)
        return QVariant();
    switch(role){
    case Qt::DisplayRole:
    case Qt::EditRole:
    case Qt::ToolTipRole:
    case Qt::AccessibleTextRole:
        return QVariant(static_cast<QString>(*tag));
    case TagCoreRole:
        return QVariant::fromValue(tag);
    default:
        return QVariant();
    }
}

bool teTagListModel::setData(const QModelIndex& index,const QVariant& value,int role)
{
    if(role!=Qt::EditRole||!value.canConvert<QString>())
        return false;
    return setTagText(index.row(),value.toString());
}

Qt::ItemFlags teTagListModel::flags(const QModelIndex& index) const
{
    if(!index.isValid())
        return Qt::NoItemFlags;
    return Qt::ItemIsSelectable|Qt::ItemIsEnabled|Qt::ItemIsEditable
           |Qt::ItemIsDragEnabled|Qt::ItemIsDropEnabled;
}

teTag* teTagListModel::tagAt(int row) const
{
    if(!m_list||row<0||row>=int(m_list->size()))
        return nullptr;
    return m_list->at(row);
}

int teTagListModel::rowOf(teTag* tag) const
{
    if(!m_list||!tag)
        return -1;
    for(int i=0;i<int(m_list->size());++i)
        if(m_list->at(i)==tag)
            return i;
    return -1;
}

int teTagListModel::insertTag(int row,std::shared_ptr<teTag> tag,int removeDuplicate)
{
    if(!m_list||!tag)
        return -1;
    if(tag->type==teTag::deleteTag)
        return -1;
    // Refuse before announcing anything: the old widget based code announced the
    // insertion first and then had to take it back.
    if(removeDuplicate==1&&m_list->remove_duplicate(tag,false))
        return -1;
    if(removeDuplicate==2){
        // Merging erases the tags already there; those erases go through the
        // signals, so they are announced properly and this stays a plain insert.
        m_list->remove_duplicate(tag,true);
    }
    row = std::clamp(row,0,int(m_list->size()));
    beginInsertRows(QModelIndex(),row,row);
    m_selfMutation=true;
    const int result = m_list->insert(row,tag,0,true);      // 0: dedupe already done
    m_selfMutation=false;
    endInsertRows();
    return result;
}

bool teTagListModel::eraseTag(int row)
{
    if(!m_list||row<0||row>=int(m_list->size()))
        return false;
    beginRemoveRows(QModelIndex(),row,row);
    m_selfMutation=true;
    m_list->erase(row);
    m_selfMutation=false;
    endRemoveRows();
    return true;
}

bool teTagListModel::eraseRows(int row,int count)
{
    if(count<=0)
        return false;
    // From the back, so the remaining indices stay valid.
    for(int i=row+count-1;i>=row;--i)
        if(!eraseTag(i))
            return false;
    return true;
}

bool teTagListModel::removeRows(int row,int count,const QModelIndex& parent)
{
    if(parent.isValid()||!m_list||count<=0)
        return false;
    if(row<0||row+count>int(m_list->size()))
        return false;
    return eraseRows(row,count);
}

bool teTagListModel::moveTag(int from,int to)
{
    if(!m_list)
        return false;
    const int size = int(m_list->size());
    if(from<0||from>=size||to<0||to>=size||from==to)
        return false;
    // `to` is the position the row ends up at; Qt wants the index *before which*
    // the row is inserted, expressed in the list as it is before the move.
    return moveRows(QModelIndex(),from,1,QModelIndex(),to>from?to+1:to);
}

bool teTagListModel::moveRows(const QModelIndex& sourceParent,int sourceRow,int count,
                              const QModelIndex& destinationParent,int destinationRow)
{
    if(sourceParent.isValid()||destinationParent.isValid()||!m_list||count<=0)
        return false;
    const int size = int(m_list->size());
    if(sourceRow<0||destinationRow<0||sourceRow+count>size||destinationRow>size)
        return false;
    // Dropping a block onto itself (or right after itself) changes nothing.
    if(destinationRow>=sourceRow&&destinationRow<=sourceRow+count)
        return false;

    // Insertion index in the list the block has been taken out of.
    const int insertAt = destinationRow>sourceRow?destinationRow-count:destinationRow;
    if(!beginMoveRows(sourceParent,sourceRow,sourceRow+count-1,destinationParent,destinationRow))
        return false;
    m_selfMutation=true;
    m_list->reorderBlock(sourceRow,count,insertAt);
    m_selfMutation=false;
    endMoveRows();
    return true;
}

bool teTagListModel::setTagText(int row,const QString& text)
{
    if(!m_list||row<0||row>=int(m_list->size()))
        return false;
    teTag* tag = m_list->at(row);
    if(!tag)
        return false;
    if(static_cast<QString>(*tag)==text)
        return true;                // nothing to do, and nothing to record for undo
    // teTagList::edit() emits tagEdited (turned into a dataChanged by
    // onListEdited) and, when the rename merged into an existing tag, tagErased
    // (turned into a model reset). Both are handled by the slots, so there is
    // deliberately nothing to announce here.
    m_list->edit(row,text,1,true);
    return true;
}

bool teTagListModel::moveTags(const QVector<int>& rows,int destination)
{
    if(!m_list||rows.isEmpty())
        return false;
    QVector<int> valid;
    valid.reserve(rows.size());
    for(int row:rows)
        if(row>=0&&row<int(m_list->size()))
            valid.append(row);
    if(valid.isEmpty())
        return false;
    std::sort(valid.begin(),valid.end());
    valid.erase(std::unique(valid.begin(),valid.end()),valid.end());

    // Our own announcements: the tagMoved() signals below must not be mistaken
    // for somebody changing the list behind the model's back.
    const bool wasSelfMutation=m_selfMutation;
    m_selfMutation=true;

    // Take the rows out from the back, so the remaining indices stay valid.
    QVector<std::shared_ptr<teTag>> moved;
    moved.reserve(valid.size());
    for(int i=int(valid.size())-1;i>=0;--i){
        const int row=valid[i];
        beginRemoveRows(QModelIndex(),row,row);
        moved.prepend(m_list->takeTagOut(row));
        endRemoveRows();
    }
    // `destination` was given in the list as it was before the removals.
    int insertAt=destination;
    for(int row:valid)
        if(row<destination)
            --insertAt;
    insertAt=std::clamp(insertAt,0,int(m_list->size()));

    beginInsertRows(QModelIndex(),insertAt,insertAt+int(moved.size())-1);
    for(int i=0;i<int(moved.size());++i)
        m_list->insertTagIn(insertAt+i,moved[i]);
    endInsertRows();

    // One undo record per moved tag, like the widget based drop used to get from
    // teTagList::move().
    for(int i=0;i<int(moved.size());++i)
        m_list->onTagMoved(moved[i],valid[i],insertAt+i);

    m_selfMutation=wasSelfMutation;
    return true;
}

QStringList teTagListModel::mimeTypes() const
{
    return {QStringLiteral("application/x-taglistrow")};
}

QMimeData* teTagListModel::mimeData(const QModelIndexList& indexes) const
{
    QByteArray rows;
    for(const QModelIndex& index:indexes){
        if(!index.isValid()||index.column()!=0)
            continue;
        rows += QByteArray::number(index.row());
        rows += ';';
    }
    if(rows.isEmpty())
        return nullptr;
    auto* data = new QMimeData;
    data->setData(mimeTypes().first(),rows);
    data->setText(QString::fromUtf8(rows));
    return data;
}

void teTagListModel::notifyExternalReorder(){
    if(!m_list)
        return;
    // No signal tells us *how* the order changed, so announce a layout change;
    // it keeps the persistent indexes valid, unlike a reset.
    emit layoutAboutToBeChanged();
    emit layoutChanged();
}

void teTagListModel::onListInserted(std::shared_ptr<teTag> tag)
{
    Q_UNUSED(tag);
    if(m_selfMutation)
        return;
    // Somebody inserted behind our back (editing a multi selection inserts the
    // tag into every selected image). The row already exists, so a proper
    // beginInsertRows() is not possible any more; a reset is the honest answer
    // and the view restores its selection by core afterwards.
    beginResetModel();
    endResetModel();
}

void teTagListModel::onListErased(std::shared_ptr<teTag> tag)
{
    Q_UNUSED(tag);
    if(m_selfMutation)
        return;
    beginResetModel();
    endResetModel();
}

void teTagListModel::onListEdited(std::shared_ptr<teTag> tag)
{
    if(m_selfMutation)
        return;
    const int row = rowOf(tag.get());
    if(row<0)
        return;
    const QModelIndex idx = index(row,0);
    emit dataChanged(idx,idx,{Qt::DisplayRole,Qt::EditRole});
}

// ------------------------------------------------------------------- teTagDelegate

teTagDelegate::teTagDelegate(QWidget* parent)
{
    m_suggestionBox=new QListWidget;
    m_suggestionBox->setWindowFlags(Qt::FramelessWindowHint|Qt::Tool);
    m_suggestionBox->hide();
    // No parent on purpose: the view reparents the editor into its viewport when
    // it is first handed out (createEditor()), and the QPointers above make the
    // ownership transfer safe whichever of the two dies first.
    m_editor=new suggestionLineEdit(m_suggestionBox,nullptr);
    m_editor->hide();
    connect(m_editor,&QLineEdit::editingFinished,this,[this]{
        emit commitData(m_editor);
        if(m_editor)
            m_editor->stop();
    });
    m_editor->installEventFilter(this);
}

teTagDelegate::~teTagDelegate()
{
    // Deleting the editor deletes the suggestion box with it.
    if(m_editor)
        delete m_editor.data();
    else if(m_suggestionBox)
        delete m_suggestionBox.data();
}

QSize teTagDelegate::sizeHint(const QStyleOptionViewItem& option,const QModelIndex& index) const
{
    Q_UNUSED(option);
    if(!index.isValid())
        return QSize(200,m_rowHeight);
    return QSize(200,m_rowHeight);
}

void teTagDelegate::paint(QPainter* painter,const QStyleOptionViewItem& option,const QModelIndex& index) const
{
    if(!index.isValid())
        return;
    const QString text = index.data(Qt::DisplayRole).toString();
    if(text.isEmpty())
        return;

    const bool selected = option.state&QStyle::State_Selected;
    const bool hovered  = option.state&QStyle::State_MouseOver;
    const bool current  = selected&&(option.state&QStyle::State_HasFocus);
    const TagColors colors = colorsFor(selected,current,hovered);

    painter->save();
    const QRect rect = option.rect;
    painter->setPen(Qt::NoPen);
    painter->setBrush(colors.background);
    painter->drawRect(rect);
    painter->setPen(QPen(hovered?colors.hoverBorder:colors.border,1));
    painter->drawRect(rect.adjusted(1,1,-1,-1));

    // The text is hidden only while the inline editor is really on screen for
    // this row. Clearing m_editingRow when the editor closes is not enough on its
    // own: cancelling with Escape used to leave the row blank until the next row
    // was edited (and it stayed blank even after switching images).
    if(!(m_activeEditor&&m_activeEditor->isVisible()&&m_editingRow==index.row())){
        painter->setPen(selected?QColor(0x00,0xd9,0x6d):Qt::white);
        QFont font(QStringLiteral("Segoe UI"),15);
        painter->setFont(font);
        painter->drawText(rect.adjusted(4,0,-4,0),Qt::AlignVCenter|Qt::AlignLeft,text);
    }
    painter->restore();
}

QWidget* teTagDelegate::createEditor(QWidget* parent,const QStyleOptionViewItem& option,const QModelIndex& index) const
{
    Q_UNUSED(option);
    if(!index.isValid()||!m_editor)
        return nullptr;
    // The view hands its viewport in and expects the editor to live there: the
    // geometry it applies later is in viewport coordinates.
    if(parent&&m_editor->parent()!=parent)
        m_editor->setParent(parent);
    m_editingRow=index.row();
    m_activeEditor=m_editor;
    m_editor->start(index.data(Qt::EditRole).toString());
    return m_editor;
}

void teTagDelegate::setEditorData(QWidget* editor,const QModelIndex& index) const
{
    auto* lineEdit = qobject_cast<QLineEdit*>(editor);
    if(!lineEdit)
        return;
    lineEdit->setText(index.data(Qt::EditRole).toString());
}

void teTagDelegate::setModelData(QWidget* editor,QAbstractItemModel* model,const QModelIndex& index) const
{
    auto* lineEdit = qobject_cast<QLineEdit*>(editor);
    if(!lineEdit||!model)
        return;
    m_editingRow=-1;
    model->setData(index,lineEdit->text(),Qt::EditRole);
    emit const_cast<teTagDelegate*>(this)->closeEditor(editor,QAbstractItemDelegate::NoHint);
}

void teTagDelegate::updateEditorGeometry(QWidget* editor,const QStyleOptionViewItem& option,const QModelIndex& index) const
{
    Q_UNUSED(index);
    if(!editor)
        return;
    editor->setGeometry(option.rect.adjusted(3,2,-2,-2));
    if(auto* lineEdit = qobject_cast<suggestionLineEdit*>(editor))
        lineEdit->moveSuggestionBox();
}

bool teTagDelegate::eventFilter(QObject* watched,QEvent* event)
{
    auto* lineEdit = dynamic_cast<suggestionLineEdit*>(watched);
    if(!lineEdit)
        return QStyledItemDelegate::eventFilter(watched,event);

    if(event->type()==QEvent::FocusOut){
        auto* focusEvent = static_cast<QFocusEvent*>(event);
        // Clicking the suggestion box must not close the editor.
        if(focusEvent->reason()==Qt::FocusReason::OtherFocusReason
           ||focusEvent->reason()==Qt::FocusReason::ActiveWindowFocusReason){
            lineEdit->setFocus();
            return true;
        }
        emit lineEdit->editingFinished();
        return true;
    }
    if(event->type()==QEvent::KeyPress){
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        if(keyEvent->key()==Qt::Key_Tab){
            if(m_suggestionBox&&m_suggestionBox->currentItem())
                lineEdit->setText(m_suggestionBox->currentItem()->data(Qt::UserRole).toString());
            m_suggestionBox->hide();
            return true;
        }
    }
    return QStyledItemDelegate::eventFilter(watched,event);
}
