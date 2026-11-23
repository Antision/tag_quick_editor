/*
 * tefacecontrols.h - controls moved out of teeditor_derive.cpp, where they used
 * to be declared inside the editor constructors.
 */
#pragma once
#include "teeditorhelpers.h"

struct mouth_buttongroup : public teTagButtonGroup {
    mouth_buttongroup(QWidget* parent = nullptr, QString* styleSheet_button = nullptr, QString* styleSheet_label = nullptr)
    :teTagButtonGroup({ {qsl("open"),qsl("open"),true},{qsl("closed"),qsl("closed"),true},{qsl("mouth"),qsl("mouth"),false} })
    {
        layout->setSpacing(1);
        // "open" and "closed" exclude each other; the label is not a button.
        rules.exclusive = {{0,1}};
    }
    /**
     * @brief 多人放宽:girl+boy+other > 1(或 multiple peoples 激活)时,
     *        open 和 closed 互不排斥,各自写一条 "open mouth" /
     *        "closed mouth"。单人时保持互斥(rules.exclusive)。
     */
    bool multiTagMode() const override {
        return isMultiPeople(taglistwidget?taglistwidget->showing_list:nullptr);
    }
};
