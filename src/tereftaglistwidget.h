#ifndef TEREFTAGLISTWIDGET_H
#define TEREFTAGLISTWIDGET_H
#include"tetag.h"
#include"tetaglistwidget.h"

typedef class teRefTag: public teTagBase
{
    Q_OBJECT
public:
    std::set<terefword*>wordWidgets;
    teRefTag(QWidget*parent=nullptr):teTagBase(parent){setStyle(teTag::normal);}
    teRefTag(std::shared_ptr<tetagcore>incore,QWidget*parent=nullptr):teTagBase(incore,parent){load();setStyle(teTag::normal);}
    ~teRefTag(){
    }
    virtual void readCore(std::shared_ptr<tetagcore>in_core)override;

    virtual void load()override;
    void clearWordWidgets()override;
    void self_giveback();
    void eraseCore(teTagList*list){
        list->erase(core);
    }
    void worddroped(teWordBase*in_word,int xpos)override;
}tereftag;

bool teRefTagCmp(tereftag*a,tereftag*b);
class teRefTagListWidget:public teTagListWidgetBase{
public:
    std::map<teRefTag*,teTagList*,bool(*)(tereftag*,tereftag*)>tags{teRefTagCmp};
    teTagListWidget*parentTagListWidget=nullptr;
    teRefTagListWidget(QWidget*parent):teTagListWidgetBase(12,parent){}
    teRefTagListWidget(teTagListWidget*parentlist,QWidget*parent):teTagListWidgetBase(12,parent),parentTagListWidget(parentlist){};
    ~teRefTagListWidget(){}
    void keyPressEvent(QKeyEvent *event) override;
    virtual int setSelectRange(teTagBase*in,bool ifclear=true)override;
    void setSelectAll();
    void clear(teTagList *in=nullptr)override;
    int findindex(std::shared_ptr<tetagcore>in_tag);
    int findindex(tetagbase*in_tag);

    virtual void tagDestroy(int index);
    virtual void tagDestroy(std::shared_ptr<tetagcore>tag=nullptr);
    virtual void tagErase(int index)override;
    virtual void tagErase(std::shared_ptr<tetagcore>tag=nullptr)override;
    std::map<teRefTag*,teTagList*,bool(*)(tereftag*,tereftag*)>::iterator findIterator(std::shared_ptr<tetagcore>tag);
    void findAndErase(std::shared_ptr<teTagCore>in_core);
    void Destroy(teRefTag*tag);
    virtual void connectTag(teTagBase* tag)override{
        teTagListWidgetBase::connectTag(tag);
        tag->core->teConnect(teCallbackType::destroy,this,(void (teRefTagListWidget::*)(std::shared_ptr<tetagcore>))&teRefTagListWidget::tagErase,tag->core);
    }
    void tagdroped(teTagBase *in_tag,int modifiers)override;
    virtual void tagInsertAbove(bool edit=true,std::shared_ptr<teTagCore>newtag=nullptr,int removeDuplicate=1)override;
    virtual void tagInsertBelow(bool edit=true,std::shared_ptr<teTagCore>newtag=nullptr,int removeDuplicate=1)override;
    void tagInsertAbove(bool edit,std::shared_ptr<tetagcore>newtag,teTagList*list,int removeDuplicate=1);
    void tagEdit(std::shared_ptr<teTagCore>tag, QString text,int removeDuplicate=1,bool ifemit=true)override;
    virtual void paste() override;
    virtual teTagBase* taginsert(int index,std::shared_ptr<tetagcore>in_tag,int removeDuplicate=-1,bool select=false)override;
    virtual teTagBase* taginsert(int index,std::shared_ptr<tetagcore>in_tag,teTagList*in_list,int removeDuplicate=-1,bool select=false);
};

#endif // TEREFTAGLISTWIDGET_H
