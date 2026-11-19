#include "secondarybutton.h"

#include "tetag.h"
#include "tetaglistwidget.h"

#include <QApplication>
#include <QKeyEvent>
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

const CaptionLayout& SecondaryButton::captionLayout() const
{
    if (m_layoutWidth != width() || m_layoutCaption != m_caption) {
        m_layoutWidth = width();
        m_layoutCaption = m_caption;
        const QStringList tags = capturedTags();
        m_layout = layoutCaption(tags.isEmpty() ? QStringList{m_caption} : tags,
                                 fontMetrics(), width() - 2 * kTextInset, m_maxLines);
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
        if (const SecondaryAction* action = menu->action(id))
            activateAction(*action, true, m_mode == Mode::Pinned);
    });
    connect(menu, &SecondaryMenu::dismissRequested, this, [this] { hideMenu(); });
    connect(menu, &SecondaryMenu::blankAreaPressed, this, [this] { hideSubmenu(); });
    connect(menu, &SecondaryMenu::actionTriggered, this, [this, menu](const QString& id, bool) {
        // Several aliases may be on together, so this toggles like every other
        // additive entry. A pinned menu survives the click, a dragged one closes.
        if (const SecondaryAction* action = menu->action(id))
            activateAction(*action, true, m_mode == Mode::Pinned);
    });
    m_submenu = menu;
    return menu;
}

bool SecondaryButton::openPreview(bool right)
{
    SecondaryMenu* menu = ensureMenu(right);
    if (!menu)
        return false;
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

bool SecondaryButton::entryCaptured(const Action& action) const
{
    const QStringList captured = capturedTags();
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
                taglistwidget->tagInsertAbove(false, std::make_shared<teTag>(text));
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
    const QStringList captured = capturedTags();
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
    const QString caption = tags.isEmpty() ? m_placeholder : tags.join(QStringLiteral(", "));
    if (caption != m_caption) {
        m_caption = caption;
        // text() stays truthful (accessibility, tooltips, the regression check);
        // paintEvent() is what wraps and elides it to the width we were given.
        QPushButton::setText(m_caption);
        m_layoutWidth = -1;
        updateGeometry();
        update();
    }
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
    m_caption = m_placeholder;
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
            taglistwidget->tagInsertAbove(false, std::make_shared<teTag>(text));
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
                taglistwidget->tagInsertAbove(false, std::make_shared<teTag>(text));
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
    // SC-05: a right press opens the second menu when the button declares one;
    // without one it keeps its older job (the first menu, then its third level).
    if (openPreview(right && !m_rightMenuPath.isEmpty())) {
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

void SecondaryButton::mouseMoveEvent(QMouseEvent* event)
{
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
