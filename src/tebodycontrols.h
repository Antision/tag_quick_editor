/*
 * tebodycontrols.h - controls moved out of teeditor_derive.cpp, where they used
 * to be declared inside the editor constructors.
 */
#pragma once
#include "teeditorhelpers.h"

struct Mature_Buttongroup : public teTagButtonGroup {
    Mature_Buttongroup(QWidget* parent = nullptr, QString* styleSheet_button = nullptr, QString* styleSheet_label = nullptr)
        :teTagButtonGroup({
              {qsl("mature"),qsl("mature"),false,true,false},
              {qsl("male"),qsl("male"),true,true,false},
              {qsl("female"),qsl("female"),true,true,false},
        })
        {layout->setSpacing(1);}
void onClicked(int id)override{
    if(!taglistwidget->showing_list)return;
    ifrefreshState=false;
    enum{male,female};
    static QString Str[2]={qsl("male"),qsl("female")};
    QString FinalStr=qsl("mature ")+Str[id];

    if(buttons[id]->isChecked()){
        std::shared_ptr<teTag>newtagcore =std::make_shared<teTag>(FinalStr);
        link(newtagcore);
        taglistwidget->tagInsertAbove(false, newtagcore);
    }else{
        for(std::shared_ptr<teTag>tc:linked_tags)
            if((QString)*tc==FinalStr){
                taglistwidget->tagErase(tc);
                break;
            }
    }
    ifrefreshState=true;
    refreshState();
    edited();
}
};

struct Breast_Buttongroup : public teTagButtonGroup {
    Breast_Buttongroup(QWidget* parent = nullptr, QString* styleSheet_button = nullptr, QString* styleSheet_label = nullptr)
        :teTagButtonGroup({
              {qsl("small"),qsl("small"),true,true,false},
              {qsl("medium"),qsl("medium"),true,true,false},
              {qsl("large"),qsl("large"),true,true,false},
              // "breasts" 现在是一个真正的按钮:单独点亮 = 捕获裸 "breasts"
              // (没有更具体的描述);它与尺寸词一起点亮时,合并后的
              // "<size> breasts" 正好点亮两个按钮。
              {qsl("breasts"),qsl("breasts"),true,true,false},
        })
        {
            layout->setSpacing(1);
            // 尺寸互斥不再走 rules/reform:单人时由 onClicked() 手工执行
            // (多人 girls>1 时互斥放宽,三个尺寸各写各的 tag)。
        }
        enum { b_small=0, b_medium=1, b_large=2, b_breasts=3 };

    std::shared_ptr<teTag>breasts_ptr=nullptr;
    /// girls > 1(只数 girl,不含 boy/other)时尺寸互斥放宽。纯查询,现算。
    bool multiTagMode() const override {
        return countNounTags(taglistwidget?taglistwidget->showing_list:nullptr,
                             QStringLiteral("girl"))>1;
    }
    /// 一个按钮负责的 tag 文本("small breasts" / "breasts")。
    QString tagTextForButton(int buttonId) const override {
        switch(buttonId){
        case b_small:   return qsl("small breasts");
        case b_medium:  return qsl("medium breasts");
        case b_large:   return qsl("large breasts");
        case b_breasts: return qsl("breasts");
        }
        return QString();
    }

    /// Is this one of the tags this control writes? Pure.
    ///
    /// 手写判定,不走 defaultFiltStrings:"breasts" 成为按钮后,那里的组合
    /// 会把裸尺寸词("small")也当成组内 tag。
    bool filter(std::shared_ptr<teTag> tag) const override
    {
        if(!tag)
            return false;
        const QString text=*tag;
        if(text==qsl("breasts"))
            return true;
        for(const QString& size:{qsl("small"),qsl("medium"),qsl("large")})
            if(text==size+qsl(" breasts"))
                return true;
        return false;
    }

    /**
     * @brief "breasts" gives way to a concrete size ("small breasts").
     *
     * Same rule as the ears group: the generic tag is dropped when a specific one
     * exists, and a specific one removes the generic tag. It used to live in
     * filter(), where it also had to be careful not to erase the tag it was
     * called for. 多人放宽只作用于尺寸互斥;通称的自动合并(用户要求)保持。
     */
    bool absorb(std::shared_ptr<teTag> tag)
    {
        if(!(autoMerge && MergeSwitch))
            return false;

        if (static_cast<QString>(*tag) == qsl("breasts")) {
            if (!linked_tags.empty()) {
                auto linked = *linked_tags.begin();
                if (linked && !linked->words.empty()
                    && (*linked->words.begin())->text != qsl("breasts")) {
                    // "breasts" gives way to the size already there - unless the two
                    // were merged once and undo brought both back.
                    if(!mergeAllowed(*linked,*tag)){
                        breasts_ptr = tag;
                        return false;
                    }
                    markMerged(*linked,*tag);
                    tag->retired=true;
                    breasts_ptr.reset();
                    return true;
                }
            }
            breasts_ptr = tag;
            return false;
        }

        // 不是 breasts，但如果它正好就是当前控制器正在编辑的那个对象，
        // 不能 erase 自己，否则会在回调链中把对象拆掉。
        if (breasts_ptr && breasts_ptr != tag) {
            std::shared_ptr<teTag> generic = breasts_ptr;
            breasts_ptr.reset();
            if(mergeAllowed(*tag,*generic)){
                markMerged(*tag,*generic);
                if(taglistwidget)
                    taglistwidget->retire(generic);
            }else
                breasts_ptr = generic;          // both stay: see above
        }
        return false;
    }

    /**
     * @brief breasts 组自己的点击逻辑。
     *
     * 基类的"组合成一个词串"在这里不成立:breasts 现在是按钮,词串组合会把
     * 通称和尺寸重复拼接("small breasts breasts")。每个按钮各写一条整 tag:
     *   - 尺寸点亮 = 写 "<size> breasts";单人时先清掉其它尺寸的 tag(互斥),
     *     通称 tag 不动(absorb 会把它并进新 tag);多人(girls>1)不清,
     *     三个尺寸可以共存。
     *   - 尺寸点灭 = 只删自己的 tag(列表违反互斥时也只删自己,顺从列表)。
     *   - 通称点亮 = 写裸 "breasts",已有的尺寸会把它合并进去。
     *   - 通称点灭 = 每个尺寸 tag 都含 "breasts" 这个词,整组一起删除
     *     (快速清空:单人删掉唯一的 "<size> breasts",多人把所有尺寸删光)。
     */
    void onClicked(int id)override{
        if(!taglistwidget->showing_list)return;
        if(id<0||id>=buttons.size())return;
        ifrefreshState=false;
        if(!buttons[id]->isChecked()){
            if(id==b_breasts){
                while(!linked_tags.empty())
                    taglistwidget->tagErase(*linked_tags.begin());
            }else
                eraseTagsForButton(id);
        }else{
            if(id!=b_breasts&&!multiTagMode()){
                for(int other=0;other<3;++other)
                    if(other!=id)
                        eraseTagsForButton(other);
            }
            insertTagForButton(id);
        }
        ifrefreshState=true;
        refreshState();
        edited();
    }

    TagHandling read(std::shared_ptr<teTag> tag)override{
        if(!tag)
            return TagHandling::NotMine;
        if(tag->retired){
            if(taglistwidget)
                taglistwidget->retire(tag);
            return TagHandling::Consumed;
        }
        if(!filter(tag))
            return TagHandling::NotMine;
        if(absorb(tag)){
            if(taglistwidget)
                taglistwidget->retire(tag);
            return TagHandling::Consumed;
        }
        if(taglistwidget)
            taglistwidget->ensureWidgetFor(tag);
        link(tag);
        if(tag->retired){
            if(taglistwidget)
                taglistwidget->retire(tag);
            return TagHandling::Consumed;
        }
        return TagHandling::Linked;
    }
    void unlink(std::shared_ptr<teTag>tag)override{
        if(tag==breasts_ptr)breasts_ptr.reset();
        teTagButtonGroup::unlink(tag);
    }
    void resetPolicy()override{
        breasts_ptr.reset();
    }
    void getDefaultFiltStrings()override{};
};

