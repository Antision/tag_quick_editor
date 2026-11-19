#include "teeditor_derive.h"
#include "teeditorcontrol.h"
// The controls live in these headers now; they used to be declared inside the
// editor constructors below, which made this file a 2400 line wall.
#include "secondarybutton.h"     // the drop-down buttons (SC-01..SC-11)
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
 * @brief The people editor: one page, character types in a menu.
 *
 * The page used to be two: a "(nobody)" button opened a sub-page with the three
 * counts and the character-type boxes, and every picture meant opening it and
 * coming back. That sub-page is gone - the types are a secondary menu that stays
 * out of the way, and the three counts are edit boxes with a number pad.
 *
 * The rows below are the whole page. Every control is created and handed to
 * addRow()/addRowTo(), which is what puts it on screen and registers it with the
 * editor - there is no second list to keep in step, and no `controls.push_back`
 * to forget (a forgotten one used to be a control that silently never saw a tag).
 */
teEditor_pretreat::teEditor_pretreat(teTagListView*in_taglistwidget,QString&& name, QWidget* parent) :teEditor_standard(in_taglistwidget,name, QStringLiteral("pretreat"), parent) {
    // One page, no sub page: the character types are a menu of their own and the
    // three counts sit where they are used. The old design put both behind a
    // "(nobody)" button that had to be opened and closed for every picture.
    auto* characterType = new SecondaryButton(
        QStringLiteral(":/secondarymenu/character_type_menu.ui"),QStringLiteral("(character type)"));
    characterType->setObjectName(qsl("character_type"));
    addControl(characterType);

    // The counts keep the line-edit control (typing "10" is still allowed); the
    // number pad is only a shortcut that appears under the box (NumberPad).
    auto* girls = new teTagLineedit_people(qsl("girl"),qsl("girl(s)"));
    auto* boys  = new teTagLineedit_people(qsl("boy"),qsl("boy(s)"));
    auto* others= new teTagLineedit_people(qsl("other"),qsl("other(s)"));
    addRow({{},{},{girls,boys,others}});

    // --- the rest of the page ------------------------------------------------
    auto* loli = new teTagCheckBox({"loli"});
    auto* shota = new teTagCheckBox({"shota"});
    auto* chibi = new teTagCheckBox({"chibi"});
    auto* solo = new teTagCheckBox({"solo"});
    addRow({{},{},{loli,shota,chibi,solo}});

    addControl(new Mature_Buttongroup);
    addControl(new multipeople_CheckBox(qsl("multiple (people)s")));

    // (quality) - the entries are buttons in res/secondarymenu/quality_menu.ui;
    // the ids here are those buttons' objectNames without the "btn_" prefix.
    // Two controls per row: a page of twelve full-width bars is a wall, and these
    // four pairs are what is read together (shape + layout, framing + angle,
    // facing + posture, light + place). `multiple views` keeps its own row under
    // framing/angle, as before.
    auto* quality = new SecondaryButton(QStringLiteral(":/secondarymenu/quality_menu.ui"),QStringLiteral("(quality)"));
    quality->setObjectName(qsl("quality"));
    auto* format = new SecondaryButton(QStringLiteral(":/secondarymenu/format_menu.ui"),QStringLiteral("(format)"));
    format->setObjectName(qsl("format"));
    addRow({{},{},{quality,format}});

    auto* framing = new SecondaryButton(QStringLiteral(":/secondarymenu/framing_menu.ui"),QStringLiteral("(framing)"));
    framing->setObjectName(qsl("framing"));
    auto* angle = new SecondaryButton(QStringLiteral(":/secondarymenu/angle_menu.ui"),QStringLiteral("(angle)"));
    angle->setObjectName(qsl("angle"));
    addRow({{},{},{framing,angle}});

    addControl(new teTagCheckBox({ "multiple views" }));

    auto* facing = new SecondaryButton(QStringLiteral(":/secondarymenu/facing_menu.ui"),QStringLiteral("(facing)"));
    facing->setObjectName(qsl("facing"));
    auto* posture = new SecondaryButton(QStringLiteral(":/secondarymenu/posture_menu.ui"),QStringLiteral("(posture)"));
    posture->setObjectName(qsl("posture"));
    addRow({{},{},{facing,posture}});

    auto* lighting = new SecondaryButton(QStringLiteral(":/secondarymenu/lighting_menu.ui"),QStringLiteral("(lighting)"));
    lighting->setObjectName(qsl("lighting"));
    auto* scene = new SecondaryButton(QStringLiteral(":/secondarymenu/scene_menu.ui"),QStringLiteral("(scene)"));
    scene->setObjectName(qsl("scene"));
    addRow({{},{},{lighting,scene}});

    // The face tags are one additive menu (misc/faces.txt): every entry is a
    // checkable button, so "smile" and "blush" coexist. Two caption lines,
    // because this is the control that captures the most at once.
    auto* faces = new SecondaryButton(
        QStringLiteral(":/secondarymenu/faces_menu.ui"),QStringLiteral("(face)"));
    faces->setObjectName(qsl("faces"));
    faces->setMaxCaptionLines(2);
    addControl(faces);

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

    addRow({{qsl("liquid")},{},{new teTagCheckBox({ "cum inside" }),
                                  new teTagCheckBox({ "lactation" }),
                                  new teTagCheckBox({ "sweat" })}});

    addRow({{},{},{new teTagCheckBox({ "(female)", "female ejaculation" }),
                    new teTagCheckBox({ "ejaculation" })}});

    // orgasm / moaning and the worked-up expressions live in the faces menu now
    // (`orgasm face` is not a danbooru tag - it is `orgasm`).

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

    // Sexual positions, in the five groups of misc/sexual positions.txt. The
    // entries are toggles: a parent position and its variant can both be on, the
    // document is explicit that no implication is implied.
    auto* positions = new SecondaryButton(
        QStringLiteral(":/secondarymenu/sexual_positions_menu.ui"),QStringLiteral("(position)"));
    positions->setObjectName(qsl("sexual_positions"));
    positions->setMaxCaptionLines(2);
    addControl(positions);

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

    auto* censor = new SecondaryButton(QStringLiteral(":/secondarymenu/censor_menu.ui"),QStringLiteral("(censor)"));
    censor->setObjectName(qsl("censor"));
    addControl(censor);
}
