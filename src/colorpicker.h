#ifndef COLORPICKER_H
#define COLORPICKER_H

/**
 * @file colorpicker.h
 * @brief The colour dialog of the tag decorations page.
 *
 * It is Qt's own QColorDialog, not a hand-made swatch grid: the wheel, the
 * luminance bar and the RGB/HSV spin boxes all stay the real ones. What the
 * program adds is the frame - the widget-based dialog (Qt::Widget) is put inside
 * a LiteWindow, which gives it the dark body and the black title bar of every
 * other window here.
 *
 * Without that, QColorDialog is a light dialog whose labels inherit the main
 * window's `QWidget{color:white}` rule: white text on a light box.
 */

#include "litewindow.h"

#include <QColor>
#include <QString>

class QColorDialog;

class ColorPickerWindow : public LiteWindow
{
    Q_OBJECT
public:
    explicit ColorPickerWindow(const QString& current,QWidget* parent=nullptr);

    /// The colour the picker currently shows.
    QColor color() const;
    /// Seeds the picker from the field's text (a colour name or #rrggbb).
    void setColorText(const QString& text);

    /// A colour name this program knows, else Qt's own parser.
    static QColor parseColor(const QString& text);

    /**
     * @brief Runs the picker modally and writes the chosen colour back as #rrggbb.
     *
     * @return false when cancelled - the caller then keeps its old value.
     */
    static bool pick(QString* inOut,QWidget* parent);
signals:
    void finished(bool accepted);
private:
    void applyDarkScheme();
    QColorDialog* m_dialog=nullptr;
};

#endif // COLORPICKER_H
