#include "secondarybutton.h"

#include "clothesemoji.h"
#include "tetag.h"
#include "tetaglistwidget.h"
#include "svgicon.h"

#include <QApplication>
#include <QClipboard>
#include <QCursor>
#include <QKeyEvent>
#include <QMenu>
#include <QStyle>
#include <QStyleOptionButton>

#include <algorithm>
#include <functional>

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
void readMenuActions(const QString& path, QVector<SecondaryAction>& out, int depth = 0)
{
    // Every level is read, not just the third: a menu may point at another one in
    // its entries' `submenu` property, and the button has to know those tags
    // before anything is shown (filter(), the capture list, the family labels).
    // `depth` is a cycle guard - a file that names itself would otherwise recurse
    // until the stack runs out.
    if (path.isEmpty() || depth > 5)
        return;
    const QVector<SecondaryAction> own = SecondaryMenu::readActions(path);
    out += own;
    for (const SecondaryAction& action : own)
        if (!action.submenuPath.isEmpty())
            readMenuActions(action.submenuPath, out, depth + 1);
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
    // A launcher that has not been told about an own tag writes nothing when
    // clicked: that is the "pure" kind, and saying it here means every button in
    // an editor column gets the faint style without each caller repeating it.
    // setOwnTag() flips this to "activatable".
    setKind(QStringLiteral("pure"));
    loadActions();
}

SecondaryButton::~SecondaryButton()
{
    hideMenu();
    if (sPinned == this)
        sPinned = nullptr;
    qDeleteAll(m_children);
    m_children.clear();
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

void SecondaryButton::showPreviewAt(const QRect& anchor, std::shared_ptr<teTag> tag)
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
    // After hideMenu(), which drops the anchor of the menu it closed.
    m_anchorTag = std::move(tag);
    menu->setPreview(true);
    menu->showAt(anchor);
    m_mode = Mode::Preview;
    qApp->installEventFilter(this);
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
    if (m_active || !m_children.isEmpty())
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

void SecondaryButton::setTagRewrite(std::function<bool(std::shared_ptr<teTag>, const QString&, const QStringList&)> sink)
{
    m_rewriteSink = std::move(sink);
}

bool SecondaryButton::rewriteAnchoredTag(const std::shared_ptr<teTag>& tag, const Action& action)
{
    // One entry is one spelling; a menu entry with several tags describes a set
    // ("censored|mosaic censoring") and is not a type this tag can be rewritten
    // to. Those keep the capture semantics.
    if (!tag || !m_rewriteSink || action.tags.size() != 1)
        return false;
    // Which spelling of the tag this menu stands for: the longest one from its
    // own family (`glowing halo` on `pink glowing halo`). Replacing only the
    // registered type (`halo`) would keep `glowing` - it is not a type on its own
    // - and append the picked spelling after it.
    const QString family = paletteTagFor(static_cast<QString>(*tag));
    if (family.isEmpty())
        return false;
    return m_rewriteSink(tag, family, action.tags);
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
    connect(menu, &SecondaryMenu::entryRightPressed, this,
            [this, menu](const QString& id) { handleMenuRightPress(menu, id); });
    connect(menu, &SecondaryMenu::dismissRequested, this,
            [this, menu] { handleMenuBlankPress(menu, true); });
    connect(menu, &SecondaryMenu::blankAreaPressed, this,
            [this, menu] { handleMenuBlankPress(menu, false); });
    slot = menu;
    return menu;
}

SecondaryMenu* SecondaryButton::childOf(const SecondaryMenu* parent) const
{
    if (!parent)
        return nullptr;
    // The second level is not in m_children, but it is the parent of the first.
    if (parent == m_active)
        return m_children.value(0);
    for (int i = 0; i + 1 < m_children.size(); ++i)
        if (m_children[i] == parent)
            return m_children[i + 1];
    return nullptr;
}

void SecondaryButton::dropChildrenOf(SecondaryMenu* parent)
{
    if (!parent)
        return;
    int index = m_children.indexOf(parent);
    if (index < 0 && parent == m_active)
        index = -1;                       // the second level owns all of them
    else if (index < 0)
        return;
    // A press that was recorded in a menu that is about to go away can never be
    // matched by a release any more.
    m_pressMenu = nullptr;
    // Everything below this menu belongs to an entry that is no longer the one
    // being used. deleteLater: this can run from a child's own click handler.
    while (m_children.size() > index + 1) {
        SecondaryMenu* doomed = m_children.takeLast();
        doomed->dismiss();
        doomed->deleteLater();
    }
}

SecondaryMenu* SecondaryButton::ensureSubmenu(const Action& action, SecondaryMenu* parent)
{
    if (action.submenuPath.isEmpty() || !parent)
        return nullptr;
    // Already showing exactly this child: keep it (and whatever hangs below it).
    if (SecondaryMenu* existing = childOf(parent)) {
        if (existing->uiPath() == action.submenuPath)
            return existing;
        dropChildrenOf(parent);
    }
    auto* menu = new SecondaryMenu(nullptr);
    if (!menu->loadUi(action.submenuPath)) {
        delete menu;
        return nullptr;
    }
    // The handlers are the same at every depth: an entry of a child menu either
    // opens the level below it or finishes the choice. `menu` is captured so a
    // deeper entry knows which level it belongs to.
    connect(menu, &SecondaryMenu::entryRightPressed, this,
            [this, menu](const QString& id) { handleMenuRightPress(menu, id); });
    connect(menu, &SecondaryMenu::dismissRequested, this,
            [this, menu] { handleMenuBlankPress(menu, true); });
    connect(menu, &SecondaryMenu::blankAreaPressed, this,
            [this, menu] { handleMenuBlankPress(menu, false); });
    connect(menu, &SecondaryMenu::actionTriggered, this, [this, menu](const QString& id, bool) {
        // Several aliases may be on together, so this toggles like every other
        // additive entry. A pinned menu stays up at every depth: adding a second
        // member of the same submenu is a normal thing to want.
        if (const SecondaryAction* action = menu->action(id)) {
            if (!action->submenuPath.isEmpty())
                openSubmenu(*action, menu);
            else
                activateAction(*action, true, m_mode == Mode::Pinned, menu);
        }
    });
    // Keep the chain contiguous: a child of a child sits right after its parent,
    // and anything that used to hang below that parent is gone already.
    const int parentIndex = m_children.indexOf(parent);
    m_children.insert(parentIndex + 1, menu);
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
    // A preview can be turned solid by a press, and then the release never comes
    // (the pointer went to the add widget / a context menu): without a watcher it
    // stayed on screen forever. The same filter closes it on an outside press.
    qApp->installEventFilter(this);
    syncMenuSelection();
    return true;
}

bool SecondaryButton::openSubmenu(const Action& action, SecondaryMenu* parent)
{
    if (action.submenuPath.isEmpty())
        return false;
    if (!parent)
        parent = m_active;
    SecondaryMenu* menu = ensureSubmenu(action, parent);
    if (!menu)
        return false;

    QRect anchor = parent ? parent->itemRect(action.id) : QRect();
    if (anchor.isNull())
        anchor = visualRect();
    // A child opened by a right press while the left button is held is where the
    // next click goes: it must not stay a click-through hover preview, or its
    // entries can never be pressed (the press fell through to the menu behind it
    // and was read as "empty area", closing the level again).
    menu->setPreview(m_mode == Mode::Preview && !m_dragging);
    menu->showAt(anchor);

    // Every entry stays listed, including the ones that are captured: a child
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
    // A pinned menu is usable at every depth: a child that stayed a preview
    // would be click-through while the user is working in it.
    for (SecondaryMenu* child : m_children)
        child->setPreview(false);
    m_mode = Mode::Pinned;
    // The press that pinned this menu belonged to the drag; a later release must
    // not be matched against it.
    m_pressMenu = nullptr;
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
    while (!m_children.isEmpty()) {
        SecondaryMenu* child = m_children.takeLast();
        child->dismiss();
        // deleteLater, not delete: this runs from a child's own click handler
        // when an entry is used, and the window is the sender.
        child->deleteLater();
    }
}

void SecondaryButton::hideMenu()
{

    qApp->removeEventFilter(this);
    m_pressMenu = nullptr;
    if (m_mode == Mode::Pinned && sPinned == this)
        sPinned = nullptr;
    hideSubmenu();
    if (m_active) {
        m_active->dismiss();
        m_active = nullptr;
    }
    m_mode = Mode::Hidden;
    m_dragging = false;
    // The menu is gone: so is the tag it was editing. A later press must not
    // rewrite a tag the user has long left.
    m_anchorTag.reset();
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
    // A menu opened at a reftag shows what that tag says, not what this button
    // captured: the tag a hover preview was opened at is the "capture" for as
    // long as the menu is up (§1.13). The family spelling is what an entry can
    // match, exactly like the add widget's palette mode.
    if (std::shared_ptr<teTag> anchor = m_anchorTag.lock()) {
        const QString spelling = paletteTagFor(static_cast<QString>(*anchor));
        return spelling.isEmpty() ? QStringList{} : QStringList{spelling};
    }
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

bool SecondaryButton::isFamilyMember(const QVector<Action>& pool,const Action& parent,
                                     const Action& member) const
{
    if (&member == &parent)
        return true;
    if (member.id == parent.id && member.menuPath == parent.menuPath)
        return true;
    // A group is the explicit "these entries are alternatives" marker. It must be
    // non-empty: every ungrouped entry would otherwise join one family (that made
    // one body part look like all of them).
    if (!parent.group.isEmpty() && member.group == parent.group)
        return true;
    if (parent.submenuPath.isEmpty())
        return false;
    // `family="true"` entries (the body parts) have no group: their family is the
    // submenu tree itself. Walk the paths the button already read.
    QStringList paths{parent.submenuPath};
    for (int i = 0; i < paths.size(); ++i) {
        if (!member.menuPath.isEmpty() && member.menuPath == paths.at(i))
            return true;
        for (const Action& action : pool)
            if (action.menuPath == paths.at(i) && !action.submenuPath.isEmpty()
                && !paths.contains(action.submenuPath))
                paths.push_back(action.submenuPath);
    }
    return false;
}

bool SecondaryButton::familyCaptured(const Action& parent) const
{
    for (const Action& member : m_actions) {
        if (member.tags.isEmpty() || !isFamilyMember(m_actions,parent,member))
            continue;
        if (entryCaptured(member))
            return true;
    }
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
        // goes, not just the one whose name the entry happens to show. An entry
        // without a group is only itself (see familyCaptured()).
        // The entry stands for its whole family: its own tag, same-group members
        // and everything in its submenu tree (readMenuActions() already read every
        // level, so the body-part variants are in m_actions even before the third
        // level was ever opened).
        QStringList members = parent.tags;
        for (const Action& member : m_actions)
            if (isFamilyMember(m_actions,parent,member))
                members += member.tags;
        const std::set<std::shared_ptr<teTag>> linked = linked_tags;
        for (const std::shared_ptr<teTag>& tag : linked)
            if (members.contains(static_cast<QString>(*tag)))
                taglistwidget->retire(tag);
    }
    edited();
    refreshState();
}

void SecondaryButton::activateFamilyEntry(const Action& parent, bool keepOpen,
                                          SecondaryMenu* owner)
{
    // The family entry is its on/off switch *and* the way into the level below:
    // switching it on captures the parent tag and offers the other spellings,
    // switching it off releases the family and puts those away again.
    const bool wasOn = familyCaptured(parent);
    setFamily(parent, !wasOn);
    if (!wasOn) {
        // Only a menu that stays up after the click can hold a level below it.
        // A release that ends a drag is a plain "switch this on": opening the
        // aliases there left the menu on screen after the gesture was over, so
        // closing it needed a second click or Escape.
        if (keepOpen && owner)
            openSubmenu(parent, owner);
        if (keepOpen) {
            syncMenuSelection();
            return;
        }
        hideMenu();
        return;
    }
    dropChildrenOf(owner ? owner : m_active);
    if (keepOpen)
        syncMenuSelection();
    else
        hideMenu();
}

void SecondaryButton::toggleSubmenuFor(const Action& parent, SecondaryMenu* owner)
{
    if (parent.submenuPath.isEmpty())
        return;
    SecondaryMenu* menu = owner ? owner : m_active;
    // Right click = "show me the other spellings", and a second right click puts
    // them away again.
    SecondaryMenu* child = childOf(menu);
    if (child && child->uiPath() == parent.submenuPath) {
        dropChildrenOf(menu);
        return;
    }
    openSubmenu(parent, menu);
}

void SecondaryButton::activateAction(SecondaryAction action, bool fromSubmenu, bool keepOpen,
                                     SecondaryMenu* owner)
{
    // A caller that has no menu at hand (activate(id), the add widget's drag)
    // means the second level, which is the only one it can be talking about.
    if (!owner)
        owner = m_active;

    // Pointing at another entry of the *second* level means the user is done with
    // whatever hung below it: a child menu never survives a click somewhere else
    // in its parent.
    if (!fromSubmenu)
        dropChildrenOf(owner);

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

    // §1.13: a menu opened at a reftag edits that tag. `red collar` dropped on
    // `sailor collar` is `red sailor collar` - not a second tag, which is what
    // applying the entry to the button's capture list produced (the coloured
    // spelling is not in the menu, so there was nothing to replace).
    if (const std::shared_ptr<teTag> anchor = m_anchorTag.lock()) {
        if (!rewriteAnchoredTag(anchor, action)) {
            // The tag no longer ends with this menu's spelling (it was edited
            // while the menu was up). Writing the entry on its own is exactly what
            // the anchored gesture exists to prevent, so nothing is written.
            telog(QStringLiteral("[secondary menu] %1 is not a spelling of %2")
                      .arg(action.id, static_cast<QString>(*anchor)));
            if (!keepOpen)
                hideMenu();
            return;
        }
        // An entry that opens a level below still offers it when the menu is
        // staying up; a drag release ends the gesture and closes everything.
        if (!action.submenuPath.isEmpty() && keepOpen)
            openSubmenu(action, owner ? owner : m_active);
        if (keepOpen)
            syncMenuSelection();
        else
            hideMenu();
        return;
    }

    if (!action.submenuPath.isEmpty()) {
        activateFamilyEntry(action, keepOpen, owner);
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
    // Per menu file, because every level has its own ids.
    const auto standingIds = [&](const QVector<Action>& entries) {
        QStringList lit;
        for (const Action& action : entries) {
            if (action.tags.isEmpty())
                continue;
            const bool stands = action.checkable ? capturedAll(action)
                                                 : [&] {
                                                       QStringList theirs = action.tags;
                                                       theirs.sort();
                                                       return theirs == captured;
                                                   }();
            if (stands)
                lit.push_back(action.id);
        }
        return lit;
    };
    const QStringList ids = standingIds(actions);

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
        // Only a family entry represents its level below; a plain submenu entry
        // is just a launcher. `family="true"` is the marker the body parts use,
        // where the variants are independent tags with no group.
        if (parent.submenuPath.isEmpty())
            continue;
        if (parent.group.isEmpty() && !parent.family)
            continue;
        const Action* firstLit = nullptr;
        bool selfLit = false;
        for (const Action& member : actions) {
            if (member.tags.isEmpty() || !isFamilyMember(actions,parent,member))
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
    // Every level lights its own entries: a child menu is a different file with
    // different ids, and pushing the parent's ids at it would light nothing.
    for (SecondaryMenu* child : m_children)
        child->syncFromCaptured(standingIds(child->actions()));
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
    if (m_active || !m_children.isEmpty())
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
        // A left click on the anchor closes the pinned menu; a right click must
        // not (right button never closes a menu - it is for the entries).
        if (left)
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
                m_dragLeftHeld = false;
                m_rightUsedMenu = nullptr;         // opened by the right button alone
                m_active->setPreview(false);
            }
            event->accept();
            return;
        }
        m_dragging = true;
        m_dragLeftHeld = !right;
        m_rightUsedMenu = nullptr;
        m_rightClickUsed = false;
        m_rightPressedId.clear();
        if (right)
            handleMenuPress(global, true);
        event->accept();
        return;
    }
    if (m_dragging) {
        // Already dragging: every press is an action inside the menus, never a new
        // preview. The third level is on top, so it is tested first - that is what
        // lets a right click walk the aliases while the left button stays down.
        m_rightPressedId.clear();
        if (!right)
            m_rightClickUsed = false;       // a new left press is a new pick
        else if (m_dragLeftHeld)
            m_rightUsedMenu = m_active;       // even on empty space
        handleMenuPress(global, right);
        event->accept();
        return;
    }

    m_rightClickUsed = false;       // a new interaction starts here
    m_rightPressedId.clear();
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
                m_dragLeftHeld = false;
                m_rightUsedMenu = nullptr;
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
        m_dragLeftHeld = left;
        m_rightUsedMenu = nullptr;
        // A held button is a deliberate act: the menu becomes solid (and no longer
        // click-through) for as long as the button is down.
        if (m_active)
            m_active->setPreview(false);
    }
    event->accept();
}

void SecondaryButton::handleMenuPress(const QPoint& global, bool right)
{
    // A menu owns its whole rectangle, gaps included: a press between two entries
    // belongs to that menu. Testing only the entries let the press fall through to
    // the level below - a right click in the seam between two third-level buttons
    // activated the second-level entry drawn underneath.
    for (int i = m_children.size() - 1; i >= 0; --i) {
        SecondaryMenu* child = m_children[i];
        if (!child || !child->frameGeometry().contains(global))
            continue;
        const int index = child->itemAt(global);
        if (index < 0) {
            if (right)
                handleMenuBlankPress(child, true);
            return;
        }
        if (right)
            handleMenuRightPress(child, child->itemId(index));
        else
            // A left press only marks the row. Acting here and again on the
            // release is how a click on a child entry switched itself off again;
            // the release (mouseReleaseEvent) is the one that picks.
            child->setHoverIndex(index);
        return;
    }
    if (m_active && m_active->frameGeometry().contains(global)) {
        const int index = m_active->itemAt(global);
        if (index < 0) {
            if (right)
                handleMenuBlankPress(m_active, true);
            return;
        }
        if (right)
            handleMenuRightPress(m_active, m_active->itemId(index));
        else
            m_active->setHoverIndex(index);
        return;
    }
    // Outside every menu. During a drag this is the button's grab doing the
    // delivery: a right click still means "put the level below away".
    if (right)
        handleMenuBlankPress(m_active, true);
}

/**
 * Right press on an entry: expand the level below it, or click a plain entry.
 *
 * The release under the cursor is part of the same click, so it is remembered in
 * m_rightPressedId and skipped there - a right press and its release are one
 * action, not a toggle followed by an undo.
 */
void SecondaryButton::handleMenuRightPress(SecondaryMenu* menu, const QString& id)
{
    if (!menu)
        return;
    const SecondaryAction* action = menu->action(id);
    if (!action)
        return;
    m_rightClickUsed = true;
    m_rightPressedId = id;
    if (m_dragging && m_dragLeftHeld)
        m_rightUsedMenu = menu;
    if (!action->submenuPath.isEmpty()) {
        // The entry is the family's entry: a right press only asks to see the other
        // spellings. Writing its own tag here would be a different gesture (the
        // left click on a pinned menu).
        toggleSubmenuFor(*action, menu);
        return;
    }
    // A plain entry *is* the click. The menu stays up: switching several entries in
    // a row is the whole point of a pinned menu, and of the drag gesture.
    activateAction(*action, true, true, menu);
    syncMenuSelection();
}

void SecondaryButton::handleMenuBlankPress(SecondaryMenu* menu, bool right)
{
    if (!menu)
        return;
    if (right && m_dragging && m_dragLeftHeld)
        m_rightUsedMenu = menu;
    // With a level open below it, the empty part means "put that away": the level
    // above stays (the user is aiming at it, not abandoning the stack). This is
    // the same for both buttons.
    if (childOf(menu)) {
        dropChildrenOf(menu);
        return;
    }
    // A right click never closes a menu. The second level is closed by releasing
    // the drag, by Escape, or - once pinned - by a click outside it; a right click
    // inside it means "work with the entries", nothing else.
    if (right)
        return;

    if (menu == m_active)
        hideMenu();
    else
        dropChildrenOf(parentOf(menu));     // close this level, keep the ones above
}

SecondaryMenu* SecondaryButton::parentOf(const SecondaryMenu* menu) const
{
    if (!menu || menu == m_active)
        return nullptr;
    for (int i = 0; i < m_children.size(); ++i) {
        if (m_children[i] != menu)
            continue;
        return i == 0 ? m_active : m_children[i - 1];
    }
    return nullptr;
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

    QAction* editAction = menu.addAction(svgicon::menuIcon(QStringLiteral(":/icons/pen-line.svg")),
                                        QStringLiteral("edit (F2/Ctrl+E)"));
    QAction* deleteAction = menu.addAction(svgicon::menuIcon(QStringLiteral(":/icons/trash.svg")),
                                          QStringLiteral("delete (Ctrl+D/del)"));
    menu.addSeparator();
    QAction* insertAction = menu.addAction(svgicon::menuIcon(QStringLiteral(":/icons/chevrons-up.svg")),
                                          QStringLiteral("insert above (Ctrl+W)"));
    QAction* insertBelowAction = menu.addAction(svgicon::menuIcon(QStringLiteral(":/icons/chevrons-down.svg")),
                                               QStringLiteral("insert below"));
    menu.addSeparator();
    QAction* copyAction = menu.addAction(svgicon::menuIcon(QStringLiteral(":/icons/copy.svg")),
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
                m_dragLeftHeld = false;
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
    if (m_dragging && right && m_dragLeftHeld) {
        // A right click inside a menu that is being dragged open is part of the
        // drag: the left button is still down. Ending the drag here pinned the
        // menu, and a pinned button swallows the next right press - which is why
        // only the first entry could be clicked.
        m_rightClickUsed = false;
        m_rightPressedId.clear();
        QPushButton::mouseReleaseEvent(event);
        return;
    }
    if (m_dragging && (left || right)) {
        m_dragging = false;
        // The level a right click landed in: its own release is the end of the
        // gesture, not a click. Another level is unaffected.
        SecondaryMenu* rightUsed = m_rightUsedMenu;
        m_rightUsedMenu = nullptr;
        SecondaryMenu* menu = m_active;
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

        // A release over one of the child menus wins: they sit on top of the
        // second level, and the deepest one is the one the cursor was aimed at.
        for (int i = m_children.size() - 1; i >= 0; --i) {
            SecondaryMenu* child = m_children[i];
            const int index = child->itemAt(global);
            if (index < 0)
                continue;
            child->setHoverIndex(-1);
            QPushButton::mouseReleaseEvent(event);
            const QString releasedId = child->itemId(index);
            if (!m_rightPressedId.isEmpty() && releasedId == m_rightPressedId) {
                // One right click is one action: the press already wrote it.
                m_rightPressedId.clear();
                m_rightClickUsed = false;
                return;
            }
            if (child == rightUsed) {
                // A right click happened in this level: letting go here must not
                // also pick whatever the cursor happens to rest on.
                m_rightPressedId.clear();
                m_rightClickUsed = false;
                hideMenu();
                return;
            }
            if (const SecondaryAction* action = child->action(releasedId))
                activateAction(*action, true, false, child);
            m_rightClickUsed = false;
            return;
        }
        if (menu) {
            const int index = menu->itemAt(global);
            if (index >= 0 && menu == rightUsed) {
                // Right clicked in this very level: the release is not a pick.
                m_rightPressedId.clear();
                m_rightClickUsed = false;
                hideMenu();
                return;
            }
            if (index >= 0 && m_rightClickUsed) {
                // A right click already switched something here: the release must
                // not switch a second entry - but it must not close the menus
                // either, the user is still working in the third level.
                QPushButton::mouseReleaseEvent(event);
                m_rightPressedId.clear();
                pinActive();                    // also makes every child usable
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
                    activateAction(*action, false, false, menu);
                // A released drag that switched a family on leaves its third level
                // up: pin both windows so the aliases can be picked (an unpinned
                // popup would belong to nothing once the drag is over).
                if (action && !action->submenuPath.isEmpty() && !wasFamilyOn && m_active) {
                    pinActive();
                    for (SecondaryMenu* child : m_children)
                        child->setPreview(false);
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
    switch (event->type()) {
    case QEvent::MouseButtonPress: {
        auto* mouse = static_cast<QMouseEvent*>(event);
        // A right press never closes anything, inside the menus or out of them.
        if (mouse->button() == Qt::RightButton)
            break;
        const QPoint global = mouse->globalPosition().toPoint();
        // A click on the button is handled by mousePressEvent() (it closes the
        // menu); a click inside a menu belongs to that menu. Anything else - the
        // add widget, a context menu, the editor behind - is "outside" and closes
        // whatever this button has open, preview or pinned.
        bool inMenu = m_active && m_active->frameGeometry().contains(global);
        for (SecondaryMenu* child : m_children)
            inMenu = inMenu || child->frameGeometry().contains(global);
        const bool onButton = rect().contains(mapFromGlobal(global));
        // Remember which menu window got this press. A pinned menu is usable at
        // every depth, so its entries receive press + release themselves; the
        // release branch below has to leave that pair alone.
        m_pressMenu = nullptr;
        if (m_active && m_active->frameGeometry().contains(global))
            m_pressMenu = m_active;
        for (int i = m_children.size() - 1; i >= 0; --i) {
            SecondaryMenu* child = m_children[i];
            if (child && child->frameGeometry().contains(global)) {
                m_pressMenu = child;        // the deepest menu wins
                break;
            }
        }
        if (!inMenu && !onButton)
            hideMenu();
        break;
    }
    case QEvent::MouseButtonRelease: {
        // Only a pinned menu needs the filter's release (the drag path handles its
        // own); taking it here too would activate an entry twice.
        if (m_mode != Mode::Pinned)
            break;
        auto* mouse = static_cast<QMouseEvent*>(event);
        // The release under a right press was already handled as that press's
        // click. It must not toggle the entry a second time, and a right release
        // never closes anything.
        if (mouse->button() == Qt::RightButton) {
            m_rightClickUsed = false;
            m_rightPressedId.clear();
            break;
        }
        if (!m_rightPressedId.isEmpty()) {
            // A left release that stays on the entry a right press just used is
            // part of that same click (the pointer never moved): do not act twice.
            SecondaryMenu* top = deepestMenu();
            const int index = top ? top->itemAt(mouse->globalPosition().toPoint()) : -1;
            const bool sameEntry = top && index >= 0
                                   && top->itemId(index) == m_rightPressedId;
            m_rightPressedId.clear();
            if (sameEntry)
                break;
            // Anywhere else the release is an ordinary pick and falls through.
        }
        // Press and release in one menu window = that entry's own click. This is
        // the pinned menu's normal path (its windows are interactive): picking it
        // here as well would run the action twice and, because this branch ends in
        // hideMenu(), close the whole stack - which is exactly what a third-level
        // click used to do while the second level stayed open for its own clicks.
        //
        // The press record is *not* cleared here: one physical event is delivered
        // through several notify() calls (window -> page -> button), so this filter
        // runs more than once for the same release. Clearing it in the first call
        // let the second call fall through and close everything. It is replaced by
        // the next press and dropped when the menus go away.
        if (m_pressMenu && m_pressMenu->isVisible()
            && m_pressMenu->frameGeometry().contains(mouse->globalPosition().toPoint()))
            break;

        SecondaryMenu* top = deepestMenu();
        if (!top || !top->isVisible())
            break;
        const QPoint global = mouse->globalPosition().toPoint();
        if (!top->frameGeometry().contains(global))
            break;
        const int index = top->itemAt(global);
        if (index < 0)
            break;
        // Only a real entry counts - a release on the scroll bar or the frame
        // must not pick whatever entry happens to be behind it.
        if (!qobject_cast<QAbstractButton*>(top->childAt(top->mapFromGlobal(global))))
            break;
        const SecondaryAction* action = top->action(top->itemId(index));
        if (!action)
            break;
        // The press that opened this level still owns the mouse (it happened on
        // the level above), so the release is the only chance to pick the entry
        // under the cursor. Consume it: the entry's own clicked() must not fire
        // a second time.
        top->setHoverIndex(-1);
        activateAction(*action, true, true, top);
        m_rightClickUsed = false;
        hideMenu();
        return true;
    }
    case QEvent::KeyPress:
        if (static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape) {
            // Escape walks back one level at a time: the deepest menu closes
            // first, and only then the one that has nothing below it.
            if (!m_children.isEmpty()) {
                SecondaryMenu* deep = m_children.takeLast();
                deep->dismiss();
                deep->deleteLater();
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
