#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

/**
 * @file settingsdialog.h
 * @brief Per-tag icon/colour overrides (the user-config layer of MARK-09).
 *
 * The dialog edits `tagDecorations` (config.json, "tagDecorations"): an exact tag
 * text plus an optional colour and icon. TagMarkRegistry applies them after its
 * own rules, so a user can repaint or re-icon any tag without touching
 * decorations.json.
 */

#include "litewindow.h"
#include "tagmarks.h"

class QWidget;

namespace Ui {
class SettingsDialog;
}

/**
 * @brief The decorations editor, inside the program's own window frame.
 *
 * It used to be a QDialog, which meant the system title bar while every other
 * window in the program is a LiteWindow. Same shell as the rest now: the frame,
 * the three glyph buttons and the drag/resize rules come from LiteWindow, and
 * the dialog part (modal, accepted/rejected) is the handful of lines in exec().
 */
class SettingsDialog : public LiteWindow
{
    Q_OBJECT
public:
    /**
     * @param seedTag A tag to add (and select) on open, normally the row the
     *        user had selected in the tag list: setting an icon for "this tag"
     *        then costs one click on the menu instead of typing the text again.
     *        Ignored when that tag already has an override.
     */
    explicit SettingsDialog(const QString& seedTag=QString(),QWidget* parent=nullptr);
    ~SettingsDialog() override;

    /**
     * @brief Runs the window modally. 1 = accepted (the value QDialog returned),
     * 0 = closed or cancelled.
     */
    int exec();

    /// The overrides as edited: rows without a tag, or with neither a colour nor
    /// an icon, are dropped (they would do nothing in the registry).
    QVector<TagMarkOverride> overrides() const;

signals:
    void accepted();
    void rejected();

private:
    void reloadTable();
    void fillEditor(int row);
    void writeEditorToItem();
    void updateRow(int row);
    void addItem();
    void removeItem();
    void pickColor();
    void pickIcon();

    Ui::SettingsDialog* ui;
    QVector<TagMarkOverride> m_items;
    /// Row whose values are in the editor fields; -1 when none is selected.
    int m_row=-1;
    /// True while the dialog fills widgets itself (their signals must not write
    /// back into the item).
    bool m_filling=false;
};

#endif // SETTINGSDIALOG_H
