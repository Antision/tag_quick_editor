#ifndef TEMULTITAGLISTVIEW_H
#define TEMULTITAGLISTVIEW_H
#include <QListView>
#include <QStandardItemModel>
#include "qstyleditemdelegate.h"
#include "teeditor.h"
#include "teeditorlist.h"
#include "tepicturefile.h"
#include "tetag.h"
struct teMultiTagListModel;
typedef struct teMultiTagCore:public teObject{
    QString text;
    std::multimap<std::shared_ptr<tetagcore>,teTagList*>linked_tags;
    teMultiTagListModel*model=nullptr;
    teMultiTagCore(teMultiTagListModel*parent=nullptr):model(parent){}
    teMultiTagCore(QString in_text,teMultiTagListModel*parent=nullptr):model(parent){
        text=in_text;
    }
    teMultiTagCore(std::shared_ptr<tetagcore>in_core,teTagList*in_list,teMultiTagListModel*parent=nullptr):model(parent){
        text=*in_core;
        linked_tags.insert({in_core,in_list});
    }
    teMultiTagCore(std::multimap<std::shared_ptr<tetagcore>,teTagList*>&&in_tags,teMultiTagListModel*parent=nullptr):model(parent){
        linked_tags=std::move(in_tags);
    }
    void link(std::shared_ptr<tetagcore>in_core,teTagList*in_list);
    bool ifexecute=true;
    void unlink(std::shared_ptr<tetagcore>in_core);
    void setText(const QString& in_text);
    void setText(QString&& in_text);
    void setCoreText();
    void unlink_tags_in_list(teTagList*in_list);
    void link_tags_in_list(teTagList*in_list);
    void clear();

    void self_destroy();
    bool re_read_switch=true;
    void re_read(std::shared_ptr<tetagcore>in,teTagList*);
    operator QString() const{
        return text;
    }
}temultitagcore;

class teMultitagListView;
class teMultiTagListModel : public QAbstractItemModel,public teObject {
    Q_OBJECT
public:
    explicit teMultiTagListModel(QObject *parent = nullptr)
        : QAbstractItemModel(parent) {}

    QList<teTagList*>linked_taglists;
    QList<teMultiTagCore*> tags;
    teEditorList* editorlist=nullptr;
    QListView*listview;
    int ifrecivenewtagcoreinsertsignal=1;
    void linkTagList(teTagList*in_list);
    void unlinkTagList(teTagList*in_list=nullptr);
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override {
        if (!parent.isValid() && row >= 0 && row < tags.size())
            return createIndex(row, column, tags[row]);
        return QModelIndex();
    }
    QModelIndex parent(const QModelIndex &child) const override { return QModelIndex(); }
    int rowCount(const QModelIndex &parent = QModelIndex()) const override {
        if (!parent.isValid()) return tags.size();
        return 0;
    }
    bool setData(const QModelIndex &index, const QVariant &value, int role)override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override { return 1; }

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    static bool multitagTextCmp(teMultiTagCore* tag, const QString& text){
        return tag->text < text;
    }
    teMultiTagCore* getMultiTag(const QString& in_text,bool& ifnew,int row=-1,bool ifselect=false);
    void linkNewTagcore(std::shared_ptr<tetagcore>in_core,teTagList*in_list,QItemSelectionModel *selectionModel = nullptr);

    teMultiTagCore* tagInsert(int row, std::shared_ptr<tetagcore>in_tag=nullptr,bool select=true);

    teMultiTagCore* tagInsert(int row,const QString&text,bool select);
    void setPos(int index);
    Qt::ItemFlags flags(const QModelIndex &index) const override {
        return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled | Qt::ItemIsEditable;
    }
    Qt::DropActions supportedDropActions() const override {
        return Qt::MoveAction;
    }
    QMimeData *mimeData(const QModelIndexList &indexes) const override;
    bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) override {return false;}
    bool moveRows(const QModelIndex &sourceParent, int sourceRow, int count,
                  const QModelIndex &destinationParent, int destinationRow) override;

    void newTagTypeFinished(teMultiTagCore *in_tag, int row);
    void loadFiles(QList<tePictureFile *> in_filelist,bool ifclear);
    void eraseFiles(QList<tePictureFile *> in_filelist);
    teMultiTagCore * insertToTaglist(teMultiTagCore *in_tag, double pos);
    virtual void connectTag(teMultiTagCore*in_tag){
        in_tag->teConnect(ready_destroy,this,(void(teMultiTagListModel::*)(teMultiTagCore *))&teMultiTagListModel::tagErase,in_tag);
    }
    void tagErase(int in){
        removeRows(in,1);
    }
    void tagErase(teMultiTagCore *in_tag){
        removeRows(tags.indexOf(in_tag),1);
    }
    void tagDestroy(int index){
        tags[index]->self_destroy();
        tagErase(index);
    }
    void tagDestroy();
    void clear();
    bool removeRows(int row, int count=1, const QModelIndex &parent = QModelIndex()) override;

signals:
    void listModified();
};
class teTagListDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    suggestionLineEdit* lineedit;
    QListWidget* suggestionBox = new QListWidget;
    QWidget*parent;
    int tagheight=30;
    teTagListDelegate(QWidget*parent=nullptr);
    bool eventFilter(QObject* watched, QEvent* e) override;

    explicit teTagListDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) {}
    mutable int editing_row=-1;
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    void setEditorData(QWidget *editor, const QModelIndex &index) const override {}
    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;
    ~teTagListDelegate(){
        delete lineedit;
    }
    void destroyEditor(QWidget* editor, const QModelIndex& index) const override {}
    mutable bool showEditor=false;
    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        if(!showEditor)return;
        editor->setGeometry(option.rect.adjusted(4,2,-1,-2));
        ((suggestionLineEdit*)editor)->moveSuggestionBox();
    }
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        return QSize(200, tagheight); // Fixed height for each tag
    }
};

class teMultitagListView : public QListView,public teObject {
    Q_OBJECT
public:
    QMenu* menu = new QMenu(this);
    teMultiTagListModel*model;
    teTagListDelegate delegate{this};
    explicit teMultitagListView(QWidget *parent = nullptr);
    QAction *editAction,*insertAction,*insertBelowAction,* deleteAction,*copyAction,*cutAction,*setposAction,*pasteAction;
    void initializeMenu();
    void enterEvent(QEnterEvent *event)override{
        setFocus();
        QListView::enterEvent(event);
    }
    void dropEvent(QDropEvent *event)override;
    void startDrag(Qt::DropActions supportedActions)override;

    void copyToClipBoard(bool ifcut);

    void paste(const QModelIndex& index);

    void keyPressEvent(QKeyEvent *event);
private slots:
    void showContextMenu(const QPoint &pos);
};
#endif // TEMULTITAGLISTVIEW_H
