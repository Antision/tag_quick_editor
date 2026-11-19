/*
 * temisccontrols.h - controls moved out of teeditor_derive.cpp, where they used
 * to be declared inside the editor constructors.
 */
#pragma once
#include "teeditorhelpers.h"
#include "numberpad.h"

    struct teTagLineedit_people : public teTagLineedit {
        QString objtext = "";
        /**
         * @brief The little keyboard that appears under the box while editing.
         *
         * Typing "3" for something with six possible answers is two keystrokes
         * too many; the pad writes the number, commits the edit and closes.
         */
        NumberPad* pad = nullptr;
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
            lineedit->installEventFilter(this);      // opens the number pad
        }
        ~teTagLineedit_people() override {
            delete pad;                              // it is a top-level window
        }
        bool eventFilter(QObject* watched, QEvent* event) override {
            if (watched == lineedit) {
                if (event->type() == QEvent::FocusIn) {
                    if (!pad)
                        pad = new NumberPad(nullptr);
                    pad->openFor(lineedit, [this] { onEditingFinished(); });
                } else if (event->type() == QEvent::FocusOut) {
                    // Not immediately: clicking a pad button may briefly move the
                    // focus, and the click that is already on its way has to
                    // arrive. dismiss() is idempotent.
                    QTimer::singleShot(250, this, [this] {
                        if (pad && !lineedit->hasFocus())
                            pad->dismiss();
                    });
                }
            }
            return teTagLineedit::eventFilter(watched, event);
        }
        QString getText() {
            if (linked_tags.empty()) { return QString{}; }
            else { return (QString)(*(*linked_tags.begin())); }
        }
        /// Pure: the number it matches is put into the line edit by refreshState()
        /// (which runs when the tag is linked), not from inside the predicate.
        bool filter(std::shared_ptr<teTag> tag) const override {
            if (!tag) return false;
            return regex.match(static_cast<QString>(*tag)).hasMatch();
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

    /**
     * @brief "multiple people": one box that stands for `multiple girls` and is
     *        informed by the counted spellings (`2girls`, `5boys`).
     */
    struct multipeople_CheckBox : public teTagCheckBoxPlus {
        multipeople_CheckBox(QString key_string1,QWidget* parent=nullptr,QString* stylesheet=nullptr)
            :teTagCheckBoxPlus({key_string1},parent,stylesheet){}

        /// "multiple girls" / "multiple boy" / "multiple others", spaced or not.
        bool filter(std::shared_ptr<teTag> tag) const override {
            static const QRegularExpression reg(QStringLiteral(R"(^multiple\s?(girl(s)?|boy(s)?|other(s)?)$)"));
            return reg.match(*tag).hasMatch();
        }
        /// "2girls", "5boys", "10+others" - what the count is written as.
        bool filter2(std::shared_ptr<teTag> tag) const override {
            static const QRegularExpression reg(QStringLiteral(R"(^(2|[3-9]|\d{2,}|\d+\+)(girls|boys|others)$)"));
            return reg.match(*tag).hasMatch();
        }

        void onStateChanged(bool state)override {
            if(excute>0){
                if(!state)
                    eraseOwnedTags();           // unticking removes what ticking wrote
                else{
                    // One "multiple <nouns>" per counted tag: "2girls" becomes
                    // "multiple girls". The count itself is not written - the
                    // counted tag stays in the list and still says how many.
                    static const QRegularExpression reg(QStringLiteral(R"(^\d+\+?([a-zA-Z]+)$)"));
                    for(const std::shared_ptr<teTag>& tag : counted_tags)
                        taglistwidget->tagInsertAbove(false,
                            std::make_shared<teTag>(QStringLiteral("multiple ")+reg.match(*tag).captured(1)),1);
                }
            }
            // The group above shows what this box stands for and listens to this
            // signal; without it the label kept saying "(nobody)" until something
            // else happened to refresh it.
            edited();
        }
    };

    struct Object_list : public teTagListControl{
        std::shared_ptr<teTag>editcore=nullptr;

        Object_list(colorsWidget*in_onEdit_widget,TagListView*parentlist,QWidget*parent = nullptr,QString*styleSheet=nullptr)
            :teTagListControl(in_onEdit_widget,parentlist,parent,styleSheet,"object"){
            sc->setMinimumHeight(110);
        }
        bool filter(std::shared_ptr<teTag> in_tag) const override{
            if(in_tag->words.count()!=3)return false;
            if(object1.contains(in_tag->words[0]->text)&&
                preposwords.contains(in_tag->words[1]->text)&&
                bodyparts.contains(in_tag->words[2]->text))
                return true;
            else return false;
        }
    };
