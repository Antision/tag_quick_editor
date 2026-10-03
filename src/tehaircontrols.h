/*
 * tehaircontrols.h - controls moved out of teeditor_derive.cpp, where they used
 * to be declared inside the editor constructors.
 */
#pragma once
#include "teeditorhelpers.h"

    struct hair_and_eyes_color_list : public teTagListControl{
        hair_and_eyes_color_list(colorsWidget*in_onEdit_widget,teTagListView*parentlist,QWidget*parent = nullptr,QString*styleSheet=nullptr)
            :teTagListControl(in_onEdit_widget,parentlist,parent,styleSheet,"color"){
            QPushButton*yellow_button=nullptr;
            auto&&color_buttons = ((colorsWidget*)onEdit_widget)->colors_buttongroup->buttons();
            for(QAbstractButton*btn:color_buttons)
                if(btn->text()==qsl("yellow")){
                    yellow_button=(QPushButton*)btn;
                    break;
                }
            QPushButton*hair_button=nullptr;
            auto&&hair_eye_buttons = ((colorsWidget*)onEdit_widget)->objectLayoutList.back().second->buttons();
            for(QAbstractButton*btn:hair_eye_buttons)
                if(btn->text()==qsl("hair")){
                    hair_button=(QPushButton*)btn;
                    break;
                }
            if(yellow_button&&hair_button)
                connect(hair_button,&QPushButton::toggled,yellow_button,[yellow_button](bool checked){if(checked)yellow_button->setText(qsl("blonde"));else yellow_button->setText(qsl("yellow"));});

            sc->setMinimumHeight(110);
        }

        bool filter(std::shared_ptr<teTag>in_tag)override{
            if(hairtag_types.contains(in_tag))
                on_hairtag_edited(in_tag);
            if(in_tag->retired)
                return false;
            if(in_tag->words.empty())return false;
            QString&&lastword=*in_tag->words.back();
            QString tagstring = *in_tag;
            if((lastword==qsl("hair")||lastword==qsl("eyes"))
                 &&
                 ((is_color(*in_tag).second>0)||tagstring==qsl("streaked hair")||tagstring==qsl("gradient hair"))){
                return true;
            }else if(tagstring.startsWith(qsl("gradient from"))||tagstring.endsWith(qsl("streaks")))
                return true;
            return false;
        }
        enum Type {
            Unknown=-1,
            BaseHair,
            StreakHair,
            GradientHair,
            rawStreakHair,
            rawGradientHair
        };
        QHash<std::shared_ptr<teTag>, Type> hairtag_types;
        Type classify_tag(std::shared_ptr<teTag> tag) {
            if(is_color(*tag).second==0||
                (!(tag->contains("hair")||tag->contains("gradient")||tag->words.size()<3)))
                return Unknown;
            if(tag->contains("with"))
                return StreakHair;
            else if(tag->contains("from"))
                return GradientHair;
            else if(tag->contains("streaked") || tag->contains("gradient"))
                return tag->contains("gradient") ?
                           rawGradientHair :
                           rawStreakHair;

            return BaseHair;
        }
        bool can_merge(std::shared_ptr<teTag> target,Type target_type, std::shared_ptr<teTag> source,Type source_type) {
            static bool matrix[5][5]{
                //basic   s    g    rs    rg
                {false,false,false,true,false},
                {false,false,false,true,false},
                {false,false,false,false,true},
                {true,true,false,false,false},
                {false,false,true,false,true}
            };
            return matrix[target_type][source_type];
        }
        std::shared_ptr<teTag> find_merge_target(std::shared_ptr<teTag> new_tag,Type type) {
            for(int i = layout->count()-2; i >=0; --i) {
                teRefTagWidget* widget = dynamic_cast<teRefTagWidget*>(layout->itemAt(i)->widget());
                std::shared_ptr<teTag> candidate;
                if(widget&&widget->core)
                    candidate = widget->core;
                else
                    continue;
                if(!hairtag_types.contains(candidate)) continue;
                if(candidate==new_tag)continue;
                if(candidate->retired)continue;

                if(can_merge(candidate,hairtag_types[candidate], new_tag,type)){
                    return candidate;
                }
            }
            return nullptr;
        }
        void perform_merge(std::shared_ptr<teTag> target, std::shared_ptr<teTag> source,Type source_type) {
            if(source_type == rawStreakHair||hairtag_types[target]==rawStreakHair) {
                merge_streaks(target, source,source_type==rawStreakHair);
                hairtag_types[target]=StreakHair;
            } else if(source_type == rawGradientHair||hairtag_types[target]==rawGradientHair) {
                merge_gradient(target, source,source_type==rawGradientHair);
                hairtag_types[target]=GradientHair;
            }else
                telog("[perform_merge]:couldn't merge tag");
        }
        void merge_streaks(std::shared_ptr<teTag> target, std::shared_ptr<teTag> source,bool source_to_target) {
            QStringList all_colors;
            if(source_to_target)
                all_colors = extract_colors(target)+extract_colors(source);
            else
                all_colors = extract_colors(source)+extract_colors(target);
            QString finalStr;

            finalStr+=all_colors[0]+" hair with ";
            int i =1;
            static QString space=" and ";
            for(;i<all_colors.size();++i){
                finalStr+=all_colors[i]+space;
            }
            finalStr.chop(4);
            finalStr+=" streaks";
            taglistwidget->tagEdit(target,finalStr);
        }
        void merge_gradient(std::shared_ptr<teTag> target, std::shared_ptr<teTag> source,bool source_to_target) {
            QStringList all_colors;
            if(source_to_target)
                all_colors = extract_colors(target)+extract_colors(source);
            else
                all_colors = extract_colors(source)+extract_colors(target);
            QString new_text = "gradient from " + all_colors.join(" to ");
            taglistwidget->tagEdit(target,new_text);
        }
        void link_hair(std::shared_ptr<teTag> tag){
            tag->teConnect(teCallbackType::edit_with_layout,this,&hair_and_eyes_color_list::on_hairtag_edited,tag);
            hairtag_types.insert(tag,classify_tag(tag));
        }
        void unlink_hair(std::shared_ptr<teTag> tag){
            if(hairtag_types.contains(tag)){
                tag->teDisconnect(this,teCallbackType::edit_with_layout);
                hairtag_types.remove(tag);
            }
        }
        void on_hairtag_edited(std::shared_ptr<teTag> edited_tag) {
            Type still_valid = classify_tag(edited_tag);
            if(still_valid==Unknown) {
                unlink_hair(edited_tag);
                return;
            }
            if(hairtag_types.contains(edited_tag))
                hairtag_types[edited_tag]=still_valid;
            std::shared_ptr<teTag> new_target = find_merge_target(edited_tag,still_valid);
            if(new_target) {
                edited_tag->retired=true;
                perform_merge(new_target, edited_tag,still_valid);
            }
        }
        virtual void tagEdit(teTagWidgetBase*tag,teWordWidgetBase*word)override{
            if(std::shared_ptr<teTag>core =tag->core; hairtag_types.contains(core)&&(hairtag_types[core]==StreakHair||hairtag_types[core]==GradientHair)){
                int maxsize=0;
                editingTag=tag;
                QString tagtext="";
                for(teWord*&word:core->words){
                    maxsize+=word->text.size()+1;
                }
                tagtext.reserve(maxsize);
                for(teWord*&word:core->words){
                    // Hide the words this editor shows for the tag; the widget a
                    // word core may carry belongs to the single image tag list.
                    for(teWordWidgetBase* shown : tag->findChildren<teWordWidgetBase*>())
                        shown->hide();
                    tagtext.append(word->text);
                    tagtext.append(' ');
                }
                lineedit->start(tagtext);
                tag->layout->insertWidget(0,lineedit);
                tag->clearWordWidgets();
                connect(lineedit,&suggestionLineEdit::editingFinished,this,&teRefTagListWidget::onLineEditStop,Qt::DirectConnection);
                lineedit->containerParent=this;
                lineedit->show();
                lineedit->moveSuggestionBox();
                lineedit->setFocus();
                lineedit->selectAll();
            }else{
                ifedit=true;
                onEdit_widget->input_and_show(tag->core);
                        }
        }
        void link(std::shared_ptr<teTag>in_tag)override{
            if(autoMerge&&MergeSwitch&&(!in_tag->contains("eyes"))&&
                (is_color(*in_tag).second>0)
                )
            {
                const Type new_type = classify_tag(in_tag);
                if(new_type==Unknown){
                    telog("[hair_and_eyes]:unable to distinguish hair tag's type");
                    goto mergeEnd;
                }
                std::shared_ptr<teTag> target_tag = find_merge_target(in_tag,new_type);

                if(target_tag) {
                    in_tag->retired=true;
                    perform_merge(target_tag,in_tag,new_type);
                    return;
                } else {
                    link_hair(in_tag);
                }
            }
            mergeEnd:
            linked_tags.insert(in_tag);
            in_tag->teConnect(teCallbackType::destroy,this,&teEditorControl::unlink,in_tag);
            teTagWidgetBase*widget =taginsert(-1,in_tag,taglistwidget->showing_list);
            extraWidgetPushBack(in_tag,widget);
        }

        void unlink(std::shared_ptr<teTag>in_tag)override{
            teDisconnect(in_tag.get());
            hairtag_types.remove(in_tag);
            linked_tags.erase(in_tag);
            tagErase(in_tag);
        }
        void extraWidgetPushBack(std::shared_ptr<teTag>in_tag,teTagWidgetBase*widget){
            if(hairtag_types.contains(in_tag)&&((hairtag_types[in_tag]==BaseHair)||(hairtag_types[in_tag]==rawStreakHair)||(hairtag_types[in_tag]==rawGradientHair))){
                int wordcount = in_tag->words.size();
                QPushButton*streaked_btn = new QPushButton("s",widget);
                streaked_btn->setStyleSheet(qsl(R"(font:8pt "Sonsolas")"));
                streaked_btn->setCheckable(true);
                streaked_btn->setFixedSize(15,15);
                for(int i =0;i<wordcount;++i)
                    if(*in_tag->words[i]==qsl("streaked"))
                        streaked_btn->setChecked(true);
                connect(streaked_btn,&QPushButton::clicked,widget,[in_tag, streaked_btn, this](bool ifchecked){
                    if(ifchecked){
                        if(in_tag->contains("streaked")){
                            streaked_btn->setChecked(false);
                            return;
                        }
                        if(auto* editorTag=tagWidgetFor(in_tag)) editorTag->insertWord(-2,qsl("streaked"));
                    }else{
                        int wordcount = in_tag->words.size();
                        for(int i =0;i<wordcount;++i)
                            if(*in_tag->words[i]==qsl("streaked"))
                            {if(auto* editorTag=tagWidgetFor(in_tag)) editorTag->destroyWord(i);--i;--wordcount;}
                    }
                    if(in_tag->retired)
                        taglistwidget->tagErase(in_tag);
                },Qt::DirectConnection);
                widget->insertExtraWidgets(this,streaked_btn);
                QPushButton*gradient_btn = new QPushButton("g",widget);
                gradient_btn->setStyleSheet(qsl(R"(font:8pt "Sonsolas")"));
                gradient_btn->setCheckable(true);
                gradient_btn->setFixedSize(15,15);
                for(int i =0;i<wordcount;++i)
                    if(*in_tag->words[i]==qsl("gradient"))
                        gradient_btn->setChecked(true);
                connect(gradient_btn,&QPushButton::clicked,widget,[in_tag, this, gradient_btn](bool ifchecked){
                    if(ifchecked){
                        if(in_tag->contains("gradient")){
                            gradient_btn->setChecked(false);
                            return;
                        }
                        if(auto* editorTag=tagWidgetFor(in_tag)) editorTag->insertWord(-2,qsl("gradient"));
                    }else{
                        int wordcount = in_tag->words.size();
                        for(int i =0;i<wordcount;++i)
                            if(*in_tag->words[i]==qsl("gradient"))
                            {if(auto* editorTag=tagWidgetFor(in_tag)) editorTag->destroyWord(i);--i;--wordcount;}
                    }
                    if(in_tag->retired)
                        taglistwidget->tagErase(in_tag);
                },Qt::DirectConnection);
                widget->insertExtraWidgets(this,gradient_btn);
            }
        }
    };

struct BangsList : public teTagListControl{
    std::shared_ptr<teTag>editcore=nullptr;
    BangsList(colorsWidget*in_onEdit_widget,teTagListView*parentlist,QWidget*parent = nullptr,QString*styleSheet=nullptr)
        :teTagListControl(in_onEdit_widget,parentlist,parent,styleSheet,"bangs"){
        QPushButton*curtained_button=nullptr;
        auto&&bangs_buttons = ((colorsWidget*)onEdit_widget)->objectLayoutList[1].second->buttons();
        for(QAbstractButton*btn:bangs_buttons)
            if(btn->text()==qsl("curtained")){
                curtained_button=(QPushButton*)btn;
                break;
            }
        QLabel*bangs_label=(QLabel*)((colorsWidget*)onEdit_widget)->objectLayoutList[2].first->itemAt(0)->widget();

        if(curtained_button&&bangs_label&&bangs_label->text()=="bangs")
            connect(curtained_button,&QPushButton::toggled,bangs_label,[bangs_label](bool checked){
                if(checked)bangs_label->setText(qsl("hair"));
                else bangs_label->setText(qsl("bangs"));
            });
        else telog("[BangsList::BangsList]could not find the \"bangs\" button or \"curtained\" button");
        sc->setMinimumHeight(110);
    }
    bool filter(std::shared_ptr<teTag>in_tag)override{
        if(in_tag->words.empty())return false;
        QString tagtext = *in_tag;
        QString&&lastword=*in_tag->words.back();
        if(lastword==qsl("bangs")
            ||tagtext==qsl("curtained hair")
            ||tagtext==qsl("hair between eyes")
            ||tagtext==qsl("hair over one eye")
            ||tagtext==qsl("hair over eyes")
            ){
            return true;
        }
        return false;
    }
    void extraWidgetPushBack(std::shared_ptr<teTag>in_tag,teTagWidgetBase*widget){}
};

struct ponytail_buttongroup : public teTagButtonGroup {
    ponytail_buttongroup(QWidget* parent = nullptr, QString* styleSheet_button = nullptr, QString* styleSheet_label = nullptr)
        :teTagButtonGroup({
            {qsl("ʃot"),qsl("short"),true,true,false},
            {qsl("low"),qsl("low"),true,true,false},
            {qsl("brd"),qsl("braided"),true,true,true},
            {qsl("side"),qsl("side"),true,true,true},
            {qsl("po"),qsl("pony"),true,false,false},
            {qsl("tw"),qsl("twin"),true,false,false},
            {qsl("tail"),qsl("tail"),false,true,false}
        })
    {layout->setSpacing(1);memset(kindOfPonytail,0,4);memset(kindOfTwintails,0,4);}
        enum {
            teshort,low,braided,side,pony,twin,tail
        };
        bool kindOfPonytail[4];
        std::shared_ptr<teTag>ponytailTag=nullptr;
        bool kindOfTwintails[4];
        const QStringList prefixes = {QString("short"),QString("low"),QString("braided"),QString("side")};
        std::shared_ptr<teTag>twintailsTag=nullptr;
    virtual void reform(int id)override {
        if (id==twin&&buttons[twin]->isChecked()) {
            buttons[pony]->setChecked(false);
            buttons[side]->setChecked(false);
        }else if(id==pony&&buttons[pony]->isChecked()){
            buttons[twin]->setChecked(false);
        }else if(id>side&&!(buttons[pony]->isChecked()||buttons[twin]->isChecked())){
            for(int i =0;i<pony;++i)
                buttons[i]->setChecked(false);
        }
        else if(id<pony&&buttons[id]->isChecked()&&!(buttons[pony]->isChecked()||buttons[twin]->isChecked())){
            buttons[pony]->setChecked(true);
        }
        if(buttons[twin]->isChecked()) allwidgets[tail].data=qsl("tails");
        else allwidgets[tail].data=qsl("tail");

        bool*thearray=nullptr;
        if(buttons[twin]->isChecked()){
            thearray=kindOfTwintails;
            clearTailMemeory(twin);
        }
        else if(buttons[pony]->isChecked()){
            thearray=kindOfPonytail;
            clearTailMemeory(pony);
        }
        if(thearray)
            for(int i=0;i<4;++i){
                thearray[i] = buttons[i]->isChecked();
            }
    }
    void onClicked(int id)override{
        reform(id);

        if(!buttons[twin]->isChecked()&&twintailsTag)
            taglistwidget->tagErase(twintailsTag);
        if(!buttons[pony]->isChecked()&&ponytailTag)
            taglistwidget->tagErase(ponytailTag);

        QString twintailsFinalString;
        for(int i=0;i<4;++i){
            if(kindOfTwintails[i])
                twintailsFinalString.append(prefixes[i]+' ');
        }
        if(!(twintailsFinalString.isEmpty()&&!buttons[twin]->isChecked())){
            twintailsFinalString+=qsl("twintails");
            if(twintailsTag)
                taglistwidget->tagEdit(twintailsTag,twintailsFinalString,1);
            else{
                std::shared_ptr<teTag> newtagcore = std::make_shared<teTag>(twintailsFinalString);
                twintailsTag=newtagcore;
                taglistwidget->tagInsertAbove(false,newtagcore,2);
            }
        } else if(!twintailsTag&&buttons[twin]->isChecked()){
            std::shared_ptr<teTag> newtagcore = std::make_shared<teTag>(qsl("twintails"));
            twintailsTag=newtagcore;
            taglistwidget->tagInsertAbove(false,newtagcore,2);
        }
        QString ponytailFinalString;
        for(int i=0;i<4;++i){
            if(kindOfPonytail[i])
                ponytailFinalString.append(prefixes[i]+' ');
        }
        if(!(ponytailFinalString.isEmpty()&&!buttons[pony]->isChecked())){
            ponytailFinalString+="ponytail";
            if(ponytailTag)
                taglistwidget->tagEdit(ponytailTag,ponytailFinalString,1,true);
            else{
                std::shared_ptr<teTag> newtagcore = std::make_shared<teTag>(ponytailFinalString);
                ponytailTag=newtagcore;
                taglistwidget->tagInsertAbove(false,newtagcore,2);
            }
        } else if(!ponytailTag&&buttons[pony]->isChecked()){
            std::shared_ptr<teTag> newtagcore = std::make_shared<teTag>("ponytail");
            ponytailTag=newtagcore;
            taglistwidget->tagInsertAbove(false,newtagcore,2);
        }

    }
    void unlink(std::shared_ptr<teTag>in_tag)override{
        if(in_tag==ponytailTag){
            clearTailMemeory(pony);
            ponytailTag.reset();
        }else if(in_tag==twintailsTag){
            clearTailMemeory(twin);
            twintailsTag.reset();
        }
        teTagButtonGroup::unlink(in_tag);
    }
    void clearTailMemeory(int tailEnum){
        if(tailEnum==pony){
            memset(kindOfPonytail,0,4);
        }else if(tailEnum==twin){
            memset(kindOfTwintails,0,4);
        }
    }
    void getPrefix(std::shared_ptr<teTag>in_tag,bool boolarray[4]){
        if(in_tag->words.count()<2)return;
        QStringList in_tag_words;
        for(teWord* wordptr: in_tag->words){
            in_tag_words.push_back(*wordptr);
        }
        std::sort(in_tag_words.begin(),in_tag_words.end());
        for(int i = 0;i<4;++i){
            auto it = std::lower_bound(in_tag_words.begin(),in_tag_words.end(),prefixes[i]);
            if(it==in_tag_words.end()) continue;
            else if((*it)[0]==prefixes[i][0]&&(*it)[1]==prefixes[i][1]){
                if(!boolarray[i]){
                    boolarray[i]=true;
                }
            }
        }
    }
    void setTagTextFromBoolArray(std::shared_ptr<teTag>in_tag,bool boolarray[4]){
        teWord*backword = in_tag->words.takeAt(in_tag->words.size()-1);
        in_tag->clear();
        for(int i =0;i<3;++i){
            if(boolarray[i]){
                teWord* newword =new teWord(prefixes[i]);
                in_tag->words.push_back(newword);
            }
        }
        in_tag->words.push_back(backword);
        // Only a tag list that owns a widget per word needs the widgets moved by
        // hand; the editors' tag lists rebuild their words from the core, which
        // is what edited_with_layout() below makes them do.
        if(teTagWidgetBase* editorTag = tagWidgetFor(in_tag)){
            if(editorTag->ownsWordWidgets()){
                // The tag widget owns its word widgets, so ask it for them instead
                // of reaching into the core (a word has no widget table any more).
                for(teWord* wc : in_tag->words){
                    if(teWordWidgetBase* wordWidget = editorTag->ensureWordWidgetFor(wc))
                        editorTag->layout->insertWidget(editorTag->layout->count()-2,wordWidget);
                }
            }
        }
        in_tag->edited_with_layout();
    }
    bool merge(std::shared_ptr<teTag>tag){
        if(*tag->words.back()==QString("twintails")){
            if(twintailsTag&&twintailsTag!=tag){
                getPrefix(tag,kindOfTwintails);
                tag->retired=true;
                taglistwidget->tagErase(tag);
                setTagTextFromBoolArray(twintailsTag,kindOfTwintails);
            }else if(!twintailsTag){
                twintailsTag=tag;
                getPrefix(tag,kindOfTwintails);
            }
            else if(twintailsTag==tag){
                clearTailMemeory(twin);
                getPrefix(tag,kindOfTwintails);
            }
        }else if(*tag->words.back()==QString("ponytail")){
            if(ponytailTag&&ponytailTag!=tag){
                getPrefix(tag,kindOfPonytail);
                tag->retired=true;
                taglistwidget->tagErase(tag);
                setTagTextFromBoolArray(ponytailTag,kindOfPonytail);
            }else if(!ponytailTag){
                ponytailTag=tag;
                getPrefix(tag,kindOfPonytail);
            }
            else if(ponytailTag==tag){
                clearTailMemeory(pony);
                getPrefix(tag,kindOfPonytail);
            }
        }
        refreshState();

        return tag->retired;
    }
    void resetPolicy()override{
        clearTailMemeory(pony);
        clearTailMemeory(twin);
        ponytailTag=nullptr;
        twintailsTag=nullptr;
    }
    bool filter(std::shared_ptr<teTag>tag)override{
        QString tagstring=*tag;
        int index = tagstring.lastIndexOf("tail");
        if(index<0)return false;
        else if(index==tagstring.size()-5)
            tagstring.chop(1);
        if(defaultFiltStrings.find(tagstring)!=defaultFiltStrings.end()){

            return !(autoMerge&&MergeSwitch&&merge(tag));
        }else return false;
    }
    void getDefaultFiltStrings()override{}
    void refreshState()override{
        buttons[twin]->setChecked((bool)twintailsTag);
        buttons[pony]->setChecked((bool)ponytailTag);
        for(int i=0;i<4;++i){
            buttons[i]->setChecked((bool)kindOfPonytail[i]||kindOfTwintails[i]);
        }
    }
};

struct HairLength_Buttongroup : public teTagButtonGroup {
    HairLength_Buttongroup(QWidget* parent = nullptr, QString* styleSheet_button = nullptr, QString* styleSheet_label = nullptr)
        :teTagButtonGroup({
            {qsl("very"),qsl("very"),true,true,false},
            {qsl("long"),qsl("long"),true,true,false},
            {qsl("short"),qsl("short"),true,true,false},
            {qsl("medium"),qsl("medium"),true,true,false},
            {qsl("hair"),qsl("hair"),false,true,false},
        })
        {
        layout->setSpacing(1);
            long_hair_iterator=defaultFiltStrings.find(qsl("long hair"));
        very_long_hair_iterator = defaultFiltStrings.find(qsl("very long hair"));
        }
    virtual void reform(int id)override {
        enum {
            very,telong,teshort,medium,hair
        };
        if (id<hair&&id>very) {
            if(buttons[id]->isChecked()){
                buttons[(id-1+1)%3+1]->setChecked(false);
                buttons[(id-1+2)%3+1]->setChecked(false);
            }else {
                if(!buttons[telong]->isChecked()&&!buttons[teshort]->isChecked())
                    buttons[very]->setChecked(false);
            }
        }
        if(id==medium&&buttons[medium]->isChecked()){
            buttons[very]->setChecked(false);
        }else if(id==very&&buttons[very]->isChecked()&&!buttons[teshort]->isChecked()){
            buttons[medium]->setChecked(false);
            buttons[telong]->setChecked(true);
        }
    }

    std::shared_ptr<teTag>long_hair_ptr=nullptr;
    std::shared_ptr<teTag>very_long_hair_ptr=nullptr;
    std::unordered_set<QString>::iterator long_hair_iterator;
    std::unordered_set<QString>::iterator very_long_hair_iterator;

    std::unordered_set<QString>::iterator filter_it;
    bool filter(std::shared_ptr<teTag>tag)override{
        if(filter_it = defaultFiltStrings.find(*tag);filter_it!=defaultFiltStrings.end()){
            if(autoMerge&&MergeSwitch){
                if(filter_it==long_hair_iterator){
                    if(very_long_hair_ptr==tag){
                        very_long_hair_ptr=nullptr;
                    }
                    if(very_long_hair_ptr){
                        tag->retired=true;
                        long_hair_ptr=nullptr;
                        return false;
                    }
                    else
                        long_hair_ptr=tag;
                }
                else if(filter_it==very_long_hair_iterator){
                    if(long_hair_ptr==tag)
                        long_hair_ptr=nullptr;
                    very_long_hair_ptr=tag;
                    if(long_hair_ptr){
                        if(tag!=long_hair_ptr)
                            taglistwidget->tagErase(long_hair_ptr);
                        long_hair_ptr=nullptr;
                    }
                }
            }
            return true;
        }else return false;
    }
    void unlink(std::shared_ptr<teTag>tag)override{
        if(tag==long_hair_ptr)long_hair_ptr=nullptr;
        else if(tag==very_long_hair_ptr)very_long_hair_ptr=nullptr;
        teTagButtonGroup::unlink(tag);
    }
    void resetPolicy()override{
        long_hair_ptr=nullptr;
        very_long_hair_ptr=nullptr;
    }
};
