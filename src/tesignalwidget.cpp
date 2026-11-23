#include "tesignalwidget.h"

#include "secondarybutton.h"   // the menu buttons of a dialog (kind / capture)
#include "mainwindow.h"     // editTagDecorations(): the one decorations entry point
#include "testyle.h"        // the SecondaryButton colours, shared with the editor columns
#include "svgicon.h"        // the tinted Lucide icons of the three signal buttons

#include <QFileInfo>
#include <QIcon>

// 一组互斥按钮里只留一个勾选;colorsWidget 的构造函数里的 lambda 要用,
// 所以先声明(定义在文件后半段)。
static void buttons_mutual_exclusion(QAbstractButton* btn, bool checked, QButtonGroup* btn_gop);

QString buttonTagText(const QAbstractButton* button)
{
    if (!button)
        return QString();
    const QVariant tag = button->property("tag");
    return tag.isValid() ? tag.toString() : button->text();
}

QLabel* makeSectionHeading(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setProperty("type","sectionHeading");
    label->setStyleSheet(qsl("QLabel{color:#c8c8c8;font-size:12px;font-style:italic;border:none;background:transparent;}"));
    return label;
}

void addButtonsToGridLayout(QGridLayout *gridLayout, QButtonGroup *buttonGroup,
                            QStringList &stringList,
                            const QHash<QString,QString>* captions) {
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
        const QString caption = captions ? captions->value(str, str) : str;
        QPushButton *button = new QPushButton(caption);
        // Only a button whose caption is not its word needs to remember the word:
        // the rest keep writing their text, which some windows rename at runtime.
        if (caption != str)
            button->setProperty("tag", str);
        button->setCheckable(true);

        buttonGroup->addButton(button);
        // One section is one tag group: its buttons sit edge to edge on purpose
        // (they are alternatives of one thing). The separator belongs between
        // sections - see colorsWidget::addSectionToContent.
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
    // A delimiter between tag groups, never inside one. Only the top level layout
    // gets one: inside a column the sections are already separated by their own
    // captions, and a line there would cut the column in half.
    if(target==content_layout&&target->count()>0){
        target->addWidget(target->direction()==QBoxLayout::TopToBottom
                              ? makeRowSeparator(this) : makeColumnSeparator(this));
    }
    if(heading.isEmpty()){
        target->addLayout(sectionLayout);
        return;
    }
    // A caption is purely visual: it is wrapped together with its section so it
    // is never part of a composed tag.
    QVBoxLayout* wrapper = new QVBoxLayout;
    wrapper->setContentsMargins(0,2,0,0);
    wrapper->setSpacing(0);
    wrapper->addWidget(makeSectionHeading(heading,this));
    wrapper->addLayout(sectionLayout);
    target->addLayout(wrapper);
}

void colorsWidget::addBuiltSection(QGridLayout* grid,QButtonGroup* group,
                                   const QString& heading,int column)
{
    if(!grid)
        return;
    auto* box = qobject_cast<QBoxLayout*>(content_layout);
    if(!box||box->direction()!=QBoxLayout::LeftToRight)
        return;                     // only the column arrangement has somewhere to put it
    // Columns are opened on demand: the constructor only creates the ones its
    // section list asked for (the clothes dialog passes just the colour column
    // and adds the body areas afterwards).
    while(column>=columns.size()){
        auto* extra = new QVBoxLayout;
        extra->setContentsMargins(0,0,0,0);
        extra->setSpacing(1);
        columns.push_back(extra);
        box->addLayout(extra);
    }
    grid->setContentsMargins(0,0,0,0);
    grid->setSpacing(1);
    objectLayoutList.push_back({grid,group});
    addSectionToContent(grid,heading,column);
}

void colorsWidget::startNewTag()
{
    uncheckAllButtons();
    for(SecondaryButton* button : findChildren<SecondaryButton*>()){
        button->setPaletteMode(true);
        button->setCaptureFromTag(QString());
    }
    emit editingTagChanged(QString());
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
    // The composition is empty again: the next tag starts from nothing. The type
    // buttons are unlit by the caller (startNewTag/applyEditingTag broadcast the
    // new tag), so this only forgets the word.
    m_composedType.clear();
    // A striped/gradient phrase belongs to the tag that was just written too: the
    // next `+` must not silently reuse the colours of the previous one.
    if(multiColorActive()||!m_colorQueue.isEmpty())
        leaveMultiColor();
}

void colorsWidget::chooseComposedType(const QString& tagText, bool toggle)
{
    QString next = tagText;
    if (toggle && next == m_composedType)
        next.clear();
    m_composedType = next;
    // One garment has one type: a menu choice replaces the plain type buttons,
    // and a plain type button replaces the menu choice (see the shared group's
    // toggled handler in the constructor).
    if (!m_composedType.isEmpty() && shared_group) {
        for (QAbstractButton* button : shared_group->buttons()) {
            if (button->isChecked())
                button->setChecked(false);
        }
    }
    for (SecondaryButton* button : findChildren<SecondaryButton*>())
        button->setCaptureFromTag(m_composedType);
    emit editingTagChanged(m_composedType);
}

colorsWidget::colorsWidget(QVector<QPair<QStringList, bool> > &&objects, QWidget *parent, int colorListPos, std::optional<QStringList> extraButtons, QVector<QString> headings, QButtonGroup* sharedExclusiveGroup, bool verticalSections, QVector<int> sectionColumns, QHash<QString,QString> buttonCaptions):LiteWidget2(parent){
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
            addButtonsToGridLayout((QGridLayout*)object_layout,object_buttongroup,stringlist,&buttonCaptions);
        }
        else{
            object_layout=new QVBoxLayout;
            for(QString&str:stringlist){
                const QString caption = buttonCaptions.value(str,str);
                QPushButton*objbtn = new QPushButton(caption,this);
                if(caption!=str)
                    objbtn->setProperty("tag",str);
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
        // A plain type button and a menu type are the same choice: ticking one
        // must drop the other, or `+` would write two garments into one tag.
        connect(sharedExclusiveGroup,&QButtonGroup::buttonToggled,this,[this](QAbstractButton*btn,bool checked){
            Q_UNUSED(btn);
            if(!checked||m_composedType.isEmpty())
                return;
            m_composedType.clear();
            for(SecondaryButton* button:findChildren<SecondaryButton*>())
                button->setCaptureFromTag(QString());
            emit editingTagChanged(QString());
        },Qt::DirectConnection);
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
        // A swatch of the queue is a plain square of the colour, so the dialog
        // has to know what each word looks like. Same table as the buttons.
        struct ColorValue{const char* word;int r,g,b;};
        for(const ColorValue& value:QVector<ColorValue>{
                {"red",255,0,0},{"orange",255,126,0},{"brown",96,48,0},{"yellow",255,255,0},
                {"green",0,255,0},{"blue",0,0,255},{"pink",255,0,255},{"purple",126,0,255},
                {"aqua",0,255,255},{"white",255,255,255},{"black",0,0,0},{"grey",127,127,127}})
            m_colorValues.insert(QString::fromLatin1(value.word),
                                 QColor(value.r,value.g,value.b));
        // In striped/gradient mode a colour button is an "append" action, not a
        // state: the queue owns the colours and the button never stays lit.
        for(QAbstractButton* button:colors_buttongroup->buttons())
            connect(button,&QPushButton::clicked,this,[this,button]{
                onColorButtonClicked(buttonTagText(button));
            });
        colorColumn->insertLayout(colorListPos+1,&colors_layout);
        objectLayoutList.insert(colorListPos+1,{&colors_layout,colors_buttongroup});
        colors_buttongroup->setExclusive(false);
        connect(colors_buttongroup,&QButtonGroup::buttonToggled,this,[this](QAbstractButton*btn,bool checked){if(checked)buttons_mutual_exclusion(btn,checked,colors_buttongroup);},Qt::DirectConnection);

        // The colour queue of a striped/gradient phrase: a QFlowLayout row of
        // small swatches under the colour buttons. Dragging one reorders the
        // phrase, a click removes it (QFlowLayoutReorderer swallows the release
        // of a drag, so a drag never turns into a removal).
        m_colorListPos = colorListPos;
        m_colorQueueHost = new QWidget(this);
        m_colorQueueHost->setToolTip(QStringLiteral(
            "Colours of the pattern: click a colour to append it, drag a swatch to "
            "reorder, click a swatch to remove it."));
        m_colorQueueLayout = new QFlowLayout(m_colorQueueHost,0,3,3);
        m_colorQueueReorderer = new QFlowLayoutReorderer(m_colorQueueLayout,m_colorQueueHost,this);
        connect(m_colorQueueReorderer,&QFlowLayoutReorderer::reordered,this,[this]{
            QStringList order;
            for(int i=0;i<m_colorQueueLayout->count();++i)
                if(QWidget* swatch=m_colorQueueLayout->itemAt(i)->widget())
                    order.append(swatch->property("colorWord").toString());
            m_colorQueue=order;
        });
        m_colorQueueHost->setVisible(false);
        colorColumn->insertWidget(colorListPos+2,m_colorQueueHost);
        m_shadeButtons=shade_buttongroup->buttons();
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
            const QString caption = buttonCaptions.value(str,str);
            QPushButton*objbtn = new QPushButton(caption,this);
            if(caption!=str)
                objbtn->setProperty("tag",str);
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
    // 对勾/加号绿、x 红,带外发光(旧 bin/tick/plus PNG 已删除)。
    applySignalButtonIcons(Ok_btn,add_btn,destroy_btn);

    layout->addLayout(content_layout);
    if(extra_layout)
        layout->addLayout(extra_layout);
    layout->addLayout(signal_button_layout);

    // striped / gradient are ordinary section buttons, and the list that says
    // which words they are arrives after construction (setMultiColorTriggers),
    // so every button is wired once and decides at click time. This runs last:
    // only now are the sections' buttons children of this widget (their layouts
    // were parented by the addLayout() calls above), so findChildren() sees them.
    for(QPushButton* button:findChildren<QPushButton*>()){
        if(qobject_cast<SecondaryButton*>(button))
            continue;
        connect(button,&QPushButton::toggled,this,[this,button](bool on){
            const QString word=buttonTagText(button);
            if(!m_multiColorTriggers.contains(word))
                return;
            if(on)
                enterMultiColor(word);
            else if(m_multiColorTrigger==word)
                leaveMultiColor();
        });
    }

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
                sendData=buttonTagText(b);
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
        // The colours of a striped/gradient phrase belong where a single colour
        // would have been: in front of the modifiers and the type word.
        if(i==m_colorListPos){
            const QString phrase=multiColorPhrase();
            if(!phrase.isEmpty())
                sendData+=phrase+' ';
        }
        if(i==otherWordsBefore){
            // The words no button stands for, then the type chosen in the palette
            // (`skirt`, `pleated skirt`, `school uniform`). Both go in front of the
            // type sections, which is where the tag's type word belongs.
            if(!otherWords.isEmpty())
                sendData+=otherWords;
            if(!m_composedType.isEmpty())
                sendData+=m_composedType+' ';
        }
        auto&[layout,group] = objectLayoutList[i];
        if(group)
            for(QAbstractButton* b:group->buttons()){
                if(b->isChecked()&&!emitted.contains(b)){
                    emitted.insert(b);
                    // The pattern word the phrase ends with is already written:
                    // its button stays lit only to keep the colour queue open.
                    // With an empty queue there is no phrase, so the word still
                    // has to come from the button.
                    if(multiColorActive()&&!m_colorQueue.isEmpty()
                        &&buttonTagText(b)==m_multiColorTrigger)
                        continue;
                    sendData+=buttonTagText(b)+' ';
                }
            }
        else
            sendData+=dynamic_cast<QLabel*>(layout->itemAt(0)->widget())->text()+' ';
    }
    if(!sendData.isEmpty())sendData.chop(1);
    // Nothing chosen: `+` must not insert an empty tag (the empty row used to be
    // how a stray click on the add button blanked the list).
    if(sendData.isEmpty()){
        uncheckAllButtons();
        return;
    }
    emit stringSignal(sendData,ifadd);
    uncheckAllButtons();
    // The composition belongs to the tag that was just written. A palette that
    // kept its old type lit would add the same tag again on the next `+`.
    for(SecondaryButton* button:findChildren<SecondaryButton*>())
        button->setCaptureFromTag(QString());
    emit editingTagChanged(QString());
}

void colorsWidget::input_and_show(std::shared_ptr<teTag> in_tag)
{
    applyEditingTag(in_tag);
    showNearCursor();
}

void colorsWidget::applyEditingTag(std::shared_ptr<teTag> in_tag){
    // The dialog edits *one* tag: whatever was ticked for the previous one (or
    // for a new tag that was never composed) goes away first, otherwise a stale
    // "skirt" would stay lit while a pair of gloves is being edited.
    uncheckAllButtons();
    const QString edited = in_tag ? static_cast<QString>(*in_tag) : QString();
    if(in_tag){
        QStringList words;
        words.reserve(in_tag->words.size());
        for(teWord* word : in_tag->words)
            words.append(word->text);
        int wordCount = words.count();
        if(extra_buttongroup){
            auto extrabuttonlist = extra_buttongroup->buttons();
            QString tagText = edited;
            for(QAbstractButton*btn:extrabuttonlist)
                if(buttonTagText(btn)==tagText){
                    btn->setChecked(true);
                    for(SecondaryButton* button : findChildren<SecondaryButton*>()){
                        button->setPaletteMode(true);
                        button->setCaptureFromTag(edited);
                    }
                    emit editingTagChanged(edited);
                    show();
                    return;
                }
        }
        // A striped/gradient phrase first: it owns its colour words, so they must
        // not also light a single colour button (and "and"/"to" are no leftovers).
        QStringList phraseColors;
        QString phraseTrigger;
        const bool phrase = takeMultiColorPhrase(words,phraseColors,phraseTrigger);
        wordCount = words.count();
        // The type is the *suffix* of the tag, and it can be longer than its last
        // word: `pleated skirt` / `sailor collar` are menu entries of the skirt /
        // collar button, `school uniform` is one type word. The longest suffix
        // that some type button answers for is the one to keep; its words are not
        // "leftovers" and must not be appended to otherWords a second time.
        QString composedType;
        QAbstractButton* plainType = nullptr;
        for(SecondaryButton* button : findChildren<SecondaryButton*>()){
            const QString candidate = button->paletteTagFor(edited);
            if(candidate.size() > composedType.size())
                composedType = candidate;
        }
        if(shared_group){
            for(QAbstractButton* button : shared_group->buttons()){
                const QString candidate = buttonTagText(button);
                if(candidate.isEmpty())
                    continue;
                const bool suffix = edited == candidate
                                    || edited.endsWith(QLatin1Char(' ') + candidate);
                if(!suffix || candidate.size() < composedType.size())
                    continue;
                composedType = candidate;
                plainType = button;
            }
        }
        QVector<bool> typeWords(wordCount,false);
        if(!composedType.isEmpty()){
            const int typeWordCount = composedType.split(QLatin1Char(' '),Qt::SkipEmptyParts).size();
            for(int i=0;i<typeWordCount&&i<wordCount;++i)
                typeWords[wordCount-1-i]=true;
            if(plainType)
                plainType->setChecked(true);
            else
                m_composedType=composedType;
        }
        // Turns on the button with this text; true when one was found (or when a
        // label section claimed the word, which is how it was before).
        auto lightWord = [this](const QString& wordtext)->bool{
            for(auto&[layout,buttongroup]:objectLayoutList){
                if(buttongroup){
                    auto buttonlist = buttongroup->buttons();
                    for(QAbstractButton*btn:buttonlist)
                        if(buttonTagText(btn)==wordtext){
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
        for(int i=0;i<wordCount;++i)
            matched[i]=typeWords[i];
        if(wordCount>0&&!typeWords[wordCount-1])
            matched[wordCount-1]=lightWord(words.back());
        for(int i=0;i<wordCount;++i)
            if(i!=wordCount-1&&!matched[i])
                matched[i]=lightWord(words[i]);
        for(int i=0;i<wordCount;++i)
            if(!matched[i])
                otherWords.append(words[i]+' ');
        if(phrase){
            // Light the trigger through its own button - the tick is what puts the
            // colour column into queue mode - and then hand it the colours.
            for(QAbstractButton* button:findChildren<QAbstractButton*>())
                if(buttonTagText(button)==phraseTrigger){
                    button->setChecked(true);
                    break;
                }
            // A tag that carried both pattern words (or a leftover one) must not
            // leave the pair lit together.
            uncheckOtherTriggers(phraseTrigger);
            setColorQueue(phraseColors);
        }
    }
    // The buttons of this dialog are its children (clothesMenuButton makes them
    // with the dialog as parent), so the dialog can light them itself. Depending on
    // a signal connection elsewhere is what left them unlit.
    for(SecondaryButton* button : findChildren<SecondaryButton*>()){
        button->setPaletteMode(true);
        button->setCaptureFromTag(edited);
    }
    emit editingTagChanged(edited);
}
QString colorsWidget::multiColorPhrase() const
{
    if(m_multiColorTrigger.isEmpty()||m_colorQueue.isEmpty())
        return QString();
    // "blue and white striped" / "white to blue gradient": the colours, their
    // connector and the pattern word *as one run*. The trigger used to be left to
    // its own button, which sits behind the other modifiers in the column - so
    // "blue and white" + frilled + striped came out as
    // "blue and white frilled striped", and once the clothes editor sorted that
    // the phrase was torn in half. sendString() skips the trigger's button now
    // that the phrase carries it.
    return m_colorQueue.join(m_multiColorTrigger==qsl("gradient")?qsl(" to ")
                                                               :qsl(" and "))
           +QLatin1Char(' ')+m_multiColorTrigger;
}

void colorsWidget::setColorQueue(const QStringList& colors)
{
    m_colorQueue=colors;
    refreshColorQueue();
}

void colorsWidget::clearColorQueue()
{
    m_colorQueue.clear();
    refreshColorQueue();
}

void colorsWidget::clampToScreen()
{
    QScreen* screen=this->screen();
    if(!screen)
        screen=QGuiApplication::primaryScreen();
    if(!screen)
        return;
    const QRect available=screen->availableGeometry();
    QRect geometry=this->geometry();
    geometry.setWidth(qMin(geometry.width(),available.width()));
    geometry.setHeight(qMin(geometry.height(),available.height()));
    geometry.moveLeft(qBound(available.left(),geometry.left(),
                             qMax(available.left(),available.right()-geometry.width()+1)));
    geometry.moveTop(qBound(available.top(),geometry.top(),
                            qMax(available.top(),available.bottom()-geometry.height()+1)));
    setGeometry(geometry);
}

void colorsWidget::onColorButtonClicked(const QString& word)
{
    if(!multiColorActive()||word.isEmpty())
        return;
    // The queue owns the colours: a click appends, it does not select.
    if(!m_colorQueue.contains(word))
        m_colorQueue.append(word);
    for(QAbstractButton* button:colors_buttongroup->buttons())
        if(buttonTagText(button)==word)
            button->setChecked(false);
    refreshColorQueue();
    // One more swatch may be one more row: the window has to grow, and it is
    // anchored at the cursor, so it is pushed back onto the screen afterwards.
    adjustSize();
    clampToScreen();
}

void colorsWidget::refreshColorQueue()
{
    if(!m_colorQueueLayout)
        return;
    // Rebuilt from the list: a handful of squares is cheaper to recreate than to
    // keep in step with removals and drags.
    while(QLayoutItem* item=m_colorQueueLayout->takeAt(0)){
        if(QWidget* widget=item->widget()){
            widget->hide();
            widget->deleteLater();
        }
        delete item;
    }
    for(const QString& color:m_colorQueue){
        auto* swatch=new QPushButton(m_colorQueueHost);
        swatch->setProperty("colorWord",color);
        swatch->setToolTip(color+QStringLiteral(" - click to remove"));
        swatch->setFixedSize(20,20);
        swatch->setCursor(Qt::PointingHandCursor);
        // A plain square of the colour; its name is in the tooltip, not the label.
        // "light blue" is stored with its shade, so the swatch paints the colour
        // word itself.
        const QString base=color.section(QLatin1Char(' '),-1);
        const QColor value=m_colorValues.value(base,QColor(90,90,90));
        swatch->setStyleSheet(QStringLiteral(
            "QPushButton{background-color:rgb(%1,%2,%3);border:1px solid #b0b0b0;"
            "border-radius:3px;padding:0px;}"
            "QPushButton:hover{border:1px solid #00e07a;}")
            .arg(value.red()).arg(value.green()).arg(value.blue()));
        connect(swatch,&QPushButton::clicked,this,[this,swatch]{
            const int index=m_colorQueue.indexOf(swatch->property("colorWord").toString());
            if(index<0)
                return;
            m_colorQueue.removeAt(index);
            refreshColorQueue();
            adjustSize();
            clampToScreen();
        });
        m_colorQueueLayout->addWidget(swatch);
        if(m_colorQueueReorderer)
            m_colorQueueReorderer->attach(swatch);
    }
    m_colorQueueHost->setVisible(multiColorActive());
    // One swatch may be one more row: the window has to grow with the queue.
    if(multiColorActive()){
        adjustSize();
        clampToScreen();
    }
}

void colorsWidget::uncheckOtherTriggers(const QString& keep)
{
    // `striped` and `gradient` are the one pair of modifiers here that exclude
    // each other: the composed phrase differs only in its connector
    // ("blue and white striped" vs "white to blue gradient"), so both lit at once
    // would leave the tag undecided.
    for(QAbstractButton* button:findChildren<QAbstractButton*>()){
        const QString word=buttonTagText(button);
        if(button->isChecked()&&word!=keep&&m_multiColorTriggers.contains(word))
            button->setChecked(false);
    }
}

void colorsWidget::enterMultiColor(const QString& trigger)
{
    if(m_multiColorTrigger==trigger)
        return;
    // Switching between striped and gradient keeps the picked colours: they came
    // first, the pattern is what the user is still deciding.
    const bool switching=multiColorActive();
    m_multiColorTrigger=trigger;
    uncheckOtherTriggers(trigger);
    // "light"/"dark"/"deep" prefix a single colour; a queue of colours has no slot
    // for them, so they step aside while the queue is in use.
    for(QAbstractButton* button:m_shadeButtons){
        button->setChecked(false);
        button->setEnabled(false);
    }
    if(!switching){
        // A colour picked before the pattern was ticked becomes the first colour
        // of the queue - colours first, pattern after, is the order one works in.
        m_colorQueue.clear();
        for(QAbstractButton* button:colors_buttongroup->buttons())
            if(button->isChecked())
                m_colorQueue.append(buttonTagText(button));
    }
    for(QAbstractButton* button:colors_buttongroup->buttons())
        button->setChecked(false);
    refreshColorQueue();
    adjustSize();
    clampToScreen();
}

void colorsWidget::leaveMultiColor()
{
    if(m_multiColorTrigger.isEmpty()&&m_colorQueue.isEmpty())
        return;
    m_multiColorTrigger.clear();
    m_colorQueue.clear();
    for(QAbstractButton* button:m_shadeButtons)
        button->setEnabled(true);
    refreshColorQueue();
    adjustSize();
    clampToScreen();
}

bool colorsWidget::takeMultiColorPhrase(QStringList& words,QStringList& colors,
                                        QString& trigger) const
{
    if(m_multiColorTriggers.isEmpty()||words.isEmpty())
        return false;
    int triggerIndex=-1;
    for(int i=0;i<words.size();++i)
        if(m_multiColorTriggers.contains(words[i])){
            triggerIndex=i;
            trigger=words[i];
            break;
        }
    if(triggerIndex<0)
        return false;
    // Everything in front of the trigger has to be colours and their connectors:
    // "blue and white striped" / "white to blue gradient". Any other word means
    // the tag is not a colour phrase ("blue torn striped ...").
    QStringList picked;
    QString pendingShade;
    for(int i=0;i<triggerIndex;++i){
        const QString& word=words[i];
        if(m_colorValues.contains(word)){
            picked.append(pendingShade.isEmpty()?word:pendingShade+' '+word);
            pendingShade.clear();
            continue;
        }
        if((word==qsl("and")||word==qsl("to"))&&!picked.isEmpty())
            continue;
        if((word==qsl("light")||word==qsl("dark")||word==qsl("deep"))
            &&pendingShade.isEmpty()){
            pendingShade=word;
            continue;
        }
        return false;
    }
    if(picked.isEmpty())
        return false;
    // The phrase words leave the list: the trigger is carried by its own button
    // (which is lit for the mode), the connectors are not words of the tag.
    words.erase(words.begin(),words.begin()+triggerIndex+1);
    colors=picked;
    return true;
}

QString liteWindowStyle = QStringLiteral(R"(
QWidget{
    background-color:rgb(3,3,3);
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
background-color:rgba(255,0,0,30);
border-color:rgb(150,0,0);
}
QPushButton:hover{
background-color:rgba(255,0,0,120);
border-color:rgb(255,0,0);
}
QPushButton:hover:pressed{
background-color:rgba(255,0,0,150);
}
)");


QString OkButtonStyle = qsl(R"(
QPushButton{
background-color:rgba(25,235,130,30);
border-color:#50bc86;
}
QPushButton:hover{
background-color:rgba(25,235,130,120);
border-color:#54eda1;
}
QPushButton:hover:pressed{
background-color:rgba(25,235,130,150);
}
)");
QString addButtonStyle = qsl(R"(
QPushButton{
background-color:rgba(25,235,130,30);
border-color:#50bc86;
}
QPushButton:hover{
background-color:rgba(25,235,130,120);
border-color:#54eda1;
}
QPushButton:hover:pressed{
background-color:rgba(25,235,130,150);
}
)");

void applySignalButtonIcons(QPushButton* ok,QPushButton* add,QPushButton* destroy){
    // 图标 20px,光晕外扩 3*2px → 画布 32px;iconSize 跟着画布走,别让 Qt 缩。
    const QSize iconSize(20,20);
    const QSize slotSize(32,32);
    if(ok){
        ok->setIcon(svgicon::icon(":/icons/check.svg",QColor(25,235,130),iconSize,2.0));
        ok->setIconSize(slotSize);
    }
    if(add){
        add->setIcon(svgicon::icon(":/icons/plus.svg",QColor(25,235,130),iconSize,2.0));
        add->setIconSize(slotSize);
    }
    if(destroy){
        destroy->setIcon(svgicon::icon(":/icons/x.svg",QColor(255,64,64),iconSize,2.0));
        destroy->setIconSize(slotSize);
    }
}
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

static void applyLiteWindowStyleImpl(QWidget* window, QWidget* content)
{
    if (!window || !content)
        return;
    // 旧的整套窗口配色(深色底、白字、滚动条、按钮)搬到内容面板上。
    // `QWidget#content` 那条规则不会再命中 —— 面板不叫 content,
    // 所以不会在窗口自己的轮廓里再套一圈紫色边框。
    // 二级按钮的配色单独一份(liteWindowStyle 是通用 QPushButton 规则,没有
    // captured/kind 这几态):没有它,add widget 里的基按钮点选后不会亮。
    content->setStyleSheet(liteWindowStyle + teStyle::sheet(QStringLiteral("secondarybutton")));
    window->setWindowIcon(QIcon(":/res/icon.ico"));
}

void applyLiteWindowStyle(LiteWidget2* window)
{
    if (window)
        applyLiteWindowStyleImpl(window, window->contentWidget());
}

void applyLiteWindowStyle(LiteWindow* window)
{
    if (window)
        applyLiteWindowStyleImpl(window, window->contentWidget());
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
            texts << chipTag(button);
    }
    return texts;
}

QString FilterTagWidget::chipTag(const QPushButton* button)
{
    if(!button)
        return QString();
    // The chip's label may carry an icon; the tag is what it was created with.
    const QVariant stored=button->property("tagText");
    return stored.isValid()?stored.toString():button->text();
}

void FilterTagWidget::relabel(QPushButton* button)
{
    if(!button)
        return;
    const QString tag=chipTag(button);
    const QString icon=m_iconProvider?m_iconProvider(tag).trimmed():QString();
    if(icon.isEmpty()){
        button->setIcon(QIcon());
        button->setText(tag);
        return;
    }
    // A file name (PNG/ICO under data/icons, or an absolute path) becomes the
    // button's icon; anything else is an emoji/character and goes into the text.
    TagIconSpec spec;
    spec.value=icon;
    if(spec.isImage()){
        const QString path=QFileInfo(icon).isAbsolute()
                               ?icon
                               :QCoreApplication::applicationDirPath()
                                    +QStringLiteral("/data/icons/")+icon;
        const QIcon image(path);
        if(!image.isNull()){
            button->setIcon(image);
            button->setIconSize(QSize(16,16));
            button->setText(tag);
            return;
        }
    }
    button->setIcon(QIcon());
    button->setText(icon+QStringLiteral(" ")+tag);
}

void FilterTagWidget::setIconProvider(std::function<QString(const QString&)> provider)
{
    m_iconProvider=std::move(provider);
    refreshIcons();
}

void FilterTagWidget::refreshIcons()
{
    for(QPushButton* button:tagItems)
        relabel(button);
}

void FilterTagWidget::addTag(const QString &text) {
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty())
        return;
    QPushButton* tagItem = new QPushButton(this);
    tagItem->setProperty("tagText",trimmed);
    extern QString tagItemStyle;
    tagItem->setStyleSheet(tagItemStyle);
    relabel(tagItem);
    // A plain click removes the chip; a drag reorders it. The reorderer eats the
    // release event of a drag, so `clicked` is only emitted for real clicks.
    connect(tagItem, &QPushButton::clicked, this, [this, tagItem]{
        emit tagEdit(chipTag(tagItem));
        tagItems.removeAll(tagItem);
        m_layout->removeWidget(tagItem);
        tagItem->deleteLater();
        emit orderChanged();
    },Qt::DirectConnection);
    if(m_iconEditing){
        // Right-click edits the icon instead of removing the chip. The menu is
        // only installed here: the filter rules share this widget and have no
        // icons to offer.
        tagItem->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(tagItem,&QWidget::customContextMenuRequested,this,[this,tagItem](const QPoint& pos){
            QMenu menu;
            QAction* editAction=menu.addAction(QStringLiteral("Set Icon..."));
            if(menu.exec(tagItem->mapToGlobal(pos))==editAction)
                emit iconEditRequested(chipTag(tagItem));
        });
    }
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
    applySignalButtonIcons(OK_Btn,nullptr,Bin_Btn);
    buttonLayout->addWidget(OK_Btn);
    buttonLayout->addWidget(Bin_Btn);
    mainLayout->addLayout(buttonLayout);
}

void customControlWidget::showEvent(QShowEvent* event)
{
    LiteWidget2::showEvent(event);
    tagWidget->refreshIcons();
}

void customControlWidget::connectSignals() {
    // MARK-09: chips in this window show the tag's decoration icon and offer to
    // edit it. The icon is *not* stored here any more: the decorations dialog is
    // the only place a tag's look is set, and this window just asks the registry
    // what that tag looks like right now.
    tagWidget->setIconEditingEnabled(true);
    tagWidget->setIconProvider([](const QString& tag){
        const TagVisual visual=TagMarkRegistry::instance().decorate(teTag(tag));
        return visual.icons.isEmpty()?QString():visual.icons.first().value;
    });
    connect(tagWidget,&FilterTagWidget::iconEditRequested,this,[this](const QString& tag){
        // Opens the decorations dialog on that tag; the dialog does the writing.
        if(editTagDecorations(this,tag))
            tagWidget->refreshIcons();
    });

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
