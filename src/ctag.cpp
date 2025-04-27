#include "ctag.h"
std::vector<ctag> ctags;

ctag::ctag() {}

ctag::ctag(std::ifstream &f) {
    if (f.flags() | std::ios::binary) {
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
        std::cout << "read flag should be binary\n";
    }
}

bool ctag::operator<(const ctag &other) const {
    return std::strcmp(text, other.text) < 0;
}

std::pair<size_t, size_t> find_prefix_range(const std::vector<ctag> &read_tags, char *str) {
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

std::pair<size_t, size_t> refine_range(const std::vector<ctag> &readtags, const char *str, size_t left, size_t right, char *new_content, unsigned int n)
{
    size_t str_len = std::strlen(str);
    size_t new_content_len = new_content ? std::strlen(new_content) : 0;

    char* new_str = new char[str_len - n + new_content_len + 1];
    std::strncpy(new_str, str, str_len - n);
    if (new_content) {
        std::strcpy(new_str + str_len - n, new_content);
    }

    auto compare = [](const ctag& w, const char* s) {
        return std::strncmp(w.text, s, std::strlen(s)) < 0;
    };

    auto compare_end = [](const char* s, const ctag& w) {
        return std::strncmp(s, w.text, std::strlen(s)) < 0;
    };

    auto start = std::lower_bound(readtags.begin() + left, readtags.begin() + right, new_str, compare);
    auto end = std::upper_bound(readtags.begin() + left, readtags.begin() + right, new_str, compare_end);

    delete[] new_str;
    return { start - readtags.begin(), end - readtags.begin() };
}

using namespace std;

/**
 * @brief Temporary storage structure for unordered tags during .csv to .tag file conversion
 *        Used when local tag.tag file is missing and auto-converting from danbooru_e621_merged.csv
 */
struct tag_read {
    tag_read(string&& str, int frequency=0, tag_read* ref_tag = nullptr)
        :text(std::move(str)), frequency(frequency), ref_tag(ref_tag)
    {}
    string text;
    tag_read* ref_tag;
    unsigned int frequency;
    unsigned int ref_tag_pos = 0xffffffffu;
    bool operator<(const tag_read& in) const{
        return text < in.text;
    }
};

vector<tag_read*> read_tags;
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

    char* new_str = new char[str_len - n + new_content_len + 1];
    std::strncpy(new_str, str, str_len - n);
    if (new_content) {
        std::strcpy(new_str + str_len - n, new_content);
    }

    auto compare = [](const ctag& w, const char* s) {
        return std::strncmp(w.text, s, std::strlen(s)) < 0;
        };

    auto compare_end = [](const char* s, const ctag& w) {
        return std::strncmp(s, w.text, std::strlen(s)) < 0;
        };

    auto start = std::lower_bound(readtags.begin() + left, readtags.begin() + right, new_str, compare);
    auto end = std::upper_bound(readtags.begin() + left, readtags.begin() + right, new_str, compare_end);

    delete[] new_str;
    return { start - readtags.begin(), end - readtags.begin() };
}

bool cmp(tag_read*a, tag_read*b) {
    return a->text < b->text;
}
map<tag_read*, int> ref_tags;
void removeunderline(string&str) {
    std::replace(str.begin(), str.end(), '_', ' ');
}
void load(string& str) {
    if (str.back() == '\r')
        str.pop_back();
    if (str.back() == '"')
        str.pop_back();
    // else if (str.back() != ',')
    //     telog("");

    static int b_pos = 0;
    static int n_pos = 0;
    n_pos = str.find(',');
    if (n_pos == -1) {
        telog("");
        n_pos == str.size() - 1;
    }
    tag_read* reftagptr = new tag_read(str.substr(0, n_pos));
    removeunderline(reftagptr->text);
    read_tags.push_back(reftagptr);
    b_pos = str.find(',', n_pos+1)+1;
    if (b_pos == 0) {
        reftagptr->frequency = 1;
        telog("");
        return;
    }
    n_pos = str.find('"', b_pos) - 1;
    if (n_pos == -2) {
        n_pos== str.find(',', b_pos);
        if(n_pos==-1)
        {
            telog("");
            n_pos = str.size() - 1;
        }
    }
    static int frequency;
    frequency = atoi(str.substr(b_pos, n_pos - b_pos).c_str());
    reftagptr->frequency = frequency;
    static string nstr;
    b_pos = n_pos + 2;
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

void readCSV(stringstream&& ss) {
    read_tags.reserve(262144);
    string buffer;
    buffer.reserve(1024);
    string folder("./");
    while (getline(ss, buffer, '\n')) {
        load(buffer);
    }
    sort(read_tags.begin(), read_tags.end(), cmp);
    unsigned int size = read_tags.size();
    for (int i = 0; i < size; ++i) {
        if (read_tags[i]->ref_tag != nullptr) {
            int reftagpos=-1;
            map<tag_read*, int>::iterator reftag_it = ref_tags.find(read_tags[i]->ref_tag);
            if (reftag_it != ref_tags.end())
                reftagpos = reftag_it->second;
            else {
                reftagpos = binarySearch(0, size, read_tags[i]->ref_tag);
                if (reftagpos == size)
                    telog("");
                ref_tags[read_tags[i]->ref_tag] = reftagpos;
            }
            read_tags[i]->ref_tag_pos = reftagpos;
        }
    }
    ofstream f("./tag.tag",ios::binary);
    ctags.reserve(size+1);
    f.write((char*)&size, sizeof(unsigned int));
    for (int i = 0; i < size; ++i) {
        unsigned int length = read_tags[i]->text.size();
        f.write((char*)&length, sizeof(unsigned int));
        f.write(read_tags[i]->text.data(), length);
        unsigned int ref_pos = read_tags[i]->ref_tag_pos;
        f.write((char*)&ref_pos, sizeof(unsigned int));
        if(ref_pos== 0xffffffffu){
            unsigned int frequency = read_tags[i]->frequency;
            f.write((char*)&frequency, sizeof(unsigned int));
            ctags.push_back({length,read_tags[i]->text.data(),frequency,ref_pos});
        }
        else
            ctags.push_back({length,read_tags[i]->text.data(),0,ref_pos});
    }
    f.close();
}
void download_tags(){
    QNetworkAccessManager manager;
    const QUrl url("https://raw.githubusercontent.com/DominikDoom/a1111-sd-webui-tagcomplete/main/tags/danbooru_e621_merged.csv");

    QNetworkRequest request(url);
    QNetworkReply* reply = manager.get(request);

    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray data = reply->readAll();
        readCSV(std::stringstream(data.toStdString()));
    } else {
        qDebug() << "failed to download danbooru tags" << reply->errorString();
    }
    reply->deleteLater();
}
void load_tags(){
    std::ifstream inf("./tag.tag", std::ios::binary);
    if(!inf.is_open()){
        telog("could not open tag path");
        thread_pool.add(download_tags);
        return;
    }
    unsigned int inwords_size;
    inf.read((char*)&inwords_size, sizeof(unsigned int));
    ctags.reserve(inwords_size);
    for (int i = 0; i < inwords_size; ++i) {
        ctags.push_back(ctag(inf));
    }
    inf.close();
    for (int i = 0; i < inwords_size; ++i) {
        ctag&thistag = ctags[i];
        if(thistag.frequency==0)
        {
            if(thistag.ref_pos>inwords_size-1){
                telog(QString("[load_tags]:Tag ref to a index which is out of range"));
                continue;
            }
            thistag.frequency=ctags[thistag.ref_pos].frequency;
        }
    }
}

std::pair<char *, int> compareStrings(const QString &str1, const QString &str2) {
    int len1 = str1.length();
    int len2 = str2.length();
    int diffIndex = 0;
    while (diffIndex < len1 && diffIndex < len2 && str1[diffIndex] == str2[diffIndex]) {
        diffIndex++;
    }

    QString subStr = str2.mid(diffIndex);
    int subStrLength = subStr.length()+1;
    char* cStyleSubStr = new char[subStrLength];
    memcpy(cStyleSubStr,subStr.toLatin1().data(),subStrLength);

    int charsToRemove = len1 - diffIndex;

    return {cStyleSubStr, charsToRemove};
}

getSuggestionTagsArray_type getSuggestionTagsArray(std::vector<ctag>& ctags, int l, int r, int max) {
    std::priority_queue<TagIndexPair, std::vector<TagIndexPair>, TagIndexPairCmp> minHeap(TagIndexPairCmp{});
    for (int i = l; i < r; ++i) {
        minHeap.push({ctags[i].frequency, i});
        if (minHeap.size() > max) {
            minHeap.pop();
        }
    }
    return minHeap;
}

