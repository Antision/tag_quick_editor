/*
 * tefacecontrols.h - controls moved out of teeditor_derive.cpp, where they used
 * to be declared inside the editor constructors.
 */
#pragma once
#include "teeditorhelpers.h"

struct smile_buttongroup : public teTagButtonGroup {
    smile_buttongroup(QWidget* parent = nullptr, QString* styleSheet_button = nullptr, QString* styleSheet_label = nullptr)
        :teTagButtonGroup({ {qsl("light"),qsl("light"),true},{qsl("naughty"),qsl("naughty"),true},{qsl("seductive"),qsl("seductive"),true},{qsl("smile"),qsl("smile"),true} })
    {
        layout->setSpacing(1);
        // "light", "naughty" and "seductive" only describe a smile that is there:
        // each of them implies the "smile" button, and turning "smile" off takes
        // them with it (that is the same rule read backwards).
        rules.implies = {{0,3},{1,3},{2,3}};
    }
};

struct mouth_buttongroup : public teTagButtonGroup {
    mouth_buttongroup(QWidget* parent = nullptr, QString* styleSheet_button = nullptr, QString* styleSheet_label = nullptr)
    :teTagButtonGroup({ {qsl("open"),qsl("open"),true},{qsl("closed"),qsl("closed"),true},{qsl("mouth"),qsl("mouth"),false} })
    {
        layout->setSpacing(1);
        // "open" and "closed" exclude each other; the label is not a button.
        rules.exclusive = {{0,1}};
    }
};

struct ClosedEyes_Buttongroup : public teTagButtonGroup {
    ClosedEyes_Buttongroup(QWidget* parent = nullptr, QString* styleSheet_button = nullptr, QString* styleSheet_label = nullptr)
        :teTagButtonGroup({
            {qsl("half-"),qsl("half-"),true,false,false},
            {qsl("closed"),qsl("closed"),true,true,false},
            {qsl("eyes"),qsl("eyes"),false,true,false},
        })
        {
            layout->setSpacing(1);
            // "half-" is a prefix of "closed": it implies that button, and
            // unchecking "closed" unchecks it (the table reads the statement
            // backwards as well).
            rules.implies = {{half,closed,{}}};
        }
        enum {
            half,closed,eyes
        };
        void refreshState()override{
            bool halfb=false,closedb=false;
            for(std::shared_ptr<teTag>tc:linked_tags)
                if(tc->words[0]->text==qsl("half-closed")){
                    halfb=true;
                    closedb=true;
                }
                else if(tc->words[0]->text==qsl("closed"))
                    closedb=true;
            buttons[half]->setChecked(halfb);
            buttons[closed]->setChecked(closedb);
        }
};

struct Ears_Buttongroup : public teTagButtonGroup {
    Ears_Buttongroup(QWidget* parent = nullptr, QString* styleSheet_button = nullptr, QString* styleSheet_label = nullptr)
        :teTagButtonGroup(
            {{qsl("🐱"),qsl("cat"),true,true,false},
            {qsl("🐰"),qsl("rabbit"),true,true,false},
            {qsl("🦊"),qsl("fox"),true,true,false},
            {qsl("🐶"),qsl("dog"),true,true,false},
             {qsl("🐺"),qsl("wolf"),true,true,false},
             {qsl("🐴"),qsl("horse"),true,true,false},
             {qsl("pointy"),qsl("pointy"),true,true,false},
             {qsl("ears"),qsl("ears"),false,true,false},
        })
        {
            // The seven ear buttons are mutually exclusive.
            rules.exclusive = {{0,1,2,3,4,5,6}};
        }
        std::shared_ptr<teTag>animal_ears_ptr;
        bool explicit_ear=false;
/// "X ears" - pure: the "animal ears" fallback is absorb()'s business.
bool filter(std::shared_ptr<teTag> in) const override{
    if(!in||in->words.size()!=2)return false;
    return in->words.back()->text==qsl("ears");
}

/**
 * @brief "animal ears" gives way to an explicit kind ("cat ears").
 *
 * The generic tag is dropped when a specific one already exists, and adding a
 * specific one removes the generic tag - which is what filter() used to do while
 * it was answering a question that is supposed to have no side effects.
 */
bool absorb(std::shared_ptr<teTag> in){
    if(in->words.size()!=2)
        return false;
    if(in->words.front()->text==qsl("animal")){
        if(explicit_ear){
            in->retired=true;
            animal_ears_ptr=nullptr;
            return true;
        }
        animal_ears_ptr=in;
        return false;
    }
    if(animal_ears_ptr){
        std::shared_ptr<teTag> generic = animal_ears_ptr;
        animal_ears_ptr=nullptr;
        if(generic!=in&&taglistwidget)
            taglistwidget->retire(generic);
    }
    explicit_ear=true;
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
void resetPolicy()override{
    animal_ears_ptr=nullptr;
    explicit_ear=false;
}
void unlink(std::shared_ptr<teTag>tag)override{
    if(tag==animal_ears_ptr)animal_ears_ptr=nullptr;
    teTagButtonGroup::unlink(tag);
}
void getDefaultFiltStrings()override{}
};
