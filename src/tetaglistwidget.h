#ifndef TETAGLISTWIDGET_H
#define TETAGLISTWIDGET_H
#include "suggestionlineedit.h"
#include "tesignalwidget.h"
#include "tetaglistmodel.h"
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
class teTagListWidgetBase : public QWidget,virtual public teObject
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
    explicit teTagListView(QWidget* parent=nullptr):QListView(parent){}
protected:
    void dropEvent(QDropEvent* event) override;
    /// Draws its own drag pixmap and, more importantly, does not let Qt remove
    /// the dragged rows a second time (see the comment in the implementation).
    void startDrag(Qt::DropActions supportedActions) override;
private:
    static void sortRows(QVector<int>& rows);
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
class teTagListWidget : public teTagListWidgetBase
{
    Q_OBJECT
public:
    teTagList* showing_list=nullptr;///< the teTagList which is displaying
    teEditorList* editorlist=nullptr;///< the EditorList connected with
    /// The file whose caption is shown. Must be initialized: clear() reads it
    /// (and calls into it) before loadFile() ever ran, and an uninitialized
    /// pointer there made the first clear()/destruction crash at random.
    tePictureFile* file=nullptr;
    teTagListWidget(QWidget *parent = nullptr);
    ~teTagListWidget(){
        clear();
    }
    size_t size()const;

    /// The view that draws the tags (one row per tag).
    teTagListView* view() const { return m_view; }
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

    virtual void clear(teTagList* in=nullptr)override;
    virtual void load(teTagList* newlist=nullptr)override;
    virtual void tagdroped(teTagWidgetBase*in_tag,int modifiers)override;
    virtual teTagWidgetBase* taginsert(int index, std::shared_ptr<teTag>in_tag,int removeDuplicate=1,bool select=true)override;
    void onTagEdited(std::shared_ptr<teTag>tag);
    void undo();
    void redo();
    void loadFile(tePictureFile*f);
    void tagErase(int index)override;
    virtual void tagErase(std::shared_ptr<teTag> tag=nullptr)override;
    virtual void tagEdit(std::shared_ptr<teTag>tag, QString text,int removeDuplicate=1,bool ifemit=true)override;
    virtual void tagEditCore(std::shared_ptr<teTag> core)override;
    virtual void tagInsertAbove(bool edit=true,std::shared_ptr<teTag>newtag=nullptr,int removeDuplicate=1)override;
    virtual void tagInsertBelow(bool edit=true,std::shared_ptr<teTag>newtag=nullptr,int removeDuplicate=1)override;
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent*event)override{
        if(event->button() == Qt::RightButton){
            onTagRightButtonClicked(nullptr,event->pos(),event->modifiers());
        }
    }
    void onFileDeleted(tePictureFile*obj);

    // -- selection, addressed by tag core (the view owns the selection) --------
    virtual void setSelectCurrentCore(std::shared_ptr<teTag> core,bool ifclear=true)override;
    virtual void setSelectCore(std::shared_ptr<teTag> core)override;
    virtual int setUnselectCore(std::shared_ptr<teTag> core=nullptr)override;
    virtual int setSelectRangeCore(std::shared_ptr<teTag> core,bool ifclear=true)override;
    virtual bool isCoreSelected(std::shared_ptr<teTag> core) const override;
    virtual void ensureCoreVisible(std::shared_ptr<teTag> core)override;
    /// Right edge of the row showing `core` (see the base class).
    QPoint tagDisplayAnchor(std::shared_ptr<teTag> core) const override;
    virtual int setSelectRange(teTagWidgetBase*in,bool ifclear=true)override;
    /// The text of the selected rows, one tag per line.
    QString getSelectText()override;
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
    /// Starts editing `row` (or opens the sentence window).
    void startEditingRow(int row);
    /// Erases every selected row.
    void eraseSelectedRows();
    /// Drops a row that was left empty by the inline editor.
    void finishRowEdit(int row);

    teTagListView* m_view=nullptr;
    teTagListModel* m_model=nullptr;
    teTagDelegate* m_delegate=nullptr;
    /// Core that was current before a model reset (external changes rebuild the
    /// model, so the selection has to be restored by core).
    std::shared_ptr<teTag> m_currentCoreBeforeReset;
};

#endif // TETAGLISTWIDGET_H
