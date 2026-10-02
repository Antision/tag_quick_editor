#ifndef TETAGLISTMODEL_H
#define TETAGLISTMODEL_H
#include "tetag.h"

/**
 * @brief QAbstractListModel over a teTagList: one row per tag.
 *
 * This is the data layer of the model/view based tag list - the replacement for
 * "one teTag widget per tag" plus the widget pool. It is usable on its own
 * (the regression check drives it directly, including with
 * QAbstractItemModelTester), which is why it does not depend on the widget that
 * will host it.
 *
 * Mutation contract
 * -----------------
 * Structural changes must go through the model so the views receive proper
 * begin/end notifications: insertTag(), eraseTag(), eraseRows(), moveTag() and
 * setTagText().
 *
 * The tag list can also be changed by other parts of the program: editing a
 * multi selection inserts a tag into every selected image, and a rename erases
 * the tag it merged into. Those changes arrive through the teTagList signals;
 * the model answers a structural one with a model reset, after which the view
 * restores its selection by tag core (see
 * teTagListWidgetBase::setSelectCurrentCore()). That is affordable because
 * those changes happen in the multi selection mode, in which the single image
 * tag list is not even visible.
 */
class teTagListModel : public QAbstractListModel
{
    Q_OBJECT
public:
    /// Role carrying the teTagCore* behind a row.
    ///
    /// DisplayRole/EditRole carry the tag *text*, as Qt expects them to (and as
    /// QAbstractItemModelTester insists on), so the tag object itself needs a
    /// role of its own.
    enum { TagCoreRole = Qt::UserRole + 1 };

    explicit teTagListModel(QObject* parent=nullptr);
    ~teTagListModel() override;

    /// Starts showing `list` (nullptr empties the model).
    void setTagList(teTagList* list);
    teTagList* tagList() const { return m_list; }

    int rowCount(const QModelIndex& parent=QModelIndex()) const override;
    QVariant data(const QModelIndex& index,int role=Qt::DisplayRole) const override;
    bool setData(const QModelIndex& index,const QVariant& value,int role=Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    bool removeRows(int row,int count,const QModelIndex& parent=QModelIndex()) override;
    bool moveRows(const QModelIndex& sourceParent,int sourceRow,int count,
                  const QModelIndex& destinationParent,int destinationRow) override;
    /// Dragging a row has to carry data, otherwise Qt never starts the drag;
    /// the payload is the row number (an internal move needs nothing more).
    QMimeData* mimeData(const QModelIndexList& indexes) const override;
    Qt::DropActions supportedDropActions() const override { return Qt::MoveAction; }
    QStringList mimeTypes() const override;

    /// Tag shown in `row`, or nullptr when the row does not exist.
    teTag* tagAt(int row) const;
    /// Row of `tag`, or -1.
    int rowOf(teTag* tag) const;

    /// Inserts `tag` at `row`. Returns -1 when it was refused as a duplicate.
    /// `removeDuplicate`: 0 = allow duplicates, 1 = refuse, 2 = merge (the tags
    /// already there are erased first, so the result is a single tag).
    int insertTag(int row,std::shared_ptr<teTag> tag,int removeDuplicate=1);
    /// Erases one row (and the tag behind it).
    bool eraseTag(int row);
    /// Erases `count` rows starting at `row`.
    bool eraseRows(int row,int count);
    /// Moves one row onto another position.
    bool moveTag(int from,int to);
    /// Replaces the text of one row (dedupe, undo record and the editors are
    /// all handled by teTagList::edit()).
    bool setTagText(int row,const QString& text);

    /// Announces an order change that happened behind the model's back (used by
    /// undo/redo, which reorders teTagList directly).
    void notifyExternalReorder();

private slots:
    void onListInserted(std::shared_ptr<teTag> tag);
    void onListErased(std::shared_ptr<teTag> tag);
    void onListEdited(std::shared_ptr<teTag> tag);

private:
    void connectList(teTagList* list);
    void disconnectList();
    /// True while this model itself changes the list, so its own signals are not
    /// mistaken for an external change.
    bool m_selfMutation=false;
    teTagList* m_list=nullptr;
};

/**
 * @brief Draws and edits one tag of teTagListModel.
 *
 * The colours mirror the sheets the widget based tag list used
 * (teTagBase::setStyle()), and the inline editor is the same
 * suggestionLineEdit the rest of the program uses, so tag completion keeps
 * working.
 */
class teTagDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit teTagDelegate(QWidget* parent=nullptr);
    ~teTagDelegate() override;

    /// Height of one row; 30 matches the multi tag list.
    int rowHeight() const { return m_rowHeight; }
    void setRowHeight(int height){ m_rowHeight=height; }

    void paint(QPainter* painter,const QStyleOptionViewItem& option,const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option,const QModelIndex& index) const override;

    QWidget* createEditor(QWidget* parent,const QStyleOptionViewItem& option,const QModelIndex& index) const override;
    void setEditorData(QWidget* editor,const QModelIndex& index) const override;
    void setModelData(QWidget* editor,QAbstractItemModel* model,const QModelIndex& index) const override;
    void updateEditorGeometry(QWidget* editor,const QStyleOptionViewItem& option,const QModelIndex& index) const override;
    /// The editor is reused for every row, so Qt must not delete it.
    void destroyEditor(QWidget* editor,const QModelIndex& index) const override {
        Q_UNUSED(editor); Q_UNUSED(index);
    }

protected:
    bool eventFilter(QObject* watched,QEvent* event) override;

private:
    int m_rowHeight=30;
    /// Owned suggestion box + shared inline editor (same scheme as the multi tag
    /// list, so tag completion keeps working while editing inline).
    ///
    /// Both are QPointers: once the editor has been handed to a view, that view's
    /// viewport owns it, and the viewport may well be destroyed first.
    QPointer<QListWidget> m_suggestionBox;
    QPointer<suggestionLineEdit> m_editor;
    mutable int m_editingRow=-1;
};

#endif // TETAGLISTMODEL_H
