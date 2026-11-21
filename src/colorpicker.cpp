#include "colorpicker.h"

#include "tagmarks.h"           // the named colour table the rules understand

#include <QColorDialog>
#include <QEventLoop>
#include <QPalette>
#include <QScreen>

ColorPickerWindow::ColorPickerWindow(const QString& current,QWidget* parent)
    : LiteWindow(parent)
{
    setWindowTitle(QStringLiteral("Pick colour"));
    // Qt::Widget turns the dialog into a plain widget; without it setContentWidget
    // would take a window and the frame would never be drawn.
    m_dialog=new QColorDialog;
    m_dialog->setWindowFlags(Qt::Widget);
    // The native Windows dialog is light and ignores every stylesheet.
    m_dialog->setOption(QColorDialog::DontUseNativeDialog,true);
    m_dialog->setOption(QColorDialog::ShowAlphaChannel,false);
    setContentWidget(m_dialog);
    applyDarkScheme();
    resize(540,420);

    connect(m_dialog,&QColorDialog::colorSelected,this,[this](const QColor& color){
        hide();
        emit finished(color.isValid());
    });
    connect(m_dialog,&QColorDialog::rejected,this,[this]{
        hide();
        emit finished(false);
    });
    setColorText(current);
}

QColor ColorPickerWindow::parseColor(const QString& text)
{
    const QString trimmed=text.trimmed();
    if(trimmed.isEmpty())
        return QColor();
    // The program's own table first ("blonde" is not a QColor name), then Qt's.
    const QColor named=TagMarkRegistry::instance().colorFor(trimmed.toLower());
    if(named.isValid())
        return named;
    return QColor(trimmed);
}

QColor ColorPickerWindow::color() const
{
    return m_dialog?m_dialog->currentColor():QColor();
}

void ColorPickerWindow::setColorText(const QString& text)
{
    const QColor value=parseColor(text);
    if(m_dialog&&value.isValid())
        m_dialog->setCurrentColor(value);
}

void ColorPickerWindow::applyDarkScheme()
{
    // The palette is what the wheel, the luminance bar and the spin boxes paint
    // themselves with; the sheet takes care of the text and the buttons.
    QPalette dark=m_dialog->palette();
    dark.setColor(QPalette::Window,QColor(0x18,0x18,0x18));
    dark.setColor(QPalette::WindowText,QColor(0xe6,0xe6,0xe6));
    dark.setColor(QPalette::Base,QColor(0x23,0x23,0x23));
    dark.setColor(QPalette::AlternateBase,QColor(0x1c,0x1c,0x1c));
    dark.setColor(QPalette::Text,QColor(0xe6,0xe6,0xe6));
    dark.setColor(QPalette::Button,QColor(0x1f,0x1f,0x1f));
    dark.setColor(QPalette::ButtonText,QColor(0xe6,0xe6,0xe6));
    dark.setColor(QPalette::Highlight,QColor(0x4c,0x39,0x9e));
    dark.setColor(QPalette::HighlightedText,QColor(0xff,0xff,0xff));
    m_dialog->setPalette(dark);
    m_dialog->setAutoFillBackground(true);
    m_dialog->setStyleSheet(QStringLiteral(
        "QColorDialog{background:#181818;}"
        "QLabel{color:#e6e6e6;background:transparent;}"
        "QPushButton{color:#e6e6e6;background:transparent;border:1px solid #4c399e;"
        "padding:4px 12px;border-radius:3px;}"
        "QPushButton:hover{background:rgba(177,100,255,60);}"
        "QLineEdit,QSpinBox{background:#232323;color:#e6e6e6;border:1px solid #4c399e;"
        "border-radius:3px;padding:2px 4px;}"
        "QSpinBox::up-button,QSpinBox::down-button{background:#1f1f1f;border:none;width:14px;}"));
}

bool ColorPickerWindow::pick(QString* inOut,QWidget* parent)
{
    ColorPickerWindow window(inOut?*inOut:QString(),parent);
    bool accepted=false;
    QEventLoop loop;
    QObject::connect(&window,&ColorPickerWindow::finished,&loop,[&accepted,&loop](bool ok){
        accepted=ok;
        loop.quit();
    });
    // Esc and the close glyph end the modal run as "cancelled".
    QObject::connect(&window,&LiteWindow::closed,&loop,&QEventLoop::quit);

    window.show();
    QWidget* anchor=parent?parent->window():nullptr;
    if(anchor){
        const QRect area=anchor->frameGeometry();
        window.move(area.center()-QPoint(window.width()/2,window.height()/2));
    }
    if(QScreen* screen=window.screen()){
        const QRect available=screen->availableGeometry();
        QRect frame=window.frameGeometry();
        if(frame.left()<available.left()) frame.moveLeft(available.left());
        if(frame.top()<available.top()) frame.moveTop(available.top());
        if(frame.right()>available.right()) frame.moveRight(available.right());
        if(frame.bottom()>available.bottom()) frame.moveBottom(available.bottom());
        window.move(frame.topLeft());
    }
    loop.exec();
    window.hide();
    if(!accepted)
        return false;
    const QColor chosen=window.color();
    if(!chosen.isValid())
        return false;
    if(inOut)
        *inOut=chosen.name();       // #rrggbb: the field takes both forms
    return true;
}
