#ifndef SECONDARYBUTTON_H
#define SECONDARYBUTTON_H

#include "secondarycaption.h"
#include "secondarymenu.h"
#include "teeditorcontrol.h"

#include <QPoint>
#include <QStringList>
#include <QVector>

/**
 * @brief A button that owns a popup menu, instead of a combo box (SC-01..SC-11).
 *
 * Why not a combo box: the menus are not lists of strings, they are designer
 * files with buttons laid out by hand, and a combo box can only ever show a
 * column of items. A SecondaryButton is a normal button that happens to drop a
 * .ui file under itself.
 *
 * Everything about a menu lives in that file: the labels and the positions are
 * ordinary designer properties, and the meaning of an entry is three dynamic
 * properties on the entry itself (`tags` / `group` / `submenu`, see
 * SecondaryAction). The class below therefore has no table of its own - point it
 * at a file and it knows what the menu can do:
 *
 *  - a plain button (btn_*) replaces the capture set with its own tags;
 *  - a checkable button (chk_*, checkable=true) toggles its tags in or out, which
 *    is what an additive menu (faces, character traits, positions) needs;
 *  - an entry with `submenu` opens a third-level menu instead of ending the
 *    interaction (`trap` and its aliases are the case this exists for);
 *  - entries sharing a `group` exclude each other but leave the rest of the
 *    capture alone.
 *
 * Capture (what the button shows): the tags it claimed, in reading order, wrapped
 * into as many lines as the width allows (default 1, the faces button uses 2),
 * with a middle ellipsis when even that is not enough.
 */
class SecondaryButton : public QPushButton, public EditorControl
{
    Q_OBJECT
public:
    using Action = SecondaryAction;

    SecondaryButton(const QString& menuPath, QString placeholder, QWidget* parent = nullptr);
    ~SecondaryButton() override;

    /**
     * @brief The second menu (SC-05): what a right press shows.
     *
     * A left press (and the hover preview) always shows the menu the constructor
     * was given; a right press shows this one instead. The rest of the rules are
     * shared: the same drag, the same "release over the button pins it", the same
     * close-on-outside-click. When this is left empty the right button keeps its
     * older job - working the third level of the first menu.
     */
    void setRightMenuPath(const QString& path);
    QString rightMenuPath() const { return m_rightMenuPath; }

    /// How many lines the caption may use. 1 by default; the faces button uses 2.
    void setMaxCaptionLines(int lines);
    int maxCaptionLines() const { return m_maxLines; }

    /// Every captured tag, separated by ", " (the placeholder when there is none).
    QString caption() const { return m_caption; }
    /// The same text broken into the lines that are actually drawn.
    const CaptionLayout& captionLayout() const;
    /// What it shows when nothing is captured.
    const QString& placeholder() const { return m_placeholder; }
    /// The entries of the menu plus those of its third-level menus.
    const QVector<Action>& actions() const { return m_actions; }
    QString menuPath() const { return m_menuPath; }

    bool filter(std::shared_ptr<teTag> tag) const override;
    void clear() override;
    void reset() override;
    void refreshState() override;

    /// Writes the tag list of one entry - the same thing the combo box did on a
    /// selection change: retire what the entry does not contain, insert the rest.
    /// Public so the regression check can drive it without a mouse.
    void applyAction(const QString& id);
    /// Adds or removes one entry's tags (a checkable entry in the menu).
    void toggleAction(const QString& id, bool on);
    /**
     * @brief What a left click on an entry does - one place, so every gesture agrees.
     *
     * Additive entries toggle from the *capture*, not from the widget's own check
     * state: the same entry is clicked through the menu, through a drag and through
     * the third level, and only the capture knows whether it is currently on.
     * A left click never opens a third level: that is what the right button is for.
     *
     * `keepOpen` is true for a pinned menu (it stays up) and false for a drag that
     * ends on the entry. The Action is passed by value because using an entry of the
     * third level may delete that window.
     */
    void activateAction(SecondaryAction action, bool fromSubmenu, bool keepOpen);
    /// Same, by id - for code that has no menu at hand (tests, programmatic use).
    void activate(const QString& id, bool keepOpen = false);

    /// The menu that is on screen, if any.
    SecondaryMenu* menu() const { return m_active; }
    /// The third-level menu that is on screen, if any.
    SecondaryMenu* submenu() const { return m_submenu; }

protected:
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void wheelEvent(QWheelEvent* event) override { event->ignore(); }
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    enum class Mode { Hidden, Preview, Pinned };

    SecondaryMenu* ensureMenu(bool right);
    SecondaryMenu* ensureSubmenu(const Action& action);
    /// Shows the menu as a click-through preview under the button. `right` picks
    /// the second menu (SC-05), which is what a right press opens.
    bool openPreview(bool right = false);
    /// Opens the third-level menu of `action`, anchored at its entry.
    bool openSubmenu(const Action& action);
    /// Turns the preview that is on screen into the pinned one.
    void pinActive();
    void pin();
    void hideMenu();
    void hideSubmenu();
    void syncMenuSelection();
    QStringList capturedTags() const;
    void rebuildCaption();
    const Action* findAction(const QString& id) const;
    /// Is every tag of this entry in the capture?
    bool entryCaptured(const Action& action) const;
    /// Is any member of this entry's display family captured?
    bool familyCaptured(const Action& parent) const;
    /// Captures the parent's tags, or releases every captured family member.
    void setFamily(const Action& parent, bool on);
    /// A left click on a family entry: toggle the whole family, and show the third
    /// level when it just came on.
    void activateFamilyEntry(const Action& parent, bool keepOpen);
    /// A press that lands inside one of the menus (the preview is click-through, so
    /// those arrive at the button).
    void handleMenuPress(const QPoint& global, bool right);
    /// Right click on a family entry: show its third level, or put it away again.
    void toggleSubmenuFor(const Action& parent);
    void setCaptured(bool captured);
    void loadActions();
    /// The entries of the menu that is on screen (the first one when none is).
    const QVector<Action>& activeActions() const;

    QString m_menuPath;
    QString m_rightMenuPath;
    QString m_placeholder;
    QString m_caption;
    int m_maxLines = 1;
    /// The entries of the first menu and of every third-level menu it declares.
    QVector<Action> m_actions;
    /// The same for the second menu (SC-05).
    QVector<Action> m_rightActions;

    SecondaryMenu* m_menu = nullptr;
    /// The second menu, built on the first right press (SC-05).
    SecondaryMenu* m_rightMenu = nullptr;
    /// The menu that is on screen.
    SecondaryMenu* m_active = nullptr;
    /// The third-level menu that is on screen, when an entry was opened.
    SecondaryMenu* m_submenu = nullptr;

    mutable CaptionLayout m_layout;
    mutable int m_layoutWidth = -1;
    mutable QString m_layoutCaption;

    Mode m_mode = Mode::Hidden;
    bool m_dragging = false;
    bool m_captured = false;
    /// A right click already used this interaction to switch an entry. The left
    /// release that follows must then not switch a second one (the pointer is often
    /// still resting on a menu entry when the third level appears).
    bool m_rightClickUsed = false;
};

#endif // SECONDARYBUTTON_H
