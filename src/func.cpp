#include "func.h"
#include "pch.h"
#include"mainwindow.h"
int findWidgetIndexInLayout(QBoxLayout* layout, QWidget* widget) {
    if (!layout || !widget)
        return -1;

    for (int i = 0; i < layout->count(); ++i) {
        QLayoutItem* item = layout->itemAt(i);
        if (item && item->widget() == widget) {
            return i;
        }
    }
    return -1;
}


bool autoMerge=true;
bool MergeSwitch=true;
int nsfwMode=0;
int autoSaveSec=20;
QString defaultPath;
QStringList custom_tags;
std::atomic<int> loading_count=0;
QMap<QString,teEditorControl*>* custom_controls;
editorListLayout editorlistlayout;
int mainWindowSplitterLength[MainWindowWidgetCount];
QRect mainwindowGeometry;
extern QWidget* global_window;

void editorLayoutFix(editorListLayout&layout){
    extern QStringList editorNames;
    int size = editorNames.size();
    bool* ifExist = new bool[size]();
    for(QStringList& strlist:layout.editors){
        for(QString&str:strlist){
            for(int i=0;i<size;++i)
                if(editorNames[i]==str){
                    ifExist[i]=true;
                }
        }
    }
    if(layout.editors.size()==0)
        layout.editors.push_back({});
    for(int i=0;i<size;++i)
        if(!ifExist[i])
            layout.editors[0].push_back(editorNames[i]);
}

int load_config(){
    QFile f("./config.json");
    QJsonObject configObj;
    try{
        if(!f.open(QFile::ReadOnly | QFile::Text)){
            throw std::exception("[load_config]:failed to open config json file");
        }
        QTextStream stream(&f);
        stream.setEncoding(QStringConverter::Utf8);

        QString str = stream.readAll();
        f.close();

        QJsonParseError jsonError;
        QJsonDocument doc = QJsonDocument::fromJson(str.toUtf8(), &jsonError);
        if (jsonError.error != QJsonParseError::NoError && !doc.isNull()) {
            throw std::exception("[load_config]:error json");
        }
        configObj = doc.object();

        defaultPath = configObj.value("defaultPath").toString();

        if(configObj.contains("nsfwMode"))
            nsfwMode = configObj.value("nsfwMode").toInt();

        QJsonValue ifautoMergeV= configObj.value("autoMergeTags");
        if(ifautoMergeV.isBool())
            autoMerge = ifautoMergeV.toBool();

        QJsonValue autoSaveSecV = configObj.value("autoSaveSecs");
        if(autoSaveSecV.isDouble())
            autoSaveSec = autoSaveSecV.toInt();

        QJsonArray customTagsArray = configObj.value("customTags").toArray();
        for(QJsonValue v:customTagsArray){
            custom_tags.push_back(v.toString());
        }

        QJsonArray mainWindowSplitterLengthArr = configObj.value("mainWindowSplitterLength").toArray();
        if(!mainWindowSplitterLengthArr.isEmpty())
            for(int i =0;i<MainWindowWidgetCount;++i){
                mainWindowSplitterLength[i]=mainWindowSplitterLengthArr[i].toInt();
            }

        QJsonArray mainWindowSizeArr = configObj.value("mainWindowSize").toArray();
        if(!mainWindowSizeArr.isEmpty())
            mainwindowGeometry = {mainWindowSizeArr[0].toInt(),mainWindowSizeArr[1].toInt(),mainWindowSizeArr[2].toInt(),mainWindowSizeArr[3].toInt()};

        QJsonArray editorListPages = configObj.value("editorListPages").toArray();
        for(QJsonValue v:editorListPages){
            QJsonArray editorObjs = v.toArray();
            QStringList pageEditorsStrList;
            for(QJsonValue editorObjstr:editorObjs){
                pageEditorsStrList.push_back(editorObjstr.toString());
            }
            editorlistlayout.editors.push_back(std::move(pageEditorsStrList));
        }
    }catch(std::exception &e){
        telog(e.what());
    }
    if(nsfwMode==0){
        adultCheckWindow w(&nsfwMode);
    }
    editorLayoutFix(editorlistlayout);
    return -1;
}
bool isOpenBracket(const std::string &s) {
    return s == "(" || s == "\\(";
}

bool isCloseBracket(const std::string &s) {
    return s == ")" || s == "\\)";
}

std::string joinTag(const teTagCore& tag) {
    std::string res;
    int wordCount = tag.words.size();
    for (int i = 0; i < wordCount; ++i) {
        std::string currentWord = tag.words[i]->text.toStdString();
        if (i == 0) {
            res += currentWord;
        } else {
            if (isCloseBracket(currentWord)) {
                res += currentWord;
            }
            else {
                std::string prevWord = tag.words[i-1]->text.toStdString();
                if (isOpenBracket(prevWord)) {
                    res += currentWord;
                }
                else {
                    res += " " + currentWord;
                }
            }
        }
    }
    return res;
}

int save_config() {
    QJsonDocument doc;
    QJsonObject configObj;

    configObj.insert("autoMergeTags", autoMerge);
    configObj.insert("autoSaveSecs", autoSaveSec);

    configObj.insert("defaultPath", defaultPath);

    QJsonArray customTags;
    for (int i = 0; i < custom_controls->keys().size(); ++i)
        customTags.append(custom_controls->keys()[i]);
    configObj.insert("customTags", customTags);
    configObj.insert("nsfwMode", nsfwMode);
    QJsonArray mainWindowSplitterLengthArr;
    QList<int> mainwindowSplitterSizes = static_cast<MainWindow*>(global_window)->splitter->sizes();
    for(int i =0;i<MainWindowWidgetCount;++i){
        mainWindowSplitterLengthArr.push_back(mainwindowSplitterSizes[i]);
    }
    configObj.insert("mainWindowSplitterLength", mainWindowSplitterLengthArr);

    QJsonArray mainWindowSizeArr;
    mainWindowSizeArr.append(global_window->x());
    mainWindowSizeArr.append(global_window->y());
    mainWindowSizeArr.append(global_window->width());
    mainWindowSizeArr.append(global_window->height());
    configObj.insert("mainWindowSize", mainWindowSizeArr);

    QJsonArray editorlistPages;
    for (QStringList &strlist : editorlistlayout.editors) {
        QJsonArray pageEditors;
        for (QString &str : strlist) {
            pageEditors.append(str);
        }
        editorlistPages.append(pageEditors);
    }
    configObj.insert("editorListPages", editorlistPages);


    doc.setObject(configObj);
    QFile f("./config.json");
    if (f.open(QFile::WriteOnly | QFile::Text)) {
        QTextStream stream(&f);
        stream.setEncoding(QStringConverter::Utf8);
        stream << doc.toJson();
        f.close();
        return 0;
    }
    return -1;
}

void CreateAutoSaveThread(MainWindow*w){
    thread_pool.add([=]{
        while(autoSaveSec){
            std::this_thread::sleep_for(std::chrono::seconds(autoSaveSec));
            w->saveState(true);
        }
    });
}

void MainWindow::checkForUpdate() {
    QNetworkAccessManager *manager = new QNetworkAccessManager(this);
    const QVersionNumber currentVersion = QVersionNumber::fromString(qApp->applicationVersion());

    QEventLoop loop;
    connect(manager, &QNetworkAccessManager::finished, &loop, &QEventLoop::quit);

    QNetworkRequest request(QUrl("https://api.github.com/repos/Antision/tag_quick_editor/releases/latest"));
    request.setRawHeader("User-Agent", "TagQuickEditor/"+qApp->applicationVersion().toUtf8());

    QNetworkReply *reply = manager->get(request);
    loop.exec();

    if (reply->error()) {
        qDebug() << "Update error:" << reply->errorString();
        reply->deleteLater();
        return;
    }

    QJsonParseError parseError{};
    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qDebug() << "JSON parse error:" << parseError.errorString();
        reply->deleteLater();
        return;
    }

    QJsonObject json = doc.object();
    if (!json.contains("tag_name") || !json["tag_name"].isString()) {
        qDebug() << "Invalid release data";
        reply->deleteLater();
        return;
    }

    QVersionNumber latestVersion = QVersionNumber::fromString(json["tag_name"].toString().remove('v'));
    if (latestVersion > currentVersion) {
        QMetaObject::invokeMethod(this, [this, json, latestVersion](){
            QJsonArray assets = json["assets"].toArray();
            if (!assets.isEmpty()) {
                QUrl downloadUrl = assets[0].toObject()["browser_download_url"].toString();
                QMessageBox::StandardButton reply = QMessageBox::question(
                    this,
                    tr("Update Available"),
                    tr("New version %1 is available. Download now?").arg(latestVersion.toString()),
                    QMessageBox::Yes|QMessageBox::No
                    );
                if (reply == QMessageBox::Yes) {
                    QDesktopServices::openUrl(downloadUrl);
                }
            }
        }, Qt::QueuedConnection);
    }
    reply->deleteLater();
}
