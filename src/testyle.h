#ifndef TESTYLE_H
#define TESTYLE_H

/*
 * 样式表集中管理。
 *
 * 这个程序过去把 QSS 分散在十几处:R"()" 字面量塞在构造函数里、塞在信号控件里、
 * 甚至单独一个 stylesheet.cpp 只为了放一个字符串(还被叫成 "liststyle")。结果是
 * 想改一处配色要 grep 半个工程,而且有些样式写死了只为某一个控件服务,换到别处
 * 就不对。
 *
 * 现在样式表放 `res/style/*.qss`(编进 qrc),用名字取:
 *
 *   widget->setStyleSheet(teStyle::sheet("list"));
 *   widget->setStyleSheet(teStyle::list(11));      // 需要填参数的少数几份
 *
 * 读取只发生一次并缓存(QSS 解析才是贵的那一步,这个是纯 I/O)。
 * 名字就是文件名去掉 .qss:res/style/list.qss -> teStyle::sheet("list")。
 *
 * 约定:能写进 .ui 的样式写进 .ui(designer 里能直接看见效果),
 * 只有"运行时才知道参数"或"被许多控件共用"的才放这里。
 */
#include <QString>

namespace teStyle {

/// The stylesheet stored in :/style/<name>.qss. Empty when there is no such file.
const QString& sheet(const QString& name);

/**
 * @brief controls.qss + secondarybutton.qss.
 *
 * The secondary button's own colours are a separate file because the add widget
 * (a window with its own legacy palette) hosts the same buttons but never gets
 * controls.qss. Everything that used to apply controls.qss and can contain a
 * SecondaryButton asks for this instead.
 */
QString controls();

/**
 * @brief The tag list sheet (list.qss), with the word font size filled in.
 *
 * It is a template because the font size follows the tag list's own text size,
 * which the caller knows and the sheet cannot.
 */
QString list(int wordFontSizePt);

/**
 * @brief 滚动条那一份(箭头 + 滑块 + 两端按钮)。
 *
 * 主窗口的 TagListView 是 QListView,它原本什么样式都没拿,所以滚动条用的是
 * Qt 默认箭头——深色主题下几乎看不见。这一份只动滚动条,不碰列表边框和字号。
 * list() 把它拼在 list.qss 前面,ref list / 编辑器页拿到的还是原来那一套。
 */
QString scrollbar();

} // namespace teStyle

#endif // TESTYLE_H
