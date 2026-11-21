#ifndef LINKGEOMETRY_H
#define LINKGEOMETRY_H

/**
 * @file linkgeometry.h
 * @brief Where a capture line starts and ends (LINK-02..04).
 *
 * Pure geometry: no widgets, no painting. ConnectivityOverlay feeds it the
 * rectangles it sees, which is also what makes the rules testable without a real
 * window (see testConnectionEndpoints).
 *
 * The two ends are deliberately different. The tag side goes to the middle of the
 * row; the control side goes to the point of the control nearest the tag. The tag
 * list and the first editor column are only a scrollbar apart, so an
 * edge-to-edge rule piles every line onto that one border, and the lines cannot
 * be told apart.
 *
 * Nothing here clips against a viewport: a row scrolled out of the list still has
 * a real position, and the line is supposed to leave the window to reach it.
 * The overlay's own edges clip the drawing, and an end that is off screen simply
 * has no dot.
 */

#include <QColor>
#include <QPoint>
#include <QRect>

/// One drawn link, in whatever coordinate space the rects came in.
struct LinkSegment {
    bool visible=false;
    QPoint from;        ///< inside the control (the line starts here)
    QPoint to;          ///< the centre of the tag row
};

/// How far inside the control its end sits (LINK-03; was 3, the columns are too
/// close for that - see the file comment).
constexpr int kLinkInset=6;

/// The colour order of the lines (LINK-05): green, red, cyan, orange, blue,
/// yellow, purple, then green again.
QColor linkColor(int index);
/// How many colours the cycle has.
int linkColorCount();

/**
 * @brief The point `inset` pixels inside `rect`, on the border nearest `point`.
 *
 * A point already inside the deflated rect is kept; one on (or outside) a border
 * is pulled inside by `inset` on that axis, which is what keeps a dot off the
 * border.
 */
QPoint linkInsetPoint(const QRect& rect,const QPoint& point,int inset);

/**
 * @brief The two ends of one link (LINK-02).
 *
 * `from` is the point of `control` nearest the tag, `inset` pixels inside its
 * border; `to` is the centre of the tag row.
 *
 * @return false when either rect is empty.
 */
bool linkEndpoints(const QRect& control,const QRect& row,QPoint* from,QPoint* to,
                   int inset=kLinkInset);

#endif // LINKGEOMETRY_H
