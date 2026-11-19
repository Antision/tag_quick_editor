#include "suggestionsource.h"
#include "ctag.h"

CtagsSuggestionSource::CtagsSuggestionSource(std::vector<ctag>& index)
    :m_tags(index)
{
}

QVector<SuggestionItem> CtagsSuggestionSource::suggest(const QString& prefix,int limit)
{
    QVector<SuggestionItem> out;
    if(prefix.isEmpty()||limit<=0||m_tags.empty())
        return out;

    std::lock_guard<std::mutex> lg(m_mutex);
    if(m_tagsSize!=m_tags.size()){
        m_ranges.clear();
        m_tagsSize=m_tags.size();
    }

    size_t left=0,right=0;
    bool narrowed=false;
    // Longest proper prefix already looked up. Appending characters can only
    // narrow a range, so the previous search is reused instead of binary
    // searching from scratch.
    for(int cut=int(prefix.size())-1;cut>0&&!narrowed;--cut){
        const QString shorter=prefix.left(cut);
        const auto it=m_ranges.constFind(shorter);
        if(it==m_ranges.constEnd())
            continue;
        const auto [appended,removed]=compareStrings(shorter,prefix);
        const auto [l,r]=refine_range(m_tags,shorter.toLatin1().data(),it->left,it->right,appended,removed);
        delete[] appended;
        left=l;
        right=r;
        narrowed=true;
    }
    if(!narrowed){
        const auto [l,r]=find_prefix_range(m_tags,prefix.toLatin1().data());
        left=l;
        right=r;
    }
    if(left>=right)
        return out;

    m_ranges.insert(prefix,Range{left,right});
    if(m_ranges.size()>128)
        m_ranges.clear();            // bounded; a cleared cache only costs one search

    auto highest=getSuggestionTagsArray(m_tags,int(left),int(right),limit);
    out.reserve(highest.size());
    // Most frequent first; the suggestion box inserts each one on top, which is
    // the order it has always shown (best match ends up on top of the list).
    while(!highest.empty()){
        const auto [frequency,index]=highest.top();
        highest.pop();
        const ctag& tag=m_tags[index];
        SuggestionItem item;
        if(tag.ref_pos!=0xffffffffu&&tag.ref_pos<m_tags.size()){
            item.text=QString(m_tags[tag.ref_pos].text);
            item.display=QStringLiteral("%1 -> %2 (%3)").arg(tag.text,item.text).arg(tag.frequency);
        }else{
            item.text=QString(tag.text);
            item.display=QStringLiteral("%1 (%2)").arg(item.text).arg(tag.frequency);
        }
        out.append(item);
    }
    return out;
}
