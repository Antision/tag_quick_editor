#ifndef TEEDITORCONTROL_H
#define TEEDITORCONTROL_H
#include"tetag.h"
#include"tereftaglistwidget.h"
#include"tesignalwidget.h"
class teTagListView;
struct teEditor;
struct anytype{
    template<typename...Args>
    anytype(Args...args){}
    template<typename T>
    operator T&&(){}
};

class teEditorControl:virtual public teObject{
public:
    std::set<std::shared_ptr<teTag>>linked_tags;
    teEditor* editor=nullptr;
    teTagListView* taglistwidget=nullptr;
    std::set<QString>captureList;
    /// "Is this my tag?" - deliberately still a plain question, not a TagHandling:
    /// it changes nothing and only the control itself decides what to do with the
    /// answer (that is read()).
    virtual bool filter(std::shared_ptr<teTag>tag)=0;
    /// Calls filter() from outside. Only the regression check uses it, to
    /// attribute the cost of reading a tag list to the control causing it.
    bool filterForTiming(std::shared_ptr<teTag> tag){ return filter(tag); }
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
    virtual void setTaglistwidget(teTagListView* in_taglistwidget);
    /**
     * @brief The widget that displays `core` in this editor's tag list.
     *
     * The editors used to reach a tag's widget with `core->widget`, which was
     * the single image tag list's widget. That list draws its tags through a
     * delegate now and owns no widget per tag, so the editor has to resolve the
     * widget it made itself (returns nullptr when the tag is not shown here).
     */
    teTagWidgetBase* tagWidgetFor(std::shared_ptr<teTag> core) const;
    void edited();
    virtual ~teEditorControl(){};
};

class teTagButtonGroup:public QWidget,public teEditorControl{
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
    virtual bool filter(std::shared_ptr<teTag>tag)override;
    virtual void refreshState()override;
    virtual void reform(int id){};
    /**
     * @brief Unchecks every button in `others` except `keep`.
     *
     * "Only one of these may be on" was hand written in several button groups
     * (mouth, ears, breast): the same loop over their indices, each time with a
     * different set. Using a QButtonGroup for it is not possible because these
     * groups must allow *no* selection as well, which an exclusive group forbids.
     */
    void uncheckOthers(int keep,std::initializer_list<int> others){
        for(int i:others)
            if(i!=keep&&i>=0&&i<buttons.size())
                buttons[i]->setChecked(false);
    }
    std::unordered_set<QString> defaultFiltStrings;
    virtual void getDefaultFiltStrings();
};
class teTagCheckBox : public QPushButton, public teEditorControl{
    Q_OBJECT
public:
    int excute=1;
    QList<QString> strings;
    teTagCheckBox(QList<QString>&& in_strings,QWidget*parent=nullptr,QString*stylesheet=nullptr);
    virtual void reset()override;
    virtual void clear()override;
    virtual void addString(QList<QString> in_strings);
    virtual void clearString();
    virtual void select();
    virtual void unselect();
    virtual bool filter(std::shared_ptr<teTag>tag)override;
    virtual void onStateChanged(bool state);
    virtual void refreshState()override;
};

class teTagCheckBoxPlus:public teTagCheckBox{
    Q_OBJECT
public:
    using teTagCheckBox::teTagCheckBox;
    std::set<std::shared_ptr<teTag>>second_tags;
    virtual void link2(std::shared_ptr<teTag> tag);
    virtual void unlink2(std::shared_ptr<teTag> tag);
    virtual bool filter2(std::shared_ptr<teTag>tag)=0;
    TagHandling read(std::shared_ptr<teTag>tag)override;
    /// Re-checks one of the two groups this control watches (`taggroup` is 1 for
    /// the first, 2 for the second). It returns nothing: it is used as a callback
    /// and nobody ever looked at the result.
    virtual void re_readGroup(std::shared_ptr<teTag>tag,int taggroup);
    virtual void clear()override;
    bool linked(std::shared_ptr<teTag>tag)override{
        return linked_tags.find(tag)!=linked_tags.end()||second_tags.find(tag)!=second_tags.end();
    }
};

class teTagComboBox : public QComboBox,public teEditorControl{
    Q_OBJECT
public:
    int excute=1;
    QString default_text="...";
    teTagComboBox(QList<QPair<QString,QStringList>>&& string_datas,QString&&default_text, QWidget *parent = nullptr,QString*styleSheet=nullptr);
    virtual bool filter(std::shared_ptr<teTag>tag)override;
    virtual void clear()override;
    virtual void reset()override;
    virtual void onIndexChanged(int index);
    virtual void refreshState()override;
    void wheelEvent(QWheelEvent *event) override {
        event->ignore();
    }
};

class teTagLineedit:public QWidget,public teEditorControl{
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
    QVector<teEditorControl*>children;
    teTagControlGroup(QWidget*in_widget,QVector<teEditorControl*>&&in_children,QWidget* parent=nullptr):QObject(parent),linked_widget(in_widget),children(std::move(in_children)){
        for(teEditorControl*control:children){
            control->teConnect(teCallbackType::edit,this,&teTagControlGroup::onChildControlEdited,control);
        }
    }
    virtual void onChildControlEdited(teEditorControl*)=0;
    virtual ~teTagControlGroup(){}
};

class teTagListControl:public teRefTagListWidget,public teEditorControl {
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
    bool filter(std::shared_ptr<teTag> tag) override;
    colorsWidget*onEdit_widget;
    QHBoxLayout* buttonLayout= new QHBoxLayout();
    teTagListControl(colorsWidget*in_onEdit_widget,teTagListView*parentlist,QWidget*parent = nullptr,QString*styleSheet=nullptr,QString title = QString{});
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
