#include "mainwindow.h"
#include "teselectiontaglist.h"
#include "tepicturelistview.h"
#include "ui_mainwindow.h"
#include"tepicturefile.h"
#include"teeditor_derive.h"
#include"logwindow.h"

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
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // The log window is created first so that everything logged from here on
    // (including the tag list loading) is visible to the user.
    logWindow = new LogWindow(nullptr);
    teSetLogTarget([this](const QString& line){ if(logWindow) logWindow->append(line); });
    connect(ui->action_log_window,&QAction::triggered,this,&MainWindow::showLogWindow,Qt::DirectConnection);

    thread_pool.detach_task(load_tags);

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
            tePictureFile* tmpPictureFile = ui->taglist->file;
            QItemSelection tmpSelection= picturefileListView->selectionModel()->selection();
            if(tmpSelection.count()==1&&
                tmpPictureFile==qvariant_cast<tePictureFile*>(picturefileListView->model()->data(tmpSelection.indexes().first())))
                ui->taglist->loadFile(tmpPictureFile);
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
        QMessageBox::aboutQt(this, tr("About Qt"));
    });
    connect(ui->actionLicense,&QAction::triggered,this,[this]{
        QFile licenseFile(":/gpl-3.0.txt");
        licenseFile.open(QIODevice::ReadOnly);
        teWidget* licenseWidget = new teWidget;
        connect(licenseWidget->close_btn,&QPushButton::pressed,licenseWidget,&QWidget::deleteLater);
        QVBoxLayout* contentLayout = new QVBoxLayout;
        QScrollArea* textScrollArea = new QScrollArea;
        textScrollArea->setWidgetResizable(true);
        QVBoxLayout* textLayout = new QVBoxLayout;
        QWidget*centralWidget = new QWidget;
        textScrollArea->setWidget(centralWidget);
        textScrollArea->widget()->setLayout(textLayout);
        QTextEdit* textEdit = new QTextEdit();
        textEdit->setReadOnly(true);
        textEdit->setWordWrapMode(QTextOption::WordWrap);
        textEdit->setText(QString::fromUtf8(licenseFile.readAll()));
        contentLayout->addWidget(textEdit);
        licenseWidget->content->setLayout(contentLayout);
        licenseWidget->resize(400,500);
        licenseWidget->show();
    });
    connect(editorlistlayoutwidget,&EditorListLayoutWidget::cancelSignal,ui->editorlist,&teEditorList::setEditorsToPages);

    picturefileListView=ui->picturelist;
    picturefileModel = picturefileListView->fileModel();
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

    ui->taglist->editorlist = ui->editorlist;
    ui->editorlist->tagListWidget = ui->taglist;
    global_window=this;
    connect(ui->action_undo,&QAction::triggered,ui->taglist,&teTagListWidget::undo,Qt::DirectConnection);
    connect(ui->action_redo,&QAction::triggered,ui->taglist,&teTagListWidget::redo,Qt::DirectConnection);

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
        "font: 10pt bold \"Segoe UI\";"
        "color: qlineargradient(spread:pad, x1:0, y1:0, x1:0, y1:1,stop:0 #666666, stop:1 #000000);");

    titleLabel2->setPixmap(icon.scaled(20,20,Qt::KeepAspectRatio,Qt::SmoothTransformation));
    menulayout->addWidget(titleLabel2);//title
    menulayout->addWidget(titleLabel1);
    menulayout->addItem(menuspacer2);
    close_btn = new QPushButton(ui->menuBar);
    maximize_btn = new QPushButton(ui->menuBar);
    minimize_btn = new QPushButton(ui->menuBar);

    close_btn->setStyleSheet(QStringLiteral(R"(QPushButton{
border-image:url(:/res/close_mainwindow_sleep.png);border-top-right-radius:7px;}
QPushButton:hover{background-color:rgba(255,100,100,100);})"));
    maximize_btn->setStyleSheet(QStringLiteral(R"(QPushButton{
border-image:url(:/res/maximize_mainwindow_sleep.png);}
QPushButton:hover{background-color:rgba(100,100,100,100);})"));
    minimize_btn->setStyleSheet(QStringLiteral(R"(QPushButton{
border-image:url(:/res/minimize_mainwindow_sleep.png);}
QPushButton:hover{background-color:rgba(100,100,100,100);})"));

    connect(close_btn,&QPushButton::pressed,this, &MainWindow::onApplicationClose);
    connect(maximize_btn,&QPushButton::pressed,[this]{if(isMaximized())showNormal(); else showMaximized();});
    connect(minimize_btn,&QPushButton::pressed,this,&QMainWindow::showMinimized);
    menulayout->addWidget(minimize_btn);
    menulayout->addWidget(maximize_btn);
    menulayout->addWidget(close_btn);
    minimize_btn->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Expanding);
    maximize_btn->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Expanding);
    close_btn->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Expanding);

    QVector<teEditor*>allEditors{
        new teEditor_custom{ui->taglist,QStringLiteral("custom")}
        ,new teEditor_pretreat{ui->taglist,QStringLiteral("pretreat")}
        ,new teEditor_hair_and_eyes{ui->taglist,QStringLiteral("hair and eyes")}
        ,new teEditor_clothes{ui->taglist,QStringLiteral("clothes")}
        ,new teEditor_nsfw{ui->taglist,QStringLiteral("nsfw")}};
    extern int nsfwMode;
    if(nsfwMode<1)
        delete allEditors.takeLast();
    ui->editorlist->loadEditors(allEditors,&editorlistlayout);

    ui->editorlist->connectTaglistWidget(ui->taglist);
    widgetpool.initialize(ui->taglist,ui->taglist->layout);
    widgetpool_ref.initialize(ui->taglist,ui->editorlist->layout);

    ui->actionAuto_Merge_Tags->setChecked(autoMerge);

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
        ui->taglist->loadFile(selectedFiles.first());
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
            *result = 0;
            return true;
        }
        break;
    }
    case WM_NCHITTEST: {
        // 把 lParam (物理屏幕像素) -> Qt 逻辑坐标（和 Qt 的 geometry/控件坐标一致）
        POINT ptWin = { GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam) };
        double scale = getWindowScale(msg->hwnd); // e.g. 1.5, 2.0
        // 转换为 Qt 逻辑全局点
        QPointF qtGlobal(ptWin.x / scale, ptWin.y / scale);

        // 计算相对于窗口左上角的逻辑坐标（和 frameGeometry()/width()/height() 在同一坐标系）
        int xPos = static_cast<int>(qtGlobal.x()) - this->frameGeometry().x();
        int yPos = static_cast<int>(qtGlobal.y()) - this->frameGeometry().y();

        // 如果 boundaryWidth 是以逻辑像素定义（推荐这样），可以直接使用
        // 判断是否为标题栏区域（注意：你原来用的 last action x 等都是 Qt 逻辑坐标）
        QRect lastActionGeom = ui->menuBar->actionGeometry(ui->menuBar->actions().back());
        int lastActionRightX = lastActionGeom.x() + lastActionGeom.width();

        if (yPos > boundaryWidth && yPos < ui->menuBar->height()
            && xPos > lastActionRightX && xPos < minimize_btn->x())  // 标题栏区域
        {
            *result = HTCAPTION;
        }
        else if (xPos < boundaryWidth && yPos < boundaryWidth)
            *result = HTTOPLEFT;
        else if (xPos >= width() - boundaryWidth && yPos < boundaryWidth)
            *result = HTTOPRIGHT;
        else if (xPos < boundaryWidth && yPos >= height() - boundaryWidth)
            *result = HTBOTTOMLEFT;
        else if (xPos >= width() - boundaryWidth && yPos >= height() - boundaryWidth)
            *result = HTBOTTOMRIGHT;
        else if (xPos < boundaryWidth)
            *result = HTLEFT;
        else if (xPos >= width() - boundaryWidth)
            *result = HTRIGHT;
        else if (yPos < boundaryWidth)
            *result = HTTOP;
        else if (yPos >= height() - boundaryWidth)
            *result = HTBOTTOM;
        else
            return false;

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
                telog(QStringLiteral("[dataset] could not rename %1 to %2").arg(sourcePath,targetPath));
                continue;
            }

            if (hasTagFile && !QFile::copy(sourceTagPath,targetTagPath)) {
                // Roll the image rename back so the caption layout stays
                // consistent with what the user had before.
                const bool rolledBack = QFile::rename(targetPath,sourcePath);
                report.skipped++;
                report.details << QStringLiteral("  skipped %1: the caption file could not be copied").arg(fileName);
                telog(QStringLiteral("[dataset] could not copy the caption of %1; image rename rolled back: %2")
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
