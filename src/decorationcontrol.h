#ifndef DECORATIONCONTROL_H
#define DECORATIONCONTROL_H

#include "teeditorcontrol.h"

class teEditorList;

/**
 * @brief The invisible control of the decoration page.
 *
 * It owns the `decorations.json` rules: for every tag no other editor control
 * claimed, it resolves the visual and stores it on the tag (teTag::decoration).
 * The delegate and the reftaglist QSS only read that stored value, so removing
 * this page/control removes the decoration - there is no paint-time fallback.
 *
 * It has no widget. `teEditor_standard::addRowTo()` skips non-widget controls,
 * which is exactly what MARK-07 asks for: the page exists in the layout but
 * shows nothing.
 */
class DecorationControl : public EditorControl
{
public:
    /// The list is needed to ask the other controls whether they claimed a tag.
    /// Set by DecorationEditorPage once teEditorList has loaded its editors.
    teEditorList* editorList=nullptr;

    bool filter(std::shared_ptr<teTag> tag) const override;
    TagHandling read(std::shared_ptr<teTag> tag) override;
    TagHandling re_read(std::shared_ptr<teTag> tag) override;
    void reset() override;
    void clear() override;

private:
    bool handledElsewhere(const std::shared_ptr<teTag>& tag) const;
    void applyDecoration(const std::shared_ptr<teTag>& tag) const;
    void clearDecoration(const std::shared_ptr<teTag>& tag) const;
};

#endif // DECORATIONCONTROL_H
