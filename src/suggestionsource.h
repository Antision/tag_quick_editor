#ifndef SUGGESTIONSOURCE_H
#define SUGGESTIONSOURCE_H

#include "tagviewcontracts.h"
#include "ctag.h"
#include <QHash>
#include <mutex>

/**
 * @brief tqe's SuggestionSource: the `ctags` index (TV-03).
 *
 * The prefix search itself is unchanged - a binary search into the sorted tag
 * array, then the most frequent hits in that range - but it no longer lives in
 * the line edit. The range of every prefix is cached, so typing one more
 * character only narrows the previous range (the refinement the editor used to
 * do with its own l/r members). A range is a property of the prefix, not of the
 * editor, which is why two editors can share the cache.
 */
class CtagsSuggestionSource : public SuggestionSource
{
public:
    /// Searches `index` (sorted by text). The default is the program's index;
    /// tests pass a small vector of their own.
    explicit CtagsSuggestionSource(std::vector<ctag>& index=ctags);
    QVector<SuggestionItem> suggest(const QString& prefix,int limit) override;

private:
    struct Range{
        size_t left=0;
        size_t right=0;
    };
    std::vector<ctag>& m_tags;
    std::mutex m_mutex;
    QHash<QString,Range> m_ranges;
    /// Size of the index when the cache was built. The index is loaded once, but
    /// a reload would invalidate every cached range.
    size_t m_tagsSize=0;
};

#endif // SUGGESTIONSOURCE_H
