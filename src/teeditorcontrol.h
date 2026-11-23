#ifndef TEEDITORCONTROL_H
#define TEEDITORCONTROL_H
#include"tetag.h"
#include"tereftaglistwidget.h"
#include"tesignalwidget.h"

#include <QRegularExpression>

class TagListView;
struct teEditor;
struct anytype{
    template<typename...Args>
    anytype(Args...args){}
    template<typename T>
    operator T&&(){}
};

/**
 * @brief 数一数 tag list 里某种"人数"tag 的合计。
 *
 * 纯函数,每次询问现算。识别三种写法:"1girl"/"2girls"(计数)、
 * "6+girls"/"10+others"(数字就取 "+" 前的整数)、"multiple girls"
 * (用户明确写下多人,按 2 计)。breasts 组用它判断 girls>1;
 * mouth/hair length 组用 isMultiPeople()。
 */
inline int countNounTags(teTagList* list,const QString& noun)
{
    if(!list)
        return 0;
    const QRegularExpression counted(QStringLiteral(R"(^(\d+)\+?%1s?$)").arg(noun));
    const QRegularExpression multiple(QStringLiteral(R"(^multiple\s?%1s?$)").arg(noun));
    int total=0;
    for(const std::shared_ptr<teTag>& tag:list->getTags()){
        if(!tag||tag->retired)
            continue;
        const QString text=static_cast<QString>(*tag);
        const auto count=counted.match(text);
        if(count.hasMatch())
            total+=qMax(0,count.captured(1).toInt());
        else if(multiple.match(text).hasMatch())
            total+=2;
    }
    return total;
}

/**
 * @brief 这张图是不是"多人"图(girl+boy+other 合计 > 1)。
 *
 * mouth 与 hair length 的多人放宽条件。"multiple girls" 等 tag 已经把 2
 * 计入对应名词,所以不需要单独判断"multiple peoples"框。
 */
inline bool isMultiPeople(teTagList* list)
{
    return countNounTags(list,QStringLiteral("girl"))
          +countNounTags(list,QStringLiteral("boy"))
          +countNounTags(list,QStringLiteral("other"))>1;
}

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
     * MARK-02 A: a control that gives its tags a look declares the mark here
     * (`mark` = a colour name, an emoji or a PNG path) and the base class stamps
     * it on every linked tag as `tag->setMark(controlId(), mark)`. The delegate
     * then draws it through TagMarkRegistry::resolve(), exactly like the marks
     * the annotation rules produce.
     *
     * The mark lives as long as the claim does: unlink()/clearLinks() drop it,
     * which is what makes "unticking the box removes what ticking it wrote" hold
     * for the look as well. Called from link() and after a claim is dropped.
     * An empty `mark` means "this control paints nothing" and costs one branch.
     */
    virtual void applyMarks();
    /// The mark this control stamps on its linked tags; empty means none.
    QString mark;
    /**
     * @brief The owner id the current marks were written with.
     *
     * Remembered instead of asking controlId() again: clear() runs from
     * destructors, where a virtual call would no longer reach the subclass, and
     * a mark has to be dropped under the same key it was set with.
     */
    QString markOwner;
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
 *
 * -- 互斥按钮的通用规则(2026-11-23 定案,所有 teTagButtonGroup 遵守) ----------
 *
 * 1. **tag list 违反互斥时,顺从 tag list。** 列表里同时存在本组的两条以上
 *    tag(多人图、自动合并被撤销/禁止造成)时,控件不再把它们重写成一个词串:
 *    每条 tag 点亮自己的按钮,用户点灭其中一个只删除它自己的 tag,对其它按钮
 *    的激活态没有任何影响;用户点亮一个灭掉的按钮时,互斥语义恢复——其它按钮
 *    (及其 tag)一起灭掉。
 * 2. **依赖控件的多人放宽。** 一个组可以声明依赖其它控件/人数状态
 *    (multiTagMode()):条件满足时互斥暂时失效,每个点亮的按钮各自维护一个
 *    tag,互不删除;组内仍然有效的自动合并(如 breasts 通称)照常进行。
 *      - breasts:girls > 1(只数 girl,不含 boy/other)时 large/medium/small
 *        互不排斥;"breasts" 是全组通称,点灭它删除整个家族的 tag。
 *      - open/closed mouth:girl+boy+other > 1,或 multiple peoples 激活。
 *      - very long/short/medium hair:同 mouth 条件;multiTagMode() 下 very 与
 *        long 解绑——very 只写 "very long hair",long 只写 "long hair"。
 *        非多人时保持原逻辑:互斥、very 隐含 long、自动合并 very long hair
 *        和 long hair,禁止 "long short hair"。
 * 3. 人数以 countNounTags() 计("1girl"/"2girls"/"6+girls"/"multiple girls"),
 *    每次点击时重新计算,不缓存——人数变化后下一次点击就生效。
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

    /**
     * @brief 多人放宽:该组此刻是否"每个按钮各写一个 tag"。
     *
     * 见 ButtonRuleSet 上方的通用规则。默认关闭;依赖人数的组(breasts、
     * mouth、hair length)按 countNounTags()/isMultiPeople() 现算,不缓存。
     */
    virtual bool multiTagMode() const;
    /**
     * @brief multiTagMode() 下一个按钮对应的 tag 文本。
     *
     * 默认按"只点亮这一个按钮"组合(onClicked() 的同一套词序,共享的 label
     * 词照旧附在后面,得到 "open mouth" / "cat ears");组合规则特殊的组
     * (hair length 的 "very long hair")覆写它。
     */
    virtual QString tagTextForButton(int buttonId) const;
    /// multiTagMode() 的点击:点亮 = 写入该按钮的 tag,点灭 = 只删它。
    void onClickedMulti(int buttonId);
    /// 写入 `tagTextForButton(buttonId)`(已存在同文 tag 时不写)。
    void insertTagForButton(int buttonId);
    /// 删除该按钮负责的 tag(与 tagTextForButton(buttonId) 同文的 linked tag)。
    void eraseTagsForButton(int buttonId);
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
    /// "insert above/below" opened the add widget: the confirmed tag goes below
    /// the current one instead of above it.
    bool m_insertBelow=false;
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
    /// Ctrl+W in the list is the keyboard form of the "+" button.
    void addButtonClicked()override{ onAddButtonClicked(); }
    /// 右键 insert above / below:打开新增窗口,记住插入方向。
    void insertRequested(bool below)override;
    /// The add-tag window produced `data`. Virtual: a control may normalise the
    /// wording before it becomes a tag (the hair list writes "[colour] streaks").
    virtual void reciveWidgetSignal(QString data,bool ifadd);
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
