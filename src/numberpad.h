#ifndef NUMBERPAD_H
#define NUMBERPAD_H

#include "litewidget.h"

#include <QString>
#include <functional>

class QLineEdit;
class QPushButton;

/**
 * @brief A small keyboard-shaped pad that appears under an edit box.
 *
 * Typing "3" into the people count is two keystrokes for something that has six
 * possible answers, and the answer decides the tag ("3girls"). The pad puts
 * [1][2][3][4][5][6+] right under the box: one click writes the number, commits
 * the edit (so the tag is added or changed immediately) and goes away.
 *
 * Typing something else - "10", or a number the pad does not offer - is still
 * possible: the pad is a shortcut, not a replacement for the edit box.
 *
 * It never takes the keyboard focus (WA_ShowWithoutActivating +
 * WindowDoesNotAcceptFocus), so the line edit keeps its cursor and its selection
 * while the pad is open, and `6+` can be clicked after typing a prefix.
 */
class NumberPad : public LiteWidget
{
    Q_OBJECT
public:
    explicit NumberPad(QWidget* parent = nullptr);

    /// Opens the pad under `target`. `commit` runs after the number was written.
    void openFor(QLineEdit* target, std::function<void()> commit);
    void dismiss();
    bool isOpenFor(const QLineEdit* target) const { return m_open && m_target == target; }

    /// The labels the pad offers, in order (regression: the six answers).
    static QStringList labels();

signals:
    void picked(const QString& text);

private:
    void pick(const QString& text);

    QLineEdit* m_target = nullptr;
    std::function<void()> m_commit;
    bool m_open = false;
    QVector<QPushButton*> m_buttons;
};

#endif // NUMBERPAD_H
