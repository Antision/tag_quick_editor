#ifndef TAGVIEWHOST_H
#define TAGVIEWHOST_H

#include "tagviewcontracts.h"

#include <QObject>

/**
 * @brief tqe's TagViewHost: the magnified popup and the sentence window.
 *
 * Both are tqe-specific (the popup drags a tag widget, the sentence window is a
 * LiteWidget2), so they stay here while the view itself is shared. The view asks
 * for them through TagViewHost and never names either class.
 */
class TqeTagViewHost : public QObject, public TagViewHost
{
    Q_OBJECT
public:
    explicit TqeTagViewHost(QObject* parent=nullptr);
    TagDisplay* createTagDisplay(TagDisplayOwner* owner) override;
    void editLongText(TagListView* view,std::shared_ptr<teTag> core) override;
};

#endif // TAGVIEWHOST_H
