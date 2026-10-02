#ifndef TEPICTURELISTVIEW_H
#define TEPICTURELISTVIEW_H
#include "qstyleditemdelegate.h"
#include "tepicturefile.h"

/// One rule of the filter window: all / any / none / exactly one of.
struct teFiltRule{
    QVector<teTag> a;
    QVector<teTag> r;
    QVector<teTag> n;
    QVector<teTag> x;
};

class tePictureListView;

class tePictureFileModel : public QAbstractListModel, public teObject {
    Q_OBJECT

public:
    QList<tePictureFile*> picturefiles;
    tePictureListView*parentView = nullptr;

    explicit tePictureFileModel(QObject *parent = nullptr) : QAbstractListModel(parent) {}

    /// Deletes every file. Emits clearAllFiles().
    void clear();
    /// Appends files, wires their signals and starts their thumbnail decode.
    void append(const QList<tePictureFile *> &files);
    /// Removes and deletes the given files (used by "move to recycle bin").
    void removeFiles(const QList<tePictureFile *> &files);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override {
        if (parent.isValid())
            return 0;
        return picturefiles.size();
    }

    void save();

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    /// Refreshes one row (tag count changed, thumbnail arrived, ...).
    void updatePicturefile(tePictureFile* in_file);

    QVector<QModelIndex> filt(const teFiltRule& rule);

    /// Index of `file`, or -1.
    int indexOf(tePictureFile* file) const { return picturefiles.indexOf(file); }

signals:
    void newFileLoaded(QList<tePictureFile *> files, bool ifclear);
    void clearAllFiles();
};

class tePictureFileDelegate : public QStyledItemDelegate {
    Q_OBJECT

public:
    explicit tePictureFileDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) {}

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};

class tePictureListView:public QListView{
    Q_OBJECT
    friend class tePictureFileModel;
public:
    tePictureFileDelegate picturefileDelegate;
    tePictureFileModel picturefileModel;
    explicit tePictureListView(QWidget*parent=nullptr);

    tePictureFileModel* fileModel(){ return &picturefileModel; }

    void enterEvent(QEnterEvent *event)override{
        setFocus();
        QListView::enterEvent(event);
    }

    void selectNext();
    void selectPrevious();
    void selectIndexList(QVector<QModelIndex> indexes);

    QVector<QModelIndex> filt(const teFiltRule&rule){
        return picturefileModel.filt(rule);
    }

    /// Data of the rows currently selected, in view order.
    QList<tePictureFile*> selectedFiles() const;

    void contextMenuEvent(QContextMenuEvent* event) override;

signals:
    /// Emitted after files were successfully moved to the recycle bin.
    void filesRemoved(QList<tePictureFile*> files);
};
#endif
