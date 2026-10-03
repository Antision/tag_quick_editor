#include "teeditor_derive.h"
#include "teeditorcontrol.h"
#include "tetaglistwidget.h"
#include "tesignalwidget.h"
QSet<QString> all_colors = { "red","blue","green","orange","yellow",
                    "blonde","purple","white","black","brown","pink","aqua"
                    ,"grey" };
bool is_color(const QString& color_word)
{
    return all_colors.contains(color_word);
}
QSet<QString> shades = { "light","deep","dark"};
bool is_shade(const QString& shade_word){
    return shades.contains(shade_word);
}
QStringList editorNames{qsl("custom"),qsl("pretreat"),qsl("hair and eyes"),qsl("clothes"),qsl("nsfw")};

QPair<int,int> is_color(teTag& tag,int offset=0) {
    int index;
    int size = tag.words.size();
    for (index = offset; index < size; ++index) {
        if (all_colors.contains(*tag.words[index])){
            if (index > 0&&(*tag.words[index - 1] == "light" || *tag.words[index - 1] == "dark"))
                return {index - 1,2};
            else
                return {index,1};
        }
    }
    return {0,0};
}
QStringList extract_colors(std::shared_ptr<teTag> tag) {
    QStringList colors;
    int pos = 0;
    while(pos < tag->words.size()) {
        auto [color_pos, color_cnt] = is_color(*tag, pos);
        if(color_cnt == 0) break;

        QString color;
        for(int i=color_pos; i<color_pos+color_cnt; ++i) {
            color += *tag->words[i] + " ";
        }
        colors << color.trimmed();

        pos = color_pos + color_cnt;
    }
    return colors;
}
bool has_color_conflict(const QStringList& existing, const QStringList& new_colors) {
    foreach(const QString& c, new_colors) {
        if(existing.contains(c)) return true;
    }
    return false;
}


extern bool autoMerge;
extern bool MergeSwitch;
extern QStringList custom_tags;
extern teCustomControlList* custom_controls;
QString teEditor_custom_style = QStringLiteral(R"(
QPushButton#addNewTagButton{
font:12pt "Segoe UI";color:white;
}
QPushButton#addNewTagButton:!hover{
border:1px solid rgb(50,50,50);background:qlineargradient(spread:pad, x1:0, y1:1, x2:0, y2:0, stop:0 rgba(0, 23, 175, 255), stop:1 rgba(37, 135, 255, 255));
}
QPushButton#addNewTagButton:hover{
border:1px solid white;background:qlineargradient(spread:pad, x1:0, y1:1, x2:0, y2:0, stop:0 rgba(0, 60, 255, 255), stop:1 rgba(126, 216, 255, 255));
}

QPushButton{
    outline: 0px solid black;
    border: 1px solid #2c5ea9;
    background-color:transparent;
    padding: 0px;
}
QPushButton:hover{
    border: 1px solid #3071d1;
    background-color:rgba(46,92,162,20);
}
QPushButton:checked{
    border: 1px solid #3183ff;
    background-color:rgba(60,130,235,150);
}
QPushButton:checked:hover{
    border: 1px solid #4c94ff;
    background-color:rgba(60,130,235,255);
}
)");
teEditor_custom::teEditor_custom(teTagListView*in_taglistwidget,QString&& name, QString* styleSheet, QWidget* parent):teEditor(in_taglistwidget,name, parent) {
    if(!styleSheet)
        setStyleSheet(teEditor_custom_style);
    QPushButton* newtagButton = new QPushButton("add custom tags");
    mainLayout->setContentsMargins(1,1,1,1);
    mainLayout->setSpacing(1);
    flowLayout->setVerticalSpacing(3);
    flowLayout->setHorizontalSpacing(5);
    newtagButton->setObjectName("addNewTagButton");
    newtagButton->setFixedHeight(25);
    mainLayout->addWidget(newtagButton);
    mainLayout->addLayout(flowLayout);
    custom_controls=&this->string_controls;
    // The custom tag buttons can be dragged into any order; that order is what
    // gets stored in config.json.
    controlReorderer = new QFlowLayoutReorderer(flowLayout,this,this);
    connect(controlReorderer,&QFlowLayoutReorderer::reordered,this,&teEditor_custom::syncOrderFromLayout);
    connect(newtagButton,&QPushButton::clicked,&controlWidget,&QWidget::show);
    connect(&controlWidget,&customControlWidget::tagsUpdated,this,[this](QStringList strlist){
        setControls(strlist);
    });
    for(const QString &str:custom_tags){
        addControl(str);
        controlWidget.tagWidget->addTag(str);
    }
    connect(&controlWidget,&customControlWidget::tagsClearAll,this,[this](){
        for(teEditorControl*ec:controls)
            delete ec;
        controls.clear();string_controls.clear();
    });
}

teEditorControl* teEditor_custom::findControl(const QString& str) const {
    for(const auto& [text,control]:string_controls)
        if(text==str)
            return control;
    return nullptr;
}

void teEditor_custom::removeControl(const QString &str){
    for(int i=0;i<string_controls.size();++i){
        if(string_controls[i].first!=str)
            continue;
        teEditorControl* control = string_controls[i].second;
        string_controls.removeAt(i);
        controls.removeAll(control);
        if(auto* widget = dynamic_cast<QWidget*>(control)){
            flowLayout->removeWidget(widget);   // deletes the layout item
            widget->hide();
        }
        delete control;
        return;
    }
}

void teEditor_custom::addControl(const QString &str){
    teTagCheckBox* cbptr=new teTagCheckBox({str,str});
    cbptr->setTaglistwidget(taglistwidget);
    string_controls.append({str,cbptr});
    controls.push_back(cbptr);
    if(taglist)
        for(std::shared_ptr<teTag>tc:*taglist)
            cbptr->read(tc);
    flowLayout->addWidget(cbptr);   // addWidget() reparents the button
    controlReorderer->attach(cbptr);
}

void teEditor_custom::syncOrderFromLayout(){
    teCustomControlList reordered;
    reordered.reserve(string_controls.size());
    for(int i=0;i<flowLayout->count();++i){
        QWidget* widget = flowLayout->itemAt(i)->widget();
        for(const auto& entry:string_controls){
            if(dynamic_cast<QWidget*>(entry.second)==widget&&!reordered.contains(entry)){
                reordered.append(entry);
                break;
            }
        }
    }
    // Anything that is not in the layout (should not happen) keeps its place at
    // the end so no control is ever lost.
    for(const auto& entry:string_controls)
        if(!reordered.contains(entry))
            reordered.append(entry);
    string_controls=reordered;
}

void teEditor_custom::setControls(const QStringList& order){
    // Remove the controls the user deleted from the dialog.
    for(int i=string_controls.size()-1;i>=0;--i)
        if(!order.contains(string_controls[i].first))
            removeControl(string_controls[i].first);
    // Add the new ones.
    for(const QString& str:order)
        if(!findControl(str))
            addControl(str);
    // Reorder so the editor matches the dialog.
    int target=0;
    for(const QString& str:order){
        teEditorControl* control=findControl(str);
        if(!control){
            ++target;
            continue;
        }
        int current=-1;
        for(int i=0;i<flowLayout->count();++i)
            if(flowLayout->itemAt(i)->widget()==dynamic_cast<QWidget*>(control)){
                current=i;
                break;
            }
        if(current>=0&&current!=target)
            flowLayout->moveItem(current,target);
        ++target;
    }
    syncOrderFromLayout();
}

QString teEditor_pretreat_style = QStringLiteral(R"(
QPushButton#editor_switch{
font:16pt "Segoe UI";color:qlineargradient(spread:pad, x1:0, y1:0, x2:0, y2:1, stop:0 rgba(0, 0, 0, 255), stop:1 rgba(255, 255, 255, 255)); background:black;
}
QPushButton#editor_switch:!checked{
border:1px solid rgb(50,50,50);
background:transparent;
}
QPushButton#editor_switch:checked{
border:2px solid #008b46;
})");
teEditor_pretreat::teEditor_pretreat(teTagListView*in_taglistwidget,QString&& name, QString* styleSheet, QWidget* parent) :teEditor_standard(in_taglistwidget,name, &teEditor_pretreat_style, parent) {
    QPushButton* girls_btn = new QPushButton;
    contentLayout->addWidget(girls_btn);
    QWidget* people_widget = new QWidget;
    QVBoxLayout* people_widget_layout = new QVBoxLayout(people_widget);
    struct teTagLineedit_people :teTagLineedit {
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
    struct page1_button_link:public teTagControlGroup{
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
    {
        teTagLineedit_people* girls_lnedt = new teTagLineedit_people(QStringLiteral("girl"), QStringLiteral("girl(s)"));
        controls.push_back(girls_lnedt);
        teTagLineedit_people* boys_lnedt = new teTagLineedit_people(QStringLiteral("boy"), QStringLiteral("boy(s)"));
        controls.push_back(boys_lnedt);
        teTagLineedit_people* other_lnedt = new teTagLineedit_people(QStringLiteral("other"), QStringLiteral("other(s)"));
        controls.push_back(other_lnedt);
        people_widget_layout->addWidget(girls_lnedt);
        people_widget_layout->addWidget(boys_lnedt);
        people_widget_layout->addWidget(other_lnedt);
        QHBoxLayout*line1 = new QHBoxLayout;
        teTagCheckBox*furry_cbb = new teTagCheckBox({qsl("furry")});
        teTagCheckBox*magical_girl_cb =new teTagCheckBox({"magical girl"});
        teTagCheckBox*elf_cb =new teTagCheckBox({"elf"});
        teTagCheckBox*mesugaki_cb =new teTagCheckBox({"mesugaki"});
        {
            line1->addWidget(furry_cbb);
            line1->addWidget(magical_girl_cb);
            line1->addWidget(elf_cb);
            line1->addWidget(mesugaki_cb);
            controls.push_back(furry_cbb);
            controls.push_back(magical_girl_cb);
            controls.push_back(elf_cb);
            controls.push_back(mesugaki_cb);
            people_widget_layout->addLayout(line1);
        }
        mainLayout->addWidget(people_widget);
        people_widget_layout->setSpacing(1);
        people_widget_layout->setContentsMargins(0,0,0,0);
        people_widget->hide();
        page1_button_link*page1link = new page1_button_link(girls_btn,{girls_lnedt,boys_lnedt,other_lnedt,furry_cbb,magical_girl_cb,elf_cb,mesugaki_cb},this);
        extraInterfaces.push_back(people_widget);
        connect(girls_btn,&QPushButton::clicked,this,[this]{showInterface(1);},Qt::DirectConnection);
    }
    QHBoxLayout* secondline_layout = new QHBoxLayout;
    secondline_layout->setSpacing(1);
    teTagCheckBox* loli_cb = new teTagCheckBox({ "loli" });
    teTagCheckBox* shota_cb = new teTagCheckBox({ "shota" });
    teTagCheckBox* chibi_cb = new teTagCheckBox({ "chibi" });
    teTagCheckBox* solo_cb = new teTagCheckBox({ "solo" });
    {
        secondline_layout->addWidget(loli_cb);
        secondline_layout->addWidget(shota_cb);
        secondline_layout->addWidget(chibi_cb);
        secondline_layout->addWidget(solo_cb);
        controls.push_back(loli_cb);
        controls.push_back(shota_cb);
        controls.push_back(chibi_cb);
        controls.push_back(solo_cb);
        contentLayout->addLayout(secondline_layout);
    }
struct Mature_Buttongroup :teTagButtonGroup {
    Mature_Buttongroup(QWidget* parent = nullptr, QString* styleSheet_button = nullptr, QString* styleSheet_label = nullptr)
        :teTagButtonGroup({
              {qsl("mature"),qsl("mature"),false,true,false},
              {qsl("male"),qsl("male"),true,true,false},
              {qsl("female"),qsl("female"),true,true,false},
        })
        {layout->setSpacing(1);}
void onClicked(int id)override{
    if(!taglistwidget->showing_list)return;
    ifrefreshState=false;
    enum{male,female};
    static QString Str[2]={qsl("male"),qsl("female")};
    QString FinalStr=qsl("mature ")+Str[id];

    if(buttons[id]->isChecked()){
        std::shared_ptr<teTag>newtagcore =std::make_shared<teTag>(FinalStr);
        link(newtagcore);
        taglistwidget->tagInsertAbove(false, newtagcore);
    }else{
        for(std::shared_ptr<teTag>tc:linked_tags)
            if((QString)*tc==FinalStr){
                taglistwidget->tagErase(tc);
                break;
            }
    }
    ifrefreshState=true;
    refreshState();
    edited();
}
}*mature_bgop = new Mature_Buttongroup;
contentLayout->addWidget(mature_bgop);
controls.push_back(mature_bgop);

    struct multipeople_CheckBox :teTagCheckBoxPlus {
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
}*mcb = new multipeople_CheckBox(QStringLiteral("multiple (people)s"));
contentLayout->addWidget(mcb);
controls.push_back(mcb);

teTagComboBox* quality_cbb = new teTagComboBox({
    {qsl("(none)"),{}},
    {qsl("masterpiece,best quality"),{qsl("masterpiece"),qsl("best quality")}},
    {qsl("mst,bst,absurdres,very aesthetic"),{qsl("masterpiece"),qsl("best quality"),qsl("absurdres"),qsl("very aesthetic")}},
    {qsl("score_9,score_8_up"),{qsl("score_9"),qsl("score_8_up")}},
    {qsl("low quality,worst quality"),{qsl("low quality"),qsl("worst quality")}}
                                               },QStringLiteral("(quality)"));
contentLayout->addWidget(quality_cbb);
controls.push_back(quality_cbb);
quality_cbb->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);

teTagComboBox* camara_cbb = new teTagComboBox({
    {qsl("(none)"),{}},
    {qsl("full body"),{qsl("full body")}},
    {qsl("upper body"),{qsl("upper body")}},
    {qsl("cowboy shot"),{qsl("cowboy shot")}},
    {qsl("pov"),{qsl("pov")}},
    {qsl("close-up"),{qsl("close-up")}},
    {qsl("face focus"),{qsl("face focus")}},
    {qsl("eyes focus"),{qsl("eyes focus")}},
    {qsl("foot focus"),{qsl("foot focus")}},
    {qsl("ass focus"),{qsl("ass focus")}},
},QStringLiteral("(framing)"));
contentLayout->addWidget(camara_cbb);
controls.push_back(camara_cbb);

teTagComboBox* bodydirection_cbb = new teTagComboBox({
    {qsl("(none)"),{}},
    {qsl("looking at viewer"),{qsl("looking at viewer")}},
    {qsl("facing viewer"),{qsl("facing viewer")}},
    {qsl("lookingat&facing viewer"),{qsl("looking at viewer"),qsl("facing viewer")}},
    {qsl("looking back"),{qsl("looking back")}},
    {qsl("looking to the side"),{qsl("looking to the side")}},
    {qsl("facing to the side"),{qsl("facing to the side")}},
    {qsl("looking away"),{qsl("looking away")}},
    {qsl("facing away"),{qsl("facing away")}},
    {qsl("looking&facing away"),{qsl("looking away"),qsl("facing away")}},
    {qsl("looking up"),{qsl("looking up")}},
    {qsl("looking down"),{qsl("looking down")}},
    {qsl("looking at object"),{qsl("looking at object")}}
},QStringLiteral("(facing)"));
contentLayout->addWidget(bodydirection_cbb);
controls.push_back(bodydirection_cbb);
bodydirection_cbb->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);

teTagComboBox* posture_cbb = new teTagComboBox(
    {
        {qsl("(none)"),{}},
        {qsl("sitting"),{qsl("sitting")}},
        {qsl("wariza"),{qsl("wariza")}},
        {qsl("squatting"),{qsl("squatting")}},
        {qsl("on back"),{qsl("on back")}},
        {qsl("on side"),{qsl("on side")}},
        {qsl("on stomach"),{qsl("on stomach")}},
        {qsl("kneeling"),{qsl("kneeling")}},
     {qsl("knees up"),{qsl("knees up")}},
     {qsl("knee up"),{qsl("knee up")}},
        {qsl("all fours"),{qsl("all fours")}},
        {qsl("pose"),{qsl("pose")}},
     {qsl("fetal position"),{qsl("fetal position")}},
        {qsl("floating"),{qsl("floating")}},
        {qsl("standing"),{qsl("standing")}},
     {qsl("top-down bottom-up"),{qsl("top-down bottom-up")}}

    },QStringLiteral("(posture)"));
contentLayout->addWidget(posture_cbb);
controls.push_back(posture_cbb);

teTagComboBox* angle_cbb = new teTagComboBox({
    {qsl("(none)"),{}},
    {qsl("from above"),{qsl("from above")}},
    {qsl("from side"),{qsl("from side")}},
    {qsl("front view"),{qsl("front view")}},
    {qsl("from below"),{qsl("from below")}},
    {qsl("from behind"),{qsl("from behind")}},
    {qsl("mtu virus"),{qsl("mtu virus")}},
    {qsl("pantyshot"),{qsl("pantyshot")}}
},QStringLiteral("(angle)"));
contentLayout->addWidget(angle_cbb);
controls.push_back(angle_cbb);

teTagCheckBox* multipleviews_cb = new teTagCheckBox({ "multiple views" });
contentLayout->addWidget(multipleviews_cb);
controls.push_back(multipleviews_cb);

teTagComboBox* light_cbb = new teTagComboBox(
    {{qsl("(none)"),{}},
     {qsl("front lighting"),{qsl("front lighting")}},
     {qsl("side lighting"),{qsl("side lighting")}},
     {qsl("backlighting"),{qsl("backlighting")}},
     {qsl("rim light"),{qsl("rim light")}},
     {qsl("dim lighting"),{qsl("dim lighting")}},
     {qsl("sunbeam"),{qsl("sunbeam")}}
    },QStringLiteral("(lighting)"));
contentLayout->addWidget(light_cbb);
controls.push_back(light_cbb);

teTagComboBox* scene_cbb = new teTagComboBox(
    {{qsl("(none)"),{}},
        {qsl("scenery"),{qsl("scenery")}},
        {qsl("landscape"),{qsl("landscape")}},
        {qsl("cityscape"),{qsl("cityscape")}},
        {qsl("indoors"),{qsl("indoors")}},
        {qsl("outdoors"),{qsl("outdoors")}},
        {qsl("on bed"),{qsl("on bed"),qsl("indoors")}}
    },QStringLiteral("(scene)"));
contentLayout->addWidget(scene_cbb);
controls.push_back(scene_cbb);

teTagComboBox* type_cbb = new teTagComboBox({
    {qsl("(none)"),{}},
    {qsl("comic"),{qsl("comic")}},
    {qsl("comic,greyscale,monochrome"),{qsl("comic"),qsl("greyscale"),qsl("monochrome")}},
    {qsl("game cg"),{qsl("game cg")}},
    {qsl("doujin cover"),{qsl("doujin cover"),qsl("cover page")}},
    {qsl("tachi-e"),{qsl("tachi-e")}},
    {qsl("pixel art"),{qsl("pixel art")}},
    {qsl("lineart"),{qsl("lineart")}},
    {qsl("dakimakura"),{qsl("dakimakura ( medium )")}},

},QStringLiteral("(format)"));
contentLayout->addWidget(type_cbb);
controls.push_back(type_cbb);

QHBoxLayout* tenthline_layout = new QHBoxLayout;
tenthline_layout->setSpacing(1);
teTagCheckBox* blush_cb = new teTagCheckBox({ "blush" });
teTagCheckBox* tears_cb = new teTagCheckBox({ "tears" });
{
    tenthline_layout->addWidget(blush_cb);
    tenthline_layout->addWidget(tears_cb);
    controls.push_back(blush_cb);
    controls.push_back(tears_cb);
    contentLayout->addLayout(tenthline_layout);
}

struct smile_buttongroup :teTagButtonGroup {
    smile_buttongroup(QWidget* parent = nullptr, QString* styleSheet_button = nullptr, QString* styleSheet_label = nullptr)
        :teTagButtonGroup({ {qsl("light"),qsl("light"),true},{qsl("naughty"),qsl("naughty"),true},{qsl("seductive"),qsl("seductive"),true},{qsl("smile"),qsl("smile"),true} }) {layout->setSpacing(1);}
    virtual void reform(int id)override {
        if (!buttons[3]->isChecked()&&id==3) {
            buttons[0]->setChecked(false);
            buttons[1]->setChecked(false);
            buttons[2]->setChecked(false);
        }
        else if(!buttons[3]->isChecked()){
            buttons[3]->setChecked(true);
        }
    }
}*smile_bgop = new smile_buttongroup;
contentLayout->addWidget(smile_bgop);
controls.push_back(smile_bgop);

struct ClosedEyes_Buttongroup :teTagButtonGroup {
    ClosedEyes_Buttongroup(QWidget* parent = nullptr, QString* styleSheet_button = nullptr, QString* styleSheet_label = nullptr)
        :teTagButtonGroup({
            {qsl("half-"),qsl("half-"),true,false,false},
            {qsl("closed"),qsl("closed"),true,true,false},
            {qsl("eyes"),qsl("eyes"),false,true,false},
        })
        {layout->setSpacing(1);}
        enum {
            half,closed,eyes
        };
        virtual void reform(int id)override {
            if (id==half&&buttons[id]->isChecked()) {
                buttons[closed]->setChecked(true);
            }else if(id==closed&&!buttons[closed]->isChecked()) {
                buttons[half]->setChecked(false);
            }
        }
        void refreshState()override{
            bool halfb=false,closedb=false;
            for(std::shared_ptr<teTag>tc:linked_tags)
                if(tc->words[0]->text==qsl("half-closed")){
                    halfb=true;
                    closedb=true;
                }
                else if(tc->words[0]->text==qsl("closed"))
                    closedb=true;
            buttons[half]->setChecked(halfb);
            buttons[closed]->setChecked(closedb);
        }
}*closedeyes_bgop = new ClosedEyes_Buttongroup;
contentLayout->addWidget(closedeyes_bgop);
controls.push_back(closedeyes_bgop);

teTagComboBox* expression_cbb = new teTagComboBox(
    {
     {qsl("(none)"),{}},
     {qsl("surprised"),{qsl("surprised")}},
     {qsl("scared"),{qsl("scared")}},
     {qsl("smug"),{qsl("smug")}},
     {qsl("confused"),{qsl("confused")}},
     {qsl("annoyed"),{qsl("annoyed")}},
     {qsl("sad"),{qsl("sad")}},
     {qsl("flustered"),{qsl("flustered")}},
     {qsl("crying"),{qsl("crying")}},
     {qsl("angry"),{qsl("angry")}},
     {qsl("exhausted"),{qsl("exhausted")}},
     {qsl("embarrassed"),{qsl("embarrassed")}},
     {qsl("grin"),{qsl("grin")}},
     {qsl("expressionless"),{qsl("expressionless")}},
     },QStringLiteral("(expression)"));
contentLayout->addWidget(expression_cbb);
controls.push_back(expression_cbb);
teTagComboBox* eye_cbb = new teTagComboBox(
    {
     {qsl("empty eyes"),{qsl("empty eyes")}},
     {qsl("constricted pupils"),{qsl("constricted pupils")}},
     {qsl("wide-eyed"),{qsl("wide-eyed")}},
     {qsl("sparkling"),{qsl("sparkling")}},
     {qsl("one eye closed"),{qsl("one eye closed")}},
     {qsl("dashed eyes"),{qsl("dashed eyes")}},
     {qsl("squiggle eyes"),{qsl("squiggle eyes")}}
     },QStringLiteral("(eye)"));
contentLayout->addWidget(eye_cbb);
controls.push_back(eye_cbb);
teTagComboBox* eye_shape_cbb = new teTagComboBox(
    {
        {qsl("jitome"),{qsl("jitome")}},
        {qsl("tareme"),{qsl("tareme")}},
        {qsl("tsurime"),{qsl("tsurime")}},
        {qsl("sanpaku"),{qsl("sanpaku")}},
    },QStringLiteral("(eye shape)"));
contentLayout->addWidget(eye_shape_cbb);
controls.push_back(eye_shape_cbb);

struct mouth_buttongroup :teTagButtonGroup {
    mouth_buttongroup(QWidget* parent = nullptr, QString* styleSheet_button = nullptr, QString* styleSheet_label = nullptr)
    :teTagButtonGroup({ {qsl("open"),qsl("open"),true},{qsl("closed"),qsl("closed"),true},{qsl("mouth"),qsl("mouth"),false} })
    {layout->setSpacing(1);}
    void reform(int id)override{
        if(buttons[id]->isChecked())
            buttons[!id]->setChecked(false);
    }
}*mouth_bgop = new mouth_buttongroup;
contentLayout->addWidget(mouth_bgop);
controls.push_back(mouth_bgop);

struct Breast_Buttongroup :teTagButtonGroup {
    Breast_Buttongroup(QWidget* parent = nullptr, QString* styleSheet_button = nullptr, QString* styleSheet_label = nullptr)
        :teTagButtonGroup({
              {qsl("small"),qsl("small"),true,true,false},
              {qsl("medium"),qsl("medium"),true,true,false},
              {qsl("large"),qsl("large"),true,true,false},
              {qsl("breasts"),qsl("breasts"),false,true,false},
        })
        {layout->setSpacing(1);breasts_it=defaultFiltStrings.find("breasts");}
    virtual void reform(int id)override {
        enum {
            tesmall,temedium,telarge,tebreasts
        };
        if (id<tebreasts) {
            if(buttons[id]->isChecked()){
                buttons[(id+1)%3]->setChecked(false);
                buttons[(id+2)%3]->setChecked(false);
            }
        }
    }

    std::shared_ptr<teTag>breasts_ptr=nullptr;
    std::unordered_set<QString>::iterator breasts_it;
    std::unordered_set<QString>::iterator filter_it;
    bool filter(std::shared_ptr<teTag> tag) override
    {
        filter_it = defaultFiltStrings.find(*tag);
        if (filter_it == defaultFiltStrings.end())
            return false;

        if (!(autoMerge && MergeSwitch))
            return true;

        const bool isBreastsToken = (filter_it == breasts_it);
        const bool isCurrentBreastsTag = (breasts_ptr && breasts_ptr == tag);

        if (isBreastsToken) {
            // 当前这个 tag 就是 breasts / 由 breasts 维护的对象
            // 不要把“正在编辑中的同一个对象”再删掉
            if (!linked_tags.empty()) {
                auto linked = *linked_tags.begin();
                if (linked && !linked->words.empty()
                    && linked->words.begin() != linked->words.end()
                    && (*linked->words.begin())->text != qsl("breasts")) {
                    tag->retired=true;
                    breasts_ptr.reset();
                    return false;
                }
            }

            breasts_ptr = tag;
            return true;
        }

        // 不是 breasts，但如果它正好就是当前控制器正在编辑的那个对象，
        // 不能 erase 自己，否则会在回调链中把对象拆掉。
        if (breasts_ptr && !isCurrentBreastsTag) {
            taglistwidget->tagErase(breasts_ptr);
            breasts_ptr.reset();
        }

        return true;
    }
    void unlink(std::shared_ptr<teTag>tag)override{
        if(tag==breasts_ptr)breasts_ptr.reset();
        teTagButtonGroup::unlink(tag);
    }
    void clear()override{
        breasts_ptr.reset();
        teTagButtonGroup::clear();
    }
    void getDefaultFiltStrings()override{};
}*hairlength_bgop = new Breast_Buttongroup;
contentLayout->addWidget(hairlength_bgop);
controls.push_back(hairlength_bgop);
}

QString teEditor_hair_and_eyes_style = QStringLiteral(R"(
QPushButton#editor_switch{
font:16pt "Segoe UI";color:qlineargradient(spread:pad, x1:0, y1:0, x2:0, y2:1, stop:0 rgba(255, 146, 235, 255), stop:0.497207 rgba(235, 239, 195, 255), stop:1 rgba(176, 246, 244, 255)); background:black;
}
QPushButton#editor_switch:!checked{
border:1px solid rgb(50,50,50);
background:transparent;
}
QPushButton#editor_switch:checked{
border:2px solid #008b46;
})");
teEditor_hair_and_eyes::teEditor_hair_and_eyes(teTagListView*in_taglistwidget,QString &&name, QString *styleSheet, QWidget *parent):teEditor_standard(in_taglistwidget,name, &teEditor_hair_and_eyes_style, parent){
    struct hair_and_eyes_color_list: teTagListControl{
        hair_and_eyes_color_list(colorsWidget*in_onEdit_widget,teTagListView*parentlist,QWidget*parent = nullptr,QString*styleSheet=nullptr)
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

        bool filter(std::shared_ptr<teTag>in_tag)override{
            if(hairtag_types.contains(in_tag))
                on_hairtag_edited(in_tag);
            if(in_tag->retired)
                return false;
            if(in_tag->words.empty())return false;
            QString&&lastword=*in_tag->words.back();
            QString tagstring = *in_tag;
            if((lastword==qsl("hair")||lastword==qsl("eyes"))
                 &&
                 ((is_color(*in_tag).second>0)||tagstring==qsl("streaked hair")||tagstring==qsl("gradient hair"))){
                return true;
            }else if(tagstring.startsWith(qsl("gradient from"))||tagstring.endsWith(qsl("streaks")))
                return true;
            return false;
        }
        enum Type {
            Unknown=-1,
            BaseHair,
            StreakHair,
            GradientHair,
            rawStreakHair,
            rawGradientHair
        };
        QHash<std::shared_ptr<teTag>, Type> hairtag_types;
        Type classify_tag(std::shared_ptr<teTag> tag) {
            if(is_color(*tag).second==0||
                (!(tag->contains("hair")||tag->contains("gradient")||tag->words.size()<3)))
                return Unknown;
            if(tag->contains("with"))
                return StreakHair;
            else if(tag->contains("from"))
                return GradientHair;
            else if(tag->contains("streaked") || tag->contains("gradient"))
                return tag->contains("gradient") ?
                           rawGradientHair :
                           rawStreakHair;

            return BaseHair;
        }
        bool can_merge(std::shared_ptr<teTag> target,Type target_type, std::shared_ptr<teTag> source,Type source_type) {
            static bool matrix[5][5]{
                //basic   s    g    rs    rg
                {false,false,false,true,false},
                {false,false,false,true,false},
                {false,false,false,false,true},
                {true,true,false,false,false},
                {false,false,true,false,true}
            };
            return matrix[target_type][source_type];
        }
        std::shared_ptr<teTag> find_merge_target(std::shared_ptr<teTag> new_tag,Type type) {
            for(int i = layout->count()-2; i >=0; --i) {
                teRefTagWidget* widget = dynamic_cast<teRefTagWidget*>(layout->itemAt(i)->widget());
                std::shared_ptr<teTag> candidate;
                if(widget&&widget->core)
                    candidate = widget->core;
                else
                    continue;
                if(!hairtag_types.contains(candidate)) continue;
                if(candidate==new_tag)continue;
                if(candidate->retired)continue;

                if(can_merge(candidate,hairtag_types[candidate], new_tag,type)){
                    return candidate;
                }
            }
            return nullptr;
        }
        void perform_merge(std::shared_ptr<teTag> target, std::shared_ptr<teTag> source,Type source_type) {
            if(source_type == rawStreakHair||hairtag_types[target]==rawStreakHair) {
                merge_streaks(target, source,source_type==rawStreakHair);
                hairtag_types[target]=StreakHair;
            } else if(source_type == rawGradientHair||hairtag_types[target]==rawGradientHair) {
                merge_gradient(target, source,source_type==rawGradientHair);
                hairtag_types[target]=GradientHair;
            }else
                telog("[perform_merge]:couldn't merge tag");
        }
        void merge_streaks(std::shared_ptr<teTag> target, std::shared_ptr<teTag> source,bool source_to_target) {
            QStringList all_colors;
            if(source_to_target)
                all_colors = extract_colors(target)+extract_colors(source);
            else
                all_colors = extract_colors(source)+extract_colors(target);
            QString finalStr;

            finalStr+=all_colors[0]+" hair with ";
            int i =1;
            static QString space=" and ";
            for(;i<all_colors.size();++i){
                finalStr+=all_colors[i]+space;
            }
            finalStr.chop(4);
            finalStr+=" streaks";
            taglistwidget->tagEdit(target,finalStr);
        }
        void merge_gradient(std::shared_ptr<teTag> target, std::shared_ptr<teTag> source,bool source_to_target) {
            QStringList all_colors;
            if(source_to_target)
                all_colors = extract_colors(target)+extract_colors(source);
            else
                all_colors = extract_colors(source)+extract_colors(target);
            QString new_text = "gradient from " + all_colors.join(" to ");
            taglistwidget->tagEdit(target,new_text);
        }
        void link_hair(std::shared_ptr<teTag> tag){
            tag->teConnect(teCallbackType::edit_with_layout,this,&hair_and_eyes_color_list::on_hairtag_edited,tag);
            hairtag_types.insert(tag,classify_tag(tag));
        }
        void unlink_hair(std::shared_ptr<teTag> tag){
            if(hairtag_types.contains(tag)){
                tag->teDisconnect(this,teCallbackType::edit_with_layout);
                hairtag_types.remove(tag);
            }
        }
        void on_hairtag_edited(std::shared_ptr<teTag> edited_tag) {
            Type still_valid = classify_tag(edited_tag);
            if(still_valid==Unknown) {
                unlink_hair(edited_tag);
                return;
            }
            if(hairtag_types.contains(edited_tag))
                hairtag_types[edited_tag]=still_valid;
            std::shared_ptr<teTag> new_target = find_merge_target(edited_tag,still_valid);
            if(new_target) {
                edited_tag->retired=true;
                perform_merge(new_target, edited_tag,still_valid);
            }
        }
        virtual void tagEdit(teTagWidgetBase*tag,teWordWidgetBase*word)override{
            if(std::shared_ptr<teTag>core =tag->core; hairtag_types.contains(core)&&(hairtag_types[core]==StreakHair||hairtag_types[core]==GradientHair)){
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
                connect(lineedit,&suggestionLineEdit::editingFinished,this,&teRefTagListWidget::onLineEditStop,Qt::DirectConnection);
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
            if(autoMerge&&MergeSwitch&&(!in_tag->contains("eyes"))&&
                (is_color(*in_tag).second>0)
                )
            {
                const Type new_type = classify_tag(in_tag);
                if(new_type==Unknown){
                    telog("[hair_and_eyes]:unable to distinguish hair tag's type");
                    goto mergeEnd;
                }
                std::shared_ptr<teTag> target_tag = find_merge_target(in_tag,new_type);

                if(target_tag) {
                    in_tag->retired=true;
                    perform_merge(target_tag,in_tag,new_type);
                    return;
                } else {
                    link_hair(in_tag);
                }
            }
            mergeEnd:
            linked_tags.insert(in_tag);
            in_tag->teConnect(teCallbackType::destroy,this,&teEditorControl::unlink,in_tag);
            teTagWidgetBase*widget =taginsert(-1,in_tag,taglistwidget->showing_list);
            extraWidgetPushBack(in_tag,widget);
        }

        void unlink(std::shared_ptr<teTag>in_tag)override{
            teDisconnect(in_tag.get());
            hairtag_types.remove(in_tag);
            linked_tags.erase(in_tag);
            tagErase(in_tag);
        }
        void extraWidgetPushBack(std::shared_ptr<teTag>in_tag,teTagWidgetBase*widget){
            if(hairtag_types.contains(in_tag)&&((hairtag_types[in_tag]==BaseHair)||(hairtag_types[in_tag]==rawStreakHair)||(hairtag_types[in_tag]==rawGradientHair))){
                int wordcount = in_tag->words.size();
                QPushButton*streaked_btn = new QPushButton("s",widget);
                streaked_btn->setStyleSheet(qsl(R"(font:8pt "Sonsolas")"));
                streaked_btn->setCheckable(true);
                streaked_btn->setFixedSize(15,15);
                for(int i =0;i<wordcount;++i)
                    if(*in_tag->words[i]==qsl("streaked"))
                        streaked_btn->setChecked(true);
                connect(streaked_btn,&QPushButton::clicked,widget,[in_tag, streaked_btn, this](bool ifchecked){
                    if(ifchecked){
                        if(in_tag->contains("streaked")){
                            streaked_btn->setChecked(false);
                            return;
                        }
                        if(auto* editorTag=tagWidgetFor(in_tag)) editorTag->insertWord(-2,qsl("streaked"));
                    }else{
                        int wordcount = in_tag->words.size();
                        for(int i =0;i<wordcount;++i)
                            if(*in_tag->words[i]==qsl("streaked"))
                            {if(auto* editorTag=tagWidgetFor(in_tag)) editorTag->destroyWord(i);--i;--wordcount;}
                    }
                    if(in_tag->retired)
                        taglistwidget->tagErase(in_tag);
                },Qt::DirectConnection);
                widget->insertExtraWidgets(this,streaked_btn);
                QPushButton*gradient_btn = new QPushButton("g",widget);
                gradient_btn->setStyleSheet(qsl(R"(font:8pt "Sonsolas")"));
                gradient_btn->setCheckable(true);
                gradient_btn->setFixedSize(15,15);
                for(int i =0;i<wordcount;++i)
                    if(*in_tag->words[i]==qsl("gradient"))
                        gradient_btn->setChecked(true);
                connect(gradient_btn,&QPushButton::clicked,widget,[in_tag, this, gradient_btn](bool ifchecked){
                    if(ifchecked){
                        if(in_tag->contains("gradient")){
                            gradient_btn->setChecked(false);
                            return;
                        }
                        if(auto* editorTag=tagWidgetFor(in_tag)) editorTag->insertWord(-2,qsl("gradient"));
                    }else{
                        int wordcount = in_tag->words.size();
                        for(int i =0;i<wordcount;++i)
                            if(*in_tag->words[i]==qsl("gradient"))
                            {if(auto* editorTag=tagWidgetFor(in_tag)) editorTag->destroyWord(i);--i;--wordcount;}
                    }
                    if(in_tag->retired)
                        taglistwidget->tagErase(in_tag);
                },Qt::DirectConnection);
                widget->insertExtraWidgets(this,gradient_btn);
            }
        }
        void refreshState()override{}
    }*colors_list = new hair_and_eyes_color_list(new colorsWidget({{{qsl("gradient"),qsl("streaked")},true},{{qsl("hair"),qsl("eyes")},true}},nullptr),taglistwidget);
    contentLayout->addWidget(colors_list);
    controls.push_back(colors_list);

    teTagCheckBox* heterochromia_cb = new teTagCheckBox({ "heterochromia" });
    contentLayout->addWidget(heterochromia_cb);
    controls.push_back(heterochromia_cb);

    QHBoxLayout* multicolored_layout = new QHBoxLayout;
    QLabel*multicolored_label = new QLabel("multicolored");
    multicolored_layout->addWidget(multicolored_label);
    teTagCheckBox* multicolored_eyes_cb = new teTagCheckBox({ "eyes","multicolored eyes" });
    multicolored_layout->addWidget(multicolored_eyes_cb);
    controls.push_back(multicolored_eyes_cb);

    teTagCheckBox* multicolored_hair_cb = new teTagCheckBox({ "hair","multicolored hair" });
    multicolored_layout->addWidget(multicolored_hair_cb);
    controls.push_back(multicolored_hair_cb);

    contentLayout->addLayout(multicolored_layout);

struct ponytail_buttongroup :teTagButtonGroup {
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
        if (id==twin&&buttons[twin]->isChecked()) {
            buttons[pony]->setChecked(false);
            buttons[side]->setChecked(false);
        }else if(id==pony&&buttons[pony]->isChecked()){
            buttons[twin]->setChecked(false);
        }else if(id>side&&!(buttons[pony]->isChecked()||buttons[twin]->isChecked())){
            for(int i =0;i<pony;++i)
                buttons[i]->setChecked(false);
        }
        else if(id<pony&&buttons[id]->isChecked()&&!(buttons[pony]->isChecked()||buttons[twin]->isChecked())){
            buttons[pony]->setChecked(true);
        }
        if(buttons[twin]->isChecked()) allwidgets[tail].data=qsl("tails");
        else allwidgets[tail].data=qsl("tail");

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
                for(teWord* wc : in_tag->words) if(wc && !wc->widget) wc->load();
                for(int i = 0;i<in_tag->words.size();++i)
                    if(in_tag->words[i]->widget)
                        editorTag->layout->insertWidget(editorTag->layout->count()-2,
                                                        in_tag->words[i]->widget);
            }
        }
        in_tag->edited_with_layout();
    }
    bool merge(std::shared_ptr<teTag>tag){
        if(*tag->words.back()==QString("twintails")){
            if(twintailsTag&&twintailsTag!=tag){
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
    void clear()override{
        clearTailMemeory(pony);
        clearTailMemeory(twin);
        ponytailTag=nullptr;
        twintailsTag=nullptr;
        teTagButtonGroup::clear();
    }
    bool filter(std::shared_ptr<teTag>tag)override{
        QString tagstring=*tag;
        int index = tagstring.lastIndexOf("tail");
        if(index<0)return false;
        else if(index==tagstring.size()-5)
            tagstring.chop(1);
        if(defaultFiltStrings.find(tagstring)!=defaultFiltStrings.end()){

            return !(autoMerge&&MergeSwitch&&merge(tag));
        }else return false;
    }
    void getDefaultFiltStrings()override{}
    void refreshState()override{
        buttons[twin]->setChecked((bool)twintailsTag);
        buttons[pony]->setChecked((bool)ponytailTag);
        for(int i=0;i<4;++i){
            buttons[i]->setChecked((bool)kindOfPonytail[i]||kindOfTwintails[i]);
        }
    }
}*ponytail_bgop = new ponytail_buttongroup;
contentLayout->addWidget(ponytail_bgop);
controls.push_back(ponytail_bgop);

teTagCheckBox* two_side_up_cb = new teTagCheckBox({ "two side up" });
contentLayout->addWidget(two_side_up_cb);
controls.push_back(two_side_up_cb);

struct BangsList: teTagListControl{
    std::shared_ptr<teTag>editcore=nullptr;
    BangsList(colorsWidget*in_onEdit_widget,teTagListView*parentlist,QWidget*parent = nullptr,QString*styleSheet=nullptr)
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
    bool filter(std::shared_ptr<teTag>in_tag)override{
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
    void refreshState()override{}
}*bangs_list = new BangsList(
    new colorsWidget({
                      {{qsl("long"),qsl("short")},true},
                      {{qsl("asymmetrical"),qsl("double-parted"),qsl("crossed"),qsl("blunt"),qsl("parted"),qsl("choppy"),qsl("swept"),qsl("braided"),qsl("diagonal"),qsl("center-flap"),qsl("wispy"),qsl("arched"),qsl("dyed"),qsl("curtained"),qsl("fanged"),qsl("flipped"),qsl("sideless"),qsl("loosely tucked")},false},
                      {{qsl("bangs")},true}
                     },
                     nullptr,-1,
                     {{
                       qsl("hair between eyes"),
                       qsl("hair over one eye"),
                       qsl("hair over eyes")
                     }}
                     ),taglistwidget);
contentLayout->addWidget(bangs_list);
controls.push_back(bangs_list);

QHBoxLayout* seventhline_layout = new QHBoxLayout;
seventhline_layout->setSpacing(1);
teTagCheckBox* forehead = new teTagCheckBox({ "forehead" });
teTagCheckBox* ahoge_cb = new teTagCheckBox({ "ahoge" });
teTagCheckBox* hairband_cb = new teTagCheckBox({ "hairband" });
teTagCheckBox* hood_cb = new teTagCheckBox({ "hood" });
{
    seventhline_layout->addWidget(forehead);
    seventhline_layout->addWidget(ahoge_cb);
    seventhline_layout->addWidget(hairband_cb);
    seventhline_layout->addWidget(hood_cb);
    controls.push_back(forehead);
    controls.push_back(ahoge_cb);
    controls.push_back(hairband_cb);
    controls.push_back(hood_cb);
    contentLayout->addLayout(seventhline_layout);
}

struct HairLength_Buttongroup :teTagButtonGroup {
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
            long_hair_iterator=defaultFiltStrings.find(qsl("long hair"));
        very_long_hair_iterator = defaultFiltStrings.find(qsl("very long hair"));
        }
    virtual void reform(int id)override {
        enum {
            very,telong,teshort,medium,hair
        };
        if (id<hair&&id>very) {
            if(buttons[id]->isChecked()){
                buttons[(id-1+1)%3+1]->setChecked(false);
                buttons[(id-1+2)%3+1]->setChecked(false);
            }else {
                if(!buttons[telong]->isChecked()&&!buttons[teshort]->isChecked())
                    buttons[very]->setChecked(false);
            }
        }
        if(id==medium&&buttons[medium]->isChecked()){
            buttons[very]->setChecked(false);
        }else if(id==very&&buttons[very]->isChecked()&&!buttons[teshort]->isChecked()){
            buttons[medium]->setChecked(false);
            buttons[telong]->setChecked(true);
        }
    }

    std::shared_ptr<teTag>long_hair_ptr=nullptr;
    std::shared_ptr<teTag>very_long_hair_ptr=nullptr;
    std::unordered_set<QString>::iterator long_hair_iterator;
    std::unordered_set<QString>::iterator very_long_hair_iterator;

    std::unordered_set<QString>::iterator filter_it;
    bool filter(std::shared_ptr<teTag>tag)override{
        if(filter_it = defaultFiltStrings.find(*tag);filter_it!=defaultFiltStrings.end()){
            if(autoMerge&&MergeSwitch){
                if(filter_it==long_hair_iterator){
                    if(very_long_hair_ptr==tag){
                        very_long_hair_ptr=nullptr;
                    }
                    if(very_long_hair_ptr){
                        tag->retired=true;
                        long_hair_ptr=nullptr;
                        return false;
                    }
                    else
                        long_hair_ptr=tag;
                }
                else if(filter_it==very_long_hair_iterator){
                    if(long_hair_ptr==tag)
                        long_hair_ptr=nullptr;
                    very_long_hair_ptr=tag;
                    if(long_hair_ptr){
                        if(tag!=long_hair_ptr)
                            taglistwidget->tagErase(long_hair_ptr);
                        long_hair_ptr=nullptr;
                    }
                }
            }
            return true;
        }else return false;
    }
    void unlink(std::shared_ptr<teTag>tag)override{
        if(tag==long_hair_ptr)long_hair_ptr=nullptr;
        else if(tag==very_long_hair_ptr)very_long_hair_ptr=nullptr;
        teTagButtonGroup::unlink(tag);
    }
    void clear()override{
        long_hair_ptr=nullptr;
        very_long_hair_ptr=nullptr;
        teTagButtonGroup::clear();
    }
}*hairlength_bgop = new HairLength_Buttongroup;
contentLayout->addWidget(hairlength_bgop);
controls.push_back(hairlength_bgop);
}

QString teEditor_clothes_style = QStringLiteral(R"(
QPushButton#editor_switch{
font:16pt "Segoe UI";color:qlineargradient(spread:pad, x1:0, y1:0, x2:0, y2:1, stop:0 rgba(0, 255, 170, 255), stop:0.49162 rgba(191, 255, 0, 255), stop:1 rgba(0, 238, 255, 255)); background:black;
}
QPushButton#editor_switch:!checked{
border:1px solid rgb(50,50,50);
background:transparent;
}
QPushButton#editor_switch:checked{
border:2px solid #008b46;
})");

// Clothes types, grouped by the body area they belong to. The groups are only
// a visual aid: every button shares one exclusive group (built in
// teEditor_clothes), so a clothes tag still has exactly one type word.
QStringList headAndNeckClothesTypes{
    qsl("headdress"),qsl("headwear"),qsl("hat"),qsl("hood"),
    qsl("hairclip"),qsl("hairband")
};
QStringList upperBodyClothesTypes{
    qsl("shirt"),qsl("jacket"),qsl("coat"),qsl("hoodie"),qsl("sweater"),
    qsl("bra"),qsl("apron"),qsl("uniform")
};
QStringList lowerBodyClothesTypes{
    qsl("skirt"),qsl("shorts"),qsl("pants"),qsl("leotard"),
    qsl("thighhighs"),qsl("pantyhose"),qsl("socks")
};
QStringList underwearClothesTypes{
    qsl("underwear"),qsl("panties"),qsl("swimsuit"),qsl("bikini")
};
QStringList fullBodyClothesTypes{
    qsl("dress"),qsl("kimono")
};
QStringList feetClothesTypes{
    qsl("shoes"),qsl("boots"),qsl("footwear")
};
QStringList accessoryClothesTypes{
    qsl("bow"),qsl("ribbon"),qsl("bowtie"),qsl("gloves"),qsl("sleeves"),
    qsl("wings"),qsl("glasses"),qsl("horns"),qsl("bag")
};

/// Every word the clothes editor recognises as a garment type.
QStringList allClothesTypes = QStringList{}
    << headAndNeckClothesTypes << upperBodyClothesTypes << lowerBodyClothesTypes
    << underwearClothesTypes << fullBodyClothesTypes << feetClothesTypes
    << accessoryClothesTypes;

/// Generic garment words the user does not want, and the specific words they are
/// absorbed by. A tag that says "underwear" or "footwear" is merged into an
/// existing tag of the same family, and the *specific* spelling survives.
QHash<QString,QSet<QString>> genericClothesTypes{
    {qsl("underwear"),{qsl("panties")}},
    {qsl("footwear"),{qsl("shoes"),qsl("boots")}},
};

/// True when the two type words mean the same garment, i.e. may not both exist in
/// one tag list. A generic word matches every specific word of its family.
bool sameClothesType(const QString& a,const QString& b){
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
bool isGenericClothesType(const QString& type){
    return genericClothesTypes.contains(type);
}
std::multimap<QString, QString> prefix_back{
    {qsl("dress"),qsl("wedding")},
    {qsl("waist"),qsl("apron")},
    {qsl("bikini"),qsl("one-piece")},
    {qsl("swimsuit"),qsl("one-piece")},
    {qsl(""),qsl("fishnet")},
    {qsl(""),qsl("sleeveless")},
    { qsl("dress"),qsl("china") }
};
std::multimap<QString, QString> prefix_front{
    {qsl(""),qsl("torn")},
    {qsl(""),qsl("detached")},
    {qsl(""),qsl("unworn")},
    {qsl(""),qsl("short")},
    {qsl(""),qsl("long")},
    {qsl(""),qsl("frilled")}
};
QVector<QSet<QString>> exclusiveModifierGroups{

};

/// Prepositions that turn a tag into an interaction ("reaching through
/// panties", "hand in panties") instead of a piece of clothing.
///
/// This is deliberately a *small* list: it is the exclusion dictionary asked
/// for, and a word such as "with" is missing on purpose so that legitimate
/// garment tags ("dress with bow") keep working.
QStringList preposwords{
    qsl("in"),qsl("on"),qsl("under"),qsl("from"),qsl("through"),qsl("into"),
    qsl("onto"),qsl("inside"),qsl("beneath"),qsl("underneath"),qsl("at"),
    qsl("beside"),qsl("between"),qsl("across"),qsl("around"),qsl("against"),
    qsl("past"),qsl("toward"),qsl("towards")
};

/// True when a word before the trailing type word is a preposition, i.e. the
/// tag describes what someone does with a garment rather than the garment.
bool looksLikeActionPhrase(const teTag& tag){
    const int last = tag.words.size()-1;
    for(int i=0;i<last;++i)
        if(preposwords.contains(tag.words[i]->text))
            return true;
    return false;
}
teEditor_clothes::teEditor_clothes(teTagListView*in_taglistwidget,QString &&name, QString *styleSheet, QWidget *parent):teEditor_standard(in_taglistwidget,name, &teEditor_clothes_style, parent){
    struct ClothesList: teTagListControl{
        std::shared_ptr<teTag> clothes_editing=nullptr;
        struct teClothes:public teObject{
            ClothesList*parentList = nullptr;
            /**
             * @brief True once this wrapper has been retired.
             *
             * Retiring means "removed from all_clothes, delete it later". The
             * deletion itself has to be deferred: readCore() emits
             * edited_with_layout(), whose slot (reReadClothes) uses to delete the
             * wrapper - and readCore() then kept using `this`, so `parentList`
             * was read from freed memory (the crash in teeditor_derive.cpp:1768).
             * Every entry point checks this flag instead.
             */
            bool dead=false;
            teTagListView*parentTagListWidget=nullptr;
            std::shared_ptr<teTag>core=nullptr;
            QVector<teWord*>colors;
            QVector<teWord*>front_adjectives;
            QVector<teWord*>adjectives;
            QVector<teWord*>back_adjectives;
            QVector<teWord*>exclusiveModifiers;
            QString type;
            teClothes(std::shared_ptr<teTag>core,ClothesList*parent,teTagListView*in_parentTagListWidget):parentList(parent),parentTagListWidget(in_parentTagListWidget){
                readCore(core);
                core->teConnect(teCallbackType::edit,this,&teClothes::reReadClothes,core);
                core->teConnect(teCallbackType::edit_with_layout,this,&teClothes::reReadClothes,core);
            }
            // Deliberately no constructor without a parentList: every method here
            // goes through it, and a wrapper built without one was exactly the
            // dangling-parentList crash this class used to have.
            ~teClothes(){
                // Drop the callbacks the tag core holds for this wrapper, so a
                // deleted wrapper can never be called again.
                teDisconnect();
            }

            void reReadClothes(std::shared_ptr<teTag>tag){
                if(dead)
                    return;
                if(tag==parentList->clothes_editing)return;
                if(tag->retired)
                    return;
                teClothes*tagClothes = nullptr;
                if(tagClothes=parentList->findClothes(tag);!tagClothes){
                    return;
                }
                parentList->all_clothes.erase(tagClothes);
                // Snapshot: merge() below can retire other wrappers (it goes
                // through readCore() -> edited_with_layout() -> reReadClothes),
                // which would invalidate an iterator into the set.
                const QVector<teClothes*> others(parentList->all_clothes.begin(),
                                                 parentList->all_clothes.end());
                for(teClothes*clz:others){
                    if(clz==this||clz->dead||!parentList->all_clothes.count(clz))
                        continue;
                    if(clz->merge(tag)){
                        tag->retired=true;
                        parentTagListWidget->tagErase(tag);
                        parentList->retire(tagClothes);
                        return;
                    }
                }
                if(tagClothes->dead)
                    return;
                if(tagClothes->readCore()>=0)
                    parentList->all_clothes.insert(tagClothes);
                else
                    parentList->retire(tagClothes);
            }
            int readCore(std::shared_ptr<teTag>in_core=nullptr){
                if(dead)
                    return 0;
                if((core||!in_core)&&parentList->clothes_editing==core)return 0;
                if(core==in_core) in_core=nullptr;

                bool isNewEntry=false;
                bool ifhascolor=in_core?(!colors.empty()):false;
                bool isTypeChanged=false;
                if(in_core&&(!core)){
                    isNewEntry=true;
                    core=in_core;
                    type=*in_core->words.back();
                }
                else if((!in_core)&&core){
                    isNewEntry=true;
                    colors.clear();
                    front_adjectives.clear();
                    adjectives.clear();
                    back_adjectives.clear();
                    if(type!= *core->words.back()){
                        isTypeChanged=true;
                        auto it = parentList->all_clothes.find(this);
                        if(it!=parentList->all_clothes.end()){
                            parentList->all_clothes.erase(it);
                            type = *core->words.back();
                            parentList->all_clothes.insert(this);
                        }
                    }
                    in_core=core;
                    MergeSwitch=false;
                    if(!parentList->filter(core)){
                        parentList->unlink(core);
                        MergeSwitch=true;
                        return -1;
                    }
                    MergeSwitch=true;
                    // filter() re-enters this class through the tag signals and
                    // can retire this wrapper.
                    if(dead)
                        return 1;
                }
                auto [colorpos,colorcount] = is_color(*in_core);

                static auto findInMultimap = [](std::multimap<QString,QString>&map,QString& type,QString adj)->bool{
                    auto range = prefix_front.equal_range(type);
                    if(range.first!=range.second)
                        for (auto it = range.first; it != range.second; ++it) {
                            if (adj == it->second){
                                return true;
                            }
                        }
                    auto general_range = prefix_front.equal_range(qsl(""));
                    if(general_range.first!=general_range.second)
                        for (auto it = general_range.first; it != general_range.second; ++it) {
                            if (adj == it->second){
                                return true;
                            }
                        }
                    return false;
                };
                static auto ifduplicate = [](QVector<teWord*>vec,QString str)->bool{
                    for(teWord*wc:vec)
                        if(wc->text==str)
                            return true;
                    return false;
                };
                int wordcountMinusOne = in_core->words.count()-1;
                for(int i=0;i<wordcountMinusOne;++i){
                    if(i>=colorpos&&i<colorpos+colorcount){
                        if(ifhascolor)continue;
                        if(isNewEntry||!ifduplicate(colors,in_core->words[i]->text)){
                            colors.push_back(in_core->words[i]);
                            if(!isNewEntry){
                                teWord*takecore = in_core->takeWordAt(i,false);
                                Q_UNUSED(takecore);
                                --i;--wordcountMinusOne;--colorpos;
                            }
                        }
                    }
                    else if(findInMultimap(prefix_front,type,in_core->words[i]->text)){
                        if(isNewEntry||!ifduplicate(front_adjectives,in_core->words[i]->text)){
                            front_adjectives.push_back(in_core->words[i]);
                            if(!isNewEntry){
                                teWord*takecore = in_core->takeWordAt(i,false);
                                Q_UNUSED(takecore);
                                --i;--wordcountMinusOne;--colorpos;
                            }
                        }
                    }
                    else if(findInMultimap(prefix_back,type,in_core->words[i]->text)){
                        if(isNewEntry||!ifduplicate(back_adjectives,in_core->words[i]->text)){
                            back_adjectives.push_back(in_core->words[i]);
                            if(!isNewEntry){
                                teWord*takecore = in_core->takeWordAt(i,false);
                                Q_UNUSED(takecore);       // no widget for it: the list owns widgets
                                --i;--wordcountMinusOne;--colorpos;
                            }
                        }
                    }
                    else if(isNewEntry||!ifduplicate(adjectives,in_core->words[i]->text)){
                        adjectives.push_back(in_core->words[i]);
                        if(!isNewEntry){
                            teWord*takecore = in_core->takeWordAt(i,false);
                            Q_UNUSED(takecore);
                            --i;--wordcountMinusOne;--colorpos;
                        }
                    }
                }
                int wordindex=-1;
                for(teWord*wc:colors)
                    if(core->words[++wordindex]!=wc)
                        goto sortwords;
                for(teWord*wc:front_adjectives)
                    if(core->words[++wordindex]!=wc)
                        goto sortwords;
                for(teWord*wc:adjectives)
                    if(core->words[++wordindex]!=wc)
                        goto sortwords;
                for(teWord*wc:back_adjectives)
                    if(core->words[++wordindex]!=wc)
                        goto sortwords;

                return 0;
                sortwords:
                static auto insertWords = [](std::shared_ptr<teTag>tag,QVector<QVector<teWord*>>wordListList,teTagWidgetBase* tagWidget){
                        bool hasWidget=tagWidget!=nullptr;
                        if(hasWidget)
                            tagWidget->disconnectWord();
                        tag->words.clear();
                        int pos=-1;
                        QHash<QString, bool> seen;
                        for(QVector<teWord*>& wordList:wordListList)
                            for(teWord*wc:wordList){
                                if(seen[*wc]){
                                    delete wc;
                                    continue;
                                }
                                seen[*wc]=true;
                                if(hasWidget)
                                    tagWidget->insertWord(++pos,wc,true,false);
                                else
                                    tag->words.append(wc);
                            }
                    };
                teWord*typeWord;
                if(isTypeChanged){
                    typeWord=new teWord{type};
                    delete core->words.back();
                }else{
                    typeWord=core->words.back();
                }
                insertWords(core,{colors,front_adjectives,adjectives,back_adjectives,{typeWord}},parentList?parentList->tagWidgetFor(core):nullptr);
                parentList->clothes_editing = in_core;
                core->edited_with_layout();
                // The signal above may have retired this wrapper (and thus freed
                // nothing yet, but every member is off limits from then on).
                if(dead)
                    return 1;
                parentList->clothes_editing = nullptr;
                if(in_core&&in_core!=core){
                    parentList->clothes_editing = in_core;
                    in_core->retired=true;
                    in_core->edited_with_layout();
                    if(!dead)
                        parentList->clothes_editing=nullptr;
                }
                return 1;
            }
            void clear(){
                colors.clear();
                front_adjectives.clear();
                adjectives.clear();
                back_adjectives.clear();
                exclusiveModifiers.clear();
                type=nullptr;
                core=nullptr;
            }
            QString text(){
                QString final;
                for(teWord*wd:colors){
                    final.append(wd->text);
                    final.append(qsl(" "));
                }
                for(teWord*wd:front_adjectives){
                    final.append(wd->text);
                    final.append(qsl(" "));
                }
                for(teWord*wd:adjectives){
                    final.append(wd->text);
                    final.append(qsl(" "));
                }
                for(teWord*wd:back_adjectives){
                    final.append(wd->text);
                    final.append(qsl(" "));
                }
                final.append(type);
                return final;
            }
            QStringList allAdjectives(){
                QStringList final;
                for(teWord*wd:front_adjectives)
                    final.append(wd->text);
                for(teWord*wd:adjectives)
                    final.append(wd->text);
                for(teWord*wd:back_adjectives)
                    final.append(wd->text);
                return final;
            }

            bool merge(std::shared_ptr<teTag>in_core){
                if(dead)
                    return false;
                if(!sameType(in_core))return false;
                if(auto[cp,cc] = is_color(*in_core);cc>0&&!colors.empty()){
                    if(cc!=colors.count())return false;
                    for(int i = 0;i<cc;++i)
                        if(in_core->words[i+cp]->text!=colors[i]->text)
                            return false;
                }
                if(in_core==core)
                    return false;
                // The generic word is the one being absorbed, so the specific
                // spelling of the incoming tag replaces it: with "underwear" in
                // the list and "panties" typed, the result says "panties".
                const QString incoming = in_core->words.isEmpty()?QString{}:in_core->words.back()->text;
                if(!incoming.isEmpty()&&isGenericClothesType(type)&&!isGenericClothesType(incoming)){
                    type = incoming;
                    if(core&&!core->words.isEmpty())
                        core->words.back()->text = incoming;
                }
                readCore(in_core);
                return true;
            }
            bool sameType(std::shared_ptr<teTag>core){
                if(type.isEmpty()||core->words.empty())
                    return false;
                // Synonyms ("underwear" / "panties") and the generic words
                // ("footwear" / "boots") count as the same garment, so picking one
                // merges the other instead of leaving both.
                return sameClothesType(core->words.back()->text,type);
            }
        };

        std::multiset<teClothes*,std::function<bool(teClothes*, teClothes*)>>all_clothes{[](teClothes *a, teClothes *b)->bool{
            if(a->type==nullptr)return true;
            else if(b->type==nullptr)return false;
            else
                return a->type<b->type;
        }};
        /// Wrappers waiting to be deleted (see teClothes::dead).
        QVector<teClothes*> retired_clothes;
        bool purgeScheduled=false;
        /**
         * @brief Takes a wrapper out of all_clothes and deletes it later.
         *
         * Never delete a teClothes while anything may still be running inside it:
         * both reReadClothes() and readCore() are re-entered through the tag's
         * signals, and they used to delete the wrapper they were called from.
         */
        void retire(teClothes*wrapper){
            if(!wrapper||wrapper->dead)
                return;
            wrapper->dead=true;
            wrapper->core.reset();
            all_clothes.erase(wrapper);
            retired_clothes.append(wrapper);
            purgeRetired();
        }
        void purgeRetired(){
            if(purgeScheduled||retired_clothes.isEmpty())
                return;
            purgeScheduled=true;
            // Deferred to the event loop: everything that is still on the stack
            // only looks at the dead flag, which is already set.
            QTimer::singleShot(0,this,[this]{
                purgeScheduled=false;
                for(teClothes*wrapper:retired_clothes)
                    delete wrapper;
                retired_clothes.clear();
            });
        }
        void purgeRetiredNow(){
            purgeScheduled=false;
            for(teClothes*wrapper:retired_clothes)
                delete wrapper;
            retired_clothes.clear();
        }
        teClothes* findClothes(std::shared_ptr<teTag>tag){
            for(teClothes*clz:all_clothes){
                if(clz->core==tag){
                    return clz;
                }
            }
            telog("[reReadClothes]:didn't find clothes object for input tag");
            return nullptr;
        }
        ClothesList(colorsWidget*in_onEdit_widget,teTagListView*parentlist,QWidget*parent = nullptr,QString*styleSheet=nullptr)
            :teTagListControl(in_onEdit_widget,parentlist,parent,styleSheet,"clothes"){
            this->info=QStringLiteral("ClothesList");
            sc->setMinimumHeight(110);
        }
        ~ClothesList(){
            // The deferred purge is bound to this widget, so it will not run any
            // more: delete what it was still holding.
            purgeRetiredNow();
        }
        bool filter(std::shared_ptr<teTag>in_tag)override{
            if(in_tag==clothes_editing)return true;
            if(in_tag->words.empty())return false;
            // "reaching through panties" and friends are actions, not garments.
            if(looksLikeActionPhrase(*in_tag))
                return false;
            if(allClothesTypes.contains(*in_tag->words.back())){
                if(autoMerge&&MergeSwitch){
                    if(!all_clothes.empty()){
                        // Every wrapper is a candidate: the type words that mean
                        // the same garment ("panties"/"underwear") are *not*
                        // adjacent in the type-ordered set, so looking them up
                        // with lower_bound()/upper_bound() never found them -
                        // which is why the merge was not automatic.
                        // Snapshot first: readCore()/merge() re-enter this class
                        // through the tag signals and can retire wrappers.
                        const QVector<teClothes*> candidates(all_clothes.begin(),all_clothes.end());
                        for(teClothes*clothes:candidates){
                            if(clothes->dead||!all_clothes.count(clothes))
                                continue;
                            if(in_tag==clothes->core){
                                clothes->readCore();
                                return false;
                            }
                        }
                        for(teClothes*clothes:candidates){
                            if(clothes->dead||!all_clothes.count(clothes))
                                continue;
                            if(clothes->merge(in_tag)){
                                in_tag->retired=true;
                                return false;
                            }
                        }
                    }
                    auto newClothse = new teClothes(in_tag,this,taglistwidget);
                    all_clothes.insert(newClothse);
                }
                return true;
            }
            return false;
        }

        virtual void clear()override{
            for(teClothes*clothes:all_clothes)
                clothes->dead=true;         // no callback may touch them any more
            purgeRetiredNow();
            all_clothes.clear();
            teTagListControl::clear();
        };
        void link(std::shared_ptr<teTag>in_tag)override{
            linked_tags.insert(in_tag);
            in_tag->teConnect(teCallbackType::destroy,this,&teEditorControl::unlink,in_tag);
            teTagWidgetBase*widget =taginsert(-1,in_tag,taglistwidget->showing_list);
            extraWidgetPushBack(in_tag,widget);
        }
        void connectTag(teTagWidgetBase*tag)override{
            teTagListWidgetBase::connectTag(tag);
        }
        void unlink(std::shared_ptr<teTag>in_tag)override{
            // teDisconnect(in_tag.get());
            for(teClothes*clothes:QVector<teClothes*>(all_clothes.begin(),all_clothes.end()))
                if(clothes->core==in_tag){
                    retire(clothes);
                    break;
                }
            tagErase(in_tag);
            linked_tags.erase(in_tag);
        }
        void extraWidgetPushBack(std::shared_ptr<teTag>in_tag,teTagWidgetBase*widget){
            if(*in_tag->words.back()==qsl("bow")){
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
                    if(ifchecked)
                        if(auto* editorTag=tagWidgetFor(in_tag)) editorTag->insertWord(-2,qsl("hair"));
                    else{
                        int wordcount = in_tag->words.size();
                        for(int i =0;i<wordcount;++i)
                            if(*in_tag->words[i]==qsl("hair"))
                            {if(auto* editorTag=tagWidgetFor(in_tag)) editorTag->destroyWord(i);--i;--wordcount;}
                    }
                },Qt::DirectConnection);
                widget->insertExtraWidgets(this,hairbow_btn);
            }
        }
        void refreshState()override{

        }
    }*clothes_list = new ClothesList(new colorsWidget(
        {
            { {qsl("torn"),qsl("striped"),qsl("fishnet"),qsl("frilled")}, false },
            { headAndNeckClothesTypes, true },
            { upperBodyClothesTypes, true },
            { lowerBodyClothesTypes, true },
            { underwearClothesTypes, true },
            { fullBodyClothesTypes, true },
            { feetClothesTypes, true },
            { accessoryClothesTypes, true },
        },
        nullptr, 0, {},
        { QString{}, qsl("head / neck"), qsl("upper body"), qsl("legs"),
          qsl("underwear / swimwear"), qsl("full body"), qsl("feet"),
          qsl("accessories") },
        // Every type button belongs to one exclusive group, so a clothes tag
        // always keeps exactly one type word. The columns put the colours and the
        // features on the left and the clothes types on the right, which keeps
        // this dialog square instead of a tall strip.
        new QButtonGroup(nullptr),
        false,
        { 0, 1, 1, 1, 1, 1, 1, 1 }),taglistwidget);
    contentLayout->addWidget(clothes_list);
    controls.push_back(clothes_list);

    QHBoxLayout* secondline_layout = new QHBoxLayout;
    QHBoxLayout* thirdline_layout = new QHBoxLayout;
    secondline_layout->setSpacing(1);
    thirdline_layout->setSpacing(1);
    teTagCheckBox* torn_cb = new teTagCheckBox({ "torn clothes" });
    teTagCheckBox* fishnets_cb = new teTagCheckBox({ "fishnets" });
    teTagCheckBox* frills_cb = new teTagCheckBox({ "frills" });
    teTagCheckBox* striped_cb = new teTagCheckBox({ "striped clothes" });
    {
        secondline_layout->addWidget(torn_cb);
        secondline_layout->addWidget(fishnets_cb);
        controls.push_back(torn_cb);
        controls.push_back(fishnets_cb);
        contentLayout->addLayout(secondline_layout);

        thirdline_layout->addWidget(frills_cb);
        thirdline_layout->addWidget(striped_cb);
        controls.push_back(frills_cb);
        controls.push_back(striped_cb);
        contentLayout->addLayout(thirdline_layout);
    }

struct Ears_Buttongroup :teTagButtonGroup {
    Ears_Buttongroup(QWidget* parent = nullptr, QString* styleSheet_button = nullptr, QString* styleSheet_label = nullptr)
        :teTagButtonGroup(
            {{qsl("🐱"),qsl("cat"),true,true,false},
            {qsl("🐰"),qsl("rabbit"),true,true,false},
            {qsl("🦊"),qsl("fox"),true,true,false},
            {qsl("🐶"),qsl("dog"),true,true,false},
             {qsl("🐺"),qsl("wolf"),true,true,false},
             {qsl("🐴"),qsl("horse"),true,true,false},
             {qsl("pointy"),qsl("pointy"),true,true,false},
             {qsl("ears"),qsl("ears"),false,true,false},
        }){}
        virtual void reform(int id)override {
        if(buttons[id]->isChecked())
        for(int i =1;i<7;++i)
            buttons[(id+i)%7]->setChecked(false);}
        std::shared_ptr<teTag>animal_ears_ptr;
        bool explicit_ear=false;
bool filter(std::shared_ptr<teTag>in)override{
    QVector<teWord*>&words = in->words;
    if(words.size()!=2)return false;
    if(words.back()->text!=qsl("ears"))return false;
    if(words.front()->text==qsl("animal")){
        if(explicit_ear){
            in->retired=true;
            animal_ears_ptr=nullptr;
            return false;
        }
        else
            animal_ears_ptr=in;
    }
    else{
        if(animal_ears_ptr){
            if(in!=animal_ears_ptr)
                taglistwidget->tagErase(animal_ears_ptr);
            animal_ears_ptr=nullptr;
        }
        explicit_ear=true;
    }
    return true;
}
void clear()override{
    animal_ears_ptr=nullptr;
    explicit_ear=false;
    teTagButtonGroup::clear();
}
void unlink(std::shared_ptr<teTag>tag)override{
    if(tag==animal_ears_ptr)animal_ears_ptr=nullptr;
    teTagButtonGroup::unlink(tag);
}
void getDefaultFiltStrings()override{}
}*ears_Buttongroup = new Ears_Buttongroup;
contentLayout->addWidget(ears_Buttongroup);
controls.push_back(ears_Buttongroup);

}


QString teEditor_nsfw_style = QStringLiteral(R"(
QPushButton#editor_switch{
font:16pt "Segoe UI";color:qlineargradient(spread:pad, x1:0, y1:0, x2:0, y2:1, stop:0 rgba(255, 0, 0, 255), stop:0.536313 rgba(217, 0, 255, 255), stop:1 rgba(98, 0, 255, 255)); background:black;
}
QPushButton#editor_switch:!checked{
border:1px solid rgb(50,50,50);
background:transparent;
}
QPushButton#editor_switch:checked{
border:2px solid #008b46;
})");
teEditor_nsfw::teEditor_nsfw(teTagListView*in_taglistwidget,QString &&name, QString *styleSheet, QWidget *parent):teEditor_standard(in_taglistwidget,name, &teEditor_nsfw_style, parent){

    static QSet<QString> object1 {qsl("cum"),qsl("erection"),qsl("tentacle"),qsl("egg"),qsl("slime"),qsl("worm"),qsl("insect")};
    static QSet<QString> bodyparts{qsl("pussy"),qsl("ass"),qsl("body"),qsl("face"),qsl("mouth"),qsl("breasts"),qsl("uterus"),qsl("clothes"),qsl("panties"),qsl("penis"),qsl("nipples"),qsl("urethra")};
    struct Object_list: teTagListControl{
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
        void refreshState()override{}
    }*object_list = new Object_list(new colorsWidget({{QStringList{object1.begin(),object1.end()},true},{QStringList{preposwords.begin(),preposwords.end()},true},{QStringList{bodyparts.begin(),bodyparts.end()},true}},nullptr,-1),taglistwidget);
    contentLayout->addWidget(object_list);
    controls.push_back(object_list);

    {
        const int layout_count = 32;
        QHBoxLayout* layouts[layout_count];
        for(int i =0;i<layout_count;++i){
            layouts[i] = new QHBoxLayout;
            layouts[i]->setSpacing(1);
        }
        auto insertToEditor = [this](QBoxLayout*layout,QVector<teEditorControl*>&controls_array,QVector<teEditorControl*>controls,QString title={}){
            if(!title.isEmpty()){
                QLabel*titleLabel = new QLabel(title);
                titleLabel->setProperty("type","title");
                titleLabel->setStyleSheet(qsl("font:italic 14px;color:rgb(200,200,200);"));
                contentLayout->addWidget(titleLabel);
            }
            for(auto control:controls){
                layout->addWidget(dynamic_cast<QWidget*>(control));
                controls_array.push_back(control);
            }
            contentLayout->addLayout(layout);
        };
        int i=-1;


        teTagCheckBox* sex_cb = new teTagCheckBox({ "sex" });
        teTagCheckBox* nude_cb = new teTagCheckBox({ "nude" });
        teTagCheckBox* vaginal_cb = new teTagCheckBox({ "vaginal" });
        teTagCheckBox* anal_cb = new teTagCheckBox({ "anal" });
        teTagCheckBox* oral_cb = new teTagCheckBox({ "oral" });
        insertToEditor(layouts[++i],controls,{sex_cb,nude_cb,vaginal_cb,anal_cb,oral_cb},"type");

        teTagButtonGroup* vaginal_bg = new teTagButtonGroup{{{qsl("aft"),qsl("after"),true},{qsl("imm"),qsl("imminent"),true},{qsl("vaginal"),qsl("vaginal"),false}}};
        teTagButtonGroup* rape_bg = new teTagButtonGroup{{{qsl("aft"),qsl("after"),true},{qsl("imm"),qsl("imminent"),true},{qsl("rape"),qsl("rape"),false}}};
        insertToEditor(layouts[++i],controls,{vaginal_bg,rape_bg});

        teTagCheckBox* paizuri_cb = new teTagCheckBox({ "paizuri" });
        teTagCheckBox* footjob_cb = new teTagCheckBox({ "footjob" });
        teTagCheckBox* fingering_cb = new teTagCheckBox({ "fingering" });
        teTagCheckBox* group_cb = new teTagCheckBox({ "group","group sex" });
        insertToEditor(layouts[++i],controls,{paizuri_cb,footjob_cb,fingering_cb,group_cb});

        teTagCheckBox*cervical_penetration_cb =new teTagCheckBox({ "cervical penetration" });
        insertToEditor(layouts[++i],controls,{cervical_penetration_cb});

        teTagCheckBox*female_masturbation_cb =new teTagCheckBox({"(female)", "female masturbation" });
        teTagCheckBox*male_masturbation_cb =new teTagCheckBox({ "(male)", "male masturbation"  });
        teTagCheckBox*masturbation_cb =new teTagCheckBox({ "masturbation" });
        insertToEditor(layouts[++i],controls,{female_masturbation_cb,male_masturbation_cb,masturbation_cb});

        teTagCheckBox*futanari_cb = new teTagCheckBox({qsl("futanari")});
        teTagCheckBox*tomgirl_cb = new teTagCheckBox({qsl("tomgirl")});
        teTagCheckBox*otoko_no_ko_cb = new teTagCheckBox({qsl("otoko no ko")});
        insertToEditor(layouts[++i],controls,{futanari_cb,tomgirl_cb,otoko_no_ko_cb},"gender");

        teTagCheckBox* nakadashi_cb = new teTagCheckBox({ "cum inside" });
        teTagCheckBox* lacatation_cb = new teTagCheckBox({ "lactation" });
        teTagCheckBox* sweat_cb = new teTagCheckBox({ "sweat" });
        insertToEditor(layouts[++i],controls,{nakadashi_cb,lacatation_cb,sweat_cb},"liquid");

        teTagCheckBox* female_ejaculation_cb = new teTagCheckBox({ "(female)", "female ejaculation" });
        teTagCheckBox* ejaculation_cb = new teTagCheckBox({ "ejaculation" });
        insertToEditor(layouts[++i],controls,{female_ejaculation_cb,ejaculation_cb});

        teTagCheckBox* orgasm_cb = new teTagCheckBox({ "orgasm" });
        teTagCheckBox* orgasm_face_cb = new teTagCheckBox({ "face", "orgasm face" });
        teTagCheckBox* furrowed_brow_cb = new teTagCheckBox({ "furrowed brow" });
        insertToEditor(layouts[++i],controls,{orgasm_cb,orgasm_face_cb,furrowed_brow_cb},"expression");
        teTagCheckBox* torogao_cb = new teTagCheckBox({ "torogao" });
        teTagCheckBox* ahegao_cb = new teTagCheckBox({ "ahegao" });
        teTagCheckBox* moaning_cb = new teTagCheckBox({ "moaning" });
        insertToEditor(layouts[++i],controls,{torogao_cb,ahegao_cb,moaning_cb});

        teTagCheckBox* trembling_cb = new teTagCheckBox({ "trembling" });
        teTagCheckBox* speech_bubble_cb = new teTagCheckBox({ "spch bubble","speech bubble" });
        teTagCheckBox* sound_effects_cb = new teTagCheckBox({ "sd effects" , "sound effects" });
        insertToEditor(layouts[++i],controls,{trembling_cb,speech_bubble_cb,sound_effects_cb},"comic elements");
        teTagCheckBox* text_cb = new teTagCheckBox({ "text" });
        teTagCheckBox* xray_cb = new teTagCheckBox({ "x-ray" });
        teTagCheckBox* emphasis_lines_cb = new teTagCheckBox({ "emphasis lines" });
        insertToEditor(layouts[++i],controls,{text_cb,xray_cb,emphasis_lines_cb});

        teTagCheckBox* internal_cumshot_cb = new teTagCheckBox({ "internal cumshot" });
        teTagCheckBox* cross_section_cb = new teTagCheckBox({ "cross-section" });
        insertToEditor(layouts[++i],controls,{internal_cumshot_cb,cross_section_cb});

        teTagCheckBox*impregnation_cb =new teTagCheckBox({ "impregnation" });
        teTagCheckBox*fertilization_cb =new teTagCheckBox({ "fertilization" });
        insertToEditor(layouts[++i],controls,{impregnation_cb,fertilization_cb});

        teTagCheckBox* female_pubic_hair_cb = new teTagCheckBox({ "(female)", "female pubic hair" });
        teTagCheckBox* male_pubic_hair_cb = new teTagCheckBox({  "(male)","male pubic hair" });
        teTagCheckBox* pubic_hair_cb = new teTagCheckBox({ "pubic hair" });
        insertToEditor(layouts[++i],controls,{female_pubic_hair_cb,male_pubic_hair_cb,pubic_hair_cb},"physical traits");

        teTagCheckBox*large_penis_cb = new teTagCheckBox({ "large penis"  });
        teTagCheckBox*large_insertion_cb = new teTagCheckBox({ "large insertion"  });
        insertToEditor(layouts[++i],controls,{large_penis_cb,large_insertion_cb});

        teTagCheckBox*smalldom_cb =new teTagCheckBox({ "smalldom" });
        teTagCheckBox*size_difference_cb =new teTagCheckBox({ "size diff","size difference" });
        teTagCheckBox*mounting_cb =new teTagCheckBox({ "mounting" });
        insertToEditor(layouts[++i],controls,{smalldom_cb,size_difference_cb,mounting_cb});

        teTagCheckBox* pregnant_cb = new teTagCheckBox({ "pregnant" });
        teTagCheckBox* cum_inflation_cb = new teTagCheckBox({ "cum inflation" });
        insertToEditor(layouts[++i],controls,{pregnant_cb,cum_inflation_cb});


        teTagCheckBox*riding_machine_cb =new teTagCheckBox({"ride","riding machine" });
        teTagCheckBox*sex_machine_cb =new teTagCheckBox({"machine","sex machine" });
        teTagCheckBox*sex_toy_cb =new teTagCheckBox({"toy", "sex toy"  });
        teTagCheckBox*dildo_cb =new teTagCheckBox({"dildo"});
        teTagCheckBox*gaping_cb = new teTagCheckBox({ "gap","gaping"  });
        insertToEditor(layouts[++i],controls,{riding_machine_cb,sex_machine_cb,sex_toy_cb,dildo_cb,gaping_cb},"object");

        teTagCheckBox*vaginal_object_cb =new teTagCheckBox({"vaginal","vaginal object insertion" });
        teTagCheckBox*anal_object_cb =new teTagCheckBox({"anal", "anal object insertion"  });
        teTagCheckBox*object_cb = new teTagCheckBox({ "object insertion", "object insertion"  });
        insertToEditor(layouts[++i],controls,{vaginal_object_cb,anal_object_cb,object_cb});

        teTagCheckBox* birth_cb = new teTagCheckBox({ "giving birth" });
        teTagCheckBox* unbirth_cb = new teTagCheckBox({ "unbirth" });
        teTagCheckBox* prolapse_cb =new teTagCheckBox({ "prolapse" });
        insertToEditor(layouts[++i],controls,{birth_cb,unbirth_cb,prolapse_cb});

        teTagCheckBox*bestiality_cb =new teTagCheckBox({ "bestiality" });
        teTagCheckBox*dog_cb =new teTagCheckBox({ "🐶","dog" });
        teTagCheckBox*horse_cb =new teTagCheckBox({ "🐴","horse" });
        teTagCheckBox*pig_cb =new teTagCheckBox({ "🐷","pig" });
        teTagCheckBox*frog_cb =new teTagCheckBox({ "🐸","frog" });
        insertToEditor(layouts[++i],controls,{bestiality_cb,dog_cb,horse_cb,pig_cb,frog_cb});
        teTagCheckBox*snake_cb =new teTagCheckBox({ "🐍","snake" });
        teTagCheckBox*orangutan_cb =new teTagCheckBox({ "🦍","orangutan" });
        teTagCheckBox*monkey_cb =new teTagCheckBox({ "🐵","monkey" });
        teTagCheckBox*insect_cb =new teTagCheckBox({ "🦋","insect" });
        teTagCheckBox*worm_cb =new teTagCheckBox({ "🐛","worm" });
        teTagCheckBox*slime_cb =new teTagCheckBox({ "💧","slime" });
        teTagCheckBox*goblin_cb =new teTagCheckBox({ "👺","goblin" });
        insertToEditor(layouts[++i],controls,{snake_cb,orangutan_cb,monkey_cb,insect_cb,worm_cb,slime_cb,goblin_cb});

        teTagCheckBox*living_clothes_cb =new teTagCheckBox({"living clothes"});
        teTagCheckBox*bondage_cb =new teTagCheckBox({"bondage"});
        teTagCheckBox*tickling_cb =new teTagCheckBox({"tickling"});
        insertToEditor(layouts[++i],controls,{living_clothes_cb,bondage_cb,tickling_cb});

        teTagCheckBox*plant_tentacles_cb =new teTagCheckBox({"plant", "plant tentacles" });
        teTagCheckBox*tentacles_cb =new teTagCheckBox({ "tentacles" });
        teTagCheckBox*ovipositor_cb =new teTagCheckBox({ "ovipositor" });
        teTagCheckBox*egg_cb =new teTagCheckBox({ "egg" });
        insertToEditor(layouts[++i],controls,{plant_tentacles_cb,tentacles_cb,ovipositor_cb,egg_cb});

        teTagCheckBox* cowgirl_position = new teTagCheckBox({ "cowgirl","cowgirl position" });
        teTagCheckBox* from_behind_cb=  new teTagCheckBox({ "from behind","sex from behind" });
        teTagCheckBox* leg_lock_cb = new teTagCheckBox({ "leg lock" });
        insertToEditor(layouts[++i],controls,{cowgirl_position,from_behind_cb,leg_lock_cb},"sexual positions");

        teTagCheckBox* doggystyle_cb = new teTagCheckBox({ "doggystyle" });
        teTagCheckBox* spooning_cb = new teTagCheckBox({ "spooning" });
        teTagCheckBox* piledriver_cb = new teTagCheckBox({ "piledriver","piledriver (sex)" });
        insertToEditor(layouts[++i],controls,{doggystyle_cb,spooning_cb,piledriver_cb});
struct legUp_Buttongroup :teTagButtonGroup {
    legUp_Buttongroup(QWidget* parent = nullptr, QString* styleSheet_button = nullptr, QString* styleSheet_label = nullptr)
        :teTagButtonGroup({
            {qsl("one"),qsl("one"),true,true,false},
            {qsl("leg(s)"),qsl("leg"),true,true,false},
            {qsl("up"),qsl("up"),false,true,false},})
        {}
    virtual void reform(int id)override {
        enum {
            one,leg
        };
    if(id==leg)
    buttons[one]->setChecked(false);
    if(buttons[one]->isChecked()){
        allwidgets[leg].data=qsl("leg");
        if(id==one)buttons[leg]->setChecked(true);
    }
    else{
        allwidgets[leg].data=qsl("legs");
    }
}

bool filter(std::shared_ptr<teTag>in)override{
    QVector<teWord*>&words = in->words;
    if(words.size()<2)return false;
    if(words.at(words.size()-2)->text.startsWith(qsl("leg"))&&in->words.back()->text==qsl("up"))
        return true;
    return false;
}
}*legup_bgop = new legUp_Buttongroup;
contentLayout->addWidget(legup_bgop);
controls.push_back(legup_bgop);
teTagCheckBox* spread_legs_cb = new teTagCheckBox({ "spread" ,qsl("spread legs")});
teTagCheckBox* folded_cb = new teTagCheckBox({ "folded" ,qsl("folded")});
insertToEditor(layouts[++i],controls,{legup_bgop,spread_legs_cb,folded_cb});


teTagCheckBox* toddlercon_cb = new teTagCheckBox({ "toddlercon" });
teTagCheckBox* diaper_cb = new teTagCheckBox({ "diaper" });
insertToEditor(layouts[++i],controls,{toddlercon_cb,diaper_cb},"misc");
        teTagCheckBox*fisting_cb =new teTagCheckBox({"fisting"});
        teTagCheckBox*omorashi_cb =new teTagCheckBox({"omorashi"});
        teTagCheckBox*urination_cb =new teTagCheckBox({"peeing"});
        teTagCheckBox*fart_cb =new teTagCheckBox({"fart"});
        insertToEditor(layouts[++i],controls,{fisting_cb,omorashi_cb,urination_cb,fart_cb});
        teTagCheckBox*vore_cb =new teTagCheckBox({"vore"});
        teTagCheckBox*vomit_cb =new teTagCheckBox({"vomit"});
        teTagCheckBox*enema_cb =new teTagCheckBox({"enema"});
        teTagCheckBox*scat_cb =new teTagCheckBox({"scat"});
        insertToEditor(layouts[++i],controls,{vore_cb,vomit_cb,enema_cb,scat_cb});


        teTagCheckBox*guro_cb =new teTagCheckBox({"guro"});
        teTagCheckBox*snuff_cb =new teTagCheckBox({"snuff"});
        teTagCheckBox*amputee_cb =new teTagCheckBox({"amputee"});
        teTagCheckBox*netorare_cb =new teTagCheckBox({"netorare"});
        insertToEditor(layouts[++i],controls,{guro_cb,snuff_cb,amputee_cb,netorare_cb});
    }


teTagComboBox* censor_cbb = new teTagComboBox(
        {{qsl("(none)"),{}},
            {qsl("bar censor"),{qsl("bar censor"),qsl("censored")}},
            {qsl("blank censor"),{qsl("blank censor"),qsl("censored")}},
            {qsl("heart censor"),{qsl("heart censor"),qsl("censored")}},
            {qsl("blur censor"),{qsl("blur censor"),qsl("censored")}},
            {qsl("mosaic censoring"),{qsl("mosaic censoring"),qsl("censored")}},
         {qsl("light censor"),{qsl("light censor"),qsl("censored")}},
            {qsl("censored"),{qsl("censored")}}
        },QStringLiteral("(censor)"));

contentLayout->addWidget(censor_cbb);
controls.push_back(censor_cbb);
}
