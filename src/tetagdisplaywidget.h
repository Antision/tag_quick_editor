#ifndef TETAGDISPLAYWIDGET_H
#define TETAGDISPLAYWIDGET_H
#include "tetag.h"
#include "tagviewcontracts.h"

class QFlowLayout;
class QFlowLayoutReorderer;
class teTagListWidgetBase;
class TagListView;

/**
 * @brief Floating, magnified view of the tag the pointer currently rests on.
 *
 * The popup shares the tag's `teTag` with the tag list, so anything done
 * here shows up there immediately:
 *  - its words are layed out by a QFlowLayout, i.e. a long tag wraps instead of
 *    being clipped, and word positions stay meaningful across lines;
 *  - a word can be dragged to another position (also across lines);
 *  - double-clicking a word opens an inline editor with that word preselected;
 *    double-clicking a *sentence* tag closes the popup and opens the plain text
 *    editor of the owning tag list instead;
 *  - dragging the background moves the tag inside the tag list (the same
 *    behaviour the tag itself has) and dropping it reorders the list.
 *
 * The popup is a focus-less tool window so it neither steals the keyboard from
 * the tag list nor disappears when the pointer moves from the tag to the popup.
 */
class teTagDisplayWidget : public QFrame, public teObject, public TagDisplay
{
    Q_OBJECT
public:
    explicit teTagDisplayWidget(TagDisplayOwner* owner);
    ~teTagDisplayWidget();

    /// Makes the popup show `source` and places it next to `globalMousePos`.
    void showFor(teTagWidgetBase* source,const QPoint& globalMousePos);
    /**
     * @brief Shows the popup for a tag that has no widget of its own.
     *
     * Used by the model/view based tag list, where a row is drawn by a delegate
     * and there is no teTagBase to point at. Dragging the popup body (which
     * moves the source tag) is only available in the widget based list; word
     * dragging and double-click editing work the same.
     */
    void showForCore(std::shared_ptr<teTag> tagCore,const QPoint& globalMousePos) override;
    /// The tag shown. The popup used to inherit teTagWidgetBase just to get this
    /// member; the base also brought a widget-per-word layout the popup does not
    /// use (it has its own flow layout), so it is a plain QFrame + teObject now.
    std::shared_ptr<teTag> core;
    /// Hides the popup and forgets the tag.
    void hideDisplay() override;
    // -- TagDisplay -----------------------------------------------------------
    QWidget* widget() override { return this; }
    bool isVisible() const override { return QFrame::isVisible(); }
    QRect frameGeometry() const override { return QFrame::frameGeometry(); }
    void setPointerLeftHandler(std::function<void()> handler) override {
        m_pointerLeft=std::move(handler);
    }
    /// True while the tag it shows is still part of the owner's list.
    bool isCoreStillListed() const;

    void readCore(std::shared_ptr<teTag> in_core);
    void load();
    void worddroped(teWordWidgetBase* in_word,int xpos);
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

protected:
    void leaveEvent(QEvent* event) override;
    /// Closes the popup when the mouse is pressed outside it and outside the tag
    /// list (installed on the application while the popup is up).
    bool eventFilter(QObject* watched,QEvent* event) override;

private:
    void startEditing(teWordWidgetBase* clickedWord);
    void finishEditing(bool accept);
    /// Rewrites core->words from the current layout order (after a word drag).
    void syncWordOrderFromLayout();
    /// Deletes our own word widgets without clearing their core's `widget`
    /// pointer, which belongs to the tag list.
    void deleteWordWidgets();
    /// Point size of the magnified words (1.5x, but 1x for sentence tags).
    int displayPointSize() const;
    /// Character offset of word `index` inside the serialised tag text.
    int wordOffsetInText(int index) const;
    void placeNextTo(const QPoint& globalMousePos);
    /// Widest the popup (and a wrapped sentence word) may become.
    int maxContentWidth() const;
    /// Width a wrapped sentence label takes: the popup's own width when it is on
    /// screen, the cap before that.
    int sentenceWordWidth() const;
    /// Makes a wrapped sentence fill the popup after it was placed or reloaded.
    void fitWrappedSentence();
    /// Runs the flow layout synchronously (Qt defers it while hidden).
    void layOutNow();
    /// Hides the words while the inline editor covers the popup, and brings them
    /// back when it closes.
    void hideWordWidgets();
    void showWordWidgets();
    /// Translucent while it only hangs next to the pointer (clicks and the wheel
    /// must reach the list behind it); opaque once it is being used.
    void showIdle();
    void setOpaque();

    TagDisplayOwner* m_owner=nullptr;
    teTagWidgetBase* m_source=nullptr;
    /// The tag shown. The popup used to inherit teTagWidgetBase just to get this
    /// member; the base also brought a widget-per-word layout the popup does not
    /// use (it has its own flow layout), so it is a plain QFrame + teObject now.
    /// True while this widget filters the application's events (popup is up).
    bool m_watching=false;
    /// Width the popup would like (single line, uncapped); the cap is applied when
    /// it is placed.
    int m_naturalWidth=0;
    /// Where it was placed last, so a reload while shown can refit itself.
    QPoint m_lastAnchor;
    /// The view's "pointer left" callback (see TagDisplay).
    std::function<void()> m_pointerLeft;
    QFlowLayout* m_flow=nullptr;
    QFlowLayoutReorderer* m_wordReorderer=nullptr;
    QPlainTextEdit* m_editor=nullptr;
    QPoint m_pressGlobal;
    int m_grabOffsetY=0;
    bool m_draggingTag=false;
};

#endif // TETAGDISPLAYWIDGET_H
