#ifndef TEEDITORCONTROL_H
#define TEEDITORCONTROL_H
#include"tetag.h"
#include"tereftaglistwidget.h"
#include"tesignalwidget.h"
class TagListView;
struct teEditor;
struct anytype{
    template<typename...Args>
    anytype(Args...args){}
    template<typename T>
    operator T&&(){}
};

/**
 * @brief What every editor control has in common: tags it claimed, and nothing
 *        else.
 *
 * A teObject because a tag is not a QObject and cannot connect to one: a tag
 * announces "I was edited" / "I am going away" through teObject, so whoever wants
 * to hear it has to be one. Nothing else about this class needs the base - it
 * owns no signal of its own (edited() relays teCallbackType::edit).
 *
 * Concrete controls add this to a widget (a `QPushButton`, a `QComboBox`, ...) or
 * to `teRefTagListWidget`, which is a teObject itself. The virtual base is what
 * makes the two halves of a tag list control share one teObject, so a tag sees a
 * single receiver whether the list half or the control half subscribed. That is
 * also why teDisconnect(tag) on such a control drops both sets.
 */
class EditorControl:virtual public teObject{
public:
    std::set<std::shared_ptr<teTag>>linked_tags;
    teEditor* editor=nullptr;
    TagListView* taglistwidget=nullptr;
    std::set<QString>captureList;
    /// "Is this my tag?" - deliberately still a plain question, not a TagHandling:
    /// it changes nothing and only the control itself decides what to do with the
    /// answer (that is read()).
    virtual bool filter(std::shared_ptr<teTag> tag) const =0;
    /// Calls filter() from outside. Only the regression check uses it, to
    /// attribute the cost of reading a tag list to the control causing it.
    bool filterForTiming(std::shared_ptr<teTag> tag) const { return filter(tag); }
    /// Same for refreshState().
    void refreshStateForTiming(){ refreshState(); }
    /// Re-checks a tag this control already linked (it was edited).
    virtual TagHandling re_read(std::shared_ptr<teTag>tag);
    /// Offers a tag to this control; see TagHandling.
    virtual TagHandling read(std::shared_ptr<teTag>tag);
    virtual void reset()=0;
    virtual void clear()=0;
    /**
     * @brief Drops control-specific state when the control is cleared.
     *
     * The clear() of every control kind calls this *first*, so a subclass never
     * has to override clear() just to forget a pointer or a remembered sub-state
     * (several controls in teeditor_derive.cpp did). What is left in the subclass
     * is its actual policy.
     */
    virtual void resetPolicy(){}
    virtual void link(std::shared_ptr<teTag>in_tag);
    virtual void unlink(std::shared_ptr<teTag> in_tag);
    bool ifrefreshState=true;
    /**
     * @brief Brings the control's own display in line with its linked tags.
     *
     * Not pure: several controls have nothing to show beyond what the base does
     * (they used to write an empty override just to satisfy the pure virtual).
     */
    virtual void refreshState(){}
    virtual bool linked(std::shared_ptr<teTag>tag);
    virtual void setTaglistwidget(TagListView* in_taglistwidget);
    /**
     * @brief The widget that displays `core` in this editor's tag list.
     *
     * The editors used to reach a tag's widget with `core->widget`, which was
     * the single image tag list's widget. That list draws its tags through a
     * delegate now and owns no widget per tag, so the editor has to resolve the
     * widget it made itself (returns nullptr when the tag is not shown here).
     */
    teTagWidgetBase* tagWidgetFor(std::shared_ptr<teTag> core) const;
    /**
     * @brief Drops the claim on one tag: disconnect it, forget it, refresh.
     *
     * This is what unlink() is, and it is what a control with a second set of
     * claimed tags needs as well (see teTagCheckBoxPlus), so it takes the set to
     * drop it from instead of assuming linked_tags.
     */
    void dropClaim(std::shared_ptr<teTag> tag,std::set<std::shared_ptr<teTag>>& tags);
    /**
     * @brief Drops every subscription this control holds on `tags`, then empties it.
     *
     * Every control's clear() spelled this loop out (four copies, in four
     * slightly different orders), and the second set a check box plus claims
     * needed a fifth. Doing it in one place is what makes "a cleared control is
     * never called again" a property of the base class instead of a habit that
     * has to be remembered.
     */
    static void dropAll(std::set<std::shared_ptr<teTag>>& tags,teObject* owner);
    void clearLinks();
    /**
     * @brief May these two tags still be merged automatically?
     *
     * No when both were merged together once already: that is what makes undoing a
     * merge stick (see teTag::mergeSuppressed). One unmarked side is enough to
     * allow it again, which is what editing either tag produces.
     */
    static bool mergeAllowed(const teTag& first,const teTag& second){
        return !(first.mergeSuppressed&&second.mergeSuppressed);
    }
    /**
     * @brief Marks both sides of a merge that is happening now.
     *
     * The mark has to reach the state undo will restore, not only the live tags:
     * the step was opened before this merge, so its copies are marked as well.
     */
    void markMerged(teTag& survivor,teTag& absorbed);
    /**
     * @brief While this is alive the control ignores its own state changes.
     *
     * Several controls guard their own updates with an `excute` counter and a
     * hand-written --excute/++excute pair around the block. A return or an
     * exception in the middle of such a block left the counter off by one, and
     * the control then swallowed every later click. The guard cannot be
     * unbalanced.
     */
    struct UpdateGuard {
        int& depth;
        explicit UpdateGuard(int& d):depth(d){ --depth; }
        ~UpdateGuard(){ ++depth; }
        UpdateGuard(const UpdateGuard&)=delete;
        UpdateGuard& operator=(const UpdateGuard&)=delete;
    };
    /**
     * @brief Announces that this control changed one of its tags.
     *
     * teCallbackType::edit - the same thing a tag says when its text changed, so
     * the group around a control (the label above the "multiple people" row)
     * listens to the control exactly the way it listens to a tag.
     */
    void edited();

    /**
     * @brief A stable name for this control.
     *
     * The secondary button menus name their .ui file after it (SC-10) and the
     * mark rules address controls by it, so it has to come from the control, not
     * from an index in a page. Defaults to the widget's object name.
     */
    virtual QString controlId() const;
    /**
     * @brief Where this control is on screen, in global coordinates.
     *
     * The connection lines (S7) start at the control, and a control is not
     * necessarily the widget it is drawn in, so it answers for itself.
     */
    virtual QRect visualRect() const;
    /**
     * @brief Extension point: repaint the marks this control puts on its tags.
     *
     * Nothing marks anything yet, so nothing calls it: a control claims tags
     * today and the marks arrive with the mark registry (S6), which is also what
     * decides when a claim change is worth a repaint. It exists now so the
     * controls have one agreed place for it instead of each growing its own.
     */
    virtual void applyMarks(){}
    virtual ~EditorControl(){}
};

/**
 * @brief The "which buttons may be on together" rules of a button group, as data.
 *
 * Every group used to encode the same three ideas in its own if-chain over
 * button indices: "only one of these may be on", "this one implies that one",
 * and "that one is meaningless on its own". Written down instead of coded, the
 * rules can be read as a whole and applied by one function (applyButtonRules),
 * which is also what makes them checkable without clicking anything.
 *
 * A group fills `rules` in its constructor and needs no reform() at all; the two
 * groups whose rules are order dependent (ponytail, hair length) keep their own
 * pure functions and are pinned by the regression harness.
 */
struct ButtonRuleSet {
    /// At most one member of each group may be on. The one that was clicked wins.
    QVector<QVector<int>> exclusive;
    /// One button implies another.
    struct Implication {
        int first;
        int second;
        /// Buttons that cancel the implication when one of them is on: "very long
        /// hair" implies "long hair" only when the hair is not short.
        QVector<int> unless;
    };
    QVector<Implication> implies;
    /// A button that is meaningless while none of `others` is on.
    struct Requirement {
        int button;
        QVector<int> others;
    };
    QVector<Requirement> needsOneOf;
};

/**
 * @brief The button states after `clicked` was toggled, with `rules` applied.
 *
 * Pure: `checkedIn` is the state Qt delivered (the clicked button already has its
 * new state) and the result is what the buttons have to be set to. The order is
 * what a user expects from a click: the button just turned off takes the ones
 * that imply it with it, the exclusivity of the button just turned on wins, and
 * only then are implications followed - so a rule can never re-check a button the
 * user just unchecked.
 */
inline QVector<bool> applyButtonRules(int clicked,const QVector<bool>& checkedIn,
                                      const ButtonRuleSet& rules)
{
    QVector<bool> checked = checkedIn;
    auto on=[&](int i){ return i>=0&&i<checked.size()&&checked[i]; };
    auto set=[&](int i,bool state){ if(i>=0&&i<checked.size()) checked[i]=state; };

    // 1. The clicked button was turned off: nothing that implies it may stay on.
    if(!on(clicked))
        for(const ButtonRuleSet::Implication& rule:rules.implies)
            if(rule.second==clicked)
                set(rule.first,false);

    // 2. Exclusivity: the clicked button wins its groups.
    if(on(clicked))
        for(const QVector<int>& group:rules.exclusive)
            if(group.contains(clicked))
                for(int other:group)
                    if(other!=clicked)
                        set(other,false);

    // 3. What the clicked button implies.
    if(on(clicked))
        for(const ButtonRuleSet::Implication& rule:rules.implies){
            if(rule.first!=clicked)
                continue;
            bool blocked=false;
            for(int unless:rule.unless)
                blocked = blocked||on(unless);
            if(!blocked)
                set(rule.second,true);
        }

    // 4. A button that needs company.
    for(const ButtonRuleSet::Requirement& rule:rules.needsOneOf){
        if(!on(rule.button))
            continue;
        bool has=false;
        for(int other:rule.others)
            has = has||on(other);
        if(!has)
            set(rule.button,false);
    }
    return checked;
}

class teTagButtonGroup:public QWidget,public EditorControl{
    Q_OBJECT
public:
    struct item{
        QWidget*widget;
        QString data;
        bool ifbutton;
        bool ifspace=true;
        bool ifvariablePos=false;
    };
    struct in_item{
        QString text;
        QString data;
        bool ifbutton;
        bool ifspace=true;
        bool ifvariablePos=false;
    };
    QHBoxLayout*layout;
    QButtonGroup* group;
    QVector<QPushButton*>buttons;
    QVector<item>allwidgets;
    teTagButtonGroup(const QVector<in_item>&in_buttons,QWidget*parent=nullptr,QString* styleSheet_button=nullptr,QString* styleSheet_label=nullptr);
    virtual void reset()override;
    virtual void onClicked(int id);
    virtual void clear()override;
    virtual bool filter(std::shared_ptr<teTag> tag) const override;
    virtual void refreshState()override;
    /// The group's rules; a subclass fills this in and gets reform() for free.
    ButtonRuleSet rules;
    /**
     * @brief Sets the buttons to the states the group's rules ask for.
     *
     * This is the whole of "reforming" a group: read the states, apply the rules,
     * write back the ones that differ. Groups whose rules are order dependent
     * (ponytail, hair length) override reform() with their own pure function.
     */
    void applyRules(int id){
        QVector<bool> checked(buttons.size());
        for(int i=0;i<buttons.size();++i)
            checked[i]=buttons[i]->isChecked();
        const QVector<bool> after = applyButtonRules(id,checked,rules);
        for(int i=0;i<buttons.size();++i)
            if(buttons[i]->isChecked()!=after[i])
                buttons[i]->setChecked(after[i]);
    }
    virtual void reform(int id){ applyRules(id); }
    std::unordered_set<QString> defaultFiltStrings;
    virtual void getDefaultFiltStrings();
};
class teTagCheckBox : public QPushButton, public EditorControl{
    Q_OBJECT
public:
    int excute=1;
    QList<QString> strings;
    teTagCheckBox(QList<QString>&& in_strings,QWidget*parent=nullptr,QString*stylesheet=nullptr);
    virtual void reset()override;
    virtual void clear()override;
    virtual void addString(QList<QString> in_strings);
    virtual void clearString();
    /**
     * @brief Deletes every tag this control owns from the tag list.
     *
     * "Unticking the box removes what ticking it wrote." The erased tags announce
     * their destruction and unlink() drops each one from linked_tags as it goes,
     * which is why this loops on the container instead of copying it first.
     */
    void eraseOwnedTags();
    virtual void select();
    virtual void unselect();
    virtual bool filter(std::shared_ptr<teTag> tag) const override;
    virtual void onStateChanged(bool state);
    virtual void refreshState()override;
};

/**
 * @brief A check box that claims two kinds of tag.
 *
 * "Multiple people" is the only user: the box stands for `multiple girls` (the
 * tags it *is*, filter()) and is informed by `2girls` / `5boys` (the tags it
 * *counts*, filter2()). Ticking it writes one `multiple <nouns>` per counted tag;
 * unticking it deletes the tags it owns.
 *
 * Both sets are claims - a tag in either one is not another control's business -
 * so linked() covers both. What is *not* shared is what they mean, which is why
 * the two sets stay separate rather than being merged into one with a flag: the
 * owned set drives the check state, the counted set never does.
 */
class teTagCheckBoxPlus:public teTagCheckBox{
    Q_OBJECT
public:
    using teTagCheckBox::teTagCheckBox;
    /// Tags that say how many rather than what (the counterpart of linked_tags).
    std::set<std::shared_ptr<teTag>>counted_tags;
    /// The second claim rule: is this a "how many" tag? Pure.
    virtual bool filter2(std::shared_ptr<teTag>tag) const =0;
    /// Claims `tag` as counted - the counterpart of link(), for what it means.
    virtual void linkCounted(std::shared_ptr<teTag> tag);
    /// Drops a counted claim - the counterpart of unlink().
    virtual void unlinkCounted(std::shared_ptr<teTag> tag);
    TagHandling read(std::shared_ptr<teTag>tag)override;
    /// An owned tag was edited: drop it if it is no longer one of ours.
    virtual void re_readOwned(std::shared_ptr<teTag>tag);
    /// A counted tag was edited: same, against filter2().
    virtual void re_readCounted(std::shared_ptr<teTag>tag);
    virtual void clear()override;
    bool linked(std::shared_ptr<teTag>tag)override{
        return EditorControl::linked(tag)||counted_tags.find(tag)!=counted_tags.end();
    }
};

class teTagLineedit:public QWidget,public EditorControl{
    Q_OBJECT
public:
    QHBoxLayout* layout = new QHBoxLayout(this);
    struct autoSelectLineedit:QLineEdit{
        virtual void focusInEvent(QFocusEvent*e)override{
            setFocus();
            QTimer::singleShot(0, this, &QLineEdit::selectAll);
            QLineEdit::focusInEvent(e);
        }
    };
    autoSelectLineedit* lineedit = new autoSelectLineedit{};
    QLabel* label=nullptr;
    teTagLineedit(QString &&text);
    virtual void onEditingFinished();
    virtual void clear()override;
    virtual void reset()override;
    virtual void refreshState()override{}
};

class teTagControlGroup:public QObject,public teObject{
    Q_OBJECT
public:
    QWidget*linked_widget=nullptr;
    QVector<EditorControl*>children;
    teTagControlGroup(QWidget*in_widget,QVector<EditorControl*>&&in_children,QWidget* parent=nullptr):QObject(parent),linked_widget(in_widget),children(std::move(in_children)){
        for(EditorControl*control:children)
            control->teConnect(teCallbackType::edit,this,&teTagControlGroup::onChildControlEdited,control);
    }
    virtual void onChildControlEdited(EditorControl*)=0;
    virtual ~teTagControlGroup(){}
};

class teTagListControl:public teRefTagListWidget,public EditorControl {
    Q_OBJECT
public:
    bool ifedit=false;
    /**
     * @brief Words (or whole tags) this control claims.
     *
     * A control whose filter is "these words are mine" only has to fill this in
     * its constructor: the shared filter() below tests both the tag's last word
     * and the whole tag text against it. Controls with a real pattern (the
     * object list's three-word shape, the clothes merge, the hair colours) still
     * override filter() - their rule is not a word list.
     */
    QSet<QString> vocabulary;
    /// Optional extra test, used together with `vocabulary`.
    std::function<bool(const teTag&)> accept;
    /// teEditorControl: the shared word-list filter (override for real patterns).
    bool filter(std::shared_ptr<teTag> tag) const override;
    colorsWidget*onEdit_widget;
    QHBoxLayout* buttonLayout= new QHBoxLayout();
    teTagListControl(colorsWidget*in_onEdit_widget,TagListView*parentlist,QWidget*parent = nullptr,QString*styleSheet=nullptr,QString title = QString{});
    ~teTagListControl(){
        delete onEdit_widget;
    }
    void link(std::shared_ptr<teTag>in_tag)override;
    void unlink(std::shared_ptr<teTag>in_tag)override;
    virtual void onAddButtonClicked();
    void reciveWidgetSignal(QString data,bool ifadd);
    void reciveDestroySignal();
    void reciveCancelSignal(){
        ifedit=false;
    }
    virtual void onSubButtonClicked(){
        tagDestroy();
    }
    virtual void reset()override{
        teRefTagListWidget::clear();
    };
    virtual void tagEdit(teTagWidgetBase*tag,teWordWidgetBase*word)override;
    virtual void setSelectCurrent(teTagWidgetBase*in=nullptr,bool ifclear=true)override;
    virtual void setSelect(teTagWidgetBase* in)override;
    virtual int setUnselect(teTagWidgetBase*in=nullptr)override;;
    virtual void clear()override;;
};
#endif // TEEDITORCONTROL_H
