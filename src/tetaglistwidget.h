#ifndef TETAGLISTWIDGET_H
#define TETAGLISTWIDGET_H
#include "suggestionedit.h"
#include "tesignalwidget.h"
#include "tagviewcontracts.h"
#include "taglistview.h"
extern QStringList ClipBoard;

class teTagDisplayWidget;
class teTagWidget;
class teTag;


class teTagList;
class teInputWidget;

/**
 * @brief Widget for displaying and managing teTagList
 */
class teTagListWidgetBase : public QWidget, public TagListHost, virtual public teObject
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
    SuggestionEdit* lineedit;///< Auto-completing line edit for tag editing
    teInputWidget* plaintextedit;///< Plain text editor for long-form tags
    QListWidget* suggestionBox = new QListWidget;///< Auto-completion suggestion box
    teTagWidgetBase*editingTag=nullptr;///< Tag currently being edited
    std::shared_ptr<teTag> editingCore=nullptr;///< Tag being edited when it has no widget
    teTagWidgetBase* select_current=nullptr;///< Currently selected tag (current)
    int pendingSelectionIndex=-1;///< Temporary index for maintaining selection position
    std::set<teTagWidgetBase*> select;///< Set of selected tags (excluding current)

    QMenu* menu = new QMenu(this);///< Context menu for right-click operations
    /// Widgets this list owns, by tag (see registerWidgetFor). This is the table
    /// that replaces teTag::widget: a QPointer so a widget recycled by the pool
    /// cannot leave a dangling entry behind.
    QHash<teTag*,QPointer<teTagWidgetBase>> m_widgets;

    /// The list this widget currently shows (nullptr when it shows nothing).
    virtual teTagList* shownList() const { return nullptr; }

    // Style enums for different tag states
    teTagWidgetBase::teTagStyle normal_style_enum = teTagWidgetBase::teTagStyle::normal;///< Normal state style
    teTagWidgetBase::teTagStyle select_style_enum = teTagWidgetBase::teTagStyle::select;///< Selected state style
    teTagWidgetBase::teTagStyle select_current_style_enum = teTagWidgetBase::teTagStyle::select_current;///< Current selection state style
    QAction *editAction,*deleteAction,*insertAction,*insertBelowAction,*cutAction,*copyAction,*pasteAction;

    /**
     * @brief Drops `widget` from the selection bookkeeping.
     *
     * The selection addresses tags by widget pointer. A widget handed back to the
     * pool (or deleted) must not stay in it: setUnselect()/clear() compared and
     * re-styled that pointer later, which was a use-after-free on the picture
     * switch that followed a merge.
     */
    void forgetSelection(teTagWidgetBase* widget){
        if(!widget)
            return;
        if(select_current==widget)
            select_current=nullptr;
        select.erase(widget);
    }

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
    /**
     * @brief The list's own widget table.
     *
     * This is what replaces `teTag::widget` (step 3b): widgets belong to the list
     * that shows them. TagListView never fills it, which is why a view based
     * list can never hand out - or leak - a tag widget.
     */
    void registerWidgetFor(std::shared_ptr<teTag> core,teTagWidgetBase* widget);
    void unregisterWidgetFor(std::shared_ptr<teTag> core);
    /// TagListHost: the list's entry point for "the widget of this tag".
    teTagWidgetBase* ensureWidgetFor(std::shared_ptr<teTag> core) override;
    /// TagListHost: gives the widget of `core` back to the pool.
    void releaseWidgetFor(std::shared_ptr<teTag> core) override;
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
 * @brief The model/view based tag list, now shared with gallery_info.
 *
 * It lives in common/tagview/taglistview.*; this header keeps including it so
 * the editors, the UI file and mainwindow keep one include for "the tag list".
 */

#endif // TETAGLISTWIDGET_H
