/*
 * teeditorhelpers.h - editor wide state and small helpers that the control
 * classes share. They used to live at the top of teeditor_derive.cpp, which made
 * every control that moved to its own header depend on that file.
 */
#pragma once
#include "teeditorcontrol.h"
#include "tetaglistwidget.h"

extern bool autoMerge;
extern bool MergeSwitch;
extern QStringList custom_tags;
extern teCustomControlList* custom_controls;
extern QSet<QString> all_colors;

bool is_color(const QString& color_word);
QStringList extract_colors(std::shared_ptr<teTag> tag);
QPair<int,int> is_color(teTag& tag,int offset=0);

/// Prepositions that turn a clothes tag into an action phrase.
inline const QSet<QString> preposwords{
    qsl("in"),qsl("on"),qsl("under"),qsl("from"),qsl("through"),qsl("into"),
    qsl("onto"),qsl("inside"),qsl("beneath"),qsl("underneath"),qsl("at"),
    qsl("beside"),qsl("between"),qsl("across"),qsl("around"),qsl("against"),
    qsl("past"),qsl("toward"),qsl("towards")
};

/// Object list vocabulary (used by its filter()).
    inline QSet<QString> object1 {qsl("cum"),qsl("erection"),qsl("tentacle"),qsl("egg"),qsl("slime"),qsl("worm"),qsl("insect")};
    inline QSet<QString> bodyparts{qsl("pussy"),qsl("ass"),qsl("body"),qsl("face"),qsl("mouth"),qsl("breasts"),qsl("uterus"),qsl("clothes"),qsl("panties"),qsl("penis"),qsl("nipples"),qsl("urethra")};
