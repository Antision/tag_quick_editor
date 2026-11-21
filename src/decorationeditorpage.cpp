#include "decorationeditorpage.h"
#include "decorationcontrol.h"
#include "mainwindow.h"
#include "teeditorlist.h"

DecorationEditorPage::DecorationEditorPage(TagListView* list,const QString& name)
    : teEditor_standard(list,name,QStringLiteral("decor"))
{
    // A non-widget control: addControl() registers it without adding a row.
    m_control=new DecorationControl;
    addControl(m_control);

    // The page has nothing to show, so it must not take the height of an empty
    // control page either.
    content->setVisible(false);
    content->setFixedHeight(0);
    mainLayout->setContentsMargins(0,0,0,0);
    // MARK-07: the title is kept, but lower than a normal editor page.
    editor_switch->setProperty("small",true);
    editor_switch->setFixedHeight(20);

    // The title is a button, like the custom editor's "add custom tags": it opens
    // the tag decorations dialog instead of folding a page that has nothing to
    // fold. (teEditor_standard wires it to onSwitchToggled in its constructor.)
    editor_switch->setCheckable(false);
    editor_switch->setToolTip(QStringLiteral("Edit the tag decorations"));
    disconnect(editor_switch,&QPushButton::clicked,this,&teEditor_standard::onSwitchToggled);
    connect(editor_switch,&QPushButton::clicked,this,[this]{
        editTagDecorations(this,QString());
    });
}
