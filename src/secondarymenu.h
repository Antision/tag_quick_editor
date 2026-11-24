#ifndef SECONDARYMENU_H
#define SECONDARYMENU_H

#include "litewidget.h"

#include <QPoint>
#include <QRect>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QVector>

class QAbstractButton;
class QScrollArea;

/**
 * @brief One entry of a secondary menu, as declared in the .ui file.
 *
 * The .ui is the only source of truth: the label and the position are ordinary
 * designer properties, and what the entry *means* is three dynamic properties on
 * the same widget:
 *
 *   tags     "masterpiece|best quality"   the tags this entry writes
 *   group    "trap"                       entries sharing it exclude each other
 *   submenu  ":/secondarymenu/trap_menu.ui"  opens a third level instead
 *
 * Writing the table in the file instead of in C++ is what keeps SC-10 honest:
 * a menu can be re-laid-out, renamed or extended in Designer, and the button
 * learns the new entries by reading the file it was already pointed at.
 */
struct SecondaryAction {
    QString id;
    QStringList tags;
    QString submenuPath;
    /// Display family only (the trap aliases): the entry that opens the submenu
    /// borrows the label of whichever member is captured, and is highlighted when
    /// any member is. It no longer means "exclusive" - these are multi-select.
    QString group;
    /// "This entry and everything in its submenu are one family" (`family="true"`
    /// in the .ui). The body-part entries use it without a group: `stomach` and
    /// its variants (`navel`, `belly`, ...) belong together for the label and the
    /// on/off switch, but they are independent tags with no exclusive group.
    bool family = false;
    /// The .ui file this action was read from. Filled by readActions(); it is how
    /// an entry knows which submenu entries are its own descendants.
    QString menuPath;
    /// Added when this entry is switched on, never removed by switching it off
    /// ("mosaic censoring" implies "censored"; the user may still drop `censored`
    /// on its own).
    QStringList implies;
    /// The label as the designer wrote it (the entry may be relabelled at runtime).
    QString label;
    /// A checkable button toggles its tags; a plain one replaces the capture.
    bool checkable = false;
};

/**
 * @brief The popup a SecondaryButton shows: a LiteWidget with a .ui file inside.
 *
 * The layout is *not* built here (SC-10). Every menu is a designer file under
 * res/secondarymenu/, compiled into the resources and loaded at runtime by
 * name, so adding an entry or moving one is a change in Designer and nothing
 * else. This class only knows four things:
 *
 *  - which child is an action: a button whose object name starts with `btn_`
 *    (or `chk_`); the rest of the name is the action id (SC-11),
 *  - where each action is on screen, which is also what hit-testing uses - the
 *    two cannot disagree because there is one source for both (see items()),
 *  - the two presentation states: Preview (hover, mouse-transparent, 0.85) and
 *    Pinned (interactive, opaque) - SC-02,
 *  - how the current capture looks inside the menu: a checkable entry is checked,
 *    a plain entry is highlighted when its tag list is exactly the capture - the
 *    same rule for both levels of menu.
 *
 * The mouse never reaches a Preview menu, so dragging from the button across the
 * entries is the button's job: it asks itemAt() with the global cursor position
 * and highlights the entry it lands on (SC-04).
 */
class SecondaryMenu : public LiteWidget
{
    Q_OBJECT
public:
    /// One action as the menu sees it: its id and where it is drawn.
    struct Item {
        QString id;
        QRect rect;                     ///< global coordinates
    };

    explicit SecondaryMenu(QWidget* parent = nullptr);

    /// Loads a .ui (a resource path) as the menu's content. False when it cannot
    /// be read - the button then simply has no menu instead of an empty box.
    bool loadUi(const QString& path);
    bool isLoaded() const { return m_page != nullptr; }
    QString uiPath() const { return m_uiPath; }
    /// The entries declared by the loaded file, in designer order.
    const QVector<SecondaryAction>& actions() const { return m_actions; }
    const SecondaryAction* action(const QString& id) const;
    /// The same reading over a file that is not (yet) a window: the button needs
    /// the tag list of its third-level menus before anything is shown.
    static QVector<SecondaryAction> readActions(const QString& path);
    static void collectActions(QWidget* page, QVector<SecondaryAction>& out);

    /// The action entries, in the order the designer laid them out.
    QVector<Item> items() const;
    /// The id of entry `index`, empty when out of range.
    QString itemId(int index) const;
    /// The global rectangle of one entry; a null rect when there is no such entry.
    QRect itemRect(const QString& id) const;
    /// The entry under a global position, -1 when the position is in a gap.
    int itemAt(const QPoint& globalPos) const;
    /**
     * @brief The hit rule itself, as a pure function.
     *
     * itemAt() is this function over items(); the regression check uses the same
     * one, so "the rect that is drawn is the rect that is hit" is a property that
     * can be asserted without a mouse.
     */
    static int hitTest(const QVector<QRect>& rects, const QPoint& globalPos);

    bool hasEntry(const QString& id) const;
    /// The label declared in the .ui, even after setEntryText() changed it.
    QString entryLabel(const QString& id) const;
    /// The label the entry shows right now (the owner may have relabelled it).
    QString entryText(const QString& id) const;
    /// Relabels one entry (the trap entry shows the captured alias' name).
    void setEntryText(const QString& id, const QString& text);
    /// A checkable entry toggles its tags; a plain one replaces the capture.
    bool entryIsCheckable(const QString& id) const;
    /// Hides one entry (the alias that is already captured is not offered twice).
    void setEntryVisible(const QString& id, bool visible);

    /// Preview: click-through and 0.85. Pinned: interactive and opaque.
    void setPreview(bool on);
    bool isPreview() const { return m_preview; }

    /// Highlights the entry the cursor is over while a drag is in progress.
    void setHoverIndex(int index);
    /// Marks the entries that stand for the current capture (SC-11). The owner
    /// passes the ids it decided on: the tags live in its action table, not here.
    void syncFromCaptured(const QStringList& capturedIds);

    /// Puts the menu below (or above) `anchor` and shows it. SC-03.
    void showAt(const QRect& anchor);
    /// Where that is; pure, so the flip and the clamping can be asserted.
    static QPoint placeMenu(const QRect& anchor, const QSize& size, const QRect& available);

    /// Hides the menu and forgets its transient states.
    void dismiss();

signals:
    /// An action entry was clicked. `checked` is the new state of a checkable one.
    void actionTriggered(const QString& id, bool checked);
    /// The right button was pressed on an entry (opens a third-level menu).
    void entryRightPressed(const QString& id);
    /// The right button was pressed on empty space (SC-02: that closes a pinned menu).
    void dismissRequested();
    /// The left button was pressed on empty space - nothing to run, but the level
    /// above/behind the cursor is no longer in use.
    void blankAreaPressed();

protected:
    QSize sizeHint() const override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    QVector<QAbstractButton*> actionButtons() const;
    QAbstractButton* buttonFor(const QString& id) const;
    static QString idOf(const QAbstractButton* button);
    static void setFlag(QAbstractButton* button, const char* name, bool on);

    QWidget* m_page = nullptr;
    QScrollArea* m_scroll = nullptr;
    QString m_uiPath;
    QVector<SecondaryAction> m_actions;
    bool m_preview = false;
};

#endif // SECONDARYMENU_H
