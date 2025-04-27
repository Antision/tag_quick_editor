#ifndef TESIGNALWIDGET_H
#define TESIGNALWIDGET_H
#include "tepicturelistview.h"

class teWidget:public QWidget,public teObject{
    Q_OBJECT
public:
    QWidget*titleWidget=new QWidget(this);
    QWidget*content = new QWidget(this);
    QVBoxLayout vlayout;

    QHBoxLayout*titlelayout = new QHBoxLayout;

    QPushButton*close_btn;
    QPushButton*maximize_btn;
    QPushButton*minimize_btn;

    teWidget(QWidget*parent=nullptr);
    ~teWidget(){
        delete titlelayout;
    }
    int boundaryWidth=6;
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result);


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
    QHBoxLayout*content_layout = new QHBoxLayout;
    QVBoxLayout shade_layout;
    QButtonGroup*shade_buttongroup = new QButtonGroup(content);

    QGridLayout colors_layout;
    QButtonGroup*colors_buttongroup = new QButtonGroup(content);
    QHBoxLayout* extra_layout=nullptr;
    QButtonGroup* extra_buttongroup =nullptr;

    QVector<std::pair<QLayout*,QButtonGroup*>>objectLayoutList;
    QString otherWords;
    void uncheckAllButtons();
    colorsWidget(QVector<QPair<QStringList,bool>>&&objects,QWidget*parent=nullptr,int colorListPos=0,std::optional<QStringList>extraButtons ={});
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

class FilterTagWidget : public QWidget {
    Q_OBJECT
public:
    explicit FilterTagWidget(QWidget* parent = nullptr) : QWidget(parent) {
        m_layout = new QFlowLayout(this);
        m_layout->setContentsMargins(2, 2, 2, 2);
    }
    QVector<QPushButton*> tagItems;
    void addTag(const QString& text);
    void clear(){
        for(QPushButton*btn:tagItems)
            delete btn;
        tagItems.clear();
    }
    void setLineEdit(QLineEdit*le){
        lineedit=le;
        if(le)
            m_layout->insertWidget(0,le);
    }
signals:
    void tagRemoved(QPushButton*);
    void tagEdit(QString);
private:
    QFlowLayout* m_layout;
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
        QObject::connect(Ok_btn, &QPushButton::clicked, &loop, &QEventLoop::quit);
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
