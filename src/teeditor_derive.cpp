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
#include "testyle.h"
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
teEditor_custom::teEditor_custom(teTagListView*in_taglistwidget,QString&& name, QString* styleSheet, QWidget* parent):teEditor(in_taglistwidget,name, parent) {
    if(!styleSheet)
        setStyleSheet(teStyle::sheet(QStringLiteral("editor_custom")));
    QPushButton* newtagButton = new QPushButton("add custom tags");
    mainLayout->setContentsMargins(1,1,1,1);
    mainLayout->setSpacing(1);
    // Same 1 px gap as every other editor control keeps; the custom tags used to
    // sit 5 px / 3 px apart and looked like a different grid.
    flowLayout->setVerticalSpacing(1);
    flowLayout->setHorizontalSpacing(1);
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
        for(EditorControl*ec:controls)
            delete ec;
        controls.clear();string_controls.clear();
        updateGeometry();
    });
}

EditorControl* teEditor_custom::findControl(const QString& str) const {
    for(const auto& [text,control]:string_controls)
        if(text==str)
            return control;
    return nullptr;
}

void teEditor_custom::removeControl(const QString &str){
    for(int i=0;i<string_controls.size();++i){
        if(string_controls[i].first!=str)
            continue;
        EditorControl* control = string_controls[i].second;
        string_controls.removeAt(i);
        controls.removeAll(control);
        if(auto* widget = dynamic_cast<QWidget*>(control)){
            flowLayout->removeWidget(widget);   // deletes the layout item
            widget->hide();
        }
        delete control;
        updateGeometry();
        return;
    }
}

void teEditor_custom::addControl(const QString &str){
    teTagCheckBox* cbptr=new teTagCheckBox({str,str});
    string_controls.append({str,cbptr});
    // Registers it (it learns the tag list there) instead of two hand-written
    // lines that had to agree with each other.
    registerControl(cbptr);
    if(taglist)
        for(std::shared_ptr<teTag>tc:*taglist)
            cbptr->read(tc);
    flowLayout->addWidget(cbptr);   // addWidget() reparents the button
    controlReorderer->attach(cbptr);
    // The tag row makes the editor taller: the dialog / the config can add
    // controls long after the page was laid out, and the page layout has to be
    // told (see QFlowLayout::addItem()).
    updateGeometry();
}

bool teEditor_custom::event(QEvent* event){
    switch (event->type()) {
    case QEvent::StyleChange:
    case QEvent::FontChange:
        // The tag buttons inherit their font (and so their height) from the
        // editor column's style sheet, and that sheet is applied when the editor
        // is dropped into the column - i.e. after this editor was built and
        // already asked for room. Without re-asking, the page keeps the height
        // the editor had as an empty frame and the rows are painted on top of the
        // "add custom tags" button.
        updateGeometry();
        break;
    default:
        break;
    }
    return QFrame::event(event);
}

teEditor_custom::~teEditor_custom(){
    // save_config() lists the custom tags through this pointer; it must not keep
    // pointing into a destroyed editor. A rebuilt layout (layout dialog, NSFW
    // toggle) deletes editors, and the next save then read freed memory - the
    // release build reported that as an unhandled win32 exception.
    if (custom_controls == &string_controls)
        custom_controls = nullptr;
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
        EditorControl* control=findControl(str);
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
    updateGeometry();
}


/**
 * @brief The people editor: a page of coarse tags plus a sub-page of details.
 *
 * The rows below are the whole page. Every control is created and handed to
 * addRow()/addRowTo(), which is what puts it on screen and registers it with the
 * editor - there is no second list to keep in step, and no `controls.push_back`
 * to forget (a forgotten one used to be a control that silently never saw a tag).
 */
teEditor_pretreat::teEditor_pretreat(teTagListView*in_taglistwidget,QString&& name, QWidget* parent) :teEditor_standard(in_taglistwidget,name, QStringLiteral("pretreat"), parent) {
    // --- the people sub-page -------------------------------------------------
    auto* peopleButton = new QPushButton;
    contentLayout->addWidget(peopleButton);

    auto* peoplePanel = new QWidget;
    auto* peopleLayout = new QVBoxLayout(peoplePanel);
    peopleLayout->setSpacing(1);
    peopleLayout->setContentsMargins(0,0,0,0);

    auto* girls = new teTagLineedit_people(qsl("girl"),qsl("girl(s)"));
    auto* boys  = new teTagLineedit_people(qsl("boy"),qsl("boy(s)"));
    auto* others= new teTagLineedit_people(qsl("other"),qsl("other(s)"));
    addRowTo(peopleLayout,{{},{},{girls}});
    addRowTo(peopleLayout,{{},{},{boys}});
    addRowTo(peopleLayout,{{},{},{others}});

    auto* furry = new teTagCheckBox({qsl("furry")});
    auto* magicalGirl = new teTagCheckBox({qsl("magical girl")});
    auto* elf = new teTagCheckBox({qsl("elf")});
    auto* mesugaki = new teTagCheckBox({qsl("mesugaki")});
    addRowTo(peopleLayout,{{},{},{furry,magicalGirl,elf,mesugaki}});

    addSubPage(peopleButton,peoplePanel);
    // The sub-page controls are still editor controls: the group below is what
    // makes the three counts behave like one line ("1girl, 2boys, ...").
    new page1_button_link(peopleButton,{girls,boys,others,furry,magicalGirl,elf,mesugaki},this);

    // --- the main page -------------------------------------------------------
    auto* loli = new teTagCheckBox({"loli"});
    auto* shota = new teTagCheckBox({"shota"});
    auto* chibi = new teTagCheckBox({"chibi"});
    auto* solo = new teTagCheckBox({"solo"});
    addRow({{},{},{loli,shota,chibi,solo}});

    addControl(new Mature_Buttongroup);
    addControl(new multipeople_CheckBox(qsl("multiple (people)s")));

    auto* quality = new teTagComboBox({
        {qsl("(none)"),{}},
        {qsl("masterpiece,best quality"),{qsl("masterpiece"),qsl("best quality")}},
        {qsl("mst,bst,absurdres,very aesthetic"),{qsl("masterpiece"),qsl("best quality"),qsl("absurdres"),qsl("very aesthetic")}},
        {qsl("score_9,score_8_up"),{qsl("score_9"),qsl("score_8_up")}},
        {qsl("low quality,worst quality"),{qsl("low quality"),qsl("worst quality")}}
    },QStringLiteral("(quality)"));
    quality->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);
    addControl(quality);

    addControl(new teTagComboBox({
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
    },QStringLiteral("(framing)")));

    auto* facing = new teTagComboBox({
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
    facing->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);
    addControl(facing);

    addControl(new teTagComboBox({
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
    },QStringLiteral("(posture)")));

    addControl(new teTagComboBox({
        {qsl("(none)"),{}},
        {qsl("from above"),{qsl("from above")}},
        {qsl("from side"),{qsl("from side")}},
        {qsl("front view"),{qsl("front view")}},
        {qsl("from below"),{qsl("from below")}},
        {qsl("from behind"),{qsl("from behind")}},
        {qsl("mtu virus"),{qsl("mtu virus")}},
        {qsl("pantyshot"),{qsl("pantyshot")}}
    },QStringLiteral("(angle)")));

    addControl(new teTagCheckBox({ "multiple views" }));

    addControl(new teTagComboBox({
        {qsl("(none)"),{}},
        {qsl("front lighting"),{qsl("front lighting")}},
        {qsl("side lighting"),{qsl("side lighting")}},
        {qsl("backlighting"),{qsl("backlighting")}},
        {qsl("rim light"),{qsl("rim light")}},
        {qsl("dim lighting"),{qsl("dim lighting")}},
        {qsl("sunbeam"),{qsl("sunbeam")}}
    },QStringLiteral("(lighting)")));

    addControl(new teTagComboBox({
        {qsl("(none)"),{}},
        {qsl("scenery"),{qsl("scenery")}},
        {qsl("landscape"),{qsl("landscape")}},
        {qsl("cityscape"),{qsl("cityscape")}},
        {qsl("indoors"),{qsl("indoors")}},
        {qsl("outdoors"),{qsl("outdoors")}},
        {qsl("on bed"),{qsl("on bed"),qsl("indoors")}}
    },QStringLiteral("(scene)")));

    addControl(new teTagComboBox({
        {qsl("(none)"),{}},
        {qsl("comic"),{qsl("comic")}},
        {qsl("comic,greyscale,monochrome"),{qsl("comic"),qsl("greyscale"),qsl("monochrome")}},
        {qsl("game cg"),{qsl("game cg")}},
        {qsl("doujin cover"),{qsl("doujin cover"),qsl("cover page")}},
        {qsl("tachi-e"),{qsl("tachi-e")}},
        {qsl("pixel art"),{qsl("pixel art")}},
        {qsl("lineart"),{qsl("lineart")}},
        {qsl("dakimakura"),{qsl("dakimakura ( medium )")}},
    },QStringLiteral("(format)")));

    addRow({{},{},{new teTagCheckBox({ "blush" }),
                    new teTagCheckBox({ "nose blush" }),
                    new teTagCheckBox({ "tears" })}});

    addControl(new smile_buttongroup);
    addControl(new ClosedEyes_Buttongroup);

    addControl(new teTagComboBox({
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
    },QStringLiteral("(expression)")));

    addControl(new teTagComboBox({
        {qsl("empty eyes"),{qsl("empty eyes")}},
        {qsl("constricted pupils"),{qsl("constricted pupils")}},
        {qsl("wide-eyed"),{qsl("wide-eyed")}},
        {qsl("sparkling"),{qsl("sparkling")}},
        {qsl("one eye closed"),{qsl("one eye closed")}},
        {qsl("dashed eyes"),{qsl("dashed eyes")}},
        {qsl("squiggle eyes"),{qsl("squiggle eyes")}}
    },QStringLiteral("(eye)")));

    addControl(new teTagComboBox({
        {qsl("jitome"),{qsl("jitome")}},
        {qsl("tareme"),{qsl("tareme")}},
        {qsl("tsurime"),{qsl("tsurime")}},
        {qsl("sanpaku"),{qsl("sanpaku")}},
    },QStringLiteral("(eye shape)")));

    addControl(new mouth_buttongroup);
    addControl(new Breast_Buttongroup);
}

teEditor_hair_and_eyes::teEditor_hair_and_eyes(teTagListView*in_taglistwidget,QString &&name, QWidget *parent):teEditor_standard(in_taglistwidget,name, QStringLiteral("hair"), parent){
    addControl(new hair_and_eyes_color_list(
        new colorsWidget({{{qsl("gradient"),qsl("streaked")},true},{{qsl("hair"),qsl("eyes")},true}},nullptr),
        taglistwidget));

    addControl(new teTagCheckBox({ "heterochromia" }));

    auto* multicoloredEyes = new teTagCheckBox({ "eyes","multicolored eyes" });
    auto* multicoloredHair = new teTagCheckBox({ "hair","multicolored hair" });
    addRow({{},QStringLiteral("multicolored"),{multicoloredEyes,multicoloredHair}});

    addControl(new ponytail_buttongroup);
    addControl(new teTagCheckBox({ "two side up" }));

    addControl(new BangsList(
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
                         ),taglistwidget));

    addRow({{},{},{new teTagCheckBox({ "forehead" }),
                    new teTagCheckBox({ "ahoge" }),
                    new teTagCheckBox({ "hairband" }),
                    new teTagCheckBox({ "hood" })}});

    addControl(new HairLength_Buttongroup);
}


teEditor_clothes::teEditor_clothes(teTagListView*in_taglistwidget,QString &&name, QWidget *parent):teEditor_standard(in_taglistwidget,name, QStringLiteral("clothes"), parent){
    addControl(new ClothesList(new colorsWidget(
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
        { 0, 1, 1, 1, 1, 1, 1, 1 }),taglistwidget));

    addRow({{},{},{new teTagCheckBox({ "torn clothes" }),
                    new teTagCheckBox({ "fishnets" })}});
    addRow({{},{},{new teTagCheckBox({ "frills" }),
                    new teTagCheckBox({ "striped clothes" })}});

    addControl(new Ears_Buttongroup);
}


teEditor_nsfw::teEditor_nsfw(teTagListView*in_taglistwidget,QString &&name, QWidget *parent):teEditor_standard(in_taglistwidget,name, QStringLiteral("nsfw"), parent){
    addControl(new Object_list(new colorsWidget({{QStringList{object1.begin(),object1.end()},true},{QStringList{preposwords.begin(),preposwords.end()},true},{QStringList{bodyparts.begin(),bodyparts.end()},true}},nullptr,-1),taglistwidget));

    addRow({{qsl("type")},{},{new teTagCheckBox({ "sex" }),
                                new teTagCheckBox({ "nude" }),
                                new teTagCheckBox({ "vaginal" }),
                                new teTagCheckBox({ "anal" }),
                                new teTagCheckBox({ "oral" })}});

    addRow({{},{},{new teTagButtonGroup{{{qsl("aft"),qsl("after"),true},{qsl("imm"),qsl("imminent"),true},{qsl("vaginal"),qsl("vaginal"),false}}},
                    new teTagButtonGroup{{{qsl("aft"),qsl("after"),true},{qsl("imm"),qsl("imminent"),true},{qsl("rape"),qsl("rape"),false}}}}}); 

    addRow({{},{},{new teTagCheckBox({ "paizuri" }),
                    new teTagCheckBox({ "footjob" }),
                    new teTagCheckBox({ "fingering" }),
                    new teTagCheckBox({ "group","group sex" })}});

    addControl(new teTagCheckBox({ "cervical penetration" }));

    addRow({{},{},{new teTagCheckBox({"(female)", "female masturbation" }),
                    new teTagCheckBox({ "(male)", "male masturbation"  }),
                    new teTagCheckBox({ "masturbation" })}});

    addRow({{qsl("gender")},{},{new teTagCheckBox({qsl("futanari")}),
                                  new teTagCheckBox({qsl("tomgirl")}),
                                  new teTagCheckBox({qsl("otoko no ko")}),
                                  new teTagCheckBox({qsl("trap")})}});

    addRow({{qsl("liquid")},{},{new teTagCheckBox({ "cum inside" }),
                                  new teTagCheckBox({ "lactation" }),
                                  new teTagCheckBox({ "sweat" })}});

    addRow({{},{},{new teTagCheckBox({ "(female)", "female ejaculation" }),
                    new teTagCheckBox({ "ejaculation" })}});

    addRow({{qsl("expression")},{},{new teTagCheckBox({ "orgasm" }),
                                      new teTagCheckBox({ "face", "orgasm face" }),
                                      new teTagCheckBox({ "furrowed brow" })}});
    addRow({{},{},{new teTagCheckBox({ "torogao" }),
                    new teTagCheckBox({ "ahegao" }),
                    new teTagCheckBox({ "moaning" })}});

    addRow({{qsl("comic elements")},{},{new teTagCheckBox({ "trembling" }),
                                              new teTagCheckBox({ "spch bubble","speech bubble" }),
                                              new teTagCheckBox({ "sd effects" , "sound effects" })}});
    addRow({{},{},{new teTagCheckBox({ "text" }),
                    new teTagCheckBox({ "x-ray" }),
                    new teTagCheckBox({ "emphasis lines" })}});

    addRow({{},{},{new teTagCheckBox({ "internal cumshot" }),
                    new teTagCheckBox({ "cross-section" })}});

    addRow({{},{},{new teTagCheckBox({ "impregnation" }),
                    new teTagCheckBox({ "fertilization" })}});

    addRow({{qsl("physical traits")},{},{new teTagCheckBox({ "(female)", "female pubic hair" }),
                                             new teTagCheckBox({  "(male)","male pubic hair" }),
                                             new teTagCheckBox({ "pubic hair" })}});

    addRow({{},{},{new teTagCheckBox({ "large penis"  }),
                    new teTagCheckBox({ "large insertion"  })}});

    addRow({{},{},{new teTagCheckBox({ "smalldom" }),
                    new teTagCheckBox({ "size diff","size difference" }),
                    new teTagCheckBox({ "mounting" })}});

    addRow({{},{},{new teTagCheckBox({ "pregnant" }),
                    new teTagCheckBox({ "cum inflation" })}});

    addRow({{qsl("object")},{},{new teTagCheckBox({"ride","riding machine" }),
                                  new teTagCheckBox({"machine","sex machine" }),
                                  new teTagCheckBox({"toy", "sex toy"  }),
                                  new teTagCheckBox({"dildo"}),
                                  new teTagCheckBox({ "gap","gaping"  })}});

    addRow({{},{},{new teTagCheckBox({"vaginal","vaginal object insertion" }),
                    new teTagCheckBox({"anal", "anal object insertion"  }),
                    new teTagCheckBox({ "object insertion", "object insertion"  })}});

    addRow({{},{},{new teTagCheckBox({ "giving birth" }),
                    new teTagCheckBox({ "unbirth" }),
                    new teTagCheckBox({ "prolapse" })}});

    addRow({{},{},{new teTagCheckBox({ "bestiality" }),
                    new teTagCheckBox({ "🐶","dog" }),
                    new teTagCheckBox({ "🐴","horse" }),
                    new teTagCheckBox({ "🐷","pig" }),
                    new teTagCheckBox({ "🐸","frog" })}});
    addRow({{},{},{new teTagCheckBox({ "🐍","snake" }),
                    new teTagCheckBox({ "🦍","orangutan" }),
                    new teTagCheckBox({ "🐵","monkey" }),
                    new teTagCheckBox({ "🦋","insect" }),
                    new teTagCheckBox({ "🐛","worm" }),
                    new teTagCheckBox({ "💧","slime" }),
                    new teTagCheckBox({ "👺","goblin" })}});

    addRow({{},{},{new teTagCheckBox({"living clothes"}),
                    new teTagCheckBox({"bondage"}),
                    new teTagCheckBox({"tickling"})}});

    addRow({{},{},{new teTagCheckBox({"plant", "plant tentacles" }),
                    new teTagCheckBox({ "tentacles" }),
                    new teTagCheckBox({ "ovipositor" }),
                    new teTagCheckBox({ "egg" })}});

    addRow({{qsl("sexual positions")},{},{new teTagCheckBox({ "cowgirl","cowgirl position" }),
                                              new teTagCheckBox({ "from behind","sex from behind" }),
                                              new teTagCheckBox({ "leg lock" })}});

    addRow({{},{},{new teTagCheckBox({ "doggystyle" }),
                    new teTagCheckBox({ "spooning" }),
                    new teTagCheckBox({ "piledriver","piledriver (sex)" })}});

    addRow({{},{},{new legUp_Buttongroup,
                    new teTagCheckBox({ "spread" ,qsl("spread legs")}),
                    new teTagCheckBox({ "folded" ,qsl("folded")})}});

    addRow({{qsl("misc")},{},{new teTagCheckBox({ "toddlercon" }),
                              new teTagCheckBox({ "diaper" })}});
    addRow({{},{},{new teTagCheckBox({"fisting"}),
                    new teTagCheckBox({"omorashi"}),
                    new teTagCheckBox({"peeing"}),
                    new teTagCheckBox({"fart"})}});
    addRow({{},{},{new teTagCheckBox({"vore"}),
                    new teTagCheckBox({"vomit"}),
                    new teTagCheckBox({"enema"}),
                    new teTagCheckBox({"scat"})}});

    addRow({{},{},{new teTagCheckBox({"guro"}),
                    new teTagCheckBox({"snuff"}),
                    new teTagCheckBox({"amputee"}),
                    new teTagCheckBox({"netorare"})}});

    addControl(new teTagComboBox({
        {qsl("(none)"),{}},
        {qsl("bar censor"),{qsl("bar censor"),qsl("censored")}},
        {qsl("blank censor"),{qsl("blank censor"),qsl("censored")}},
        {qsl("heart censor"),{qsl("heart censor"),qsl("censored")}},
        {qsl("blur censor"),{qsl("blur censor"),qsl("censored")}},
        {qsl("mosaic censoring"),{qsl("mosaic censoring"),qsl("censored")}},
        {qsl("light censor"),{qsl("light censor"),qsl("censored")}},
        {qsl("censored"),{qsl("censored")}}
    },QStringLiteral("(censor)")));
}
