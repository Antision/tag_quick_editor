#ifndef TEIMAGEWIDGET_H
#define TEIMAGEWIDGET_H

#include "imageview.h"
#include "tesignalwidget.h"     // applyLiteWindowStyle()

/**
 * @brief tqe's preview window: the shared ImageView with tqe's window look.
 *
 * The canvas and the window live in common/imageview now; only the stylesheet is
 * program-specific, so this class exists just to apply it.
 */
class teImageWidget : public ImageView {
    Q_OBJECT
public:
    explicit teImageWidget(QWidget *parent = nullptr)
        : ImageView(parent)
    {
        applyLiteWindowStyle(this);
    }
};

#endif // TEIMAGEWIDGET_H
