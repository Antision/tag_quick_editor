/*
 * tefacecontrols.h - controls moved out of teeditor_derive.cpp, where they used
 * to be declared inside the editor constructors.
 */
#pragma once
#include "teeditorhelpers.h"

struct smile_buttongroup : public teTagButtonGroup {
    smile_buttongroup(QWidget* parent = nullptr, QString* styleSheet_button = nullptr, QString* styleSheet_label = nullptr)
        :teTagButtonGroup({ {qsl("light"),qsl("light"),true},{qsl("naughty"),qsl("naughty"),true},{qsl("seductive"),qsl("seductive"),true},{qsl("smile"),qsl("smile"),true} }) {layout->setSpacing(1);}
    virtual void reform(int id)override {
        if (!buttons[3]->isChecked()&&id==3) {
            buttons[0]->setChecked(false);
            buttons[1]->setChecked(false);
            buttons[2]->setChecked(false);
        }
        else if(!buttons[3]->isChecked()){
            buttons[3]->setChecked(true);
        }
    }
};

struct mouth_buttongroup : public teTagButtonGroup {
    mouth_buttongroup(QWidget* parent = nullptr, QString* styleSheet_button = nullptr, QString* styleSheet_label = nullptr)
    :teTagButtonGroup({ {qsl("open"),qsl("open"),true},{qsl("closed"),qsl("closed"),true},{qsl("mouth"),qsl("mouth"),false} })
    {layout->setSpacing(1);}
    void reform(int id)override{
        // "open" and "closed" exclude each other; the label button does not.
        if(buttons[id]->isChecked())
            uncheckOthers(id,{0,1});
    }
};

struct ClosedEyes_Buttongroup : public teTagButtonGroup {
    ClosedEyes_Buttongroup(QWidget* parent = nullptr, QString* styleSheet_button = nullptr, QString* styleSheet_label = nullptr)
        :teTagButtonGroup({
            {qsl("half-"),qsl("half-"),true,false,false},
            {qsl("closed"),qsl("closed"),true,true,false},
            {qsl("eyes"),qsl("eyes"),false,true,false},
        })
        {layout->setSpacing(1);}
        enum {
            half,closed,eyes
        };
        virtual void reform(int id)override {
            if (id==half&&buttons[id]->isChecked()) {
                buttons[closed]->setChecked(true);
            }else if(id==closed&&!buttons[closed]->isChecked()) {
                buttons[half]->setChecked(false);
            }
        }
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
        }){}
        virtual void reform(int id)override {
        // The seven ear buttons are mutually exclusive.
        if(buttons[id]->isChecked())
            uncheckOthers(id,{0,1,2,3,4,5,6});}
        std::shared_ptr<teTag>animal_ears_ptr;
        bool explicit_ear=false;
bool filter(std::shared_ptr<teTag>in)override{
    QVector<teWord*>&words = in->words;
    if(words.size()!=2)return false;
    if(words.back()->text!=qsl("ears"))return false;
    if(words.front()->text==qsl("animal")){
        if(explicit_ear){
            in->retired=true;
            animal_ears_ptr=nullptr;
            return false;
        }
        else
            animal_ears_ptr=in;
    }
    else{
        if(animal_ears_ptr){
            if(in!=animal_ears_ptr)
                taglistwidget->tagErase(animal_ears_ptr);
            animal_ears_ptr=nullptr;
        }
        explicit_ear=true;
    }
    return true;
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
