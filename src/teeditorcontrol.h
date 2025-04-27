#ifndef TEEDITORCONTROL_H
#define TEEDITORCONTROL_H
#include"tetag.h"
#include"tereftaglistwidget.h"
#include"tesignalwidget.h"
struct teTagListWidget;
struct teEditor;
struct anytype{
    template<typename...Args>
    anytype(Args...args){}
    template<typename T>
    operator T&&(){}
};

struct teEditorControl:virtual teObject{
    std::set<std::shared_ptr<tetagcore>>linked_tags;
    teEditor* editor;
    teTagListWidget* taglistwidget=nullptr;
    std::set<QString>captureList;
    virtual bool filter(std::shared_ptr<tetagcore>tag)=0;
    virtual bool re_read(std::shared_ptr<tetagcore>tag);
    virtual bool read(std::shared_ptr<tetagcore>tag);
    virtual void reset()=0;
    virtual void clear()=0;
    virtual void link(std::shared_ptr<tetagcore>in_tag);
    virtual void unlink(std::shared_ptr<tetagcore> in_tag);
    bool ifrefreshState=true;
    virtual void refreshState()=0;
    virtual bool linked(std::shared_ptr<tetagcore>tag);
    virtual void setTaglistwidget(teTagListWidget* in_taglistwidget);
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
    virtual bool filter(std::shared_ptr<tetagcore>tag)override;
    virtual void refreshState()override;
    virtual void reform(int id){};
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
    virtual bool filter(std::shared_ptr<tetagcore>tag)override;
    virtual void onStateChanged(bool state);
    virtual void refreshState()override;
};

class teTagCheckBoxPlus:public teTagCheckBox{
    Q_OBJECT
public:
    using teTagCheckBox::teTagCheckBox;
    std::set<std::shared_ptr<tetagcore>>second_tags;
    virtual void link2(std::shared_ptr<tetagcore> tag);
    virtual void unlink2(std::shared_ptr<tetagcore> tag);
    virtual bool filter2(std::shared_ptr<tetagcore>tag)=0;
    bool read(std::shared_ptr<tetagcore>tag)override;
    virtual bool re_read(std::shared_ptr<tetagcore>tag,int taggroup);
    virtual void clear()override;
    bool linked(std::shared_ptr<tetagcore>tag)override{
        return linked_tags.find(tag)!=linked_tags.end()||second_tags.find(tag)!=second_tags.end();
    }
};

class teTagComboBox : public QComboBox,public teEditorControl{
    Q_OBJECT
public:
    int excute=1;
    QString default_text="...";
    teTagComboBox(QList<QPair<QString,QStringList>>&& string_datas,QString&&default_text, QWidget *parent = nullptr,QString*styleSheet=nullptr);
    virtual bool filter(std::shared_ptr<tetagcore>tag)override;
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
    colorsWidget*onEdit_widget;
    QHBoxLayout* buttonLayout= new QHBoxLayout();
    teTagListControl(colorsWidget*in_onEdit_widget,teTagListWidget*parentlist,QWidget*parent = nullptr,QString*styleSheet=nullptr);
    ~teTagListControl(){
        delete onEdit_widget;
    }
    void link(std::shared_ptr<tetagcore>in_tag)override;
    void unlink(std::shared_ptr<tetagcore>in_tag)override;
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
    virtual void tagEdit(teTagBase*tag,teWordBase*word)override;
    virtual void setSelectCurrent(teTagBase*in=nullptr,bool ifclear=true)override;
    virtual void setSelect(teTagBase* in)override;
    virtual int setUnselect(teTagBase*in=nullptr)override;;
    virtual void clear()override;;
};
#endif // TEEDITORCONTROL_H
