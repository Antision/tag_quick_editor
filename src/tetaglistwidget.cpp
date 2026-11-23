#include "tetaglistwidget.h"
#include "testyle.h"
#include "tetag.h"
#include "tesignalwidget.h"
#include "func.h"
#include "svgicon.h"
#include <QPointer>
#include <QShortcut>

#include <algorithm>

#include "tetaglistwidget.h"
#include "testyle.h"
#include "teeditor.h"
#include "tetag.h"
#include "tetagdisplaywidget.h"
#include "tesignalwidget.h"
#include "func.h"
#include <QPointer>
QStringList ClipBoard;

teTagListWidgetBase::teTagListWidgetBase(int wordsize,QWidget *parent): QWidget{parent}{
    // 列表吃键盘(focusOwningList 把焦点送到这里):ClickFocus,不参与 Tab 链。
    setFocusPolicy(Qt::ClickFocus);
    lineedit=new SuggestionEdit(suggestionBox,this);
    plaintextedit = new teInputWidget;
    QSizePolicy sp =QSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
    sp.setHorizontalStretch(1);
    sp.setVerticalStretch(1);
    lineedit->setSizePolicy(sp);

    wlayout = new QVBoxLayout(this);
    wlayout->setSpacing(0);
    wlayout->setContentsMargins(0,0,0,0);
    sc = new QScrollArea(this);
    sc->setStyleSheet(teStyle::list(wordsize));
    sc->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
    sc->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    sc->setContentsMargins(0,0,0,0);
    wlayout->addWidget(sc);
    QWidget * container = new QWidget(sc);
    layout = new QVBoxLayout(container);
    layout->setSpacing(0);
    layout->setContentsMargins(1,1,1,1);
    sc->setWidget(container);
    sc->setWidgetResizable(true);
    container->setLayout(layout);
    suggestionBox->hide();
    QSpacerItem* bottomspacer = new QSpacerItem(0,0,QSizePolicy::Fixed,QSizePolicy::Expanding);
    layout->insertSpacerItem(-1,bottomspacer);
    lineedit->hide();
    editAction = menu->addAction(svgicon::menuIcon(":/icons/pen-line.svg"),"edit (F2/Ctrl+E)");
    deleteAction = menu->addAction(svgicon::menuIcon(":/icons/trash.svg"),"delete (Ctrl+D/del)");
    insertAction = menu->addAction(svgicon::menuIcon(":/icons/chevrons-up.svg"),"insert above (Ctrl+W)");
    insertBelowAction = menu->addAction(svgicon::menuIcon(":/icons/chevrons-down.svg"),"insert below");
    cutAction = menu->addAction(svgicon::menuIcon(":/icons/scissors.svg"),"cut (Ctrl+X)");
    copyAction = menu->addAction(svgicon::menuIcon(":/icons/copy.svg"),"copy (Ctrl+C)");
    pasteAction = menu->addAction(svgicon::menuIcon(":/icons/clipboard-list.svg"),"paste (Ctrl+V)");
    // 手动合并(layers-2)的菜单项在 onTagRightButtonClicked() 里按需创建;
    // 快捷键 Tab / Shift+M 由列表的 keyPressEvent 处理(见 teRefTagListWidget),
    // 没实现合并的列表不会吞掉 Tab。
    menu->setStyleSheet(
        R"(QMenu {
        background-color: transparent;
        font-size:14px;
        color:#8dfda7;
        border-radius:3px;
    }
    QMenu::item {
        background-color: rgb(27,29,37);
        padding: 3px;
        border-radius:2px;
        margin: 0px;
        border: 1px solid #8b9ac6;
    }
    QMenu::item:selected {background-color: #3d258f;}
)");
    menu->setAttribute(Qt::WA_TranslucentBackground);
    menu->setWindowFlags(menu->windowFlags() | Qt::FramelessWindowHint);
}

teTagListWidgetBase::~teTagListWidgetBase(){
    delete plaintextedit;
}

bool teTagListWidgetBase::isSelected(teTagWidgetBase *in){
    if(select_current==in) return true;
    else if(select.find(in)!=select.end()) return true;
    else return false;
}

teTagWidgetBase* teTagListWidgetBase::widgetForCore(const std::shared_ptr<teTag>& core) const
{
    if(!core)
        return nullptr;
    // The list's own registry: widgets belong to the list that shows them, so this
    // table - not teTag::widget - is what answers "which widget shows this tag".
    if(teTagWidgetBase* registered = m_widgets.value(core.get(),nullptr))
        return registered;
    // Slow path: a core may be shown by a widget that never registered itself
    // (that is how the editors' teRefTag works).
    if(!layout)
        return nullptr;
    for(int i=0;i<layout->count();++i){
        if(auto* tag = dynamic_cast<teTagWidgetBase*>(layout->itemAt(i)->widget())){
            if(tag->core==core)
                return tag;
        }
    }
    return nullptr;
}

void teTagListWidgetBase::registerWidgetFor(std::shared_ptr<teTag> core,teTagWidgetBase* widget)
{
    if(core)
        m_widgets.insert(core.get(),widget);
}

void teTagListWidgetBase::unregisterWidgetFor(std::shared_ptr<teTag> core)
{
    if(core)
        m_widgets.remove(core.get());
}

teTagWidgetBase* teTagListWidgetBase::ensureWidgetFor(std::shared_ptr<teTag> core)
{
    if(!core)
        return nullptr;
    if(teTagWidgetBase* existing = widgetForCore(core))
        return existing;
    // The widget pool belongs to the list that shows the widgets, not to the tag:
    // this is the call that used to be teTag::load() (core->widget = pool.getTag()).
    auto* built = widgetpool.getTag(core);
    registerWidgetFor(core,built);
    return built;
}

void teTagListWidgetBase::releaseWidgetFor(std::shared_ptr<teTag> core)
{
    if(!core)
        return;
    if(teTagWidgetBase* widget = widgetForCore(core)){
        unregisterWidgetFor(core);
        widgetpool.give_back(widget);           // used to be teTag::unload()
    }
}

void teTagListWidgetBase::setSelectCurrentCore(std::shared_ptr<teTag> core,bool ifclear){
    setSelectCurrent(widgetForCore(core),ifclear);
    ensureCoreVisible(core);
}

void teTagListWidgetBase::setSelectCore(std::shared_ptr<teTag> core){
    if(teTagWidgetBase* tag = widgetForCore(core))
        setSelect(tag);
}

int teTagListWidgetBase::setUnselectCore(std::shared_ptr<teTag> core){
    if(!core)
        return setUnselect(nullptr);
    if(teTagWidgetBase* tag = widgetForCore(core))
        return setUnselect(tag);
    return 0;
}

int teTagListWidgetBase::setSelectRangeCore(std::shared_ptr<teTag> core,bool ifclear){
    return setSelectRange(widgetForCore(core),ifclear);
}

bool teTagListWidgetBase::isCoreSelected(std::shared_ptr<teTag> core) const{
    teTagWidgetBase* tag = widgetForCore(core);
    if(!tag)
        return false;
    return select_current==tag||select.find(tag)!=select.end();
}

void teTagListWidgetBase::ensureCoreVisible(std::shared_ptr<teTag> core){
    teTagWidgetBase* tag = widgetForCore(core);
    if(!tag||!sc)
        return;
    sc->ensureWidgetVisible(tag);
    // The bound lambda keeps the timer from firing after this widget is gone.
    QTimer::singleShot(0,this,[this]{
        if(sc)
            sc->horizontalScrollBar()->setValue(0);
    });
}

void teTagListWidgetBase::tagEdit(teTagWidgetBase *tag, teWordWidgetBase *inw){
    // Editing is one tag at a time. A multi selection is what the context menu
    // works on; the editor itself must not inherit it.
    if(tag&&(select_current!=tag||!select.empty()))
        setSelectCurrent(tag,true);
    editingTag = tag;
    if(!tag||!tag->core)
        return;                             // unknown tag: nothing to edit
    editingCore.reset();
    if(tag->core->type==teTag::sentence){
        plaintextedit->start(*tag->core);
        plaintextedit->show();
        connect(plaintextedit,&teInputWidget::stringSignal,this,&teTagListWidgetBase::onPlainTextEditStop,Qt::DirectConnection);
        connect(plaintextedit,&teInputWidget::cancelSignal,this,[this]{
            disconnect(plaintextedit,&teInputWidget::stringSignal,0,0);
            disconnect(plaintextedit,&teInputWidget::cancelSignal,0,0);
        });
    }else{
        int maxsize=0;
        int begin=0;
        int count=0;
        QString tagtext="";
        if(inw!=nullptr){
            for(teWord*&word:tag->core->words){
                if(inw!=tag->wordWidgetFor(word)){
                    maxsize+=word->text.size()+1;
                }else{
                    begin=maxsize;
                    maxsize+=word->text.size()+1;
                    count = word->text.size()+1;
                }
            }
        }else{
            for(teWord*&word:tag->core->words){
                maxsize+=word->text.size()+1;
            }
        }
        tagtext.reserve(maxsize);
        for(teWord*&word:tag->core->words){
            if(teWordWidgetBase* wordWidget = tag->wordWidgetFor(word))
                wordWidget->hide();
            tagtext.append(word->text);
            tagtext.append(' ');
        }
        lineedit->start(tagtext);
        tag->layout->insertWidget(0,lineedit);
        connect(lineedit,&SuggestionEdit::editingFinished,this,&teTagListWidgetBase::onLineEditStop,Qt::DirectConnection);
        lineedit->containerParent=tag;
        lineedit->show();
        lineedit->moveSuggestionBox();
        lineedit->setFocus();
        inw==nullptr?lineedit->selectAll():lineedit->setSelection(begin,count);
    }
}

void teTagListWidgetBase::onPlainTextEditStop(QString in_str){
    disconnect(plaintextedit,0,this,0);
    // An editor control can start this without a widget behind the tag, so the
    // core is what actually identifies the tag being edited.
    if(editingTag)
        tagEdit(editingTag->core,in_str);
    else if(editingCore)
        tagEdit(editingCore,in_str);
    editingTag=nullptr;
    editingCore.reset();
}

void teTagListWidgetBase::tagEditCore(std::shared_ptr<teTag> core){
    tagEdit(widgetForCore(core));
}

void teTagListWidgetBase::onLineEditStop(){
    disconnect(lineedit,0,this,0);
    editingTag->layout->removeWidget(lineedit);
    tagEdit(editingTag->core,lineedit->text());
    editingTag=nullptr;
}

void teTagListWidgetBase::setSelectCurrent(teTagWidgetBase *tag, bool ifclear){
    if(ifclear){
        if(select_current!=nullptr&&tag!=select_current){
            select_current->setStyle(normal_style_enum);
        }
        if(!select.empty()){
            for(teTagWidgetBase*tp:select){
                tp->setStyle(normal_style_enum);
            }
            select.clear();
        }
    }else{
        if(select_current==tag)return;
        if(select.find(tag)!=select.end()){
            select.erase(tag);
        }
        if(select_current!=nullptr){
            select_current->setStyle(select_style_enum);
            select.insert(select_current);
        }
    }
    select_current=tag;
    pendingSelectionIndex = findWidgetIndexInLayout(layout,select_current);

    if(tag!=nullptr)
        tag->setStyle(select_current_style_enum);
}

void teTagListWidgetBase::setSelect(teTagWidgetBase *in){
    in->setStyle(select_style_enum);
    select.insert(in);
}

int teTagListWidgetBase::setUnselect(teTagWidgetBase *in){
    if(in==nullptr){
        if(select_current!=nullptr){
            select_current->setStyle(normal_style_enum);
            select_current=nullptr;
        }
        if(!select.empty()){
            for(teTagWidgetBase*tp:select){
                tp->setStyle(normal_style_enum);
            }
            select.clear();
        }
        return 0;
    }else if(in==select_current){
        in->setStyle(normal_style_enum);
        if(!select.empty()){
            select_current=*select.begin();
            select.erase(select_current);
            select_current->setStyle(select_current_style_enum);
        }else
            select_current=nullptr;
        return 0;
    }else if(select.find(in)!=select.end()){
        in->setStyle(normal_style_enum);
        select.erase(in);
        return 0;
    }
    return 0;
}

void teTagListWidgetBase::tagEdit(){
    if(select_current!=nullptr)
        tagEdit(select_current);
}

QVector<std::shared_ptr<teTag>> teTagListWidgetBase::selectedCoresInVisualOrder() const
{
    QVector<QPair<int,teTagWidgetBase*>> indexed;
    auto push=[&](teTagWidgetBase* widget){
        if(widget&&widget->core)
            indexed.append({findWidgetIndexInLayout(layout,widget),widget});
    };
    push(select_current);
    for(teTagWidgetBase* widget:select)
        push(widget);
    std::sort(indexed.begin(),indexed.end(),
              [](const QPair<int,teTagWidgetBase*>& a,const QPair<int,teTagWidgetBase*>& b){
                  return a.first<b.first;
              });
    QVector<std::shared_ptr<teTag>> out;
    out.reserve(indexed.size());
    for(const auto& entry:indexed)
        out.append(entry.second->core);
    return out;
}
void teTagListWidgetBase::focusOutEvent(QFocusEvent *e){
    // The context menu is a focus-out, not a "clicked elsewhere": clearing the
    // selection here made a multi-tag right click act on nothing.
    if(!m_contextMenuOpen)
        setUnselect();
    return QWidget::focusOutEvent(e);
}

QString teTagListWidgetBase::getSelectText()
{
    QList<std::shared_ptr<teTag>> cores;

    if (select_current && select_current->core)
        cores.push_back(select_current->core);

    for (teTagWidgetBase* tw : select) {
        if (tw && tw->core)
            cores.push_back(tw->core);
    }
    return serializePieces(cores.size(), [&](int i) -> std::shared_ptr<teTag> {
        return cores[i];
    }, true);
}

void teTagListWidgetBase::paste()
{
    QString clipboardText = QApplication::clipboard()->text();
    const auto pieces = splitTextToPieces(clipboardText);

    for (const auto& piece : pieces) {
        if (!piece.text.isEmpty()) {
            tagInsertBelow(false, std::make_shared<teTag>(piece.text, piece.sentence), 1);
        }
    }
}

void teTagListWidgetBase::connectTag(teTagWidgetBase*tagwidget){
    connect(tagwidget,&teTagWidgetBase::droped,this,&teTagListWidgetBase::tagdroped);
    connect(tagwidget,&teTagWidgetBase::rightButtonPress,this,&teTagListWidgetBase::onTagRightButtonClicked);
    connect(tagwidget,&teTagWidgetBase::leftButtonPress,this,&teTagListWidgetBase::onTagLeftButtonClicked);
    connect(tagwidget,&teTagWidgetBase::mouseDoubleClicked,this,static_cast<void(teTagListWidgetBase::*)(teTagWidgetBase*,teWordWidgetBase*)>(&teTagListWidgetBase::tagEdit),Qt::DirectConnection);
    // Drives the magnified hover view.
}

void teTagListWidgetBase::disconnectTag(teTagWidgetBase *tagwidget){
    disconnect(tagwidget,0,this,0);
    teDisconnect(tagwidget->core.get());
}

void teTagListWidgetBase::onTagRightButtonClicked(teTagWidgetBase *tag, QPoint point, int modifiers){
    Q_UNUSED(point);
    Q_UNUSED(modifiers);
    if(manualMergeSupported()&&!mergeAction){
        menu->addSeparator();
        mergeAction = menu->addAction(svgicon::menuIcon(":/icons/layers-2.svg"),"merge (Tab / Shift+M)");
    }
    if(tag)
        // Keep the multi selection: the menu acts on all selected tags, and a
        // right click is how that set is reached (edit still edits one).
        setSelectCurrent(tag,false);
    menu->show();
    // `exec` takes the focus away from the list; without this the list's
    // focusOutEvent cleared the selection the menu was about to work on.
    m_contextMenuOpen = true;
    QAction *action = menu->exec(QCursor::pos());
    m_contextMenuOpen = false;
    if(action==editAction){
        tagEdit();
    }else if(action == deleteAction){
        // 与 Ctrl+D/del 一致:从所属列表删除(ref list 覆写成 tagDestroy),
        // 而不是只把 ref widget 收回池子——后者会让 tag 留在 tag list 里。
        deleteSelection();
    }else if(action==insertAction){
        tagInsertAbove(true,nullptr);
    }else if(action==insertBelowAction){
        tagInsertBelow(true,nullptr);
    }else if(action==cutAction){
        cut();
    }else if(action==copyAction){
        copy();
    }else if(action==pasteAction){
        paste();
    }else if(mergeAction&&action==mergeAction){
        manualMerge();
    }

}

void teTagListWidgetBase::onTagLeftButtonClicked(teTagWidgetBase *tag, QPoint point, int modifiers){
    if(modifiers&Qt::ShiftModifier&&select_current!=nullptr){
        if(modifiers&Qt::ControlModifier)
            setSelectRange(tag,false);
        else
            setSelectRange(tag);
    }else if(modifiers&Qt::ControlModifier&&select_current!=nullptr){
        if((select.find(tag)==select.end())&&tag!=select_current)
            setSelect(tag);
        else
            setUnselect(tag);
    }else{
        if(!isSelected(tag))
            setSelectCurrent(tag,true);
    }
    // Clicking a tag is what opens the magnified popup (it used to appear after
    // hovering for a while, which got in the way of clicking and scrolling). The
    // widget based lists do not want it at all.
}
