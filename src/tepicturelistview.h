#ifndef TEPICTURELISTVIEW_H
#define TEPICTURELISTVIEW_H
#include "qstyleditemdelegate.h"
#include "tepicturefile.h"
struct teFiltRule{
    QVector<tetagcore> a;
    QVector<tetagcore> r;
    QVector<tetagcore> n;
    QVector<tetagcore> x;
};
class tePictureListView;

class tePictureFileModel : public QAbstractListModel,public teObject {
    Q_OBJECT

public:
    QList<tePictureFile*> picturefiles;
    std::mutex pictureListMutex;
    tePictureListView*parentView;
    explicit tePictureFileModel(QObject *parent = nullptr) : QAbstractListModel(parent) {}

    void clear();

    void append(const QList<tePictureFile *> &files);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override {
        if (parent.isValid()) return 0;
        return picturefiles.size();
    }
    void save(){
        std::lock_guard<std::mutex> ul{pictureListMutex};
        for(tePictureFile*file:picturefiles){
            file->save();
        }
    }
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    void updatePicturefile(tePictureFile*in_file);
    void emitdataChanged(tePictureFile*caller,QModelIndex index);

    QVector<QModelIndex> filt(const teFiltRule& rule);
signals:
    void newFileLoaded(QList<tePictureFile *> files,bool ifclear);
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
    tePictureListView(QWidget*parent=nullptr);
    void enterEvent(QEnterEvent *event)override{
        setFocus();
        QListView::enterEvent(event);
    }
    void selectNext(){
        if(auto selections = selectionModel()->selectedRows();!selections.empty()&&selections[0].row()<model()->rowCount())
            selectionModel()->select(model()->index(selections[0].row()+1,0),QItemSelectionModel::ClearAndSelect);
    }
    void selectPrevious(){
        if(auto selections = selectionModel()->selectedRows();!selections.empty()&&selections[0].row()>0)
            selectionModel()->select(model()->index(selections[0].row()-1,0),QItemSelectionModel::ClearAndSelect);
    }
    void selectIndexList(QVector<QModelIndex>indexes){
        selectionModel()->clearSelection();
        for(QModelIndex&idx:indexes)
            selectionModel()->select(idx,QItemSelectionModel::Select);
    }
    QVector<QModelIndex> filt(const teFiltRule&rule){
        return dynamic_cast<tePictureFileModel*>(model())->filt(rule);
    }
};
#endif
