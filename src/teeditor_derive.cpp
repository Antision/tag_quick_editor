#include "teeditor_derive.h"
#include "teeditorcontrol.h"
// The controls live in these headers now; they used to be declared inside the
// editor constructors below, which made this file a 2400 line wall.
#include "secondarybutton.h"     // the drop-down buttons (SC-01..SC-11)
#include "clothesbuttons.h"      // the 77 clothes buttons, generated from the spec
#include "clothesemoji.h"        // display-only emoji (never part of a tag)
#include "teclothescontrol.h"
#include "tefacecontrols.h"
#include "tebodycontrols.h"
#include "tehaircontrols.h"
#include "temisccontrols.h"
#include "tetaglistwidget.h"
#include "tesignalwidget.h"
#include <QCursor>
#include <QPointer>
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
QStringList editorNames{qsl("custom"),qsl("pretreat"),qsl("hair and eyes"),qsl("clothes"),qsl("nsfw"),qsl("decorations")};

QPair<int,int> is_color(const teTag& tag,int offset) {
    // The whole colour phrase, not only its first colour: "blue and white",
    // "white to blue", with a shade in front of any of them and optionally closed
    // by the pattern word of a multi-colour tag ("blue and white striped",
    // "white to blue gradient").
    //
    // Returning a one-word slot is what let a modifier be inserted *inside* the
    // phrase: "blue and white striped panties" + "frilled" became "blue frilled
    // and white striped panties", i.e. the colour phrase was torn in half and the
    // tag no longer read as the one the add widget wrote.
    const int size=tag.words.size();
    // The first colour may sit behind other words ("torn red dress"): the colour
    // part of a tag is not tied to its first word.
    int index=offset;
    while(index<size&&!all_colors.contains(*tag.words[index]))
        ++index;
    if(index>=size)
        return {0,0};
    int first=index;
    if(first>offset&&is_shade(*tag.words[first-1]))
        --first;
    int last=index;
    ++index;
    while(index<size&&(*tag.words[index]==qsl("and")||*tag.words[index]==qsl("to"))){
        int next=index+1;
        if(next<size&&is_shade(*tag.words[next]))
            ++next;
        if(next>=size||!all_colors.contains(*tag.words[next]))
            break;
        last=next;
        index=next+1;
    }
    // striped / gradient are the two words that say which multi-colour form this
    // is; they travel with the colours, not with the adjectives.
    if(last+1<size&&(*tag.words[last+1]==qsl("striped")||*tag.words[last+1]==qsl("gradient")))
        ++last;
    return {first,last-first+1};
}
QStringList extract_colors(std::shared_ptr<teTag> tag) {
    // Every colour word in the tag, in order - not only the leading run.
    // "yellow hair with blue streaks" has two colours (base + streak) and
    // "gradient from red to blue" starts with no colour at all; stopping at the
    // first non-colour word used to lose everything after "hair"/"from".
    QStringList colors;
    if(!tag)
        return colors;
    for(int i=0;i<tag->words.size();++i){
        const QString word=*tag->words[i];
        if(!all_colors.contains(word))
            continue;
        if(i>0&&is_shade(*tag->words[i-1]))
            colors << (*tag->words[i-1]+QStringLiteral(" ")+word);
        else
            colors << word;
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
teEditor_custom::teEditor_custom(TagListView*in_taglistwidget,QString&& name, QString* styleSheet, QWidget* parent):teEditor(in_taglistwidget,name, parent) {
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
teEditor_pretreat::teEditor_pretreat(TagListView*in_taglistwidget,QString&& name, QWidget* parent) :teEditor_standard(in_taglistwidget,name, QStringLiteral("pretreat"), parent) {
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

teEditor_hair_and_eyes::teEditor_hair_and_eyes(TagListView*in_taglistwidget,QString &&name, QWidget *parent):teEditor_standard(in_taglistwidget,name, QStringLiteral("hair"), parent){
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


namespace {
/**
 * @brief "🎩 hat" - what a clothes button or menu entry shows.
 *
 * The emoji is display text and nothing else: emoji_for_tag() is a display table,
 * a tag never carries it (§1.8).
 */
QString clothesLabel(const QString& tag)
{
    const QString symbol = emoji_for_tag(tag);
    return symbol.isEmpty() ? tag : symbol + QLatin1Char(' ') + tag;
}

/**
 * @brief Give `widget` a font family list that has an emoji face behind the text.
 *
 * The embedded Ubuntu face has no emoji glyphs; Windows resolves a missing glyph
 * per character, but only through the families the widget names.
 */
void applyEmojiFont(QWidget* widget)
{
    QFont font = widget->font();
    font.setFamilies({appFontFamily(), QStringLiteral("Segoe UI Emoji")});
    widget->setFont(font);
}

/**
 * @brief One `>>T` / `>>P` button: it opens res/secondarymenu/clothes_<id>_menu.ui.
 *
 * The tag builder is what keeps a multi-word type together ("school uniform" is
 * one word, §1.2), so a tag written from the menu reads back as the same type.
 */
SecondaryButton* clothesMenuButton(const QString& tag,char kind,QWidget* parent)
{
    auto* button = new SecondaryButton(clothesMenuPath(tag), clothesLabel(tag), parent);
    button->setObjectName(clothesMenuId(tag));
    button->setMaxCaptionLines(2);
    button->setTagBuilder(clothesTagFromText);
    // T writes its own tag when clicked (the menu's first entry is the same tag);
    // P has no tag of its own and only opens its menu, which is also the style
    // difference the sheet draws (solid vs dashed).
    button->setOwnTag(kind=='T' ? tag : QString());
    button->setDisplayName(clothesLabel(tag));
    applyEmojiFont(button);
    // The name is the point here: let the layout ask for the width the caption
    // actually needs (with the font the sheet paints) instead of the old
    // hand-measured +40, which was taken before that font was applied and left
    // long labels squeezed below their text.
    button->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    return button;
}

/// A `B` button: an ordinary toggle that writes its own word, caption with emoji.
QPushButton* clothesPlainButton(const QString& tag,QWidget* parent,QButtonGroup* group)
{
    auto* button = new QPushButton(clothesLabel(tag),parent);
    button->setCheckable(true);
    // The caption carries the emoji, the tag does not: sendString() reads this.
    button->setProperty("tag",tag);
    applyEmojiFont(button);
    group->addButton(button);
    return button;
}

/// How one body area is laid out: which column of the dialog it sits in and how
/// many buttons wide it is. Two areas share a column, which is what keeps the
/// 77 buttons on one screen instead of in a strip.
struct clothesAreaLayout {
    int row;        // 0 = upper row of the category grid, 1 = lower row
    int column;     // macro column (0..3); the colour column is outside this grid
    int columns;    // how many button columns the block itself needs
};
/**
 * @brief Hovering a reftag previews the menu of its type (§1.13).
 *
 * The add widget is not an editor control of the tag list in the button-group
 * sense: the *tag* is what the list captured, and the menu is only a way to
 * change which spelling of that type the tag uses. So the preview belongs to the
 * tag, not to the button: the cursor over `microskirt` in the list shows the
 * skirt menu right there, a click on the tag solidifies it, and dragging onto an
 * entry switches the spelling.
 *
 * A timer is enough: the tag widgets are created and destroyed by the list, so an
 * event filter would have to follow every one of them.
 */
class ClothesHoverPreview : public QObject {
public:
    ClothesHoverPreview(QWidget* page,const QVector<SecondaryButton*>& buttons,QObject* parent)
        :QObject(parent),m_page(page),m_buttons(buttons){
        m_timer.setInterval(120);
        connect(&m_timer,&QTimer::timeout,this,[this]{ poll(); });
        m_timer.start();
    }
protected:
    bool eventFilter(QObject* watched,QEvent* event) override{
        if(watched!=m_anchor)
            return QObject::eventFilter(watched,event);
        switch(event->type()){
        case QEvent::MouseButtonPress: {
            auto* mouse = static_cast<QMouseEvent*>(event);
            // Left is the list's own tag drag, so this gesture is the right button
            // here. Everything else is the SecondaryButton's own rule: a press
            // *materialises* the menu (no longer the dimmed click-through preview)
            // and the button stays down while the cursor walks onto an entry.
            if(mouse->button()!=Qt::RightButton)
                break;
            if(!m_shown){
                SecondaryButton* button = nullptr;
                teTagWidgetBase* tagWidget = nullptr;
                if(!resolve(m_anchor,button,tagWidget))
                    break;
                m_shown = button;
                // The tag is part of the anchor: an entry picked from the menu
                // rewrites *this* tag's type (`red collar` -> `red sailor
                // collar`), it does not write the button's capture list.
                m_shown->showPreviewAt(QRect(m_anchor->mapToGlobal(QPoint(0,0)),m_anchor->size()),
                                       tagWidget?tagWidget->core:nullptr);
            }
            // Undecided, exactly like a SecondaryButton's own right press: a
            // drag materialises the hover menu, a click opens the tag operations
            // (edit / delete / ... and later "merge"). Pinning on the press made
            // the second impossible, and a tag whose type has a menu could then
            // never be merged by hand.
            m_pending = m_shown;
            m_pendingPos = mouse->globalPosition().toPoint();
            m_shown = nullptr;
            m_rightUsed = true;
            // Swallowing the press costs the implicit grab, and without it the
            // moves that matter happen with the cursor already over the menu. The
            // tag keeps the mouse until the release, exactly like the button does.
            m_anchor->grabMouse();
            return true;
        }
        case QEvent::MouseMove:
            if(m_pending){
                auto* mouse = static_cast<QMouseEvent*>(event);
                if((mouse->buttons() & Qt::RightButton)
                    &&(mouse->globalPosition().toPoint()-m_pendingPos).manhattanLength()
                          >= QApplication::startDragDistance()){
                    // The press turned into a drag: that is the menu gesture.
                    m_pinned = m_pending;
                    m_pending = nullptr;
                    m_pinned->pinShownMenu();   // solid + interactive, like holding the button
                    m_dragging = true;
                }
                return true;
            }
            if(m_dragging&&m_pinned&&m_pinned->menu()){
                m_pinned->menu()->setHoverIndex(
                    m_pinned->menu()->itemAt(static_cast<QMouseEvent*>(event)->globalPosition().toPoint()));
                return true;
            }
            break;
        case QEvent::MouseButtonRelease: {
            auto* mouse = static_cast<QMouseEvent*>(event);
            if(mouse->button()!=Qt::RightButton)
                break;
            if(m_pending){
                // A right click that did not move: the tag's own operations.
                teTagWidgetBase* tag = m_anchor;
                m_pending = nullptr;
                if(m_anchor)
                    m_anchor->releaseMouse();
                dismiss();
                openTagMenu(tag);
                m_rightUsed = false;
                return true;
            }
            if(m_dragging){
                m_dragging = false;
                if(m_anchor)
                    m_anchor->releaseMouse();
                if(m_pinned&&m_pinned->menu()){
                    SecondaryMenu* menu = m_pinned->menu();
                    const int index = menu->itemAt(mouse->globalPosition().toPoint());
                    menu->setHoverIndex(-1);
                    if(index>=0){
                        const SecondaryAction* action = menu->action(menu->itemId(index));
                        if(action){
                            m_rightUsed = false;
                            m_pinned->activateAction(*action,false,false);
                            m_pinned->dismissMenu();
                            m_pinned = nullptr;
                            dropAnchor();
                            return true;
                        }
                    }
                }
                // Released outside every entry: the menu was materialised by the
                // press, so it simply stays up and can be clicked normally.
                m_rightUsed = false;
                if(m_pinned)
                    m_pinned = nullptr;         // it is pinned now, not ours
                dropAnchor();
                return true;
            }
            if(m_rightUsed){
                m_rightUsed = false;
                return true;
            }
            if(m_shown){
                // A left click on the tag is the "solidify" gesture: the preview
                // becomes interactive and the click stops here (it must not start a
                // word edit).
                m_shown->pinShownMenu();
                m_shown = nullptr;
                dropAnchor();
                return true;
            }
            break;
        }
        case QEvent::Leave:
        case QEvent::Hide:
            // Leaving the tag is how the drag reaches the menu (they are one row
            // apart); only a hover that is not being used closes here.
            if(!m_dragging&&!m_pending)
                dismiss();
            break;
        default:
            break;
        }
        return QObject::eventFilter(watched,event);
    }
private:
    /// The menu button that stands for `under`'s tag, and the tag widget itself.
    bool resolve(QWidget* under,SecondaryButton*& button,teTagWidgetBase*& tagWidget){
        button = nullptr;
        tagWidget = nullptr;
        for(QWidget* widget = under;widget;widget = widget->parentWidget())
            if(auto* candidate = qobject_cast<teTagWidgetBase*>(widget)){
                tagWidget = candidate;
                break;
            }
        if(!tagWidget||!tagWidget->core)
            return false;
        // The same family rule the add widget uses: `wide sleeves` belongs to the
        // pure `sleeves` button, `sailor collar` to `collar`, and the longest
        // matching spelling wins.
        const QString text = static_cast<QString>(*tagWidget->core);
        QString best;
        for(SecondaryButton* candidate : m_buttons){
            const QString match = candidate->paletteTagFor(text);
            if(match.size() > best.size()){
                best = match;
                button = candidate;
            }
        }
        return !best.isEmpty();
    }
    /// The tag list `tag` lives in (the editor's ref list); nullptr for a tag that
    /// is not in one.
    static teTagListWidgetBase* listOf(QWidget* tag){
        for(QWidget* widget = tag;widget;widget = widget->parentWidget())
            if(auto* list = qobject_cast<teTagListWidgetBase*>(widget))
                return list;
        return nullptr;
    }
    void poll(){
        if(!m_page->isVisible()){
            dismiss();
            return;
        }
        // A press owns the interaction until it is released. The menu hangs under
        // the tag and routinely reaches past the editor page's own rect (the page
        // ends a couple of rows below the ref list), so testing the page here used
        // to end the drag as soon as the cursor walked onto the lower rows: the
        // tag was never rewritten, and the menu - already pinned by the drag -
        // stayed up and swallowed clicks ("the menu locked itself").
        // The tag can also be recycled under the press (the list was rebuilt); that
        // gesture can never finish, and keeping it would freeze the preview for the
        // rest of the session.
        if((m_pending||m_dragging)&&!m_anchor)
            dismiss();
        if(m_pending||m_dragging)
            return;
        const QPoint global = QCursor::pos();
        if(!m_page->rect().contains(m_page->mapFromGlobal(global))){
            dismiss();
            return;
        }
        if(m_shown&&!m_shown->previewVisible())
            m_shown=nullptr;                    // pinned: it is the user's now
        SecondaryButton* button = nullptr;
        teTagWidgetBase* tagWidget = nullptr;
        if(!resolve(QApplication::widgetAt(global),button,tagWidget))
            return;
        teTagListWidgetBase* owner = listOf(tagWidget);
        const bool selected = owner && owner->isSelected(tagWidget);
        if(tagWidget==m_anchor){
            // The same tag: only "it just became selected" is news. Clicking a tag
            // is how it becomes the one whose menu may lift, and the cursor never
            // leaves it - while re-offering the menu on every poll would make it
            // pop back up right after the user closed it.
            if(!m_anchorShown&&selected&&button&&!m_pinned){
                m_anchorShown=true;
                m_shown=button;
                button->showPreviewAt(QRect(tagWidget->mapToGlobal(QPoint(0,0)),tagWidget->size()),
                                      tagWidget->core);
            }
            return;
        }
        if(!button){
            dismiss();
            return;
        }
        dismiss();
        // The gesture (a right press / drag, and the right-click menu) works on
        // any tag, so the filter is installed for whatever is under the cursor.
        // The *preview menu*, though, is only for the tag the user selected: a menu
        // popping up under every tag the cursor happens to cross is noise on the
        // way to the one being edited (user, 2026-11-24).
        m_anchor = tagWidget;
        m_anchor->installEventFilter(this);
        m_anchorShown = false;
        if(!selected)
            return;
        m_anchorShown = true;
        m_shown = button;
        button->showPreviewAt(QRect(tagWidget->mapToGlobal(QPoint(0,0)),tagWidget->size()),
                              tagWidget->core);
    }
    /// The tag list that owns `tag`: its context menu is the one every tag of
    /// this editor uses, so a click here behaves exactly like a click there.
    void openTagMenu(teTagWidgetBase* tag){
        if(!tag)
            return;
        for(QWidget* widget=tag->parentWidget();widget;widget=widget->parentWidget())
            if(auto* list=qobject_cast<teTagListWidgetBase*>(widget)){
                list->onTagRightButtonClicked(tag,QPoint(),Qt::NoModifier);
                return;
            }
    }
    void dropAnchor(){
        if(m_anchor)
            m_anchor->removeEventFilter(this);
        m_anchor = nullptr;
        m_anchorShown = false;
    }
    void dismiss(){
        m_pending = nullptr;
        if(m_dragging&&m_anchor){
            m_dragging = false;
            m_anchor->releaseMouse();
        }
        // A menu this gesture materialised belongs to the gesture: aborting it
        // (the tag went away, the page was hidden) must not leave that menu
        // pinned on screen with nothing behind it. A menu the *user* pinned was
        // handed over long ago - m_pinned is null by then.
        if(m_pinned){
            m_pinned->dismissMenu();
            m_pinned = nullptr;
        }
        if(m_shown&&m_shown->previewVisible())
            m_shown->dismissMenu();
        m_shown = nullptr;
        dropAnchor();
    }

    QWidget* m_page;
    QVector<SecondaryButton*> m_buttons;
    QTimer m_timer;
    SecondaryButton* m_shown = nullptr;
    /// The button whose menu the right press materialised (kept while dragging).
    SecondaryButton* m_pinned = nullptr;
    /// A right press that has not become a drag yet: the button whose menu it
    /// would show, and where the press happened.
    SecondaryButton* m_pending = nullptr;
    QPoint m_pendingPos;
    QPointer<teTagWidgetBase> m_anchor;   // switching the spelling destroys it
    /// The menu of the anchored tag has already been offered (or the tag was not
    /// the selected one when it was entered): a later poll must not offer it again.
    bool m_anchorShown = false;
    bool m_dragging = false;
    bool m_rightUsed = false;
};

constexpr clothesAreaLayout kClothesAreaLayout[] = {
    { 0, 0, 3 },   // head
    { 1, 0, 3 },   // neck - collar/choker/necktie/bowtie/neckerchief/scarf/necklace
    { 0, 1, 2 },   // upper body
    { 1, 1, 2 },   // lower body
    { 0, 2, 2 },   // legwear - thighhighs/kneehighs/socks/pantyhose/leggings/leg warmers
    { 1, 2, 2 },   // feet
    { 0, 3, 2 },   // full body / uniform / swimwear (+ the merged `clothes` button)
    { 1, 3, 2 },   // accessories
};
} // namespace


teEditor_clothes::teEditor_clothes(TagListView*in_taglistwidget,QString &&name, QWidget *parent):teEditor_standard(in_taglistwidget,name, QStringLiteral("clothes"), parent){
    QVector<clothesAddArea> areas = clothesAddAreas();
    // `clothes` (the traditional-clothes menu) is a one-button area in the
    // generated table. One button does not need a block of its own: it joins the
    // full-body block, which it fills exactly (11 + 1 = 12 = 2 x 6).
    for(int i=areas.size()-1;i>=0;--i){
        if(QString::fromUtf8(areas[i].heading)!=QStringLiteral("traditional"))
            continue;
        bool merged=false;
        for(clothesAddArea& target:areas)
            if(QString::fromUtf8(target.heading)==QStringLiteral("full body / uniform / swimwear")){
                target.buttons+=areas[i].buttons;
                merged=true;
                break;
            }
        if(merged)
            areas.removeAt(i);
    }
    // Split the neck accessories out of the generated `head` area. A scarf or a
    // collar is not headwear, and the eight resulting blocks fill the 4 x 2
    // category grid without an empty cell.
    {
        const QStringList neckTags{
            QStringLiteral("collar"),QStringLiteral("choker"),QStringLiteral("necktie"),
            QStringLiteral("bowtie"),QStringLiteral("neckerchief"),QStringLiteral("scarf"),
            QStringLiteral("necklace")};
        for(int i=0;i<areas.size();++i){
            if(QString::fromUtf8(areas[i].heading)!=QStringLiteral("head"))
                continue;
            clothesAddArea neck;
            neck.heading="neck";
            QVector<clothesAddButton> headButtons;
            for(const clothesAddButton& entry:areas[i].buttons)
                if(neckTags.contains(QString::fromUtf8(entry.tag)))
                    neck.buttons.push_back(entry);
                else
                    headButtons.push_back(entry);
            areas[i].buttons=headButtons;
            areas.insert(i+1,neck);
            break;
        }
    }

    // Every type button shares one exclusive group, so a clothes tag always keeps
    // exactly one type word. The constructor is handed only the colour column (the
    // shades, the palette and the four feature toggles); the garment categories are
    // one grid to the right of it (see the block grid below).
    auto* typeGroup = new QButtonGroup(nullptr);
    // The colour column also carries the modifiers: what a garment *is* (colour,
    // state, trim) is one composed tag, and these are the words that go in front
    // of the type. They are all additive, so two trims can be on together. Only
    // words that really combine are here - `naked apron`, `see-through dress`,
    // `fur-trimmed coat` - a noun tag of its own (`frills`, `tassel`) is not a
    // modifier and stays out of this column.
    auto* widget = new colorsWidget(
        {
            // The two pattern words sit next to each other (they are the two
            // triggers of the colour queue), the rest around them.
            {{qsl("torn"),qsl("frilled"),qsl("striped"),qsl("gradient"),qsl("fishnet")},false},
            {{qsl("naked"),qsl("open"),qsl("unworn")},false},
            {{qsl("see-through"),qsl("wet"),qsl("backless")},false},
            {{qsl("crotchless"),qsl("double-breasted")},false},
            {{qsl("fur-trimmed"),qsl("lace-trimmed")},false},
            {{qsl("ribbon-trimmed"),qsl("gold-trimmed")},false},
        },
        nullptr, 0, {},
        { QString{}, QString{}, QString{}, QString{}, QString{}, QString{} },
        typeGroup,
        false,
        { 0, 0, 0, 0, 0, 0 });
    // "striped" and "gradient" describe a pattern of *several* colours, so they
    // turn the colour column into a queue: "blue and white striped panties",
    // "white to blue gradient pants".
    widget->setMultiColorTriggers({qsl("striped"),qsl("gradient")});

    // The type list reads tags before the buttons do, exactly as it did when it
    // was the only control here.
    auto* clothesList = new ClothesList(widget,taglistwidget);
    addControl(clothesList);

    // A choice made in one of the menus clears the plain type buttons that are
    // still ticked: they live in another button group, so nothing else would.
    const auto clearPlainTypes = [typeGroup]{
        for(QAbstractButton* button : typeGroup->buttons())
            button->setChecked(false);
    };

    // One grid for every garment category: the four macro columns hold
    // head/legwear, upper/lower body, feet/full body and accessories, and the two
    // macro rows start at the same height. A column of stacked sections used to
    // let each section follow its own height, so `legwear` started halfway down
    // while `lower body` started at the top.
    auto* blockGrid = new QGridLayout;
    blockGrid->setContentsMargins(0,0,0,0);
    blockGrid->setHorizontalSpacing(14);
    blockGrid->setVerticalSpacing(8);
    QVector<SecondaryButton*> menuButtons;
    for(int index=0;index<areas.size()&&index<int(std::size(kClothesAreaLayout));++index){
        const clothesAddArea& area = areas[index];
        const clothesAreaLayout& layout = kClothesAreaLayout[index];
        const int columns = layout.columns;
        const int rows = (area.buttons.size()+columns-1)/columns;
        auto* grid = new QGridLayout;
        grid->setContentsMargins(0,0,0,0);
        grid->setSpacing(1);
        int position = 0;
        for(const clothesAddButton& entry : area.buttons){
            const QString tag = QString::fromUtf8(entry.tag);
            QWidget* control = nullptr;
            if(entry.kind == 'B'){
                control = clothesPlainButton(tag,widget,typeGroup);
            }else{
                auto* button = clothesMenuButton(tag,entry.kind,widget);
                menuButtons.push_back(button);
                // A control of the page (it writes tags itself), not of the dialog.
                registerControl(button);
                button->teConnect(teCallbackType::edit,clearPlainTypes);
                control = button;
            }
            grid->addWidget(control,position%rows,position/rows);
            ++position;
        }
        // The heading and its grid stay together as one block, and the block is
        // pinned to the top of its grid cell: the row height is the tallest block
        // in it, and the others must not stretch their label away from the buttons.
        auto* block = new QWidget(widget);
        auto* blockLayout = new QVBoxLayout(block);
        blockLayout->setContentsMargins(0,0,0,0);
        blockLayout->setSpacing(1);
        blockLayout->addWidget(makeSectionHeading(QString::fromUtf8(area.heading),block));
        blockLayout->addLayout(grid);
        blockGrid->addWidget(block,layout.row,layout.column,Qt::AlignTop);
    }
    // One built section for the whole category grid; addBuiltSection() resets the
    // spacing to its default single-pixel, so put the real gaps back afterwards.
    widget->addBuiltSection(blockGrid,typeGroup,QString(),1);
    blockGrid->setHorizontalSpacing(14);
    blockGrid->setVerticalSpacing(8);
    // A little air between the colour/modifier column and the garment categories:
    // with only a 1px layout spacing the two groups read as one wide grid. A plain
    // full-height rule (makeColumnSeparator() draws an 18px tick, right for a
    // toolbar but not for a column that is 600px tall).
    if(auto* box=qobject_cast<QHBoxLayout*>(widget->content_layout)){
        auto* host=new QWidget(widget);
        host->setFixedWidth(9);
        auto* sepLayout=new QHBoxLayout(host);
        sepLayout->setContentsMargins(4,4,4,4);
        sepLayout->setSpacing(0);
        auto* line=new QFrame(host);
        line->setFixedWidth(1);
        line->setStyleSheet(QStringLiteral("QFrame{background:#4c399e;border:none;}"));
        sepLayout->addWidget(line);
        box->insertWidget(1,host);
    }
    // Both columns keep their content at the top. The colour column is taller than
    // the category grid; without the stretch its spare height would be spread
    // between the modifier sections.
    for(QVBoxLayout* column : widget->columns)
        column->addStretch(1);
    // The add widget edits one tag at a time: its type buttons light for that tag
    // and for nothing else (a "skirt" somewhere else in the list is irrelevant).
    // They also *select* the type instead of writing the tag list: `+` is what
    // turns the colour / modifier / type composition into a tag.
    for(SecondaryButton* button : menuButtons){
        button->setPaletteMode(true);
        button->setComposeSink([widget](const QString& tag,bool toggle){
            widget->chooseComposedType(tag,toggle);
        });
        // §1.13: a menu opened from a reftag edits that tag's type. The rule
        // lives in the list (it owns "what is this tag's type" and how a tag is
        // rewritten); the button only knows the gesture and which spelling its
        // menu stands for.
        button->setTagRewrite([clothesList](std::shared_ptr<teTag> tag,const QString& family,
                                            const QStringList& tags){
            return clothesList->retag(tag,family,tags);
        });
    }
    connect(widget,&colorsWidget::editingTagChanged,this,[menuButtons](const QString& tagText){
        for(SecondaryButton* button : menuButtons)
            button->setCaptureFromTag(tagText);
    });
    // §1.13: hovering a tag in the list previews the menu of its type right there.
    new ClothesHoverPreview(this,menuButtons,this);

    addRow({{},{},{new teTagCheckBox({ "torn clothes" }),
                    new teTagCheckBox({ "fishnets" })}});
    addRow({{},{},{new teTagCheckBox({ "frills" }),
                    new teTagCheckBox({ "striped clothes" })}});

    // Animal ears are an accessory type now (`ears` + its menu, in the add
    // widget): the old Ears_Buttongroup captured "X ears" a second time, and
    // two controls claiming the same tag drew two links for one tag.
}


namespace {

/**
 * @brief The generic `object insertion`: captured, never offered.
 *
 * `object insertion` is a danbooru tag the software does not recommend, but old
 * pictures still contain it. The button therefore claims it (so it can be seen
 * and switched off through the button) while its menu only offers the specific
 * spellings. Any of those - or an insertion site the editor does recommend
 * (`vaginal`, `anal`, `large insertion`) - retires the generic tag, whichever of
 * the two is read first.
 */
struct ObjectInsertionButton : public SecondaryButton
{
    ObjectInsertionButton()
        : SecondaryButton(QStringLiteral(":/secondarymenu/object_insertion_menu.ui"),
                          QStringLiteral("(object insertion)"))
    {
        setObjectName(qsl("object_insertion"));
        setMaxCaptionLines(2);
        captureList.insert(qsl("object insertion"));
    }

    TagHandling read(std::shared_ptr<teTag> tag) override
    {
        if (!tag)
            return TagHandling::NotMine;
        if (!tag->retired) {
            const QString text = static_cast<QString>(*tag);
            if (text == qsl("object insertion")) {
                // Read before the specific tags: ask the whole list, not only
                // what has been claimed so far (the scan is top to bottom).
                if (showingListHasTrigger()) {
                    if (taglistwidget)
                        taglistwidget->retire(tag);
                    return TagHandling::Consumed;
                }
            } else if (triggers().contains(text)) {
                retireGeneric();
            }
        }
        return EditorControl::read(tag);
    }

private:
    static const QSet<QString>& triggers()
    {
        static const QSet<QString> set{
            qsl("vaginal object insertion"), qsl("anal object insertion"),
            qsl("nipple object insertion"), qsl("urethral insertion"),
            qsl("vaginal"), qsl("anal"), qsl("large insertion")};
        return set;
    }

    bool showingListHasTrigger() const
    {
        if (!taglistwidget || !taglistwidget->showing_list)
            return false;
        for (const std::shared_ptr<teTag>& tag : taglistwidget->showing_list->getTags())
            if (tag && !tag->retired && triggers().contains(static_cast<QString>(*tag)))
                return true;
        return false;
    }

    void retireGeneric()
    {
        if (!taglistwidget)
            return;
        // The claim is a set; retiring a tag erases the claim it holds, so work
        // on a copy.
        const std::set<std::shared_ptr<teTag>> linked = linked_tags;
        for (const std::shared_ptr<teTag>& tag : linked)
            if (tag && !tag->retired && static_cast<QString>(*tag) == qsl("object insertion")) {
                taglistwidget->retire(tag);
                return;
            }
    }
};

} // namespace

teEditor_nsfw::teEditor_nsfw(TagListView*in_taglistwidget,QString &&name, QWidget *parent):teEditor_standard(in_taglistwidget,name, QStringLiteral("nsfw"), parent){
    addControl(new Object_list(new colorsWidget({{QStringList{object1.begin(),object1.end()},true},{QStringList{preposwords.begin(),preposwords.end()},true},{QStringList{bodyparts.begin(),bodyparts.end()},true}},nullptr,-1),taglistwidget));

    // Body parts: one button, three regions, and every part with variants opens
    // one more level (region -> part -> variant, four in all).
    auto* bodyParts = new SecondaryButton(
        QStringLiteral(":/secondarymenu/body_parts_menu.ui"), qsl("(body part)"));
    bodyParts->setObjectName(qsl("body_parts"));
    bodyParts->setMaxCaptionLines(2);
    addControl(bodyParts);

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

    addRow({{},{},{new teTagCheckBox({ "smalldom" }),
                    new teTagCheckBox({ "size diff","size difference" }),
                    new teTagCheckBox({ "mounting" })}});

    addRow({{},{},{new teTagCheckBox({ "pregnant" }),
                    new teTagCheckBox({ "cum inflation" })}});

    addRow({{qsl("object")},{},{new teTagCheckBox({"ride","riding machine" }),
                                  new teTagCheckBox({"machine","sex machine" }),
                                  new teTagCheckBox({"toy", "sex toy"  }),
                                  new teTagCheckBox({ "gap","gaping"  })}});

    // The two insertion buttons share a row: the generic one (capture only) on
    // the left, the objects that can actually be added on the right.
    addRow({{},{},{new ObjectInsertionButton,
                    [] {
                        auto* objects = new SecondaryButton(
                            QStringLiteral(":/secondarymenu/objects_menu.ui"),
                            qsl("(objects)"));
                        objects->setObjectName(qsl("objects"));
                        objects->setMaxCaptionLines(2);
                        return objects;
                    }()}});

    addRow({{},{},{new teTagCheckBox({ "giving birth" }),
                    new teTagCheckBox({ "unbirth" }),
                    new teTagCheckBox({ "prolapse" })}});

    // Bestiality is one button whose menu holds the animals (and zoophilia):
    // a click writes `bestiality` itself, a menu pick adds the animal word.
    {
        auto* bestiality = new SecondaryButton(
            QStringLiteral(":/secondarymenu/bestiality_menu.ui"), qsl("bestiality"));
        bestiality->setObjectName(qsl("bestiality"));
        bestiality->setMaxCaptionLines(2);
        bestiality->setOwnTag(qsl("bestiality"));
        addControl(bestiality);
    }

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
