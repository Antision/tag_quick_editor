#pragma once
#ifdef __cplusplus
#define __STDC_CONSTANT_MACROS
#endif
#ifdef _STDINT_H
#undef _STDINT_H
#endif
#define USE_ALLOCA

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <io.h>
#include <iomanip>
#include <filesystem>
#include <set>
#include <map>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <thread>
#include <mutex>
#include <Windows.h>
#include <windowsx.h>
#include <winuser.h>
#include <wingdi.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <queue>
#include <iterator>
#include <condition_variable>
#include <functional>
#include <exception>
#include <format>
#include <coroutine>
#include <cstddef>
#include <chrono>
#include <limits>
#include <regex>
#include <memory>
#include <optional>
#include <tuple>
#include <unordered_set>
#include <utility>


#include <QApplication>
#include <QObject>
#include <QEvent>
#include <QWidget>
#include <QLabel>
#include <qpushbutton>
#include <QButtonGroup>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QFileDialog>
#include <QScrollArea>
#include <QScrollBar>
#include <QSlider>
#include <QClipboard>
#include <QToolButton>
#include <QMouseEvent>
#include <QThread>
#include <QThreadPool>
#include <QImage>
#include <QImageReader>
#include <QLineEdit>
#include <QListWidget>
#include <QStringListModel>
#include <QCompleter>
#include <QComboBox>
#include <QTimer>
#include <QMenu>
#include <qmenubar>
#include <QShortcut>
#include <QSplitter>
#include <QPainter>
#include <QMimeData>
#include <QDrag>
#include <QPlainTextEdit>
#include <QLoggingCategory>
#include "qflowlayout.h"
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QJsonParseError>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QVersionNumber>
#include <QNetworkReply>
#include <QDesktopServices>

#include "func.h"
#include "BS_thread_pool.hpp"
#include "ctag.h"
#include "teobject.h"
#include "widget_pool.h"
//lite_memory_check_begin
#if defined(WIN32) && defined(_MSC_VER) &&  defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#define DEBUG_NEW new( _NORMAL_BLOCK, __FILE__, __LINE__ )
#define new DEBUG_NEW
#endif
//lite_memory_check_end
