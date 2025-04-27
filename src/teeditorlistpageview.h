#ifndef TEEDITORLISTPAGEVIEW_H
#define TEEDITORLISTPAGEVIEW_H
#include "pch.h"
#include "temultitaglistview.h"
#include "tesignalwidget.h"

class teEditorListPageView;
class EditorListLayoutWidget;
class teEditorListPageModel : public QAbstractItemModel,public teObject {
    Q_OBJECT
public:
    explicit teEditorListPageModel(EditorListLayoutWidget*in_parentLayoutWidget,editorListLayout*in_page,int in_pageIdx,QObject *parent = nullptr)
        : QAbstractItemModel(parent),layout(in_page),parentLayoutWidget(in_parentLayoutWidget),myPageIdx(in_pageIdx) {}

    editorListLayout*layout=nullptr;
    EditorListLayoutWidget*parentLayoutWidget=nullptr;
    int myPageIdx=0;
    void setPagePtr(editorListLayout*in_page){
        layout=in_page;
    }
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override {
        if (!parent.isValid() && row >= 0 && row < layout->editors[myPageIdx].size())
            return createIndex(row, column, &layout->editors[myPageIdx][row]);
        return QModelIndex();
    }
    QModelIndex parent(const QModelIndex &child) const override { return QModelIndex(); }
    int rowCount(const QModelIndex &parent = QModelIndex()) const override {
        if (!parent.isValid()) return layout->editors[myPageIdx].size();
        return 0;
    }
    bool setData(const QModelIndex &index, const QVariant &value, int role)override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override { return 1; }

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override {
        if (!index.isValid() || index.row() >= layout->editors[myPageIdx].size()) return QVariant();
        return QVariant::fromValue(layout->editors[myPageIdx][index.row()]);
    }

    Qt::ItemFlags flags(const QModelIndex &index) const override {
        return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;
    }
    Qt::DropActions supportedDropActions() const override {
        return Qt::CopyAction;
    }
    Qt::DropActions supportedDragActions() const override {
        return Qt::CopyAction;
    }
    QMimeData *mimeData(const QModelIndexList &indexes) const override;
    bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) override;
    bool removeRows(int row, int count=1, const QModelIndex &parent = QModelIndex()) override {
        int d=row+count;
        if (row < 0 || d > layout->editors[myPageIdx].size())
            return false;
        beginRemoveRows(parent, row, d - 1);
        endRemoveRows();
        return true;
    }
signals:
    void listModified();
};

// --- Custom Delegate for teTagList ---
class teEditorListPageDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    int tagheight=30;
    explicit teEditorListPageDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) {}
    void setEditorData(QWidget *editor, const QModelIndex &index) const override {}
    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override {
        QLineEdit *lineEdit = qobject_cast<QLineEdit*>(editor);
        model->setData(index,lineEdit->text());
    }
    mutable bool showEditor=false;
    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        return QSize(200, tagheight); // Fixed height for each tag
    }
};

class EditorListLayoutWidget:public teSignalWidget{
public:
    QVector<QPair<QListView*,teEditorListPageModel*>>listviews;
    teEditorListPageDelegate* delegate = new teEditorListPageDelegate;
    editorListLayout*editorlistlayout=nullptr;
    QHBoxLayout *layout = new QHBoxLayout;
    QPushButton*addPageButton;
    int pageCount=0;
    EditorListLayoutWidget(editorListLayout*in_editorlistlayout,QWidget*parent=nullptr);
    void loadPages(){
        clear();
        for(int i=0;i<editorlistlayout->editors.size();++i){
            addList(true);
        }
    }

    void clear(){
        for(auto[list,pagelayout]:listviews){
            delete list;
        }
        listviews.clear();
        pageCount=0;
    }
    void addList(bool notAddPageInLayout);
    void load(){
        loadPages();
        this->show();
    }
};

#endif // TEEDITORLISTPAGEVIEW_H
