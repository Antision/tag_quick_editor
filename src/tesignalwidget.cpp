#include "tesignalwidget.h"

// 一组互斥按钮里只留一个勾选;colorsWidget 的构造函数里的 lambda 要用,
// 所以先声明(定义在文件后半段)。
static void buttons_mutual_exclusion(QAbstractButton* btn, bool checked, QButtonGroup* btn_gop);

void addButtonsToGridLayout(QGridLayout *gridLayout, QButtonGroup *buttonGroup, QStringList &stringList) {
    if (!gridLayout || !buttonGroup || stringList.isEmpty()) {
        return;
    }
    const int itemCount = stringList.size();
    int rows = 4;
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

void colorsWidget::addSectionToContent(QLayout* sectionLayout,const QString& heading,int column)
{
    QBoxLayout* target = (column>=0&&column<int(columns.size()))?columns[column]:content_layout;
    if(heading.isEmpty()){
        target->addLayout(sectionLayout);
        return;
    }
    // A caption is purely visual: it is wrapped together with its section so it
    // is never part of a composed tag.
    QVBoxLayout* wrapper = new QVBoxLayout;
    wrapper->setContentsMargins(0,2,0,0);
    wrapper->setSpacing(0);
    QLabel* label = new QLabel(heading,this);
    label->setProperty("type","sectionHeading");
    label->setStyleSheet(qsl("QLabel{color:#c8c8c8;font-size:12px;font-style:italic;border:none;background:transparent;}"));
    wrapper->addWidget(label);
    wrapper->addLayout(sectionLayout);
    target->addLayout(wrapper);
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

colorsWidget::colorsWidget(QVector<QPair<QStringList, bool> > &&objects, QWidget *parent, int colorListPos, std::optional<QStringList> extraButtons, QVector<QString> headings, QButtonGroup* sharedExclusiveGroup, bool verticalSections, QVector<int> sectionColumns):LiteWidget2(parent){
    applyLiteWindowStyle(this);
    // 每个窗口类各自记住自己上次的尺寸/位置。颜色窗和服饰窗都是 colorsWidget,
    // 内容差得多,用第一段的第一个词当后缀区分开。
    {
        QString geometryKey = QStringLiteral("colorsWidget");
        if (!objects.isEmpty() && !objects.first().first.isEmpty())
            geometryKey += QLatin1Char('|') + objects.first().first.first();
        setGeometryKey(geometryKey);
    }
    layout = new QVBoxLayout(contentWidget());
    // Three arrangements:
    //  - sectionColumns given: that many columns, each section stacked inside its
    //    column (the clothes dialog puts the colours/features left and the
    //    clothes types right, so the window stays square instead of a strip);
    //  - verticalSections: every section on its own line;
    //  - otherwise: every section in its own column (the old default).
    int columnCount = 0;
    for(int column:sectionColumns)
        columnCount = std::max(columnCount,column+1);
    if(columnCount>0){
        content_layout = new QHBoxLayout;
        columns.reserve(columnCount);
        for(int i=0;i<columnCount;++i){
            auto* column = new QVBoxLayout;
            column->setContentsMargins(0,0,0,0);
            column->setSpacing(1);
            columns.push_back(column);
            content_layout->addLayout(column);
        }
    }else
        content_layout = verticalSections ? static_cast<QBoxLayout*>(new QVBoxLayout)
                                          : static_cast<QBoxLayout*>(new QHBoxLayout);
    content_layout->setContentsMargins(0,0,0,0);
    content_layout->setSpacing(1);
    objectLayoutList.reserve(objects.size()+3);
    if(extraButtons){
        extra_buttongroup= new QButtonGroup(this);
        extra_layout=new QHBoxLayout;
    }

    const bool sharedGroup = (sharedExclusiveGroup != nullptr);
    shared_group = sharedExclusiveGroup;

    for(int sectionIndex=0;sectionIndex<objects.size();++sectionIndex){
        auto&[stringlist,ifExclusive]=objects[sectionIndex];
        QLayout* object_layout;
        if(stringlist.size()==1&&ifExclusive){
            QLabel* label = new QLabel(stringlist.first());
            object_layout=new QVBoxLayout;
            object_layout->addWidget(label);
            objectLayoutList.push_back({object_layout,nullptr});
            addSectionToContent(object_layout,sectionIndex<headings.size()?headings[sectionIndex]:QString(),
                                sectionColumns.value(sectionIndex,-1));
            extern QString labelItemStyle;
            label->setStyleSheet(labelItemStyle);
            continue;
        }
        // Sections that must be mutually exclusive across the whole dialog share
        // one QButtonGroup; the rest get their own.
        const bool reuseShared = sharedGroup && ifExclusive;
        QButtonGroup*object_buttongroup = reuseShared ? sharedExclusiveGroup : new QButtonGroup(this);
        if(!reuseShared){
            object_buttongroup->setExclusive(false);
            if(ifExclusive)
                connect(object_buttongroup,&QButtonGroup::buttonToggled,this,[object_buttongroup](QAbstractButton*btn,bool checked){if(checked)buttons_mutual_exclusion(btn,checked,object_buttongroup);},Qt::DirectConnection);
            if(extraButtons)
                connect(object_buttongroup,&QButtonGroup::buttonToggled,this,[this](QAbstractButton*btn,bool checked){
                    if(checked)
                    {foreach (QAbstractButton *button, extra_buttongroup->buttons()) {
                            button->setChecked(false);}
                    }},Qt::DirectConnection);
        }

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
        addSectionToContent(object_layout,sectionIndex<headings.size()?headings[sectionIndex]:QString(),
                            sectionColumns.value(sectionIndex,-1));
    }

    if(sharedGroup){
        // One connection for the whole shared group: any newly checked button
        // unchecks all the others, across every section.
        sharedExclusiveGroup->setParent(this);      // the dialog owns the group
        sharedExclusiveGroup->setExclusive(false);
        connect(sharedExclusiveGroup,&QButtonGroup::buttonToggled,this,[sharedExclusiveGroup](QAbstractButton*btn,bool checked){if(checked)buttons_mutual_exclusion(btn,checked,sharedExclusiveGroup);},Qt::DirectConnection);
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

        QBoxLayout* colorColumn = columns.empty()?content_layout:columns.front();
        colorColumn->insertLayout(colorListPos,&shade_layout);
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
        colorColumn->insertLayout(colorListPos+1,&colors_layout);
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
    connect(this,&LiteWidget2::closed,this,[this]{
        emit cancelSignal();
        uncheckAllButtons();
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

void colorsWidget::showNearCursor()
{
    // These windows are opened with the mouse, so they belong next to the
    // pointer - but a long one used to be pushed off the screen (and then stayed
    // there), so it is clamped onto the screen it appears on.
    adjustSize();
    const QPoint cursor = QCursor::pos();
    QScreen* screen = QGuiApplication::screenAt(cursor);
    if(!screen)
        screen = QGuiApplication::primaryScreen();
    const QRect available = screen?screen->availableGeometry():QRect(cursor,QSize(800,600));
    QPoint pos(cursor.x()-width(),cursor.y()-height()/2);
    pos.setX(std::clamp(pos.x(),available.left(),std::max(available.left(),available.right()-width()+1)));
    pos.setY(std::clamp(pos.y(),available.top(),std::max(available.top(),available.bottom()-height()+1)));
    move(pos);
    show();
    raise();
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
    // Where the words go that no button of this dialog stands for (they are what
    // the tag had beyond the buttons: "short", "sleeveless", ...). They used to
    // be appended just before the *last* section, which is right for a dialog
    // whose last section is the variant word ("<colour> <other> hair") - but the
    // clothes dialog has one type section per garment family, all sharing one
    // exclusive group, so a type button can sit anywhere. Appending the leftover
    // words after it produced "blue swimsuit short sleeveless": the type word in
    // the middle then no longer identified the tag as a clothes tag, so the
    // editor never sorted it back and the wrong order stuck.
    // With a shared group, the words belong in front of its first section - that
    // is exactly in front of the type word.
    int otherWordsBefore = objectLayoutListSize-1;
    if(shared_group){
        for(int i=0;i<objectLayoutListSize;++i){
            if(objectLayoutList[i].second==shared_group){
                otherWordsBefore=i;
                break;
            }
        }
    }
    // Several sections may share one QButtonGroup (categories of the clothes
    // dialog), so a button must only contribute its text once.
    QSet<QAbstractButton*> emitted;
    for(int i=0;i<objectLayoutListSize;++i){
        if(i==otherWordsBefore&&!otherWords.isEmpty()){
            sendData+=otherWords;
        }
        auto&[layout,group] = objectLayoutList[i];
        if(group)
            for(QAbstractButton* b:group->buttons()){
                if(b->isChecked()&&!emitted.contains(b)){
                    emitted.insert(b);
                    sendData+=b->text()+' ';
                }
            }
        else
            sendData+=dynamic_cast<QLabel*>(layout->itemAt(0)->widget())->text()+' ';
    }
    if(!sendData.isEmpty())sendData.chop(1);
    emit stringSignal(sendData,ifadd);
    uncheckAllButtons();
}

void colorsWidget::input_and_show(std::shared_ptr<teTag> in_tag){
    if(in_tag){
        QList<teWord*>&words = in_tag->words;
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
        // Turns on the button with this text; true when one was found (or when a
        // label section claimed the word, which is how it was before).
        auto lightWord = [this](const QString& wordtext)->bool{
            for(auto&[layout,buttongroup]:objectLayoutList){
                if(buttongroup){
                    auto buttonlist = buttongroup->buttons();
                    for(QAbstractButton*btn:buttonlist)
                        if(btn->text()==wordtext){
                            btn->setChecked(true);
                            return true;
                        }
                }else{
                    int count=layout->count();
                    for(int i=0;i<count;++i){
                        if(qobject_cast<QLabel*>(layout->itemAt(i)->widget()))
                            return true;
                    }
                }
            }
            return false;
        };
        // The tag's last word is the variant selector: in the hair colour window
        // it is the "hair"/"eyes" button that renames "yellow" into "blonde".
        // Matching it first is what makes the renamed button light up for a
        // "blonde hair" tag - matching in tag order left "blonde" unrecognised
        // (it landed in otherWords) and the tag then came back as
        // "pink blonde hair". The remaining words are matched in their original
        // order so otherWords keeps the order of the tag.
        QVector<bool> matched(wordCount,false);
        if(wordCount>0)
            matched[wordCount-1]=lightWord(words.back()->text);
        for(int i=0;i<wordCount;++i)
            if(i!=wordCount-1&&!matched[i])
                matched[i]=lightWord(words[i]->text);
        for(int i=0;i<wordCount;++i)
            if(!matched[i])
                otherWords.append(*words[i]+' ');
    }
    showNearCursor();
}
QString liteWindowStyle = QStringLiteral(R"(
QWidget{
    background-color:rgb(27,27,27);
    color:white;
    font-size:12pt;
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
/* License 窗用的是只读 QTextEdit:它自己按调色板画底,不写这条会是白底白字。 */
QTextEdit{
background-color:rgb(27,27,27);
color:white;
border:1px solid rgb(64,64,64);
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
    font-size:10pt;
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
font-size:12pt;
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

void applyLiteWindowStyle(LiteWidget2* window)
{
    if (!window || !window->contentWidget())
        return;
    // 旧的整套窗口配色(深色底、白字、滚动条、按钮)搬到内容面板上。
    // `QWidget#content` 那条规则不会再命中 —— 面板叫 litewidget2Content,
    // 所以不会在 LiteWidget2 自己的轮廓里再套一圈紫色边框。
    window->contentWidget()->setStyleSheet(liteWindowStyle);
    window->setWindowIcon(QIcon(":/res/icon.ico"));
}

static void buttons_mutual_exclusion(QAbstractButton *btn, bool checked, QButtonGroup *btn_gop){
    if(checked){
        auto buttons = btn_gop->buttons();
        for(QAbstractButton* the_btn:buttons){
            if(btn!=the_btn)
                the_btn->setChecked(false);
        }
    }
}

teInputWidget::teInputWidget(QWidget *parent):LiteWidget2(parent){
    applyLiteWindowStyle(this);
    inputLayout = new QVBoxLayout(contentWidget());
    plainTextEdit = new QPlainTextEdit;
    inputLayout->setAlignment(Qt::AlignRight);
    QPushButton* Ok_btn = new QPushButton;
    Ok_btn->setStyleSheet(OkButtonStyle);
    connect(Ok_btn,&QPushButton::pressed,this,&teInputWidget::onOkClicked,Qt::DirectConnection);
    Ok_btn->setFixedSize(60,40);
    inputLayout->setContentsMargins(3,3,3,3);
    inputLayout->addWidget(plainTextEdit);
    inputLayout->addWidget(Ok_btn,Qt::AlignHCenter);
    connect(this,&LiteWidget2::closed,this,[this]{
        emit cancelSignal();
    },Qt::DirectConnection);
    resize(400,300);
}

void teInputWidget::onOkClicked()
{
    hide();
    emit stringSignal(plainTextEdit->toPlainText(), false);
    plainTextEdit->clear();
}

void teFilteredPictureModel::clear() {
    beginResetModel();
    picturefiles.clear();
    endResetModel();
    emit clearAllFiles();
}

void teFilteredPictureModel::setdata(QList<QModelIndex> &&indexes){
    beginResetModel();
    picturefiles = std::move(indexes);
    endResetModel();
}

void teFilteredPictureModel::append(const QList<QModelIndex> &files) {
    if (files.isEmpty()) return;
    beginInsertRows(QModelIndex(), picturefiles.size(), picturefiles.size() + files.size() - 1);
    picturefiles.append(files);
    endInsertRows();
}

int teFilteredPictureModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) return 0;
    return picturefiles.size();
}

QVariant teFilteredPictureModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= picturefiles.size())
        return QVariant();
    if (role == Qt::DisplayRole) {
        return listview->model()->data(picturefiles[index.row()]);
    }
    return QVariant();
}

FilterTagWidget::FilterTagWidget(QWidget* parent) : QWidget(parent) {
    m_layout = new QFlowLayout(this);
    m_layout->setContentsMargins(2, 2, 2, 2);
    m_reorderer = new QFlowLayoutReorderer(m_layout, this, this);
    connect(m_reorderer,&QFlowLayoutReorderer::reordered,this,&FilterTagWidget::orderChanged);
}

QStringList FilterTagWidget::tagTexts() const {
    QStringList texts;
    for (int i = 0; i < m_layout->count(); ++i) {
        if (auto* button = qobject_cast<QPushButton*>(m_layout->itemAt(i)->widget()))
            texts << button->text();
    }
    return texts;
}

void FilterTagWidget::addTag(const QString &text) {
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty())
        return;
    QPushButton* tagItem = new QPushButton(trimmed, this);
    extern QString tagItemStyle;
    tagItem->setStyleSheet(tagItemStyle);
    // A plain click removes the chip; a drag reorders it. The reorderer eats the
    // release event of a drag, so `clicked` is only emitted for real clicks.
    connect(tagItem, &QPushButton::clicked, this, [this, tagItem]{
        emit tagEdit(tagItem->text());
        tagItems.removeAll(tagItem);
        m_layout->removeWidget(tagItem);
        tagItem->deleteLater();
        emit orderChanged();
    },Qt::DirectConnection);
    tagItems.push_back(tagItem);
    if(lineedit)
        m_layout->insertWidget(-1,tagItem);
    else
        m_layout->addWidget(tagItem);
    m_reorderer->attach(tagItem);
}

void filterWidget::initializeUI() {
    applyLiteWindowStyle(this);
    QVBoxLayout* mainLayout = new QVBoxLayout(contentWidget());

    const QStringList rules = {"Must include", "At least one", "Does not include", "Includes only one"};
    for(int i = 0; i < 4; ++i) {
        QHBoxLayout* ruleLayout = new QHBoxLayout;

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
    myview->setMinimumHeight(360);
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
    if(lastSelected.indexes().empty()&&lastDeselected.indexes().empty())
        return;

    auto* filteredModel = static_cast<teFilteredPictureModel*>(myview->model());
    const QModelIndexList selected = myview->selectionModel()->selectedIndexes();

    QItemSelection newSelection;
    QModelIndex firstSource;
    for(const QModelIndex& idx : selected){
        if(idx.row()<0||idx.row()>=filteredModel->picturefiles.size())
            continue;
        // picturefiles already holds the index of the *main* model. Asking the
        // proxy for its data() returns the tePictureFile pointer instead, so the
        // old qvariant_cast<QModelIndex> produced an invalid index and the main
        // view was selected but never scrolled to.
        const QModelIndex sourceIndex = filteredModel->picturefiles[idx.row()];
        if(!sourceIndex.isValid())
            continue;
        if(!firstSource.isValid())
            firstSource = sourceIndex;
        newSelection.select(sourceIndex,sourceIndex);
    }

    if(newSelection.isEmpty())
        return;

    listview->selectionModel()->select(newSelection, QItemSelectionModel::ClearAndSelect);
    listview->selectionModel()->setCurrentIndex(firstSource, QItemSelectionModel::NoUpdate);
    listview->scrollTo(firstSource, QAbstractItemView::PositionAtCenter);
}

bool filterWidget::eventFilter(QObject *watched, QEvent *event){
    Q_UNUSED(watched);
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
    const QStringList all = tagWidgets[AllOf]->tagTexts();
    const QStringList any = tagWidgets[AnyOf]->tagTexts();
    const QStringList none = tagWidgets[NoneOf]->tagTexts();
    const QStringList exact = tagWidgets[ExactOne]->tagTexts();
    for(const QString& text:all) rule.a.push_back(text);
    for(const QString& text:any) rule.r.push_back(text);
    for(const QString& text:none) rule.n.push_back(text);
    for(const QString& text:exact) rule.x.push_back(text);
    mymodel.setdata(listview->filt(rule));
}

void customControlWidget::initializeUI() {
    applyLiteWindowStyle(this);
    resize(300,250);
    QVBoxLayout* mainLayout = new QVBoxLayout(contentWidget());
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
        // tagTexts() reflects the visual (dragged) order, which is the order the
        // custom tags are stored and rebuilt in.
        emit tagsUpdated(tagWidget->tagTexts());
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
