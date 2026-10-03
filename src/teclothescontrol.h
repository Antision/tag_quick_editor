/*
 * teclothescontrol.h - the clothes tag list control and its helpers.
 *
 * Moved out of teeditor_derive.cpp, where the whole clothes editor used to be
 * declared inside the constructor of teEditor_clothes. Nothing but the name
 * changed: what was a local struct is a file scope class now.
 */
#pragma once
#include "teeditorhelpers.h"

// Clothes types, grouped by the body area they belong to. The groups are only
// a visual aid: every button shares one exclusive group (built in
// teEditor_clothes), so a clothes tag still has exactly one type word.
inline QStringList headAndNeckClothesTypes{
    qsl("headdress"),qsl("headwear"),qsl("hat"),qsl("hood"),
    qsl("hairclip"),qsl("hairband")
};
inline QStringList upperBodyClothesTypes{
    qsl("shirt"),qsl("jacket"),qsl("coat"),qsl("hoodie"),qsl("sweater"),
    qsl("bra"),qsl("apron"),qsl("uniform")
};
inline QStringList lowerBodyClothesTypes{
    qsl("skirt"),qsl("shorts"),qsl("pants"),qsl("leotard"),
    qsl("thighhighs"),qsl("pantyhose"),qsl("socks")
};
inline QStringList underwearClothesTypes{
    qsl("underwear"),qsl("panties"),qsl("swimsuit"),qsl("bikini")
};
inline QStringList fullBodyClothesTypes{
    qsl("dress"),qsl("kimono")
};
inline QStringList feetClothesTypes{
    qsl("shoes"),qsl("boots"),qsl("footwear")
};
inline QStringList accessoryClothesTypes{
    qsl("bow"),qsl("ribbon"),qsl("bowtie"),qsl("gloves"),qsl("sleeves"),
    qsl("wings"),qsl("glasses"),qsl("horns"),qsl("bag")
};

/// Every word the clothes editor recognises as a garment type.
inline QStringList allClothesTypes = QStringList{}
    << headAndNeckClothesTypes << upperBodyClothesTypes << lowerBodyClothesTypes
    << underwearClothesTypes << fullBodyClothesTypes << feetClothesTypes
    << accessoryClothesTypes;

/// Generic garment words the user does not want, and the specific words they are
/// absorbed by. A tag that says "underwear" or "footwear" is merged into an
/// existing tag of the same family, and the *specific* spelling survives.
inline QHash<QString,QSet<QString>> genericClothesTypes{
    {qsl("underwear"),{qsl("panties")}},
    {qsl("footwear"),{qsl("shoes"),qsl("boots")}},
};

/// True when the two type words mean the same garment, i.e. may not both exist in
/// one tag list. A generic word matches every specific word of its family.
inline bool sameClothesType(const QString& a,const QString& b){
    if(a==b)
        return true;
    for(auto it=genericClothesTypes.constBegin();it!=genericClothesTypes.constEnd();++it){
        if(it.key()==a&&it.value().contains(b))
            return true;
        if(it.key()==b&&it.value().contains(a))
            return true;
    }
    return false;
}

/// True when `type` is one of the generic words (which never survive a merge).
bool isGenericClothesType(const QString& type){
    return genericClothesTypes.contains(type);
}
std::multimap<QString, QString> prefix_back{
    {qsl("dress"),qsl("wedding")},
    {qsl("waist"),qsl("apron")},
    {qsl("bikini"),qsl("one-piece")},
    {qsl("swimsuit"),qsl("one-piece")},
    {qsl(""),qsl("fishnet")},
    {qsl(""),qsl("sleeveless")},
    { qsl("dress"),qsl("china") }
};
std::multimap<QString, QString> prefix_front{
    {qsl(""),qsl("torn")},
    {qsl(""),qsl("detached")},
    {qsl(""),qsl("unworn")},
    {qsl(""),qsl("short")},
    {qsl(""),qsl("long")},
    {qsl(""),qsl("frilled")}
};
QVector<QSet<QString>> exclusiveModifierGroups{

};

/// Prepositions that turn a tag into an interaction ("reaching through
/// panties", "hand in panties") instead of a piece of clothing.
///
/// This is deliberately a *small* list: it is the exclusion dictionary asked
/// for, and a word such as "with" is missing on purpose so that legitimate
/// garment tags ("dress with bow") keep working.

/// True when a word before the trailing type word is a preposition, i.e. the
/// tag describes what someone does with a garment rather than the garment.
inline bool looksLikeActionPhrase(const teTag& tag){
    const int last = tag.words.size()-1;
    for(int i=0;i<last;++i)
        if(preposwords.contains(tag.words[i]->text))
            return true;
    return false;
}

    struct ClothesList : public teTagListControl {
public:
        std::shared_ptr<teTag> clothes_editing=nullptr;
        struct teClothes:public teObject{
            ClothesList*parentList = nullptr;
            /**
             * @brief True once this wrapper has been retired.
             *
             * Retiring means "removed from all_clothes, delete it later". The
             * deletion itself has to be deferred: readCore() emits
             * edited_with_layout(), whose slot (reReadClothes) uses to delete the
             * wrapper - and readCore() then kept using `this`, so `parentList`
             * was read from freed memory (the crash in teeditor_derive.cpp:1768).
             * Every entry point checks this flag instead.
             */
            bool dead=false;
            teTagListView*parentTagListWidget=nullptr;
            std::shared_ptr<teTag>core=nullptr;
            QVector<teWord*>colors;
            QVector<teWord*>front_adjectives;
            QVector<teWord*>adjectives;
            QVector<teWord*>back_adjectives;
            QVector<teWord*>exclusiveModifiers;
            QString type;
            teClothes(std::shared_ptr<teTag>core,ClothesList*parent,teTagListView*in_parentTagListWidget):parentList(parent),parentTagListWidget(in_parentTagListWidget){
                readCore(core);
                core->teConnect(teCallbackType::edit,this,&teClothes::reReadClothes,core);
                core->teConnect(teCallbackType::edit_with_layout,this,&teClothes::reReadClothes,core);
            }
            // Deliberately no constructor without a parentList: every method here
            // goes through it, and a wrapper built without one was exactly the
            // dangling-parentList crash this class used to have.
            ~teClothes(){
                // Drop the callbacks the tag core holds for this wrapper, so a
                // deleted wrapper can never be called again.
                teDisconnect();
            }

            void reReadClothes(std::shared_ptr<teTag>tag){
                if(dead)
                    return;
                if(tag==parentList->clothes_editing)return;
                if(tag->retired)
                    return;
                teClothes*tagClothes = nullptr;
                if(tagClothes=parentList->findClothes(tag);!tagClothes){
                    return;
                }
                parentList->all_clothes.erase(tagClothes);
                // Snapshot: merge() below can retire other wrappers (it goes
                // through readCore() -> edited_with_layout() -> reReadClothes),
                // which would invalidate an iterator into the set.
                const QVector<teClothes*> others(parentList->all_clothes.begin(),
                                                 parentList->all_clothes.end());
                for(teClothes*clz:others){
                    if(clz==this||clz->dead||!parentList->all_clothes.count(clz))
                        continue;
                    if(clz->merge(tag)){
                        tag->retired=true;
                        parentTagListWidget->tagErase(tag);
                        parentList->retire(tagClothes);
                        return;
                    }
                }
                if(tagClothes->dead)
                    return;
                if(tagClothes->readCore()>=0)
                    parentList->all_clothes.insert(tagClothes);
                else
                    parentList->retire(tagClothes);
            }
            int readCore(std::shared_ptr<teTag>in_core=nullptr){
                if(dead)
                    return 0;
                if((core||!in_core)&&parentList->clothes_editing==core)return 0;
                if(core==in_core) in_core=nullptr;

                bool isNewEntry=false;
                bool ifhascolor=in_core?(!colors.empty()):false;
                bool isTypeChanged=false;
                if(in_core&&(!core)){
                    isNewEntry=true;
                    core=in_core;
                    type=*in_core->words.back();
                }
                else if((!in_core)&&core){
                    isNewEntry=true;
                    colors.clear();
                    front_adjectives.clear();
                    adjectives.clear();
                    back_adjectives.clear();
                    if(type!= *core->words.back()){
                        isTypeChanged=true;
                        auto it = parentList->all_clothes.find(this);
                        if(it!=parentList->all_clothes.end()){
                            parentList->all_clothes.erase(it);
                            type = *core->words.back();
                            parentList->all_clothes.insert(this);
                        }
                    }
                    in_core=core;
                    MergeSwitch=false;
                    if(!parentList->filter(core)){
                        parentList->unlink(core);
                        MergeSwitch=true;
                        return -1;
                    }
                    MergeSwitch=true;
                    // filter() re-enters this class through the tag signals and
                    // can retire this wrapper.
                    if(dead)
                        return 1;
                }
                auto [colorpos,colorcount] = is_color(*in_core);

                static auto findInMultimap = [](std::multimap<QString,QString>&map,QString& type,QString adj)->bool{
                    auto range = prefix_front.equal_range(type);
                    if(range.first!=range.second)
                        for (auto it = range.first; it != range.second; ++it) {
                            if (adj == it->second){
                                return true;
                            }
                        }
                    auto general_range = prefix_front.equal_range(qsl(""));
                    if(general_range.first!=general_range.second)
                        for (auto it = general_range.first; it != general_range.second; ++it) {
                            if (adj == it->second){
                                return true;
                            }
                        }
                    return false;
                };
                static auto ifduplicate = [](QVector<teWord*>vec,QString str)->bool{
                    for(teWord*wc:vec)
                        if(wc->text==str)
                            return true;
                    return false;
                };
                int wordcountMinusOne = in_core->words.count()-1;
                for(int i=0;i<wordcountMinusOne;++i){
                    if(i>=colorpos&&i<colorpos+colorcount){
                        if(ifhascolor)continue;
                        if(isNewEntry||!ifduplicate(colors,in_core->words[i]->text)){
                            colors.push_back(in_core->words[i]);
                            if(!isNewEntry){
                                teWord*takecore = in_core->takeWordAt(i,false);
                                Q_UNUSED(takecore);
                                --i;--wordcountMinusOne;--colorpos;
                            }
                        }
                    }
                    else if(findInMultimap(prefix_front,type,in_core->words[i]->text)){
                        if(isNewEntry||!ifduplicate(front_adjectives,in_core->words[i]->text)){
                            front_adjectives.push_back(in_core->words[i]);
                            if(!isNewEntry){
                                teWord*takecore = in_core->takeWordAt(i,false);
                                Q_UNUSED(takecore);
                                --i;--wordcountMinusOne;--colorpos;
                            }
                        }
                    }
                    else if(findInMultimap(prefix_back,type,in_core->words[i]->text)){
                        if(isNewEntry||!ifduplicate(back_adjectives,in_core->words[i]->text)){
                            back_adjectives.push_back(in_core->words[i]);
                            if(!isNewEntry){
                                teWord*takecore = in_core->takeWordAt(i,false);
                                Q_UNUSED(takecore);       // no widget for it: the list owns widgets
                                --i;--wordcountMinusOne;--colorpos;
                            }
                        }
                    }
                    else if(isNewEntry||!ifduplicate(adjectives,in_core->words[i]->text)){
                        adjectives.push_back(in_core->words[i]);
                        if(!isNewEntry){
                            teWord*takecore = in_core->takeWordAt(i,false);
                            Q_UNUSED(takecore);
                            --i;--wordcountMinusOne;--colorpos;
                        }
                    }
                }
                int wordindex=-1;
                for(teWord*wc:colors)
                    if(core->words[++wordindex]!=wc)
                        goto sortwords;
                for(teWord*wc:front_adjectives)
                    if(core->words[++wordindex]!=wc)
                        goto sortwords;
                for(teWord*wc:adjectives)
                    if(core->words[++wordindex]!=wc)
                        goto sortwords;
                for(teWord*wc:back_adjectives)
                    if(core->words[++wordindex]!=wc)
                        goto sortwords;

                return 0;
                sortwords:
                static auto insertWords = [](std::shared_ptr<teTag>tag,QVector<QVector<teWord*>>wordListList,teTagWidgetBase* tagWidget){
                        bool hasWidget=tagWidget!=nullptr;
                        if(hasWidget)
                            tagWidget->disconnectWord();
                        tag->words.clear();
                        int pos=-1;
                        QHash<QString, bool> seen;
                        for(QVector<teWord*>& wordList:wordListList)
                            for(teWord*wc:wordList){
                                if(seen[*wc]){
                                    delete wc;
                                    continue;
                                }
                                seen[*wc]=true;
                                if(hasWidget)
                                    tagWidget->insertWord(++pos,wc,true,false);
                                else
                                    tag->words.append(wc);
                            }
                    };
                teWord*typeWord;
                if(isTypeChanged){
                    typeWord=new teWord{type};
                    delete core->words.back();
                }else{
                    typeWord=core->words.back();
                }
                insertWords(core,{colors,front_adjectives,adjectives,back_adjectives,{typeWord}},parentList?parentList->tagWidgetFor(core):nullptr);
                parentList->clothes_editing = in_core;
                core->edited_with_layout();
                // The signal above may have retired this wrapper (and thus freed
                // nothing yet, but every member is off limits from then on).
                if(dead)
                    return 1;
                parentList->clothes_editing = nullptr;
                if(in_core&&in_core!=core){
                    parentList->clothes_editing = in_core;
                    in_core->retired=true;
                    in_core->edited_with_layout();
                    if(!dead)
                        parentList->clothes_editing=nullptr;
                }
                return 1;
            }
            void clear(){
                colors.clear();
                front_adjectives.clear();
                adjectives.clear();
                back_adjectives.clear();
                exclusiveModifiers.clear();
                type=nullptr;
                core=nullptr;
            }
            QString text(){
                QString final;
                for(teWord*wd:colors){
                    final.append(wd->text);
                    final.append(qsl(" "));
                }
                for(teWord*wd:front_adjectives){
                    final.append(wd->text);
                    final.append(qsl(" "));
                }
                for(teWord*wd:adjectives){
                    final.append(wd->text);
                    final.append(qsl(" "));
                }
                for(teWord*wd:back_adjectives){
                    final.append(wd->text);
                    final.append(qsl(" "));
                }
                final.append(type);
                return final;
            }
            QStringList allAdjectives(){
                QStringList final;
                for(teWord*wd:front_adjectives)
                    final.append(wd->text);
                for(teWord*wd:adjectives)
                    final.append(wd->text);
                for(teWord*wd:back_adjectives)
                    final.append(wd->text);
                return final;
            }

            bool merge(std::shared_ptr<teTag>in_core){
                if(dead)
                    return false;
                if(!sameType(in_core))return false;
                if(auto[cp,cc] = is_color(*in_core);cc>0&&!colors.empty()){
                    if(cc!=colors.count())return false;
                    for(int i = 0;i<cc;++i)
                        if(in_core->words[i+cp]->text!=colors[i]->text)
                            return false;
                }
                if(in_core==core)
                    return false;
                // The generic word is the one being absorbed, so the specific
                // spelling of the incoming tag replaces it: with "underwear" in
                // the list and "panties" typed, the result says "panties".
                const QString incoming = in_core->words.isEmpty()?QString{}:in_core->words.back()->text;
                if(!incoming.isEmpty()&&isGenericClothesType(type)&&!isGenericClothesType(incoming)){
                    type = incoming;
                    if(core&&!core->words.isEmpty())
                        core->words.back()->text = incoming;
                }
                readCore(in_core);
                return true;
            }
            bool sameType(std::shared_ptr<teTag>core){
                if(type.isEmpty()||core->words.empty())
                    return false;
                // Synonyms ("underwear" / "panties") and the generic words
                // ("footwear" / "boots") count as the same garment, so picking one
                // merges the other instead of leaving both.
                return sameClothesType(core->words.back()->text,type);
            }
        };

        std::multiset<teClothes*,std::function<bool(teClothes*, teClothes*)>>all_clothes{[](teClothes *a, teClothes *b)->bool{
            if(a->type==nullptr)return true;
            else if(b->type==nullptr)return false;
            else
                return a->type<b->type;
        }};
        /// Wrappers waiting to be deleted (see teClothes::dead).
        QVector<teClothes*> retired_clothes;
        bool purgeScheduled=false;
        /**
         * @brief Takes a wrapper out of all_clothes and deletes it later.
         *
         * Never delete a teClothes while anything may still be running inside it:
         * both reReadClothes() and readCore() are re-entered through the tag's
         * signals, and they used to delete the wrapper they were called from.
         */
        void retire(teClothes*wrapper){
            if(!wrapper||wrapper->dead)
                return;
            wrapper->dead=true;
            wrapper->core.reset();
            all_clothes.erase(wrapper);
            retired_clothes.append(wrapper);
            purgeRetired();
        }
        void purgeRetired(){
            if(purgeScheduled||retired_clothes.isEmpty())
                return;
            purgeScheduled=true;
            // Deferred to the event loop: everything that is still on the stack
            // only looks at the dead flag, which is already set.
            QTimer::singleShot(0,this,[this]{
                purgeScheduled=false;
                for(teClothes*wrapper:retired_clothes)
                    delete wrapper;
                retired_clothes.clear();
            });
        }
        void purgeRetiredNow(){
            purgeScheduled=false;
            for(teClothes*wrapper:retired_clothes)
                delete wrapper;
            retired_clothes.clear();
        }
        teClothes* findClothes(std::shared_ptr<teTag>tag){
            for(teClothes*clz:all_clothes){
                if(clz->core==tag){
                    return clz;
                }
            }
            telog("[reReadClothes]:didn't find clothes object for input tag");
            return nullptr;
        }
        ClothesList(colorsWidget*in_onEdit_widget,teTagListView*parentlist,QWidget*parent = nullptr,QString*styleSheet=nullptr)
            :teTagListControl(in_onEdit_widget,parentlist,parent,styleSheet,"clothes"){
            this->info=QStringLiteral("ClothesList");
            sc->setMinimumHeight(110);
        }
        ~ClothesList(){
            // The deferred purge is bound to this widget, so it will not run any
            // more: delete what it was still holding.
            purgeRetiredNow();
        }
        bool filter(std::shared_ptr<teTag>in_tag)override{
            if(in_tag==clothes_editing)return true;
            if(in_tag->words.empty())return false;
            // "reaching through panties" and friends are actions, not garments.
            if(looksLikeActionPhrase(*in_tag))
                return false;
            if(allClothesTypes.contains(*in_tag->words.back())){
                if(autoMerge&&MergeSwitch){
                    if(!all_clothes.empty()){
                        // Every wrapper is a candidate: the type words that mean
                        // the same garment ("panties"/"underwear") are *not*
                        // adjacent in the type-ordered set, so looking them up
                        // with lower_bound()/upper_bound() never found them -
                        // which is why the merge was not automatic.
                        // Snapshot first: readCore()/merge() re-enter this class
                        // through the tag signals and can retire wrappers.
                        const QVector<teClothes*> candidates(all_clothes.begin(),all_clothes.end());
                        for(teClothes*clothes:candidates){
                            if(clothes->dead||!all_clothes.count(clothes))
                                continue;
                            if(in_tag==clothes->core){
                                clothes->readCore();
                                return false;
                            }
                        }
                        for(teClothes*clothes:candidates){
                            if(clothes->dead||!all_clothes.count(clothes))
                                continue;
                            if(clothes->merge(in_tag)){
                                in_tag->retired=true;
                                return false;
                            }
                        }
                    }
                    auto newClothse = new teClothes(in_tag,this,taglistwidget);
                    all_clothes.insert(newClothse);
                }
                return true;
            }
            return false;
        }

        void resetPolicy()override{
            for(teClothes*clothes:all_clothes)
                clothes->dead=true;         // no callback may touch them any more
            purgeRetiredNow();
            all_clothes.clear();
        };
        void link(std::shared_ptr<teTag>in_tag)override{
            linked_tags.insert(in_tag);
            in_tag->teConnect(teCallbackType::destroy,this,&teEditorControl::unlink,in_tag);
            teTagWidgetBase*widget =taginsert(-1,in_tag,taglistwidget->showing_list);
            extraWidgetPushBack(in_tag,widget);
        }
        void connectTag(teTagWidgetBase*tag)override{
            teTagListWidgetBase::connectTag(tag);
        }
        void unlink(std::shared_ptr<teTag>in_tag)override{
            // teDisconnect(in_tag.get());
            for(teClothes*clothes:QVector<teClothes*>(all_clothes.begin(),all_clothes.end()))
                if(clothes->core==in_tag){
                    retire(clothes);
                    break;
                }
            tagErase(in_tag);
            linked_tags.erase(in_tag);
        }
        void extraWidgetPushBack(std::shared_ptr<teTag>in_tag,teTagWidgetBase*widget){
            if(*in_tag->words.back()==qsl("bow")){
                int wordcount = in_tag->words.size();
                QPushButton*hairbow_btn = new QPushButton("hair",widget);
                hairbow_btn->setStyleSheet(qsl(R"(font:8pt "Sonsolas")"));
                hairbow_btn->setCheckable(true);
                hairbow_btn->setFixedHeight(15);
                hairbow_btn->setContentsMargins(2,1,2,1);
                for(int i =0;i<wordcount;++i)
                    if(*in_tag->words[i]==qsl("hair"))
                        hairbow_btn->setChecked(true);
                connect(hairbow_btn,&QPushButton::clicked,widget,[in_tag,this](bool ifchecked){
                    if(ifchecked)
                        if(auto* editorTag=tagWidgetFor(in_tag)) editorTag->insertWord(-2,qsl("hair"));
                    else{
                        int wordcount = in_tag->words.size();
                        for(int i =0;i<wordcount;++i)
                            if(*in_tag->words[i]==qsl("hair"))
                            {if(auto* editorTag=tagWidgetFor(in_tag)) editorTag->destroyWord(i);--i;--wordcount;}
                    }
                },Qt::DirectConnection);
                widget->insertExtraWidgets(this,hairbow_btn);
            }
        }
        void refreshState()override{

        }

};