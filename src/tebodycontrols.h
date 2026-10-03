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
        {layout->setSpacing(1);breasts_it=defaultFiltStrings.find("breasts");}
    virtual void reform(int id)override {
        enum {
            tesmall,temedium,telarge,tebreasts
        };
        // The three sizes exclude each other; the "breasts" label does not.
        if (id<tebreasts&&buttons[id]->isChecked())
            uncheckOthers(id,{tesmall,temedium,telarge});
    }

    std::shared_ptr<teTag>breasts_ptr=nullptr;
    std::unordered_set<QString>::iterator breasts_it;
    std::unordered_set<QString>::iterator filter_it;
    bool filter(std::shared_ptr<teTag> tag) override
    {
        filter_it = defaultFiltStrings.find(*tag);
        if (filter_it == defaultFiltStrings.end())
            return false;

        if (!(autoMerge && MergeSwitch))
            return true;

        const bool isBreastsToken = (filter_it == breasts_it);
        const bool isCurrentBreastsTag = (breasts_ptr && breasts_ptr == tag);

        if (isBreastsToken) {
            // 当前这个 tag 就是 breasts / 由 breasts 维护的对象
            // 不要把“正在编辑中的同一个对象”再删掉
            if (!linked_tags.empty()) {
                auto linked = *linked_tags.begin();
                if (linked && !linked->words.empty()
                    && linked->words.begin() != linked->words.end()
                    && (*linked->words.begin())->text != qsl("breasts")) {
                    tag->retired=true;
                    breasts_ptr.reset();
                    return false;
                }
            }

            breasts_ptr = tag;
            return true;
        }

        // 不是 breasts，但如果它正好就是当前控制器正在编辑的那个对象，
        // 不能 erase 自己，否则会在回调链中把对象拆掉。
        if (breasts_ptr && !isCurrentBreastsTag) {
            taglistwidget->tagErase(breasts_ptr);
            breasts_ptr.reset();
        }

        return true;
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

struct legUp_Buttongroup : public teTagButtonGroup {
    legUp_Buttongroup(QWidget* parent = nullptr, QString* styleSheet_button = nullptr, QString* styleSheet_label = nullptr)
        :teTagButtonGroup({
            {qsl("one"),qsl("one"),true,true,false},
            {qsl("leg(s)"),qsl("leg"),true,true,false},
            {qsl("up"),qsl("up"),false,true,false},})
        {}
    virtual void reform(int id)override {
        enum {
            one,leg
        };
    if(id==leg)
    buttons[one]->setChecked(false);
    if(buttons[one]->isChecked()){
        allwidgets[leg].data=qsl("leg");
        if(id==one)buttons[leg]->setChecked(true);
    }
    else{
        allwidgets[leg].data=qsl("legs");
    }
}

bool filter(std::shared_ptr<teTag>in)override{
    QVector<teWord*>&words = in->words;
    if(words.size()<2)return false;
    if(words.at(words.size()-2)->text.startsWith(qsl("leg"))&&in->words.back()->text==qsl("up"))
        return true;
    return false;
}
};
