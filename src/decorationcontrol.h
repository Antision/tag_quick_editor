#ifndef DECORATIONCONTROL_H
#define DECORATIONCONTROL_H

#include "teeditorcontrol.h"

/**
 * @brief The invisible control of the decoration page.
 *
 * It owns the `decorations.json` rules: for every tag the rules can decorate it
 * resolves the visual and stores it on the tag (teTag::decoration). The delegate
 * and the reftaglist QSS only read that stored value, so removing this
 * page/control removes the decoration - there is no paint-time fallback.
 *
 * Every editor sees every tag: a tag another control has already claimed is
 * decorated here as well. The decoration is a display value computed from the
 * text, not a thing one control "owns", so the old "only the tags nobody else
 * wanted" rule made the result depend on which control happened to run first.
 *
 * It has no widget. `teEditor_standard::addRowTo()` skips non-widget controls,
 * which is exactly what MARK-07 asks for: the page exists in the layout but
 * shows nothing.
 */
class DecorationControl : public EditorControl
{
public:
    bool filter(std::shared_ptr<teTag> tag) const override;
    TagHandling read(std::shared_ptr<teTag> tag) override;
    TagHandling re_read(std::shared_ptr<teTag> tag) override;
    void reset() override;
    void clear() override;

private:
    void applyDecoration(const std::shared_ptr<teTag>& tag) const;
    void clearDecoration(const std::shared_ptr<teTag>& tag) const;
};

#endif // DECORATIONCONTROL_H
