#include "secondarybutton.h"

#include "clothesemoji.h"
#include "tetag.h"
#include "tetaglistwidget.h"

#include <QApplication>
#include <QClipboard>
#include <QCursor>
#include <QKeyEvent>
#include <QMenu>
#include <QStyle>
#include <QStyleOptionButton>

#include <algorithm>

namespace {
/// The only button that can be Pinned. Pinning a second one closes the first:
/// two pinned menus at once is not a state the requirements allow (SC-02).
SecondaryButton* sPinned = nullptr;

constexpr int kTextInset = 7;
constexpr int kVerticalPadding = 3;
constexpr int kMinHeight = 24;

/// What the caption shows for one captured tag.
///
/// The caption is painted by hand from the captured tags, so it needs the emoji
/// on this path as well - otherwise picking an entry from a menu would make the
/// emoji disappear from the button (§1.8). The emoji is display text only: the
/// tag in the list never carries it.
QString captionText(const QString& tag)
{
    const QString symbol = emoji_for_tag(tag);
    return symbol.isEmpty() ? tag : symbol + QLatin1Char(' ') + tag;
}

/// Reads one menu file plus the third-level files it declares into `out`.
void readMenuActions(const QString& path, QVector<SecondaryAction>& out)
{
    if (path.isEmpty())
        return;
    out = SecondaryMenu::readActions(path);
    QVector<SecondaryAction> nested;
    for (const SecondaryAction& action : out) {
        if (action.submenuPath.isEmpty())
            continue;
        for (const SecondaryAction& child : SecondaryMenu::readActions(action.submenuPath))
            nested.push_back(child);
    }
    out += nested;
}
} // namespace

SecondaryButton::SecondaryButton(const QString& menuPath, QString placeholder, QWidget* parent)
    : QPushButton(parent)
    , m_menuPath(menuPath)
    , m_placeholder(std::move(placeholder))
    , m_caption(m_placeholder)
{
    // The object name is left to the caller: it *is* the control id SC-10 names
    // the menu file after ("quality" -> res/secondarymenu/quality_menu.ui). The
    // sheet addresses the class, not the name, so nothing breaks without it.
    setMinimumHeight(kMinHeight);
    // Generic launchers take whatever width their row gives them and elide the
    // caption. The clothes palette overrides this with Minimum: there the name is
    // the point (see clothesMenuButton).
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    setFocusPolicy(Qt::NoFocus);
    setCursor(Qt::ArrowCursor);
    QPushButton::setText(m_caption);
    setProperty("captured", false);
    loadActions();
}

SecondaryButton::~SecondaryButton()
{
    hideMenu();
    if (sPinned == this)
        sPinned = nullptr;
    delete m_submenu;
    delete m_rightMenu;
    delete m_menu;
}

void SecondaryButton::loadActions()
{
    // What the menu can do has to be known before the menu is ever opened: the
    // capture list is what filter() answers with while a tag list is being read,
    // and that happens long before a popup is shown. Reading the .ui twice (once
    // here, once when it becomes a window) is a few hundred microseconds at
    // startup and keeps the file the only source of truth.
    readMenuActions(m_menuPath, m_actions);
    readMenuActions(m_rightMenuPath, m_rightActions);
    m_familyDirty = true;

    // filter() answers for both menus: a tag of the second one is just as much
    // this control's business as a tag of the first.
    captureList.clear();
    for (const Action& action : m_actions)
        for (const QString& tag : action.tags)
            captureList.insert(tag);
    for (const Action& action : m_rightActions)
        for (const QString& tag : action.tags)
            captureList.insert(tag);
}

void SecondaryButton::setRightMenuPath(const QString& path)
{
    if (m_rightMenuPath == path)
        return;
    m_rightMenuPath = path;
    // A menu that is already on screen belongs to the old file; the next press
    // builds the new one.
    if (m_rightMenu)
        hideMenu();
    delete m_rightMenu;
    m_rightMenu = nullptr;
    loadActions();
}

const QVector<SecondaryButton::Action>& SecondaryButton::activeActions() const
{
    if (m_rightMenu && m_active == m_rightMenu)
        return m_rightActions;
    return m_actions;
}

bool SecondaryButton::activeMenuHasSubmenu() const
{
    for (const Action& action : activeActions())
        if (!action.submenuPath.isEmpty())
            return true;
    return false;
}

void SecondaryButton::setMaxCaptionLines(int lines)
{
    const int clamped = qMax(1, lines);
    if (m_maxLines == clamped)
        return;
    m_maxLines = clamped;
    m_layoutWidth = -1;
    updateGeometry();
    update();
}

void SecondaryButton::setTagBuilder(std::function<std::shared_ptr<teTag>(const QString&)> builder)
{
    m_tagBuilder = std::move(builder);
    // The capture only ever compares tag *text*, so nothing has to be reloaded:
    // the builder is used when an entry is written, from then on.
}

std::shared_ptr<teTag> SecondaryButton::makeTag(const QString& text) const
{
    return m_tagBuilder ? m_tagBuilder(text) : std::make_shared<teTag>(text);
}

void SecondaryButton::setOwnTag(const QString& tag)
{
    m_ownTag = tag;
    m_familyDirty = true;
    setKind(tag.isEmpty() ? QStringLiteral("pure") : QStringLiteral("activatable"));
}

void SecondaryButton::showPreviewAt(const QRect& anchor)
{
    // A menu anchored at a *tag* edits that tag in the list: the palette is not
    // what this gesture means, even though the same button paints it.
    m_compose = false;
    SecondaryMenu* menu = ensureMenu(false);
    if (!menu)
        return;
    if (m_active && m_active != menu)
        hideMenu();
    m_active = menu;
    menu->setPreview(true);
    menu->showAt(anchor);
    m_mode = Mode::Preview;
    syncMenuSelection();
}

void SecondaryButton::pinShownMenu()
{
    if (m_mode != Mode::Preview)
        return;
    pinActive();
}

void SecondaryButton::setPaletteMode(bool on)
{
    m_palette = on;
    if (on)
        setCaptured(false);
    m_compose = m_palette && bool(m_composeSink);
}

void SecondaryButton::setCaptureFromTag(const QString& tagText)
{
    m_contextTag = tagText;
    // The add widget has to light the button that owns the tag's *type*, not only
    // the button whose name happens to be in the tag: `red pleated skirt` is the
    // skirt button, `sailor collar` the collar one. paletteTagFor() answers with
    // the family spelling, which is also what the menu highlights (so the button
    // and its menu entry can never disagree).
    if (m_palette)
        m_paletteTag = paletteTagFor(tagText);
    bool on = false;
    if (!tagText.isEmpty()) {
        if (m_palette)
            on = !m_paletteTag.isEmpty();
        else {
            // Non-palette callers (older dialogs) keep the exact-tag rule.
            on = captureList.contains(tagText);
            if (!on && !m_ownTag.isEmpty())
                on = tagText == m_ownTag || tagText.endsWith(QLatin1Char(' ') + m_ownTag);
        }
    }
    setCaptured(on);
    // The label follows the selection: `gloves` shows `elbow gloves` while that
    // spelling is the one being composed.
    if (m_palette)
        rebuildCaption();
    // The menu follows the same source; a pinned menu used to keep its old
    // highlight until it was reopened.
    if (m_active || m_submenu)
        syncMenuSelection();
}

const QStringList& SecondaryButton::familyTags() const
{
    if (!m_familyDirty)
        return m_familyTags;
    m_familyTags.clear();
    const auto add = [this](const QString& tag) {
        if (!tag.isEmpty() && !m_familyTags.contains(tag))
            m_familyTags.push_back(tag);
    };
    add(m_ownTag);
    for (const Action& action : m_actions)
        for (const QString& tag : action.tags)
            add(tag);
    for (const Action& action : m_rightActions)
        for (const QString& tag : action.tags)
            add(tag);
    // Longest first: paletteTagFor() returns the first suffix that matches, and
    // `pleated skirt` has to win over `skirt`.
    std::stable_sort(m_familyTags.begin(), m_familyTags.end(),
                     [](const QString& left, const QString& right) {
                         return left.size() > right.size();
                     });
    m_familyDirty = false;
    return m_familyTags;
}

QString SecondaryButton::paletteTagFor(const QString& tagText) const
{
    if (tagText.isEmpty())
        return QString();
    for (const QString& candidate : familyTags())
        if (tagText == candidate || tagText.endsWith(QLatin1Char(' ') + candidate))
            return candidate;
    return QString();
}

void SecondaryButton::setComposeSink(std::function<void(const QString& tag, bool toggle)> sink)
{
    m_composeSink = std::move(sink);
    // The popup's buttons compose; a menu opened by a reftag's hover preview has
    // to keep editing the tag list, and showPreviewAt() turns m_compose back off
    // for exactly that gesture.
    m_compose = m_palette && bool(m_composeSink);
}

bool SecondaryButton::previewVisible() const
{
    return m_mode == Mode::Preview;
}

void SecondaryButton::dismissMenu()
{
    if (m_mode == Mode::Hidden)
        return;
    hideMenu();
}

void SecondaryButton::setDisplayName(const QString& name)
{
    m_displayName = name;
    if (!name.isEmpty()) {
        m_caption = name;
        QPushButton::setText(name);
        m_layoutWidth = -1;
        updateGeometry();
        update();
    }
}

void SecondaryButton::setKind(const QString& kind)
{
    if (m_kind == kind)
        return;
    m_kind = kind;
    // Two styles, two behaviours: the sheet draws a solid button when a click
    // writes a tag and a dashed one when it only opens a menu.
    setProperty("kind", m_kind);
    style()->unpolish(this);
    style()->polish(this);
    update();
}

/**
 * @brief A click on the button itself: write or remove its own tag.
 *
 * The clothes dialog has no button group to fall back on - the add widget only
 * adds tags, and the tag list (not this control) is what captures them. So the
 * click has to *do* something: "leotard" adds `leotard`, a second click takes it
 * away, and the entry of the same name in the menu is the same action.
 */
void SecondaryButton::toggleOwnTag()
{
    if (m_ownTag.isEmpty() || !taglistwidget || !taglistwidget->showing_list)
        return;
    const std::set<std::shared_ptr<teTag>> linked = linked_tags;
    for (const std::shared_ptr<teTag>& tag : linked) {
        if (static_cast<QString>(*tag) == m_ownTag) {
            taglistwidget->retire(tag);
            return;
        }
    }
    taglistwidget->tagInsertAbove(false, makeTag(m_ownTag));
}

const CaptionLayout& SecondaryButton::captionLayout() const
{
    if (m_layoutWidth != width() || m_layoutCaption != m_caption) {
        m_layoutWidth = width();
        m_layoutCaption = m_caption;
        // A named button (every clothes type) and an empty generic launcher show
        // m_caption. Only an unnamed launcher with captured tags paints those tags
        // itself, because it has no name to protect and needs the per-tag emoji.
        // This is what used to make the add widget's `skirt` button read
        // "microskirt, pleated skirt" from the *tag list*: the caption was right,
        // but the painter ignored it.
        const QStringList tags = capturedTags();
        QStringList shown;
        if (m_displayName.isEmpty() && !tags.isEmpty()) {
            for (const QString& tag : tags)
                shown.push_back(captionText(tag));
        } else {
            shown.push_back(m_caption);
        }
        m_layout = layoutCaption(shown, fontMetrics(), width() - 2 * kTextInset, m_maxLines);
    }
    return m_layout;
}

QStringList SecondaryButton::capturedTags() const
{
    QStringList tags;
    for (const std::shared_ptr<teTag>& tag : linked_tags)
        tags.push_back(static_cast<QString>(*tag));
    // Stable order: the std::set iterates by pointer, which changes whenever the
    // tag list is rebuilt - a caption whose line breaks move on every image
    // switch looks like a bug.
    tags.sort();
    return tags;
}

SecondaryMenu* SecondaryButton::ensureMenu(bool right)
{
    SecondaryMenu*& slot = right ? m_rightMenu : m_menu;
    if (slot)
        return slot;
    const QString path = right ? m_rightMenuPath : m_menuPath;
    if (path.isEmpty())
        return nullptr;
    SecondaryMenu* menu = new SecondaryMenu(nullptr);
    if (!menu->loadUi(path)) {
        delete menu;
        return nullptr;
    }
    connect(menu, &SecondaryMenu::actionTriggered, this, [this](const QString& id, bool) {
        // A pinned menu stays up, so several entries can be used one after another;
        // an unpinned one (only reachable through the drag) closes with the entry.
        activate(id, m_mode == Mode::Pinned);
    });
    connect(menu, &SecondaryMenu::entryRightPressed, this, [this](const QString& id) {
        // Right-clicking an entry: a family parent shows its third level, an
        // additive entry toggles like a left click would (the pin stays up).
        const Action* action = findAction(id);
        if (!action)
            return;
        if (!action->submenuPath.isEmpty())
            openSubmenu(*action);
        else
            activate(id, true);
    });
    connect(menu, &SecondaryMenu::dismissRequested, this, [this] { hideMenu(); });
    connect(menu, &SecondaryMenu::blankAreaPressed, this, [this] { hideSubmenu(); });
    slot = menu;
    return menu;
}

SecondaryMenu* SecondaryButton::ensureSubmenu(const Action& action)
{
    if (action.submenuPath.isEmpty())
        return nullptr;
    if (m_submenu && m_submenu->uiPath() == action.submenuPath)
        return m_submenu;

    // A different third level than the one that was open: drop the old window
    // instead of leaving an unparented top-level widget behind.
    if (m_submenu) {
        m_submenu->dismiss();
        delete m_submenu;
        m_submenu = nullptr;
    }
    auto* menu = new SecondaryMenu(nullptr);
    if (!menu->loadUi(action.submenuPath)) {
        delete menu;
        return nullptr;
    }
    connect(menu, &SecondaryMenu::entryRightPressed, this, [this, menu](const QString& id) {
        m_rightClickUsed = true;
        if (const SecondaryAction* action = menu->action(id)) {
            activateAction(*action, true, m_mode == Mode::Pinned);
            hideMenu();                 // a third-level pick is a complete choice
        }
    });
    connect(menu, &SecondaryMenu::dismissRequested, this, [this] { hideMenu(); });
    connect(menu, &SecondaryMenu::blankAreaPressed, this, [this] { hideSubmenu(); });
    connect(menu, &SecondaryMenu::actionTriggered, this, [this, menu](const QString& id, bool) {
        // Several aliases may be on together, so this toggles like every other
        // additive entry. A third-level pick finishes the gesture: the tag is
        // changed, there is nothing left to do in this menu.
        if (const SecondaryAction* action = menu->action(id)) {
            activateAction(*action, true, m_mode == Mode::Pinned);
            hideMenu();
        }
    });
    m_submenu = menu;
    return menu;
}

bool SecondaryButton::openPreview(bool right)
{
    SecondaryMenu* menu = ensureMenu(right);
    if (!menu)
        return false;
    // A menu opened by pressing this button belongs to the add widget: choosing
    // an entry selects it in the composition instead of writing the tag list.
    m_compose = m_palette && bool(m_composeSink);
    // Switching between the two menus mid-interaction: only one window at a time.
    if (m_active && m_active != menu)
        hideMenu();

    m_active = menu;
    menu->setPreview(true);
    menu->showAt(visualRect());
    m_mode = Mode::Preview;
    syncMenuSelection();
    return true;
}

bool SecondaryButton::openSubmenu(const Action& action)
{
    if (action.submenuPath.isEmpty())
        return false;
    SecondaryMenu* menu = ensureSubmenu(action);
    if (!menu)
        return false;

    QRect anchor = m_active ? m_active->itemRect(action.id) : QRect();
    if (anchor.isNull())
        anchor = visualRect();
    menu->setPreview(m_mode == Mode::Preview);
    menu->showAt(anchor);

    // Every entry stays listed, including the ones that are captured: the third
    // level is also how a captured alias is turned off again.
    syncMenuSelection();
    return true;
}

void SecondaryButton::pinActive()
{
    if (!m_active)
        return;
    pin();                  // closes the menu pinned by another button, if any
    m_active->setPreview(false);
    m_active->setHoverIndex(-1);
    if (m_submenu)
        m_submenu->setPreview(false);       // a pinned menu is usable, both levels
    m_mode = Mode::Pinned;
    // SC-02's "click anywhere else closes it": while pinned, every press in the
    // application passes through here first.
    qApp->installEventFilter(this);
    syncMenuSelection();
}

void SecondaryButton::pin()
{
    if (sPinned && sPinned != this) {
        SecondaryButton* previous = sPinned;
        sPinned = nullptr;              // hideMenu() clears it, keep the order safe
        previous->hideMenu();
    }
    sPinned = this;
}

void SecondaryButton::hideSubmenu()
{
    if (!m_submenu)
        return;
    m_submenu->dismiss();
    // deleteLater, not delete: this runs from the third level's own click handler
    // when an entry is used, and the window is the sender.
    m_submenu->deleteLater();
    m_submenu = nullptr;
}

void SecondaryButton::hideMenu()
{
    if (m_mode == Mode::Pinned) {
        qApp->removeEventFilter(this);
        if (sPinned == this)
            sPinned = nullptr;
    }
    hideSubmenu();
    if (m_active) {
        m_active->dismiss();
        m_active = nullptr;
    }
    m_mode = Mode::Hidden;
    m_dragging = false;
}

const SecondaryButton::Action* SecondaryButton::findAction(const QString& id) const
{
    // Both menus: an action id is unique across the whole button (the .ui files
    // are self-checked for duplicates per file, and a button never declares the
    // same id in both).
    for (const Action& action : m_actions)
        if (action.id == id)
            return &action;
    for (const Action& action : m_rightActions)
        if (action.id == id)
            return &action;
    return nullptr;
}

QStringList SecondaryButton::highlightedTags() const
{
    // Palette mode: the add widget is editing one tag, so that tag *is* the
    // capture. The button and the entries of its menu must agree - reading the tag
    // list for one and the edited tag for the other is exactly how "the menu entry
    // is green but the button is not" happened. The family spelling (not the whole
    // tag) is what an entry can match: `red pleated skirt` lights `pleated skirt`,
    // never `skirt`.
    if (m_palette)
        return m_paletteTag.isEmpty() ? QStringList{} : QStringList{m_paletteTag};
    return capturedTags();
}

bool SecondaryButton::entryCaptured(const Action& action) const
{
    const QStringList captured = highlightedTags();
    QStringList theirs = action.tags;
    theirs.sort();
    return !theirs.isEmpty()
           && std::includes(captured.begin(), captured.end(), theirs.begin(), theirs.end());
}

bool SecondaryButton::familyCaptured(const Action& parent) const
{
    for (const Action& member : m_actions)
        if (member.group == parent.group && member.tags != parent.tags && entryCaptured(member))
            return true;
    return entryCaptured(parent);
}

void SecondaryButton::setFamily(const Action& parent, bool on)
{
    if (!taglistwidget || !taglistwidget->showing_list)
        return;
    if (on) {
        for (const QString& text : parent.tags) {
            bool found = false;
            for (const std::shared_ptr<teTag>& tag : linked_tags)
                if (static_cast<QString>(*tag) == text) {
                    found = true;
                    break;
                }
            if (!found)
                taglistwidget->tagInsertAbove(false, makeTag(text));
        }
    } else {
        // The entry stands for the whole family: "off" means every captured member
        // goes, not just the one whose name the entry happens to show.
        QStringList members;
        for (const Action& member : m_actions)
            if (member.group == parent.group)
                members += member.tags;
        const std::set<std::shared_ptr<teTag>> linked = linked_tags;
        for (const std::shared_ptr<teTag>& tag : linked)
            if (members.contains(static_cast<QString>(*tag)))
                taglistwidget->retire(tag);
    }
    edited();
    refreshState();
}

void SecondaryButton::activateFamilyEntry(const Action& parent, bool keepOpen)
{
    // The family entry is its on/off switch *and* the way into the third level:
    // switching it on captures the parent tag and offers the other spellings,
    // switching it off releases the family and puts those away again.
    const bool wasOn = familyCaptured(parent);
    setFamily(parent, !wasOn);
    if (!wasOn) {
        if (m_active)
            openSubmenu(parent);
        syncMenuSelection();
        return;                     // the second level stays up to hold the third
    }
    hideSubmenu();
    if (keepOpen)
        syncMenuSelection();
    else
        hideMenu();
}

void SecondaryButton::toggleSubmenuFor(const Action& parent)
{
    if (parent.submenuPath.isEmpty())
        return;
    // Right click = "show me the other spellings", and a second right click puts
    // them away again.
    if (m_submenu && m_submenu->uiPath() == parent.submenuPath) {
        hideSubmenu();
        return;
    }
    openSubmenu(parent);
}

void SecondaryButton::activateAction(SecondaryAction action, bool fromSubmenu, bool keepOpen)
{
    // Pointing at another entry of the *second* level means the user is done with
    // the third one: it never survives a click somewhere else in its menu.
    if (!fromSubmenu)
        hideSubmenu();

    // The add widget composes one tag. An entry selected there must not insert
    // anything by itself (`sailor collar` used to land in the tag list the moment
    // it was clicked, with no colour or modifier chosen): it hands the spelling to
    // the palette, which writes it only when `+` is pressed.
    if (m_compose && m_composeSink) {
        if (!action.tags.isEmpty())
            m_composeSink(action.tags.join(QLatin1Char(' ')), false);
        // One garment has one type: the menu has done its job, whether it was a
        // hover preview, a drag or a pinned menu. Leaving a pinned menu up would
        // hide the colours and modifiers the user is about to pick.
        hideMenu();
        return;
    }

    if (!action.submenuPath.isEmpty()) {
        activateFamilyEntry(action, keepOpen);
        return;
    }
    if (!action.checkable) {
        // A plain entry has no state of its own: every click does the same thing
        // (replace the capture with its tags - or clear it, for "(none)").
        applyAction(action.id);
    } else if (entryCaptured(action)) {
        toggleAction(action.id, false);
    } else {
        toggleAction(action.id, true);
    }

    if (keepOpen)
        syncMenuSelection();
    else
        hideMenu();
}

void SecondaryButton::activate(const QString& id, bool keepOpen)
{
    const Action* action = findAction(id);
    if (action)
        activateAction(*action, false, keepOpen);
}

void SecondaryButton::syncMenuSelection()
{
    const QStringList captured = highlightedTags();
    const auto capturedAll = [&captured](const Action& action) {
        QStringList theirs = action.tags;
        theirs.sort();
        return std::includes(captured.begin(), captured.end(), theirs.begin(), theirs.end());
    };
    // Highlighting is per menu file: the two menus may declare different ids for
    // the same tag, and an id the shown file does not have must not be pushed at
    // it (it would silently do nothing, or worse, hit an unrelated entry name).
    const QVector<Action>& actions = activeActions();

    // Which entry stands for the capture: an additive (checkable) entry when all of
    // its tags are in it, a plain entry when its tag list is exactly the capture.
    QStringList ids;
    for (const Action& action : actions) {
        if (action.tags.isEmpty())
            continue;
        const bool stands = action.checkable ? capturedAll(action)
                                             : [&] {
                                                   QStringList theirs = action.tags;
                                                   theirs.sort();
                                                   return theirs == captured;
                                               }();
        if (stands)
            ids.push_back(action.id);
    }

    // The entry that opens a third level represents its whole family: it lights up
    // when any member is captured, and borrows that member's label. `trap` itself
    // wins whenever it is one of the captured tags, so "otoko no ko + trap" still
    // reads "trap" and trap stays the entry point.
    QVector<QPair<QString, QString>> labels;
    QStringList familyIds;
    for (const Action& parent : actions) {
        // Only an entry that opens a third level represents a family. A plain
        // entry (`skirt` in the skirt menu) lights up for its own tag and nothing
        // else: picking `microskirt` lights `microskirt` and the skirt *button*,
        // not the `skirt` entry - that one would claim a tag nobody wrote.
        if (parent.submenuPath.isEmpty() || parent.group.isEmpty())
            continue;
        const Action* firstLit = nullptr;
        bool selfLit = false;
        for (const Action& member : actions) {
            if (member.group != parent.group || member.tags.isEmpty())
                continue;
            if (!capturedAll(member))
                continue;
            if (!firstLit)
                firstLit = &member;
            if (member.id == parent.id)
                selfLit = true;
        }
        const QString ownLabel = parent.label.isEmpty() ? parent.id : parent.label;
        const Action* named = selfLit ? &parent : firstLit;
        if (!named) {
            // Nothing of the family is captured: put the declared label back (the
            // entry still reads "otoko no ko" from a moment ago otherwise).
            labels.push_back({parent.id, ownLabel});
            continue;
        }
        familyIds.push_back(parent.id);
        const QString label = named->label.isEmpty() ? named->id : named->label;
        labels.push_back({parent.id, named == &parent ? ownLabel : label});
    }

    if (m_active) {
        for (const QPair<QString, QString>& entry : labels)
            m_active->setEntryText(entry.first, entry.second);
        // The family entry lights up for any of its members - but only here: the
        // third level has an entry with the same id (`trap`) that must show whether
        // the *tag* is captured, not whether the family is.
        m_active->syncFromCaptured(ids + familyIds);
    }
    if (m_submenu)
        m_submenu->syncFromCaptured(ids);
}

void SecondaryButton::setCaptured(bool captured)
{
    if (m_captured == captured)
        return;
    m_captured = captured;
    // The three states live in controls.qss as a property selector; the repolish
    // is what makes the new rule take effect without re-parsing the sheet.
    setProperty("captured", captured);
    style()->unpolish(this);
    style()->polish(this);
    update();
}

void SecondaryButton::rebuildCaption()
{
    const QStringList tags = capturedTags();
    // Two different jobs live here.
    //  · Add widget (palette mode): the button names the spelling that is being
    //    composed - `gloves` reads "elbow gloves" once that entry was picked, and
    //    falls back to its own word when nothing is selected. It never joins the
    //    tag list's other tags; that was the "microskirt, skirt" bug.
    //  · Everywhere else: a named button keeps its own name (the row of type
    //    names must stay readable), and a generic launcher (quality, framing,
    //    faces) joins the tags it claims.
    const QString caption = m_palette && !m_paletteTag.isEmpty()
                                ? captionText(m_paletteTag)
                                : (!m_displayName.isEmpty()
                                       ? m_displayName
                                       : (tags.isEmpty() ? m_placeholder
                                                         : tags.join(QStringLiteral(", "))));
    if (caption != m_caption) {
        m_caption = caption;
        // text() stays truthful (accessibility, tooltips, the regression check);
        // paintEvent() is what wraps and elides it to the width we were given.
        QPushButton::setText(m_caption);
        m_layoutWidth = -1;
        updateGeometry();
        update();
    }
    if (!m_palette)
        setCaptured(!tags.isEmpty());
}

void SecondaryButton::refreshState()
{
    rebuildCaption();
    // A menu that is on screen follows the capture: the pinned menu of a tag that
    // was just edited elsewhere has to show the same thing the button shows.
    if (m_active || m_submenu)
        syncMenuSelection();
}

bool SecondaryButton::filter(std::shared_ptr<teTag> tag) const
{
    return captureList.find(static_cast<QString>(*tag)) != captureList.end();
}

void SecondaryButton::clear()
{
    // SC-02: a menu that belongs to a control whose editor was cleared has
    // nothing left to show.
    hideMenu();
    m_contextTag.clear();
    m_paletteTag.clear();
    // A named button keeps its name even when the list is empty: the add widget
    // is a palette, not a caption of the tag list.
    m_caption = m_displayName.isEmpty() ? m_placeholder : m_displayName;
    QPushButton::setText(m_caption);
    setCaptured(false);
    m_layoutWidth = -1;
    clearLinks();
    updateGeometry();
}

void SecondaryButton::reset()
{
    // No index and no item data to put back: the tags are gone with clearLinks().
}

void SecondaryButton::applyAction(const QString& id)
{
    const Action* action = findAction(id);
    if (!action)
        return;
    if (!taglistwidget || !taglistwidget->showing_list)
        return;

    // The same two steps a combo box did when its selection changed: what the
    // entry does not contain is retired, what it contains and is missing is
    // inserted. A tag the entry keeps is left alone, so an edited tag text is not
    // reverted by opening the menu.
    const std::set<std::shared_ptr<teTag>> linked = linked_tags;
    for (const std::shared_ptr<teTag>& tag : linked) {
        if (!action->tags.contains(static_cast<QString>(*tag)))
            taglistwidget->retire(tag);
    }
    for (const QString& text : action->tags) {
        bool found = false;
        for (const std::shared_ptr<teTag>& tag : linked_tags) {
            if (static_cast<QString>(*tag) == text) {
                found = true;
                break;
            }
        }
        if (!found)
            taglistwidget->tagInsertAbove(false, makeTag(text));
    }
    edited();
    refreshState();
}

void SecondaryButton::toggleAction(const QString& id, bool on)
{
    const Action* action = findAction(id);
    if (!action || !taglistwidget || !taglistwidget->showing_list)
        return;

    if (on) {
        // `group` is "these entries exclude each other": switching one on takes
        // the rest of its group out of the capture, which is how a `>>T` button
        // replaces its parent entry with a variant. A menu that opens a third
        // level is exempt - setFamily() already owns the whole capture there
        // (trap and its aliases), and retiring the parent would tear the third
        // level down while it is being used.
        if (!action->group.isEmpty() && !activeMenuHasSubmenu()) {
            QStringList siblings;
            for (const Action& member : activeActions())
                if (member.group == action->group && member.id != action->id)
                    siblings += member.tags;
            const std::set<std::shared_ptr<teTag>> linked = linked_tags;
            for (const std::shared_ptr<teTag>& tag : linked)
                if (siblings.contains(static_cast<QString>(*tag)))
                    taglistwidget->retire(tag);
        }
        // `implies` is what "mosaic censoring" does with "censored": switched on it
        // brings the implied tag along, but switching it off never takes that tag
        // away - the user may want only the specific one, and drops `censored`
        // himself.
        QStringList toAdd = action->tags;
        for (const QString& implied : action->implies)
            if (!toAdd.contains(implied))
                toAdd.push_back(implied);
        for (const QString& text : toAdd) {
            bool found = false;
            for (const std::shared_ptr<teTag>& tag : linked_tags) {
                if (static_cast<QString>(*tag) == text) {
                    found = true;
                    break;
                }
            }
            if (!found)
                taglistwidget->tagInsertAbove(false, makeTag(text));
        }
    } else {
        const std::set<std::shared_ptr<teTag>> linked = linked_tags;
        for (const std::shared_ptr<teTag>& tag : linked) {
            if (action->tags.contains(static_cast<QString>(*tag)))
                taglistwidget->retire(tag);
        }
    }
    edited();
    refreshState();
}

QSize SecondaryButton::sizeHint() const
{
    QSize hint = QPushButton::sizeHint();
    const int lines = qMax(1, captionLayout().lineCount());
    hint.setHeight(qMax(kMinHeight, lines * fontMetrics().lineSpacing() + 2 * kVerticalPadding));
    return hint;
}

QSize SecondaryButton::minimumSizeHint() const
{
    QSize hint = QPushButton::minimumSizeHint();
    hint.setWidth(qMin(hint.width(), 48));
    hint.setHeight(sizeHint().height());
    return hint;
}

void SecondaryButton::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QStyleOptionButton option;
    initStyleOption(&option);
    option.text.clear();                    // the wrapped text is drawn below

    QPainter painter(this);
    style()->drawControl(QStyle::CE_PushButton, &option, &painter, this);

    const CaptionLayout& layout = captionLayout();
    if (layout.lines.isEmpty())
        return;
    painter.setPen(option.palette.buttonText().color());
    const int lineHeight = fontMetrics().lineSpacing();
    int y = (height() - layout.lines.size() * lineHeight) / 2;
    for (const QString& line : layout.lines) {
        painter.drawText(QRect(0, y, width(), lineHeight), Qt::AlignCenter, line);
        y += lineHeight;
    }
}

void SecondaryButton::resizeEvent(QResizeEvent* event)
{
    QPushButton::resizeEvent(event);
    // The number of lines follows the width, so a resize can change the height the
    // layout has to give us. updateGeometry() re-asks; the width does not depend
    // on the height, so this settles after one pass.
    m_layoutWidth = -1;
    updateGeometry();
}

void SecondaryButton::enterEvent(QEnterEvent* event)
{
    // SC-02: hovering previews the menu. It is click-through and it goes away as
    // soon as the cursor leaves the button.
    if (m_mode == Mode::Hidden && isEnabled())
        openPreview();
    QPushButton::enterEvent(event);
}

void SecondaryButton::leaveEvent(QEvent* event)
{
    // While the button is held the cursor is expected to leave it - that is how an
    // entry is picked - so only a hover preview closes here.
    if (m_mode == Mode::Preview && !m_dragging)
        hideMenu();
    QPushButton::leaveEvent(event);
}

void SecondaryButton::mousePressEvent(QMouseEvent* event)
{
    const bool left = event->button() == Qt::LeftButton;
    const bool right = event->button() == Qt::RightButton;
    if (!left && !right) {
        QPushButton::mousePressEvent(event);
        return;
    }
    // "Clicking the button again" is one of the ways a pinned menu closes (SC-02).
    m_rightPressPending = false;
    if (m_mode == Mode::Pinned) {
        hideMenu();
        event->accept();
        return;
    }

    const QPoint global = event->globalPosition().toPoint();
    // The hover preview is click-through, so a press on it arrives here with no
    // drag in progress. It belongs to the menu (its entries must be clickable), and
    // it has to start the same interaction a press on the button would.
    if (!m_dragging && m_active && m_mode == Mode::Preview
        && m_active->frameGeometry().contains(global)) {
        // SC-05: a right press switches to the second menu even when it lands on
        // the first one's preview - the preview is what the cursor happened to
        // hover, not what was asked for.
        if (right && !m_rightMenuPath.isEmpty() && m_active != m_rightMenu) {
            QPushButton::mousePressEvent(event);
            if (openPreview(true)) {
                m_dragging = true;
                m_active->setPreview(false);
            }
            event->accept();
            return;
        }
        m_dragging = true;
        m_rightClickUsed = false;
        if (right)
            handleMenuPress(global, true);
        event->accept();
        return;
    }
    if (m_dragging) {
        // Already dragging: every press is an action inside the menus, never a new
        // preview. The third level is on top, so it is tested first - that is what
        // lets a right click walk the aliases while the left button stays down.
        handleMenuPress(global, right);
        event->accept();
        return;
    }

    m_rightClickUsed = false;       // a new interaction starts here
    QPushButton::mousePressEvent(event);    // keeps the :pressed look while dragging
    if (right) {
        // Undecided: a drag opens a menu (SC-05), a plain click opens the tag
        // operations of what this button captured. Opening the menu on press made
        // the second impossible - and a tag that no button offers a menu for can
        // never be merged by hand.
        if (linked_tags.empty()) {
            // A launcher with nothing captured has no operations to offer; it
            // keeps the old gesture, where the menu opens on the press itself.
            if (openPreview(!m_rightMenuPath.isEmpty())) {
                m_dragging = true;
                if (m_active)
                    m_active->setPreview(false);
            }
            event->accept();
            return;
        }
        m_rightPressPending = true;
        m_rightPressPos = global;
        event->accept();
        return;
    }
    // SC-05: a left press opens the menu (the right menu on the way back).
    if (openPreview(false)) {
        m_dragging = true;
        // A held button is a deliberate act: the menu becomes solid (and no longer
        // click-through) for as long as the button is down.
        if (m_active)
            m_active->setPreview(false);
    }
    event->accept();
}

void SecondaryButton::handleMenuPress(const QPoint& global, bool right)
{
    // The third level is on top, so it is always tested first.
    if (m_submenu) {
        const int index = m_submenu->itemAt(global);
        if (index >= 0) {
            m_rightClickUsed = m_rightClickUsed || right;
            if (const SecondaryAction* action = m_submenu->action(m_submenu->itemId(index)))
                activateAction(*action, true, true);
            return;
        }
    }
    if (m_active) {
        const int index = m_active->itemAt(global);
        if (index >= 0) {
            const SecondaryAction* action = m_active->action(m_active->itemId(index));
            if (!action)
                return;
            m_rightClickUsed = m_rightClickUsed || right;
            if (!action->submenuPath.isEmpty())
                toggleSubmenuFor(*action);      // right click opens/closes the level
            else
                activateAction(*action, false, true);
        }
    }
}

void SecondaryButton::showCapturedTagMenu()
{
    if (!taglistwidget || linked_tags.empty())
        return;
    // The claims are a set; the menu works on a snapshot because erasing a tag
    // erases the claim it holds.
    QVector<std::shared_ptr<teTag>> tags(linked_tags.begin(), linked_tags.end());
    QMenu menu(this);
    menu.setStyleSheet(QStringLiteral(R"(QMenu {
        background-color: transparent;
        font-size:14px;
        color:#8dfda7;
        border-radius:3px;
    }
    QMenu::item {
        background-color: rgb(27,29,37);
        padding: 3px;
        border-radius:2px;
        margin: 0px;
        border: 1px solid #8b9ac6;
    }
    QMenu::item:selected {background-color: #3d258f;}
)"));
    menu.setAttribute(Qt::WA_TranslucentBackground);
    menu.setWindowFlags(menu.windowFlags() | Qt::FramelessWindowHint);

    QAction* editAction = menu.addAction(QIcon(QStringLiteral(":/res/menu_edit.png")),
                                        QStringLiteral("edit (F2/Ctrl+E)"));
    QAction* deleteAction = menu.addAction(QIcon(QStringLiteral(":/res/menu_remove.png")),
                                          QStringLiteral("delete (Ctrl+D/del)"));
    menu.addSeparator();
    QAction* insertAction = menu.addAction(QIcon(QStringLiteral(":/res/menu_add.png")),
                                          QStringLiteral("insert above (Ctrl+W)"));
    QAction* insertBelowAction = menu.addAction(QIcon(QStringLiteral(":/res/menu_add.png")),
                                               QStringLiteral("insert below"));
    menu.addSeparator();
    QAction* copyAction = menu.addAction(QIcon(QStringLiteral(":/res/menu_copy.png")),
                                        QStringLiteral("copy (Ctrl+C)"));

    QAction* chosen = menu.exec(QCursor::pos());
    if (!chosen)
        return;
    if (chosen == editAction) {
        // Editing is one tag at a time - the same rule the lists follow.
        taglistwidget->tagEditCore(tags.front());
    } else if (chosen == deleteAction) {
        const QVector<std::shared_ptr<teTag>> doomed = tags;
        for (const std::shared_ptr<teTag>& tag : doomed)
            taglistwidget->tagErase(tag);
    } else if (chosen == insertAction) {
        taglistwidget->tagInsertAbove(true, nullptr, 2);
    } else if (chosen == insertBelowAction) {
        taglistwidget->tagInsertBelow(true, nullptr, 2);
    } else if (chosen == copyAction) {
        QStringList texts;
        for (const std::shared_ptr<teTag>& tag : tags)
            texts.append(static_cast<QString>(*tag));
        QApplication::clipboard()->setText(texts.join(QStringLiteral(", ")));
    }
}

void SecondaryButton::mouseMoveEvent(QMouseEvent* event)
{
    if (m_rightPressPending && (event->buttons() & Qt::RightButton)) {
        // The press turned into a drag: that is the "show me the menu" gesture.
        const QPoint global = event->globalPosition().toPoint();
        if ((global - m_rightPressPos).manhattanLength() >= QApplication::startDragDistance()) {
            m_rightPressPending = false;
            // SC-05: the second menu when the button declares one; without one the
            // first menu, whose third level is what a right drag is for there.
            if (openPreview(!m_rightMenuPath.isEmpty())) {
                m_dragging = true;
                if (m_active)
                    m_active->setPreview(false);
            }
        }
    }
    if (m_dragging && m_active && m_mode == Mode::Preview) {
        // The preview menu is mouse-transparent, so the button keeps the implicit
        // grab: the cursor position is mapped into the menu and the entry under it
        // is highlighted (SC-04). The same rects are used on release.
        m_active->setHoverIndex(m_active->itemAt(event->globalPosition().toPoint()));
        event->accept();
        return;
    }
    QPushButton::mouseMoveEvent(event);
}

void SecondaryButton::mouseReleaseEvent(QMouseEvent* event)
{
    const bool left = event->button() == Qt::LeftButton;
    const bool right = event->button() == Qt::RightButton;
    if (m_rightPressPending && right) {
        // A right click that did not move: the tags this button captured.
        m_rightPressPending = false;
        QPushButton::mouseReleaseEvent(event);
        showCapturedTagMenu();
        return;
    }
    if (m_dragging && (left || right)) {
        m_dragging = false;
        SecondaryMenu* menu = m_active;
        SecondaryMenu* submenu = m_submenu;
        const QPoint global = event->globalPosition().toPoint();
        const bool onButton = rect().contains(event->position().toPoint());

        // Releasing over the button itself always pins. This is checked before the
        // entries because a tall menu can be flipped above the button and put one
        // of its entries right under the cursor: pressing there must lock the menu,
        // not "click" whatever happens to be underneath.
        if (onButton) {
            QPushButton::mouseReleaseEvent(event);
            // An activatable button is clicked, not pinned: the click selects its
            // own tag. In the add widget that selection is the type of the tag
            // being composed (a second click takes it away); elsewhere it writes
            // or removes the tag. Its menu stays one hover (or one right press)
            // away, and a press that ended on an entry never reaches this branch.
            if (left && !m_rightClickUsed && !m_ownTag.isEmpty()) {
                hideMenu();
                if (m_compose && m_composeSink)
                    m_composeSink(m_ownTag, true);
                else
                    toggleOwnTag();
                return;
            }
            pinActive();
            m_rightClickUsed = false;
            return;
        }

        // A release over the third level wins: the aliases sit on top of the menu.
        if (submenu) {
            const int index = submenu->itemAt(global);
            if (index >= 0) {
                submenu->setHoverIndex(-1);
                QPushButton::mouseReleaseEvent(event);
                if (const SecondaryAction* action = submenu->action(submenu->itemId(index)))
                    activateAction(*action, true, false);
                m_rightClickUsed = false;
                return;
            }
        }
        if (menu) {
            const int index = menu->itemAt(global);
            if (index >= 0 && m_rightClickUsed) {
                // A right click already switched something here: the release must
                // not switch a second entry - but it must not close the menus
                // either, the user is still working in the third level.
                QPushButton::mouseReleaseEvent(event);
                pinActive();
                if (m_submenu)
                    m_submenu->setPreview(false);
                m_rightClickUsed = false;
                return;
            }
            if (index >= 0) {
                menu->setHoverIndex(-1);
                QPushButton::mouseReleaseEvent(event);
                const SecondaryAction* action = menu->action(menu->itemId(index));
                const bool wasFamilyOn = action && !action->submenuPath.isEmpty()
                                         && familyCaptured(*action);
                if (action)
                    activateAction(*action, false, false);
                // A released drag that switched a family on leaves its third level
                // up: pin both windows so the aliases can be picked (an unpinned
                // popup would belong to nothing once the drag is over).
                if (action && !action->submenuPath.isEmpty() && !wasFamilyOn && m_active) {
                    pinActive();
                    if (m_submenu)
                        m_submenu->setPreview(false);
                }
                m_rightClickUsed = false;
                return;
            }
        }
        m_rightClickUsed = false;
        hideMenu();
    }
    QPushButton::mouseReleaseEvent(event);
}

void SecondaryButton::hideEvent(QHideEvent* event)
{
    // A folded page or a closed window cannot own a popup that stays on screen.
    m_rightPressPending = false;
    hideMenu();
    QPushButton::hideEvent(event);
}

bool SecondaryButton::eventFilter(QObject* watched, QEvent* event)
{
    if (m_mode != Mode::Pinned)
        return QPushButton::eventFilter(watched, event);

    switch (event->type()) {
    case QEvent::MouseButtonPress: {
        auto* mouse = static_cast<QMouseEvent*>(event);
        const QPoint global = mouse->globalPosition().toPoint();
        // A click on the button is handled by mousePressEvent() (it closes the
        // menu); a click inside a menu belongs to that menu.
        const bool inMenu = m_active && m_active->frameGeometry().contains(global);
        const bool inSubmenu = m_submenu && m_submenu->frameGeometry().contains(global);
        if (!inMenu && !inSubmenu)
            hideMenu();
        break;
    }
    case QEvent::MouseButtonRelease: {
        auto* mouse = static_cast<QMouseEvent*>(event);
        if (!m_submenu || !m_submenu->isVisible())
            break;
        const QPoint global = mouse->globalPosition().toPoint();
        if (!m_submenu->frameGeometry().contains(global))
            break;
        const int index = m_submenu->itemAt(global);
        if (index < 0)
            break;
        // Only a real entry counts - a release on the scroll bar or the frame
        // must not pick whatever entry happens to be behind it.
        if (!qobject_cast<QAbstractButton*>(m_submenu->childAt(m_submenu->mapFromGlobal(global))))
            break;
        const SecondaryAction* action = m_submenu->action(m_submenu->itemId(index));
        if (!action)
            break;
        // The press that opened this level still owns the mouse (it happened on
        // the level above), so the release is the only chance to pick the entry
        // under the cursor. Consume it: the entry's own clicked() must not fire
        // a second time.
        m_submenu->setHoverIndex(-1);
        activateAction(*action, true, true);
        m_rightClickUsed = false;
        hideMenu();
        return true;
    }
    case QEvent::KeyPress:
        if (static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape) {
            if (m_submenu) {
                hideSubmenu();
                return true;
            }
            hideMenu();
            return true;
        }
        break;
    case QEvent::ApplicationDeactivate:
    case QEvent::WindowDeactivate:
        hideMenu();
        break;
    default:
        break;
    }
    return QPushButton::eventFilter(watched, event);
}
