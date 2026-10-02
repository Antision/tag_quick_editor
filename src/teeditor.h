#ifndef TEEDITOR_H
#define TEEDITOR_H
#include "tetaglistwidget.h"
class teTagWidget;
class teTagList;
struct teTagOperation;
class teTagOperationList;
class teTag;
class teEditorControl;
class teEditorList;
class teEditor:public QFrame,public teObject{
public:
    QString name;
    bool ifrun=true;
    teEditor(teTagListWidget*in_taglistwidget,const QString& name,QWidget*parent=nullptr);
    QList<teEditorControl*>controls;
    teTagListWidget*taglistwidget;
    teTagList*taglist=nullptr;
    virtual void reset();
    virtual bool read(std::shared_ptr<teTag>tag);
    virtual bool re_read(std::shared_ptr<teTag>tag);
    virtual void clear();
    void setTagListWidget(teTagListWidget*listwidget);
};

class teEditor_standard:public teEditor{
public:
    QPushButton *back_to_main_page = new QPushButton(this);
    QPushButton *editor_switch= new QPushButton(this);
    QVBoxLayout *mainLayout=new QVBoxLayout(this);
    QHBoxLayout *titleLayout= new QHBoxLayout;
    QVBoxLayout *contentLayout = new QVBoxLayout;
    QVector<QWidget*>extraInterfaces;
    teEditor_standard(teTagListWidget*in_taglistwidget,const QString& name,QString*styleSheet=nullptr,QWidget*parent=nullptr);
    int showing_interface=0;
    virtual void showInterface(int index = 0);
    virtual void onSwitchToggled(bool state);
    virtual void fold();
    virtual void unfold();
};

#endif // TEEDITOR_H
