#include "mainwindow.h"
#include "temultitaglistview.h"
#include "tepicturelistview.h"
#include "ui_mainwindow.h"
#include"tepicturefile.h"
#include"teeditor_derive.h"
QWidget* global_window;
extern bool autoMerge;

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
    thread_pool.add(load_tags);

    setWindowFlags(Qt::WindowMinimizeButtonHint|Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);

    HWND hwnd = reinterpret_cast<HWND>(winId());
    DWORD style = GetWindowLong(hwnd, GWL_STYLE);
    SetWindowLongPtr(hwnd, GWL_STYLE, style | WS_MAXIMIZEBOX | WS_THICKFRAME | WS_CAPTION);

    connect(ui->actionAuto_Merge_Tags,&QAction::toggled,this,[](bool state){autoMerge=state;});
    connect(ui->actionAuto_Save_State,&QAction::toggled,this,[](bool state){extern int autoSaveSec;autoSaveSec=state?20:0;});

    setWindowIcon(QIcon(":/res/icon.ico"));
    if(mainwindowGeometry.width()>0)
        this->setGeometry(mainwindowGeometry);
    splitter = new QSplitter(Qt::Horizontal);
    splitter->setStyleSheet("background-color:transparent");
    splitter->setAttribute(Qt::WA_TranslucentBackground);
    splitter->addWidget(ui->picturelist);
    splitter->addWidget(ui->TaglistTabWidget);
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
    multitaglist = new teMultitagListView;
    multitaglistmodel=multitaglist->model;

    connect(ui->GlobalMultiTaglistView->model,&teMultiTagListModel::listModified,this,[this]{
        connect(ui->TaglistTabWidget,&QTabWidget::currentChanged,ui->taglist,[this]{
            tePictureFile* tmpPictureFile = ui->taglist->file;
            QItemSelection tmpSelection= picturefileListView->selectionModel()->selection();
            if(tmpSelection.count()==1&&
                tmpPictureFile==qvariant_cast<tePictureFile*>(picturefileListView->model()->data(tmpSelection.indexes().first())))
                ui->taglist->loadFile(tmpPictureFile);
            disconnect(ui->TaglistTabWidget,&QTabWidget::currentChanged,ui->taglist,0);
        },Qt::SingleShotConnection);
    },Qt::DirectConnection);

    ui->selectList_tab_layout->addWidget(multitaglist);
    ui->TaglistTabWidget->setContentsMargins(0,0,0,0);
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

    static bool called=false;
    static QList<tePictureFile*> lastfiles;
    picturefileListView=ui->picturelist;
    picturefileModel = dynamic_cast<tePictureFileModel*>(picturefileListView->model());
    imageWidget = new teImageWidget{nullptr};
    imageWidget->hide();
    connect(picturefileListView, &QListView::doubleClicked, this, [this](const QModelIndex&index){
        imageWidget->show();
        tePictureFile*file = qvariant_cast<tePictureFile*>(picturefileModel->data(index));

        imageWidget->imgArea->setImage(file->filepath.qstring);

        QRect windowRect = this->geometry();
        int leftSpace = windowRect.left();
        int rightSpace = QGuiApplication::primaryScreen()->geometry().right() - windowRect.right();
        if(leftSpace<rightSpace){
            imageWidget->setGeometry(windowRect.right(),windowRect.top(),QGuiApplication::primaryScreen()->geometry().right()-windowRect.right(),windowRect.height());
        }else{
            imageWidget->setGeometry(0,windowRect.top(),windowRect.left(),windowRect.height());
        }
        imageWidget->imgArea->adjustImageScaleToFit();
    },Qt::DirectConnection);
    connect(imageWidget,&teImageWidget::prevImage,picturefileListView,&tePictureListView::selectPrevious,Qt::DirectConnection);
    connect(imageWidget,&teImageWidget::nextImage,picturefileListView,&tePictureListView::selectNext,Qt::DirectConnection);
    connect(ui->action_save,&QAction::triggered,this,&MainWindow::save,Qt::DirectConnection);

    connect(picturefileListView->selectionModel(),&QItemSelectionModel::selectionChanged,this,[this](const QItemSelection &selected){
        if(imageWidget->isVisible()&&selected.size()==1)
        imageWidget->imgArea->setImage(qvariant_cast<tePictureFile*>(picturefileListView->model()->data(selected[0].indexes()[0]))->filepath.qstring);
    },Qt::DirectConnection);
    void newFileLoaded(QList<tePictureFile *> files);
    void clearAllFiles();
    connect(picturefileModel,&tePictureFileModel::newFileLoaded,ui->GlobalMultiTaglistView->model,&teMultiTagListModel::loadFiles,Qt::DirectConnection);
    connect(picturefileModel,&tePictureFileModel::clearAllFiles,ui->GlobalMultiTaglistView->model,&teMultiTagListModel::clear,Qt::DirectConnection);

    filterWindow = new filterWidget(picturefileListView);
    connect(ui->action_filt,&QAction::triggered,this,[this]{
        filterWindow->show();
    },Qt::DirectConnection);

    connect(picturefileListView->selectionModel(), &QItemSelectionModel::selectionChanged, this,&MainWindow::emitloadlists,Qt::DirectConnection);
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
void MainWindow::emitloadlists(const QItemSelection &selected, const QItemSelection &deselected){
    ui->TaglistTabWidget->setCurrentIndex(0);
    QList<tePictureFile *> selectedFiles;
    for (const auto &index : selected.indexes()) {
        auto *file = picturefileModel->data(index, Qt::DisplayRole).value<tePictureFile *>();
        if (file) selectedFiles.append(file);
    }
    QList<tePictureFile *> deSelectedFiles;
    for (const auto &index : deselected.indexes()) {
        auto *file = picturefileModel->data(index, Qt::DisplayRole).value<tePictureFile *>();
        if (file) deSelectedFiles.append(file);
    }
    int selectcount = picturefileListView->selectionModel()->selectedRows().size();
    if (selectcount==1) {
        ui->editorlist->unloadList();
        if(multitaglist->isVisible()){
            ui->taglist->show();
            multitaglistmodel->clear();
            multitaglist->hide();
        }
        ui->taglist->sc->verticalScrollBar()->setValue(0);
        ui->taglist->loadFile(picturefileListView->selectionModel()->selectedIndexes().first().data().value<tePictureFile *>());
    } else if(selectcount>1){
        if(ui->taglist->isVisible()){
            multitaglist->show();
            ui->taglist->hide();
            ui->editorlist->unloadList();
        }
        if((selected.size()+deselected.size())<std::min(multitaglistmodel->linked_taglists.size()/2,selectedFiles.size()/2)){
            if(!deselected.empty())
                multitaglistmodel->eraseFiles(deSelectedFiles);
            if(!selected.empty())
                multitaglistmodel->loadFiles(selectedFiles,false);
        }else{
            multitaglistmodel->clear();
            QList<tePictureFile *> allSelectedFiles;
            QModelIndexList indexlist = picturefileListView->selectionModel()->selectedIndexes();
            for(QModelIndex index:indexlist){
                allSelectedFiles.push_back(index.data().value<tePictureFile *>());
            }
            multitaglistmodel->loadFiles(allSelectedFiles,false);
        }
    }

}
MainWindow::~MainWindow()
{
    save_config();
    delete filterWindow;
    delete imageWidget;
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

int MainWindow::saveState(bool ifRunning){
    QJsonDocument doc;
    QJsonObject configObj;

    configObj.insert("running", ifRunning);
    auto&picturefiles = picturefileModel->picturefiles;

    QJsonArray picturesArr;
    QJsonObject pictures;
    {
        std::lock_guard<std::mutex>lg(picturefileModel->pictureListMutex);
        for(tePictureFile*file:picturefiles){
            QJsonObject picture;
            auto& taglist = file->taglist;
            QJsonArray tags;
            if(taglist.isSaved)
                continue;
            taglist.tagsMt.lock();
            int tagCount = taglist.size();
            for (int t = 0; t < tagCount; ++t) {
                teTagCore& tag = taglist[t];
                tags.append(QString::fromStdString(joinTag(tag)));
            }
            taglist.tagsMt.unlock();
            picture.insert("filepath",file->filepath.qstring);
            picture.insert("tags",tags);
            picturesArr.append(picture);
        }
    }
    configObj.insert("pictures",picturesArr);
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

int MainWindow::loadState(){
    QFile f("./runtime_state.json");
    if(!f.open(QFile::ReadOnly|QFile::Text)){
        return -1;
    }
    QTextStream stream(&f);
    stream.setEncoding(QStringConverter::Utf8);
    QString str = stream.readAll();
    f.close();
    QJsonParseError jsonError;
    QJsonDocument doc = QJsonDocument::fromJson(str.toUtf8(),&jsonError);
    if(jsonError.error!=QJsonParseError::NoError&&!doc.isNull())return -1;

    QJsonObject configObj = doc.object();
    QJsonArray picturesArr= configObj.value("pictures").toArray();
    if(configObj.value("running").toBool()&&!picturesArr.isEmpty()){
        QMessageBox msgBox;
        msgBox.setWindowTitle("");
        msgBox.setText("An unexpected exit was detected during your last session. Would you like to load the auto-saved states of your unsaved files?");
        msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        if (msgBox.exec() == QMessageBox::No) {
            return 0;
        }
    }else return 0;
    QList<tePictureFile*>appendPicturelist;
    if(picturesArr.isEmpty())return 0;
    for(QJsonValue pictureV:picturesArr){
        QJsonObject picture = pictureV.toObject();
        tePictureFile* newpicture = new tePictureFile(picture.value("filepath").toString());
        newpicture->taglist.clear();
        QJsonArray tags = picture.value("tags").toArray();
        for(QJsonValue tagstrV:tags){
            QString tagstr = tagstrV.toString();
            newpicture->taglist.initialize_push_back(tagstr);
        }
        newpicture->taglist.isSaved=false;
        appendPicturelist.push_back(newpicture);
    }
    ((tePictureFileModel*)ui->picturelist->model())->append(appendPicturelist);
}

extern QStringList custom_tags;
extern QMap<QString,teEditorControl*>* custom_controls;


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
    case WM_NCHITTEST:{
        int xPos = GET_X_LPARAM(msg->lParam) - this->frameGeometry().x();
        int yPos = GET_Y_LPARAM(msg->lParam) - this->frameGeometry().y();
        if (yPos > boundaryWidth&&yPos < ui->menuBar->height()&&xPos>ui->menuBar->actionGeometry(ui->menuBar->actions().back()).x()+(ui->menuBar->actionGeometry(ui->menuBar->actions().back()).width())&&xPos<minimize_btn->x())
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
    {
        std::lock_guard<std::mutex>lg(picturefileModel->pictureListMutex);
        for(tePictureFile*file:picturefileModel->picturefiles){
            if(!file->taglist.isSaved)
                goto saveFlag;
        }
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
std::set<tepath>folder_paths;

QList<QPair<tepath,bool>> is_duplicate_path(const tepath&in){
    if(folder_paths.empty()) return {};
    std::set<tepath>::iterator l = folder_paths.lower_bound(in);

    if(l==folder_paths.end()) return {};
    else if(in==*l||in.isSubpath(*l)) return {{*l,true}};
    else if(l->isSubpath(in)){
        QList<QPair<tepath,bool>> r{{*l,false}};
        for(++l;l!=folder_paths.end();++l){
            if(l->isSubpath(in)){
                r.push_back({*l,false});
            }else return r;
        }return r;
    }
}
void MainWindow::dialog_LoadPath(bool clear){
    extern QString defaultPath;
    QString folderPath = QFileDialog::getExistingDirectory(this, tr("Select folder"), defaultPath)+"/";

    if(folderPath.size()<2)return;
    try{
        tepath rootpath(folderPath);
        QList<tePictureFile*>appendPicturelist;
        if(clear){
            checkSave();
            ((tePictureFileModel*)ui->picturelist->model())->clear();
            folder_paths.clear();

            for(const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(rootpath)){
                if(!entry.is_directory()&&is_image_file(entry.path())){
                    appendPicturelist.push_back(new tePictureFile(tepath(entry)));
                }
            }

            folder_paths.insert(rootpath);
        }else{
            QList<QPair<tepath,bool>>duplicate_list = is_duplicate_path(rootpath);
            if(duplicate_list.size()==1&&duplicate_list.front().second==true)
                return;
            else if(!duplicate_list.empty()){
                for (const auto& entry : std::filesystem::directory_iterator(rootpath.stdpath)) {
                    if (entry.is_directory()) {
                        tepath currentPath(QString::fromStdString(entry.path().string())+'/');
                        bool shouldExclude = false;
                        for (const auto& excludedFolder : folder_paths) {
                            if (currentPath.isSubpath(excludedFolder)||currentPath==excludedFolder) {
                                shouldExclude = true;
                                break;
                            }
                        }
                        if (!shouldExclude) {
                            for(const std::filesystem::directory_entry& pic_entry : std::filesystem::recursive_directory_iterator(currentPath)){
                                if(is_image_file(pic_entry.path())){
                                    appendPicturelist.push_back(new tePictureFile(tepath(pic_entry)));
                                }
                            }
                        }
                    }else if(is_image_file(entry.path())){
                        appendPicturelist.push_back(new tePictureFile(tepath(entry)));
                    }
                }
            }else{
                for(const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(rootpath)){
                    if(is_image_file(entry.path())){
                        appendPicturelist.push_back(new tePictureFile(tepath(entry)));
                    }
                }
            }
            folder_paths.insert(rootpath);
        }
        defaultPath=folderPath;
        ((tePictureFileModel*)ui->picturelist->model())->append(appendPicturelist);
    }catch(std::exception e){
        telog(e.what());
        telog("[MainWindow::dialog_openpath]:exception occured, maybe the filepath includes some characters that can't be recognized by c++");
    }
    defaultPath=folderPath;
    save_config();
}
bool is_image_file(const std::filesystem::path &file) {
    std::string extension = file.extension().string();
    return extension == ".png" || extension == ".jpg"
           || extension == ".jpeg" || extension == ".bmp"
           || extension == ".gif"||extension == ".avif"|| extension == ".webp";
}
