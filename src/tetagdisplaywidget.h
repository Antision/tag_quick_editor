#ifndef TETAGDISPLAYWIDGET_H
#define TETAGDISPLAYWIDGET_H
#include "tetag.h"

class QFlowLayout;
class QFlowLayoutReorderer;
class teTagListWidgetBase;

/**
 * @brief Floating, magnified view of the tag the pointer currently rests on.
 *
 * The popup shares the tag's `teTagCore` with the tag list, so anything done
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
class teTagDisplayWidget : public teTagBase
{
    Q_OBJECT
public:
    explicit teTagDisplayWidget(teTagListWidgetBase* owner);
    ~teTagDisplayWidget();

    /// Makes the popup show `source` and places it next to `globalMousePos`.
    void showFor(teTagBase* source,const QPoint& globalMousePos);
    /**
     * @brief Shows the popup for a tag that has no widget of its own.
     *
     * Used by the model/view based tag list, where a row is drawn by a delegate
     * and there is no teTagBase to point at. Dragging the popup body (which
     * moves the source tag) is only available in the widget based list; word
     * dragging and double-click editing work the same.
     */
    void showForCore(std::shared_ptr<teTagCore> tagCore,const QPoint& globalMousePos);
    /// Hides the popup and forgets the tag.
    void hideDisplay();

    void readCore(std::shared_ptr<tetagcore> in_core) override;
    void load() override;
    void worddroped(teWordBase* in_word,int xpos) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

signals:
    /// The pointer left the popup; the owner decides whether to hide it.
    void pointerLeft();

protected:
    void leaveEvent(QEvent* event) override;

private:
    void startEditing(teWordBase* clickedWord);
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

    teTagListWidgetBase* m_owner=nullptr;
    teTagBase* m_source=nullptr;
    QFlowLayout* m_flow=nullptr;
    QFlowLayoutReorderer* m_wordReorderer=nullptr;
    QLineEdit* m_editor=nullptr;
    QPoint m_pressGlobal;
    int m_grabOffsetY=0;
    bool m_draggingTag=false;
};

#endif // TETAGDISPLAYWIDGET_H
