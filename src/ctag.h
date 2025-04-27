#ifndef CTAG_H
#define CTAG_H
/**
 * @brief Storage format for tag data used in autocomplete functionality
 */
struct ctag {
    unsigned int length;///< Character length of the tag excluding the null terminator '\0'
    char* text;///< Memory address of the tag text
    unsigned int frequency = 0;///< Occurrence frequency of the tag
    unsigned int ref_pos;///< Index of the preferred synonym in ctags array. 0xffffffff if this is the most preferred tag
    ctag();
    ctag(std::ifstream& f);
    ctag(unsigned int in_length,char* in_text,unsigned int in_frequency,unsigned int in_ref_pos)
        :length(in_length),text(in_text),frequency(in_frequency),ref_pos(in_ref_pos){};
    bool operator<(const ctag& other) const;
};
/**
 * @brief Finds the index range of tags in the ctags array that can complete the input C-style string
 * @param read_tags The tags array to search in
 * @param str The C-style string to be completed
 * @return Range [left, right) containing valid completion tags
 */
std::pair<size_t, size_t> find_prefix_range(const std::vector<ctag>& read_tags, char* str);
/**
 * @brief Refines the tag range based on string modifications after previous prefix search
 * @param readtags The tags array to search in
 * @param str Original string used in previous search
 * @param left Previous range start index
 * @param right Previous range end index
 * @param new_content Newly appended content (C-style string)
 * @param n Number of characters deleted from the end before appending new content
 * @return New refined range [left, right)
 */
std::pair<size_t, size_t> refine_range(
    const std::vector<ctag>& readtags,
    const char* str,
    size_t left,
    size_t right,
    char* new_content,
    unsigned int n);

/**
 * @brief Loads tag.tag file contents into the ctags array
 */
void load_tags();

/**
 * @brief Stores tag data used for autocomplete functionality
 */
extern std::vector<ctag> ctags;

/**
 * @brief Compares two strings to get modification details for refine_range
 * @return Pair containing:
 *         - First: Heap-allocated new content string (must be manually freed)
 *         - Second: Number of characters deleted
 */
std::pair<char*, int> compareStrings(const QString& str1, const QString& str2);
using TagIndexPair = std::pair<unsigned int, unsigned int>; // {frequency, index}
struct TagIndexPairCmp{
    bool operator()(const TagIndexPair& a, const TagIndexPair& b) {
        return a.first > b.first;
    }
};
using getSuggestionTagsArray_type=  std::priority_queue<
    TagIndexPair,
    std::vector<TagIndexPair>,
    TagIndexPairCmp> ;

/**
 * @brief Gets top N most frequent tags within specified range
 * @param ctags Tag array to search
 * @param l Start index of range
 * @param r End index of range (exclusive)
 * @param max Maximum number of suggestions to return
 * @return Array of {frequency, index} pairs for top tags
 */
getSuggestionTagsArray_type getSuggestionTagsArray(std::vector<ctag>& ctags, int l, int r, int max);

#endif // CTAG_H
