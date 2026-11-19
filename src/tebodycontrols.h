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
              {qsl("breasts"),qsl("breasts"),false,true,false},
        })
        {
            layout->setSpacing(1);
            // The three sizes exclude each other; the "breasts" label does not.
            rules.exclusive = {{0,1,2}};
        }

    std::shared_ptr<teTag>breasts_ptr=nullptr;
    /// Is this one of the tags this control writes? Pure.
    bool filter(std::shared_ptr<teTag> tag) const override
    {
        if(!tag)
            return false;
        return defaultFiltStrings.find(*tag)!=defaultFiltStrings.end();
    }

    /**
     * @brief "breasts" gives way to a concrete size ("small breasts").
     *
     * Same rule as the ears group: the generic tag is dropped when a specific one
     * exists, and a specific one removes the generic tag. It used to live in
     * filter(), where it also had to be careful not to erase the tag it was
     * called for.
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

