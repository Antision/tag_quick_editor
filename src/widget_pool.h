#ifndef WIDGET_POOL_H
#define WIDGET_POOL_H
#include "tetag.h"
#include "tereftaglistwidget.h"
#include"tetaglistwidget.h"

struct teTagListWidget;
/**
 * @brief Manages global tetag and teword widget resources via pre-allocation and automatic recycling to avoid performance overhead from frequent creation/destruction.
 *
 * WidgetPool pre-allocates a fixed number of widgets and recycles them after use. When additional widgets are needed beyond the pre-allocated count,
 * they are temporarily created and stored in extra_tags, then destroyed during realloc().
 *
 * @tparam TagType Type of tag widget (must derive from teTagBase)
 * @tparam WordType Type of word widget (must derive from teWordBase)
 * @tparam total_tags_count Total number of pre-allocated tag widgets
 * @tparam total_words_count Total number of pre-allocated word widgets
 */
template<typename TagType,typename WordType,size_t total_tags_count,size_t total_words_count>requires (std::derived_from<TagType,teTagBase>&&std::derived_from<WordType,teWordBase>)
struct WidgetPool:teObject{
    std::vector<QWidget*>extra_tags;//< Stores pointers to temporarily created widgets when pre-allocated widgets are insufficient
    int tagpool_end;//< Stack top index for available tag widgets
    int wordpool_end;//< Stack top index for available word widgets
    int tagpool_initial_top=total_tags_count;//< Stack bottom index for initialized tag widgets (initially points to last pre-allocated tag)
    int wordpool_initial_top=total_words_count;//< Stack bottom index for initialized word widgets (initially points to last pre-allocated word)
    teTagListWidgetBase*parent=nullptr;//< Parent widget for all managed widgets
    QBoxLayout*parent_layout=nullptr;//< Layout container for all managed widgets
    TagType* tags_free[total_tags_count];//< Stack storing pre-allocated tag widgets
    WordType* words_free[total_words_count];//< Stack storing pre-allocated word widgets
    WidgetPool(){}
    ~WidgetPool(){
        realloc();
    }
    /**
     * @brief Initializes the widget pool with parent widget and layout.
     *
     * Pre-allocates initial widgets and adds them to the parent layout. Widgets are hidden until requested.
     *
     * @param in_parent Parent widget to attach managed widgets
     * @param in_layout Layout to contain managed widgets
     */
    void initialize(teTagListWidgetBase*in_parent,QBoxLayout*in_layout){
        parent_layout = in_layout;
        parent=in_parent;
        tagpool_initial_top-=total_tags_count/4;
        wordpool_initial_top-=total_words_count/4;
        for(tagpool_end=tagpool_initial_top;tagpool_end<total_tags_count;++tagpool_end){
            TagType*widget = new TagType;
            parent_layout->addWidget(widget);
            tags_free[tagpool_end] = widget;
            widget->hide();
        }
        --tagpool_end;
        for(wordpool_end=wordpool_initial_top;wordpool_end<total_words_count;++wordpool_end){
            WordType*widget = new WordType;
            parent_layout->addWidget(widget);
            words_free[wordpool_end] = widget;
            widget->hide();
        }
        --wordpool_end;
    }
    /**
     * @brief Extends widget initialization when initial pre-allocation is insufficient.
     *
     * Initializes additional widgets in batches (1/4 of total capacity each time) until capacity is reached.
     */
    void continue_initialize_widgets(){
        if(tagpool_end<tagpool_initial_top&&tagpool_initial_top>0){
            tagpool_initial_top-=total_tags_count/4;
            if(tagpool_initial_top<0)tagpool_initial_top=0;
            for(int i=tagpool_end;i>=tagpool_initial_top;--i){
                TagType*widget = new TagType;
                parent_layout->addWidget(widget);
                tags_free[i] = widget;
                widget->hide();
            }
        }
        if(wordpool_end<wordpool_initial_top&&wordpool_initial_top>0){
            wordpool_initial_top-=total_words_count/4;
            if(wordpool_initial_top<0)wordpool_initial_top=0;
            for(int i=wordpool_end;i>=wordpool_initial_top;--i){
                WordType*widget = new WordType;
                parent_layout->addWidget(widget);
                words_free[i] = widget;
                widget->hide();
            }
        }
    }
    static void give_back(teWordBase*in_word){
        in_word->core=nullptr;
        in_word->hide();
        QLayout* layout = in_word->parentWidget()->layout();
        if (layout)
            layout->removeWidget(in_word);
        in_word->disconnect();
        in_word->teDisconnect();
    }
    static void give_back(teTagBase*in_tag){
        in_tag->reset();
        QLayout* layout = in_tag->parentWidget()->layout();
        if (layout)
            layout->removeWidget(in_tag);
        in_tag->clearWordWidgets();
        in_tag->clearExtraWidgets();
        in_tag->disconnect();
        in_tag->teDisconnect();
        in_tag->hide();
    }
    void realloc(){
        tagpool_end=total_tags_count-1;
        wordpool_end=total_words_count-1;
        if(!extra_tags.empty()){
            for(QWidget*w:extra_tags){
                delete w;
            }
            extra_tags.clear();
        }
    }
    template<typename T> requires (std::is_same_v<T,TagType>||std::is_same_v<T,WordType>)
    T* try_to_get_widget();
    WordType* getWord(const QString& in);
    WordType* getWord(teWordCore* in);
    TagType* getTag(const QString& in);
    TagType* getTag(std::shared_ptr<tetagcore> in);
};

template<typename TAG,typename WORD,size_t C1,size_t C2>requires (std::derived_from<TAG,teTagBase>&&std::derived_from<WORD,teWordBase>)
template<typename T> requires (std::is_same_v<T,TAG>||std::is_same_v<T,WORD>)
T* WidgetPool<TAG,WORD,C1,C2>::try_to_get_widget(){
    static bool called=false;
    T*ret;
    if(!called){
        called=true;
        if((std::derived_from<T,teWordBase>&&wordpool_initial_top>0)||(std::derived_from<T,teTagBase>&&tagpool_initial_top>0)){
            continue_initialize_widgets();
            if constexpr(std::derived_from<T,teTagBase>)
                ret = (T*)tags_free[tagpool_end--];
            else if constexpr(std::derived_from<T,teWordBase>)
                ret = (T*)words_free[wordpool_end--];
        }else{
            // if(teTagListWidget*taglistwidget = dynamic_cast<teTagListWidget*>(parent);taglistwidget){
            //     if(taglistwidget->showing_list&&taglistwidget->showing_list->isWidgetLoaded){
            //         teTagList*tmp_showinglist = taglistwidget->showing_list;
            //         parent->clear();
            //         parent->load(tmp_showinglist);
            //     }
            // }
        }
        called=false;
    }
    if((std::derived_from<T,teWordBase>&&wordpool_end<wordpool_initial_top)||(std::derived_from<T,teTagBase>&&tagpool_end<tagpool_initial_top)){
        ret = new T(parent);
        extra_tags.push_back(ret);
    }else{
        if constexpr(std::derived_from<T,teTagBase>)
            ret = (T*)tags_free[tagpool_end--];
        else if constexpr(std::derived_from<T,teWordBase>)
            ret = (T*)words_free[wordpool_end--];
        else telog("[WidgetPool::try_to_get_widget]:unexpected type");
    }
    return (T*)ret;
}

template<typename TAG,typename WORD,size_t C1,size_t C2>requires (std::derived_from<TAG,teTagBase>&&std::derived_from<WORD,teWordBase>)
WORD *WidgetPool<TAG,WORD,C1,C2>::getWord(const QString &in){
    WORD*ret;
    if(wordpool_end<wordpool_initial_top)
        ret = try_to_get_widget<WORD>();
    else
        ret = words_free[wordpool_end--];
    ret->setText(in);
    ret->show();
    return ret;
}
template<typename TAG,typename WORD,size_t C1,size_t C2>requires (std::derived_from<TAG,teTagBase>&&std::derived_from<WORD,teWordBase>)
WORD *WidgetPool<TAG,WORD,C1,C2>::getWord(teWordCore *in){
    WORD*ret;
    if(wordpool_end<wordpool_initial_top)
        ret = try_to_get_widget<WORD>();
    else
        ret= words_free[wordpool_end--];
    ret->readCore(in);
    ret->show();
    return ret;
}

template<typename TAG,typename WORD,size_t C1,size_t C2>requires (std::derived_from<TAG,teTagBase>&&std::derived_from<WORD,teWordBase>)
TAG *WidgetPool<TAG,WORD,C1,C2>::getTag(const QString &in){
    TAG*ret;

    if(tagpool_end<tagpool_initial_top)
        ret = try_to_get_widget<TAG>();
    else
        ret = tags_free[tagpool_end--];
    ret->setText(in);
    ret->show();
    ret->clearExtraWidgets();
    return ret;
}

template<typename TAG,typename WORD,size_t C1,size_t C2>requires (std::derived_from<TAG,teTagBase>&&std::derived_from<WORD,teWordBase>)
TAG *WidgetPool<TAG,WORD,C1,C2>::getTag(std::shared_ptr<tetagcore>in){
    TAG*ret;
    if(tagpool_end<tagpool_initial_top)
        ret = try_to_get_widget<TAG>();
    else
        ret = tags_free[tagpool_end--];
    ret->readCore(in);
    ret->show();
    ret->clearExtraWidgets();
    return ret;
}

extern WidgetPool<tetag,teword,512,1024> widgetpool;
extern WidgetPool<tereftag,terefword,128,256> widgetpool_ref;

#endif // WIDGET_POOL_H
