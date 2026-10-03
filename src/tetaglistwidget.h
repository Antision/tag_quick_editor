#ifndef TETAGLISTWIDGET_H
#define TETAGLISTWIDGET_H
#include "suggestionlineedit.h"
#include "tesignalwidget.h"
#include "tetaglistmodel.h"
#include "tetagdisplaywidget.h"    // teTagDisplayOwner: what the popup needs
extern QStringList ClipBoard;

extern QString liststyle;
extern QString lineeditstyle;
class teEditorList;
class tePictureFile;
class teTagDisplayWidget;
class teTagWidget;
class teTag;


class teTagList;
class teInputWidget;

/**
 * @brief Widget for displaying and managing teTagList
 */
class teTagListWidgetBase : public QWidget, public teTagDisplayOwner, virtual public teObject
{
    Q_OBJECT
public:
    /**
     * @brief Constructor controlling word size in tag display
     * @param wordsize Font size for words in tags
     * @param parent Parent widget
     */
    teTagListWidgetBase(int wordsize,QWidget *parent);
    ~teTagListWidgetBase();
    QVBoxLayout* wlayout=nullptr;///< Main layout of the widget
    QVBoxLayout* layout=nullptr;///< Layout for scroll area
    QScrollArea* sc=nullptr;///< Container for displaying teTags
    suggestionLineEdit* lineedit;///< Auto-completing line edit for tag editing
    teInputWidget* plaintextedit;///< Plain text editor for long-form tags
    QListWidget* suggestionBox = new QListWidget;///< Auto-completion suggestion box
    teTagWidgetBase*editingTag=nullptr;///< Tag currently being edited
    std::shared_ptr<teTag> editingCore=nullptr;///< Tag being edited when it has no widget
    teTagWidgetBase* select_current=nullptr;///< Currently selected tag (current)
    int pendingSelectionIndex=-1;///< Temporary index for maintaining selection position
    std::set<teTagWidgetBase*> select;///< Set of selected tags (excluding current)

    QMenu* menu = new QMenu(this);///< Context menu for right-click operations

    // -- magnified popup ------------------------------------------------------
    teTagDisplayWidget* tagDisplay=nullptr;///< floating magnified tag
    QTimer* tagDisplayHideTimer=nullptr;///< grace period while moving into the popup
    QPointer<teTagWidgetBase> pendingDisplayTag;
    std::shared_ptr<teTag> pendingDisplayCore;///< ditto, for the model/view list
    /// The popup follows the pointer while it is up (a click is what opens it).
    void followTagDisplay(teTagWidgetBase* tag);
    /// Same, for a tag that is drawn by a delegate and has no widget.
    void followTagDisplayCore(std::shared_ptr<teTag> core);
    void scheduleTagDisplayHide();
    void showTagDisplay(teTagWidgetBase* tag);
    /// Same, for a tag that is drawn by a delegate and has no widget.
    void showTagDisplayCore(std::shared_ptr<teTag> core);
    void hideTagDisplay();
    /**
     * @brief Global point the popup is placed next to.
     *
     * The widget based list uses the right edge of the tag's widget; a model/view
     * list overrides this with the right edge of the row, which is what the user
     * asked for ("at the right border of the tag list view").
     */
    virtual QPoint tagDisplayAnchor(std::shared_ptr<teTag> core) const;
    /// Hides the popup when the key was Escape; true when it did.
    bool handleTagDisplayEscape(QKeyEvent* event);
    void tagDisplayHideTick();
    /// Installs the hover filter on a tag and on the words it already owns.
    void installTagDisplayFilters(teTagWidgetBase* tag);
    /**
     * @brief True when this list wants the magnified popup.
     *
     * Only the model/view list does: a row cannot be dragged word by word or
     * edited word by word, which is the whole reason the popup exists. The widget
     * based lists (the editors' tag lists) drag words and open the inline editor
     * on the tag itself, so the popup would only get in their way.
     */
    virtual bool wantsTagDisplay() const { return false; }
    /// The list this widget currently shows (nullptr when it shows nothing).
    virtual teTagList* shownList() const { return nullptr; }
    /// Hides the popup when the cursor really left it (buttons, editors, ...).
    bool eventFilter(QObject* watched,QEvent* event) override;

    // Style enums for different tag states
    teTagWidgetBase::teTagStyle normal_style_enum = teTagWidgetBase::teTagStyle::normal;///< Normal state style
    teTagWidgetBase::teTagStyle select_style_enum = teTagWidgetBase::teTagStyle::select;///< Selected state style
    teTagWidgetBase::teTagStyle select_current_style_enum = teTagWidgetBase::teTagStyle::select_current;///< Current selection state style
    QAction *editAction,*deleteAction,*insertAction,*insertBelowAction,*cutAction,*copyAction,*pasteAction;

public slots:
    /**
     * @brief Sets a tag as the current selection
     * @param tag Tag to set as current (nullptr to clear)
     * @param ifclear Whether to clear previous selections first
     */
    virtual void setSelectCurrent(teTagWidgetBase*tag=nullptr,bool ifclear=true);

    /**
     * @brief Selects a range of tags from current to specified tag
     * @param in End tag of selection range
     * @param ifclear Whether to clear previous selections first
     * @return Number of tags selected
     */
    virtual int setSelectRange(teTagWidgetBase*in,bool ifclear=true)=0;

    /**
     * @brief Adds a tag to the selection set
     * @param in Tag to select
     */
    virtual void setSelect(teTagWidgetBase* in);

    /**
     * @brief Removes tags from selection set
     * @param in Tag to unselect (nullptr to clear all)
     * @return Number of tags unselected
     */
    virtual int setUnselect(teTagWidgetBase*in=nullptr);

    /**
     * @brief Checks if a tag is selected
     * @param in Tag to check
     * @return True if selected
     */
    virtual bool isSelected(teTagWidgetBase*in);

    /* -- selection addressed by tag core ------------------------------------
     * The editors know only the tag *core* they are linked to, so they used to
     * reach into it with `core->widget` and pass the widget they found. That
     * assumption breaks as soon as a tag has no widget of its own (a tag that
     * is scrolled out of view, or any model/view based list), and it also
     * dereferenced the pointer without checking it.
     *
     * These helpers are the single place where a core is turned into a widget
     * for the widget based implementation; a model/view based implementation
     * only has to override them to work on rows instead.
     */
    /// teTagDisplayOwner: a click inside the list keeps the popup open.
    bool ownsWidget(QWidget* widget) const override {
        return widget&&(widget==this||isAncestorOf(widget));
    }
    /// teTagDisplayOwner: dragging the popup body moves the tag it belongs to.
    void tagDragged(teTagWidgetBase* tag,Qt::KeyboardModifiers modifiers) override;

    /// The widget currently representing `core` in this list, or nullptr.
    teTagWidgetBase* widgetForCore(const std::shared_ptr<teTag>& core) const;
    virtual void setSelectCurrentCore(std::shared_ptr<teTag> core,bool ifclear=true);
    virtual void setSelectCore(std::shared_ptr<teTag> core);
    virtual int setUnselectCore(std::shared_ptr<teTag> core=nullptr);
    virtual int setSelectRangeCore(std::shared_ptr<teTag> core,bool ifclear=true);
    virtual bool isCoreSelected(std::shared_ptr<teTag> core) const;
    /// Scrolls the list so `core` is visible. Safe when it has no widget.
    virtual void ensureCoreVisible(std::shared_ptr<teTag> core);

    /**
     * @brief Loads a new teTagList
     * @param newlist New tag list to load (nullptr to clear)
     */
    virtual void load(teTagList* newlist=nullptr){};

    /**
     * @brief Edits a tag (triggered by double-click)
     * @param tag Tag to edit
     * @param inw Specific word in tag to edit (if provided)
     */
    virtual void tagEdit(teTagWidgetBase*tag,teWordWidgetBase *inw=nullptr);

    /**
     * @brief Edits a tag that has no widget of its own (model/view list).
     *
     * The widget based implementation simply looks the widget up; a model/view
     * list overrides this to open the delegate editor on the tag's row.
     */
    virtual void tagEditCore(std::shared_ptr<teTag> core);

    /**
     * @brief Handles text completion from plain text editor
     * @param in_str Edited text content
     */
    void onPlainTextEditStop(QString in_str);

    /**
     * @brief Handles text completion from line editor
     */
    void onLineEditStop();
public:
    /**
     * @brief Connects a tag for signal handling
     * @param tagwidget Tag widget to connect
     */
    virtual void connectTag(teTagWidgetBase *tagwidget);

    /**
     * @brief Disconnects a tag from signal handling
     * @param tagwidget Tag widget to disconnect
     */
    void disconnectTag(teTagWidgetBase *tagwidget);

    /**
     * @brief Clears all displayed tags
     * @param in Optional tag list to keep managed (not cleared)
     */
    virtual void clear(teTagList* in=nullptr)=0;

    /**
     * @brief Deletes a tag from the managed list
     * @param tag Tag to delete
     */
    virtual void tagErase(std::shared_ptr<teTag> tag=nullptr)=0;

    /**
     * @brief Deletes a tag by index
     * @param index Index of tag to delete
     */
    virtual void tagErase(int index)=0;

    /**
     * @brief Edits the currently selected tag
     */
    virtual void tagEdit();

    /**
     * @brief Directly modifies tag content
     * @param tag Tag to modify
     * @param text New text content
     * @param removeDuplicate Duplicate handling mode (0=allow,1=skip,2=replace)
     * @param ifemit Whether to emit modification signals
     */
    virtual void tagEdit(std::shared_ptr<teTag>tag, QString text,int removeDuplicate=1,bool ifemit=true)=0;

    /**
     * @brief Inserts a new tag at specified position
     * @param index Insertion position
     * @param in_tag Tag to insert
     * @param removeDuplicate Duplicate handling mode
     * @param select Whether to select the new tag
     * @return Pointer to inserted tag widget
     */
    virtual teTagWidgetBase* taginsert(int index,std::shared_ptr<teTag>in_tag,int removeDuplicate=1,bool select=true)=0;

    /**
     * @brief Inserts a new tag above current selection
     * @param edit Whether to enter edit mode after insertion
     * @param newtag Pre-configured tag to insert (nullptr for empty tag)
     * @param removeDuplicate Duplicate handling mode
     */
    virtual void tagInsertAbove(bool edit=true,std::shared_ptr<teTag>newtag=nullptr,int removeDuplicate=1)=0;

    /**
     * @brief Inserts a new tag below current selection
     * @param edit Whether to enter edit mode after insertion
     * @param newtag Pre-configured tag to insert (nullptr for empty tag)
     * @param removeDuplicate Duplicate handling mode
     */
    virtual void tagInsertBelow(bool edit=true,std::shared_ptr<teTag>newtag=nullptr,int removeDuplicate=1)=0;

    void focusOutEvent(QFocusEvent *e)override;

    /**
     * @brief Converts selected tags to comma-separated string
     * @return Formatted string of selected tags
     */
    virtual QString getSelectText();

    virtual void cut(){
        QApplication::clipboard()->setText(getSelectText());
        tagErase();
    }
    virtual void copy(){
        QApplication::clipboard()->setText(getSelectText());
    }
    virtual void paste();

    /**
     * @brief Handles tag movement after drag-and-drop
     * @param in_tag Moved tag widget
     * @param modifiers Keyboard modifiers during drop
     */
    virtual void tagdroped(teTagWidgetBase*in_tag,int modifiers)=0;

    /**
     * @brief Handles right-click events on tags
     * @param tag Clicked tag widget
     * @param point Click position
     * @param modifiers Keyboard modifiers
     */
    void onTagRightButtonClicked(teTagWidgetBase*tag,QPoint point,int modifiers);

    /**
     * @brief Handles left-click events on tags
     * @param tag Clicked tag widget
     * @param point Click position
     * @param modifiers Keyboard modifiers
     */
    void onTagLeftButtonClicked(teTagWidgetBase*tag,QPoint point,int modifiers);
};

/**
 * @brief QListView that reorders rows when one is dropped on another.
 *
 * Qt's InternalMove would need mime data and a model that accepts drops; the
 * row move is expressed directly through teTagListModel::moveRows() instead,
 * which is also what the undo records expect.
 */
class teTagListView : public QListView
{
    Q_OBJECT
public:
    /// Marker for "the controller builds this view for itself".
    struct OwnedByController{};
    /**
     * @brief The view mainwindow.ui creates.
     *
     * It makes the tag list controller for itself and adopts it, so `taglist` in
     * the .ui *is* the view that draws the tags. (Before, the controller made a
     * second, private view and the one from the .ui stayed empty - which is why
     * nothing was displayed.)
     */
    explicit teTagListView(QWidget* parent=nullptr);
    /// The controller's own view (this one does not build a controller).
    teTagListView(OwnedByController,QWidget* parent);
    /// The controller behind this view (nullptr for the controller's own view).
    teTagListWidget* listWidget() const { return m_controller; }
    /**
     * @brief Moves the selected rows to where a drop at `viewportPos` would go.
     *
     * dropEvent() is only the plumbing around this; separating the two keeps the
     * placement rule testable without a real drag (a synthetic QDropEvent sent to
     * the viewport never reaches dropEvent()).
     *
     * @return false when there is nothing to move or the move changed nothing.
     */
    bool dropRowsAt(const QPoint& viewportPos);
    /// The picture whose caption is shown (see teTagListWidget::file).
    tePictureFile* file() const;
    /// The editor list connected with this list.
    void setEditorList(teEditorList* in);
    /// Shows `f`'s caption.
    void loadFile(tePictureFile* f);
    /// Detaches the current list.
    void clear();
    /// Scrolls back to the top.
    void scrollToTop();
protected:
    void dropEvent(QDropEvent* event) override;
    /// Keys (F2/Ctrl+W/Del/...) belong to the controller when it adopted this view.
    void keyPressEvent(QKeyEvent* event) override;
    /// Draws its own drag pixmap and, more importantly, does not let Qt remove
    /// the dragged rows a second time (see the comment in the implementation).
    void startDrag(Qt::DropActions supportedActions) override;
private:
    static void sortRows(QVector<int>& rows);
    /// The controller, when mainwindow.ui's view built one.
    teTagListWidget* m_controller=nullptr;
};

/**
 * @brief The tag list of one picture.
 *
 * The tags are drawn by teTagListView + teTagListModel + teTagDelegate: one row
 * per tag and no widget per tag, which is what makes switching from one image
 * to the next cheap. Everything the rest of the program uses (loading a file,
 * inserting/erasing/editing a tag, the selection addressed by tag core) keeps
 * the same signature; only the internals changed.
 */
class teTagListWidget : public QObject, public teTagDisplayOwner, virtual public teObject
{
    Q_OBJECT
public:
    teTagList* showing_list=nullptr;///< the teTagList which is displaying
    teEditorList* editorlist=nullptr;///< the EditorList connected with
    /// The file whose caption is shown. Must be initialized: clear() reads it
    /// (and calls into it) before loadFile() ever ran, and an uninitialized
    /// pointer there made the first clear()/destruction crash at random.
    tePictureFile* file=nullptr;
    /**
     * @brief Drives the view mainwindow.ui created.
     *
     * This used to be a QWidget that hosted a private teTagListView. It is a
     * QObject now: the view is the widget, this class is only the controller
     * behind it (that is why it does not need the widget based tag list base
     * class at all any more).
     */
    explicit teTagListWidget(teTagListView& view);
    ~teTagListWidget();
    size_t size()const;

    /// The view that draws the tags (one row per tag).
    teTagListView* view() const { return m_view; }
    /// Plain text window for long-form tags.
    teInputWidget* plaintextedit=nullptr;
    /// The magnified popup.
    teTagDisplayWidget* tagDisplay=nullptr;
    /// The model behind the view.
    teTagListModel* model() const { return m_model; }
    /// The delegate that draws and edits the rows.
    teTagDelegate* delegate() const { return m_delegate; }
    /// Tag of the current row, or nullptr.
    std::shared_ptr<teTag> currentCore() const;
    /// Scrolls back to the top of the list.
    void scrollToTop();
    /// Selects every row (Ctrl+A).
    void selectAllRows();

    void clear(teTagList* in=nullptr);
    void load(teTagList* newlist=nullptr);
    void onTagEdited(std::shared_ptr<teTag>tag);
    void undo();
    void redo();
    void loadFile(tePictureFile*f);
    void tagErase(int index);
    void tagErase(std::shared_ptr<teTag> tag=nullptr);
    void tagEdit(std::shared_ptr<teTag>tag, QString text,int removeDuplicate=1,bool ifemit=true);
    void tagEditCore(std::shared_ptr<teTag> core);
    /// Inserts a tag as a row. Returns nullptr: this list has no tag widgets, the
    /// signature is kept because the editors call it.
    teTagWidgetBase* taginsert(int index,std::shared_ptr<teTag> in_tag,int removeDuplicate=1,bool select=true);
    void tagInsertAbove(bool edit=true,std::shared_ptr<teTag>newtag=nullptr,int removeDuplicate=1);
    void tagInsertBelow(bool edit=true,std::shared_ptr<teTag>newtag=nullptr,int removeDuplicate=1);
    /// Keyboard shortcuts of the list; the view forwards its key events here.
    void keyPressEvent(QKeyEvent *event);
    /// Long-form tags are edited in a plain text window.
    void onPlainTextEditStop(QString in_str);
    void onFileDeleted(tePictureFile*obj);

    // -- clipboard -------------------------------------------------------------
    void copy();
    void cut();
    void paste();

    // -- selection, addressed by tag core (the view owns the selection) --------
    void setSelectCurrentCore(std::shared_ptr<teTag> core,bool ifclear=true);
    void setSelectCore(std::shared_ptr<teTag> core);
    int setUnselectCore(std::shared_ptr<teTag> core=nullptr);
    int setSelectRangeCore(std::shared_ptr<teTag> core,bool ifclear=true);
    bool isCoreSelected(std::shared_ptr<teTag> core) const;
    void ensureCoreVisible(std::shared_ptr<teTag> core);

    // -- the magnified popup ---------------------------------------------------
    /// The tag list this controller shows (used by the popup).
    teTagList* shownList() const override { return showing_list; }
    /// teTagDisplayOwner: a click inside the view keeps the popup open.
    bool ownsWidget(QWidget* widget) const override {
        return widget&&m_view&&(widget==m_view||m_view->isAncestorOf(widget));
    }
    /// Right edge of the row showing `core`.
    QPoint tagDisplayAnchor(std::shared_ptr<teTag> core) const;
    void showTagDisplayCore(std::shared_ptr<teTag> core);
    void hideTagDisplay();
    /// Keeps the popup following the current row while it is up.
    void followTagDisplayCore(std::shared_ptr<teTag> core);
    /// Esc closes the popup (returns true when it consumed the key).
    bool handleTagDisplayEscape(QKeyEvent* event);

    /// The text of the selected rows, in the caption's own format.
    QString getSelectText();
    /// Hides the magnified popup when the pointer really left it.
    bool eventFilter(QObject* watched,QEvent* event)override;
public slots:
    void onViewClicked(const QModelIndex& index);
    void onViewDoubleClicked(const QModelIndex& index);
    void onViewContextMenu(const QPoint& pos);
    void onViewCurrentChanged(const QModelIndex& current,const QModelIndex& previous);
    void onModelReset();
signals:
    void newlistloaded(teTagList*);
    void showinglistDestroyed();
private:
    /// Creates the model and the delegate.
    void createModelAndDelegate();
    /// Wires `view` up as this controller's view.
    void adoptView(teTagListView* view);
    /// Builds the context menu (it lives here: the view is only its parent).
    void createMenu();
    /// Starts editing `row` (or opens the sentence window).
    void startEditingRow(int row);
    /// Erases every selected row.
    void eraseSelectedRows();
    /// Drops a row that was left empty by the inline editor.
    void finishRowEdit(int row);
    /// Scrolls the popup away when the pointer left it (grace period).
    void scheduleTagDisplayHide();
    void tagDisplayHideTick();

    teTagListView* m_view=nullptr;
    teTagListModel* m_model=nullptr;
    teTagDelegate* m_delegate=nullptr;
    /// Context menu (parented to the view) and its actions.
    QMenu* menu=nullptr;
    QAction *editAction=nullptr,*deleteAction=nullptr,*insertAction=nullptr,
            *insertBelowAction=nullptr,*cutAction=nullptr,*copyAction=nullptr,
            *pasteAction=nullptr;
    /// The popup's grace timer.
    QTimer* tagDisplayHideTimer=nullptr;
    /// Core that was current before a model reset (external changes rebuild the
    /// model, so the selection has to be restored by core).
    std::shared_ptr<teTag> m_currentCoreBeforeReset;
    /// Row to select as soon as the next list is loaded (kept across a reset).
    int m_pendingSelectionIndex=-1;
    /// Core being edited in the plain text window (a row has no widget to ask).
    std::shared_ptr<teTag> m_plainTextCore;
};

#endif // TETAGLISTWIDGET_H
