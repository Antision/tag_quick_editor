#include "teeditorlistpageview.h"


bool teEditorListPageModel::setData(const QModelIndex &index, const QVariant &value, int role) {
    if (!index.isValid() || role != Qt::EditRole) {
        return false;
    }

    if (!value.canConvert<QString>()) {
        return false;
    }

    int row = index.row();
    if (row < 0 || row >= layout->editors[myPageIdx].size()) {
        return false;
    }

    QString& data = (layout->editors)[myPageIdx][row];

    data = value.toString();

    emit dataChanged(index, index, {role});

    return true;
}

QMimeData *teEditorListPageModel::mimeData(const QModelIndexList &indexes) const {
    if (indexes.isEmpty()) return nullptr;
    QMimeData *mimeData = new QMimeData;

    QByteArray encodedData;
    QDataStream stream(&encodedData, QIODevice::WriteOnly);
    for (const QModelIndex &index : indexes) {
        if (index.isValid()) {
            stream << myPageIdx << index.row();
        }
    }
    mimeData->setData("application/x-qabstractitemmodeldatalist", encodedData);

    QString finalStr;
    for (const QModelIndex &index : indexes) {
        finalStr.append(layout->editors[myPageIdx][index.row()]);
        finalStr.append(qsl(", "));
    }
    finalStr.chop(2);
    mimeData->setText(finalStr);
    return mimeData;
}

bool teEditorListPageModel::dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) {
    QByteArray encodedData = data->data(QStringLiteral("application/x-qabstractitemmodeldatalist"));
    QDataStream stream(&encodedData, QIODevice::ReadOnly);
    if(stream.atEnd())return false;
    QVector<QPair<int,int>>pageAndIndex;
    int pageIndex,sourceRow;
    while (!stream.atEnd()) {
        stream >> pageIndex >> sourceRow;
        pageAndIndex.push_back({pageIndex,sourceRow});
    }
    if (row == -1) {
        if(parent.row()>-1)
            row = parent.row();
        else
            row = rowCount();
    }

    QList<std::pair<int, int>> samePageItems;
    std::function<bool(const std::pair<int, int>&,const std::pair<int, int>&)>f = [](const std::pair<int, int>&l,const std::pair<int, int>&r)->bool{
        if(l.first!=r.first)
            return l.first<r.first;
        else
            return l.second>r.second;
    };
    QList<std::pair<int, int>> otherPageItems;
    for (const auto& item : pageAndIndex) {
        parentLayoutWidget->listviews[item.first].first->model()->removeRow(sourceRow);
        if (item.first == myPageIdx) {
            samePageItems.append(item);
        } else {
            otherPageItems.append(item);
        }
    }

    int countBeforeRow = 0;
    for (const auto& [page, index] : samePageItems) {
        if (index < row) {
            countBeforeRow++;
        }
    }

    int adjustedRow = row - countBeforeRow;

    std::sort(samePageItems.begin(), samePageItems.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });
    std::sort(otherPageItems.begin(), otherPageItems.end(),
              [](const auto& l, const auto& r) {
        if(l.first!=r.first)
            return l.first<r.first;
        else
            return l.second>r.second; });
    QStringList movedEditors;

    for (const auto& [page, index] : samePageItems) {
        movedEditors.prepend(layout->editors[page].takeAt(index));
        parentLayoutWidget->listviews[page].first->model()->removeRows(index,1);
    }

    for (const auto& [page, index] : otherPageItems) {
        movedEditors.append(layout->editors[page].takeAt(index));
        parentLayoutWidget->listviews[page].first->model()->removeRows(index,1);
    }

    beginInsertRows(QModelIndex(), adjustedRow, adjustedRow + movedEditors.size() - 1);
    for (const QString& editorName : movedEditors) {
        layout->editors[myPageIdx].insert(adjustedRow, editorName);
    }
    endInsertRows();
    return true;
}

void teEditorListPageDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const {
    if(!showEditor)return;
    editor->setGeometry(option.rect.adjusted(4,2,-1,-2));
    ((suggestionLineEdit*)editor)->moveSuggestionBox();
}

void teEditorListPageDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const {
    painter->save();
    const QString* editorNamePtr = static_cast<QString*>(index.internalPointer());
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

    painter->setPen(Qt::white);
    painter->setFont(QFont("Segoe UI", 15));
    painter->drawText(rect.adjusted(3, 0, 0, 0), Qt::AlignVCenter | Qt::AlignLeft, *editorNamePtr);
    painter->restore();
}

EditorListLayoutWidget::EditorListLayoutWidget(editorListLayout *in_editorlistlayout, QWidget *parent):teSignalWidget(parent),editorlistlayout(in_editorlistlayout){
    content->setLayout(layout);
    layout->setContentsMargins(0,0,0,0);
    layout->setSpacing(2);
    addPageButton = new QPushButton;
    addPageButton->setStyleSheet(addButtonStyle);
    addPageButton->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Expanding);
    connect(addPageButton,&QPushButton::clicked,this,&EditorListLayoutWidget::addList,Qt::DirectConnection);
}

void EditorListLayoutWidget::addList(bool notAddPageInLayout){
    QListView* newlistview = new QListView;
    teEditorListPageModel* neweditorlistpagemodel = new teEditorListPageModel(this,editorlistlayout,pageCount++,newlistview);
    if(!notAddPageInLayout)
        editorlistlayout->editors.push_back({});
    newlistview->setModel(neweditorlistpagemodel);
    newlistview->setItemDelegate(delegate);
    newlistview->setEditTriggers(QAbstractItemView::NoEditTriggers);
    newlistview->setSelectionMode(QAbstractItemView::ExtendedSelection);
    newlistview->setDragDropMode(QAbstractItemView::DragDrop);
    newlistview->setDefaultDropAction(Qt::MoveAction);
    newlistview->setDragDropOverwriteMode(false);
    newlistview->setDragEnabled(true);
    newlistview->setAcceptDrops(true);
    newlistview->setDropIndicatorShown(true);
    listviews.push_back({newlistview,neweditorlistpagemodel});
    newlistview->setStyleSheet(liststyle.arg(0));
    layout->addWidget(newlistview);
    layout->addWidget(addPageButton);
}
