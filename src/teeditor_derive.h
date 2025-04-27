#ifndef TEEDITOR_DERIVE_H
#define TEEDITOR_DERIVE_H

#include "teeditor.h"
#include "tesignalwidget.h"
class teEditor_custom:public teEditor{
public:
    QVBoxLayout*mainLayout=new QVBoxLayout(this);
    QFlowLayout*flowLayout=new QFlowLayout;
    teEditor_custom(teTagListWidget*in_taglistwidget,QString&& name=QStringLiteral("custom"),QString*styleSheet=nullptr,QWidget*parent=nullptr);
    QMap<QString,teEditorControl*> string_controls;
    customControlWidget controlWidget;
    void removeControl(const QString& str);
    void addControl(const QString & str);
    ~teEditor_custom(){}
};

class teEditor_pretreat:public teEditor_standard{
public:
    teEditor_pretreat(teTagListWidget*in_taglistwidget,QString&& name=QStringLiteral("pretreat"),QString*styleSheet=nullptr,QWidget*parent=nullptr);
    ~teEditor_pretreat(){}
    enum{
        peoplePage=1
    };
};

class teEditor_hair_and_eyes : public teEditor_standard
{
public:
    teEditor_hair_and_eyes(teTagListWidget*in_taglistwidget,QString&& name=QStringLiteral("hair and eyes"),QString*styleSheet=nullptr,QWidget*parent=nullptr);
    ~teEditor_hair_and_eyes(){}
};


class teEditor_clothes:public teEditor_standard
{
public:
    teEditor_clothes(teTagListWidget*in_taglistwidget,QString &&name=QStringLiteral("clothes"), QString *styleSheet=nullptr, QWidget *parent=nullptr);
};

class teEditor_nsfw:public teEditor_standard
{
public:
    teEditor_nsfw(teTagListWidget*in_taglistwidget,QString &&name=QStringLiteral("nsfw"), QString *styleSheet=nullptr, QWidget *parent=nullptr);
};
#endif // TEEDITOR_DERIVE_H
