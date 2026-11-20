#ifndef TESIGNALWIDGET_H
#define TESIGNALWIDGET_H
#include "tepicturelistview.h"
#include "litewidget2.h"
#include "litewindow.h"
#include "pch.h"

extern QString destroyButtonStyle, OkButtonStyle, addButtonStyle;

/// 给 LiteWidget2 的内容面板套上原来的窗口配色(深色底、白字、滚动条)。
/// 面板会被窗口裁成圆角,所以不再画 `QWidget#content` 那圈紫色边框。
void applyLiteWindowStyle(LiteWidget2* window);
/// 同上,给 LiteWindow(整宽标题栏那种普通窗体)。
void applyLiteWindowStyle(LiteWindow* window);

class teInputWidget : public LiteWidget2
{
    Q_OBJECT
public:
    QVBoxLayout* inputLayout = nullptr;
    QPlainTextEdit* plainTextEdit = nullptr;
    teInputWidget(QWidget* parent = nullptr);
    void start(const QString& in_str) { plainTextEdit->setPlainText(in_str); }
    void onOkClicked();
signals:
    void stringSignal(QString data, bool ifadd);
    void destroySignal();
    void cancelSignal();
};

void addButtonsToGridLayout(QGridLayout *gridLayout, QButtonGroup *buttonGroup, QStringList &stringList);

class colorsWidget : public LiteWidget2 {
    Q_OBJECT
public:
    QVBoxLayout* layout = nullptr;
    QHBoxLayout* signal_button_layout = new QHBoxLayout;
    /// Sections are layed out horizontally by default; pass
    /// `verticalSections = true` to stack them instead, or `sectionColumns` to
    /// group them into a few columns (each section stacked in its own column).
    QBoxLayout* content_layout = nullptr;
    /// One entry per column when `sectionColumns` was used.
    QVector<QVBoxLayout*> columns;
    QVBoxLayout shade_layout;
    QButtonGroup* shade_buttongroup = new QButtonGroup(this);

    QGridLayout colors_layout;
    QButtonGroup* colors_buttongroup = new QButtonGroup(this);
    QHBoxLayout* extra_layout=nullptr;
    QButtonGroup* extra_buttongroup =nullptr;

    QVector<std::pair<QLayout*,QButtonGroup*>>objectLayoutList;
    /// The group every exclusive section shares, when the constructor got one.
    /// It is what tells sendString() where the "type" sections start, i.e. where
    /// the words that no button stands for belong (see sendString).
    QButtonGroup* shared_group = nullptr;
    QString otherWords;
    void uncheckAllButtons();
    /// Adds one section (with an optional caption above it) to content_layout
    /// (or to `column` when the widget was built with section columns).
    void addSectionToContent(QLayout* sectionLayout,const QString& heading,int column=-1);
    /**
     * @param objects one entry per button section. A single-element list with
     *        `ifExclusive == true` renders as a fixed word (a label that becomes
     *        part of every composed tag) instead of a button.
     * @param colorListPos where to insert the shade/colour rows (-1 = no colour row).
     * @param extraButtons an extra row of exclusive buttons.
     * @param headings optional caption drawn *above* the matching section. It is
     *        purely visual and never becomes part of a tag.
     * @param sharedExclusiveGroup when given, every section with
     *        `ifExclusive == true` puts its buttons into this group, so only one
     *        button of the whole widget can be checked at a time.
     * @param verticalSections stack the sections instead of placing them side
     *        by side.
     * @param sectionColumns one entry per section: which column it belongs to
     *        (empty = one column per section). Used by the clothes dialog to put
     *        the colours and features in the left column and the clothes types in
     *        the right one, which keeps the window square.
     */
    colorsWidget(QVector<QPair<QStringList,bool>>&&objects,QWidget*parent=nullptr,int colorListPos=0,
                 std::optional<QStringList>extraButtons={},
                 QVector<QString> headings={},
                 QButtonGroup* sharedExclusiveGroup=nullptr,
                 bool verticalSections=false,
                 QVector<int> sectionColumns={});


    /// Moves itself next to the cursor, clamped to the screen, and shows.
    void showNearCursor();

    QPushButton* getColorButton(const QString&colorText,int r,int g,int b,bool ifblacktext=false);
    void sendString(bool ifadd);
    virtual void input_and_show(std::shared_ptr<teTag> in_tag=nullptr);
signals:
    void stringSignal(QString data, bool ifadd);
    void destroySignal();
    void cancelSignal();
};

class teFilteredPictureModel : public QAbstractListModel,public teObject {
    Q_OBJECT
public:
    QList<QModelIndex> picturefiles;
    tePictureListView*listview;
    teFilteredPictureModel(tePictureListView*parentListView,QObject *parent = nullptr) : QAbstractListModel(parent),listview(parentListView) {
        connect(dynamic_cast<tePictureFileModel*>(listview->model()),&tePictureFileModel::clearAllFiles,this,&teFilteredPictureModel::clear,Qt::DirectConnection);
    }

    void clear();
    void setdata(QList<QModelIndex>&&indexes);
    void append(const QList<QModelIndex> &files);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
signals:
    void newFileLoaded(QList<tePictureFile *> files,bool ifclear);
    void clearAllFiles();
};

/**
 * @brief A row of removable "tag chips" layed out by a QFlowLayout.
 *
 * Clicking a chip removes it and sends its text back through tagEdit(); the
 * user may also drag a chip to another position, which only reorders. A drag
 * never removes the chip: QFlowLayoutReorderer swallows the mouse release of a
 * drag, so QPushButton::clicked() is not emitted for it.
 */
class FilterTagWidget : public QWidget {
    Q_OBJECT
public:
    explicit FilterTagWidget(QWidget* parent = nullptr);
    QVector<QPushButton*> tagItems;
    void addTag(const QString& text);
    void clear(){
        for(QPushButton*btn:tagItems)
            delete btn;
        tagItems.clear();
    }
    void setLineEdit(QLineEdit*le){
        lineedit=le;
        if(le){
            m_layout->insertWidget(0,le);
            if(m_reorderer)
                m_reorderer->setFirstMovableIndex(1);
        }
    }
    /// The chip texts in *visual* order, i.e. including any drag reordering.
    QStringList tagTexts() const;
signals:
    void tagRemoved(QPushButton*);
    void tagEdit(QString);
    /// The user dragged a chip to another position.
    void orderChanged();
private:
    QFlowLayout* m_layout;
    QFlowLayoutReorderer* m_reorderer=nullptr;
    QLineEdit* lineedit = nullptr;
};
class filterWidget : public LiteWindow {
    Q_OBJECT
public:
    filterWidget(tePictureListView* in_listview) : listview(in_listview) {
        setWindowTitle(QStringLiteral("Filter"));
        initializeUI();
        connectSignals();
    }
    ~filterWidget(){
        // myview is a child widget (destroyed by ~QWidget, after the members),
        // while mymodel is a member and dies first. The view's selection model
        // still holds persistent indexes into it at that point; Qt asserts
        // "persistent model indexes corrupted" when they are released. Detach
        // the model while both are alive.
        myview->setModel(nullptr);
    }
    enum RuleType { AllOf, AnyOf, NoneOf, ExactOne };
    /// 筛选窗下面是图片列表,要的是"高窗口":四条规则 + 按钮 + 一屏缩略图。
    QSize sizeHint() const override { return QSize(430, 780); }
    QSize minimumSizeHint() const override { return QSize(360, 520); }
    tePictureListView* listview;
    QListView* myview = new QListView;
    teFilteredPictureModel mymodel{listview};
    tePictureFileDelegate pictureFileDelegate;
    FilterTagWidget* tagWidgets[4];
    QLineEdit* lineEdits[4];
    QStringListModel* completerModel;
    QPushButton* filterBtn;
private:
    void initializeUI();
    void setupCompleter(QLineEdit* edit);
    QStringList fetchSuggestions(const QString& input, int maxSuggestions = 10);

    QItemSelection lastSelected,lastDeselected;
    bool SelectionSent=true;
    void sendSelection();

    bool eventFilter(QObject *watched, QEvent *event);
    void connectSignals();

    void addTagToRule(RuleType type, const QString& tag);

    void performFilter();

};

class customControlWidget : public LiteWidget2 {
    Q_OBJECT
public:
    customControlWidget()  {
        initializeUI();
        connectSignals();
    }
    QHBoxLayout* customTagsLayout = new QHBoxLayout;
    FilterTagWidget* tagWidget = new FilterTagWidget;
    QLineEdit* lineEdit = new QLineEdit(this);
    QPushButton* OK_Btn = nullptr;
    QPushButton* Bin_Btn = nullptr;
    QHBoxLayout*buttonLayout = new QHBoxLayout;
private:
    void initializeUI();
    void connectSignals();
signals:
    void tagsUpdated(QStringList);
    void tagsClearAll();
};
#endif // TESIGNALWIDGET_H
