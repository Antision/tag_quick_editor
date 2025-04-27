QT       += core gui
QT       += network
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++20

VERSION = 0.0.0
DEFINES += APP_VERSION=\\\"$$VERSION\\\"

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

CONFIG+=precompile_header
PRECOMPILED_HEADER=src/pch.h
# QMAKE_CXXFLAGS += /MP
LIBS += -luser32
LIBS += -ldwmapi
RC_ICONS = res/icon.ico
SOURCES += \
    src/ctag.cpp \
    src/func.cpp \
    src/main.cpp \
    src/mainwindow.cpp \
    src/prefix_priority.cpp \
    src/qflowlayout.cpp \
    src/suggestionlineedit.cpp \
    src/teeditor.cpp \
    src/teeditor_derive.cpp \
    src/teeditorcontrol.cpp \
    src/teeditorlist.cpp \
    src/teeditorlistpageview.cpp \
    src/teimagewidget.cpp \
    src/temultitaglistview.cpp \
    src/teobject.cpp \
    src/tepicturefile.cpp \
    src/tepicturelist.cpp \
    src/tepicturelistview.cpp \
    src/tereftaglistwidget.cpp \
    src/tesignalwidget.cpp \
    src/tetag.cpp \
    src/tetaglistwidget.cpp \
    src/widget_pool.cpp

HEADERS += \
    src/ctag.h \
    src/func.h \
    src/mainwindow.h \
    src/pch.h \
    src/prefix_priority.h \
    src/qflowlayout.h \
    src/suggestionlineedit.h \
    src/teeditor.h \
    src/teeditor_derive.h \
    src/teeditorcontrol.h \
    src/teeditorlist.h \
    src/teeditorlistpageview.h \
    src/teimagewidget.h \
    src/temultitaglistview.h \
    src/teobject.h \
    src/tepicturefile.h \
    src/tepicturelistview.h \
    src/tereftaglistwidget.h \
    src/tesignalwidget.h \
    src/tetag.h \
    src/tetaglistwidget.h \
    src/threadpool.h \
    src/widget_pool.h

FORMS += \
    src/mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    LICENSES/licenses.qrc \
    src/res.qrc \
    res/adult-check/websiteIcon.qrc

DISTFILES +=
