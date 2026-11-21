#ifndef TEEDITORLIST_H
#define TEEDITORLIST_H
#include"teeditor.h"
#include"tagviewcontracts.h"

extern const QString editorliststyle;
extern const QString editorListScrollAreaStyle;
class TagListView;


/**
 * @brief Where a dropped editor title belongs in the target column.
 *
 * @param anchors   (editor name, its rectangle inside the column) in display order
 * @param dragged   the editor being dragged; it is not an anchor, because it is
 *                  about to leave the list
 * @param y         drop position, in the column's own coordinates
 *
 * The result counts the editors that stay above the drop point, which is the
 * index to insert at *after* the dragged editor has been taken out. Counting the
 * dragged one as well is what used to push a same-column drop one row too far
 * (A dropped on B's lower half jumped past C instead of landing between B and C).
 *
 * Pure on purpose: the drop handler only has to feed it the geometry it sees.
 */
inline int editorDropRow(const QVector<QPair<QString,QRect>>& anchors,
                         const QString& dragged,int y)
{
    int row=0;
    for(const auto& anchor:anchors){
        if(anchor.first==dragged)
            continue;
        if(y<anchor.second.center().y())
            break;
        ++row;
    }
    return row;
}

class teEditorList:public QWidget,public teObject,public TagEditorHost
{
    Q_OBJECT
public:
    QVector<teEditor*> editorlist;
    QPoint m_editorDragStart;
    QString m_editorDragName;
    QHBoxLayout* layout = new QHBoxLayout(this);
    TagListView* tagListWidget=nullptr;
    teTagList* connectedList=nullptr;
    QSplitter* pageSplitter = new QSplitter(Qt::Horizontal,this);
    editorListLayout* editorLayout;
    ~teEditorList();
    teEditorList(QVector<teEditor*>&&input_vector,editorListLayout*in_listpage,QWidget* parent=nullptr)
        :QWidget(parent){loadEditors(std::forward<QVector<teEditor*>>(input_vector),in_listpage);}
    teEditorList(QWidget* parent=nullptr):QWidget(parent){}
    void loadEditors(const QVector<teEditor*>& input_vector_ptr,editorListLayout*in_listpage);
    void setEditorsToPages();
    void connectTaglistWidget(TagListView*in_taglist);
    /// Drag an editor by its title between the page columns (main window).
    void installEditorDrag();
    bool eventFilter(QObject* watched,QEvent* event) override;
    /// TagEditorHost: the tag view loaded `list`; make every editor read it.
    void readAll(teTagList* list) override;
    /// TagEditorHost: the shown list is gone; drop what the editors display.
    void clearAll() override;
    /// TagEditorHost: see TagEditorHost::mergeEnabled(). tqe keeps it as the
    /// global MergeSwitch the editor controls already consult.
    bool mergeEnabled() const override;
    void setMergeEnabled(bool enabled) override;
    /// TagEditorHost: the decoration rules/overrides changed - rebuild every
    /// linked tag and re-style the widget that shows it.
    void refreshDecorations() override;
    void onNewTaglistLoaded();
    void readList(teTagList*input_taglist);
    void unloadList();
    void onNewTagInserted(std::shared_ptr<teTag>in_tag);
    void onTagEdited(std::shared_ptr<teTag>in_tag);
    QVector<teEditor*>::iterator begin(){
        return editorlist.begin();
    }
    QVector<teEditor*>::iterator end(){
        return editorlist.end();
    }
    QVector<teEditor*>::const_iterator begin()const{
        return editorlist.begin();
    }
    QVector<teEditor*>::const_iterator end()const{
        return editorlist.end();
    }
};

#endif // TEEDITORLIST_H
