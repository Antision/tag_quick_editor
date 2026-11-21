#ifndef CONNECTIVITYOVERLAY_H
#define CONNECTIVITYOVERLAY_H

/**
 * @file connectivityoverlay.h
 * @brief The capture lines between editor controls and the tag rows they own (S7).
 *
 * One transparent sheet over the splitter area. It draws nothing by itself and
 * takes no input: the pointer decides what is drawn (LINK-01), and everything
 * geometric lives in linkgeometry.* so it can be tested without a window.
 */

#include "linkgeometry.h"

#include <QRect>
#include <QVector>
#include <QWidget>

#include <memory>

class MainWindow;
class TagListView;
class TagListModel;
class teEditorList;
class teTag;
class QTimer;

/**
 * @brief Mouse-transparent painter of control -> tag links.
 *
 * Triggers (scroll, resize, layout switch, tag add/remove/edit, hover change,
 * image switch) only call schedule(); the 16ms timer turns a burst of them into
 * one repaint, which is what keeps dragging a splitter at 60Hz cheap (LINK-05).
 */
class ConnectivityOverlay : public QWidget
{
    Q_OBJECT
public:
    ConnectivityOverlay(MainWindow* window,TagListView* tagView,teEditorList* editors);

    /**
     * @brief Decides what the pointer is over and repaints when that changed.
     *
     * Public because it *is* the entry point: the application-wide mouse filter
     * calls it, and the regression harness calls it with the real global position
     * of a widget (a synthetic mouse event never reaches an application filter,
     * so there is no second code path to test).
     */
    void updateHoverAt(const QPoint& globalPos);
    /// Coalesces any number of triggers into one repaint.
    void schedule();
    /// Segments drawn by the last paint (0 when the pointer is not over a source).
    int lastSegmentCount() const { return m_lastSegments; }
protected:
    void paintEvent(QPaintEvent* event) override;
    bool eventFilter(QObject* watched,QEvent* event) override;
private:
    /// What the pointer is over; only these two states draw anything.
    enum class Hover { None, Column, TagRow };

    /// Moves/resizes the sheet onto the splitter area (and keeps it on top).
    void reposition();
    /// Installs the scroll/resize hooks on pages that appeared after construction.
    void attachPages();
    void attachPage(QWidget* page);
    void attachModel(TagListModel* model);
    /// Recomputes m_segments for the current hover state.
    void refreshSegments();
    /// Bounding rect of the current segments (null when there are none).
    QRect segmentsBounds() const;
    /**
     * @brief Repaints only the lines that changed.
     *
     * The sheet is not opaque, so every dirty pixel also repaints whatever is
     * below it. Repainting the whole splitter on every scroll event would repaint
     * the picture list's thumbnails 60 times a second; the union of the old and
     * the new line bounds is a far smaller region.
     */
    void repaintArea(const QRect& previous);
    /// Collects the segments for the current hover state, in this widget's coords.
    void collectSegments(QVector<LinkSegment>& out) const;
    void collectPageLinks(int pageIndex,TagListModel* model,
                          const QPoint& origin,QVector<LinkSegment>& out,
                          const teTag* onlyTag) const;
    int pageCount() const;

    MainWindow* m_window=nullptr;
    TagListView* m_tagView=nullptr;
    teEditorList* m_editors=nullptr;
    QTimer* m_timer=nullptr;
    Hover m_hover=Hover::None;
    int m_hoverPage=-1;
    /// The row under the pointer while it is over the tag list. A weak pointer:
    /// the list can be reloaded between two paints.
    std::weak_ptr<teTag> m_hoverTag;
    /// The segments of the last refresh; paintEvent only draws them.
    QVector<LinkSegment> m_segments;
    int m_lastSegments=0;
};

#endif // CONNECTIVITYOVERLAY_H
