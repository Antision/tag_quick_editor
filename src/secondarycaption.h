#ifndef SECONDARYCAPTION_H
#define SECONDARYCAPTION_H

#include <QFontMetrics>
#include <QString>
#include <QStringList>

/**
 * @brief How a SecondaryButton's caption is broken into lines.
 *
 * The button used to print "<first tag> …" for every multi-tag capture, which
 * says "there is more" but not what. A menu button is often wide enough to name
 * two or three tags, so the caption now uses the width it is given: as many tags
 * per line as fit, up to `maxLines` lines, and only then does it fall back to an
 * ellipsis in the middle.
 *
 * The elision is a *middle* one, not a tail one: the first and the last tags
 * carry the meaning ("1girl, solo, …, from behind"), and keeping a prefix plus a
 * suffix says more than keeping only the prefix.
 */
struct CaptionLayout
{
    QStringList lines;      ///< the text of each line, already wrapped
    int shownTags = 0;      ///< tags that are readable (the elided ones are not)
    bool elided = false;

    int lineCount() const { return int(lines.size()); }
};

/**
 * @brief Lays `tags` out for a button `width` wide and `maxLines` tall.
 *
 * Pure: the regression check asserts the wrapping and the middle elision without
 * a window. `width <= 0` yields an empty layout (the widget has not been sized
 * yet), and an empty tag list yields an empty layout as well - the caller shows
 * its placeholder in that case.
 */
CaptionLayout layoutCaption(const QStringList& tags, const QFontMetrics& metrics,
                            int width, int maxLines);

#endif // SECONDARYCAPTION_H
