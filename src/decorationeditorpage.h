#ifndef DECORATIONEDITORPAGE_H
#define DECORATIONEDITORPAGE_H

#include "teeditor.h"

class DecorationControl;

/**
 * @brief The decoration page: one invisible control, no visible tag widgets.
 *
 * The control owns `decorations.json` and writes the resolved visual onto every
 * tag the rules can decorate. The page itself only exists so that the rules have
 * an owner in the editor layout (MARK-07); it deliberately shows no controls and
 * its content area is collapsed.
 *
 * Its title is a button, like the custom editor's: clicking it opens the tag
 * decorations dialog. That dialog is the only place a tag's look is edited, so
 * the page that owns the rules is the natural place to open it from.
 */
class DecorationEditorPage : public teEditor_standard
{
public:
    DecorationEditorPage(TagListView* list,const QString& name);

private:
    DecorationControl* m_control=nullptr;
};

#endif // DECORATIONEDITORPAGE_H
