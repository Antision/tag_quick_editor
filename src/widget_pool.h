#ifndef WIDGET_POOL_H
#define WIDGET_POOL_H
#include "tetag.h"
#include "tereftaglistwidget.h"
#include"tetaglistwidget.h"

struct teTagListWidget;

/**
 * @brief Recycles the tag/word widgets used by the tag lists and the editors.
 *
 * Widgets are created on first use and pushed back onto a free list by
 * give_back(), so switching files reuses the existing widgets instead of
 * allocating a fresh set every time.
 *
 * Two things changed compared to the previous implementation, both aimed at the
 * rendering cost that made the tag list feel slow:
 *
 *  - **Nothing is pre-allocated.** The old code built ~1150 hidden widgets at
 *    start-up and parked them in the container layout, so every single layout
 *    pass had to walk a list of more than a thousand items.
 *  - **give_back() actually recycles.** It used to only detach the widget, so
 *    the pool was drained once and every later request allocated (and every
 *    realloc() deleted) a widget.
 *
 * Ownership
 * ---------
 * The pool does not own the widgets: they are children of the widget the pool
 * was initialized with (Qt owns them, and they are destroyed with the window).
 * The pool only keeps raw pointers to the *free* ones, and deletes those in
 * realloc(). The destructor deliberately does nothing, so shutdown order can
 * never turn the bookkeeping into a double delete.
 *
 * @tparam TagType Type of tag widget (must derive from teTagBase)
 * @tparam WordType Type of word widget (must derive from teWordBase)
 * @tparam total_tags_count/total_words_count Kept for the historical signature;
 *         the pool now grows on demand instead of pre-allocating.
 */
template<typename TagType,typename WordType,size_t total_tags_count,size_t total_words_count>
requires (std::derived_from<TagType,teTagBase>&&std::derived_from<WordType,teWordBase>)
struct WidgetPool:public teObject{
    teTagListWidgetBase* parent=nullptr;///< Initialization widget; also the fallback parking parent
    QBoxLayout* parent_layout=nullptr;///< Layout the widgets will be shown in

    WidgetPool(){}
    /// Does nothing on purpose: the widgets belong to the widget tree.
    ~WidgetPool(){}

    /// Number of widgets currently waiting to be reused (for diagnostics).
    int freeTagCount(){
        purgeDead();
        return int(tags_free.size());
    }
    int freeWordCount(){
        purgeDead();
        return int(words_free.size());
    }

    /**
     * @brief Binds the pool to the widget its widgets belong to.
     *
     * No widget is created here; the pool grows on demand.
     */
    void initialize(teTagListWidgetBase*in_parent,QBoxLayout*in_layout){
        parent = in_parent;
        parent_layout = in_layout;
    }

    /// Widget recycled widgets are parked under until they are shown again.
    QWidget* parkingParent() const {
        if(!parking.isNull())
            return parking;
        if(parent_layout&&parent_layout->parentWidget())
            parking = parent_layout->parentWidget();
        else
            parking = parent;
        return parking;
    }

    TagType* getTag(std::shared_ptr<tetagcore> in){
        TagType* ret = takeTag();
        ret->readCore(std::move(in));
        ret->show();
        ret->clearExtraWidgets();
        return ret;
    }
    WordType* getWord(teWordCore* in){
        WordType* ret = takeWord();
        ret->readCore(in);
        ret->show();
        return ret;
    }

    /// Returns a word widget to the pool. Signalling the same widget twice is
    /// harmless (the second call is ignored).
    void give_back(teWordBase*in_word){
        if(!in_word)
            return;
        in_word->hide();
        if(QWidget* owner = in_word->parentWidget()){
            if(QLayout* layout = owner->layout())
                layout->removeWidget(in_word);
            // Drop exactly the connections teTagBase::connectWord() made
            // (word -> owning tag). A wildcard disconnect would also cut Qt's
            // internal ones, and prints a warning for every single word.
            QObject::disconnect(in_word,nullptr,owner,nullptr);
        }
        in_word->teDisconnect();
        // Never leave the core pointing at a widget that is about to be reused.
        if(in_word->core && static_cast<teWordBase*>(in_word->core->widget) == in_word)
            in_word->core->widget = nullptr;
        if(auto* typed = dynamic_cast<WordType*>(in_word)){
            if(typed->inWidgetPool)
                return;                 // already parked: a second give_back is a no-op
            // Detach from the core. The widget stays where it is (hidden under
            // its old owner): reparenting every widget on every switch cost far
            // more than it saved, and the inWidgetPool flag already stops the
            // old owner from handing it back twice.
            in_word->core = nullptr;
            purgeDead();
            if(words_free.size() >= total_words_count){
                delete typed;           // the pool is full, do not keep it around
                return;
            }
            typed->inWidgetPool = true;
            words_free.push_back(typed);
        }
    }

    /// Returns a tag widget (and the words it owns) to the pool.
    void give_back(teTagBase*in_tag){
        if(!in_tag)
            return;
        in_tag->hide();
        if(QWidget* owner = in_tag->parentWidget()){
            if(QLayout* layout = owner->layout())
                layout->removeWidget(in_tag);
        }
        // Drop the connections teTagListWidgetBase::connectTag() made
        // (tag -> tag list widget) without the warning that a wildcard
        // disconnect prints for every tag.
        for(QObject* ancestor = in_tag->parent(); ancestor; ancestor = ancestor->parent()){
            if(qobject_cast<teTagListWidgetBase*>(ancestor)){
                QObject::disconnect(in_tag,nullptr,ancestor,nullptr);
                break;
            }
        }
        in_tag->clearWordWidgets();     // dispatches to the matching pool
        in_tag->clearExtraWidgets();
        in_tag->teDisconnect();
        // Never leave the core pointing at a widget that is about to be reused.
        if(in_tag->core && static_cast<teTagBase*>(in_tag->core->widget) == in_tag)
            in_tag->core->widget = nullptr;
        in_tag->reset();                // the tag no longer owns a core
        if(auto* typed = dynamic_cast<TagType*>(in_tag)){
            if(typed->inWidgetPool)
                return;                 // already parked: a second give_back is a no-op
            purgeDead();
            if(tags_free.size() >= total_tags_count){
                delete typed;           // the pool is full, do not keep it around
                return;
            }
            typed->inWidgetPool = true;
            tags_free.push_back(typed);
        }
    }

    /**
     * @brief Destroys every widget the pool currently holds.
     *
     * Only call this when nothing is handed out any more (the tag list calls it
     * from clear(), after it gave all of its widgets back). Widgets that are
     * still in use are not in the free list and stay untouched.
     */
    void realloc(){
        for(const QPointer<TagType>& tag:tags_free)
            if(tag)
                delete tag.data();
        tags_free.clear();
        for(const QPointer<WordType>& word:words_free)
            if(word)
                delete word.data();
        words_free.clear();
    }

private:
    /// Drops entries whose widget was destroyed by Qt (it can happen if the
    /// widget a recycled widget was parked under goes away). QPointer makes the
    /// pool notice instead of handing out a dangling pointer.
    void purgeDead(){
        tags_free.erase(std::remove_if(tags_free.begin(),tags_free.end(),
                                       [](const QPointer<TagType>& tag){ return tag.isNull(); }),
                        tags_free.end());
        words_free.erase(std::remove_if(words_free.begin(),words_free.end(),
                                        [](const QPointer<WordType>& word){ return word.isNull(); }),
                         words_free.end());
    }

    /// Creates a widget under the parking parent. It must have a parent before
    /// it is shown, otherwise show() would turn it into a top level window.
    template<typename T>
    T* makeWidget(){
        if(QWidget* parkingWidget = parkingParent())
            return new T(parkingWidget);
        return new T;
    }

    TagType* takeTag(){
        while(!tags_free.empty()){
            QPointer<TagType> candidate = tags_free.back();
            tags_free.pop_back();
            if(candidate){
                candidate->inWidgetPool = false;
                return candidate.data();
            }
        }
        // The caller puts it into a layout, which reparents it next to the
        // other tags.
        return makeWidget<TagType>();
    }
    WordType* takeWord(){
        while(!words_free.empty()){
            QPointer<WordType> candidate = words_free.back();
            words_free.pop_back();
            if(candidate){
                candidate->inWidgetPool = false;
                return candidate.data();
            }
        }
        return makeWidget<WordType>();
    }

    std::vector<QPointer<TagType>> tags_free;///< Recycled tag widgets, ready to be handed out
    std::vector<QPointer<WordType>> words_free;///< Recycled word widgets
    mutable QPointer<QWidget> parking;///< Widget recycled widgets wait under
};

extern WidgetPool<tetag,teword,512,1024> widgetpool;
extern WidgetPool<tereftag,terefword,128,256> widgetpool_ref;

#endif // WIDGET_POOL_H
