#ifndef DECORATIONEDITORPAGE_H
#define DECORATIONEDITORPAGE_H

#include "teeditor.h"

class DecorationControl;
class teEditorList;

/**
 * @brief The decoration page: one invisible control, no visible tag widgets.
 *
 * The control owns `decorations.json` and writes the resolved visual onto the
 * tags no other editor control claimed. The page itself only exists so that the
 * rules have an owner in the editor layout (MARK-07); it deliberately shows no
 * controls and its content area is collapsed.
 */
class DecorationEditorPage : public teEditor_standard
{
public:
    DecorationEditorPage(TagListView* list,const QString& name);
    /// Called once teEditorList has loaded the editors: the control needs to
    /// know which tags the other controls claimed.
    void setEditorList(teEditorList* list);

private:
    DecorationControl* m_control=nullptr;
};

#endif // DECORATIONEDITORPAGE_H
