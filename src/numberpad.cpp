#include "numberpad.h"

#include "secondarymenu.h"      // placeMenu(): the pad and the menus clamp the same way
#include "testyle.h"

#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QScreen>

QStringList NumberPad::labels()
{
    // 0 first: it is not "zero people", it is "no count at all" - the same as
    // clearing the box, which removes the tag.
    QStringList result{QStringLiteral("0")};
    for (int i = 1; i <= 5; ++i)
        result << QString::number(i);
    result << QStringLiteral("6+");
    return result;
}

NumberPad::NumberPad(QWidget* parent)
    : LiteWidget(parent)
{
    setNoActivate(true);
    setWindowFlag(Qt::WindowDoesNotAcceptFocus, true);
    setAttribute(Qt::WA_ShowWithoutActivating, true);
    setStyleSheet(teStyle::sheet(QStringLiteral("controls")));

    auto* row = new QHBoxLayout;
    row->setContentsMargins(4, 4, 4, 4);
    row->setSpacing(3);
    if (auto* layout = contentLayout())
        layout->addLayout(row);

    for (const QString& label : labels()) {
        auto* button = new QPushButton(label, this);
        button->setMinimumSize(30, 26);
        button->setFocusPolicy(Qt::NoFocus);
        button->setCursor(Qt::ArrowCursor);
        connect(button, &QPushButton::clicked, this, [this, label] { pick(label); });
        row->addWidget(button);
        m_buttons.push_back(button);
    }
}

void NumberPad::openFor(QLineEdit* target, std::function<void()> commit)
{
    if (!target)
        return;
    m_target = target;
    m_commit = std::move(commit);
    m_open = true;

    // WS_EX_NOACTIVATE is what keeps the click on a pad button from taking the
    // focus away from the edit box: WA_ShowWithoutActivating only affects the
    // show, Windows still activates a window on a mouse click.
    const HWND hwnd = reinterpret_cast<HWND>(winId());
    LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    SetWindowLongPtrW(hwnd, GWL_EXSTYLE, style | WS_EX_NOACTIVATE);

    const QRect anchor(target->mapToGlobal(QPoint(0, 0)), target->size());
    QScreen* screen = QGuiApplication::screenAt(anchor.center());
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    if (!screen)
        return;
    const QRect available = screen->availableGeometry();
    const QSize wanted = sizeHint();
    resize(wanted);
    move(SecondaryMenu::placeMenu(anchor, wanted, available));
    show();
    raise();
}

void NumberPad::dismiss()
{
    m_open = false;
    m_target = nullptr;
    m_commit = nullptr;
    hide();
}

void NumberPad::pick(const QString& text)
{
    QLineEdit* target = m_target;
    auto commit = m_commit;
    // Write first, then close: the commit reads the text (and may re-set it), and
    // a visible pad during that would show a stale value for one frame.
    if (target)
        target->setText(text);
    dismiss();
    emit picked(text);
    if (commit)
        commit();
    // The pad was a shortcut for this one value: the edit is finished, so the box
    // lets go of the focus instead of keeping a caret blinking behind a closed pad.
    if (target)
        target->clearFocus();
}
