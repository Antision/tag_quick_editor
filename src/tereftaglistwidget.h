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
    TagListView*parentTagListWidget=nullptr;
    teRefTagListWidget(QWidget*parent):teTagListWidgetBase(12,parent){updateAdaptiveHeight();}
    teRefTagListWidget(TagListView*parentlist,QWidget*parent):teTagListWidgetBase(12,parent),parentTagListWidget(parentlist){updateAdaptiveHeight();}
    ~teRefTagListWidget(){}
    void keyPressEvent(QKeyEvent *event) override;
    /**
     * @brief Tab 不能被焦点链先吃掉。
     *
     * QWidget::event() 在 Tab 上先调 focusNextPrevChild(),返回 true 就直接换焦点、
     * 根本走不到 keyPressEvent —— 于是"Tab 手动合并"永远没反应,而 Shift+M
     * (不参与焦点导航)却正常。支持手动合并的列表把 Tab 留给自己的 keyPressEvent。
     */
    bool focusNextPrevChild(bool next) override;
    /**
     * @brief What the "+" button of this list does.
     *
     * Ctrl+W is the keyboard form of the same gesture (the user asked for it):
     * the list that owns an add-widget control overrides this to open it, and a
     * list without a "+" does nothing.
     */
    virtual void addButtonClicked(){}
    /**
     * @brief 「insert above / below」:打开新增窗口,新 tag 插到当前位置的上/下。
     *
     * 基类没有新增窗口,什么也不做;编辑器里的列表(teTagListControl)覆写它。
     */
    virtual void insertRequested(bool below){ Q_UNUSED(below); }
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
    /**
     * @brief 自适应高度:1..3 条 tag 时正好占它们的高度,超过 3 条按 3 条算。
     *
     * 高度 = 行数 × 一条 tag 的高度 + 6px。行高取第一个可见 ref widget 的
     * sizeHint(),所以改字号不用同步改这里。列表原来固定 110px(每个宿主控件
     * 各写一遍 sc->setMinimumHeight(110)),两条 tag 的列表也占那么高。
     */
    void updateAdaptiveHeight();
    /// 重新显示时再量一次:页面重建前加进去的 tag 那时还没有真实高度。
    void showEvent(QShowEvent* event) override;
    /// 真正量高度的那一步(updateAdaptiveHeight() 把它推到下一轮事件循环)。
    void applyAdaptiveHeight();
    bool m_heightUpdatePending=false;
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
    /**
     * @brief 剪切 = 复制 + 从所属列表删除。
     *
     * 基类的 cut() 走 tagErase()——那只是把 ref widget 收回池子,tag 本体
     * 还留在 tag list 里(本次修的 bug)。tagDestroy() 走 Destroy():从
     * taglistwidget->showing_list 删 core,destroy 回调再把 widget 丢掉,
     * 单选与多选都成立。
     */
    void cut()override{
        QApplication::clipboard()->setText(getSelectText());
        tagDestroy();
    }
    /// 右键菜单的 delete:同 Ctrl+D,从所属列表删除(不是只丢 widget)。
    void deleteSelection()override{ tagDestroy(); }
    virtual teTagWidgetBase* taginsert(int index,std::shared_ptr<teTag>in_tag,int removeDuplicate=-1,bool select=false)override;
    virtual teTagWidgetBase* taginsert(int index,std::shared_ptr<teTag>in_tag,teTagList*in_list,int removeDuplicate=-1,bool select=false);
};

#endif // TEREFTAGLISTWIDGET_H
