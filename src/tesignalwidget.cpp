#include "tesignalwidget.h"

void addButtonsToGridLayout(QGridLayout *gridLayout, QButtonGroup *buttonGroup, QStringList &stringList) {
    if (!gridLayout || !buttonGroup || stringList.isEmpty()) {
        return;
    }
    const int itemCount = stringList.size();
    int rows = 4; // Default to 3 rows
    if (itemCount < 16) {
        int threerowsremainder = itemCount%3;
        if(threerowsremainder==0)threerowsremainder=3;
        int fourrowsremainder = itemCount%4;
        if(fourrowsremainder==0)threerowsremainder=4;
        if(threerowsremainder>fourrowsremainder)rows=3;
        else rows=4;
    }

    int cols = (itemCount + rows - 1) / rows; // Calculate the number of columns based on rows

    int currentRow = 0;
    int currentCol = 0;

    for (QString &str : stringList) {
        QPushButton *button = new QPushButton(std::move(str));
        button->setCheckable(true);

        buttonGroup->addButton(button);
        gridLayout->addWidget(button, currentRow, currentCol);

        // Move to the next grid cell
        currentCol++;
        if (currentCol >= cols) {
            currentCol = 0;
            currentRow++;
        }
    }
}

void colorsWidget::uncheckAllButtons(){
    for(auto&[layout,buttongroup]:objectLayoutList){
        if(buttongroup){
            auto buttons = buttongroup->buttons();
            for(QAbstractButton*btn:buttons)
                btn->setChecked(false);
        }
    }
    if(extra_buttongroup)
        for(QAbstractButton*btn:extra_buttongroup->buttons())
            btn->setChecked(false);
    otherWords.clear();
}

colorsWidget::colorsWidget(QVector<QPair<QStringList, bool> > &&objects, QWidget *parent, int colorListPos, std::optional<QStringList> extraButtons):teSignalWidget(parent){
    objectLayoutList.reserve(objects.size()+3);
    if(extraButtons){
        extra_buttongroup= new QButtonGroup(this);
        extra_layout=new QHBoxLayout(this);
    }

    for(auto&[stringlist,ifExclusive]:objects){
        QLayout* object_layout;
        if(stringlist.size()==1&&ifExclusive){
            QLabel* label = new QLabel(stringlist.first());
            object_layout=new QVBoxLayout(this);
            object_layout->addWidget(label);
            objectLayoutList.push_back({object_layout,nullptr});
            content_layout->addLayout(object_layout);
            extern QString labelItemStyle;
            label->setStyleSheet(labelItemStyle);
            continue;
        }
        QButtonGroup*object_buttongroup = new QButtonGroup(this);
        object_buttongroup->setExclusive(false);
        if(ifExclusive)
            connect(object_buttongroup,&QButtonGroup::buttonToggled,this,[object_buttongroup](QAbstractButton*btn,bool checked){if(checked)buttons_mutual_exclusion(btn,checked,object_buttongroup);},Qt::DirectConnection);
        if(extraButtons)
            connect(object_buttongroup,&QButtonGroup::buttonToggled,this,[this](QAbstractButton*btn,bool checked){
                if(checked)
                {foreach (QAbstractButton *button, extra_buttongroup->buttons()) {
                        button->setChecked(false);}
                }},Qt::DirectConnection);

        if(stringlist.size()>4){
            object_layout=new QGridLayout;
            addButtonsToGridLayout((QGridLayout*)object_layout,object_buttongroup,stringlist);
        }
        else{
            object_layout=new QVBoxLayout;
            for(QString&str:stringlist){
                QPushButton*objbtn = new QPushButton(str,this);
                object_buttongroup->addButton(objbtn);
                object_layout->addWidget(objbtn);
                objbtn->setCheckable(true);
            }
        }
        objectLayoutList.push_back({object_layout,object_buttongroup});
        content_layout->addLayout(object_layout);
    }

    if(colorListPos>-1){
        QPushButton* light_btn = new QPushButton(qsl("light"));
        shade_layout.addWidget(light_btn);
        shade_buttongroup->addButton(light_btn);
        light_btn->setCheckable(true);
        QPushButton* dark_btn = new QPushButton(qsl("dark"));
        shade_layout.addWidget(dark_btn);
        shade_buttongroup->addButton(dark_btn);
        dark_btn->setCheckable(true);
        QPushButton* deep_btn = new QPushButton(qsl("deep"));
        shade_layout.addWidget(deep_btn);
        shade_buttongroup->addButton(deep_btn);
        deep_btn->setCheckable(true);

        content_layout->insertLayout(colorListPos,&shade_layout);
        shade_buttongroup->setExclusive(false);
        connect(shade_buttongroup,&QButtonGroup::buttonToggled,this,[this](QAbstractButton*btn,bool checked){if(checked)buttons_mutual_exclusion(btn,checked,shade_buttongroup);},Qt::DirectConnection);
        objectLayoutList.insert(colorListPos,{&shade_layout,shade_buttongroup});
        colors_layout.setSpacing(2);
        colors_layout.setContentsMargins(1,1,1,1);

        colors_layout.addWidget(getColorButton(qsl("red"),255,0,0),0,0);
        colors_layout.addWidget(getColorButton(qsl("orange"),255,126,0,true),1,0);
        colors_layout.addWidget(getColorButton(qsl("brown"),96,48,0),2,0);
        colors_layout.addWidget(getColorButton(qsl("yellow"),255,255,0,true),3,0);

        colors_layout.addWidget(getColorButton(qsl("green"),0,255,0),0,1);
        colors_layout.addWidget(getColorButton(qsl("blue"),0,0,255),1,1);
        colors_layout.addWidget(getColorButton(qsl("pink"),255,0,255),2,1);
        colors_layout.addWidget(getColorButton(qsl("purple"),126,0,255),3,1);

        colors_layout.addWidget(getColorButton(qsl("aqua"),0,255,255),0,2);
        colors_layout.addWidget(getColorButton(qsl("white"),255,255,255,true),1,2);
        colors_layout.addWidget(getColorButton(qsl("black"),0,0,0),2,2);
        colors_layout.addWidget(getColorButton(qsl("grey"),127,127,127),3,2);
        content_layout->insertLayout(colorListPos+1,&colors_layout);
        objectLayoutList.insert(colorListPos+1,{&colors_layout,colors_buttongroup});
        colors_buttongroup->setExclusive(false);
        connect(colors_buttongroup,&QButtonGroup::buttonToggled,this,[this](QAbstractButton*btn,bool checked){if(checked)buttons_mutual_exclusion(btn,checked,colors_buttongroup);},Qt::DirectConnection);
    }

    if(extraButtons){
        extra_buttongroup->setExclusive(false);
        connect(extra_buttongroup,&QButtonGroup::buttonToggled,this,[this](QAbstractButton*btn,bool checked){if(checked)buttons_mutual_exclusion(btn,checked,extra_buttongroup);},Qt::DirectConnection);
        connect(extra_buttongroup,&QButtonGroup::buttonToggled,this,[this](QAbstractButton*btn,bool checked){
            if(checked)
                for(auto&[layout,bg]:objectLayoutList){
                    if(!bg||bg==extra_buttongroup)continue;
                    foreach (QAbstractButton *button, bg->buttons()) {
                        button->setChecked(false);
                    }
                }
        },Qt::DirectConnection);
        for(const QString&str:*extraButtons){
            QPushButton*objbtn = new QPushButton(str,this);
            extra_buttongroup->addButton(objbtn);
            extra_layout->addWidget(objbtn);
            objbtn->setCheckable(true);
        }
    }

    QPushButton* destroy_btn = new QPushButton(this);
    destroy_btn->setStyleSheet(destroyButtonStyle);
    connect(destroy_btn,&QPushButton::clicked,this,[this](){
        emit destroySignal();this->hide();uncheckAllButtons();
    },Qt::DirectConnection);
    connect(close_btn,&QPushButton::pressed,this,[this]{
        this->hide();uncheckAllButtons();
    },Qt::DirectConnection);

    destroy_btn->setFixedSize(60,40);

    QPushButton* Ok_btn = new QPushButton(this);
    Ok_btn->setStyleSheet(OkButtonStyle);
    connect(Ok_btn,&QPushButton::clicked,this,[this]{sendString(false);this->hide();},Qt::DirectConnection);
    Ok_btn->setFixedSize(60,40);

    QPushButton* add_btn = new QPushButton(this);
    add_btn->setStyleSheet(addButtonStyle);
    connect(add_btn,&QPushButton::clicked,this,[this]{sendString(true);},Qt::DirectConnection);
    add_btn->setFixedSize(60,40);

    signal_button_layout->addWidget(destroy_btn);
    signal_button_layout->addWidget(Ok_btn);
    signal_button_layout->addWidget(add_btn);

    layout->addLayout(content_layout);
    if(extra_layout)
        layout->addLayout(extra_layout);
    layout->addLayout(signal_button_layout);

}

QPushButton *colorsWidget::getColorButton(const QString &colorText, int r, int g, int b, bool ifblacktext){
    QPushButton*btn = new QPushButton;
    extern QString colorButtonStyleSheet;
    btn->setStyleSheet(colorButtonStyleSheet.arg(r).arg(g).arg(b).arg(ifblacktext?qsl("black"):qsl("white")));
    btn->setText(colorText);
    btn->setCheckable(true);
    colors_buttongroup->addButton(btn);
    return btn;
}

void colorsWidget::sendString(bool ifadd){
    QString sendData="";
    if(extra_buttongroup){
        for(QAbstractButton* b:extra_buttongroup->buttons()){
            if(b->isChecked()){
                sendData=b->text();
                emit stringSignal(sendData,ifadd);
                uncheckAllButtons();
                return;
            }
        }
    }
    int objectLayoutListSize = objectLayoutList.count();
    for(int i=0;i<objectLayoutListSize;++i){
        if(i==objectLayoutListSize-1&&!otherWords.isEmpty()){
            sendData+=otherWords;
        }
        auto&[layout,group] = objectLayoutList[i];
        if(group)
            for(QAbstractButton* b:group->buttons()){
                if(b->isChecked())sendData+=b->text()+' ';
            }
        else
            sendData+=dynamic_cast<QLabel*>(layout->itemAt(0)->widget())->text()+' ';
    }
    if(!sendData.isEmpty())sendData.chop(1);
    emit stringSignal(sendData,ifadd);
    uncheckAllButtons();
}

void colorsWidget::input_and_show(std::shared_ptr<tetagcore> in_tag){
    if(in_tag){
        QList<tewordcore*>&words = in_tag->words;
        int wordCount = words.count();
        if(extra_buttongroup){
            auto extrabuttonlist = extra_buttongroup->buttons();
            QString tagText = *in_tag.get();
            for(QAbstractButton*btn:extrabuttonlist)
                if(btn->text()==tagText){
                    btn->setChecked(true);
                    show();
                    return;
                }
        }
        for(int i=0;i<wordCount;++i){
            tewordcore*wc = words[i];
            QString wordtext = wc->text;

            for(auto&[layout,buttongroup]:objectLayoutList){
                if(buttongroup){
                    auto buttonlist = buttongroup->buttons();
                    for(QAbstractButton*btn:buttonlist)
                        if(btn->text()==wordtext){
                            btn->setChecked(true);
                            goto nextword;
                        }
                }else{
                    int count=layout->count();
                    for(int i=0;i<count;++i){
                        QLabel*label = qobject_cast<QLabel*>(layout->itemAt(i)->widget());
                        if(label){
                            goto nextword;
                        }
                    }
                }
            }
            otherWords.append(*wc+' ');
        nextword:;
        }
    }
    show();
}
QString teWidgetStyle = QStringLiteral(R"(
QWidget{
    background-color:rgb(27,27,27);
    color:white;
    font: 12pt "Segoe UI";
}
QWidget#title{
    border:1px solid rgb(64,64,64);
}
QWidget#content{
    border:1px solid #43367b;
    background-color:black;
}
QScrollArea{border:1px solid #4c399e;}
QScrollArea:hover{border:1px solid #ad5dff;}

QScrollBar{background:rgba(0,0,0,210);}
QScrollBar::vertical{margin: 0px 0px 0px 0px;border:0px solid #5421ff;width:5px;}
QScrollBar::horizontal{margin: 0px 0px 0px 0px;border:0px solid #5421ff;height:5px;}
QScrollBar::handle{
color:#00ff00
}
QScrollBar::handle:!pressed{background:rgba(255,255,255,60);}
QScrollBar::handle:pressed{background:rgba(255,255,255,150);}

QScrollBar::handle:vertical{min-height:20px; border-radius: 2px;}
QScrollBar::handle:horizontal{min-width:20px;border-radius: 2px;}

QScrollBar::sub-page,QScrollBar::add-page {
    background-color: None;
}
QScrollBar::add-line,QScrollBar::sub-line{
border:0px solid black;
width:0px;
height:0px;
}
QScrollBar::sub-page,QScrollBar::add-page {
    background-color: #05191f;
}
QPlainTextEdit{
background-color:rgb(35,35,35);
border-radius:4px;
border:1px solid rgb(100,100,100);
}
QLabel{
    background-color:black;
    color:white;
}
QPushButton{
    outline: 0px solid black;
    border: 1px solid #4c399e;
    background-color:transparent;
    padding: 0px;
}
QPushButton:hover{
    border: 1px solid #a858f2;
    background-color:rgba(177,100,255,60);
}
QPushButton:checked{
    border: 1px solid #12b594;
    background-color:#00592c;
}
QPushButton:checked:hover{
    border: 1px solid #12f3e0;
    background-color:#00a452;
}
QCheckBox{
    outline: 0px solid black;
    border: 1px solid #4c399e;
}
QCheckBox:hover{
    border: 1px solid #a858f2;
    background-color:rgba(177,100,255,60);
}
QLineEdit{
    outline: 0px solid black;
    border: 1px solid #4c399e;
}
QComboBox{
    border: 1px solid #4c399e;
}
QComboBox:hover{
    border: 1px solid #a858f2;
    background-color:rgba(177,100,255,60);
}
QAbstractItemView{
    font:10pt "Segoe UI";
    outline: 0px solid black;
    border: 1px solid #4c399e;
    selection-background-color: #3aaf74;
}
QAbstractItemView::item{
    height: 25px;
    border:1px solid #57487a;
    background-color: #171717;
}
QAbstractItemView::item:hover{
    border:1px solid #4a25a4;
    background-color: #4c4a5d;
}

)");

QString destroyButtonStyle = QStringLiteral(R"(
QPushButton{
image:url(:/res/bin_sleep.png);
background-color:rgba(255,0,0,30);
border-color:rgb(150,0,0);
}
QPushButton:hover{
image:url(:/res/bin.png);
background-color:rgba(255,0,0,120);
border-color:rgb(255,0,0);
}
QPushButton:hover:pressed{
background-color:rgba(255,0,0,150);
}
)");


QString OkButtonStyle = qsl(R"(
QPushButton{
image:url(:/res/tick_sleep.png);
background-color:rgba(25,235,130,30);
border-color:#50bc86;
}
QPushButton:hover{
image:url(:/res/tick.png);
background-color:rgba(25,235,130,120);
border-color:#54eda1;
}
QPushButton:hover:pressed{
background-color:rgba(25,235,130,150);
}
)");
QString addButtonStyle = qsl(R"(
QPushButton{
image:url(:/res/plus_sleep.png);
background-color:rgba(25,235,130,30);
border-color:#50bc86;
}
QPushButton:hover{
image:url(:/res/plus.png);
background-color:rgba(25,235,130,120);
border-color:#54eda1;
}
QPushButton:hover:pressed{
background-color:rgba(25,235,130,150);
}
)");
QString tagItemStyle = QStringLiteral(R"(
QPushButton{
    border-radius: 3px;
}
QPushButton:!hover{
    border:1px solid #109452;
    background-color:#1a5759;
}
QPushButton:hover{
    border:1px solid #00d96d;
    background-color:#1a5759;
}
)");
QString labelItemStyle = qsl(R"(
QLabel{
    outline: 0px solid black;
    padding: 0px;
    border: 1px solid #12b594;
    background-color:#00592c;
}
QLabel:hover{
    border: 1px solid #12f3e0;
    background-color:#00a452;
}
)");
QString colorButtonStyleSheet = qsl(R"(
QPushButton{
background-color:rgba(%1,%2,%3,164);
border:1px solid #412579;
font:12pt "Segoe UI";
color:%4;
}
QPushButton:hover{
background-color:rgba(%1,%2,%3,210);
border:1px solid #541ec1;
}
QPushButton:checked{
background-color:rgba(%1,%2,%3,255);
border:1px solid #00b059;
}
QPushButton:checked:hover{
border:1px solid #00eb77;
}
)");

teWidget::teWidget(QWidget *parent):QWidget(parent){
    setStyleSheet(teWidgetStyle);
    HWND hwnd = reinterpret_cast<HWND>(winId());
    DWORD style = GetWindowLong(hwnd, GWL_STYLE);
    SetWindowLongPtr(hwnd, GWL_STYLE, style | WS_MAXIMIZEBOX | WS_THICKFRAME | WS_CAPTION);
    setWindowIcon(QIcon(":/res/icon.ico"));

    content->setObjectName(qsl("content"));
    setLayout(&vlayout);
    vlayout.addWidget(titleWidget);
    vlayout.addWidget(content);

    vlayout.setSpacing(0);
    vlayout.setContentsMargins(0,0,0,0);

    titleWidget->setObjectName("title");
    titleWidget->setFixedHeight(20);
    titleWidget->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);
    titleWidget->setLayout(titlelayout);

    {
        QSpacerItem *titlespacer = new QSpacerItem(0,0,QSizePolicy::Expanding,QSizePolicy::Fixed);
        titlelayout->addItem(titlespacer);
        titlelayout->setContentsMargins(0,0,0,0);
        titlelayout->setSpacing(0);
        close_btn = new QPushButton(titleWidget);
        maximize_btn = new QPushButton(titleWidget);
        minimize_btn = new QPushButton(titleWidget);
        close_btn->setStyleSheet(QStringLiteral(R"(QPushButton{
border-image:url(:/res/close_sleep.png);}
QPushButton:hover{border-image:url(:/res/close.png);})"));
        maximize_btn->setStyleSheet(QStringLiteral(R"(QPushButton{
border-image:url(:/res/maximize_sleep.png);}
QPushButton:hover{border-image:url(:/res/maximize.png);})"));
        minimize_btn->setStyleSheet(QStringLiteral(R"(QPushButton{
border-image:url(:/res/minimize_sleep.png);}
QPushButton:hover{border-image:url(:/res/minimize.png);})"));
        close_btn->setFixedWidth(30);
        maximize_btn->setFixedWidth(30);
        minimize_btn->setFixedWidth(30);
        connect(close_btn,&QPushButton::pressed,this,&QWidget::hide,Qt::DirectConnection);
        connect(maximize_btn,&QPushButton::pressed,this,[this]{if(isMaximized())showNormal(); else showMaximized();},Qt::DirectConnection);
        connect(minimize_btn,&QPushButton::pressed,this,&QWidget::showMinimized,Qt::DirectConnection);
        titlelayout->addWidget(minimize_btn);
        titlelayout->addWidget(maximize_btn);
        titlelayout->addWidget(close_btn);
        minimize_btn->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Preferred);
        maximize_btn->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Preferred);
        close_btn->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Preferred);
    }
}

bool teWidget::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
    if (eventType != "windows_generic_MSG")
        return false;

    MSG* msg = static_cast<MSG*>(message);
    QWidget* widget = QWidget::find(reinterpret_cast<WId>(msg->hwnd));
    if (!widget)
        return false;

    switch (msg->message) {

    case WM_NCCALCSIZE: {
        if (msg->wParam) {
            *result = 0;
            return true;
        }
        break;
    }
    case WM_NCHITTEST:{
        POINT pt = { GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam) };
        ScreenToClient(msg->hwnd, &pt);
        int xPos = pt.x;
        int yPos = pt.y;
        if (yPos > boundaryWidth&&yPos < titleWidget->height()&&xPos>0&&xPos<minimize_btn->x())
        {
            *result = HTCAPTION;
        }
        else if(xPos < boundaryWidth && yPos<boundaryWidth)
            *result = HTTOPLEFT;
        else if(xPos>=width()-boundaryWidth&&yPos<boundaryWidth)
            *result = HTTOPRIGHT;
        else if(xPos<boundaryWidth&&yPos>=height()-boundaryWidth)
            *result = HTBOTTOMLEFT;
        else if(xPos>=width()-boundaryWidth&&yPos>=height()-boundaryWidth)
            *result = HTBOTTOMRIGHT;
        else if(xPos < boundaryWidth)
            *result = HTLEFT;
        else if(xPos>=width()-boundaryWidth)
            *result = HTRIGHT;
        else if(yPos<boundaryWidth)
            *result = HTTOP;
        else if(yPos>=height()-boundaryWidth)
            *result = HTBOTTOM;
        else{
            return false;
        }
        return true;
    }
    case WM_GETMINMAXINFO: {
        MINMAXINFO* minMax = reinterpret_cast<MINMAXINFO*>(msg->lParam);

        HMONITOR monitor = MonitorFromWindow(msg->hwnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO monitorInfo = { sizeof(MONITORINFO) };
        GetMonitorInfo(monitor, &monitorInfo);

        RECT workArea = monitorInfo.rcWork;
        RECT monitorArea = monitorInfo.rcMonitor;

        minMax->ptMaxPosition.x = workArea.left - monitorArea.left;
        minMax->ptMaxPosition.y = workArea.top - monitorArea.top;
        minMax->ptMaxSize.x = workArea.right - workArea.left;
        minMax->ptMaxSize.y = workArea.bottom - workArea.top;

        *result = 0;
        return true;
    }
    break;

    default:
        break;
    }
    return QWidget::nativeEvent(eventType, message, result);
}

void teSignalWidget::buttons_mutual_exclusion(QAbstractButton *btn, bool checked, QButtonGroup *btn_gop){
    if(checked){
        auto buttons = btn_gop->buttons();
        for(QAbstractButton* the_btn:buttons){
            if(btn!=the_btn)
                the_btn->setChecked(false);
        }
    }
}

teInputWidget::teInputWidget(QWidget *parent):teSignalWidget(parent){
    contentLayout = new QVBoxLayout(content);
    plainTextEdit = new QPlainTextEdit;
    content->setLayout(contentLayout);
    contentLayout->setAlignment(Qt::AlignRight);
    QPushButton* Ok_btn = new QPushButton;
    Ok_btn->setStyleSheet(OkButtonStyle);
    connect(Ok_btn,&QPushButton::pressed,this,&teInputWidget::onOkClicked,Qt::DirectConnection);
    Ok_btn->setFixedSize(60,40);
    contentLayout->setContentsMargins(3,3,3,3);
    contentLayout->addWidget(plainTextEdit);
    contentLayout->addWidget(Ok_btn,Qt::AlignHCenter);
    resize(400,300);
}

void tePictureFileModel_filted::clear() {
    beginResetModel();
    picturefiles.clear();
    endResetModel();
    emit clearAllFiles();
}

void tePictureFileModel_filted::setdata(QList<QModelIndex> &&indexes){
    beginResetModel();
    picturefiles = std::move(indexes);
    endResetModel();
}

void tePictureFileModel_filted::append(const QList<QModelIndex> &files) {
    if (files.isEmpty()) return;
    beginInsertRows(QModelIndex(), picturefiles.size(), picturefiles.size() + files.size() - 1);
    picturefiles.append(files);
    endInsertRows();
}

int tePictureFileModel_filted::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) return 0;
    return picturefiles.size();
}

QVariant tePictureFileModel_filted::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= picturefiles.size())
        return QVariant();
    if (role == Qt::DisplayRole) {
        return listview->model()->data(picturefiles[index.row()]);
    }
    return QVariant();
}

void FilterTagWidget::addTag(const QString &text) {
    QPushButton* tagItem = new QPushButton(text, this);
    extern QString tagItemStyle;
    tagItem->setStyleSheet(tagItemStyle);
    connect(tagItem, &QPushButton::clicked, this, [this, tagItem]{
        emit tagEdit(tagItem->text());
        tagItems.removeAt(tagItems.indexOf(tagItem));
        m_layout->removeWidget(tagItem);
        delete tagItem;
    },Qt::DirectConnection);
    tagItems.push_back(tagItem);
    lineedit?m_layout->insertWidget(-1,tagItem):m_layout->addWidget(tagItem);
}

void filterWidget::initializeUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(content);

    const QStringList rules = {"Must include", "At least one", "Does not include", "Includes only one"};
    for(int i = 0; i < 4; ++i) {
        QHBoxLayout* ruleLayout = new QHBoxLayout(this);

        QLabel* ruleLabel = new QLabel(rules[i]);
        ruleLabel->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
        ruleLayout->addWidget(ruleLabel);

        tagWidgets[i] = new FilterTagWidget;
        lineEdits[i] = new QLineEdit(this);
        lineEdits[i]->setPlaceholderText("Press Enter after entering");
        lineEdits[i]->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
        lineEdits[i]->setMinimumWidth(200);
        ruleLayout->addWidget(tagWidgets[i]);
        tagWidgets[i]->setLineEdit(lineEdits[i]);
        mainLayout->addLayout(ruleLayout);
    }
    filterBtn = new QPushButton("filt");
    mainLayout->addWidget(filterBtn);
    mainLayout->addWidget(myview);

    myview->setItemDelegate(&pictureFileDelegate);
    myview->setModel(&mymodel);
    myview->setSelectionMode(QAbstractItemView::ExtendedSelection);
    myview->setSelectionBehavior(QAbstractItemView::SelectItems);
    myview->installEventFilter(this);
    myview->viewport()->installEventFilter(this);
}

void filterWidget::setupCompleter(QLineEdit *edit) {
    completerModel = new QStringListModel(edit);
    QCompleter* completer = new QCompleter(completerModel, edit);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    edit->setCompleter(completer);

    connect(edit, &QLineEdit::textChanged, [this](const QString text){
        QStringList suggestions = fetchSuggestions(text);
        completerModel->setStringList(suggestions);
    });
}

QStringList filterWidget::fetchSuggestions(const QString &input, int maxSuggestions)
{
    struct Suggestion {
        QString text;
        unsigned int frequency;
        bool operator<(const Suggestion& other) const {
            return frequency != other.frequency ?
                       frequency > other.frequency :
                       text.length() < other.text.length();
        }
    };

    std::priority_queue<Suggestion> suggestions;
    QString lowerInput = input.toLower();

    auto it = std::lower_bound(ctags.begin(), ctags.end(), lowerInput.toStdString(),
                               [](const ctag& tag, const std::string& prefix) {
                                   return std::string(tag.text).compare(0, prefix.length(), prefix) < 0;
                               });

    for (; it != ctags.end(); ++it) {
        std::string tagText(it->text);
        if (tagText.compare(0, lowerInput.length(), lowerInput.toStdString()) != 0) {
            break;
        }

        const ctag* finalTag = &(*it);
        if (it->ref_pos != 0xffffffffu) {
            finalTag = &ctags[it->ref_pos];
        }

        QString suggestion = QString::fromStdString(finalTag->text);
        if (suggestions.size() < maxSuggestions) {
            suggestions.push({suggestion, finalTag->frequency});
        } else if (finalTag->frequency > suggestions.top().frequency) {
            suggestions.pop();
            suggestions.push({suggestion, finalTag->frequency});
        }
    }

    QStringList result;
    while (!suggestions.empty()) {
        result.prepend(suggestions.top().text);
        suggestions.pop();
    }
    return result;
}

void filterWidget::sendSelection(){
    if (lastSelected.indexes().empty()&&lastDeselected.indexes().empty()) return;

    if(!lastSelected.indexes().empty())
        listview->scrollTo(qvariant_cast<QModelIndex>(lastSelected.indexes().first().data()));

    QItemSelection newSelection;

    for(const QModelIndex& idx : myview->selectionModel()->selectedIndexes()) {
        QModelIndex targetIndex = qvariant_cast<QModelIndex>(
            dynamic_cast<tePictureFileModel_filted*>(myview->model())->picturefiles[idx.row()]
            );
        newSelection.select(targetIndex, targetIndex);
    }
    listview->selectionModel()->select(newSelection, QItemSelectionModel::ClearAndSelect);
}

bool filterWidget::eventFilter(QObject *watched, QEvent *event){
    if(event->type()==QEvent::Type::MouseButtonRelease){
        sendSelection();
    }
    return false;
}

void filterWidget::connectSignals() {
    for(int i = 0; i < 4; ++i) {
        connect(lineEdits[i], &QLineEdit::returnPressed,this, [this, i]{
            tagWidgets[i]->addTag(lineEdits[i]->text());
            lineEdits[i]->clear();
        },Qt::DirectConnection);

        connect(tagWidgets[i], &FilterTagWidget::tagEdit,this, [this, i](QString tagstr){
            lineEdits[i]->setText(tagstr);
        },Qt::DirectConnection);
    }

    connect(filterBtn, &QPushButton::clicked, this, &filterWidget::performFilter,Qt::DirectConnection);

    connect(myview->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, [this](const QItemSelection &selected, const QItemSelection &deselected) {
                if(selected.empty()&&deselected.empty()) return;
                SelectionSent=false;
                lastSelected=selected;
                lastDeselected=deselected;
                if((GetAsyncKeyState(VK_LBUTTON) & 0x8000) == 0){
                    sendSelection();
                }
            }, Qt::DirectConnection);
}

void filterWidget::addTagToRule(RuleType type, const QString &tag) {
    if(tag.isEmpty()) return;
    tagWidgets[type]->addTag(tag);
}

void filterWidget::performFilter() {
    teFiltRule rule;
    for(QPushButton*tagitem:tagWidgets[AllOf]->tagItems)
        rule.a.push_back(tagitem->text());
    for(QPushButton*tagitem:tagWidgets[AnyOf]->tagItems)
        rule.r.push_back(tagitem->text());
    for(QPushButton*tagitem:tagWidgets[NoneOf]->tagItems)
        rule.n.push_back(tagitem->text());
    for(QPushButton*tagitem:tagWidgets[ExactOne]->tagItems)
        rule.x.push_back(tagitem->text());
    mymodel.setdata(listview->filt(rule));
}

void customControlWidget::initializeUI() {
    resize(300,250);
    QVBoxLayout* mainLayout = new QVBoxLayout(content);
    customTagsLayout->addWidget(tagWidget);
    lineEdit->setPlaceholderText("Press Enter after entering");
    lineEdit->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
    lineEdit->setMinimumWidth(200);
    tagWidget->setLineEdit(lineEdit);
    mainLayout->addLayout(customTagsLayout);
    OK_Btn = new QPushButton(this);
    OK_Btn->setStyleSheet(OkButtonStyle);
    Bin_Btn = new QPushButton(this);
    Bin_Btn->setStyleSheet(destroyButtonStyle);
    buttonLayout->addWidget(OK_Btn);
    buttonLayout->addWidget(Bin_Btn);
    mainLayout->addLayout(buttonLayout);
}

void customControlWidget::connectSignals() {
    connect(lineEdit, &QLineEdit::returnPressed,this, [this]{
        tagWidget->addTag(lineEdit->text());
        lineEdit->clear();
    },Qt::DirectConnection);

    connect(tagWidget, &FilterTagWidget::tagEdit,this, [this](QString tagstr){
        lineEdit->setText(tagstr);
    },Qt::DirectConnection);

    connect(OK_Btn,&QPushButton::clicked,this,[this]{
        QStringList sendList;
        for(QPushButton*btn:tagWidget->tagItems){
            sendList.push_back(btn->text());
        }
        emit tagsUpdated(sendList);
        this->hide();
    });
    connect(Bin_Btn,&QPushButton::clicked,this,[this]{
        QMessageBox msgBox;
        msgBox.setStandardButtons(QMessageBox::Yes|QMessageBox::No);
        msgBox.setText("Are you sure you want to clear all custom tags?");
        if (msgBox.exec() == QMessageBox::No) {
            return;
        }
        emit tagsClearAll();
        tagWidget->clear();
    });
}
