#ifndef TEEDITORLIST_H
#define TEEDITORLIST_H
#include"teeditor.h"

extern const QString editorliststyle;
extern const QString editorListScrollAreaStyle;
class teTagListWidget;


class teEditorList:public QWidget,public teObject
{
    Q_OBJECT
public:
    QVector<teEditor*> editorlist;
    QHBoxLayout* layout = new QHBoxLayout(this);
    teTagListWidget* tagListWidget=nullptr;
    teTagList* connectedList=nullptr;
    QSplitter* pageSplitter = new QSplitter(Qt::Horizontal,this);
    editorListLayout* editorLayout;
    ~teEditorList(){
        for(teEditor*edt:editorlist)
            delete edt;
        editorlist.clear();
    }
    teEditorList(QVector<teEditor*>&&input_vector,editorListLayout*in_listpage,QWidget* parent=nullptr)
        :QWidget(parent){loadEditors(std::forward<QVector<teEditor*>>(input_vector),in_listpage);}
    teEditorList(QWidget* parent=nullptr):QWidget(parent){}
    void loadEditors(const QVector<teEditor*>& input_vector_ptr,editorListLayout*in_listpage);
    void setEditorsToPages();
    void connectTaglistWidget(teTagListWidget*in_taglist);
    void onNewTaglistLoaded();
    void readList(teTagList*input_taglist);
    void unloadList();
    void onNewTagInserted(std::shared_ptr<tetagcore>in_tag);
    void onTagEdited(std::shared_ptr<tetagcore>in_tag);
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
