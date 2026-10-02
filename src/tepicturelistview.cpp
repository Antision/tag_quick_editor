#include "tepicturelistview.h"
#include "func.h"
#include <QContextMenuEvent>
#include <QMenu>
#include <QProcess>
#include <QDir>
#include <QMessageBox>
#include <shellapi.h>

namespace {

/// Moves `paths` to the recycle bin. Returns false when nothing was removed.
bool moveToRecycleBin(const QStringList& paths, QString* errorOut)
{
    if (paths.isEmpty()) {
        if (errorOut)
            *errorOut = QStringLiteral("nothing to delete");
        return false;
    }

    // SHFileOperation wants a double-null-terminated list of absolute,
    // backslash-separated paths.
    std::wstring from;
    for (const QString& path : paths) {
        const QString native = QDir::toNativeSeparators(QFileInfo(path).absoluteFilePath());
        from.append(native.toStdWString());
        from.push_back(L'\0');
    }
    from.push_back(L'\0');

    SHFILEOPSTRUCTW op{};
    op.wFunc = FO_DELETE;
    op.pFrom = from.c_str();
    op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;
    const int result = SHFileOperationW(&op);

    if (result != 0 || op.fAnyOperationsAborted) {
        if (errorOut) {
            if (result != 0)
                *errorOut = QStringLiteral("SHFileOperation failed with code %1").arg(result);
            else
                *errorOut = QStringLiteral("the operation was aborted");
        }
        return false;
    }
    return true;
}

}

void tePictureFileModel::clear() {
    // Stop every in-flight thumbnail decode before the files disappear.
    for (tePictureFile* file : picturefiles)
        file->cancelLoading();

    beginResetModel();
    const QList<tePictureFile*> removed = picturefiles;
    picturefiles.clear();
    endResetModel();

    for (tePictureFile* file : removed)
        delete file;

    emit clearAllFiles();
}

void tePictureFileModel::append(const QList<tePictureFile *> &files) {
    if (files.isEmpty())
        return;
    beginInsertRows(QModelIndex(), picturefiles.size(), picturefiles.size() + files.size() - 1);
    picturefiles.append(files);
    for (tePictureFile* file : files) {
        // Refresh the row when the caption changes ...
        file->taglist.teConnect(teCallbackType::edit, this, &tePictureFileModel::updatePicturefile, file);
        // ... and once when the thumbnail has been decoded. The connection is
        // made here (once per file) instead of from the delegate's sizeHint,
        // which used to connect thousands of times while the view was laid out.
        file->teConnect(teCallbackType::loading_finished, this, &tePictureFileModel::updatePicturefile, file);
    }
    endInsertRows();
    emit newFileLoaded(files,false);
}

void tePictureFileModel::removeFiles(const QList<tePictureFile *> &files)
{
    if (files.isEmpty())
        return;

    QList<int> rows;
    rows.reserve(files.size());
    for (tePictureFile* file : files) {
        const int row = picturefiles.indexOf(file);
        if (row >= 0)
            rows.append(row);
    }
    if (rows.isEmpty())
        return;

    std::sort(rows.begin(), rows.end(), std::greater<int>());

    QList<tePictureFile*> removed;
    for (int row : rows) {
        // Taking rows from the back keeps the remaining indices valid.
        beginRemoveRows(QModelIndex(), row, row);
        removed.append(picturefiles.takeAt(row));
        endRemoveRows();
    }

    for (tePictureFile* file : removed) {
        file->cancelLoading();
        delete file;
    }
}

QVariant tePictureFileModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= picturefiles.size())
        return QVariant();
    if (role == Qt::DisplayRole) {
        return QVariant::fromValue(picturefiles[index.row()]);
    }
    return QVariant();
}

void tePictureFileModel::updatePicturefile(tePictureFile *in_file){
    const int row = picturefiles.indexOf(in_file);
    if (row < 0)
        return;
    const QModelIndex idx = index(row, 0);
    emit dataChanged(idx, idx, {Qt::DisplayRole, Qt::SizeHintRole});
    // A freshly decoded thumbnail changes the row height, so the layout has to
    // be redone. This is called on the GUI thread now (see tePictureFile).
    if (parentView)
        parentView->scheduleDelayedItemsLayout();
}

void tePictureFileModel::save(){
    for(tePictureFile*file:picturefiles){
        file->save();
    }
}

QVector<QModelIndex> tePictureFileModel::filt(const teFiltRule &rule)
{
    QVector<QModelIndex> result;
    const int pictureCount = picturefiles.size();
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

    const QRect rect = option.rect;
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

    // thumbnail() returns a refcounted copy taken under the loader's mutex.
    const QImage image = file->thumbnail();
    QRect imageRect{0,0,0,0};
    if (!image.isNull()) {
        imageRect = {rect.topLeft() + QPoint(1, 1), image.size()};
        painter->drawImage(imageRect, image);
    }

    static const QFont nameFont("Segoe UI", 14);
    static const QFont tagCountFont("Segoe UI", 12, QFont::StyleItalic);

    painter->setFont(nameFont);
    painter->setPen(Qt::white);
    painter->drawText(imageRect.right() + 1, rect.top()+(rect.height()-painter->fontMetrics().height())/2+ 20, file->name());

    painter->setFont(tagCountFont);
    painter->setPen(QColor(200, 200, 200));
    painter->drawText(imageRect.right() + 1, rect.bottom()- painter->fontMetrics().height()+15,
                      QString("%1 tags").arg(file->taglist.size()));

    painter->restore();
}

QSize tePictureFileDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const {
    Q_UNUSED(option);
    auto *file = index.data(Qt::DisplayRole).value<tePictureFile *>();
    if (!file) return QSize(200, 100);

    const QImage image = file->thumbnail();
    if (image.isNull())
        return QSize(200, 100);

    // Fixed metric objects: this function runs for every visible row on every
    // layout pass and used to construct a QFont/QFontMetrics each time.
    static const QFont nameFont("Segoe UI", 14);
    static const QFontMetrics nameMetrics(nameFont);
    return {image.width() + 2 + nameMetrics.horizontalAdvance(file->name()), image.height() + 2};
}

tePictureListView::tePictureListView(QWidget *parent):QListView(parent){
    setModel(&picturefileModel);
    picturefileModel.parentView=this;
    setItemDelegate(&picturefileDelegate);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setSelectionBehavior(QAbstractItemView::SelectItems);
    setContextMenuPolicy(Qt::DefaultContextMenu);
}

void tePictureListView::selectNext(){
    if(auto selections = selectionModel()->selectedRows();!selections.empty()&&selections[0].row()<model()->rowCount())
        selectionModel()->select(model()->index(selections[0].row()+1,0),QItemSelectionModel::ClearAndSelect);
}

void tePictureListView::selectPrevious(){
    if(auto selections = selectionModel()->selectedRows();!selections.empty()&&selections[0].row()>0)
        selectionModel()->select(model()->index(selections[0].row()-1,0),QItemSelectionModel::ClearAndSelect);
}

void tePictureListView::selectIndexList(QVector<QModelIndex> indexes){
    if (indexes.isEmpty())
        return;
    selectionModel()->clearSelection();
    for(QModelIndex&idx:indexes)
        selectionModel()->select(idx,QItemSelectionModel::Select);
    // Keep "current" in sync with the selection, otherwise keyboard navigation
    // and auto-scrolling start from a stale row.
    selectionModel()->setCurrentIndex(indexes.first(), QItemSelectionModel::NoUpdate);
    scrollTo(indexes.first(), QAbstractItemView::EnsureVisible);
}

QList<tePictureFile*> tePictureListView::selectedFiles() const
{
    QList<tePictureFile*> files;
    const QModelIndexList rows = selectionModel()->selectedRows();
    files.reserve(rows.size());
    for (const QModelIndex& idx : rows) {
        if (auto* file = idx.data(Qt::DisplayRole).value<tePictureFile*>())
            files.append(file);
    }
    return files;
}

void tePictureListView::contextMenuEvent(QContextMenuEvent* event)
{
    const QModelIndex clicked = indexAt(event->pos());
    if (clicked.isValid() && !selectionModel()->isSelected(clicked))
        selectionModel()->select(clicked, QItemSelectionModel::ClearAndSelect);

    const QList<tePictureFile*> files = selectedFiles();
    if (files.isEmpty()) {
        QListView::contextMenuEvent(event);
        return;
    }

    QMenu menu(this);
    QAction* openAction = menu.addAction(tr("Open in File Explorer"));
    QAction* recycleAction = menu.addAction(tr("Move to Recycle Bin"));
    if (files.size() > 1)
        recycleAction->setText(tr("Move %1 items to Recycle Bin").arg(files.size()));

    QAction* chosen = menu.exec(event->globalPos());
    if (chosen == openAction) {
        const QString native = QDir::toNativeSeparators(files.first()->filepath.qstring);
        if (!QProcess::startDetached(QStringLiteral("explorer.exe"),
                                     QStringList{QStringLiteral("/select,") + native})) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(native).absolutePath()));
        }
    } else if (chosen == recycleAction) {
        QStringList paths;
        for (tePictureFile* file : files) {
            paths.append(file->filepath.qstring);
            // Take the caption file along with its image when it exists.
            const QString tagPath = file->tagFilePath();
            if (QFileInfo::exists(tagPath))
                paths.append(tagPath);
        }

        QString error;
        if (!moveToRecycleBin(paths, &error)) {
            QMessageBox::warning(this, tr("Move to Recycle Bin"),
                                 tr("Could not move the selected files to the recycle bin:\n%1").arg(error));
            return;
        }
        emit filesRemoved(files);
    }
}
