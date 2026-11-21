#include "linkgeometry.h"

#include <algorithm>
#include <iterator>

namespace {
/// Green, red, cyan, orange, blue, yellow, purple: neighbouring hues, so two
/// lines that leave the same corner still read as two lines.
const QColor kLinkColors[] = {
    QColor(0x00,0xd9,0x6d),     // green (the selection colour)
    QColor(0xff,0x4d,0x4d),     // red
    QColor(0x22,0xd3,0xee),     // cyan
    QColor(0xff,0x9f,0x43),     // orange
    QColor(0x4c,0x8d,0xff),     // blue
    QColor(0xff,0xd9,0x3d),     // yellow
    QColor(0xb2,0x88,0xff),     // purple
};
constexpr int kLinkColorCount=int(std::size(kLinkColors));
}

QColor linkColor(int index)
{
    if(kLinkColorCount<=0)
        return QColor(0,0xd9,0x6d);
    const int normalized=((index%kLinkColorCount)+kLinkColorCount)%kLinkColorCount;
    return kLinkColors[normalized];
}

int linkColorCount()
{
    return kLinkColorCount;
}

QPoint linkInsetPoint(const QRect& rect,const QPoint& point,int inset)
{
    if(rect.isEmpty())
        return point;
    // A rect narrower than two insets has no room for an inset point; its centre
    // is the only honest answer.
    if(rect.width()<=2*inset||rect.height()<=2*inset)
        return rect.center();
    const QRect inner=rect.adjusted(inset,inset,-inset,-inset);
    return QPoint(std::clamp(point.x(),inner.left(),inner.right()),
                  std::clamp(point.y(),inner.top(),inner.bottom()));
}

bool linkEndpoints(const QRect& control,const QRect& row,QPoint* from,QPoint* to,
                   int inset)
{
    if(control.isEmpty()||row.isEmpty())
        return false;

    // The tag side: the middle of the row. A row scrolled above the list keeps its
    // real position, so this may be outside the viewport - on purpose.
    const QPoint center=row.center();

    // The control side: the border point closest to the tag, pulled `inset` inside
    // so the dot never sits on the border.
    const QPoint nearest(std::clamp(center.x(),control.left(),control.right()),
                         std::clamp(center.y(),control.top(),control.bottom()));
    if(from)
        *from=linkInsetPoint(control,nearest,inset);
    if(to)
        *to=center;
    return true;
}
