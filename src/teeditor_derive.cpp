#include "teeditor_derive.h"
#include "teeditorcontrol.h"
// The controls live in these headers now; they used to be declared inside the
// editor constructors below, which made this file a 2400 line wall.
#include "teclothescontrol.h"
#include "tefacecontrols.h"
#include "tebodycontrols.h"
#include "tehaircontrols.h"
#include "temisccontrols.h"
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

QPair<int,int> is_color(const teTag& tag,int offset) {
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
Mature_Buttongroup* mature_bgop = new Mature_Buttongroup;
contentLayout->addWidget(mature_bgop);
controls.push_back(mature_bgop);

multipeople_CheckBox* mcb = new multipeople_CheckBox(QStringLiteral("multiple (people)s"));
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

smile_buttongroup* smile_bgop = new smile_buttongroup;
contentLayout->addWidget(smile_bgop);
controls.push_back(smile_bgop);

ClosedEyes_Buttongroup* closedeyes_bgop = new ClosedEyes_Buttongroup;
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

mouth_buttongroup* mouth_bgop = new mouth_buttongroup;
contentLayout->addWidget(mouth_bgop);
controls.push_back(mouth_bgop);

Breast_Buttongroup* hairlength_bgop = new Breast_Buttongroup;
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
hair_and_eyes_color_list* colors_list = new hair_and_eyes_color_list(new colorsWidget({{{qsl("gradient"),qsl("streaked")},true},{{qsl("hair"),qsl("eyes")},true}},nullptr),taglistwidget);
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

ponytail_buttongroup* ponytail_bgop = new ponytail_buttongroup;
contentLayout->addWidget(ponytail_bgop);
controls.push_back(ponytail_bgop);

teTagCheckBox* two_side_up_cb = new teTagCheckBox({ "two side up" });
contentLayout->addWidget(two_side_up_cb);
controls.push_back(two_side_up_cb);

BangsList* bangs_list = new BangsList(
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

HairLength_Buttongroup* hairlength_bgop = new HairLength_Buttongroup;
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

teEditor_clothes::teEditor_clothes(teTagListView*in_taglistwidget,QString &&name, QString *styleSheet, QWidget *parent):teEditor_standard(in_taglistwidget,name, &teEditor_clothes_style, parent){
ClothesList* clothes_list = new ClothesList(new colorsWidget(
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

Ears_Buttongroup* ears_Buttongroup = new Ears_Buttongroup;
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

Object_list* object_list = new Object_list(new colorsWidget({{QStringList{object1.begin(),object1.end()},true},{QStringList{preposwords.begin(),preposwords.end()},true},{QStringList{bodyparts.begin(),bodyparts.end()},true}},nullptr,-1),taglistwidget);
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
legUp_Buttongroup* legup_bgop = new legUp_Buttongroup;
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
