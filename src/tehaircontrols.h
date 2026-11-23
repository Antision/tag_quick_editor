/*
 * tehaircontrols.h - controls moved out of teeditor_derive.cpp, where they used
 * to be declared inside the editor constructors.
 */
#pragma once
#include "teeditorhelpers.h"

    /// Which kind of hair tag a tag is (see classifyHairTag()).
enum class HairKind { None, BaseHair, StreakHair, GradientHair, RawStreakHair, RawGradientHair };

/**
 * @brief Classifies a hair tag. Pure: it reads the tag and nothing else.
 *
 * A *base* tag is a plain colour ("yellow hair"); a *raw* streak or gradient tag
 * is one the user just built by pressing [s] / [g] ("blue streaked hair"); the
 * merged forms are "X hair with Y streaks" and "gradient from X to Y".
 */
inline HairKind classifyHairTag(const teTag& tag)
{
    if(is_color(tag).second==0||tag.words.isEmpty())
        return HairKind::None;
    // Every form has to name what it colours. `gradient` used to be enough on its
    // own, so a garment tag with a multi-colour pattern ("white to blue gradient
    // gloves") was classified - and then captured - as hair. The new wording
    // always carries the word it belongs to ("<c1> to <c2> gradient hair"), and
    // the old one carried it too ("hair gradient from ..."), so requiring it costs
    // nothing and stops the clothes palette from feeding this editor.
    const bool hairOrEyes = tag.contains(qsl("hair"))||tag.contains(qsl("eyes"));
    if(tag.contains(qsl("with")))
        return hairOrEyes ? HairKind::StreakHair : HairKind::None;
    // "gradient" is a noun ("a white-to-blue gradient"): the merged form is
    // "white to blue gradient hair". "from" is still recognised so tags written
    // by the old wording ("hair gradient from white to blue") stay editable.
    if(tag.contains(qsl("gradient"))&&(tag.contains(qsl("to"))||tag.contains(qsl("from"))))
        return hairOrEyes ? HairKind::GradientHair : HairKind::None;
    // Danbooru's raw streak tag is "[colour] streaks" - no "hair", no "streaked".
    if(*tag.words.back()==qsl("streaks"))
        return HairKind::RawStreakHair;
    if(tag.contains(qsl("streaked"))||tag.contains(qsl("gradient")))
        return hairOrEyes ? (tag.contains(qsl("gradient"))?HairKind::RawGradientHair
                                                    :HairKind::RawStreakHair)
                          : HairKind::None;
    // A plain colour is only hair when the tag says so. Without this, "red skirt"
    // (two words, a colour) was a base hair tag.
    return hairOrEyes ? HairKind::BaseHair : HairKind::None;
}

/// True when the tag is about eyes rather than hair.
inline bool isEyesTag(const teTag& tag)
{
    return tag.contains(qsl("eyes"));
}

/// The raw streak tag for a set of colours: "blue streaks" (danbooru wording),
/// or "blue and green streaks".
inline QString rawStreakText(const QStringList& colors)
{
    return colors.join(qsl(" and "))+qsl(" streaks");
}

/**
 * @brief The tag the "add tag" window produces for hair/eyes.
 *
 * The window keeps offering "streaked" and "hair" as buttons, but the tag it
 * writes is danbooru's: "[colour] streaks" rather than "[colour] streaked hair".
 * Eyes have no streak form at all, so the word is dropped there.
 */
inline QString composeHairTagText(QString text)
{
    const QString streakedHair=qsl(" streaked hair");
    const QString streakedEyes=qsl(" streaked eyes");
    if(text.endsWith(streakedHair)&&text.size()>streakedHair.size()){
        text.chop(streakedHair.size());
        return text+qsl(" streaks");
    }
    if(text.endsWith(streakedEyes)&&text.size()>streakedEyes.size()){
        text.chop(streakedEyes.size());
        return text+qsl(" eyes");
    }
    return text;
}

/**
 * @brief True when `source` may be merged into `target`.
 *
 * A plain hair tag absorbs a raw streak tag, a merged streak tag absorbs another
 * raw streak tag, and the gradient pair does the same - nothing else merges. In
 * particular two merged tags never merge with each other. Pure, and the bounds
 * check keeps HairKind::None (which the table has no row for) out.
 */
inline bool hairTagsCanMerge(HairKind target,HairKind source)
{
    static const bool matrix[5][5]{
        //basic    s      g      rs     rg
        {false,false,false, true,false},    // basic hair
        {false,false,false, true,false},    // "x hair with y streaks"
        {false,false,false,false, true},    // "gradient from x to y"
        { true, true,false,false,false},    // raw streak ("y streaked hair")
        // A raw gradient absorbs a plain base hair as well: "red gradient hair"
        // next to "white hair" is one gradient (white to red), the same way a raw
        // streak absorbs a base ("yellow hair with blue streaks"). The user's
        // manual-merge example needs exactly this pair.
        { true,false, true,false, true},    // raw gradient ("gradient hair")
    };
    // HairKind::None (0) has no row: the table covers the five real kinds, whose
    // enumerators start at BaseHair = 1.
    const int t=int(target)-1, src=int(source)-1;
    if(t<0||t>4||src<0||src>4)
        return false;
    return matrix[t][src];
}

/// Hair and eyes never merge into each other (plain "hair"/"eyes" word).
inline bool sameHairCategory(const teTag& first,const teTag& second)
{
    return first.contains(qsl("eyes"))==second.contains(qsl("eyes"));
}

/// The words of a merged streak tag: "yellow hair with blue streaks", or with
/// several streak colours "yellow hair with blue and green streaks".
inline QStringList streakedHairWords(const QStringList& colors)
{
    if(colors.isEmpty())
        return {};
    QStringList words{colors.first(),qsl("hair"),qsl("with")};
    for(int i=1;i<int(colors.size());++i){
        words.append(colors[i]);
        if(i+1<int(colors.size()))
            words.append(qsl("and"));
    }
    words.append(qsl("streaks"));
    return words;
}

/// The text of a merged gradient tag: "yellow to blue gradient hair" /
/// "yellow to blue gradient eyes". The colours come first because "gradient" is
/// the noun the phrase is about; the old "hair gradient from yellow to blue"
/// read it as a verb and put the category word in the middle of the tag.
inline QString gradientHairText(const QStringList& colors,bool eyes=false)
{
    return colors.join(qsl(" to "))+qsl(" gradient ")+(eyes?qsl("eyes"):qsl("hair"));
}

/// The buttons of the ponytail control (index 6 is the "tail" label).
enum PonytailButton {
    PonytailShort=0, PonytailLow=1, PonytailBraided=2, PonytailSide=3,
    PonytailPony=4, PonytailTwin=5, PonytailTail=6, PonytailButtonCount=6
};

/// The word the tail button writes into the tag.
inline QString tailWord(bool twintails)
{
    return twintails?qsl("tails"):qsl("tail");
}

/// What the ponytail control has to look like after a click.
struct PonytailAfterClick {
    QVector<bool> checkedIn;
    bool twintails=false;
};

/**
 * @brief The button states after the one at `clicked` was toggled.
 *
 * Pure: "twin" excludes pony and side, "pony" excludes twin, and a *kind* button
 * (short/low/braided/side) turns pony on when no tail is chosen yet. Those rules
 * used to be a chain of ifs over button indices, checkable only by clicking in
 * the window. `checkedIn` is the state as Qt delivered it - the clicked button
 * already carries its new state.
 */
inline PonytailAfterClick ponytailAfterClick(int clicked,const QVector<bool>& checkedIn)
{
    PonytailAfterClick out;
    out.checkedIn=checkedIn;
    if(out.checkedIn.size()<=PonytailButtonCount)
        return out;
    if(clicked==PonytailTwin&&out.checkedIn[PonytailTwin]){
        out.checkedIn[PonytailPony]=false;
        out.checkedIn[PonytailSide]=false;
    }else if(clicked==PonytailPony&&out.checkedIn[PonytailPony]){
        out.checkedIn[PonytailTwin]=false;
    }else if(clicked>PonytailSide&&!(out.checkedIn[PonytailPony]||out.checkedIn[PonytailTwin])){
        for(int i=0;i<PonytailPony;++i)
            out.checkedIn[i]=false;
    }else if(clicked<PonytailPony&&out.checkedIn[clicked]&&
             !(out.checkedIn[PonytailPony]||out.checkedIn[PonytailTwin])){
        out.checkedIn[PonytailPony]=true;
    }
    out.twintails=out.checkedIn[PonytailTwin];
    return out;
}

/// The five buttons of the hair length control (index 4 is the "hair" label).
enum HairLengthButton {
    HairVeryLong=0, HairLong=1, HairShort=2, HairMedium=3, HairLabel=4, HairLengthButtonCount=4
};

/**
 * @brief The button states after the one at `clicked` was toggled.
 *
 * Pure: long/short/medium exclude each other (that is the `(id-1+1)%3+1`
 * arithmetic, which nobody could check without clicking), "medium" turns very
 * long off, and very long implies long unless short is set.
 */
inline QVector<bool> hairLengthAfterClick(int clicked,const QVector<bool>& checkedIn)
{
    QVector<bool> checked=checkedIn;
    if(checked.size()<=HairLengthButtonCount)
        return checked;
    if(clicked<HairLabel&&clicked>HairVeryLong){
        if(checked[clicked]){
            checked[(clicked-1+1)%3+1]=false;
            checked[(clicked-1+2)%3+1]=false;
        }else if(!checked[HairLong]&&!checked[HairShort]){
            checked[HairVeryLong]=false;
        }
    }
    if(clicked==HairMedium&&checked[HairMedium]){
        checked[HairVeryLong]=false;
    }else if(clicked==HairVeryLong&&checked[HairVeryLong]&&!checked[HairShort]){
        checked[HairMedium]=false;
        checked[HairLong]=true;
    }
    return checked;
}

struct hair_and_eyes_color_list : public teTagListControl{
        hair_and_eyes_color_list(colorsWidget*in_onEdit_widget,TagListView*parentlist,QWidget*parent = nullptr,QString*styleSheet=nullptr)
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

        /// Is this a hair (or eyes) tag? Pure: the merge it used to start from
        /// here is absorb() now, called from link() and onHairTagEdited().
        bool filter(std::shared_ptr<teTag> in_tag) const override{
            if(!in_tag)
                return false;
            if(in_tag->retired)
                return false;
            if(in_tag->words.empty())return false;
            QString&&lastword=*in_tag->words.back();
            QString tagstring = *in_tag;
            if((lastword==qsl("hair")||lastword==qsl("eyes"))
                 &&
                 ((is_color(*in_tag).second>0)||tagstring==qsl("streaked hair")||tagstring==qsl("gradient hair"))){
                return true;
            }else if(tagstring.endsWith(qsl("streaks"))){
                return true;        // danbooru's "[colour] streaks"
            }else if(tagstring.contains(qsl("gradient"))
                     &&(tagstring.contains(qsl(" to "))||tagstring.contains(qsl(" from ")))
                     &&(tagstring.contains(qsl("hair"))||tagstring.contains(qsl("eyes")))){
                // "white to blue gradient hair" (old: "hair gradient from ...").
                // The hair/eyes word is required: without it every garment with a
                // gradient pattern was captured here, and the clothes palette's
                // own output vanished from the clothes editor.
                return true;
            }
            return false;
        }
        /// The classification lives in classifyHairTag() (pure, testable) and is
        /// computed from the tag whenever it is needed. It used to be cached in a
        /// QHash keyed by tag, which had to be kept in step with every edit - the
        /// cache and the tag could disagree, and the entry kept the tag alive.
        /// True while absorb() is editing the surviving tag (see its guard).
        bool m_merging=false;
        struct MergeGuard{
            bool& flag;
            explicit MergeGuard(bool& in_flag):flag(in_flag){ flag=true; }
            ~MergeGuard(){ flag=false; }
            MergeGuard(const MergeGuard&)=delete;
            MergeGuard& operator=(const MergeGuard&)=delete;
        };
        HairKind classify_tag(std::shared_ptr<teTag> tag) const {
            return tag?classifyHairTag(*tag):HairKind::None;
        }
        /// The merge matrix lives in hairTagsCanMerge() (pure, testable).
        bool can_merge(std::shared_ptr<teTag>,HairKind target_type, std::shared_ptr<teTag>,HairKind source_type) {
            return hairTagsCanMerge(target_type,source_type);
        }
        std::shared_ptr<teTag> find_merge_target(std::shared_ptr<teTag> new_tag,HairKind type) {
            // Walk every layout item. The old loop started at count()-2, which
            // silently skipped the last tag when the list had no trailing button;
            // editing the first tag then found no target at all.
            for(int i = layout->count()-1; i >=0; --i) {
                teRefTagWidget* widget = dynamic_cast<teRefTagWidget*>(layout->itemAt(i)->widget());
                std::shared_ptr<teTag> candidate;
                if(widget&&widget->core)
                    candidate = widget->core;
                else
                    continue;
                if(candidate==new_tag)continue;
                if(candidate->retired)continue;
                // Hair and eyes never merge into each other: "blonde streaked
                // hair" used to be absorbed by "yellow eyes" and produced a
                // nonsensical "yellow hair with blonde streaks".
                if(!sameHairCategory(*candidate,*new_tag))
                    continue;
                const HairKind candidateType = classify_tag(candidate);
                if(candidateType==HairKind::None) continue;

                if(can_merge(candidate,candidateType, new_tag,type)){
                    return candidate;
                }
            }
            return nullptr;
        }
        void perform_merge(std::shared_ptr<teTag> target, std::shared_ptr<teTag> source,HairKind source_type) {
            const HairKind target_type = classify_tag(target);
            if(source_type == HairKind::RawStreakHair||target_type==HairKind::RawStreakHair) {
                merge_streaks(target, source,source_type==HairKind::RawStreakHair);
            } else if(source_type == HairKind::RawGradientHair||target_type==HairKind::RawGradientHair) {
                merge_gradient(target, source,source_type==HairKind::RawGradientHair);
            }else
                telog("[perform_merge]:couldn't merge tag");
        }
        void merge_streaks(std::shared_ptr<teTag> target, std::shared_ptr<teTag> source,bool source_to_target) {
            // The wording itself is streakedHairWords() (pure, testable). The old
            // code built the same sentence by chopping " and " off by four
            // characters, which left a double space before "streaks"; the words
            // are simply joined with single spaces now.
            const QStringList all_colors = source_to_target
                    ? extract_colors(target)+extract_colors(source)
                    : extract_colors(source)+extract_colors(target);
            taglistwidget->tagEdit(target,streakedHairWords(all_colors).join(qsl(" ")));
        }
        void merge_gradient(std::shared_ptr<teTag> target, std::shared_ptr<teTag> source,bool source_to_target) {
            const QStringList all_colors = source_to_target
                    ? extract_colors(target)+extract_colors(source)
                    : extract_colors(source)+extract_colors(target);
            // "white to blue gradient hair" / "... eyes": the survivor decides
            // which category word closes the tag.
            taglistwidget->tagEdit(target,gradientHairText(all_colors,isEyesTag(*target)));
        }
        /**
         * @brief A linked tag changed: see whether it can now be merged.
         *
         * Connected for every linked tag, not only for the ones that were a hair
         * tag when they were linked - the classification is taken from the tag on
         * every call, so a tag that *became* a streak ("blue streaked hair") is
         * picked up here just the same.
         */
        void onHairTagEdited(std::shared_ptr<teTag> edited_tag) {
            if(!edited_tag||edited_tag->retired||m_merging)
                return;
            const HairKind still_valid = classify_tag(edited_tag);
            if(!mergeKindAllowed(edited_tag,still_valid))
                return;                     // not a mergeable hair tag any more
            std::shared_ptr<teTag> new_target = find_merge_target(edited_tag,still_valid);
            if(new_target&&mergeAllowed(*new_target,*edited_tag)) {
                MergeGuard guard(m_merging);
                markMerged(*new_target,*edited_tag);
                edited_tag->retired=true;
                perform_merge(new_target, edited_tag,still_valid);
                foldRawSiblings(new_target);
            }
        }
        /**
         * @brief Merges `just_edited` now, without waiting for the signal.
         *
         * The word edit above announces itself (edit_with_layout) and
         * onHairTagEdited() normally merges from that. Doing it here as well is
         * what makes [s]/[g] behave exactly like typing the same tag in the
         * colour list: both end in absorb().
         */
        void mergeNow(std::shared_ptr<teTag> just_edited){
            if(!just_edited||just_edited->retired)
                return;
            absorb(just_edited);
        }
        virtual void tagEdit(teTagWidgetBase*tag,teWordWidgetBase*word)override{
            if(std::shared_ptr<teTag>core =tag->core; core&&(classify_tag(core)==HairKind::StreakHair||classify_tag(core)==HairKind::GradientHair)){
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
                connect(lineedit,&SuggestionEdit::editingFinished,this,&teRefTagListWidget::onLineEditStop,Qt::DirectConnection);
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
            if(absorb(in_tag))
                return;
            linked_tags.insert(in_tag);
            in_tag->teConnect(teCallbackType::destroy,this,&EditorControl::unlink,in_tag);
            in_tag->teConnect(teCallbackType::edit_with_layout,this,&hair_and_eyes_color_list::onHairTagEdited,in_tag);
            // Some edits only announce `edit`; subscribing to both keeps the
            // merge from depending on which one the caller emitted.
            in_tag->teConnect(teCallbackType::edit,this,&hair_and_eyes_color_list::onHairTagEdited,in_tag);
            teTagWidgetBase*widget =taginsert(-1,in_tag,taglistwidget->showing_list);
            extraWidgetPushBack(in_tag,widget);
        }

        /**
         * @brief Merges `in_tag` into a hair tag that is already linked.
         *
         * Returns true when it did: `in_tag` is marked as retired and the caller
         * has to retire it from the list. This is what filter() used to do from
         * inside its own predicate.
         */
        /// Eyes only take part in the gradient merge: danbooru has no
        /// "[colour] streaks" for eyes, and the [s] button is not offered there.
        bool mergeKindAllowed(std::shared_ptr<teTag> tag,HairKind kind) const {
            if(!tag||kind==HairKind::None)
                return false;
            if(isEyesTag(*tag))
                return kind==HairKind::RawGradientHair||kind==HairKind::GradientHair;
            return true;
        }
        /// The first candidate of `wanted` kind in this list (same category).
        std::shared_ptr<teTag> findRawSibling(std::shared_ptr<teTag> anchor,HairKind wanted) const {
            if(!layout||!anchor)
                return nullptr;
            for(int i=layout->count()-1;i>=0;--i){
                auto* widget=dynamic_cast<teRefTagWidget*>(layout->itemAt(i)->widget());
                if(!widget||!widget->core)
                    continue;
                std::shared_ptr<teTag> candidate=widget->core;
                if(candidate==anchor||candidate->retired)
                    continue;
                if(!sameHairCategory(*candidate,*anchor))
                    continue;
                if(classify_tag(candidate)==wanted)
                    return candidate;
            }
            return nullptr;
        }
        /**
         * @brief Folds any remaining raw tag into a survivor that just merged.
         *
         * Adding "brown hair" next to "blue streaks" and "blonde streaks" used to
         * absorb only the last one: the merge edits the survivor, and that edit
         * re-enters this control under the merge guard, so the second raw tag was
         * never looked at. This runs the remaining rounds explicitly.
         */
        void foldRawSiblings(std::shared_ptr<teTag> survivor) {
            for(int round=0;round<8;++round){
                if(!survivor||survivor->retired)
                    return;
                const HairKind kind=classify_tag(survivor);
                const HairKind wanted=kind==HairKind::StreakHair?HairKind::RawStreakHair
                                    :kind==HairKind::GradientHair?HairKind::RawGradientHair
                                    :HairKind::None;
                if(wanted==HairKind::None)
                    return;
                std::shared_ptr<teTag> extra=findRawSibling(survivor,wanted);
                if(!extra||!mergeAllowed(*survivor,*extra))
                    return;
                markMerged(*survivor,*extra);
                extra->retired=true;
                // Nothing edits the absorbed tag, so no re_read() will retire it
                // for us: drop it from the list here (readList() does the same for
                // the tags it reads itself).
                if(taglistwidget)
                    taglistwidget->retire(extra);
                const QStringList colors=extract_colors(survivor)+extract_colors(extra);
                if(kind==HairKind::StreakHair)
                    taglistwidget->tagEdit(survivor,streakedHairWords(colors).join(qsl(" ")));
                else
                    taglistwidget->tagEdit(survivor,
                        gradientHairText(colors,isEyesTag(*survivor)));
            }
        }
        bool absorb(std::shared_ptr<teTag> in_tag){
            if(!autoMerge||!MergeSwitch)
                return false;
            // 多人图(girl+boy+other > 1)不再自动合并渐变/挑染:两条渐变、
            // 两条挑染可能属于不同的人。需要合并时用列表右键的 merge
            // (Tab / Shift+M)手动合并。
            if(isMultiPeople(taglistwidget?taglistwidget->showing_list:nullptr))
                return false;
            // A merge edits the surviving tag, which re-enters this control
            // through its edit signals. Without this guard the nested call can
            // start a second merge on the tag that is being merged right now.
            if(m_merging)
                return false;
            if(!in_tag||in_tag->retired)
                return false;
            if(is_color(*in_tag).second<=0)
                return false;
            const HairKind new_type = classify_tag(in_tag);
            if(new_type==HairKind::None){
                telog("[hair_and_eyes]:unable to distinguish hair tag's type");
                return false;
            }
            if(!mergeKindAllowed(in_tag,new_type))
                return false;
            MergeGuard guard(m_merging);
            std::shared_ptr<teTag> target_tag = find_merge_target(in_tag,new_type);
            if(!target_tag)
                return false;
            if(!mergeAllowed(*target_tag,*in_tag))
                return false;
            markMerged(*target_tag,*in_tag);
            in_tag->retired=true;
            perform_merge(target_tag,in_tag,new_type);
            foldRawSiblings(target_tag);
            return true;
        }

        /**
         * @brief 手动合并(列表右键 merge / Tab / Shift+M)。
         *
         * 规则与 clothes 列表一致:多选从上到下链式合并;单选级联(先向上、
         * 后向下)。手动合并不看 autoMerge / mergeSuppressed——多人模式把
         * 自动合并收紧后,这里是用户显式的补充入口;markMerged 照常,撤销
         * 仍然整步还原。
         */
        bool manualMergeSupported() const override { return true; }
        void manualMerge()override{
            if(!taglistwidget||!taglistwidget->showing_list)
                return;
            const QVector<std::shared_ptr<teTag>> chosen=selectedCoresInVisualOrder();
            if(chosen.isEmpty())
                return;
            if(chosen.size()>=2){
                for(int i=1;i<int(chosen.size());++i){
                    const std::shared_ptr<teTag>& tag=chosen[i];
                    // 幸存者始终是最上面那条;方向由合并矩阵决定颜色次序。
                    if(!tag||tag->retired||chosen[i-1]->retired)
                        continue;
                    if(manualMergePair(chosen[i-1],tag))
                        taglistwidget->retire(tag);
                }
                return;
            }
            // 单选:级联。anchor 是当前活着的 tag;并入上方邻居后它变成邻居。
            // 每个方向先试"邻居幸存"(上面的幸存,符合从上到下),再试
            // "anchor 幸存"(矩阵只允许这个方向时)。
            std::shared_ptr<teTag> anchor=chosen.first();
            for(int round=0;round<64&&anchor&&!anchor->retired;++round){
                std::shared_ptr<teTag> neighbour=findManualMergeNeighbour(anchor,true);
                if(neighbour&&manualMergePair(neighbour,anchor)){
                    taglistwidget->retire(anchor);
                    anchor=neighbour;
                    continue;
                }
                if(neighbour&&manualMergePair(anchor,neighbour)){
                    taglistwidget->retire(neighbour);
                    continue;
                }
                neighbour=findManualMergeNeighbour(anchor,false);
                if(neighbour&&manualMergePair(anchor,neighbour)){
                    taglistwidget->retire(neighbour);
                    continue;
                }
                if(neighbour&&manualMergePair(neighbour,anchor)){
                    taglistwidget->retire(anchor);
                    anchor=neighbour;
                    continue;
                }
                break;
            }
        }
        /// 把 `source` 并进 `target`(方向由类型矩阵决定),成功返回 true。
        bool manualMergePair(std::shared_ptr<teTag> target,std::shared_ptr<teTag> source){
            if(!target||!source||target==source||target->retired||source->retired)
                return false;
            if(!sameHairCategory(*target,*source))
                return false;
            const HairKind sourceType=classify_tag(source);
            const HairKind targetType=classify_tag(target);
            if(sourceType==HairKind::None||targetType==HairKind::None)
                return false;
            if(!can_merge(target,targetType,source,sourceType))
                return false;
            MergeGuard guard(m_merging);
            markMerged(*target,*source);
            source->retired=true;
            perform_merge(target,source,sourceType);
            return true;
        }
        /// `anchor` 最近的、与它可合并的邻居(向上或向下),没有则 nullptr。
        /// 纯查找,不改任何东西;合并方向由级联处的矩阵判定。
        std::shared_ptr<teTag> findManualMergeNeighbour(
            const std::shared_ptr<teTag>& anchor,bool up)
        {
            if(!layout||!anchor)
                return {};
            const int anchorIndex=findindex(anchor);
            const int count=layout->count();
            if(anchorIndex<0)
                return {};
            for(int step=1;step<count;++step){
                const int index=up?anchorIndex-step:anchorIndex+step;
                if(index<0||index>=count)
                    break;
                auto* widget=dynamic_cast<teRefTagWidget*>(layout->itemAt(index)->widget());
                if(!widget||!widget->core||widget->core->retired)
                    continue;
                const std::shared_ptr<teTag> candidate=widget->core;
                if(candidate==anchor)
                    continue;
                if(!sameHairCategory(*candidate,*anchor))
                    continue;
                const HairKind candidateType=classify_tag(candidate);
                const HairKind anchorType=classify_tag(anchor);
                if(candidateType==HairKind::None||anchorType==HairKind::None)
                    continue;
                // 任一方向能合并即算可合并;谁幸存由级联处按矩阵决定。
                if(can_merge(candidate,candidateType,anchor,anchorType)
                    ||can_merge(anchor,anchorType,candidate,candidateType))
                    return candidate;
            }
            return {};
        }

        void unlink(std::shared_ptr<teTag>in_tag)override{
            teDisconnect(in_tag.get());
            linked_tags.erase(in_tag);
            tagErase(in_tag);
        }
        void extraWidgetPushBack(std::shared_ptr<teTag>in_tag,teTagWidgetBase*widget){
            const HairKind kind = classify_tag(in_tag);
            if(kind!=HairKind::BaseHair&&kind!=HairKind::RawStreakHair
                &&kind!=HairKind::RawGradientHair)
                return;
            const bool eyes=isEyesTag(*in_tag);
            const int wordcount = in_tag->words.size();

            // No [s] for eyes: danbooru has no "[colour] streaks" for them.
            if(!eyes){
                QPushButton*streaked_btn = new QPushButton("s",widget);
                streaked_btn->setStyleSheet(qsl(R"(font:8pt "Sonsolas")"));
                streaked_btn->setCheckable(true);
                streaked_btn->setFixedSize(15,15);
                streaked_btn->setChecked(kind==HairKind::RawStreakHair);
                connect(streaked_btn,&QPushButton::clicked,widget,[in_tag, this](bool ifchecked){
                    // An explicit s/g click is a user edit and clears the
                    // "already merged" mark.
                    in_tag->mergeSuppressed=false;
                    if(ifchecked){
                        if(in_tag->contains(qsl("streaks")))
                            return;                 // already a streak tag
                        // Danbooru wording: "[colour] streaks", not
                        // "[colour] streaked hair".
                        taglistwidget->tagEdit(in_tag,rawStreakText(extract_colors(in_tag)));
                        mergeNow(in_tag);
                    }else{
                        if(!in_tag->contains(qsl("streaks")))
                            return;
                        // Back to the plain base tag.
                        taglistwidget->tagEdit(in_tag,
                            extract_colors(in_tag).join(qsl(" "))+qsl(" hair"));
                    }
                    if(in_tag->retired)
                        taglistwidget->tagErase(in_tag);
                },Qt::DirectConnection);
                widget->insertExtraWidgets(this,streaked_btn);
            }

            QPushButton*gradient_btn = new QPushButton("g",widget);
            gradient_btn->setStyleSheet(qsl(R"(font:8pt "Sonsolas")"));
            gradient_btn->setCheckable(true);
            gradient_btn->setFixedSize(15,15);
            for(int i =0;i<wordcount;++i)
                if(*in_tag->words[i]==qsl("gradient"))
                    gradient_btn->setChecked(true);
            connect(gradient_btn,&QPushButton::clicked,widget,[in_tag, this, gradient_btn](bool ifchecked){
                in_tag->mergeSuppressed=false;
                auto* editorTag=tagWidgetFor(in_tag);
                if(ifchecked){
                    if(in_tag->contains(qsl("gradient")))
                        return;                 // already there: never a second one
                    if(editorTag)
                        editorTag->insertWord(-2,qsl("gradient"));
                    mergeNow(in_tag);
                }else{
                    // See the "streaked" button above: no signalling between the
                    // removals, one announcement at the end.
                    bool removed=false;
                    for(int i=in_tag->words.size()-1;i>=0;--i)
                        if(*in_tag->words[i]==qsl("gradient")){
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
                if(in_tag->retired)
                    taglistwidget->tagErase(in_tag);
            },Qt::DirectConnection);
            widget->insertExtraWidgets(this,gradient_btn);
        }

        /// The add-tag window keeps its buttons; only the tag it writes changes
        /// ("blue streaked hair" -> "blue streaks", see composeHairTagText()).
        void reciveWidgetSignal(QString data,bool ifadd)override{
            teTagListControl::reciveWidgetSignal(composeHairTagText(data),ifadd);
        }
    };

struct BangsList : public teTagListControl{
    std::shared_ptr<teTag>editcore=nullptr;
    BangsList(colorsWidget*in_onEdit_widget,TagListView*parentlist,QWidget*parent = nullptr,QString*styleSheet=nullptr)
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
    bool filter(std::shared_ptr<teTag> in_tag) const override{
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
        // The rules live in ponytailAfterClick() (pure, testable): twin excludes
        // pony and side, pony excludes twin, and a kind button implies pony when
        // no tail is chosen yet.
        QVector<bool> checked(buttons.size());
        for(int i=0;i<buttons.size();++i)
            checked[i]=buttons[i]->isChecked();
        const PonytailAfterClick after=ponytailAfterClick(id,checked);
        for(int i=0;i<buttons.size();++i)
            if(buttons[i]->isChecked()!=after.checkedIn[i])
                buttons[i]->setChecked(after.checkedIn[i]);
        allwidgets[tail].data=tailWord(after.twintails);

        // What is remembered per tail (kindOfPonytail / kindOfTwintails) stays
        // UI state: it is what the buttons should look like when the user comes
        // back to a tail they used before.
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
                if(!mergeAllowed(*twintailsTag,*tag))
                    return false;
                markMerged(*twintailsTag,*tag);
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
                if(!mergeAllowed(*ponytailTag,*tag))
                    return false;
                markMerged(*ponytailTag,*tag);
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
    /// A "tail" tag: the word "tail"/"twintails" plus the kinds this group knows.
    ///
    /// Pure - the merge of a second tail tag is absorb()'s job now, called from
    /// read(); deciding whether a tag belongs here no longer removes anything.
    bool filter(std::shared_ptr<teTag> tag) const override{
        if(!tag)
            return false;
        QString tagstring=*tag;
        int index = tagstring.lastIndexOf("tail");
        if(index<0)return false;
        else if(index==tagstring.size()-5)
            tagstring.chop(1);
        return defaultFiltStrings.find(tagstring)!=defaultFiltStrings.end();
    }

    TagHandling read(std::shared_ptr<teTag> tag) override {
        if(!tag)
            return TagHandling::NotMine;
        if(tag->retired){
            if(taglistwidget)
                taglistwidget->retire(tag);
            return TagHandling::Consumed;
        }
        if(!filter(tag))
            return TagHandling::NotMine;
        // A second "ponytail"/"twintails" tag is merged into the one already
        // there instead of being linked twice.
        if(autoMerge&&MergeSwitch&&merge(tag)){
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
        }
    /**
     * @brief 多人放宽:girl+boy+other > 1(或 multiple peoples 激活)时,
     *        四个长度互不排斥、不再控制同一条 tag,而且 very 与 long 解绑——
     *        very 只写 "very long hair",long 只写 "long hair",组内不再自动
     *        合并两者(absorbLongHair 跳过)。单人时保持原逻辑:互斥、very
     *        隐含 long、自动合并 very long hair 和 long hair,禁止
     *        "long short hair"。
     */
    bool multiTagMode() const override {
        return isMultiPeople(taglistwidget?taglistwidget->showing_list:nullptr);
    }
    /// 一个长度按钮负责的整条 tag("very long hair",不是 "very hair")。
    QString tagTextForButton(int buttonId) const override {
        switch(buttonId){
        case HairVeryLong: return qsl("very long hair");
        case HairLong:     return qsl("long hair");
        case HairShort:    return qsl("short hair");
        case HairMedium:   return qsl("medium hair");
        }
        return QString();
    }
    /// 本组是否已链接 `text` 这条 tag。Pure。
    bool linkedTextIs(const QString& text) const {
        for(const std::shared_ptr<teTag>& tag:linked_tags)
            if(tag&&static_cast<QString>(*tag)==text)
                return true;
        return false;
    }
    virtual void reform(int id)override {
        // The rules live in hairLengthAfterClick() (pure, testable), including the
        // long/short/medium exclusivity arithmetic.
        QVector<bool> checked(buttons.size());
        for(int i=0;i<buttons.size();++i)
            checked[i]=buttons[i]->isChecked();
        const QVector<bool> after=hairLengthAfterClick(id,checked);
        for(int i=0;i<buttons.size();++i)
            if(buttons[i]->isChecked()!=after[i])
                buttons[i]->setChecked(after[i]);
    }
    void refreshState()override{
        if(multiTagMode()){
            // 多人时按钮按整条 tag 对号入座:"very long hair" 只点亮 very,
            // 不再点亮 long(解绑)。
            for(int i=0;i<4;++i)
                buttons[i]->setChecked(linkedTextIs(tagTextForButton(i)));
            return;
        }
        teTagButtonGroup::refreshState();
    }

    std::shared_ptr<teTag>long_hair_ptr=nullptr;
    std::shared_ptr<teTag>very_long_hair_ptr=nullptr;

    /// Is this one of the two tags this control writes? Pure.
    ///
    /// The two iterators this used to compare against were handles into
    /// defaultFiltStrings; the strings say the same thing and cannot be left
    /// dangling by a later insert into that set.
    bool filter(std::shared_ptr<teTag> tag) const override{
        if(!tag)
            return false;
        return defaultFiltStrings.find(*tag)!=defaultFiltStrings.end();
    }

    /**
     * @brief "very long hair" wins over "long hair": only one of them survives.
     *
     * This is the merge filter() used to perform, as its own step: the control
     * only reads tags, and a tag that came back as "long hair" while "very long
     * hair" is already linked is dropped instead of being shown twice.
     */
    void absorbLongHair(std::shared_ptr<teTag> tag){
        const QString text = *tag;
        if(text==qsl("long hair")){
            if(very_long_hair_ptr==tag)
                very_long_hair_ptr=nullptr;
            if(very_long_hair_ptr){
                if(!mergeAllowed(*very_long_hair_ptr,*tag)){
                    long_hair_ptr=tag;          // both kept, both marked: no merge
                    return;
                }
                markMerged(*very_long_hair_ptr,*tag);
                tag->retired=true;
                long_hair_ptr=nullptr;
                return;
            }
            long_hair_ptr=tag;
        }else if(text==qsl("very long hair")){
            if(long_hair_ptr==tag)
                long_hair_ptr=nullptr;
            very_long_hair_ptr=tag;
            if(long_hair_ptr){
                std::shared_ptr<teTag> superseded = long_hair_ptr;
                long_hair_ptr=nullptr;
                if(tag!=superseded&&mergeAllowed(*tag,*superseded)){
                    markMerged(*tag,*superseded);
                    if(taglistwidget)
                        taglistwidget->retire(superseded);
                }
            }
        }
    }

    TagHandling read(std::shared_ptr<teTag>tag)override{
        if(!tag)
            return TagHandling::NotMine;
        if(tag->retired){
            if(taglistwidget)
                taglistwidget->retire(tag);
            return TagHandling::Consumed;
        }
        if(!filter(tag))
            return TagHandling::NotMine;
        // 多人时不自动合并 very long hair / long hair(解绑,见 multiTagMode())。
        if(autoMerge&&MergeSwitch&&!multiTagMode())
            absorbLongHair(tag);
        if(tag->retired){
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
        if(tag==long_hair_ptr)long_hair_ptr=nullptr;
        else if(tag==very_long_hair_ptr)very_long_hair_ptr=nullptr;
        teTagButtonGroup::unlink(tag);
    }
    void resetPolicy()override{
        long_hair_ptr=nullptr;
        very_long_hair_ptr=nullptr;
    }
};
