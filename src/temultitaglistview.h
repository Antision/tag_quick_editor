#ifndef TEMULTITAGLISTVIEW_H
#define TEMULTITAGLISTVIEW_H
#include <QListView>
#include <QStandardItemModel>
#include "qstyleditemdelegate.h"
#include "teeditor.h"
#include "teeditorlist.h"
#include "tepicturefile.h"
#include "tetag.h"

struct teSelectionTagModel;

/**
 * @brief One row of the global multi-selection tag list.
 *
 * A multi tag is the union of one identical tag over every selected image.
 * `linked_tags` maps the per-image tag cores to the list they live in.
 */
class teSelectionTag:public teObject{
public:
    QString text;
    std::multimap<std::shared_ptr<teTag>,teTagList*>linked_tags;
    teSelectionTagModel*model=nullptr;
    bool ifexecute=true;
    bool re_read_switch=true;

    teSelectionTag(teSelectionTagModel*parent=nullptr):model(parent){}
    teSelectionTag(QString in_text,teSelectionTagModel*parent=nullptr):model(parent){
        text=std::move(in_text);
    }
    teSelectionTag(std::shared_ptr<teTag>in_core,teTagList*in_list,teSelectionTagModel*parent=nullptr):model(parent){
        text=*in_core;
        linked_tags.insert({in_core,in_list});
    }
    teSelectionTag(std::multimap<std::shared_ptr<teTag>,teTagList*>&&in_tags,teSelectionTagModel*parent=nullptr):model(parent){
        linked_tags=std::move(in_tags);
    }
    void link(std::shared_ptr<teTag>in_core,teTagList*in_list);
    void unlink(std::shared_ptr<teTag>in_core);
    void setText(const QString& in_text);
    void setText(QString&& in_text);
    void setCoreText();
    /// Drops every link pointing into `in_list`.
    void unlink_tags_in_list(teTagList*in_list);
    void clear();

    void self_destroy();
    void re_read(std::shared_ptr<teTag>in,teTagList*);
    operator QString() const{
        return text;
    }
};

class teMultitagListView;

class teSelectionTagModel : public QAbstractItemModel,public teObject {
    Q_OBJECT
public:
    explicit teSelectionTagModel(QObject *parent = nullptr)
        : QAbstractItemModel(parent) {}

    QList<teTagList*>linked_taglists;
    QList<teSelectionTag*> tags;
    teEditorList* editorlist=nullptr;
    QListView*listview=nullptr;
    /// Nested `insert` notifications are suppressed while this is 0. Always
    /// decremented again - a leaked decrement used to silently stop the multi
    /// tag list from picking up newly typed tags.
    int tagInsertSuppression=1;

    void linkTagList(teTagList*in_list);
    void unlinkTagList(teTagList*in_list=nullptr);

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override {
        if (!parent.isValid() && row >= 0 && row < tags.size())
            return createIndex(row, column, tags[row]);
        return QModelIndex();
    }
    QModelIndex parent(const QModelIndex &child) const override {
        Q_UNUSED(child);
        return QModelIndex();
    }
    int rowCount(const QModelIndex &parent = QModelIndex()) const override {
        if (!parent.isValid()) return tags.size();
        return 0;
    }
    int columnCount(const QModelIndex &parent = QModelIndex()) const override {
        Q_UNUSED(parent);
        return 1;
    }
    bool setData(const QModelIndex &index, const QVariant &value, int role)override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    /// Finds (or, when `row` is given, creates) the multi tag for `in_text`.
    teSelectionTag* getMultiTag(const QString& in_text,bool& isNewEntry,int row=-1,bool ifselect=false);
    /// Multi tag whose text equals `in_text`, or nullptr. `exclude` is skipped.
    teSelectionTag* findMultiTag(const QString& in_text,teSelectionTag* exclude=nullptr) const;
    void linkNewTagcore(std::shared_ptr<teTag>in_core,teTagList*in_list);

    /// Inserts a placeholder row and starts editing it (used by "insert above").
    teSelectionTag* tagInsert(int row, std::shared_ptr<teTag>in_tag=nullptr,bool select=true);
    teSelectionTag* tagInsert(int row,const QString&text,bool select);
    /// Moves the multi tag to the relative position it represents in every list.
    void setPos(int index);
    Qt::ItemFlags flags(const QModelIndex &index) const override {
        Q_UNUSED(index);
        return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled | Qt::ItemIsEditable;
    }
    Qt::DropActions supportedDropActions() const override {
        return Qt::MoveAction;
    }
    QMimeData *mimeData(const QModelIndexList &indexes) const override;
    bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) override {
        Q_UNUSED(data); Q_UNUSED(action); Q_UNUSED(row); Q_UNUSED(column); Q_UNUSED(parent);
        return false;
    }
    bool moveRows(const QModelIndex &sourceParent, int sourceRow, int count,
                  const QModelIndex &destinationParent, int destinationRow) override;
    /// Moves an arbitrary set of rows to `destination` (insertion index in the
    /// current index space). Used by the view's drag & drop.
    bool moveTags(const QList<int>& rows, int destination);

    void newTagTypeFinished(teSelectionTag *in_tag);

    void loadFiles(QList<tePictureFile *> in_filelist,bool ifclear);
    void eraseFiles(QList<tePictureFile *> in_filelist);

    /// Makes sure every linked list holds exactly one tag with `in_tag->text`
    /// and that `in_tag` links to it. Returns the multi tag that owns it.
    teSelectionTag* insertToTaglist(teSelectionTag *in_tag, double pos);

    void connectTag(teSelectionTag*in_tag){
        in_tag->teConnect(ready_destroy,this,(void(teSelectionTagModel::*)(teSelectionTag *))&teSelectionTagModel::tagErase,in_tag);
    }
    void tagErase(teSelectionTag *in_tag){
        tagErase(tags.indexOf(in_tag));
    }
    void tagErase(int index){
        if(index>=0&&index<tags.size())
            removeRows(index,1);
    }
    /// Destroys the tag (removing it from all images) and its row.
    void tagDestroy(int index);
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
    explicit teTagListDelegate(QWidget*parent=nullptr);
    bool eventFilter(QObject *watched, QEvent* e) override;

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    void setEditorData(QWidget *editor, const QModelIndex &index) const override {
        Q_UNUSED(editor); Q_UNUSED(index);
    }
    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;
    ~teTagListDelegate(){
        delete lineedit;
    }
    void destroyEditor(QWidget* editor, const QModelIndex& index) const override {
        Q_UNUSED(editor); Q_UNUSED(index);
    }
    mutable bool showEditor=false;
    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        Q_UNUSED(index);
        if(!showEditor)return;
        editor->setGeometry(option.rect.adjusted(4,2,-1,-2));
        static_cast<suggestionLineEdit*>(editor)->moveSuggestionBox();
    }
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        Q_UNUSED(option); Q_UNUSED(index);
        return QSize(200, tagheight); // Fixed height for each tag
    }
};

class teMultitagListView : public QListView,public teObject {
    Q_OBJECT
public:
    QMenu* menu = new QMenu(this);
    teSelectionTagModel*model;
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
