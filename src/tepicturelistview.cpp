#include "tepicturelistview.h"


void tePictureFileModel::clear() {
    std::lock_guard<std::mutex> ul{pictureListMutex};
    emit clearAllFiles();
    beginResetModel();
    qDeleteAll(picturefiles);
    picturefiles.clear();
    for(tePictureFile*fp:picturefiles)
        fp->taglist.teDisconnect(this);
    endResetModel();
}

void tePictureFileModel::append(const QList<tePictureFile *> &files) {
    std::lock_guard<std::mutex> ul{pictureListMutex};
    if (files.isEmpty())
        return;
    beginInsertRows(QModelIndex(), picturefiles.size(), picturefiles.size() + files.size() - 1);
    picturefiles.append(files);
    for(tePictureFile*fp:files)
        fp->taglist.teConnect(teCallbackType::edit,this,&tePictureFileModel::updatePicturefile,fp);
    endInsertRows();
    emit newFileLoaded(files,false);
}

QVariant tePictureFileModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= picturefiles.size())
        return QVariant();
    if (role == Qt::DisplayRole) {
        return QVariant::fromValue(picturefiles[index.row()]);
    }
    return QVariant();
}

void tePictureFileModel::updatePicturefile(tePictureFile *in_file){
    int row = picturefiles.indexOf(in_file);
    if (row != -1) {
        QModelIndex index = createIndex(row, 0);
        emit dataChanged(index, index,{Qt::SizeHintRole});
    }
}

void tePictureFileModel::emitdataChanged(tePictureFile *caller, QModelIndex index){
    if(loading_count==0){
        parentView->scheduleDelayedItemsLayout();
    }
    if(caller)
        teDisconnect(caller);
}

QVector<QModelIndex> tePictureFileModel::filt(const teFiltRule &rule)
{
    QVector<QModelIndex> result;
    int pictureCount = picturefiles.size();
    result.reserve(pictureCount/3);

    for (int i=0;i<pictureCount;++i) {
        tePictureFile* file = picturefiles[i];
        QSet<tetagcore> currentTags;
        currentTags.reserve(file->taglist.size());
        for (std::shared_ptr<tetagcore> tag : file->taglist) {
            currentTags.insert(*tag);
        }
        if (!std::all_of(rule.a.cbegin(), rule.a.cend(),
                         [&](const tetagcore& tag) { return currentTags.contains(tag); })) {
            continue;
        }
        if (!rule.r.isEmpty() &&
            !std::any_of(rule.r.cbegin(), rule.r.cend(),
                         [&](const tetagcore& tag) { return currentTags.contains(tag); })) {
            continue;
        }

        if (!std::none_of(rule.n.cbegin(), rule.n.cend(),
                          [&](const tetagcore& tag) { return currentTags.contains(tag); })) {
            continue;
        }
        if (!rule.x.isEmpty()) {
            const int xCount = std::count_if(rule.x.cbegin(), rule.x.cend(),
                                             [&](const tetagcore& tag) { return currentTags.contains(tag); });
            if (xCount != 1) continue;
        }
        result.append(index(i));
    }
    return result;
}

void tePictureFileDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const {
    if (!index.isValid()) return;

    auto *file = index.data(Qt::DisplayRole).value<tePictureFile *>();
    if (!file) return;

    painter->save();

    QRect rect = option.rect;
    QColor borderColor;
    QColor bgColor;

    if (option.state & QStyle::State_Selected) {
        borderColor = QColor(35, 255, 140);
        if (option.state & QStyle::State_MouseOver) {
            bgColor = QColor(72, 169, 123);
        } else {
            bgColor = QColor(43, 95, 80);
        }
    } else {
        borderColor = QColor(97, 57, 255);
        if (option.state & QStyle::State_MouseOver) {
            bgColor = QColor(38, 14, 121);
        } else {
            bgColor = QColor(27, 27, 27);
        }
    }
    painter->setBrush(bgColor);
    painter->setPen(Qt::NoPen);
    painter->drawRect(rect);
    painter->setPen(QPen(borderColor, 1));
    painter->drawRect(rect.adjusted(1, 1, -1, -1));

    QRect imageRect{0,0,0,0};
    if (!file->image.isNull()) {
        imageRect = {rect.topLeft() + QPoint(1, 1), file->image.size()};
        painter->drawImage(imageRect, file->image);
    }

    painter->setFont(QFont("Segoe UI", 14));
    painter->setPen(Qt::white);
    painter->drawText(imageRect.right() + 1, rect.top()+(rect.height()-painter->fontMetrics().height())/2+ 20, QString(file->filepath.stdpath.filename().c_str()));

    painter->setFont(QFont("Segoe UI", 12, QFont::StyleItalic));
    painter->setPen(QColor(200, 200, 200));
    painter->drawText(imageRect.right() + 1, rect.bottom()- painter->fontMetrics().height()+15,
                      QString("%1 tags").arg(file->taglist.size()));

    painter->restore();
}

QSize tePictureFileDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const {
    Q_UNUSED(option);
    auto *file = index.data(Qt::DisplayRole).value<tePictureFile *>();
    if (!file) return QSize(200, 100);
    bool tryLockRst = file->image_mt.try_lock();
    if(!tryLockRst||file->image.isNull()){
        if(index.model())
            file->teConnect(teCallbackType::loading_finished,(tePictureFileModel*)((tePictureFileModel*)index.model()),&tePictureFileModel::emitdataChanged,file,index);
        else{
        }
        if(tryLockRst)
            file->image_mt.unlock();
        return QSize(200, 100);
    }else if(tryLockRst)
        file->image_mt.unlock();
    return {file->image.width()+2+QFontMetrics(QFont("Segoe UI", 14)).horizontalAdvance(file->filepath.qstring),file->image.height()+2};
}

tePictureListView::tePictureListView(QWidget *parent):QListView(parent){
    setModel(&picturefileModel);
    picturefileModel.parentView=this;
    setItemDelegate(&picturefileDelegate);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setSelectionBehavior(QAbstractItemView::SelectItems);
}
