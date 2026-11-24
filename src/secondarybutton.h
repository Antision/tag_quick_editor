#ifndef SECONDARYBUTTON_H
#define SECONDARYBUTTON_H

#include "secondarycaption.h"
#include "secondarymenu.h"
#include "teeditorcontrol.h"

#include <QPoint>
#include <QStringList>
#include <QVector>

#include <functional>
#include <memory>

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

    /**
     * @brief How a tag is built from one entry's text.
     *
     * Defaults to `teTag(text)`, which splits on spaces. The clothes editor needs
     * a different rule: "school uniform" is *one* type word there, not the words
     * "school" and "uniform" (§1.2 of docs/clothes_add_widget_v2.md), so it hands
     * in clothesTagFromText() and every entry keeps its word count.
     */
    void setTagBuilder(std::function<std::shared_ptr<teTag>(const QString&)> builder);

    /**
     * @brief The tag this button writes when it is clicked itself.
     *
     * Empty means "menu only": the button is a pure launcher, and a click pins
     * the menu (the older behaviour of every SecondaryButton). With a tag it is an
     * activatable button: a plain click selects (or, outside the add widget,
     * writes/removes) that tag, exactly like the entry named after it in its own
     * menu; the menu stays reachable by hovering, by dragging onto an entry, or by
     * a right click.
     *
     * The distinction is what the clothes dialog shows as two styles: a solid
     * yellow button is a type, a dashed one only opens a menu.
     */
    void setOwnTag(const QString& tag);
    QString ownTag() const { return m_ownTag; }

    /**
     * @brief Preview this button's menu under an arbitrary rectangle.
     *
     * The reftag list uses it: hovering a tag whose type this button stands for
     * shows the same menu the button would, anchored at the tag instead of at the
     * button (SC-02's hover preview, just with another anchor).
     *
     * `tag` is the tag the menu was opened at. It makes the menu edit *that* tag
     * instead of this button's capture list (§1.13): a coloured spelling such as
     * `red collar` is not in the collar menu's tag list, so the capture route
     * wrote the picked spelling as a second tag next to it. Empty means "no
     * tag" - the menu then behaves exactly like the button's own.
     */
    void showPreviewAt(const QRect& anchor, std::shared_ptr<teTag> tag = {});
    /// Solidify the preview that is on screen (a click on the reftag).
    void pinShownMenu();
    /// Close the menu if this button is the one showing it.
    void dismissMenu();
    /// True while the menu is the click-through hover preview (not yet pinned).
    bool previewVisible() const;

    /**
     * @brief How a menu entry rewrites the tag the preview was opened at.
     *
     * The editor this button's menu edits from the tag list decides what "give
     * this tag that type" means (`red collar` + `sailor collar` ->
     * `red sailor collar`); the button only knows the gesture, and which
     * spelling of the tag its own menu stands for (`family`: `glowing halo` on
     * `pink glowing halo`). Without a sink the anchored menu falls back to the
     * capture semantics.
     */
    void setTagRewrite(std::function<bool(std::shared_ptr<teTag>, const QString&, const QStringList&)> sink);

    /**
     * @brief The add widget's rule: light up for the tag being edited, not for the
     * tag list.
     *
     * The add widget composes one tag; what else happens to be in the list is none
     * of its business. With palette mode on, `captured` comes from
     * setCaptureFromTag() only, and linking tags (which the clothes tag list does
     * for every garment) no longer paints the button.
     */
    void setPaletteMode(bool on);
    /// Light this button when the add widget is editing `tagText`.
    void setCaptureFromTag(const QString& tagText);
    /**
     * @brief The family spelling of `tagText` this button stands for, or empty.
     *
     * The add widget edits one tag and lights the button that owns its *type*,
     * not only the exact word: `red pleated skirt` belongs to the `skirt` button,
     * `blue sailor collar` to `collar`. The longest suffix of the tag that is the
     * button's own word or one of its menu entries wins, so `pleated skirt` is
     * recognised as the entry it is instead of being reduced to `skirt`.
     */
    QString paletteTagFor(const QString& tagText) const;
    /// What setCaptureFromTag() last resolved - the family spelling, not the
    /// whole tag. Empty when the tag is none of this button's business.
    QString paletteTag() const { return m_paletteTag; }

    /**
     * @brief Where a choice in the palette goes, instead of the tag list.
     *
     * The add widget composes one tag: clicking a type button or a menu entry
     * selects that spelling for the tag being built, and only the `+` button
     * writes it. The sink is what `+` later reads (colorsWidget owns the
     * composition). `toggle` is true for a click on the button itself, so a
     * second click takes the selection away; menu entries always replace.
     */
    void setComposeSink(std::function<void(const QString& tag, bool toggle)> sink);

    /// "activatable" / "pure" - a style hook, set from setOwnTag()/setKind().
    /**
     * @brief The name this button shows when it has nothing selected.
     *
     * The clothes type buttons are named (`gloves`); a tag list linking a
     * "fingerless gloves" tag must not rename the button, or the row of names
     * disappears. The add widget is the exception: there the *selected spelling*
     * is the label on purpose (`gloves` reads "elbow gloves" once chosen), because
     * that is what `+` will write. Generic launchers never call this and keep the
     * captured tags as their caption.
     */
    void setDisplayName(const QString& name);
    QString displayName() const { return m_displayName; }

    void setKind(const QString& kind);
    QString kind() const { return m_kind; }

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
    void activateAction(SecondaryAction action, bool fromSubmenu, bool keepOpen,
                        SecondaryMenu* owner = nullptr);
    /// Same, by id - for code that has no menu at hand (tests, programmatic use).
    void activate(const QString& id, bool keepOpen = false);

    /// The menu that is on screen, if any.
    SecondaryMenu* menu() const { return m_active; }
    /**
     * @brief The third-level menu that is on screen, if any.
     *
     * A menu file may name another one in an entry's `submenu` property, so the
     * chain does not have to stop at three levels: `m_children` holds them in
     * order, and this is the first. `deepestMenu()` is the one the pointer is
     * most likely over.
     */
    SecondaryMenu* submenu() const { return m_children.value(0); }
    SecondaryMenu* deepestMenu() const { return m_children.value(m_children.size() - 1); }
    /// How many menus hang below the second one (0 when none is open).
    int childMenuCount() const { return m_children.size(); }

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
    /// Shows the menu as a click-through preview under the button. `right` picks
    /// the second menu (SC-05), which is what a right press opens.
    bool openPreview(bool right = false);
    /**
     * @brief The child menu an entry opens, hung off `parent`.
     *
     * `parent` is the menu the entry lives in - the second level when this is the
     * third, a child menu when it is one deeper. Opening a child drops everything
     * that hung below it: the chain is always a single path, never a tree of
     * abandoned windows.
     */
    SecondaryMenu* ensureSubmenu(const Action& action, SecondaryMenu* parent);
    /// Shows that child; `parent` defaults to the second level.
    bool openSubmenu(const Action& action, SecondaryMenu* parent = nullptr);
    /// The menu below `parent`, nullptr when the entry was the deepest one.
    SecondaryMenu* childOf(const SecondaryMenu* parent) const;
    /// Drops every menu below `parent` (level n+1 and deeper).
    void dropChildrenOf(SecondaryMenu* parent);
    /// Turns the preview that is on screen into the pinned one.
    void pinActive();
    void pin();
    void hideMenu();
    /// Drops every child menu (third level and deeper).
    void hideSubmenu();
    void syncMenuSelection();
    QStringList capturedTags() const;
    /// The tags this button answers for: its own word plus every menu entry (both
    /// menu levels). Built lazily; sorted longest first so paletteTagFor() can
    /// return the most specific suffix.
    const QStringList& familyTags() const;
    /// What the menu and the button both treat as "captured": the tag list, or in
    /// palette mode the single tag the add widget was opened for.
    QStringList highlightedTags() const;
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
    void activateFamilyEntry(const Action& parent, bool keepOpen,
                             SecondaryMenu* owner);
    /// A press that lands inside one of the menus (the preview is click-through, so
    /// those arrive at the button).
    void handleMenuPress(const QPoint& global, bool right);
    /**
     * @brief A right press on an entry of any menu level.
     *
     * Two meanings, and neither of them closes the menu:
     *   · the entry opens a level below -> expand / collapse it (show the aliases
     *     without writing anything);
     *   · a plain entry -> it *is* the click (write the tag); the menu stays up so
     *     several entries can be switched in a row.
     */
    void handleMenuRightPress(SecondaryMenu* menu, const QString& id);
    /**
     * @brief A press on the empty part of a menu.
     *
     * With something open below it, that is "put that away" - one level, not the
     * whole stack. Otherwise it closes this level (the second level takes the whole
     * menu with it).
     */
    void handleMenuBlankPress(SecondaryMenu* menu, bool right);
    /// The level above `menu` (the second level for its first child), nullptr for
    /// the second level itself.
    SecondaryMenu* parentOf(const SecondaryMenu* menu) const;
    /**
     * @brief The tag operations of the tags this button captured.
     *
     * A right click (without dragging) opens it. The button is the only handle a
     * captured tag has in an editor - the add widget shows the type buttons, not
     * the tags - so without this menu those tags could not be edited or, later,
     * merged by hand.
     */
    void showCapturedTagMenu();
    /// Right click on a family entry: show its third level, or put it away again.
    void toggleSubmenuFor(const Action& parent, SecondaryMenu* owner);
    void setCaptured(bool captured);
    void loadActions();
    /// One tag from an entry's text, through m_tagBuilder when one was given.
    std::shared_ptr<teTag> makeTag(const QString& text) const;
    /// True when the entry replaced the type of the tag the menu was opened at.
    bool rewriteAnchoredTag(const std::shared_ptr<teTag>& tag, const Action& action);    /// The entries of the menu that is on screen (the first one when none is).
    const QVector<Action>& activeActions() const;
    /// Does the menu on screen declare a third level? Then its groups belong to
    /// setFamily(), not to the "alternatives" rule in toggleAction().
    bool activeMenuHasSubmenu() const;

    /// Toggles the tag of setOwnTag() in the tag list (a click on the button).
    void toggleOwnTag();

    QString m_menuPath;
    QString m_rightMenuPath;
    /// The tag a click on the button writes; empty for a pure launcher.
    QString m_ownTag;
    QString m_kind;
    QString m_displayName;
    bool m_palette = false;
    QString m_contextTag;
    /// The family spelling paletteTagFor() resolved for m_contextTag.
    QString m_paletteTag;
    /// Empty unless the menu on screen was opened by the button itself (the add
    /// widget). A menu opened by a reftag's hover preview keeps editing the list.
    std::function<void(const QString& tag, bool toggle)> m_composeSink;
    bool m_compose = false;
    /**
     * @brief The tag the menu on screen was opened at (the reftag gesture).
     *
     * Weak: the tag may be merged away while the menu is up, and a pinned menu
     * outlives the hover that created it. Cleared when the menu closes.
     */
    std::weak_ptr<teTag> m_anchorTag;
    /// How an entry rewrites that tag; empty when no editor installed a rule.
    std::function<bool(std::shared_ptr<teTag>, const QString&, const QStringList&)> m_rewriteSink;
    mutable QStringList m_familyTags;
    mutable bool m_familyDirty = true;
    QString m_placeholder;
    QString m_caption;
    int m_maxLines = 1;
    /// Builds a tag from an entry's text; empty means teTag(text).
    std::function<std::shared_ptr<teTag>(const QString&)> m_tagBuilder;
    /// The entries of the first menu and of every third-level menu it declares.
    QVector<Action> m_actions;
    /// The same for the second menu (SC-05).
    QVector<Action> m_rightActions;

    SecondaryMenu* m_menu = nullptr;
    /// The second menu, built on the first right press (SC-05).
    SecondaryMenu* m_rightMenu = nullptr;
    /// The menu that is on screen.
    SecondaryMenu* m_active = nullptr;
    /**
     * @brief The menus that hang off the second one, outermost first.
     *
     * `m_children[0]` is the third level, `[1]` the fourth, and so on. They form
     * one chain: a menu is anchored at the entry of the menu before it, and only
     * the last one is interactive (the ones behind stay visible).
     */
    QVector<SecondaryMenu*> m_children;

    mutable CaptionLayout m_layout;
    mutable int m_layoutWidth = -1;
    mutable QString m_layoutCaption;

    Mode m_mode = Mode::Hidden;
    bool m_dragging = false;
    /// A right press that has not become a drag yet. A plain right click opens the
    /// captured tags' menu, a right press that moves opens the button's menu.
    bool m_rightPressPending = false;
    QPoint m_rightPressPos;
    bool m_captured = false;
    /// A right click already used this interaction to switch an entry. The left
    /// release that follows must then not switch a second one (the pointer is often
    /// still resting on a menu entry when the third level appears).
    bool m_rightClickUsed = false;
    /// The entry a right press just acted on. The release under the same cursor
    /// position must not toggle it a second time (right press + release on one
    /// entry is one click, not two).
    QString m_rightPressedId;
    /// True while the open menus belong to a held *left* button. A right click is
    /// then part of that gesture; a right-drag (no left button) is opened and
    /// finished by the right button alone, and its release still pins.
    bool m_dragLeftHeld = false;
    /// The menu a right click happened in while the left button was held. The
    /// release that ends the drag must not also "click" *that* menu - but a
    /// release over another level is still a normal pick.
    SecondaryMenu* m_rightUsedMenu = nullptr;
};

#endif // SECONDARYBUTTON_H
