#include "secondarymenu.h"

#include "testyle.h"

#include <QAbstractButton>
#include <QFile>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QScrollArea>
#include <QScreen>
#include <QStyle>
#include <QUiLoader>
#include <QVBoxLayout>

#include <algorithm>

#include "fonts.h"      // appFontFamily(): the face every emoji falls back from

namespace {
/// The gap the menu keeps from the screen edges (SC-03).
constexpr int kScreenMargin = 4;
/// 0.8 x the screen: above that the menu scrolls instead of growing (SC-06).
constexpr double kMaxScreenFraction = 0.8;
/// A menu may exceed that by this much rather than scroll for a few rounded pixels.
constexpr int kRoundingSlack = 24;
/// Height reserved under the last row so a fitted menu never scrolls for a pixel.
constexpr int kBottomSlack = 10;
/// LiteWidget's own 1px border, on both sides.
constexpr int kBorder = 1;

const char* const kActionPrefixes[] = {"btn_", "chk_"};
} // namespace

SecondaryMenu::SecondaryMenu(QWidget* parent)
    : LiteWidget(parent)
{
    // SC-02: the menu never takes the keyboard focus, in either state. Anything
    // that needs a key (Esc) is handled by the button that owns the menu.
    setNoActivate(true);
    setWindowFlag(Qt::WindowDoesNotAcceptFocus, true);
    setAttribute(Qt::WA_ShowWithoutActivating, true);
    // Same palette as the editor page the menu pops out of, so a menu does not
    // look like a different program. The rules are class based (QPushButton,
    // SecondaryButton, ...), so nothing here depends on this window.
    setStyleSheet(teStyle::sheet(QStringLiteral("controls")));

    m_scroll = new QScrollArea(this);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setWidgetResizable(true);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    // border:none is not cosmetic: controls.qss draws a 1px border on every
    // QScrollArea, which took two pixels off the viewport - and two pixels are
    // enough for the page not to fit any more, which is where the scrolling
    // scrollbars on menus that fit came from.
    m_scroll->setStyleSheet(QStringLiteral("QScrollArea{background:transparent;border:none;}"));
    if (auto* layout = contentLayout())
        layout->addWidget(m_scroll);
}

bool SecondaryMenu::loadUi(const QString& path)
{
    if (path.isEmpty() || m_page)
        return m_page != nullptr;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        telog(QStringLiteral("[secondary menu] cannot open %1").arg(path));
        return false;
    }

    QUiLoader loader;
    QWidget* page = loader.load(&file, nullptr);
    file.close();
    if (!page) {
        telog(QStringLiteral("[secondary menu] %1 is not a usable ui file").arg(path));
        return false;
    }

    m_page = page;
    m_uiPath = path;
    m_page->setParent(m_scroll);
    m_scroll->setWidget(m_page);

    // Entries show an emoji in front of their word (the clothes menus do). The
    // embedded app face has no emoji glyphs, so the emoji family is named behind
    // it here - a .ui file cannot spell out a family list.
    for (QAbstractButton* button : m_page->findChildren<QAbstractButton*>()) {
        QFont font = button->font();
        font.setFamilies({appFontFamily(), QStringLiteral("Segoe UI Emoji")});
        button->setFont(font);
    }

    m_actions.clear();
    collectActions(m_page, m_actions);
    for (const SecondaryAction& action : m_actions) {
        QAbstractButton* button = buttonFor(action.id);
        if (!button)
            continue;
        // Entries that open a level below wear the add widget's secondary-button
        // language, so "this opens something" is visible before hovering it:
        //   · owns a tag  -> activatable (solid border, no fill: the entry itself
        //     writes the group's base tag);
        //   · no tag      -> pure (dotted border, faint green: a pure launcher).
        // A plain entry keeps the list's purple border: it writes a tag.
        if (!action.submenuPath.isEmpty()) {
            button->setProperty("entry", action.tags.isEmpty()
                                             ? QStringLiteral("pure")
                                             : QStringLiteral("activatable"));
            // The page was parented before this property existed, so the button is
            // already polished: without a repolish the sheet keeps the purple
            // border it resolved at creation (trap looked like a plain entry).
            button->style()->unpolish(button);
            button->style()->polish(button);
        }
        if (button->isCheckable()) {
            connect(button, &QAbstractButton::clicked, this, [this, button] {
                emit actionTriggered(idOf(button), button->isChecked());
            });
        } else {
            connect(button, &QAbstractButton::clicked, this, [this, button] {
                emit actionTriggered(idOf(button), false);
            });
        }
    }

    adjustSize();
    return true;
}

void SecondaryMenu::collectActions(QWidget* page, QVector<SecondaryAction>& out)
{
    // Only buttons are actions. Everything else the designer put in (group
    // captions, separators, a label with an explanation) is drawn but never
    // translated into a tag action - that is SC-11's "only the controls that can
    // produce a tag action take part".
    for (QAbstractButton* button : page->findChildren<QAbstractButton*>()) {
        if (!button || button->objectName().isEmpty())
            continue;
        const QString id = idOf(button);
        if (id.isEmpty())
            continue;
        SecondaryAction action;
        action.id = id;
        const QString tags = button->property("tags").toString();
        if (!tags.isEmpty())
            action.tags = tags.split(QLatin1Char('|'), Qt::SkipEmptyParts);
        action.group = button->property("group").toString();
        action.submenuPath = button->property("submenu").toString();
        const QString implies = button->property("implies").toString();
        if (!implies.isEmpty())
            action.implies = implies.split(QLatin1Char('|'), Qt::SkipEmptyParts);
        action.label = button->text();
        action.checkable = button->isCheckable();
        out.push_back(action);
    }
}

QVector<SecondaryAction> SecondaryMenu::readActions(const QString& path)
{
    QVector<SecondaryAction> result;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return result;
    QUiLoader loader;
    QWidget* page = loader.load(&file, nullptr);
    file.close();
    if (!page)
        return result;
    collectActions(page, result);
    delete page;
    return result;
}

const SecondaryAction* SecondaryMenu::action(const QString& id) const
{
    for (const SecondaryAction& action : m_actions)
        if (action.id == id)
            return &action;
    return nullptr;
}

QVector<QAbstractButton*> SecondaryMenu::actionButtons() const
{
    QVector<QAbstractButton*> result;
    if (!m_page)
        return result;
    for (QAbstractButton* button : m_page->findChildren<QAbstractButton*>()) {
        if (!button || button->objectName().isEmpty())
            continue;
        for (const char* prefix : kActionPrefixes) {
            if (button->objectName().startsWith(QLatin1String(prefix))) {
                result.push_back(button);
                break;
            }
        }
    }
    return result;
}

QString SecondaryMenu::idOf(const QAbstractButton* button)
{
    if (!button)
        return QString();
    const QString name = button->objectName();
    for (const char* prefix : kActionPrefixes) {
        const QLatin1String p(prefix);
        if (name.startsWith(p))
            return name.mid(p.size());
    }
    return QString();
}

QAbstractButton* SecondaryMenu::buttonFor(const QString& id) const
{
    for (QAbstractButton* button : actionButtons())
        if (idOf(button) == id)
            return button;
    return nullptr;
}

QVector<SecondaryMenu::Item> SecondaryMenu::items() const
{
    QVector<Item> result;
    for (QAbstractButton* button : actionButtons())
        if (!button->isHidden())
            result.push_back({idOf(button), QRect(button->mapToGlobal(QPoint(0, 0)), button->size())});

    // The designer's grid order is not the order findChildren() reports, and the
    // order matters (it is the order the cursor walks the entries). Geometry is
    // the only thing both the drawing and the user agree on.
    std::stable_sort(result.begin(), result.end(), [](const Item& left, const Item& right) {
        if (left.rect.top() != right.rect.top())
            return left.rect.top() < right.rect.top();
        return left.rect.left() < right.rect.left();
    });
    return result;
}

QString SecondaryMenu::itemId(int index) const
{
    const QVector<Item> all = items();
    if (index < 0 || index >= all.size())
        return QString();
    return all[index].id;
}

QRect SecondaryMenu::itemRect(const QString& id) const
{
    for (const Item& item : items())
        if (item.id == id)
            return item.rect;
    return QRect();
}

int SecondaryMenu::itemAt(const QPoint& globalPos) const
{
    const QVector<Item> all = items();
    QVector<QRect> rects;
    rects.reserve(all.size());
    for (const Item& item : all)
        rects.push_back(item.rect);
    return hitTest(rects, globalPos);
}

int SecondaryMenu::hitTest(const QVector<QRect>& rects, const QPoint& globalPos)
{
    for (int i = 0; i < rects.size(); ++i)
        if (rects[i].contains(globalPos))
            return i;
    return -1;
}

bool SecondaryMenu::hasEntry(const QString& id) const
{
    return buttonFor(id) != nullptr;
}

QString SecondaryMenu::entryLabel(const QString& id) const
{
    for (const SecondaryAction& action : m_actions)
        if (action.id == id)
            return action.label;
    return QString();
}

QString SecondaryMenu::entryText(const QString& id) const
{
    const QAbstractButton* button = buttonFor(id);
    return button ? button->text() : QString();
}

void SecondaryMenu::setEntryText(const QString& id, const QString& text)
{
    if (QAbstractButton* button = buttonFor(id)) {
        if (!text.isEmpty() && button->text() != text)
            button->setText(text);
    }
}

bool SecondaryMenu::entryIsCheckable(const QString& id) const
{
    const QAbstractButton* button = buttonFor(id);
    return button && button->isCheckable();
}

void SecondaryMenu::setEntryVisible(const QString& id, bool visible)
{
    if (QAbstractButton* button = buttonFor(id))
        button->setVisible(visible);
}

void SecondaryMenu::setPreview(bool on)
{
    m_preview = on;
    if (m_preview)
        setHoverIndex(-1);

    // SC-02 asks for WA_TransparentForMouseEvents. On a top-level window that
    // attribute alone does not make the native window click-through - Qt needs
    // Qt::WindowTransparentForInput for that, and setting a window flag re-creates
    // the HWND, i.e. a visible flash on every hover. WS_EX_TRANSPARENT is the same
    // thing one layer down and can be toggled on the live window, so the attribute
    // is set for the documented meaning and the ex-style does the work.
    setAttribute(Qt::WA_TransparentForMouseEvents, on);
    const HWND hwnd = reinterpret_cast<HWND>(winId());
    LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    if (on)
        style |= WS_EX_TRANSPARENT;
    else
        style &= ~WS_EX_TRANSPARENT;
    SetWindowLongPtrW(hwnd, GWL_EXSTYLE, style);

    setWindowOpacity(on ? 0.85 : 1.0);
    update();
}

void SecondaryMenu::setHoverIndex(int index)
{
    const QVector<QAbstractButton*> buttons = actionButtons();
    const QVector<Item> all = items();
    if (index < 0 || index >= all.size()) {
        for (QAbstractButton* button : buttons)
            button->setDown(false);
        return;
    }
    const QString wanted = all[index].id;
    for (QAbstractButton* button : buttons)
        // setDown() is the "pressed" state the sheet already styles: it is the
        // highlight the drag needs, and unlike a property it costs no repolish on
        // every mouse move.
        button->setDown(idOf(button) == wanted);
}

void SecondaryMenu::syncFromCaptured(const QStringList& capturedIds)
{
    // The menu is the view: the owner (SecondaryButton) decides which entry stands
    // for the current capture, because the tags live in the button's action table
    // and not in the .ui. Here that decision is only applied.
    for (QAbstractButton* button : actionButtons()) {
        const bool on = capturedIds.contains(idOf(button));
        if (button->isCheckable()) {
            if (button->isChecked() == on)
                continue;
            QSignalBlocker blocker(button);
            button->setChecked(on);
        } else {
            setFlag(button, "menuSelected", on);
        }
    }
}

void SecondaryMenu::setFlag(QAbstractButton* button, const char* name, bool on)
{
    if (!button)
        return;
    if (button->property(name).toBool() == on)
        return;
    button->setProperty(name, on);
    // No setStyleSheet(): the rules are in controls.qss and only the property
    // selector changes. A repolish re-resolves this one widget's rules, it does
    // not re-parse the sheet.
    button->style()->unpolish(button);
    button->style()->polish(button);
    button->update();
}

QPoint SecondaryMenu::placeMenu(const QRect& anchor, const QSize& size, const QRect& available)
{
    // Window controls and text buttons are narrow, menus are wide: the menu is
    // centred on the button and only falls back to the button's left edge when
    // centring would leave the screen (then it grows to the right, which is the
    // direction the eye expects).
    int x = anchor.center().x() - size.width() / 2;
    int y = anchor.bottom() + 1;

    // Not enough room below: flip above. If neither side fits, the vertical
    // clamp wins - the menu is already capped at 0.8 of the screen.
    if (y + size.height() > available.bottom() - kScreenMargin + 1)
        y = anchor.top() - size.height();
    if (y < available.top() + kScreenMargin)
        y = available.top() + kScreenMargin;

    const int rightLimit = available.right() - kScreenMargin + 1;
    if (x + size.width() > rightLimit || x < available.left() + kScreenMargin)
        x = anchor.left();                  // left-aligned, extending right
    if (x + size.width() > rightLimit)
        x = rightLimit - size.width();      // still too wide: clamp
    if (x < available.left() + kScreenMargin)
        x = available.left() + kScreenMargin;

    return {x, y};
}

void SecondaryMenu::showAt(const QRect& anchor)
{
    QScreen* screen = QGuiApplication::screenAt(anchor.center());
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    if (!screen)
        return;
    const QRect available = screen->availableGeometry();
    const int maxHeight = int(available.height() * kMaxScreenFraction) + kRoundingSlack;

    // First from the page's hint as it stands, then - once the window is up and the
    // page has been laid out at its real width - from the hint it reports then. The
    // two differ by a few pixels in both directions (the grid's hint is computed on
    // the geometry it has, and QSS borders are only counted after polishing), and
    // that difference is exactly what used to make every menu scroll a little.
    QSize wanted = sizeHint();
    wanted.setWidth(qMax(wanted.width(), 60));
    wanted.setHeight(qMin(wanted.height(), maxHeight));
    resize(wanted);
    move(placeMenu(anchor, wanted, available));
    show();
    raise();

    // Once the window exists the page has been laid out at its real width, and its
    // hint is what the content actually needs. Pin that as the page's minimum size:
    // QScrollArea keeps the page at its minimum, so with the window sized to the
    // same number the scrollbars have nothing to scroll - which is the whole reason
    // every menu used to grow a two pixel scrollbar it could never move.
    if (m_page) {
        const QSize need = m_page->sizeHint().expandedTo(m_page->minimumSizeHint());
        m_page->setMinimumSize(need);

        QSize window = need + QSize(2 * kBorder, 2 * kBorder);
        window.setWidth(qMin(window.width(), available.width() - 2 * kScreenMargin));
        window.setHeight(qMin(window.height(), maxHeight));
        if (window != size()) {
            resize(window);
            move(placeMenu(anchor, window, available));
        }

        if (m_scroll) {
            // Content taller than the screen keeps its scrollbar; everything else
            // must not show one (a few stray pixels are layout rounding).
            const bool scrolls = need.height() > m_scroll->viewport()->height();
            m_scroll->setVerticalScrollBarPolicy(scrolls ? Qt::ScrollBarAsNeeded
                                                         : Qt::ScrollBarAlwaysOff);
        }
    }
}

void SecondaryMenu::dismiss()
{
    setHoverIndex(-1);
    if (m_preview)
        setPreview(false);
    hide();
}

QSize SecondaryMenu::sizeHint() const
{
    if (!m_page)
        return LiteWidget::sizeHint();
    // The page's own hint plus LiteWidget's border. Asking the layout chain would
    // add QScrollArea's scrollbar reserve on top of every menu.
    return m_page->sizeHint() + QSize(2 * kBorder, 2 * kBorder);
}

void SecondaryMenu::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::RightButton) {
        // Right-clicking an entry is the "show me its other level without doing
        // anything" gesture (the trap aliases); on empty space it is nothing, and
        // the button closes the menu through its own press handler.
        const int index = itemAt(event->globalPosition().toPoint());
        if (index >= 0) {
            emit entryRightPressed(itemId(index));
        } else {
            // Right-clicking the empty part of a menu is one of the ways it closes.
            emit dismissRequested();
        }
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton
        && itemAt(event->globalPosition().toPoint()) < 0) {
        emit blankAreaPressed();
    }
    LiteWidget::mousePressEvent(event);
}
