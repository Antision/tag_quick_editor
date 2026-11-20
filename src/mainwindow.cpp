#include "mainwindow.h"
#include "litewidget2.h"
#include "windowglyphbutton.h"
#include "picturefilecaptionsource.h"
#include "tagviewhost.h"
#include "decorationeditorpage.h"
#include "teselectiontaglist.h"
#include "tepicturelistview.h"
#include "ui_mainwindow.h"
#include"tepicturefile.h"
#include"teeditor_derive.h"
#include "logwidget.h"
#include "windows_frame.h"
#include "task_runner.h"

#include <QPainterPath>
#include <QRegion>
#include <QResizeEvent>
#include <QScreen>
#include <QGuiApplication>

QWidget* global_window;
bool autoMerge=true;
bool MergeSwitch=true;
int nsfwMode=0;
int autoSaveSec=20;
QString defaultPath;
QStringList custom_tags;
teCustomControlList* custom_controls = nullptr;
editorListLayout editorlistlayout;
int mainWindowSplitterLength[MainWindowWidgetCount];
QRect mainwindowGeometry;

QPair<QList<tePictureFile*>, QList<tePictureFile*>> findDifferences(
    QList<tePictureFile*> lastfiles,
    QList<tePictureFile*> files)
{
    std::sort(lastfiles.begin(), lastfiles.end());
    std::sort(files.begin(), files.end());

    QList<tePictureFile*> onlyInLast;
    QList<tePictureFile*> onlyInFiles;

    auto itLast = lastfiles.begin();
    auto itFiles = files.begin();

    while (itLast != lastfiles.end() && itFiles != files.end()) {
        if (*itLast < *itFiles) {
            onlyInLast.append(*itLast);
            ++itLast;
        } else if (*itFiles < *itLast) {
            onlyInFiles.append(*itFiles);
            ++itFiles;
        } else {
            ++itLast;
            ++itFiles;
        }
    }

    while (itLast != lastfiles.end()) {
        onlyInLast.append(*itLast);
        ++itLast;
    }
    while (itFiles != files.end()) {
        onlyInFiles.append(*itFiles);
        ++itFiles;
    }

    return qMakePair(onlyInLast, onlyInFiles);
}
void applyAboutQtStyle(QMessageBox& box)
{
    // 对话框自己的样式表比祖先(MainWindow)的更近,能盖掉那条 QWidget{color:white}
    // 带来的"只有字没有底"的状态。
    box.setStyleSheet(QStringLiteral(
        "QMessageBox{background-color:rgb(27,27,27);}"
        "QMessageBox QLabel{color:white;background:transparent;}"
        "QMessageBox QTextEdit{background-color:rgb(35,35,35);color:white;border:none;}"
        "QMessageBox QPushButton{color:white;border:1px solid #4c399e;"
        "background:transparent;padding:4px 18px;border-radius:3px;}"
        "QMessageBox QPushButton:hover{border-color:#a858f2;"
        "background:rgba(177,100,255,60);}"));
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // The log window is created first so that everything logged from here on
    // (including the tag list loading) is visible to the user.
    // The log window is the shared one from common/logwidget (moved out of
    // gallery_info): coloured levels, a "something went wrong" light, sounds and
    // the habit of showing itself again for a Warning/Error. It is owned by this
    // window, so closing the editor closes it too.
    logWindow = new LogWidget(nullptr);
    logWindow->setTitle(QStringLiteral("Log - tag quick editor"));
    logWindow->setWindowIcon(QIcon(":/res/icon.ico"));
    teSetLogTarget([this](const QString& line, teLogLevel level){
        if (!logWindow)
            return;
        switch (level) {
        case teLogLevel::Detail:  logWindow->append(line, LogWidget::Level::Detail); break;
        case teLogLevel::Good:    logWindow->append(line, LogWidget::Level::Good); break;
        case teLogLevel::Warning: logWindow->append(line, LogWidget::Level::Warning); break;
        case teLogLevel::Error:   logWindow->append(line, LogWidget::Level::Error); break;
        case teLogLevel::Info:
        default:                  logWindow->append(line, LogWidget::Level::Info); break;
        }
    });
    attachLogWindow();
    connect(ui->action_log_window,&QAction::triggered,this,&MainWindow::showLogWindow,Qt::DirectConnection);

    teDetachTask("tags-load", load_tags);

    setWindowFlags(Qt::WindowMinimizeButtonHint|Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);

    HWND hwnd = reinterpret_cast<HWND>(winId());
    DWORD style = GetWindowLong(hwnd, GWL_STYLE);
    SetWindowLongPtr(hwnd, GWL_STYLE, style | WS_MAXIMIZEBOX | WS_THICKFRAME | WS_CAPTION);

    connect(ui->actionAuto_Merge_Tags,&QAction::toggled,this,[](bool state){autoMerge=state;});

    // Auto save runs on a GUI timer instead of a std::thread. The old worker
    // read the tag lists while the GUI thread was editing them, which is what
    // made "close the list while a state save is running" crash.
    autoSaveTimer = new QTimer(this);
    autoSaveTimer->setInterval(std::max(1,autoSaveSec)*1000);
    connect(autoSaveTimer,&QTimer::timeout,this,&MainWindow::autoSaveTick);
    connect(ui->actionAuto_Save_State,&QAction::toggled,this,[this](bool state){
        autoSaveSec = state ? 20 : 0;
        if(state){
            autoSaveTimer->setInterval(autoSaveSec*1000);
            autoSaveTimer->start();
        }else{
            autoSaveTimer->stop();
        }
    });
    if(autoSaveSec>0){
        autoSaveTimer->setInterval(autoSaveSec*1000);
        autoSaveTimer->start();
    }

    setWindowIcon(QIcon(":/res/icon.ico"));
    if(mainwindowGeometry.width()>0)
        this->setGeometry(mainwindowGeometry);
    splitter = new QSplitter(Qt::Horizontal);
    splitter->setStyleSheet("background-color:transparent");
    splitter->setAttribute(Qt::WA_TranslucentBackground);
    splitter->addWidget(ui->picturelist);
    splitter->addWidget(ui->tagListTabWidget);
    splitter->addWidget(ui->editorlist);
    if(mainWindowSplitterLength[0]>0){
        QList<int>sizes;
        for(int i=0;i<MainWindowWidgetCount;++i){
            sizes.push_back(mainWindowSplitterLength[i]);
        }
        splitter->setSizes(sizes);
    }
    else{
        int totalWidth = this->width();
        splitter->setSizes({totalWidth/7*1,totalWidth/7*2,static_cast<int>(totalWidth/7*(2+editorlistlayout.editors.size()*2))});
    }
    centralWidget()->layout()->addWidget(splitter);
    multitaglist = new teSelectionTagListView;
    multitaglistmodel=multitaglist->model;

    connect(ui->GlobalMultiTaglistView->model,&teSelectionTagModel::listModified,this,[this]{
        connect(ui->tagListTabWidget,&QTabWidget::currentChanged,ui->taglist,[this]{
            const QString shownImage = ui->taglist->captionImagePath();
            QItemSelection tmpSelection= picturefileListView->selectionModel()->selection();
            if(tmpSelection.count()==1){
                tePictureFile* tmpPictureFile = qvariant_cast<tePictureFile*>(
                    picturefileListView->model()->data(tmpSelection.indexes().first()));
                if(tmpPictureFile&&shownImage==tmpPictureFile->filepath.qstring)
                    ui->taglist->loadCaption(shownImage);
            }
            disconnect(ui->tagListTabWidget,&QTabWidget::currentChanged,ui->taglist,0);
        },Qt::SingleShotConnection);
    },Qt::DirectConnection);

    ui->selectList_tab_layout->addWidget(multitaglist);
    ui->tagListTabWidget->setContentsMargins(0,0,0,0);
    ui->selectList_tab_layout->setContentsMargins(0,0,0,0);
    ui->allList_tab_layout->setContentsMargins(0,0,0,0);
    multitaglist->hide();
    connect(ui->action_open, &QAction::triggered,this, [&]{this->dialog_LoadPath(true);});
    connect(ui->action_add, &QAction::triggered,this, [&]{this->dialog_LoadPath(false);});
    connect(ui->action_close, &QAction::triggered,this, [&]{
        ui->editorlist->unloadList();this->picturefileModel->clear();
    });
    editorlistlayoutwidget = new EditorListLayoutWidget(&editorlistlayout);
    connect(ui->action_editor_setting,&QAction::triggered,this,[this]{
        editorlistlayoutwidget->load();
    });
    connect(ui->actionAbout_Qt,&QAction::triggered,this,[this]{
        // 不用 QMessageBox::aboutQt():它是静态的,套不上单独的样式,而主窗口的
        // `QWidget{color:white}` 会让它变成白底白字。
        QMessageBox box(this);
        box.setWindowTitle(tr("About Qt"));
        box.setWindowIcon(windowIcon());
        box.setIconPixmap(QApplication::style()->standardIcon(QStyle::SP_MessageBoxInformation).pixmap(64, 64));
        box.setTextFormat(Qt::RichText);
        box.setText(tr("<h3>About Qt</h3><p>This program uses Qt version %1.</p>")
                        .arg(QString::fromLatin1(qVersion())));
        box.setInformativeText(tr(
            "<p>Qt is a C++ toolkit for cross-platform application development.</p>"
            "<p>Qt is available under the GNU Lesser General Public License v3 and a "
            "commercial license; this program is distributed under the GNU General "
            "Public License v3.0.</p>"
            "<p>Copyright (C) The Qt Company Ltd and other contributors.<br>"
            "Qt and the Qt logo are trademarks of The Qt Company Ltd.</p>"
            "<p><a href=\"https://www.qt.io/licensing/\">qt.io/licensing</a></p>"));
        applyAboutQtStyle(box);
        box.exec();
    });
    connect(ui->actionLicense,&QAction::triggered,this,[this]{
        auto* licenseWidget = new LiteWindow;
        licenseWidget->setWindowTitle(tr("License"));
        // 以前这里没套窗口配色:QTextEdit 用调色板画底,和全白的字叠在一起就成了
        // 白底白字。套上以后底是深灰、字是白的。
        applyLiteWindowStyle(licenseWidget);
        connect(licenseWidget,&LiteWindow::closed,licenseWidget,&QWidget::deleteLater);
        QScrollArea* textScrollArea = new QScrollArea;
        textScrollArea->setWidgetResizable(true);
        QWidget*centralWidget = new QWidget;
        textScrollArea->setWidget(centralWidget);
        QVBoxLayout* textLayout = new QVBoxLayout(centralWidget);
        // 程序本体与内嵌字体的许可证。字体经 QFontDatabase 编译进 exe 即为再分发,
        // UFL 要求许可文本随附,所以和 GPL 一起打包在 licenses.qrc 里展示。
        const struct{const char* heading;const char* resource;} licenseSections[] = {
            {"Tag Quick Editor - GNU General Public License v3.0", ":/gpl-3.0.txt"},
            {"Embedded font \"Ubuntu\" (Light/Regular/Bold) - Ubuntu Font Licence 1.0", ":/UFL.txt"},
        };
        for (const auto& section : licenseSections) {
            QLabel* headingLabel = new QLabel(QString::fromUtf8(section.heading));
            QFont headingFont = headingLabel->font();
            headingFont.setBold(true);
            headingLabel->setFont(headingFont);
            headingLabel->setWordWrap(true);
            textLayout->addWidget(headingLabel);
            QFile licenseFile(QString::fromUtf8(section.resource));
            if (licenseFile.open(QIODevice::ReadOnly)) {
                QTextEdit* textEdit = new QTextEdit();
                textEdit->setReadOnly(true);
                textEdit->setWordWrapMode(QTextOption::WordWrap);
                textEdit->setText(QString::fromUtf8(licenseFile.readAll()));
                textLayout->addWidget(textEdit);
            }
        }
        textLayout->addStretch();
        licenseWidget->contentLayout()->addWidget(textScrollArea);
        licenseWidget->resize(400,500);
        licenseWidget->show();
    });
    connect(editorlistlayoutwidget,&EditorListLayoutWidget::cancelSignal,ui->editorlist,&teEditorList::setEditorsToPages);
    connect(editorlistlayoutwidget,&EditorListLayoutWidget::pagesChanged,ui->editorlist,&teEditorList::setEditorsToPages);

    picturefileListView=ui->picturelist;
    picturefileModel = picturefileListView->fileModel();
    captionSource = new PictureFileCaptionSource(picturefileModel);
    ui->taglist->setCaptionSource(captionSource);
    // The popup and the sentence window are tqe's; the view only knows the
    // interfaces (TV-03), which is what lets it live in common.
    ui->taglist->setViewHost(new TqeTagViewHost(ui->taglist));
    // The tag view is a child widget, so ~QWidget deletes it *after* this
    // destructor body; the source it points at has to go last. `destroyed` is
    // emitted once the children are gone.
    {
        PictureFileCaptionSource* source = captionSource;
        connect(this,&QObject::destroyed,this,[source]{ delete source; });
    }
    imageWidget = new teImageWidget{nullptr};
    imageWidget->hide();
    connect(picturefileListView, &QListView::doubleClicked, this, [this](const QModelIndex &index) {
        imageWidget->show();

        tePictureFile *file = picturefileModel->data(index, Qt::DisplayRole).value<tePictureFile*>();
        if (!file) return;

        imageWidget->setImage(file->filepath.qstring);

        QRect windowRect = this->geometry();
        int leftSpace = windowRect.left();
        int rightSpace = QGuiApplication::primaryScreen()->geometry().right() - windowRect.right();

        if (leftSpace < rightSpace) {
            imageWidget->setGeometry(
                windowRect.right(),
                windowRect.top(),
                QGuiApplication::primaryScreen()->geometry().right() - windowRect.right(),
                windowRect.height()
                );
        } else {
            imageWidget->setGeometry(0, windowRect.top(), windowRect.left(), windowRect.height());
        }
    }, Qt::DirectConnection);

    connect(imageWidget, &teImageWidget::prevImage,
            picturefileListView, &tePictureListView::selectPrevious, Qt::DirectConnection);

    connect(imageWidget, &teImageWidget::nextImage,
            picturefileListView, &tePictureListView::selectNext, Qt::DirectConnection);

    connect(ui->action_save, &QAction::triggered,
            this, &MainWindow::save, Qt::DirectConnection);

    connect(picturefileListView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, [this] {
                if (!imageWidget->isVisible())
                    return;
                const QModelIndexList rows = picturefileListView->selectionModel()->selectedRows();
                if (rows.size() != 1)
                    return;
                if (auto *file = rows.first().data(Qt::DisplayRole).value<tePictureFile*>())
                    imageWidget->setImage(file->filepath.qstring);
            }, Qt::DirectConnection);

    connect(picturefileModel,&tePictureFileModel::newFileLoaded,ui->GlobalMultiTaglistView->model,&teSelectionTagModel::loadFiles,Qt::DirectConnection);
    connect(picturefileModel,&tePictureFileModel::clearAllFiles,ui->GlobalMultiTaglistView->model,&teSelectionTagModel::clear,Qt::DirectConnection);

    // Removing files (recycle bin) has to go through the model so the rows
    // disappear and the objects are deleted exactly once.
    connect(picturefileListView,&tePictureListView::filesRemoved,
            picturefileModel,&tePictureFileModel::removeFiles,Qt::DirectConnection);

    filterWindow = new filterWidget(picturefileListView);
    connect(ui->action_filt,&QAction::triggered,this,[this]{
        filterWindow->show();
    },Qt::DirectConnection);

    // Selection changes are coalesced: clicking through the list quickly used to
    // start a tag list reload per click, and the reloads re-entered the widget
    // (processEvents) so that the file that finally got displayed was not the
    // file that was selected. Now exactly one reload runs per event loop pass
    // and it reads the *current* selection.
    selectionTimer = new QTimer(this);
    selectionTimer->setSingleShot(true);
    selectionTimer->setInterval(0);
    connect(selectionTimer,&QTimer::timeout,this,&MainWindow::applyPendingSelection);
    connect(picturefileListView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this,[this]{
                selectionTimer->start();
                ui->tagListTabWidget->setCurrentIndex(0);
            },Qt::DirectConnection);

    ui->editorlist->tagListWidget = ui->taglist;
    global_window=this;
    connect(ui->action_undo,&QAction::triggered,ui->taglist,&TagListView::undo,Qt::DirectConnection);
    connect(ui->action_redo,&QAction::triggered,ui->taglist,&TagListView::redo,Qt::DirectConnection);

    QHBoxLayout* menulayout = new QHBoxLayout(ui->menuBar);
    QSpacerItem *menuspacer1 = new QSpacerItem(0,0,QSizePolicy::Expanding,QSizePolicy::Fixed);
    QSpacerItem *menuspacer2 = new QSpacerItem(0,0,QSizePolicy::Expanding,QSizePolicy::Fixed);
    menulayout->setContentsMargins(0,0,0,0);
    menulayout->setSpacing(0);
    menulayout->addSpacing(50);
    menulayout->addItem(menuspacer1);
    QPixmap icon(":/res/icon.png");
    QLabel* titleLabel1 = new QLabel{"tag quick editor"};
    QLabel* titleLabel2 = new QLabel{};
    titleLabel1->setStyleSheet(
        "font-size:10pt;font-weight:bold;"
        "color: qlineargradient(spread:pad, x1:0, y1:0, x1:0, y1:1,stop:0 #666666, stop:1 #000000);");

    titleLabel2->setPixmap(icon.scaled(20,20,Qt::KeepAspectRatio,Qt::SmoothTransformation));
    menulayout->addWidget(titleLabel2);//title
    menulayout->addWidget(titleLabel1);
    menulayout->addItem(menuspacer2);
    close_btn = new WindowGlyphButton(WindowGlyphButton::Glyph::Close, ui->menuBar);
    maximize_btn = new WindowGlyphButton(WindowGlyphButton::Glyph::Maximize, ui->menuBar);
    minimize_btn = new WindowGlyphButton(WindowGlyphButton::Glyph::Minimize, ui->menuBar);

    connect(close_btn,&QPushButton::pressed,this, &MainWindow::onApplicationClose);
    connect(maximize_btn,&QPushButton::pressed,this,&MainWindow::toggleMaximize);

    connect(minimize_btn,&QPushButton::pressed,this,&QMainWindow::showMinimized);
    // 悬停高亮偶尔不画(见 eventFilter 里的说明),悬停状态的变化从这里接管。
    close_btn->installEventFilter(this);
    maximize_btn->installEventFilter(this);
    minimize_btn->installEventFilter(this);
    menulayout->addWidget(minimize_btn);
    menulayout->addWidget(maximize_btn);
    menulayout->addWidget(close_btn);
    for (WindowGlyphButton* button : {minimize_btn, maximize_btn, close_btn}) {
        button->setFixedWidth(30);
        button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    }
    updateWindowGlyphs();

    QVector<teEditor*>allEditors{
        new teEditor_custom{ui->taglist,QStringLiteral("custom")}
        ,new teEditor_pretreat{ui->taglist,QStringLiteral("pretreat")}
        ,new teEditor_hair_and_eyes{ui->taglist,QStringLiteral("hair and eyes")}
        ,new teEditor_clothes{ui->taglist,QStringLiteral("clothes")}
        ,new teEditor_nsfw{ui->taglist,QStringLiteral("nsfw")}
        // Last on purpose (MARK-07): it has no tag controls, it only explains
        // where the painting-time decoration rules come from.
        ,new DecorationEditorPage{ui->taglist,QStringLiteral("decorations")}
    };
    extern int nsfwMode;
    ui->editorlist->loadEditors(allEditors,&editorlistlayout);
    for(teEditor* editor:ui->editorlist->editorlist)
        if(auto* decorations=dynamic_cast<DecorationEditorPage*>(editor))
            decorations->setEditorList(ui->editorlist);

    // NSFW 编辑器始终构造出来,开关只决定它是否参与分栏与显示;
    // 这样切换时不用重建整个 editorlist(也就不会丢掉控件状态)
    ui->actionNsfw_Mode->setChecked(nsfwMode > 0);
    connect(ui->actionNsfw_Mode, &QAction::toggled, this, &MainWindow::onNsfwModeToggled);
    applyNsfwMode(nsfwMode > 0, false);

    ui->editorlist->connectTaglistWidget(ui->taglist);
    // The pool gets the view as its parking parent (the view is the widget; the
    // controller is a plain QObject now). It no longer needs a layout: pooled
    // widgets are parked on the parent until they are shown again.
    widgetpool.initialize(ui->taglist,nullptr);
    widgetpool_ref.initialize(ui->taglist,ui->editorlist->layout);

    ui->actionAuto_Merge_Tags->setChecked(autoMerge);

    // 伪标题栏(menubar)的圆角裁剪:见 updateMenuBarMask()
    ui->menuBar->installEventFilter(this);
    updateMenuBarMask();
}

void MainWindow::applyNsfwMode(bool on, bool rebuildLayout)
{
    nsfwMode = on ? 1 : 0;

    if (rebuildLayout) {
        // nsfw 单独占第二栏:它和常规 tag 编辑器不是同一套工作流,塞在第一栏里
        // 会把那一栏挤到横向溢出。
        auto& pages = editorlistlayout.editors;
        for (auto& page : pages)
            page.removeAll(QStringLiteral("nsfw"));
        if (on) {
            if (pages.isEmpty())
                pages.push_back({});
            while (pages.size() < 2)
                pages.push_back({});
            pages[1].push_back(QStringLiteral("nsfw"));
        }
        ui->editorlist->setEditorsToPages();
        save_config();
    }

    // 关闭时只隐藏、不摘出布局:setEditorsToPages() 只搬运"还在栏里"的编辑器,
    // 一旦把它 setParent(nullptr) 出去,再打开开关就找不回来了。
    for (teEditor* e : ui->editorlist->editorlist) {
        if (e->name == QStringLiteral("nsfw"))
            e->setVisible(on);
    }
}

void MainWindow::onNsfwModeToggled(bool on)
{
    applyNsfwMode(on, true);
}

// 窗口没有真正的非矩形形状(无 mask),menubar 的 border-radius 只作用于
// 它自己的背景绘制;菜单项选中高亮、关闭/最大化按钮的 hover 背景都是
// 矩形绘制,会溢出到 7px 圆角外面(Qt 的 QMenuBar::item 不支持 :first
// 伪状态,所以靠 QSS 给"第一个菜单项"单独加圆角的路走不通)。
// 这里给 menubar 本身装一个圆角 mask,把溢出部分物理裁掉,高亮自然
// 贴合圆角,且与菜单字体大小无关。
void MainWindow::paintEvent(QPaintEvent* event)
{
    QMainWindow::paintEvent(event);

    // 伪标题栏底下垫一层与 menubar 同色的底。
    // 本窗口是 WA_TranslucentBackground,Qt 不会替它刷背景(QSS 里的
    // background-color 在无系统背景的顶层窗口上也不生效),所以 menubar 与
    // 中央控件之间只要有 1 个设备像素没被覆盖,透出来的就是桌面壁纸——在
    // 125%/150% 这类分数缩放的屏幕上,1 逻辑像素的边框会被摊成 2 个设备像素,
    // 缝隙就更容易显形(100% 缩放的小屏上看不见就是这个原因)。
    if (!ui || !ui->menuBar)
        return;
    const int bottom = ui->menuBar->geometry().bottom() + 1;
    if (bottom <= 0)
        return;

    if (isMaximized()) {
        // 最大化时整条菜单栏都在屏幕里(顶边那点空白已经被内容内缩让掉了),这里
        // 只需要补菜单栏与中间控件之间那 1 个设备像素的缝。整条都重画会把菜单栏
        // 右半截盖掉:窗口放大后右侧先脏的是这条底,底一画,菜单栏那三个窗口按钮
        // 当帧就不再重画,屏幕上看着像按钮消失了(拖回来又能看见)。
        QPainter painter(this);
        painter.fillRect(QRect(0, bottom - 1, width(), 2), QColor(27, 27, 27));
        return;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(27, 27, 27));          // == QMenuBar 的 background-color
    const qreal radius = 7.0;                      // == QSS 的 border-top-*-radius
    QPainterPath strip;
    strip.moveTo(0, bottom);
    strip.lineTo(0, radius);
    strip.quadTo(0, 0, radius, 0);
    strip.lineTo(width() - radius, 0);
    strip.quadTo(width(), 0, width(), radius);
    strip.lineTo(width(), bottom);
    strip.closeSubpath();
    painter.drawPath(strip);
}

// 最大化只让 Windows 做:窗口的"最大化状态"必须由系统持有(IsZoomed)。只有那样,
//   1. 把标题栏从屏幕上边缘往下拖,窗口才会恢复成原来的大小;
//   2. 双击标题栏、拖到屏幕顶端吸附、点最大化按钮,三条路径行为完全一致;
//   3. 还原尺寸由系统记着,不用我们存。
//
// 这里绝不能自己 setGeometry 摆窗口。一旦在最大化状态下改了窗口矩形,系统就是
// "状态是最大化、矩形却没铺开":用户看到的正是窗口不放大、只贴着工作区左上角,
// 而 WM_NCCALCSIZE 又把客户区钉在工作区上,客户区比窗口还大,内容于是从左右/
// 上下各多出一条跑到屏幕外。这个坑踩过两次了,别再往这条路径上加 setGeometry。
void MainWindow::updateMaximizedContentInset()
{
    // 半透明窗口最大化后,窗口矩形 = 工作区 + 一圈看不见的边框(实测每边 9 物理像素,
    // 见 windows_frame.h)。客户区不能收窄(那会把 Qt 的画布钉死在旧尺寸),所以改成
    // 让内容自己往里让这么多:菜单栏和中间那个 splitter 一起缩,屏幕上看到的边界正好
    // 落在工作区上,一条也不越界。
    HWND hwnd = reinterpret_cast<HWND>(winId());
    const int overshoot = teMaximizedFrameOvershoot(hwnd);       // 物理像素
    const double scale = getWindowScale(hwnd);                   // 1.0 / 1.25 / 1.5
    const int inset = overshoot > 0 ? qRound(overshoot / scale) : 0;
    setContentsMargins(inset, inset, inset, inset);
    // 圆角是给"浮在桌面上"的窗口的:最大化时内容已经贴着屏幕,再按圆角削掉两个上角
    // 就会在屏幕角上露出桌面,所以这里顺带把菜单栏的遮罩清掉/恢复。
    updateMenuBarMask();
}

void MainWindow::toggleMaximize()
{
    HWND hwnd = reinterpret_cast<HWND>(winId());
    // 以系统状态为准:Qt 记的窗口状态可能落后(窗口被从最大化状态拖出来、或者上一次
    // 切换在中途被打断),信 Qt 就会出现"点一下没反应、必须点第二次"。
    const bool zoomed = ::IsZoomed(hwnd) != 0;

    if (zoomed) {
        showNormal();
        // Qt 没跟着还原(状态不同步时它会直接吞掉这次调用):直接让系统还原。
        if (::IsZoomed(hwnd))
            ::ShowWindow(hwnd, SW_RESTORE);
    } else {
        showMaximized();
        // 同上:系统没真的进入最大化就补一次,保证每次点击都翻转状态。
        if (!::IsZoomed(hwnd))
            ::ShowWindow(hwnd, SW_MAXIMIZE);
    }
    updateMaximizedContentInset();
    updateWindowGlyphs();
}

void MainWindow::updateWindowGlyphs()
{
    if (!maximize_btn)
        return;
    const bool zoomed = ::IsZoomed(reinterpret_cast<HWND>(winId())) != 0;
    maximize_btn->setGlyph(zoomed ? WindowGlyphButton::Glyph::Restore
                                  : WindowGlyphButton::Glyph::Maximize);
}

void MainWindow::changeEvent(QEvent* event)
{
    // 双击标题栏、拖到屏幕顶端吸附都会自己进入最大化:状态一变就要重算内容内缩。
    if (event->type() == QEvent::WindowStateChange)
        QTimer::singleShot(0, this, [this]{
            updateMaximizedContentInset();
            updateWindowGlyphs();
            // 整窗同步重画一次。最大化时窗口矩形被系统撑大、客户区却还是原来那一块
            // (半透明窗口的画布不跟着放大),右半截的子控件常常一次都没画上:菜单栏
            // 右边的三个窗口按钮会整块空白,看起来像按钮消失了。一次全量 repaint 把
            // 这块补齐,代价只有切换状态时的一帧。
            repaint();
        });
    QMainWindow::changeEvent(event);
}

void MainWindow::updateMenuBarMask()
{
    QMenuBar* mb = ui->menuBar;
    if (!mb)
        return;
    // 最大化时内容贴着屏幕边,上角的圆角会把桌面露出来一条,直接不要遮罩。
    if (isMaximized()) {
        if (!mb->mask().isEmpty()) {
            mb->clearMask();
            // 遮罩只管"画在哪",去掉遮罩不会让刚露出来的那块自动重画:菜单栏变宽后
            // 右半截(标题、三个窗口按钮)会一直是空的,看起来像按钮消失了。
            mb->update();
        }
        return;
    }
    const QRect r = mb->rect();
    if (r.isEmpty())
        return;

    // 圆角半径与 QSS 的 border-top-left/right-radius 一致。
    // 只按"整行"削掉两个上角:逐行算出该行中心落在圆弧之外的那段宽度,再用
    // 整数矩形从整块区域里减掉。这里刻意不用 QPainterPath 转 QRegion 的老写法:
    // 路径是浮点坐标,在 125%/150% 这类分数缩放的屏幕上会被舍入,连 menubar
    // 自己的最后一行也一起舍掉,底边就透出一条 1px 的桌面壁纸缝(100% 缩放的
    // 屏幕上不会出现,所以只有大屏上才看得见)。
    const int rad = 7;
    QRegion region(r);
    QRegion corners;
    for (int y = 0; y < rad && y < r.height(); ++y) {
        const double dy = rad - y - 0.5;                        // 行中心到圆心的纵向距离
        const double inner = double(rad) * rad - dy * dy;       // 该行在圆内的水平余量
        const double dx = inner <= 0.0 ? 0.0 : std::sqrt(inner);
        const int cut = static_cast<int>(std::ceil(double(rad) - dx));
        if (cut <= 0)
            continue;
        const int w = qMin(cut, r.width());
        corners += QRect(0, y, w, 1);
        corners += QRect(r.width() - w, y, w, 1);
    }
    region -= corners;
    if (mb->mask() != region) {
        mb->setMask(region);
        // 遮罩只决定"画在哪",改了就得重画一遍:菜单栏变宽时,新露出来的那一截
        // 不属于任何一次无效区域,不 update 就一直空着(窗口按钮看着像消失了)。
        mb->update();
    }
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == ui->menuBar && event->type() == QEvent::Resize)
        updateMenuBarMask();
    // 三个窗口按钮的悬停高亮(QSS :hover)偶尔画不出来:半透明窗口上,子控件的小块
    // 异步重绘会丢(最大化切换之后尤其明显,点击功能却一直正常,因为命中测试走的是
    // 另一条路)。悬停一进一出就同步重画一次按钮,把丢掉的帧补上;重画一个 40x26 的
    // 按钮,代价可以忽略。
    if (watched == close_btn || watched == maximize_btn || watched == minimize_btn) {
        const auto type = event->type();
        if (type == QEvent::HoverEnter || type == QEvent::HoverLeave
            || type == QEvent::Enter || type == QEvent::Leave) {
            // 延一拍:等悬停状态真正生效后再画,repaint() 是同步刷屏,不会被丢。
            if (auto* btn = qobject_cast<QPushButton*>(watched))
                QTimer::singleShot(0, btn, [btn]{ btn->repaint(); });
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

QList<tePictureFile*> MainWindow::selectedPictureFiles() const
{
    QList<tePictureFile*> files;
    const QModelIndexList rows = picturefileListView->selectionModel()->selectedRows();
    files.reserve(rows.size());
    for (const QModelIndex& index : rows) {
        if (auto* file = index.data(Qt::DisplayRole).value<tePictureFile*>())
            files.append(file);
    }
    return files;
}

void MainWindow::applyPendingSelection()
{
    emitloadlists(QItemSelection(), QItemSelection());
}

void MainWindow::autoSaveTick()
{
    if (autoSaveSec <= 0)
        return;
    saveState(true);
}

void MainWindow::showLogWindow()
{
    if (!logWindow)
        return;
    logWindow->show();
    logWindow->raise();
    logWindow->activateWindow();
}

void MainWindow::attachLogWindow()
{
    if (!logWindow)
        return;
    // An owned window: it is a real top level window (so it is never clipped by
    // this frameless, translucent main window), but it stays above its owner and
    // disappears together with it.
    logWindow->setParent(this);
    logWindow->setWindowFlags(logWindow->windowFlags() | Qt::Window);
}

void MainWindow::emitloadlists(const QItemSelection &selected, const QItemSelection &deselected){
    Q_UNUSED(selected);
    Q_UNUSED(deselected);

    const QList<tePictureFile*> selectedFiles = selectedPictureFiles();
    const int selectcount = selectedFiles.size();

    if (selectcount == 1) {
        ui->editorlist->unloadList();
        if(multitaglist->isVisible()){
            ui->taglist->show();
            multitaglistmodel->clear();
            multitaglist->hide();
        }
        shownMultiTagFiles.clear();
        // The list is a QListView now; its own scrollbar is the one to reset.
        ui->taglist->scrollToTop();
        ui->taglist->loadCaption(selectedFiles.first()->filepath.qstring);
        return;
    }

    if (selectcount == 0) {
        multitaglistmodel->clear();
        shownMultiTagFiles.clear();
        ui->taglist->clear();
        ui->editorlist->unloadList();
        return;
    }

    if(ui->taglist->isVisible()){
        multitaglist->show();
        ui->taglist->hide();
        ui->editorlist->unloadList();
    }

    // Incremental update when only a few files entered/left the selection,
    // full reload otherwise.
    const QSet<tePictureFile*> oldSet(shownMultiTagFiles.begin(),shownMultiTagFiles.end());
    const QSet<tePictureFile*> newSet(selectedFiles.begin(),selectedFiles.end());
    QList<tePictureFile*> removed,added;
    for(tePictureFile* f:shownMultiTagFiles)
        if(!newSet.contains(f))
            removed.append(f);
    for(tePictureFile* f:selectedFiles)
        if(!oldSet.contains(f))
            added.append(f);

    if(!shownMultiTagFiles.isEmpty()
        && (removed.size()+added.size())*2 <= shownMultiTagFiles.size()+selectedFiles.size()){
        if(!removed.isEmpty())
            multitaglistmodel->eraseFiles(removed);
        if(!added.isEmpty())
            multitaglistmodel->loadFiles(added,false);
    }else{
        multitaglistmodel->clear();
        multitaglistmodel->loadFiles(selectedFiles,false);
    }
    shownMultiTagFiles = selectedFiles;
}
void MainWindow::cancelPendingWork()
{
    // Every decode checks this before and after the image reader, so the pool
    // drains within one image instead of one whole folder. Without it the shutdown
    // had to wait for the folder (or, before, forever).
    if (picturefileModel)
        for (tePictureFile* file : picturefileModel->picturefiles)
            file->cancelLoading();
}

MainWindow::~MainWindow()
{
    // Stop every timer first: none of them may fire while the widgets are gone.
    if (autoSaveTimer)
        autoSaveTimer->stop();
    if (selectionTimer)
        selectionTimer->stop();

    // Detach the log target before the window that receives the lines dies.
    teSetLogTarget({});

    save_config();
    delete filterWindow;
    delete imageWidget;
    delete logWindow;
    delete ui;
    // captionSource is deleted from the `destroyed` handler (see the constructor):
    // the view that points at it is a child widget and dies after this body.
    // Nothing may read the window through this any more: save_config() uses it for
    // the splitter sizes and the geometry, and running it after the window is gone
    // was an access violation on freed memory (the release build reported it as an
    // unhandled win32 exception while closing).
    global_window = nullptr;
}

void MainWindow::save(){
    picturefileListView->setProperty("saving",true);
    picturefileListView->style()->unpolish(picturefileListView);
    picturefileListView->style()->polish(picturefileListView);
    QApplication::processEvents();
    picturefileModel->save();
    save_config();
    QTimer::singleShot(100, [this]() {
        picturefileListView->setProperty("saving",false);
        picturefileListView->style()->unpolish(picturefileListView);
        picturefileListView->style()->polish(picturefileListView);
    });
}

int MainWindow::saveState(bool ifRunning)
{
    QJsonDocument doc;
    QJsonObject configObj;

    configObj.insert("running", ifRunning);
    auto &picturefiles = picturefileModel->picturefiles;

    QJsonArray picturesArr;

    // Runs on the GUI thread (autoSaveTimer / checkSave), so the tag lists are
    // read while nothing else can be mutating them.
    for (tePictureFile* file : picturefiles) {
        QJsonObject picture;
        auto &taglist = file->taglist;

        if (taglist.isSaved)
            continue;

        std::lock_guard<std::recursive_mutex> lg(taglist.tagsMt);

        // 新格式：整段文本
        picture.insert("filepath", file->filepath.qstring);
        picture.insert("tagsText", taglist.toText());

        // 兼容旧格式：保留数组
        QJsonArray tags;
        const int tagCount = int(taglist.size());
        for (int t = 0; t < tagCount; ++t) {
            teTag* tag = taglist.at(t);
            if (tag)
                tags.append(QString::fromStdString(joinTag(*tag)));
        }
        picture.insert("tags", tags);

        picturesArr.append(picture);
    }

    configObj.insert("pictures", picturesArr);
    doc.setObject(configObj);

    QFile f("./runtime_state.json");
    if (f.open(QFile::WriteOnly | QFile::Text)) {
        QTextStream stream(&f);
        stream.setEncoding(QStringConverter::Utf8);
        stream << doc.toJson();
        f.close();
        return 0;
    }
    return -1;
}

int MainWindow::loadState()
{
    QFile f("./runtime_state.json");
    if (!f.open(QFile::ReadOnly | QFile::Text)) {
        return -1;
    }

    QTextStream stream(&f);
    stream.setEncoding(QStringConverter::Utf8);
    QString str = stream.readAll();
    f.close();

    QJsonParseError jsonError;
    QJsonDocument doc = QJsonDocument::fromJson(str.toUtf8(), &jsonError);
    if (jsonError.error != QJsonParseError::NoError || doc.isNull())
        return -1;

    QJsonObject configObj = doc.object();
    QJsonArray picturesArr = configObj.value("pictures").toArray();

    if (configObj.value("running").toBool() && !picturesArr.isEmpty()) {
        QMessageBox msgBox;
        msgBox.setWindowTitle("");
        msgBox.setText("An unexpected exit was detected during your last session. Would you like to load the auto-saved states of your unsaved files?");
        msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        if (msgBox.exec() == QMessageBox::No) {
            return 0;
        }
    } else {
        return 0;
    }

    QList<tePictureFile*> appendPicturelist;
    if (picturesArr.isEmpty())
        return 0;

    for (QJsonValue pictureV : picturesArr) {
        QJsonObject picture = pictureV.toObject();
        tePictureFile* newpicture = new tePictureFile(picture.value("filepath").toString());

        newpicture->taglist.clear();

        // 新格式：整段文本
        QString tagsText = picture.value("tagsText").toString();
        if (!tagsText.isEmpty()) {
            const auto pieces = splitTextToPieces(tagsText);
            for (const auto& piece : pieces) {
                if (!piece.text.isEmpty()) {
                    newpicture->taglist.initialize_push_back(piece.text, piece.sentence);
                }
            }
        } else {
            // 旧格式：数组
            QJsonArray tags = picture.value("tags").toArray();
            for (QJsonValue tagstrV : tags) {
                QString tagstr = tagstrV.toString();
                newpicture->taglist.initialize_push_back(tagstr,false);
            }
        }

        newpicture->taglist.isSaved = false;
        appendPicturelist.push_back(newpicture);
    }

    ((tePictureFileModel*)ui->picturelist->model())->append(appendPicturelist);
    return 0;
}

extern QStringList custom_tags;


bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
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
            // 无边框窗口:整窗都是客户区。最大化时系统会把窗口矩形按看不见的边框
            // 往外撑 9px,这里**不能**把客户区收窄回来:本窗口是半透明的,一旦客户区
            // 小于窗口矩形,Qt 的画布就不再跟着放大(实测只画出最大化之前的尺寸,
            // 屏幕上一块界面 + 其余透明)。改成让内容自己往里让 9px,见
            // updateMaximizedContentInset()。
            *result = 0;
            return true;
        }
        break;
    }
    case WM_NCHITTEST: {
        // lParam 是物理屏幕像素,换成 Qt 逻辑坐标要除以这个窗口自己的 DPI 比例。
        const QPoint phys(GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam));
        const double scale = getWindowScale(msg->hwnd); // e.g. 1.25, 1.5, 2.0
        // 用 mapFromGlobal 而不是自己减 frameGeometry():最大化时窗口矩形被系统
        // 撑大了 9px(客户区才是工作区),减 frameGeometry() 会把命中区域整块挪偏。
        const QPoint local = mapFromGlobal(QPoint(qRound(phys.x() / scale),
                                                  qRound(phys.y() / scale)));
        const bool zoomed = IsZoomed(msg->hwnd) != 0;

        // 标题栏 = 菜单栏这条横带里"不属于菜单项、也不属于窗口按钮"的那一段。
        // 三个矩形统一用 mapTo(this) 换算到窗口坐标,不再假设菜单栏正好贴在
        // y=0..height():只要两者错开一点点(最大化、以后加边框、或者某个平台给的
        // 客户区原点不是 0),写死的 y 区间就会把标题栏搬到菜单栏下面去——表现正是
        // "按住菜单栏拖不动,按住菜单栏下面那一条反而能拖下来,窗口按钮也点不动"。
        const QRect barRect(ui->menuBar->mapTo(this, QPoint(0, 0)), ui->menuBar->size());
        const QRect minBtnRect(minimize_btn->mapTo(this, QPoint(0, 0)), minimize_btn->size());
        const QRect lastActionGeom = ui->menuBar->actionGeometry(ui->menuBar->actions().back());
        const int lastActionRightX = barRect.x() + lastActionGeom.x() + lastActionGeom.width();

        LRESULT hit = HTCLIENT;
        // 顶边那条带子先判:它是缩放边框,哪怕正落在标题栏(菜单栏那条)上。
        // 以前先判 HTCAPTION,菜单栏左侧那段空白就被当成纯标题栏 —— 在那儿
        // 按住只能拖窗口,拉不出上边框(菜单项和按钮因为不算标题栏才碰巧能拉)。
        const bool topEdge = !zoomed && local.y() < boundaryWidth;
        if (topEdge) {
            if (local.x() < boundaryWidth)          hit = HTTOPLEFT;
            else if (local.x() >= width() - boundaryWidth) hit = HTTOPRIGHT;
            else                                    hit = HTTOP;
        } else if (barRect.contains(local) && local.x() > lastActionRightX && local.x() < minBtnRect.left()) {
            // 最大化时也必须返回 HTCAPTION:系统就是靠"按住标题栏往下拖"把最大化
            // 的窗口还原的,返回别的值这条路就没了。
            hit = HTCAPTION;
        } else if (!zoomed) {
            // 最大化状态下窗口没有可拖的边框(和普通 Windows 窗口一致):那几像素
            // 是贴屏幕边缘的,拖它既不该改大小,也不该把窗口撕下来。
            const bool left = local.x() < boundaryWidth;
            const bool right = local.x() >= width() - boundaryWidth;
            const bool bottom = local.y() >= height() - boundaryWidth;
            if (bottom && left)      hit = HTBOTTOMLEFT;
            else if (bottom && right)hit = HTBOTTOMRIGHT;
            else if (left)           hit = HTLEFT;
            else if (right)          hit = HTRIGHT;
            else if (bottom)         hit = HTBOTTOM;
        }

        if (hit == HTCLIENT)
            return false;   // 交回给 Qt(它自己会给出客户区/子控件)
        *result = hit;
        return true;
    }
    default:
        break;
    }
    return QWidget::nativeEvent(eventType, message, result);
}
LRESULT MainWindow::OnTestBorder(const QPoint &pt)
{
    if (::IsZoomed((HWND)this->winId()))
    {
        return HTCLIENT;
    }
    int borderSize = 0;
    int cx = this->size().width();
    int cy = this->size().height();
    QRect rectTopLeft(0, 0, borderSize, borderSize);
    if (rectTopLeft.contains(pt))
    {
        return HTTOPLEFT;
    }
    QRect rectLeft(0, borderSize, borderSize, cy - borderSize * 2);
    if (rectLeft.contains(pt))
    {
        return HTLEFT;
    }
    QRect rectTopRight(cx - borderSize, 0, borderSize, borderSize);
    if (rectTopRight.contains(pt))
    {
        return HTTOPRIGHT;
    }
    QRect rectRight(cx - borderSize, borderSize, borderSize, cy - borderSize * 2);
    if (rectRight.contains(pt))
    {
        return HTRIGHT;
    }
    QRect rectTop(borderSize, 0, cx - borderSize * 2, borderSize);
    if (rectTop.contains(pt))
    {
        return HTTOP;
    }
    QRect rectBottomLeft(0, cy - borderSize, borderSize, borderSize);
    if (rectBottomLeft.contains(pt))
    {
        return HTBOTTOMLEFT;
    }
    QRect rectBottomRight(cx - borderSize, cy - borderSize, borderSize, borderSize);
    if (rectBottomRight.contains(pt))
    {
        return HTBOTTOMRIGHT;
    }
    QRect rectBottom(borderSize, cy - borderSize, cx - borderSize * 2, borderSize);
    if (rectBottom.contains(pt))
    {
        return HTBOTTOM;
    }
    return HTCLIENT;
}

int MainWindow::checkSave(){
    for(tePictureFile*file:picturefileModel->picturefiles){
        if(!file->taglist.isSaved)
            goto saveFlag;
    }
    return 1;
saveFlag:
    QMessageBox msgBox;
    msgBox.setWindowTitle("");
    msgBox.setText("Save Changes?");
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No|QMessageBox::Cancel);
    int ret = msgBox.exec();
    if (ret == QMessageBox::Cancel) {
        return 0;
    }else if(ret == QMessageBox::Yes){
        save();
        QJsonDocument doc;
        QJsonObject configObj;

        configObj.insert("running", false);
        doc.setObject(configObj);
        QFile f("./runtime_state.json");
        if (f.open(QFile::WriteOnly | QFile::Text)) {
            QTextStream stream(&f);
            stream.setEncoding(QStringConverter::Utf8);
            stream << doc.toJson();
            f.close();
        }
        saveState(false);
        return 1;
    }else{
        saveState(false);
    }
    return 1;
}
std::set<tePath>folder_paths;

QList<QPair<tePath,bool>> is_duplicate_path(const tePath&in){
    if(folder_paths.empty()) return {};
    std::set<tePath>::iterator l = folder_paths.lower_bound(in);

    if(l==folder_paths.end()) return {};
    else if(in==*l||in.isSubpath(*l)) return {{*l,true}};
    else if(l->isSubpath(in)){
        QList<QPair<tePath,bool>> r{{*l,false}};
        for(++l;l!=folder_paths.end();++l){
            if(l->isSubpath(in)){
                r.push_back({*l,false});
            }else return r;
        }return r;
    }
    // Neither an ancestor nor a descendant: nothing to filter.
    // (Falling off the end of this function used to be undefined behaviour.)
    return {};
}

namespace {

/// Win32's classic path limit, including the terminating null.
constexpr int kMaxPathLength = 259;

/**
 * @brief Builds `<base>_<number><suffix>` without exceeding the path limit.
 *
 * Only the base name is shortened (from its tail); the `_<number>` marker is
 * never trimmed away - trimming it would turn the candidate back into the name
 * of the file being renamed, which then looks "already taken" for every number.
 */
QString makeCandidateName(const QString& folder,const QString& base,int number,const QString& suffix)
{
    const QString tail = QStringLiteral("_%1").arg(number) + suffix;
    const int prefixLength = folder.size() + 1;         // folder + '/'
    QString trimmed = base;
    while (trimmed.size() > 1 && prefixLength + trimmed.size() + tail.size() > kMaxPathLength)
        trimmed.chop(1);            // trim the tail of the name, never the head
    return trimmed + tail;
}

/// Lower-cased base names of every entry in `folder`, whatever its extension.
///
/// Working with the *base* name (instead of the complete file name) is what
/// makes the result stable: renaming `b.png` to `b_1.png` while `b_1.jpg`
/// exists would just create a new duplicate pair, and the next scan would have
/// to rename again.
QSet<QString> folderBaseNames(const QString& folder)
{
    QSet<QString> bases;
    const QDir dir(folder);
    const QStringList entries = dir.entryList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);
    for (const QString& entry : entries)
        bases.insert(QFileInfo(entry).completeBaseName().toLower());
    return bases;
}

}

teDuplicateStemReport teResolveDuplicateStems(QVector<tePath>& imagePaths)
{
    teDuplicateStemReport report;

    // Group by (folder, base name). Windows file names are case insensitive.
    QHash<QString,QVector<int>> groups;
    for (int i = 0; i < imagePaths.size(); ++i) {
        const QFileInfo info(imagePaths[i].qstring);
        const QString key = info.absolutePath().toLower() + QLatin1Char('|')
                            + info.completeBaseName().toLower();
        groups[key].append(i);
    }

    QHash<QString,QSet<QString>> folderBases;

    for (auto it = groups.cbegin(); it != groups.cend(); ++it) {
        QVector<int> ids = it.value();
        if (ids.size() < 2)
            continue;

        // The first path keeps its name (and therefore the shared caption);
        // every later one is renamed.
        std::sort(ids.begin(),ids.end(),[&](int a,int b){
            return imagePaths[a].qstring < imagePaths[b].qstring;
        });

        const QString folder = QFileInfo(imagePaths[ids[0]].qstring).absolutePath();
        QSet<QString>& bases = folderBases[folder.toLower()];
        if (bases.isEmpty())
            bases = folderBaseNames(folder);

        const QString first = QFileInfo(imagePaths[ids[0]].qstring).fileName();
        telog(QStringLiteral("[dataset] %1 images share the name \"%2\" in %3")
                  .arg(ids.size()).arg(QFileInfo(first).completeBaseName(),folder));
        report.details << QStringLiteral("%1: %2 images with the same name")
                              .arg(folder, QString::number(ids.size()));

        for (int n = 1; n < ids.size(); ++n) {
            const int index = ids[n];
            const QFileInfo info(imagePaths[index].qstring);
            const QString fileName = info.fileName();
            const QString base = info.completeBaseName();
            const QString suffix = fileName.mid(base.size());   // includes the dot
            const QString sourcePath = imagePaths[index].qstring;

            if (!info.exists()) {
                report.skipped++;
                report.details << QStringLiteral("  skipped %1: the file does not exist").arg(fileName);
                telog(QStringLiteral("[dataset] skipped %1: the file does not exist").arg(sourcePath));
                continue;
            }

            // `<base>.txt` is the caption of *every* image called `<base>.*`, so
            // it belongs to the image that keeps the original name. It must never
            // be moved away from it - that would silently strip the tags of the
            // kept image, which is exactly the kind of loss this function exists
            // to prevent. The renamed image gets a copy instead.
            const QString sourceTagPath = folder + QLatin1Char('/') + base + QStringLiteral(".txt");
            const bool hasTagFile = QFileInfo::exists(sourceTagPath);

            // Find a free `<base>_<n>` name. The candidate base is checked
            // against the real folder contents, so
            //  - an unrelated file with that base (any extension) can never be
            //    overwritten, and
            //  - the result cannot contain a new duplicate pair either.
            QString targetName;
            QString targetPath;
            QString targetTagPath;
            QString targetBase;
            bool foundFreeName = false;
            for (int counter = 1; counter <= 9999; ++counter) {
                targetName = makeCandidateName(folder,base,counter,suffix);
                targetBase = targetName.left(targetName.size() - suffix.size());
                if (bases.contains(targetBase.toLower()))
                    continue;
                targetPath = folder + QLatin1Char('/') + targetName;
                targetTagPath = folder + QLatin1Char('/') + targetBase + QStringLiteral(".txt");
                foundFreeName = true;
                break;
            }

            if (!foundFreeName) {
                report.skipped++;
                report.details << QStringLiteral("  skipped %1: no free name could be built").arg(fileName);
                telog(QStringLiteral("[dataset] skipped %1: no free name could be built").arg(sourcePath));
                continue;
            }

            if (!QFile::rename(sourcePath,targetPath)) {
                report.skipped++;
                report.details << QStringLiteral("  skipped %1: the file could not be renamed").arg(fileName);
                tewarn(QStringLiteral("[dataset] could not rename %1 to %2").arg(sourcePath,targetPath));
                continue;
            }

            if (hasTagFile && !QFile::copy(sourceTagPath,targetTagPath)) {
                // Roll the image rename back so the caption layout stays
                // consistent with what the user had before.
                const bool rolledBack = QFile::rename(targetPath,sourcePath);
                report.skipped++;
                report.details << QStringLiteral("  skipped %1: the caption file could not be copied").arg(fileName);
                tewarn(QStringLiteral("[dataset] could not copy the caption of %1; image rename rolled back: %2")
                          .arg(sourcePath, rolledBack ? QStringLiteral("yes") : QStringLiteral("NO - check this file manually")));
                continue;
            }

            // The new base is taken from now on; the old one is never reused.
            bases.insert(targetBase.toLower());

            imagePaths[index].set(targetPath);
            report.renamed++;
            report.details << QStringLiteral("  renamed %1 -> %2").arg(fileName, targetName);
            telog(QStringLiteral("[dataset] renamed %1 to %2").arg(sourcePath,targetPath));
            if (hasTagFile)
                telog(QStringLiteral("[dataset] copied the shared caption %1 to %2")
                          .arg(sourceTagPath, targetTagPath));
        }
    }

    return report;
}

void MainWindow::dialog_LoadPath(bool clear){
    const QString folderPath = QFileDialog::getExistingDirectory(this, tr("Select folder"), defaultPath)+"/";

    if(folderPath.size()<2)return;
    try{
        const tePath rootpath(folderPath);
        QVector<tePath> paths;

        auto collectFrom = [&paths](const tePath& root){
            for(const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(root.stdpath)){
                if(!entry.is_directory()&&is_image_file(entry.path()))
                    paths.append(tePath(entry));
            }
        };

        if(clear){
            if(!checkSave())
                return;                     // the user cancelled the save prompt
            picturefileModel->clear();
            // A different dataset is being opened: this is the right moment to
            // give the recycled tag/word widgets back to the system.
            widgetpool.realloc();
            widgetpool_ref.realloc();
            folder_paths.clear();
            collectFrom(rootpath);
            folder_paths.insert(rootpath);
        }else{
            const QList<QPair<tePath,bool>> duplicate_list = is_duplicate_path(rootpath);
            if(duplicate_list.size()==1&&duplicate_list.front().second==true)
                return;
            else if(!duplicate_list.empty()){
                for (const auto& entry : std::filesystem::directory_iterator(rootpath.stdpath)) {
                    if (entry.is_directory()) {
                        const tePath currentPath(QString::fromStdString(entry.path().string())+'/');
                        bool shouldExclude = false;
                        for (const auto& excludedFolder : folder_paths) {
                            if (currentPath.isSubpath(excludedFolder)||currentPath==excludedFolder) {
                                shouldExclude = true;
                                break;
                            }
                        }
                        if (!shouldExclude)
                            collectFrom(currentPath);
                    }else if(is_image_file(entry.path())){
                        paths.append(tePath(entry));
                    }
                }
            }else{
                collectFrom(rootpath);
            }
            folder_paths.insert(rootpath);
        }

        // Two images with the same base name cannot share one caption file:
        // rename the later ones and tell the user what happened.
        const teDuplicateStemReport report = teResolveDuplicateStems(paths);
        if (!report.details.isEmpty()) {
            QString text = tr("Some images in the selected folder share the same name.\n"
                              "%1 were renamed, %2 were skipped.\n\nSee the log window for details.")
                               .arg(report.renamed).arg(report.skipped);
            QMessageBox::warning(this, tr("Duplicate image names"), text);
        }

        QList<tePictureFile*> appendPicturelist;
        appendPicturelist.reserve(paths.size());
        for (const tePath& path : paths)
            appendPicturelist.push_back(new tePictureFile(path));
        picturefileModel->append(appendPicturelist);
    }catch(const std::exception& e){
        telog(e.what());
        telog("[MainWindow::dialog_LoadPath]: exception occurred, maybe a path contains characters that the C++ runtime cannot handle");
    }
    defaultPath=folderPath;
    save_config();
}

bool is_image_file(const std::filesystem::path &file) {
    // Lower-cased: ".PNG" or ".JPG" must not silently disappear from the list.
    const std::string extension = file.extension().string();
    std::string lower;
    lower.reserve(extension.size());
    for (char c : extension)
        lower.push_back(char(std::tolower(static_cast<unsigned char>(c))));
    return lower == ".png" || lower == ".jpg"
           || lower == ".jpeg" || lower == ".bmp"
           || lower == ".gif" || lower == ".avif"
           || lower == ".webp" || lower == ".jfif"
           || lower == ".tif" || lower == ".tiff";
}

void MainWindow::on_sortByLexButton_clicked()
{
    // The multi tag list only *shows* the tags the selected pictures carry; a
    // multi tag holds links to the per picture tags, never copies of them. So
    // reordering its rows cannot reorder - or otherwise touch - the tag list of
    // any single picture.
    if(ui->GlobalMultiTaglistView->model)
        ui->GlobalMultiTaglistView->model->sortByText();
}


void MainWindow::on_sortByOccurrenceButton_clicked()
{
    // Most carried tag first; ties fall back to dictionary order. See
    // on_sortByLexButton_clicked() on why this stays a pure row reorder.
    if(ui->GlobalMultiTaglistView->model)
        ui->GlobalMultiTaglistView->model->sortByOccurrence();
}
