#include "tetagdisplaywidget.h"
#include "tetaglistwidget.h"
#include "qflowlayout.h"

namespace {

/// The tag list draws its words at this point size (see teTagListWidget).
constexpr int kTagListWordPointSize = 15;

constexpr int kHorizontalMargin = 6;
constexpr int kVerticalMargin = 4;
constexpr int kGap = 6;          ///< distance to the tag the popup belongs to

/// Opacity while the popup only follows the pointer: at full opacity it swallowed
/// the clicks and wheel events meant for the list behind it.
constexpr qreal kIdleOpacity = 0.3;

QScreen* screenAt(const QPoint& globalPos)
{
    if (QScreen* screen = QGuiApplication::screenAt(globalPos))
        return screen;
    return QGuiApplication::primaryScreen();
}

/**
 * @brief A word inside the popup.
 *
 * The base class moves itself while the mouse is dragged and reorders the tag
 * from widget geometry, which cannot work once the tag wraps onto several
 * lines. Here the QFlowLayout owns the position (the word reorderer drives it)
 * and every mouse event is forwarded to the popup so the whole tag can still be
 * dragged like in the tag list.
 */
class teDisplayWord : public teWordWidgetBase {
public:
    explicit teDisplayWord(teWord& wordCore) : teWordWidgetBase(wordCore) {}
    void mousePressEvent(QMouseEvent* event) override {
        if (auto* tag = qobject_cast<teTagWidgetBase*>(parentWidget()))
            tag->mousePressEvent(event);
    }
    void mouseMoveEvent(QMouseEvent* event) override {
        if (auto* tag = qobject_cast<teTagWidgetBase*>(parentWidget()))
            tag->mouseMoveEvent(event);
    }
    void mouseReleaseEvent(QMouseEvent* event) override {
        if (auto* tag = qobject_cast<teTagWidgetBase*>(parentWidget()))
            tag->mouseReleaseEvent(event);
    }
    void mouseDoubleClickEvent(QMouseEvent* event) override {
        // The words cover the whole popup, so without this the popup never saw a
        // double click (editing a sentence tag did nothing).
        if (auto* tag = qobject_cast<teTagWidgetBase*>(parentWidget()))
            tag->mouseDoubleClickEvent(event);
    }
};

}

teTagDisplayWidget::teTagDisplayWidget(teTagListWidgetBase* owner)
    : teTagWidgetBase(), m_owner(owner)
{
    // A tool window: it floats above the main window, never takes focus and is
    // not part of the window's z-order.
    setWindowFlags(Qt::Tool|Qt::FramelessWindowHint|Qt::NoDropShadowWindowHint
                   |Qt::WindowDoesNotAcceptFocus);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setObjectName(QStringLiteral("tagDisplay"));
    setFocusPolicy(Qt::StrongFocus);

    m_flow = new QFlowLayout(this,kHorizontalMargin,8,4);
    m_flow->setContentsMargins(kHorizontalMargin,kVerticalMargin,kHorizontalMargin,kVerticalMargin);

    // Reuse the flow layout reorderer so a word can be dragged onto another line.
    m_wordReorderer = new QFlowLayoutReorderer(m_flow,this,this);
    connect(m_wordReorderer,&QFlowLayoutReorderer::reordered,this,&teTagDisplayWidget::syncWordOrderFromLayout);
}

teTagDisplayWidget::~teTagDisplayWidget()
{
    deleteWordWidgets();
    // teTagBase's destructor clears core->widget when it still holds a core;
    // that pointer belongs to the tag list, so drop our reference first.
    m_source = nullptr;
    core.reset();
}

void teTagDisplayWidget::deleteWordWidgets()
{
    const QList<teWordWidgetBase*> words = findChildren<teWordWidgetBase*>();
    for (teWordWidgetBase* word : words)
        word->core = nullptr;       // ~teWordBase must not clear core->widget
    for (teWordWidgetBase* word : words)
        delete word;
}

void teTagDisplayWidget::readCore(std::shared_ptr<teTag> in_core)
{
    if (core == in_core) {
        load();
        return;
    }
    if (core)
        core->teDisconnect(this);
    core = in_core;
    if (core){
        core->teConnect(teCallbackType::edit,this,&teTagDisplayWidget::load);
        core->teConnect(teCallbackType::edit_with_layout,this,&teTagDisplayWidget::load);
    }
    load();
}

bool teTagDisplayWidget::isCoreStillListed() const
{
    // Holding the shared_ptr keeps the tag object alive, but a merge moves (and
    // deletes) its word cores and takes the tag out of the list. Anything that
    // touches the words - dragging one, editing one - has to ask first, otherwise
    // it works on freed word cores.
    if (!core)
        return false;
    if (!m_owner)
        return false;
    teTagList* list = m_owner->shownList();
    if (!list)
        return false;
    return list->find(core) >= 0;
}

void teTagDisplayWidget::load()
{
    if (!core)
        return;

    // The tag list owns the real word widgets; ours are a magnified copy.
    deleteWordWidgets();

    const int pointSize = displayPointSize();
    const QString style = QStringLiteral("teTagDisplayWidget{background-color:#2b2b2b;border:1px solid #6b6b6b;}"
                                         "teWordWidgetBase{font:%1pt \"Segoe UI\";color:white;background:transparent;}"
                                         "teWordWidgetBase:hover{border:1px solid #21ffbd;}")
                              .arg(pointSize);
    setStyleSheet(style);

    for (teWord* wordCore : core->words) {
        auto* word = new teDisplayWord(*wordCore);
        word->setParent(this);
        word->setMaximumHeight(QWIDGETSIZE_MAX);       // undo the 27px tag-list cap
        word->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
        word->show();
        connect(word,&teWordWidgetBase::mouseDoubleClicked,this,[this](teWordWidgetBase* clicked){
            startEditing(clicked);
        });
        m_flow->addWidget(word);
        m_wordReorderer->attach(word);
    }

    // Natural single-line width unless the screen is narrower; the height then
    // follows from the flow layout, so a wrapped tag grows downwards while its
    // left edge stays where it is.
    const QFontMetrics metrics(QFont(QStringLiteral("Segoe UI"),pointSize));
    int natural = 2*kHorizontalMargin;
    for (teWord* wordCore : core->words)
        natural += metrics.horizontalAdvance(wordCore->text) + 8;
    natural = std::max(natural,60);

    const QScreen* screen = screenAt(QCursor::pos());
    const int screenWidth = screen ? screen->availableGeometry().width() : natural;
    const int width = std::min(natural,std::max(screenWidth-40,160));
    const int height = std::max(m_flow->heightForWidth(width),metrics.height()+2*kVerticalMargin);
    setFixedSize(width,height);
    m_flow->setGeometry(rect());
}

int teTagDisplayWidget::displayPointSize() const
{
    if (core && core->type == teTag::sentence)
        return kTagListWordPointSize;           // sentences must not get bigger
    return int(kTagListWordPointSize * 1.5);
}

void teTagDisplayWidget::showFor(teTagWidgetBase* source,const QPoint& anchor)
{
    if (!source || !source->core)
        return;
    if (m_editor)
        finishEditing(false);                   // switching tag aborts a pending edit

    m_source = source;
    readCore(source->core);
    showIdle();
    placeNextTo(anchor);
    show();
    raise();
}

void teTagDisplayWidget::showForCore(std::shared_ptr<teTag> tagCore,const QPoint& anchor)
{
    if (!tagCore)
        return;
    if (m_editor)
        finishEditing(false);                   // switching tag aborts a pending edit

    m_source = nullptr;                         // no widget behind this row
    readCore(tagCore);
    showIdle();
    placeNextTo(anchor);
    show();
    raise();
}

void teTagDisplayWidget::showIdle()
{
    // Translucent while it just hangs next to the tag, so that clicks and wheel
    // events reach the list behind it. Clicking it (or dragging a word) makes it
    // opaque, see setOpaque().
    setWindowOpacity(kIdleOpacity);
    // Watch the application while it is up, so a click anywhere outside it and
    // outside the tag list closes it.
    if (qApp && !m_watching){
        qApp->installEventFilter(this);
        m_watching=true;
    }
}

void teTagDisplayWidget::setOpaque()
{
    if (!qFuzzyCompare(windowOpacity(),qreal(1.0)))
        setWindowOpacity(1.0);
}

bool teTagDisplayWidget::eventFilter(QObject* watched,QEvent* event)
{
    if (event->type()!=QEvent::MouseButtonPress&&event->type()!=QEvent::Wheel)
        return QWidget::eventFilter(watched,event);
    const QPoint global = (event->type()==QEvent::MouseButtonPress)
                              ? static_cast<QMouseEvent*>(event)->globalPosition().toPoint()
                              : QCursor::pos();
    if (frameGeometry().contains(global))
        return QWidget::eventFilter(watched,event);     // our own event
    QWidget* under = QApplication::widgetAt(global);
    if (!under)
        return QWidget::eventFilter(watched,event);     // cannot tell where it landed
    if (m_owner&&(under==m_owner||m_owner->isAncestorOf(under)))
        return QWidget::eventFilter(watched,event);     // a click in the tag list
    hideDisplay();
    return QWidget::eventFilter(watched,event);
}

void teTagDisplayWidget::placeNextTo(const QPoint& anchor)
{
    // The popup belongs to the right of the tag (the user asked for the right
    // edge of the list), flipping to its left when that would leave the screen.
    const QScreen* screen = screenAt(anchor);
    const QRect available = screen?screen->availableGeometry():QRect(anchor,QSize(800,600));
    // Cap the width so a long tag wraps instead of growing until it is clamped
    // to the edge of the screen (which used to park it at the far left).
    const int maxWidth = std::max(240,available.width()/3);
    if (maximumWidth() != maxWidth)
        setMaximumWidth(maxWidth);
    adjustSize();                               // the flow layout re-wraps
    int x = anchor.x()+kGap;
    if (x + width() > available.right())
        x = anchor.x() - width() - kGap;        // flip to the left of the anchor
    x = std::clamp(x,available.left(),std::max(available.left(),available.right()-width()));
    const int y = std::clamp(anchor.y() - height()/2,
                             available.top(),
                             std::max(available.top(),available.bottom()-height()));
    move(x,y);
}

void teTagDisplayWidget::hideDisplay()
{
    if (m_editor)
        finishEditing(false);
    if (m_watching&&qApp){
        qApp->removeEventFilter(this);
        m_watching=false;
    }
    hide();
    m_source = nullptr;
    if (core)
        core->teDisconnect(this);
    core.reset();
}

void teTagDisplayWidget::leaveEvent(QEvent* event)
{
    Q_UNUSED(event);
    emit pointerLeft();
}

void teTagDisplayWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton)
        return;
    if (!isCoreStillListed()) {
        hideDisplay();                          // its tag was merged away
        return;
    }
    setOpaque();                                // clicked: show it at full opacity
    m_pressGlobal = event->globalPosition().toPoint();
    m_draggingTag = false;
    if (m_source) {
        m_grabOffsetY = m_pressGlobal.y() - m_source->mapToGlobal(QPoint(0,0)).y();
        // Give the source tag focus so the tag list keeps its selection in sync.
        m_source->setFocus();
    }
}

void teTagDisplayWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (!m_source || !(event->buttons() & Qt::LeftButton))
        return;
    const QPoint global = event->globalPosition().toPoint();
    if (!m_draggingTag) {
        if ((global - m_pressGlobal).manhattanLength() < QApplication::startDragDistance())
            return;
        m_draggingTag = true;
        setOpaque();                            // dragging a tag: full opacity
        if (m_editor)
            finishEditing(false);       // do not edit while moving the tag
    }

    if (QWidget* container = m_source->parentWidget()) {
        const int newY = container->mapFromGlobal(global).y() - m_grabOffsetY;
        m_source->move(m_source->x(),newY);
    }
    placeNextTo(global);                // the popup follows the pointer
    raise();
}

void teTagDisplayWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton)
        return;
    if (m_draggingTag && m_source && m_owner) {
        m_draggingTag = false;
        teTagWidgetBase* source = m_source;
        hideDisplay();                  // destroys nothing the reorder needs
        // Same entry point the tag itself uses when it is dropped.
        m_owner->tagdroped(source,event->modifiers());
    }
}

void teTagDisplayWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (!core)
        return;
    if (core->type == teTag::sentence) {
        // Sentences are edited in the plain text window of the tag list.
        const std::shared_ptr<teTag> tagCore = core;
        hideDisplay();
        if (m_owner)
            m_owner->tagEditCore(tagCore);
        return;
    }
    // The word under the pointer is the one to preselect; a double click next to
    // the words (on the border of the popup) selects the whole tag. The position
    // is taken from the *global* one: the event may have been forwarded by a word,
    // in which case its local position belongs to that word.
    teWordWidgetBase* clicked = nullptr;
    if (QWidget* under = childAt(mapFromGlobal(event->globalPosition().toPoint()))) {
        clicked = qobject_cast<teWordWidgetBase*>(under);
        if (!clicked)
            clicked = qobject_cast<teWordWidgetBase*>(under->parentWidget());
    }
    startEditing(clicked);
}

void teTagDisplayWidget::startEditing(teWordWidgetBase* clickedWord)
{
    if (!core || m_editor)
        return;
    if (!isCoreStillListed()) {
        hideDisplay();                          // its tag was merged away
        return;
    }
    setOpaque();                                // editing: do not dim the text
    // The popup is created as a tool window that never takes the focus (so it
    // does not steal it from the list). While its inline editor is open it has to
    // accept the focus, otherwise the line edit has no caret and cannot be typed
    // into - which is exactly what happened.
    setAttribute(Qt::WA_ShowWithoutActivating,false);
    setWindowFlag(Qt::WindowDoesNotAcceptFocus,false);
    show();
    activateWindow();

    m_editor = new QLineEdit(this);
    m_editor->setStyleSheet(QStringLiteral("QLineEdit{font:%1pt \"Segoe UI\";color:white;"
                                           "background-color:#101010;border:1px solid #12b594;}")
                                .arg(displayPointSize()));
    m_editor->setText(*core);
    m_editor->setGeometry(rect().adjusted(2,2,-2,-2));
    m_editor->show();
    m_editor->setFocus();

    int index = -1;
    if (clickedWord && clickedWord->core) {
        for (int i = 0; i < core->words.size(); ++i) {
            if (core->words[i] == clickedWord->core) {
                index = i;
                break;
            }
        }
    }
    if (index >= 0)
        m_editor->setSelection(wordOffsetInText(index),clickedWord->core->text.size());
    else
        m_editor->selectAll();

    connect(m_editor,&QLineEdit::returnPressed,this,[this]{ finishEditing(true); });
    connect(m_editor,&QLineEdit::editingFinished,this,[this]{ finishEditing(true); });
}

void teTagDisplayWidget::finishEditing(bool accept)
{
    if (!m_editor)
        return;
    QLineEdit* editor = m_editor;
    m_editor = nullptr;                     // never re-enter through the signals
    editor->disconnect(this);
    const QString text = editor->text();
    editor->hide();
    editor->deleteLater();

    if (accept && core && m_owner && text != static_cast<QString>(*core) && isCoreStillListed())
        m_owner->tagEdit(core,text);        // dedupe + undo + editors, all in one place

    // Back to "never takes the focus", so the next click goes to the list.
    setWindowFlag(Qt::WindowDoesNotAcceptFocus,true);
    setAttribute(Qt::WA_ShowWithoutActivating,true);
    if (isVisible())
        show();
}

int teTagDisplayWidget::wordOffsetInText(int index) const
{
    if (!core)
        return 0;
    const bool sentence = (core->type == teTag::sentence);
    QString out;
    for (int i = 0; i < core->words.size(); ++i) {
        const QString word = core->words[i]->text;
        if (i == index)
            return out.size();
        out += word;
        if (sentence) {
            out += QLatin1Char(' ');
            continue;
        }
        const QString next = (i + 1 < core->words.size()) ? core->words[i+1]->text : QString();
        const bool open = (word == QLatin1String("(") || word == QLatin1String("\\("));
        const bool close = (next == QLatin1String(")") || next == QLatin1String(")\\)"));
        if (!open && !close)
            out += QLatin1Char(' ');
    }
    return out.size();
}

void teTagDisplayWidget::worddroped(teWordWidgetBase* in_word,int xpos)
{
    Q_UNUSED(in_word);
    Q_UNUSED(xpos);
    // Word order is maintained by the flow layout reorderer; the tag list's
    // geometry based reordering cannot work across wrapped lines.
    syncWordOrderFromLayout();
}

void teTagDisplayWidget::syncWordOrderFromLayout()
{
    if (!core)
        return;
    if (!isCoreStillListed()) {
        // The tag was merged away while the drag was running: its word cores now
        // belong to (or were deleted by) the surviving tag, so the order must not
        // be written anywhere.
        hideDisplay();
        return;
    }

    QList<teWord*> ordered;
    for (int i = 0; i < m_flow->count(); ++i) {
        if (auto* word = dynamic_cast<teWordWidgetBase*>(m_flow->itemAt(i)->widget())) {
            if (word->core && core->words.contains(word->core) && !ordered.contains(word->core))
                ordered.append(word->core);
        }
    }
    for (teWord* wordCore : core->words)
        if (!ordered.contains(wordCore))
            ordered.append(wordCore);
    if (ordered == core->words)
        return;
    core->words = ordered;

    std::shared_ptr<teTag> tag = core;
    teTagListWidgetBase* owner = m_owner;
    if (!owner)
        return;
    const QString text = static_cast<QString>(*core);
    // Deferred on purpose: applying the edit rebuilds our word widgets, and
    // doing that synchronously would delete the widget that is still handling
    // the mouse release inside the reorderer's event filter. The tag may be gone
    // by then (a merge), hence the check inside the lambda.
    QTimer::singleShot(0,this,[this,owner,tag,text]{
        if (core != tag || !isCoreStillListed()) {
            if (core == tag)
                hideDisplay();
            return;
        }
        owner->tagEdit(tag,text);
    });
}

void teTagDisplayWidget::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        hideDisplay();
        event->accept();
        return;
    }
    teTagWidgetBase::keyPressEvent(event);
}
