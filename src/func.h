#ifndef FUNC_H
#define FUNC_H
class QBoxLayout;
class QWidget;
inline const int MainWindowWidgetCount=3;
extern int mainWindowSplitterLength[MainWindowWidgetCount];
int findWidgetIndexInLayout(QBoxLayout* layout, QWidget* widget);
extern QRect mainwindowGeometry;

/**
 * @brief Global font family: the embedded Ubuntu fonts (Light/Regular/Bold).
 *
 * Registered on first call (:/res/fonts/Ubuntu-*.ttf, Ubuntu Font Licence,
 * license text embedded at :/UFL.txt, source copy in LICENSES/UFL.txt). Returns an empty QString on failure
 * (Qt then falls back to the platform default). Must be called after the
 * QApplication instance exists. The Light weight is applied by main.cpp
 * when setting the application font.
 */
const QString& appFontFamily();

/**
 * @brief setStyleSheet() only when the sheet really changes.
 *
 * Qt re-parses a stylesheet on every call, which costs on the order of a
 * millisecond for these small editor widgets. The editors set the same sheet for
 * the same state over and over - per control, per tag and per image switch - and
 * that is what made switching images slow.
 */
void setStyleSheetOnce(QWidget* widget,const QString& sheet);

/**
 * @brief Sets a widget's "linked / not linked" text colour.
 *
 * A setStyleSheet() on a QComboBox costs about 6 ms (Qt re-parses the sheet and
 * re-polishes the combo together with its popup view). The palette would be free,
 * but it is invisible for these widgets: the editors' page sheet sets
 * `*{color:white}` for everything inside it, and a sheet colour always wins over
 * a palette one - which is exactly why a captured tag stopped turning green.
 * The colour therefore goes into the widget's own sheet (which does win), and
 * only when it actually changes.
 */
void setStateTextColor(QWidget* widget,const QColor& color);

/**
 * @brief A small blue dash: the delimiter between two tag *groups* on one line.
 *
 * Inside a group the buttons are alternatives of one thing and must stay packed
 * (`open | closed | mouth`); two groups side by side are two meanings and need the
 * dash, or the whole line reads as one long strip. A plain label with its own
 * colour, because the editor pages and the LiteWindow popups do not share one
 * stylesheet.
 */
QWidget* makeGroupSeparator(QWidget* parent = nullptr);

/**
 * @brief The same delimiter, drawn as a short vertical line between two columns.
 *
 * The colour dialogs put several tag groups side by side; between columns a dash
 * would sit at an odd angle, so the delimiter turns with the layout: a 1px blue
 * line, vertically centred, in a 5px slot.
 */
QWidget* makeColumnSeparator(QWidget* parent = nullptr);

/**
 * @brief The delimiter for two stacked sections: a 1px blue line across the row.
 */
QWidget* makeRowSeparator(QWidget* parent = nullptr);

int load_config();

int save_config();
class teTag;
std::string joinTag(const teTag& tag);

class EditorControl;
/// The custom-tag controls of the "custom" editor, in *insertion* order.
/// A QMap would sort them alphabetically and lose the user's arrangement.
using teCustomControlList = QVector<QPair<QString,EditorControl*>>;
/// Points at the custom editor's control list; owned by teEditor_custom.
extern teCustomControlList* custom_controls;

bool isOpenBracket(const std::string &s);
bool isCloseBracket(const std::string &s);

struct editorListLayout{
    QVector<QScrollArea*> pages;
    QVector<QStringList> editors;
};
extern editorListLayout editorlistlayout;

template<typename Container,typename T> requires requires(Container c){(*c.begin())->lock();}
int sharedIndexInWeakContainer(const Container& container, const std::shared_ptr<T>& target) {
    for (int i = 0; i < container.size(); ++i) {
        const std::weak_ptr<T>& wp = container[i];
        if (!wp.owner_before(target) && !target.owner_before(wp)) {
            return i;
        }
    }
    return -1;
}
class MainWindow;

// 日志(target / 宏)已经随 tag 层一起搬进 common/log,两程序共用。
#include "log.h"

/// Convenience wrappers kept so existing call sites stay readable.
#define qsl(x) QStringLiteral(x)
double getWindowScale(HWND hwnd);

#endif // FUNC_H
