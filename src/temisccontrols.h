/*
 * temisccontrols.h - controls moved out of teeditor_derive.cpp, where they used
 * to be declared inside the editor constructors.
 */
#pragma once
#include "teeditorhelpers.h"

    struct teTagLineedit_people : public teTagLineedit {
        QString objtext = "";
        /**
         * @brief This control's pattern, compiled once.
         *
         * Building (and therefore compiling) the QRegularExpression inside
         * filter() meant a fresh compile for every tag, for every control, on
         * every image switch - that alone was most of the per-image cost of the
         * pretreat editor.
         */
        QRegularExpression regex;
        teTagLineedit_people(QString&& objtext, QString&& text) :teTagLineedit(std::move(text)), objtext(std::move(objtext)),
            regex(QStringLiteral(R"(^(\d+)\+?%1s?$)").arg(this->objtext)) {
            lineedit->setValidator(new QRegularExpressionValidator(QRegularExpression(QStringLiteral(R"(^\d+\+?$)"))));
        }
        QString getText() {
            if (linked_tags.empty()) { return QString{}; }
            else { return (QString)(*(*linked_tags.begin())); }
        }
        bool filter(std::shared_ptr<teTag> tag)override {
            if (!tag) return false;
            QString tagStr = static_cast<QString>(*tag);
            QRegularExpressionMatch m = regex.match(tagStr);
            if(m.hasMatch()){
                lineedit->setText(m.captured(1));
                return true;
            }else
                return false;
        }
        virtual void refreshState()override{
            if(linked_tags.empty()) {
                lineedit->clear();
                setStyleSheetOnce(this,"color:white;");
            }
            else{
                lineedit->setText(regex.match(((QString)*(*linked_tags.begin()))).captured(1));
                setStyleSheetOnce(this,"color:#07f680;");
            }
            edited();
        }
        void onEditingFinished()override {
            if(!taglistwidget->showing_list)return;
            QString text = lineedit->text();
            QString newTagText;
            int num=0;
            if(text.size()>0&&text.back()==QChar('+'))
                newTagText=text+ objtext + 's';
            else{
                bool ok;
                if(text.size()>0)
                    num=text.toInt(&ok);
                if (num == 0 || lineedit->text().isEmpty()||!ok) {
                    while (!linked_tags.empty())
                        taglistwidget->tagErase(*linked_tags.begin());
                    return;
                }
                if (num == 1) {
                    newTagText = QChar('1') + objtext;
                }
                else if (num >= 6) {
                    newTagText = "6+" + objtext + 's';
                }else {
                    newTagText = QString::number(num) + objtext + 's';
                }
            }
            if (linked_tags.empty()) {
                std::shared_ptr<teTag>newtagcore = std::make_shared<teTag>(newTagText);
                    link(newtagcore);
                taglistwidget->tagInsertAbove(false, newtagcore);
            }else{
                while(linked_tags.size()>1)
                    taglistwidget->tagErase(*(++linked_tags.begin()));
                taglistwidget->tagEdit(*linked_tags.begin(),newTagText);
            }
            edited();
        }
    };

    struct page1_button_link : public teTagControlGroup{
        page1_button_link(QPushButton* linked,QVector<teEditorControl*>&&children,QWidget*parent=nullptr):
            teTagControlGroup(linked,std::move(children),parent){}
        auto extract(const QString&in){
            int people_count=0;
            bool has_suffix=false;
            QChar c='\0';
            if(in.size()>0)
                people_count=in[0].unicode()-'0';
            if(in.size()>1){
                has_suffix=true;
                c=((teTagLineedit_people*)children[1])->lineedit->text()[1];
            }
            bool ok;
            in.toInt(&ok);
            struct{int peoplecount;bool hassuffix;QChar suffix;}ret{people_count,has_suffix,c};
            return ret;
        }
        virtual void refreshState(){
            QString showing_string{};
            int i = 0;
            for(;i<3;++i){
                for(std::shared_ptr<teTag>tag_ptr:((teTagLineedit_people*)children[i])->linked_tags)
                    showing_string+=(*tag_ptr)+',';
            }
            for(;i<7;++i){
                if(((teTagCheckBox*)children[i])->isChecked())
                    for(std::shared_ptr<teTag>tag_ptr:((teTagCheckBox*)children[i])->linked_tags)
                        showing_string+=(*tag_ptr)+',';
            }

            if(!showing_string.isEmpty()){
                showing_string.chop(1);
                setStyleSheetOnce(linked_widget,"color:#07f680;");
            }else{
                showing_string=QStringLiteral("(nobody)");
                setStyleSheetOnce(linked_widget,"color:white;");
            }
            QFontMetrics fontMetrics(((QPushButton*)linked_widget)->font());
            ((QPushButton*)linked_widget)->setText(fontMetrics.elidedText(showing_string, Qt::ElideMiddle, linked_widget->width()));
        }
        virtual void onChildControlEdited(teEditorControl*){
            refreshState();
        }
    };

    struct multipeople_CheckBox : public teTagCheckBoxPlus {
                                                    multipeople_CheckBox(QString key_string1, QWidget* parent = nullptr, QString* stylesheet = nullptr) :
                                                        teTagCheckBoxPlus({ key_string1 }, parent, stylesheet) {
                                                        }
                                                    bool filter(std::shared_ptr<teTag> tag)override {
                                                                                          static QRegularExpression reg(QStringLiteral(R"(^multiple\s?(girl(s)?|boy(s)?|other(s)?)$)"));
    if (reg.match(*tag).hasMatch()) {
        return true;
    }else
        return false;
}
bool filter2(std::shared_ptr<teTag> tag)override {
    static QRegularExpression reg(QStringLiteral(R"(^(2|[3-9]|\d{2,}|\d+\+)(girls|boys|others)$)"));
    if (reg.match(*tag).hasMatch()) {
        return true;
    }
    else
        return false;
}
void onStateChanged(bool state)override {
    if (excute>0) {
        if(!state){
            while(linked_tags.size()>0){
                taglistwidget->tagErase(*linked_tags.begin());
            }
            linked_tags.clear();
        }else{
            for (std::shared_ptr<teTag> tag : second_tags) {
                static QRegularExpression reg(QStringLiteral(R"(^\d+\+?([a-zA-Z]+)$)"));
                taglistwidget->tagInsertAbove(false, std::make_shared<teTag>(QString(QStringLiteral("multiple ") + reg.match(*tag).captured(1))),1);
            }
        }
    }
}
};

    struct Object_list : public teTagListControl{
        std::shared_ptr<teTag>editcore=nullptr;

        Object_list(colorsWidget*in_onEdit_widget,teTagListView*parentlist,QWidget*parent = nullptr,QString*styleSheet=nullptr)
            :teTagListControl(in_onEdit_widget,parentlist,parent,styleSheet,"object"){
            sc->setMinimumHeight(110);
        }
        bool filter(std::shared_ptr<teTag>in_tag)override{
            if(in_tag->words.count()!=3)return false;
            if(object1.contains(in_tag->words[0]->text)&&
                preposwords.contains(in_tag->words[1]->text)&&
                bodyparts.contains(in_tag->words[2]->text))
                return true;
            else return false;
        }
    };
