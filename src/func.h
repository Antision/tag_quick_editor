#ifndef FUNC_H
#define FUNC_H
class QBoxLayout;
class QWidget;
inline const int MainWindowWidgetCount=3;
extern int mainWindowSplitterLength[MainWindowWidgetCount];
int findWidgetIndexInLayout(QBoxLayout* layout, QWidget* widget);
extern QRect mainwindowGeometry;

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
 * @brief Sets a widget's "linked / not linked" text colour without a stylesheet.
 *
 * A setStyleSheet() on a QComboBox costs about 7 ms (Qt re-parses the sheet and
 * re-polishes the combo together with its popup view), and the editors' combo
 * boxes toggle that colour on every image switch - eleven of them made up almost
 * the whole 100 ms of the per-image editor rebuild. The palette is free.
 */
void setStateTextColor(QWidget* widget,const QColor& color);

int load_config();

int save_config();
class teTag;
std::string joinTag(const teTag& tag);

class teEditorControl;
/// The custom-tag controls of the "custom" editor, in *insertion* order.
/// A QMap would sort them alphabetically and lose the user's arrangement.
using teCustomControlList = QVector<QPair<QString,teEditorControl*>>;
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

/// Registers the receiver of application log lines. The target is always
/// invoked on the GUI thread, so it may touch widgets. Pass an empty
/// std::function to detach it (e.g. when the log window is destroyed).
void teSetLogTarget(std::function<void(const QString&)> target);
/// Appends a line to the application log. Safe to call from any thread.
void teLog(const QString& message);

/// Convenience wrappers kept so existing call sites stay readable.
#define telog(a) teLog(QString(a))
#define qsl(x) QStringLiteral(x)
double getWindowScale(HWND hwnd);


struct ParsedPiece {
    QString text;
    bool sentence = false;
};

#endif // FUNC_H
