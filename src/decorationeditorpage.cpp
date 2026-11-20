#include "decorationeditorpage.h"
#include "decorationcontrol.h"
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
}

void DecorationEditorPage::setEditorList(teEditorList* list)
{
    if(m_control)
        m_control->editorList=list;
}
