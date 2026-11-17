#include "func.h"

#include <QFontDatabase>
#include <QFile>
#include <QTextStream>
#include "pch.h"
#include"mainwindow.h"
#include"logwindow.h"

namespace {
std::mutex g_logTargetMutex;
std::function<void(const QString&)> g_logTarget;
}

const QString& appFontFamily()
{
    static const QString family = [] {
        // Ubuntu 三字重(Light/Regular/Bold)一起注册,合并出 "Ubuntu" family:
        // main.cpp 用 Light 作主字重,需要粗体的地方能命中真 Bold 而不是合成粗体
        int loaded = 0;
        for (const QString& res : {QStringLiteral(":/res/fonts/Ubuntu-Light.ttf"),
                                   QStringLiteral(":/res/fonts/Ubuntu-Regular.ttf"),
                                   QStringLiteral(":/res/fonts/Ubuntu-Bold.ttf")})
            if (QFontDatabase::addApplicationFont(res) >= 0)
                ++loaded;
        return loaded ? QStringLiteral("Ubuntu") : QStringLiteral("Segoe UI");
    }();
    return family;
}

void teSetLogTarget(std::function<void(const QString&)> target)
{
    std::lock_guard<std::mutex> lg(g_logTargetMutex);
    g_logTarget = std::move(target);
}

void teLog(const QString& message)
{
    qDebug().noquote() << message;

    // GUI 进程没有控制台,qDebug 只进调试器,用户现场的日志一条也拿不到。设了
    // TE_LOG_FILE 就同时追加到那个文件:排查窗口行为这类只在用户机器上复现的问题
    // 时,让对方带这个环境变量启动再把文件发过来即可。
    {
        static const QString logPath = qEnvironmentVariable("TE_LOG_FILE");
        static std::mutex fileMutex;
        if (!logPath.isEmpty()) {
            std::lock_guard<std::mutex> lg(fileMutex);
            QFile f(logPath);
            if (f.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
                QTextStream ts(&f);
                ts.setEncoding(QStringConverter::Utf8);
                ts << message << '\n';
            }
        }
    }

    QCoreApplication* app = QCoreApplication::instance();
    if (!app)
        return;

    // teLog() may be called from the image-loader worker threads, so the widget
    // update has to be marshalled onto the GUI thread.
    QMetaObject::invokeMethod(app, [message] {
        std::function<void(const QString&)> target;
        {
            std::lock_guard<std::mutex> lg(g_logTargetMutex);
            target = g_logTarget;
        }
        if (target)
            target(message);
    }, Qt::QueuedConnection);
}

void setStyleSheetOnce(QWidget* widget,const QString& sheet)
{
    if (!widget)
        return;
    static const char* const kLastSheet = "teLastStyleSheet";
    if (widget->property(kLastSheet).toString() == sheet)
        return;                                 // already in place, do not re-parse
    widget->setProperty(kLastSheet,sheet);
    widget->setStyleSheet(sheet);
}

void setStateTextColor(QWidget* widget,const QColor& color)
{
    if (!widget)
        return;
    static const char* const kStateColor = "teStateColor";
    const QVariant cached = widget->property(kStateColor);
    if (cached.isValid() && cached.value<QColor>() == color)
        return;
    widget->setProperty(kStateColor,color);
    // Through the widget's own sheet, not through the palette: a sheet colour
    // always wins over a palette one, and the editors' page sheet sets
    // `*{color:white}` for every widget inside it - which is why the palette this
    // used to set was invisible and a captured tag no longer turned green.
    //
    // setStyleSheetOnce() is what keeps it cheap: the sheet is only re-parsed
    // when the colour really changes (Qt re-parsing one costs about 6 ms per
    // combo box, see the regression harness, which measures this).
    setStyleSheetOnce(widget,QStringLiteral("color:%1;").arg(color.name()));
}

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
    for(int i=0;i<size;++i){
        if(ifExist[i])
            continue;
        // nsfw 编辑器单独占第二栏:它和常规 tag 编辑器不是同一套工作流,塞进第一栏会把
        // 那一栏挤到横向溢出(通过成人认证后的首次启动拿到的就是这个默认布局)。
        // 已经有配置的用户不受影响——这里只处理"配置里完全没有"的编辑器。
        if(editorNames[i]==QStringLiteral("nsfw") && nsfwMode>0){
            if(layout.editors.size()<2)
                layout.editors.push_back({});
            layout.editors[1].push_back(editorNames[i]);
            continue;
        }
        layout.editors[0].push_back(editorNames[i]);
    }
    delete[] ifExist;
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
    editorLayoutFix(editorlistlayout);
    return -1;
}
bool isOpenBracket(const std::string &s) {
    return s == "(" || s == "\\(";
}

bool isCloseBracket(const std::string &s) {
    return s == ")" || s == "\\)";
}

std::string joinTag(const teTag& tag) {
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
    if (custom_controls)
        for (const auto& [name, control] : *custom_controls) {
            Q_UNUSED(control);
            customTags.append(name);
        }
    configObj.insert("customTags", customTags);
    configObj.insert("nsfwMode", nsfwMode);
    QJsonArray mainWindowSplitterLengthArr;
    QList<int> mainwindowSplitterSizes = static_cast<MainWindow*>(global_window)->splitter->sizes();
    for(int i =0;i<MainWindowWidgetCount;++i){
        mainWindowSplitterLengthArr.push_back(mainwindowSplitterSizes[i]);
    }
    configObj.insert("mainWindowSplitterLength", mainWindowSplitterLengthArr);

    // 最大化状态下 x()/y()/width()/height() 是铺满工作区的那个矩形,存下来下次
    // 启动就变成一个铺满屏幕的普通窗口:存普通状态的矩形。
    const QRect windowGeom = global_window->isMaximized() ? global_window->normalGeometry()
                                                          : global_window->geometry();
    QJsonArray mainWindowSizeArr;
    mainWindowSizeArr.append(windowGeom.x());
    mainWindowSizeArr.append(windowGeom.y());
    mainWindowSizeArr.append(windowGeom.width());
    mainWindowSizeArr.append(windowGeom.height());
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
double getWindowScale(HWND hwnd)
{
    // 动态获取 GetDpiForWindow（Windows 10/1607+），兼容低版本
    typedef UINT(WINAPI *GetDpiForWindow_t)(HWND);
    static GetDpiForWindow_t pGetDpiForWindow = reinterpret_cast<GetDpiForWindow_t>(
        GetProcAddress(GetModuleHandleW(L"user32"), "GetDpiForWindow"));

    UINT dpi = 96; // fallback
    if (pGetDpiForWindow) {
        dpi = pGetDpiForWindow(hwnd);
    } else {
        // fallback: 从设备上下文读取 DPI（旧方法）
        HDC hdc = GetDC(hwnd);
        if (hdc) {
            dpi = GetDeviceCaps(hdc, LOGPIXELSX);
            ReleaseDC(hwnd, hdc);
        }
    }
    return static_cast<double>(dpi) / 96.0;
}
