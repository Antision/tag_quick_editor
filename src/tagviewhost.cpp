#include "tagviewhost.h"
#include "tetagdisplaywidget.h"
#include "tesignalwidget.h"

TqeTagViewHost::TqeTagViewHost(QObject* parent)
    : QObject(parent)
{
}

TagDisplay* TqeTagViewHost::createTagDisplay(TagDisplayOwner* owner)
{
    // No QWidget parent on purpose: the popup is a top-level tool window, and
    // the view owns it through TagDisplay::widget().
    return new teTagDisplayWidget(owner);
}

void TqeTagViewHost::editLongText(TagListView* view,std::shared_ptr<teTag> core)
{
    if(!view||!core)
        return;
    auto* editor=new teInputWidget;
    editor->start(static_cast<QString>(*core));
    editor->show();
    // Direct connections: the window emits from the GUI thread and the view has
    // to apply the edit before the window disappears.
    connect(editor,&teInputWidget::stringSignal,view,
            [view,editor,core](const QString& text,bool ifadd){
        Q_UNUSED(ifadd);
        editor->hide();
        editor->deleteLater();
        view->tagEdit(core,text);
    },Qt::DirectConnection);
    connect(editor,&teInputWidget::cancelSignal,view,[editor]{
        editor->hide();
        editor->deleteLater();
    },Qt::DirectConnection);
}
