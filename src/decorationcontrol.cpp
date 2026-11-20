#include "decorationcontrol.h"
#include "teeditorlist.h"
#include "tagmarks.h"

bool DecorationControl::handledElsewhere(const std::shared_ptr<teTag>& tag) const
{
    if(!editorList||!tag)
        return false;
    for(teEditor* editor:editorList->editorlist){
        if(!editor)
            continue;
        for(EditorControl* control:editor->controls){
            if(!control||control==this)
                continue;
            if(control->linked_tags.find(tag)!=control->linked_tags.end())
                return true;
        }
    }
    return false;
}

bool DecorationControl::filter(std::shared_ptr<teTag> tag) const
{
    if(!tag||tag->retired)
        return false;
    if(handledElsewhere(tag))
        return false;
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
