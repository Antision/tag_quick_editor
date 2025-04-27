#define _CRT_SECURE_NO_WARNINGS
#include<iostream>
#include<filesystem>
#include<fstream>
#include<set>
#include<map>
using namespace std;
// Storage format for tags read from .csv files using getline and load
struct tag_read {
    tag_read(string&& str, int frequency=0, tag_read* ref_tag = nullptr)
        :text(std::move(str)), frequency(frequency), ref_tag(ref_tag)
    {}
    string text;        // Tag content
    tag_read* ref_tag;  // Pointer to recommended tag in read_tags
    unsigned int frequency;  // Occurrence count
    unsigned int ref_tag_pos = 0xffffffffu;  // Index of recommended tag in read_tags
    bool operator<(const tag_read& in) const{
        return text < in.text;
    }
};

// Storage format for tags read from .tag files
struct ctag {
    unsigned int length;
    char* text;
    unsigned int frequency = 0;
    unsigned int ref_pos;
    ctag() {}
    ctag(ifstream& f) {
        if (f.flags() | ios::binary) {
            f.read((char*)&length, sizeof(unsigned int));
            text = new char[length+1];
            f.read(text, length);
            text[length] = '\0';
            f.read((char*)&ref_pos, sizeof(unsigned int));
            if (ref_pos == 0xffffffffu) {
                f.read((char*)&frequency, sizeof(unsigned int));
            }
        }
        else{
            cout << "read flag should be binary\n";
        }
    }
    // Operator overload: Dictionary order comparison
    bool operator<(const ctag& other) const {
        return std::strcmp(text, other.text) < 0;
    }
};
vector<tag_read*> read_tags;

// Finds the range in read_tags where all tags start with the given prefix (str)
// Returns a pair of indices (start, end) where end is one past the last element
// Uses binary search with custom comparison functions
std::pair<size_t, size_t> find_prefix_range(const std::vector<ctag>& read_tags, const char* str) {
    auto compare = [](const ctag& w, const char* s) {
        return std::strncmp(w.text, s, std::strlen(s)) < 0;
    };

    auto compare_end = [](const char* s, const ctag& w) {
        return std::strncmp(s, w.text, std::strlen(s)) < 0;
    };

    auto start = std::lower_bound(read_tags.begin(), read_tags.end(), str, compare);
    auto end = std::upper_bound(read_tags.begin(), read_tags.end(), str, compare_end);

    return { start - read_tags.begin(), end - read_tags.begin() };
}

// Refines search range after modifying the prefix
// Constructs new string by replacing last n characters with new_content
// Searches within the previously found range [left, right)
std::pair<size_t, size_t> refine_range(
    const std::vector<ctag>& readtags,
    const char* str,
    size_t left,
    size_t right,
    const char* new_content,
    unsigned int n)
{
    size_t str_len = std::strlen(str);
    size_t new_content_len = new_content ? std::strlen(new_content) : 0;

    // Construct temporary string
    char* new_str = new char[str_len - n + new_content_len + 1];
    std::strncpy(new_str, str, str_len - n);
    if (new_content) {
        std::strcpy(new_str + str_len - n, new_content);
    }

    // Binary search within specified range
    auto compare = [](const ctag& w, const char* s) {
        return std::strncmp(w.text, s, std::strlen(s)) < 0;
    };

    auto compare_end = [](const char* s, const ctag& w) {
        return std::strncmp(s, w.text, std::strlen(s)) < 0;
    };

    auto start = std::lower_bound(readtags.begin() + left, readtags.begin() + right, new_str, compare);
    auto end = std::upper_bound(readtags.begin() + left, readtags.begin() + right, new_str, compare_end);

    delete[] new_str; // Cleanup temporary memory
    return { start - readtags.begin(), end - readtags.begin() };
}

// Comparison function for sorting read_tags by text
bool cmp(tag_read*a, tag_read*b) {
    return a->text < b->text;
}

// Maps tags to their indices in read_tags
map<tag_read*, int> ref_tags;

// Removes underscores from tag strings
void removeunderline(string&str) {
    std::replace(str.begin(), str.end(), '_', ' ');
}

// Parses a CSV line and inserts tags into read_tags
void load(string& str) {
    if (str.back() == '"')
    str.pop_back();
else if (str.back() != ',')
    cout << "exception\n";

static int b_pos = 0;
static int n_pos = 0;
n_pos = str.find(',');
if (n_pos == -1) {
    cout << "exception\n";
    n_pos == str.size() - 1;
}
tag_read* reftagptr = new tag_read(str.substr(0, n_pos));
removeunderline(reftagptr->text);
read_tags.push_back(reftagptr);
b_pos = str.find(',', n_pos+1)+1;
if (b_pos == 0) {
    reftagptr->frequency = 1;
    cout << "exception\n";
    return;
}
n_pos = str.find('"', b_pos) - 1;
if (n_pos == -2) {
    n_pos== str.find(',', b_pos);
    if(n_pos==-1)
    {
        cout << "exception\n";
        n_pos = str.size() - 1;
    }
}
static int frequency;
frequency = atoi(str.substr(b_pos, n_pos - b_pos).c_str());
reftagptr->frequency = frequency;
static string nstr; 
b_pos = n_pos +2;
if (b_pos == 0)return;
while (true)
{
    n_pos = str.find(',', b_pos + 1);
    if (n_pos == -1) {
        nstr = str.substr(b_pos);
        removeunderline(nstr);
        read_tags.push_back(new tag_read{ std::move(nstr),frequency, reftagptr });
        return;
    }
    else {
        nstr = str.substr(b_pos, n_pos - b_pos);
        removeunderline(nstr);
        read_tags.push_back(new tag_read{ std::move(nstr),frequency ,reftagptr });
    }
    b_pos = str.find(',', b_pos + 1) + 1;
    if (b_pos >= str.length()) return;
}
}

// Binary search implementation for tag lookup
int binarySearch(int begin, int end, tag_read* value) {
    int low = begin, high = end;
	while (low < high) {
		int mid = low + (high - low) / 2;
		if (*read_tags[mid] < *value) {
			low = mid + 1;
		}
		else if (*value < *read_tags[mid]) {
			high = mid;
		}
		else {
			return mid;
		}
	}
	return end;
}
vector<ctag> ctags;

int main() {
    read_tags.reserve(242070);          // Pre-allocate memory for expected number of tags
    string buffer;
    buffer.reserve(1024);               // Buffer for reading CSV lines
    string folder("./");                // Target directory for CSV files
    // Load tags from CSV files into read_tags
    for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(folder)) {
        if (entry.path().extension() == ".csv") {
            ifstream file(entry.path());
            while (getline(file, buffer, '\n')) {
                load(buffer);  // Process each line of CSV file
            }
            file.close();
        }
    }

    // Sort read_tags in dictionary order
    sort(read_tags.begin(), read_tags.end(), cmp);

    // After sorting, find indices for each tag's reference tag in read_tags
    unsigned int size = read_tags.size();
    for (int i = 0; i < size; ++i) {
        if (read_tags[i]->ref_tag != nullptr) {
            int reftagpos = -1;
            // Check if reference tag already exists in map
            map<tag_read*, int>::iterator reftag_it = ref_tags.find(read_tags[i]->ref_tag);
            if (reftag_it != ref_tags.end())
                reftagpos = reftag_it->second;
            else {
                // Binary search for reference tag if not found
                reftagpos = binarySearch(0, size, read_tags[i]->ref_tag);
                if (reftagpos == size)
                    cout << "exception\n";  // Error handling for missing reference tag
                ref_tags[read_tags[i]->ref_tag] = reftagpos;
            }
            read_tags[i]->ref_tag_pos = reftagpos;
        }
    }

    // Write processed tags to binary .tag file
    ofstream f("./tag.tag", ios::binary);
    f.write((char*)&size, sizeof(unsigned int));  // Write total tag count
    unsigned int oint;
    for (int i = 0; i < size; ++i) {
        oint = read_tags[i]->text.size();
        f.write((char*)&oint, sizeof(unsigned int));  // Write tag length
        f.write(read_tags[i]->text.data(), oint);     // Write tag content
        oint = read_tags[i]->ref_tag_pos;
        f.write((char*)&oint, sizeof(unsigned int));  // Write reference tag index
        if (oint == 0xffffffffu) {  // Special case for root recommendation tags
            oint = read_tags[i]->frequency;
            f.write((char*)&oint, sizeof(unsigned int));  // Write occurrence frequency
        }
    }
    f.close();

    // DEBUG SECTION - Example usage of tag searching functions
#ifdef DEBUG
    // Read tags back from .tag file for testing
    ifstream inf("./tag.tag", ios::binary);
    unsigned int inwords_size;
    inf.read((char*)&inwords_size, sizeof(unsigned int));
    ctags.reserve(inwords_size);
    for (int i = 0; i < inwords_size; ++i) {
        ctags.push_back(ctag(inf));
    }
    inf.close();

    // Test prefix search
    auto p = find_prefix_range(ctags, "sh");
    cout << ctags[p.first].text;
    cout << ctags[p.second].text;

    // Test refined search after modifying prefix
    p = refine_range(ctags, "sh", p.first, p.second, "a", 1);
    cout << ctags[p.first].text;
    cout << ctags[p.second].text;
#endif // DEBUG
}