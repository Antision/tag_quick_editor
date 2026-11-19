#ifndef TEEDITOR_H
#define TEEDITOR_H
#include "tetaglistwidget.h"
class teTagWidget;
class teTagList;
class teTag;
class EditorControl;
class teEditorList;

/**
 * @brief One line of an editor page: an optional caption and the controls on it.
 *
 * A control used to be added with two hand-written lines in two places - one
 * adding it to the layout, one pushing it into `controls` - and a control that
 * got only the first one never saw a tag at all, silently. A row owns both
 * halves: it is the only thing that knows about its controls, and it registers
 * them in the order they are written here (which is also the order read() offers
 * tags to them, so the row *is* the priority list).
 */
struct teEditorRow {
    QString title;                        ///< caption above the row, optional
    QString lead;                         ///< plain label in front of the controls
    QVector<EditorControl*> controls;     ///< left to right, and in read order
};

class teEditor:public QFrame,public teObject{
public:
    QString name;
    bool ifrun=true;
    teEditor(TagListView*in_taglistwidget,const QString& name,QWidget*parent=nullptr);
    QList<EditorControl*>controls;
    TagListView*taglistwidget;
    teTagList*taglist=nullptr;
    virtual void reset();
    virtual TagHandling read(std::shared_ptr<teTag>tag);
    virtual TagHandling re_read(std::shared_ptr<teTag>tag);
    virtual void clear();
    void setTagListWidget(TagListView*listwidget);
    /// Wires one control into this editor: it learns which tag list it works for
    /// and is added to `controls`. Every control goes through here exactly once.
    void registerControl(EditorControl* control);
};

class teEditor_standard:public teEditor{
public:
    QPushButton *back_to_main_page = new QPushButton(this);
    QPushButton *editor_switch= new QPushButton(this);
    QVBoxLayout *mainLayout=new QVBoxLayout(this);
    QHBoxLayout *titleLayout= new QHBoxLayout;
    /**
     * @brief Everything that folds away when the editor is collapsed.
     *
     * The rows go into contentLayout, which lives in this one widget, so folding
     * is a single setVisible() - it used to walk the layout tree recursively and
     * hide every widget it found.
     */
    QWidget* content = new QWidget(this);
    QVBoxLayout *contentLayout = new QVBoxLayout(content);
    /**
     * @param accent The page's theme, which only decides the gradient of the
     *        switch button's text: the sheet itself is shared (res/style/editor.qss),
     *        so a new page adds one accent rule instead of a copy of the QSS.
     */
    teEditor_standard(TagListView*in_taglistwidget,const QString& name,const QString& accent=QString(),QWidget*parent=nullptr);
    /// Adds one row: the optional caption, then the controls, then registers them.
    void addRow(teEditorRow row){ addRowTo(contentLayout,row); }
    /**
     * @brief Adds a row to any layout - the sub-page panels use this too.
     *
     * Registration happens here as well, so a control that lives on a sub-page is
     * still a control of the editor (it reads tags like every other one).
     */
    void addRowTo(QBoxLayout* layout, teEditorRow row);
    /// A single control on its own row - the common case.
    void addControl(EditorControl* control){ addRow({{}, {}, {control}}); }
    /**
     * @brief A sub-page of this editor: `trigger` opens `panel`, "↑" comes back.
     *
     * The panel is added to the editor and hidden. This replaces the
     * `showInterface(int)` protocol, where the index had to be kept in step with
     * a vector by hand and `showInterface(0)` on the main page indexed -1.
     */
    void addSubPage(QPushButton* trigger, QWidget* panel);
    void showSubPage(QWidget* panel);
    void showMainPage();
    virtual void onSwitchToggled(bool state);
    virtual void fold();
    virtual void unfold();
protected:
    QVector<QWidget*> subPages;
    QWidget* openPage = nullptr;
};

#endif // TEEDITOR_H
