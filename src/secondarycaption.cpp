#include "secondarycaption.h"

namespace {

const QString& separator()
{
    static const QString sep = QStringLiteral(", ");
    return sep;
}

/// Greedy wrap: a tag never gets split, a line breaks before the tag that would
/// not fit. A single tag wider than the line is elided in place.
QStringList wrapTokens(const QStringList& tokens, const QFontMetrics& metrics, int width)
{
    QStringList lines;
    QString current;
    int currentWidth = 0;
    for (const QString& token : tokens) {
        int tokenWidth = metrics.horizontalAdvance(token);
        if (tokenWidth > width)
            tokenWidth = width;
        if (current.isEmpty()) {
            current = tokenWidth == metrics.horizontalAdvance(token)
                          ? token
                          : metrics.elidedText(token, Qt::ElideRight, width);
            currentWidth = tokenWidth;
            continue;
        }
        const int added = metrics.horizontalAdvance(separator()) + tokenWidth;
        if (currentWidth + added <= width) {
            current += separator();
            current += tokenWidth == metrics.horizontalAdvance(token)
                           ? token
                           : metrics.elidedText(token, Qt::ElideRight, width);
            currentWidth += added;
        } else {
            lines.push_back(current);
            current = tokenWidth == metrics.horizontalAdvance(token)
                          ? token
                          : metrics.elidedText(token, Qt::ElideRight, width);
            currentWidth = tokenWidth;
        }
    }
    if (!current.isEmpty())
        lines.push_back(current);
    return lines;
}

int countLines(const QStringList& tokens, const QFontMetrics& metrics, int width)
{
    return int(wrapTokens(tokens, metrics, width).size());
}

} // namespace

CaptionLayout layoutCaption(const QStringList& tags, const QFontMetrics& metrics,
                            int width, int maxLines)
{
    CaptionLayout layout;
    if (tags.isEmpty() || width <= 0 || maxLines < 1)
        return layout;

    const int total = int(tags.size());

    // Everything fits: no ellipsis at all.
    if (countLines(tags, metrics, width) <= maxLines) {
        layout.lines = wrapTokens(tags, metrics, width);
        layout.shownTags = total;
        return layout;
    }

    // Too many tags for the lines we have. Keep a prefix and a suffix and put one
    // ellipsis between them, dropping the middle tags one at a time - so the
    // result names as many tags as the width allows, from both ends inwards.
    for (int keep = total - 1; keep >= 2; --keep) {
        const int prefix = (keep + 1) / 2;
        const int suffix = keep - prefix;
        QStringList tokens = tags.mid(0, prefix);
        tokens << QStringLiteral("\u2026");
        tokens += tags.mid(total - suffix);
        if (countLines(tokens, metrics, width) <= maxLines) {
            layout.lines = wrapTokens(tokens, metrics, width);
            layout.shownTags = keep;
            layout.elided = true;
            return layout;
        }
    }

    // Not even "first, …, last": elide the joined text into the first line.
    layout.lines.push_back(metrics.elidedText(tags.join(separator()), Qt::ElideRight, width));
    layout.shownTags = 1;
    layout.elided = true;
    return layout;
}
