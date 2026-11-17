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
inline bool isGenericClothesType(const QString& type){
    return genericClothesTypes.contains(type);
}
inline std::multimap<QString, QString> prefix_back{
    {qsl("dress"),qsl("wedding")},
    {qsl("waist"),qsl("apron")},
    {qsl("bikini"),qsl("one-piece")},
    {qsl("swimsuit"),qsl("one-piece")},
    {qsl(""),qsl("fishnet")},
    {qsl(""),qsl("sleeveless")},
    { qsl("dress"),qsl("china") }
};
inline std::multimap<QString, QString> prefix_front{
    {qsl(""),qsl("torn")},
    {qsl(""),qsl("detached")},
    {qsl(""),qsl("unworn")},
    {qsl(""),qsl("short")},
    {qsl(""),qsl("long")},
    {qsl(""),qsl("frilled")}
};
/// Groups of words that may not appear together in one clothes tag.
inline QVector<QSet<QString>> exclusiveModifierGroups{

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

/**
 * @brief Which part of a clothes tag a word is.
 *
 * The clothes editor spends most of its logic on this classification: a tag is
 * "colours + front adjectives + adjectives + type", and the canonical order of
 * those parts is what the editor rewrites a tag into.
 */
enum class ClothesRole { Colour, FrontAdjective, Adjective, BackAdjective, Type };

/**
 * @brief True when `adj` is listed for `type` (or for every type) in `map`.
 *
 * Pure lookup; the maps are prefix_front / prefix_back below.
 */
inline bool findInPrefixMap(const std::multimap<QString,QString>& map,
                            const QString& type,const QString& adj){
    auto range = map.equal_range(type);
    for(auto it=range.first;it!=range.second;++it)
        if(adj==it->second)
            return true;
    auto general = map.equal_range(qsl(""));
    for(auto it=general.first;it!=general.second;++it)
        if(adj==it->second)
            return true;
    return false;
}

/**
 * @brief The role of word `index` of a clothes tag.
 *
 * This is the per word form: readCore() walks a tag word by word and calls this,
 * so nothing is allocated on that path. classifyClothesWords() is the whole tag
 * form used by the regression harness.
 *
 * prefix_back is honoured here (it was not, until the vocabulary owner asked for
 * it: the lookup used to always query prefix_front, so words such as "wedding"
 * or "one-piece" never reached back_adjectives and a tag like "wedding lace
 * dress" kept its word order instead of being sorted to "lace wedding dress").
 */
inline ClothesRole clothesRoleAt(const teTag& tag,int index,const QString& type,
                                 const QPair<int,int>& colorSlot){
    if(index>=colorSlot.first&&index<colorSlot.first+colorSlot.second)
        return ClothesRole::Colour;
    if(index>=0&&index<tag.words.size()){
        const QString& text = tag.words[index]->text;
        if(findInPrefixMap(prefix_front,type,text))
            return ClothesRole::FrontAdjective;
        if(findInPrefixMap(prefix_back,type,text))
            return ClothesRole::BackAdjective;
    }
    return ClothesRole::Adjective;
}

/**
 * @brief The role of every word of `tag`, in tag order (last one is the type).
 *
 * Pure: it reads the tag and the prefix tables and nothing else, so the clothes
 * rules can finally be checked in the regression harness - they used to be
 * reachable only by opening the window and looking at it.
 */
inline QVector<ClothesRole> classifyClothesWords(const teTag& tag){
    QVector<ClothesRole> roles;
    if(tag.words.isEmpty())
        return roles;
    roles.reserve(tag.words.size());
    const QString type = tag.words.back()->text;
    const auto colorSlot = is_color(tag);
    const int count = tag.words.size()-1;
    for(int i=0;i<count;++i)
        roles.push_back(clothesRoleAt(tag,i,type,colorSlot));
    roles.push_back(ClothesRole::Type);
    return roles;
}

/**
 * @brief A clothes tag split into the parts the editor reasons about.
 *
 * "colours + front adjectives + adjectives + back adjectives + type" is the
 * canonical order a clothes tag is rewritten into, and every question the editor
 * asks about a tag (may these two merge, what would the merged text be) is
 * answered from this - computed on demand instead of kept in a per-tag object
 * that had to be deleted at exactly the right moment.
 */
struct ClothesParts {
    QStringList colors;
    QStringList front;
    QStringList adjectives;
    QStringList back;
    QString type;
};

/**
 * @brief Splits `tag` into its parts. Pure: it reads the tag and the tables.
 *
 * The per-word rule is clothesRoleAt(), so this is the whole-tag form of the
 * classification the control makes while it walks the words.
 */
inline ClothesParts clothesParse(const teTag& tag)
{
    ClothesParts parts;
    if (tag.words.isEmpty())
        return parts;
    parts.type = tag.words.back()->text;
    const auto colorSlot = is_color(tag);
    for (int i = 0; i + 1 < tag.words.size(); ++i) {
        const QString& text = tag.words[i]->text;
        QStringList* bucket = nullptr;
        switch (clothesRoleAt(tag, i, parts.type, colorSlot)) {
        case ClothesRole::Colour:         bucket = &parts.colors;     break;
        case ClothesRole::FrontAdjective: bucket = &parts.front;      break;
        case ClothesRole::BackAdjective:  bucket = &parts.back;       break;
        case ClothesRole::Adjective:
        case ClothesRole::Type:           bucket = &parts.adjectives; break;
        }
        if (bucket && !bucket->contains(text))
            bucket->append(text);
    }
    return parts;
}

/// The words of `parts` in canonical order, each word only once.
inline QStringList clothesCanonicalWords(const ClothesParts& parts)
{
    QStringList words;
    words.reserve(parts.colors.size()+parts.front.size()
                  +parts.adjectives.size()+parts.back.size()+1);
    for (const QStringList* bucket : {&parts.colors,&parts.front,&parts.adjectives,&parts.back})
        for (const QString& text : *bucket)
            if (!words.contains(text))
                words.append(text);
    words.append(parts.type);
    return words;
}

/**
 * @brief True when `incoming` may be merged into a tag that looks like `target`.
 *
 * The garment has to be the same (sameClothesType handles the generic words) and
 * the colours have to agree: "red dress" and "blue dress" stay apart, "red
 * dress" and "red torn dress" merge. Pure, so the merge *rules* - which used to
 * be reachable only by typing two tags into the window - can be checked.
 */
inline bool clothesMergeable(const ClothesParts& target,const teTag& incoming)
{
    if(target.type.isEmpty()||incoming.words.isEmpty())
        return false;
    if(!sameClothesType(incoming.words.back()->text,target.type))
        return false;
    const auto [colorpos,colorcount] = is_color(incoming);
    if(colorcount>0&&!target.colors.isEmpty()){
        if(colorcount!=target.colors.size())
            return false;
        for(int i=0;i<colorcount;++i)
            if(incoming.words[i+colorpos]->text!=target.colors[i])
                return false;
    }
    return true;
}

/// Convenience for a target known only by its garment word and its colours.
inline bool clothesMergeable(const QString& type,const QStringList& colors,const teTag& incoming)
{
    ClothesParts target;
    target.type = type;
    target.colors = colors;
    return clothesMergeable(target,incoming);
}

/**
 * @brief The words of a tag that has absorbed `incoming`.
 *
 * The absorbing tag keeps its garment and its colours; the incoming tag
 * contributes its adjectives, and a generic garment word ("underwear",
 * "footwear") gives way to the specific spelling the incoming tag has
 * ("panties", "boots"). Pure, so what a merge produces can be checked without
 * opening the window.
 */
inline QStringList clothesMergedWords(const ClothesParts& target,const teTag& incoming)
{
    ClothesParts merged = target;
    if(!incoming.words.isEmpty()){
        const QString incomingType = incoming.words.back()->text;
        if(isGenericClothesType(merged.type)&&!isGenericClothesType(incomingType))
            merged.type = incomingType;
    }
    const ClothesParts in = clothesParse(incoming);
    if(merged.colors.isEmpty())
        merged.colors = in.colors;
    merged.front += in.front;
    merged.adjectives += in.adjectives;
    merged.back += in.back;
    return clothesCanonicalWords(merged);
}

/**
 * @brief Writes `words` into `tag`, reusing the word objects that survive.
 *
 * Words are matched by text and moved, so a widget showing one keeps its
 * connection; a word that is no longer part of the tag is deleted (after the
 * widgets have let go of it) and a new text gets a new word object. `widget` is
 * the tag's widget, or nullptr when the control does not show one. The change is
 * announced once - edited_with_layout() - which is what makes every list showing
 * the tag reload its text.
 */
inline void clothesApplyWords(std::shared_ptr<teTag> tag,const QStringList& words,
                              teTagWidgetBase* widget)
{
    if(!tag)
        return;
    QMultiHash<QString,teWord*> available;
    for(teWord* word : tag->words)
        if(word)
            available.insert(word->text,word);
    QVector<teWord*> next;
    next.reserve(words.size());
    for(const QString& text : words){
        auto it = available.find(text);
        if(it==available.end())
            next.append(new teWord{text});
        else{
            next.append(*it);
            available.erase(it);
        }
    }
    const bool hasWidget = widget!=nullptr;
    if(hasWidget)
        widget->disconnectWord();
    tag->words.clear();
    for(teWord* leftover : available){      // duplicates the canonical list dropped
        if(hasWidget)
            widget->forgetWord(leftover);
        delete leftover;
    }
    int pos = -1;
    for(teWord* word : next){
        if(hasWidget)
            widget->insertWord(++pos,word,true,false);
        else
            tag->words.append(word);
    }
    tag->edited_with_layout();
}

/// Words that make a clothes tag a *state* rather than a garment.
inline const QSet<QString>& clothesStateModifiers(){
    static const QSet<QString> words{
        qsl("no"),qsl("without"),qsl("unworn"),qsl("open")
    };
    return words;
}

/**
 * @brief True when the tag carries such a modifier.
 *
 * "no shoes", "top without sleeves", "unworn panties", "open jacket" describe a
 * state, so they must not take part in a clothes merge in either direction - the
 * same idea as the action phrases ("... through panties") that filter() keeps out
 * of the control. Unlike those, these tags *are* clothes tags and stay editable;
 * they are only excluded from merging.
 */
inline bool clothesHasStateModifier(const teTag& tag){
    for(const teWord* word : tag.words)
        if(word&&clothesStateModifiers().contains(word->text))
            return true;
    return false;
}

/**
 * @brief The clothes control.
 *
 * It used to own one `teClothes` object per tag: that object cached the parsed
 * tag, held a shared_ptr to it and kept two connections on it, and the whole
 * lifetime machine around it (a `dead` flag, a retired list and a deferred purge
 * timer) existed only to keep the object from being used after it had been
 * deleted - and it still leaked one per tag per picture switch, which kept whole
 * tag lists alive.
 *
 * None of that state was necessary. Everything a wrapper knew is a pure function
 * of the tag (clothesParse), so the control keeps nothing but the tags it linked
 * and recomputes on demand. filter() is a predicate again: the merge it used to
 * perform from inside is the explicit absorb() step in read()/onTagEdited().
 */
struct ClothesList : public teTagListControl {
    /// The tag this control is rewriting right now.
    ///
    /// Its own announcement must not send it through the merge/rewrite path
    /// again; the wrapper had the same guard and called it clothes_editing.
    std::shared_ptr<teTag> clothes_editing = nullptr;
    /**
     * @brief The linked clothes tags, in link order.
     *
     * When two tags could absorb a third one, the first one linked wins - the
     * insertion order the old type-sorted multiset gave. Only the order matters:
     * the tags are owned by the tag list and the shared_ptrs here are dropped in
     * unlink(), exactly like linked_tags.
     */
    QVector<std::shared_ptr<teTag>> clothes_order;

    ClothesList(colorsWidget*in_onEdit_widget,teTagListView*parentlist,QWidget*parent = nullptr,QString*styleSheet=nullptr)
        :teTagListControl(in_onEdit_widget,parentlist,parent,styleSheet,"clothes"){
        this->info=QStringLiteral("ClothesList");
        sc->setMinimumHeight(110);
    }

    /**
     * @brief Is this tag a garment? Pure.
     *
     * It changes nothing: the merge, the insertion and the retirement that used
     * to happen in here are absorb() now, called from the two places that really
     * want them (read() and onTagEdited()). That is what makes the answer to
     * "is this mine?" worth something.
     */
    bool filter(std::shared_ptr<teTag> in_tag) const override{
        if(!in_tag||in_tag->words.empty())
            return false;
        if(in_tag==clothes_editing)
            return true;                        // our own rewrite, still ours
        // "reaching through panties" and friends are actions, not garments.
        if(looksLikeActionPhrase(*in_tag))
            return false;
        return allClothesTypes.contains(*in_tag->words.back());
    }

    TagHandling read(std::shared_ptr<teTag> tag)override{
        if(!tag)
            return TagHandling::NotMine;
        const bool mine = filter(tag);
        if(tag->retired){
            if(taglistwidget)
                taglistwidget->retire(tag);
            return TagHandling::Consumed;
        }
        if(!mine)
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

    /**
     * @brief The tag was edited: absorb it, or bring it back into shape.
     *
     * This replaces the wrapper's reReadClothes(), which had to find its own
     * wrapper by pointer first and to live with being deleted from inside the
     * signal it was called from.
     */
    void onTagEdited(std::shared_ptr<teTag> tag){
        if(!tag||tag->retired)
            return;
        if(tag==clothes_editing)
            return;                             // our own rewrite
        if(!linked_tags.count(tag))
            return;                             // not one of ours (any more)
        if(!filter(tag)){
            unlink(tag);
            return;
        }
        if(absorb(tag)){
            if(taglistwidget)
                taglistwidget->retire(tag);
            return;
        }
        applyWords(tag);
    }

    /**
     * @brief Merges `incoming` into the first linked tag that accepts it.
     *
     * Returns true when it did, and the caller then retires `incoming` (marked by
     * this call). The absorbing tag is rewritten through applyWords(), so the
     * result is announced like any other edit.
     */
    bool absorb(std::shared_ptr<teTag> incoming){
        if(!incoming||incoming->words.isEmpty())
            return false;
        if(!autoMerge||!MergeSwitch)
            return false;
        if(!allClothesTypes.contains(incoming->words.back()->text))
            return false;
        // A tag that describes a state ("no shoes", "unworn panties") neither
        // absorbs another clothes tag nor is absorbed by one - in either
        // direction.
        if(clothesHasStateModifier(*incoming))
            return false;
        for(const std::shared_ptr<teTag>& target : clothes_order){
            if(!target||target==incoming||target->retired)
                continue;
            if(!linked_tags.count(target))
                continue;
            if(clothesHasStateModifier(*target))
                continue;
            const ClothesParts parts = clothesParse(*target);
            if(parts.type.isEmpty())
                continue;
            if(!clothesMergeable(parts,*incoming))
                continue;
            const QStringList words = clothesMergedWords(parts,*incoming);
            incoming->retired = true;
            writeWords(target,words);
            return true;
        }
        return false;
    }

    /// Puts one tag's words into canonical order (no merge, no widget churn when
    /// the tag already is canonical).
    void applyWords(std::shared_ptr<teTag> tag){
        writeWords(tag,clothesCanonicalWords(clothesParse(*tag)));
    }

    /**
     * @brief Writes `words` into `tag` and announces it.
     *
     * Nothing is written when the tag already spells them: an announcement that
     * changes nothing is not harmless here - it makes every list reload, and it
     * is what would turn "already in order" into an endless loop of re-reads.
     */
    void writeWords(std::shared_ptr<teTag> tag,const QStringList& words){
        if(!tag||tag->words.isEmpty())
            return;
        QStringList current;
        current.reserve(tag->words.size());
        for(const teWord* word : tag->words)
            current.append(word->text);
        if(current==words)
            return;
        // Another control reading this tag must not start merging while the words
        // are being replaced (the wrapper suppressed it the same way).
        const bool previousSwitch = MergeSwitch;
        MergeSwitch = false;
        const std::shared_ptr<teTag> previousEditing = clothes_editing;
        clothes_editing = tag;
        clothesApplyWords(tag,words,tagWidgetFor(tag));
        clothes_editing = previousEditing;
        MergeSwitch = previousSwitch;
    }

    void link(std::shared_ptr<teTag> in_tag)override{
        linked_tags.insert(in_tag);
        clothes_order.append(in_tag);
        in_tag->teConnect(teCallbackType::destroy,this,&EditorControl::unlink,in_tag);
        // Canonical order first, before the widget exists: that is the order the
        // old wrapper read the tag in, so the widget is built from the final words
        // instead of being rewritten right after it was created.
        applyWords(in_tag);
        in_tag->teConnect(teCallbackType::edit,qsl("ClothesList::link"),this,&ClothesList::onTagEdited,in_tag);
        in_tag->teConnect(teCallbackType::edit_with_layout,this,&ClothesList::onTagEdited,in_tag);
        teTagWidgetBase*widget =taginsert(-1,in_tag,taglistwidget->showing_list);
        extraWidgetPushBack(in_tag,widget);
    }

    void unlink(std::shared_ptr<teTag> in_tag)override{
        teDisconnect(in_tag.get());
        clothes_order.removeAll(in_tag);
        linked_tags.erase(in_tag);
        tagErase(in_tag);
    }

    void resetPolicy()override{
        clothes_order.clear();
        clothes_editing.reset();
    }

    void extraWidgetPushBack(std::shared_ptr<teTag>in_tag,teTagWidgetBase*widget){
        if(in_tag->words.empty()||*in_tag->words.back()!=qsl("bow"))
            return;
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
            auto* editorTag=tagWidgetFor(in_tag);
            if(ifchecked){
                // Never a second "hair": clicking again must not add another
                // word (it used to, so "hair" piled up).
                if(in_tag->contains(qsl("hair")))
                    return;
                if(editorTag)
                    editorTag->insertWord(-2,qsl("hair"));
            }else{
                // Every "hair" is removed *without* signalling in between.
                // Each destroyWord() re-enters this control through
                // edited_with_layout() -> onTagEdited() -> applyWords(), which
                // rewrites the tag's words - so the old loop walked a vector that
                // had already been rewritten, and the word either survived or came
                // back. One announcement at the end is enough.
                bool removed=false;
                for(int i=in_tag->words.size()-1;i>=0;--i)
                    if(*in_tag->words[i]==qsl("hair")){
                        if(editorTag)
                            editorTag->destroyWord(i,false);
                        else{
                            delete in_tag->words[i];
                            in_tag->words.erase(in_tag->words.begin()+i);
                        }
                        removed=true;
                    }
                if(removed)
                    in_tag->edited_with_layout();
            }
        },Qt::DirectConnection);
        widget->insertExtraWidgets(this,hairbow_btn);
    }
};
