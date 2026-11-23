#ifndef TESIGNALWIDGET_H
#define TESIGNALWIDGET_H
#include "tepicturelistview.h"
#include "litewidget2.h"
#include "litewindow.h"
#include "pch.h"

extern QString destroyButtonStyle, OkButtonStyle, addButtonStyle;

/// 快速添加窗/自定义 tag 窗的三颗操作按钮:Lucide 图标着色 + 外发光
/// (对勾/加号 绿,x 红)。旧 bin/tick/plus PNG 已删除。
void applySignalButtonIcons(QPushButton* ok,QPushButton* add,QPushButton* destroy);

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
    /// The tag the dialog was opened for (empty for a brand-new tag). The clothes
    /// editor lights its type buttons from this, not from the tag list: the add
    /// widget knows nothing about what else is in the list.
    void editingTagChanged(const QString& tagText);
    void destroySignal();
    void cancelSignal();
};

/**
 * @brief Adds one checkable button per word, laid out in a grid.
 *
 * `captions` maps a word to the text the button shows when the two differ - the
 * clothes dialog prefixes an emoji. Only those buttons remember the word in a
 * "tag" property; every other button keeps writing its text, which some windows
 * change at runtime (the hair colours rename "yellow" to "blonde").
 */
void addButtonsToGridLayout(QGridLayout *gridLayout, QButtonGroup *buttonGroup,
                            QStringList &stringList,
                            const QHash<QString,QString>* captions = nullptr);

/// The word a button writes: its "tag" property when it has one, its text
/// otherwise.
QString buttonTagText(const QAbstractButton* button);

/// The caption drawn above a section (purely visual, never part of a tag).
QLabel* makeSectionHeading(const QString& text, QWidget* parent = nullptr);


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
    /// The type word(s) chosen through a SecondaryButton / its menu.
    QString m_composedType;
    void uncheckAllButtons();
    /// Clears every tick and announces "no tag": the dialog is composing a new one.
    void startNewTag();
    /**
     * @brief The words that turn the colour column into a colour *list*.
     *
     * `striped` and `gradient` describe a pattern, not one colour: with one of
     * them ticked the colour buttons stop being states and become an "append"
     * action, and the picked colours show up in a swatch row under them. The
     * swatch row is a QFlowLayout, so the colours can be dragged into another
     * order and a click on one removes it.
     */
    void setMultiColorTriggers(const QStringList& words) { m_multiColorTriggers = words; }
    /// The colours picked for the striped/gradient phrase, in visual order.
    QStringList colorQueue() const { return m_colorQueue; }
    void setColorQueue(const QStringList& colors);
    void clearColorQueue();
    /// "blue and white" / "white to blue" for the current trigger, or empty when
    /// no trigger is ticked (the tail `striped` / `gradient` comes from its own
    /// button, which stays checked).
    QString multiColorPhrase() const;
    /**
     * @brief The type spelling the palette is composing right now.
     *
     * The add widget builds one tag: colours, modifiers and *one* type. The type
     * can be a button (`skirt`) or an entry of its menu (`pleated skirt`), and it
     * is kept here instead of being written to the tag list on the spot. `+` is
     * what turns the whole composition into a tag.
     */
    QString composedType() const { return m_composedType; }
    /// Chooses (or with `toggle`, toggles away) that type. Always clears the plain
    /// type buttons: one garment has one type.
    void chooseComposedType(const QString& tagText, bool toggle = false);
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
     * @param buttonCaptions words whose button text is not the word itself
     *        (clothes buttons show "🎩 hat" and write "hat"). A word without an
     *        entry here keeps using its own text as the tag.
     */
    colorsWidget(QVector<QPair<QStringList,bool>>&&objects,QWidget*parent=nullptr,int colorListPos=0,
                 std::optional<QStringList>extraButtons={},
                 QVector<QString> headings={},
                 QButtonGroup* sharedExclusiveGroup=nullptr,
                 bool verticalSections=false,
                 QVector<int> sectionColumns={},
                 QHash<QString,QString> buttonCaptions={});


    /**
     * @brief Adds a section that was built outside this widget.
     *
     * The clothes dialog needs a grid per body area that mixes plain colour-scheme
     * buttons with SecondaryButtons, which cannot be expressed as a word list.
     * Everything else is the same as any other section: it joins `objectLayoutList`
     * (so sendString() reads its checked buttons), it shares `group` with the
     * other type sections, and the caption is drawn above it.
     */
    void addBuiltSection(QGridLayout* grid,QButtonGroup* group,
                         const QString& heading,int column);


    /// Moves itself next to the cursor, clamped to the screen, and shows.
    void showNearCursor();

    QPushButton* getColorButton(const QString&colorText,int r,int g,int b,bool ifblacktext=false);
    void sendString(bool ifadd);
    virtual void input_and_show(std::shared_ptr<teTag> in_tag=nullptr);
    /// input_and_show() without the popup: what the dialog shows for `in_tag`.
    void applyEditingTag(std::shared_ptr<teTag> in_tag);
private:
    /// What the colour buttons stand for, so a swatch can be painted from a word.
    QHash<QString,QColor> m_colorValues;
    QStringList m_multiColorTriggers;
    QStringList m_colorQueue;
    /// The trigger whose phrase is being built ("striped" / "gradient"); empty
    /// while the colour column behaves the normal way.
    QString m_multiColorTrigger;
    QWidget* m_colorQueueHost = nullptr;
    QFlowLayout* m_colorQueueLayout = nullptr;
    QFlowLayoutReorderer* m_colorQueueReorderer = nullptr;
    /// 红按钮:编辑现有 tag 时是垃圾桶(删掉它),新增时是叉号(取消编辑)。
    QPushButton* m_destroyButton = nullptr;
    bool m_editingTag = false;
    void applyDestroyMode();
    /// light / dark / deep - a prefix of a single colour, so they are out of the
    /// way while a queue of colours is being picked.
    QVector<QAbstractButton*> m_shadeButtons;
    int m_colorListPos = -1;
    bool multiColorActive() const { return !m_multiColorTrigger.isEmpty(); }
    void onColorButtonClicked(const QString& word);
    void enterMultiColor(const QString& trigger);
    void leaveMultiColor();
    /// Unchecks every trigger except `keep`: striped and gradient are the one
    /// pair of modifiers that cannot be on together (the phrase's connector and
    /// its tail word depend on which one it is).
    void uncheckOtherTriggers(const QString& keep);
    void refreshColorQueue();
    /// Pushes the window back inside the screen it is on (it grows when the queue
    /// appears, and it is anchored at the cursor).
    void clampToScreen();
    /**
     * @brief Takes a striped/gradient colour phrase out of `words`.
     *
     * "blue and white striped panties" gives the colours {blue, white} and the
     * trigger "striped"; the words that form the phrase are removed from `words`
     * so the ordinary button matching does not light one colour instead of
     * filling the queue. Pure, and false when there is no trigger or no colour.
     */
    bool takeMultiColorPhrase(QStringList& words,QStringList& colors,QString& trigger) const;
signals:
    void stringSignal(QString data, bool ifadd);
    /// The tag the dialog was opened for (empty for a brand-new tag). The clothes
    /// editor lights its type buttons from this, not from the tag list: the add
    /// widget knows nothing about what else is in the list.
    void editingTagChanged(const QString& tagText);
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
    /// Always the tag text without the icon (MARK-09 keeps the two apart).
    QStringList tagTexts() const;
    /**
     * @brief MARK-09: chips show an icon in front of the tag and offer to edit it.
     *
     * Off by default - the filter rules use the same widget and have no icons.
     * The tag text itself is never touched: the chip keeps it in a property, so
     * `tagTexts()` stays the string the editors build controls from.
     */
    void setIconEditingEnabled(bool on){ m_iconEditing=on; }
    /// Where a tag's icon comes from (the decoration registry). Empty = no icon.
    void setIconProvider(std::function<QString(const QString&)> provider);
    /// Relabels every chip - the decorations changed.
    void refreshIcons();
signals:
    void tagRemoved(QPushButton*);
    void tagEdit(QString);
    /// The user dragged a chip to another position.
    void orderChanged();
    /**
     * @brief The user asked to edit `tag`'s look.
     *
     * The chip does not edit anything itself any more: the decorations dialog is
     * the one place a tag's colour/icon is set, and the host opens it.
     */
    void iconEditRequested(QString tag);
private:
    /// Tag text of a chip, icon excluded.
    static QString chipTag(const QPushButton* button);
    /// Writes icon + tag into the chip's label.
    void relabel(QPushButton* button);
    QFlowLayout* m_layout;
    QFlowLayoutReorderer* m_reorderer=nullptr;
    QLineEdit* lineedit = nullptr;
    std::function<QString(const QString&)> m_iconProvider;
    bool m_iconEditing=false;
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
protected:
    /// The decorations can be edited while this window is closed (the decoration
    /// page's title opens the same dialog), so the chips are relabelled on entry.
    void showEvent(QShowEvent* event) override;
private:
    void initializeUI();
    void connectSignals();
signals:
    void tagsUpdated(QStringList);
    void tagsClearAll();
};
#endif // TESIGNALWIDGET_H
