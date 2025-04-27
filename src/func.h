#ifndef FUNC_H
#define FUNC_H
class QBoxLayout;
class QWidget;
inline const int MainWindowWidgetCount=3;
extern int mainWindowSplitterLength[MainWindowWidgetCount];
int findWidgetIndexInLayout(QBoxLayout* layout, QWidget* widget);
extern std::atomic<int> loading_count;
extern QRect mainwindowGeometry;

int load_config();

int save_config();
class teTagCore;
std::string joinTag(const teTagCore& tag);

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
void CreateAutoSaveThread(MainWindow*w);
#define telog(a) qDebug()<<a
#define qsl(x) QStringLiteral(x)

#endif // FUNC_H
