#include "decorationcontrol.h"
#include "tagmarks.h"

bool DecorationControl::filter(std::shared_ptr<teTag> tag) const
{
    if(!tag||tag->retired)
        return false;
    // Whether another control claimed the tag does not matter: every editor sees
    // every tag (that is the whole design), and the decoration is a function of
    // the text. Asking the other controls made the result depend on which of them
    // ran first.
    return !TagMarkRegistry::instance().decorate(*tag).isEmpty();
}

TagHandling DecorationControl::read(std::shared_ptr<teTag> tag)
{
    if(!filter(tag))
        return TagHandling::NotMine;
    applyDecoration(tag);
    link(tag);                  // subscribe to later edits; the page has no widget
    return TagHandling::Linked;
}

TagHandling DecorationControl::re_read(std::shared_ptr<teTag> tag)
{
    if(!filter(tag)){
        if(tag){
            clearDecoration(tag);
            unlink(tag);
        }
        return TagHandling::NotMine;
    }
    applyDecoration(tag);
    return TagHandling::Linked;
}

void DecorationControl::reset()
{
    clear();
}

void DecorationControl::clear()
{
    for(const std::shared_ptr<teTag>& tag:linked_tags)
        clearDecoration(tag);
    clearLinks();
}

void DecorationControl::applyDecoration(const std::shared_ptr<teTag>& tag) const
{
    if(!tag)
        return;
    tag->decoration=TagMarkRegistry::instance().decorate(*tag);
    tag->decorationSet=!tag->decoration.isEmpty();
}

void DecorationControl::clearDecoration(const std::shared_ptr<teTag>& tag) const
{
    if(!tag)
        return;
    tag->decoration=TagVisual();
    tag->decorationSet=false;
}
