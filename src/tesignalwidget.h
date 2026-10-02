#ifndef TESIGNALWIDGET_H
#define TESIGNALWIDGET_H
#include "tepicturelistview.h"
#include "pch.h"

class teWidget: public QWidget, public teObject {
    Q_OBJECT
public:
    QWidget* titleWidget = new QWidget(this);
    QWidget* content = new QWidget(this);
    QVBoxLayout vlayout;
    QHBoxLayout* titlelayout = new QHBoxLayout;
    QPushButton* close_btn;
    QPushButton* maximize_btn;
    QPushButton* minimize_btn;

    teWidget(QWidget* parent = nullptr);

    ~teWidget() {
        delete titlelayout;
    }

    int boundaryWidth = 6; // 逻辑像素

    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
};
extern QString destroyButtonStyle,OkButtonStyle,addButtonStyle;
class teSignalWidget:public teWidget
{
    Q_OBJECT
public:
    teSignalWidget(QWidget*parent=nullptr):teWidget(parent){
        connect(close_btn,&QPushButton::pressed,this,&teSignalWidget::onCancelClicked,Qt::DirectConnection);
    };
    static void buttons_mutual_exclusion(QAbstractButton* btn,bool checked,QButtonGroup*btn_gop);
    void onCancelClicked(){
        emit cancelSignal();
        this->hide();
    }
signals:
    void stringSignal(QString data,bool ifadd);
    void destroySignal();
    void cancelSignal();
};

class teInputWidget:public teSignalWidget
{
    Q_OBJECT
public:
    QVBoxLayout* contentLayout=nullptr;
    QPlainTextEdit* plainTextEdit=nullptr;
    teInputWidget(QWidget*parent=nullptr);;
    void start(const QString& in_str){
        plainTextEdit->setPlainText(in_str);
    }
    void onOkClicked(){
        this->hide();
        stringSignal(plainTextEdit->toPlainText(),false);
        plainTextEdit->clear();
    }
};

void addButtonsToGridLayout(QGridLayout *gridLayout, QButtonGroup *buttonGroup, QStringList &stringList);

class colorsWidget:public teSignalWidget{
    Q_OBJECT
public:
    QVBoxLayout*layout = new QVBoxLayout(content);
    QHBoxLayout* signal_button_layout = new QHBoxLayout;
    /// Sections are layed out horizontally by default; pass
    /// `verticalSections = true` to stack them instead.
    QBoxLayout* content_layout = nullptr;
    QVBoxLayout shade_layout;
    QButtonGroup*shade_buttongroup = new QButtonGroup(content);

    QGridLayout colors_layout;
    QButtonGroup*colors_buttongroup = new QButtonGroup(content);
    QHBoxLayout* extra_layout=nullptr;
    QButtonGroup* extra_buttongroup =nullptr;

    QVector<std::pair<QLayout*,QButtonGroup*>>objectLayoutList;
    QString otherWords;
    void uncheckAllButtons();
    /// Adds one section (with an optional caption above it) to content_layout.
    void addSectionToContent(QLayout* sectionLayout,const QString& heading);
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
     */
    colorsWidget(QVector<QPair<QStringList,bool>>&&objects,QWidget*parent=nullptr,int colorListPos=0,
                 std::optional<QStringList>extraButtons={},
                 QVector<QString> headings={},
                 QButtonGroup* sharedExclusiveGroup=nullptr,
                 bool verticalSections=false);
    void closeEvent(QCloseEvent *event) override {
        emit cancelSignal();
        uncheckAllButtons();
    }

    QPushButton* getColorButton(const QString&colorText,int r,int g,int b,bool ifblacktext=false);
    void sendString(bool ifadd);
    virtual void input_and_show(std::shared_ptr<tetagcore> in_tag=nullptr);

};

class tePictureFileModel_filted : public QAbstractListModel,public teObject {
    Q_OBJECT
public:
    QList<QModelIndex> picturefiles;
    tePictureListView*listview;
    tePictureFileModel_filted(tePictureListView*parentListView,QObject *parent = nullptr) : QAbstractListModel(parent),listview(parentListView) {
        connect(dynamic_cast<tePictureFileModel*>(listview->model()),&tePictureFileModel::clearAllFiles,this,&tePictureFileModel_filted::clear,Qt::DirectConnection);
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
class filterWidget : public teWidget {
    Q_OBJECT
public:
    filterWidget(tePictureListView* in_listview) : listview(in_listview) {

        initializeUI();
        connectSignals();
    }
    ~filterWidget(){
    }
    enum RuleType { AllOf, AnyOf, NoneOf, ExactOne };
    tePictureListView* listview;
    QListView* myview = new QListView;
    tePictureFileModel_filted mymodel{listview};
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

class customControlWidget:public teWidget{
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
    QHBoxLayout*buttonLayout = new QHBoxLayout(this);
private:
    void initializeUI();
    void connectSignals();
signals:
    void tagsUpdated(QStringList);
    void tagsClearAll();
};
class adultCheckWindow:public teWidget{
public:
    QVBoxLayout*content_layout = new QVBoxLayout;
    QGridLayout*button_layout = new QGridLayout;
    QButtonGroup* bg = new QButtonGroup(this);
    QPushButton* Ok_btn = new QPushButton;
    int*nsfwMode;
    adultCheckWindow(int*in_nsfwMode):nsfwMode(in_nsfwMode){
        QLabel*title = new QLabel("Which websites are absolutely safe for children to use?",this);
        QFont ft;
        ft.setPointSize(25);
        ft.setBold(true);
        title->setFont(ft);
        content_layout->addWidget(title);

        QStringList btnIcons
            {
            ":/eh.png",":/pixiv.png",":/fantia.png",
            ":/danbooru.png",":/x.png",":/civitai.png",
            ":/wallpaper engine.png",":/tiktok.png",":/avast.png"
        };
        for(int i=0;i<3;++i){
            for(int j=0;j<3;++j){
                QPushButton* btn = new QPushButton();
                btn->setCheckable(true);
                btn->setFixedSize(64,64);
                bg->addButton(btn);
                bg->setExclusive(false);
                btn->setStyleSheet(QString(R"(image:url("%1"))").arg(btnIcons[i*3+j]));
                button_layout->addWidget(btn,i,j);
            }
        }
        content_layout->addLayout(button_layout);
        Ok_btn->setStyleSheet(OkButtonStyle);
        content_layout->addWidget(Ok_btn);
        content->setLayout(content_layout);
        show();
        QEventLoop loop;
        connect(Ok_btn, &QPushButton::clicked, &loop, &QEventLoop::quit);
        connect(close_btn,&QPushButton::clicked,this,[&loop]{
            QCoreApplication::quit();
            loop.quit();
        });
        loop.exec();
        for(int i=0;i<8;++i){
            QAbstractButton*btn = bg->buttons()[i];
            if(btn->isChecked()){
                *nsfwMode=-1;
                break;
            }
        }
        if(!bg->buttons()[8]->isChecked())
            *nsfwMode=-1;
        if(*nsfwMode==0)
            *nsfwMode=1;

    }
};
#endif // TESIGNALWIDGET_H
