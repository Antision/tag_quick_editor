#ifndef TEREFTAGLISTWIDGET_H
#define TEREFTAGLISTWIDGET_H
#include"tetag.h"
#include"tetaglistwidget.h"

class teRefTagWidget: public teTagWidgetBase
{
    Q_OBJECT
public:
    std::set<teRefWordWidget*>wordWidgets;
    teRefTagWidget(QWidget*parent=nullptr):teTagWidgetBase(parent){setStyle(teTagWidget::normal);}
    teRefTagWidget(std::shared_ptr<teTag>incore,QWidget*parent=nullptr):teTagWidgetBase(incore,parent){load();setStyle(teTagWidget::normal);}
    ~teRefTagWidget(){
    }
    virtual void readCore(std::shared_ptr<teTag>in_core)override;

    /// The editor's tag list builds its words from the tag core.
    bool ownsWordWidgets() const override { return false; }

    virtual void load()override;
    void clearWordWidgets()override;
    void self_giveback();
    void eraseCore(teTagList*list){
        list->erase(core);
    }
    void worddroped(teWordWidgetBase*in_word,int xpos)override;
};

bool teRefTagCmp(teRefTagWidget*a,teRefTagWidget*b);
class teRefTagListWidget:public teTagListWidgetBase{
public:
    std::map<teRefTagWidget*,teTagList*,bool(*)(teRefTagWidget*,teRefTagWidget*)>tags{teRefTagCmp};
    teTagListView*parentTagListWidget=nullptr;
    teRefTagListWidget(QWidget*parent):teTagListWidgetBase(12,parent){}
    teRefTagListWidget(teTagListView*parentlist,QWidget*parent):teTagListWidgetBase(12,parent),parentTagListWidget(parentlist){};
    ~teRefTagListWidget(){}
    void keyPressEvent(QKeyEvent *event) override;
    virtual int setSelectRange(teTagWidgetBase*in,bool ifclear=true)override;
    void setSelectAll();
    void clear(teTagList *in=nullptr)override;
    int findindex(std::shared_ptr<teTag>in_tag);
    int findindex(teTagWidgetBase*in_tag);

    virtual void tagDestroy(int index);
    virtual void tagDestroy(std::shared_ptr<teTag>tag=nullptr);
    virtual void tagErase(int index)override;
    virtual void tagErase(std::shared_ptr<teTag>tag=nullptr)override;
    std::map<teRefTagWidget*,teTagList*,bool(*)(teRefTagWidget*,teRefTagWidget*)>::iterator findIterator(std::shared_ptr<teTag>tag);
    void findAndErase(std::shared_ptr<teTag>in_core);
    void Destroy(teRefTagWidget*tag);
    virtual void connectTag(teTagWidgetBase* tag)override{
        teTagListWidgetBase::connectTag(tag);
        tag->core->teConnect(teCallbackType::destroy,this,(void (teRefTagListWidget::*)(std::shared_ptr<teTag>))&teRefTagListWidget::tagErase,tag->core);
    }
    void tagdroped(teTagWidgetBase *in_tag,int modifiers)override;
    virtual void tagInsertAbove(bool edit=true,std::shared_ptr<teTag>newtag=nullptr,int removeDuplicate=1)override;
    virtual void tagInsertBelow(bool edit=true,std::shared_ptr<teTag>newtag=nullptr,int removeDuplicate=1)override;
    void tagInsertAbove(bool edit,std::shared_ptr<teTag>newtag,teTagList*list,int removeDuplicate=1);
    void tagEdit(std::shared_ptr<teTag>tag, QString text,int removeDuplicate=1,bool ifemit=true)override;
    virtual void paste() override;
    virtual teTagWidgetBase* taginsert(int index,std::shared_ptr<teTag>in_tag,int removeDuplicate=-1,bool select=false)override;
    virtual teTagWidgetBase* taginsert(int index,std::shared_ptr<teTag>in_tag,teTagList*in_list,int removeDuplicate=-1,bool select=false);
};

#endif // TEREFTAGLISTWIDGET_H
