#ifndef TETAGLISTWIDGET_H
#define TETAGLISTWIDGET_H
#include "suggestionlineedit.h"
#include "tesignalwidget.h"
extern QStringList ClipBoard;

extern QString liststyle;
extern QString lineeditstyle;
class teEditorList;
struct tePictureFile;
typedef struct teTag tetag;typedef struct teTagCore tetagcore;
struct teTagList;
class teInputWidget;

/**
 * @brief Widget for displaying and managing teTagList
 */
struct teTagListWidgetBase : public QWidget,virtual public teObject
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
    teTagBase*editingTag=nullptr;///< Tag currently being edited
    teTagBase* select_current=nullptr;///< Currently selected tag (current)
    int tmpSelectIndex=-1;///< Temporary index for maintaining selection position
    std::set<teTagBase*> select;///< Set of selected tags (excluding current)

    QMenu* menu = new QMenu(this);///< Context menu for right-click operations

    // Style enums for different tag states
    teTagBase::tetagStyle normal_style_enum = teTagBase::tetagStyle::normal;///< Normal state style
    teTagBase::tetagStyle select_style_enum = teTagBase::tetagStyle::select;///< Selected state style
    teTagBase::tetagStyle select_current_style_enum = teTagBase::tetagStyle::select_current;///< Current selection state style
    QAction *editAction,*deleteAction,*insertAction,*insertBelowAction,*cutAction,*copyAction,*pasteAction;

public slots:
    /**
     * @brief Sets a tag as the current selection
     * @param tag Tag to set as current (nullptr to clear)
     * @param ifclear Whether to clear previous selections first
     */
    virtual void setSelectCurrent(teTagBase*tag=nullptr,bool ifclear=true);

    /**
     * @brief Selects a range of tags from current to specified tag
     * @param in End tag of selection range
     * @param ifclear Whether to clear previous selections first
     * @return Number of tags selected
     */
    virtual int setSelectRange(teTagBase*in,bool ifclear=true)=0;

    /**
     * @brief Adds a tag to the selection set
     * @param in Tag to select
     */
    virtual void setSelect(teTagBase* in);

    /**
     * @brief Removes tags from selection set
     * @param in Tag to unselect (nullptr to clear all)
     * @return Number of tags unselected
     */
    virtual int setUnselect(teTagBase*in=nullptr);

    /**
     * @brief Checks if a tag is selected
     * @param in Tag to check
     * @return True if selected
     */
    bool isSelected(teTagBase*in);

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
    virtual void tagEdit(teTagBase*tag,teWordBase *inw=nullptr);

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
    void enterEvent(QEnterEvent*)override;

    /**
     * @brief Connects a tag for signal handling
     * @param tagwidget Tag widget to connect
     */
    virtual void connectTag(teTagBase *tagwidget);

    /**
     * @brief Disconnects a tag from signal handling
     * @param tagwidget Tag widget to disconnect
     */
    void disconnectTag(teTagBase *tagwidget);

    /**
     * @brief Clears all displayed tags
     * @param in Optional tag list to keep managed (not cleared)
     */
    virtual void clear(teTagList* in=nullptr)=0;

    /**
     * @brief Deletes a tag from the managed list
     * @param tag Tag to delete
     */
    virtual void tagErase(std::shared_ptr<teTagCore> tag=nullptr)=0;

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
    virtual void tagEdit(std::shared_ptr<teTagCore>tag, QString text,int removeDuplicate=1,bool ifemit=true)=0;

    /**
     * @brief Inserts a new tag at specified position
     * @param index Insertion position
     * @param in_tag Tag to insert
     * @param removeDuplicate Duplicate handling mode
     * @param select Whether to select the new tag
     * @return Pointer to inserted tag widget
     */
    virtual teTagBase* taginsert(int index,std::shared_ptr<teTagCore>in_tag,int removeDuplicate=1,bool select=true)=0;

    /**
     * @brief Inserts a new tag above current selection
     * @param edit Whether to enter edit mode after insertion
     * @param newtag Pre-configured tag to insert (nullptr for empty tag)
     * @param removeDuplicate Duplicate handling mode
     */
    virtual void tagInsertAbove(bool edit=true,std::shared_ptr<teTagCore>newtag=nullptr,int removeDuplicate=1)=0;

    /**
     * @brief Inserts a new tag below current selection
     * @param edit Whether to enter edit mode after insertion
     * @param newtag Pre-configured tag to insert (nullptr for empty tag)
     * @param removeDuplicate Duplicate handling mode
     */
    virtual void tagInsertBelow(bool edit=true,std::shared_ptr<teTagCore>newtag=nullptr,int removeDuplicate=1)=0;

    void focusOutEvent(QFocusEvent *e)override;

    /**
     * @brief Converts selected tags to comma-separated string
     * @return Formatted string of selected tags
     */
    QString getSelectText();

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
    virtual void tagdroped(teTagBase*in_tag,int modifiers)=0;

    /**
     * @brief Handles right-click events on tags
     * @param tag Clicked tag widget
     * @param point Click position
     * @param modifiers Keyboard modifiers
     */
    void onTagRightButtonClicked(teTagBase*tag,QPoint point,int modifiers);

    /**
     * @brief Handles left-click events on tags
     * @param tag Clicked tag widget
     * @param point Click position
     * @param modifiers Keyboard modifiers
     */
    void onTagLeftButtonClicked(teTagBase*tag,QPoint point,int modifiers);
};

struct teTagListWidget : public teTagListWidgetBase
{
    Q_OBJECT
public:
    teTagList* showing_list=nullptr;///< the teTagList which is displaying
    teEditorList* editorlist=nullptr;///< the EditorList connected with
    tePictureFile* file;
    teTagListWidget(QWidget *parent = nullptr);
    ~teTagListWidget(){
        clear();
    }
    size_t size()const;
    virtual void clear(teTagList* in=nullptr)override;
    virtual void load(teTagList* newlist=nullptr)override;
    virtual void tagdroped(teTagBase*in_tag,int modifiers)override;
    virtual teTagBase* taginsert(int index, std::shared_ptr<teTagCore>in_tag,int removeDuplicate=1,bool select=true)override;
    void onTagEdited(std::shared_ptr<teTagCore>tag);
    void undo();
    void redo();
    void loadFile(tePictureFile*f);
    void tagErase(int index)override;
    virtual void tagErase(std::shared_ptr<teTagCore> tag=nullptr)override;
    virtual void tagEdit(std::shared_ptr<teTagCore>tag, QString text,int removeDuplicate=1,bool ifemit=true)override;
    virtual void tagInsertAbove(bool edit=true,std::shared_ptr<teTagCore>newtag=nullptr,int removeDuplicate=1)override;
    virtual void tagInsertBelow(bool edit=true,std::shared_ptr<teTagCore>newtag=nullptr,int removeDuplicate=1)override;
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent*event)override{
        if(event->button() == Qt::RightButton){
            onTagRightButtonClicked(nullptr,event->pos(),event->modifiers());
        }
    }
    void onFileDeleted(tePictureFile*obj);
public slots:
    virtual int setSelectRange(teTagBase*in,bool ifclear=true)override;
signals:
    void newlistloaded(teTagList*);
    void showinglistDestroyed();
};

#endif // TETAGLISTWIDGET_H
