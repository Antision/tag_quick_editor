QT       += core gui
QT       += network
# 二级按钮的菜单是 .ui 文件,运行时用 QUiLoader 从 qrc 里读(SC-10)
QT       += uitools
# common/audio (提示音) 用 QMediaPlayer;缺这个模块链接会失败
QT       += multimedia
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++20

VERSION = 0.0.1.1
DEFINES += APP_VERSION=\\\"$$VERSION\\\"

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

CONFIG+=precompile_header
PRECOMPILED_HEADER=src/pch.h
# QMAKE_CXXFLAGS += /MP
LIBS += -luser32
LIBS += -ldwmapi
LIBS += -lgdi32
RC_ICONS = res/icon.ico

# 公用代码库(与 gallery_info 共用,独立仓库,git submodule -> ../common)
include($$PWD/common/common.pri)
SOURCES += \
    src/colorpicker.cpp \
    src/connectivityoverlay.cpp \
    src/ctag.cpp \
    src/linkgeometry.cpp \
    src/func.cpp \
    src/main.cpp \
    src/mainwindow.cpp \
    src/numberpad.cpp \
    src/picturefilecaptionsource.cpp \
    src/prefix_priority.cpp \
    src/secondarybutton.cpp \
    src/secondarycaption.cpp \
    src/secondarymenu.cpp \
    src/testyle.cpp \
    src/suggestionsource.cpp \
    src/tagviewhost.cpp \
    src/decorationeditorpage.cpp \
    src/settingsdialog.cpp \
    src/decorationcontrol.cpp \
    src/teeditor.cpp \
    src/teeditor_derive.cpp \
    src/teeditorcontrol.cpp \
    src/teeditorlist.cpp \
    src/teeditorlistpageview.cpp \
    src/teimagewidget.cpp \
    src/tepicturefile.cpp \
    src/tepicturelistview.cpp \
    src/tereftaglistwidget.cpp \
    src/teselectiontaglist.cpp \
    src/tesignalwidget.cpp \
    src/tetag.cpp \
    src/tetagdisplaywidget.cpp \
    src/tetaglistwidget.cpp \
    src/widget_pool.cpp

HEADERS += \
    src/colorpicker.h \
    src/connectivityoverlay.h \
    src/ctag.h \
    src/linkgeometry.h \
    src/func.h \
    src/mainwindow.h \
    src/pch.h \
    src/numberpad.h \
    src/picturefilecaptionsource.h \
    src/prefix_priority.h \
    src/secondarybutton.h \
    src/secondarycaption.h \
    src/secondarymenu.h \
    src/suggestionsource.h \
    src/tagviewhost.h \
    src/decorationeditorpage.h \
    src/settingsdialog.h \
    src/decorationcontrol.h \
    src/teeditor.h \
    src/teeditor_derive.h \
    src/teeditorcontrol.h \
    src/teclothescontrol.h \
    src/tefacecontrols.h \
    src/tebodycontrols.h \
    src/tehaircontrols.h \
    src/temisccontrols.h \
    src/teeditorhelpers.h \
    src/teeditorlist.h \
    src/teeditorlistpageview.h \
    src/teimagewidget.h \
    src/tepicturefile.h \
    src/tepicturelistview.h \
    src/tereftaglistwidget.h \
    src/teselectiontaglist.h \
    src/tesignalwidget.h \
    src/testyle.h \
    src/tetag.h \
    src/tetagdisplaywidget.h \
    src/tetaglistwidget.h \
    src/type_search.h \
    src/widget_pool.h \
    src/windows_frame.h

FORMS += \
    src/mainwindow.ui \
    src/settingsdialog.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    LICENSES/licenses.qrc \
    src/res.qrc

DISTFILES +=

win32 {
    # Qt 运行时随 exe 部署:直接双击 exe 也能跑,不再依赖 Qt Creator 注入的 PATH
    CONFIG(debug, debug|release) {
        RUNTIME_DST = $$OUT_PWD/debug
    } else {
        RUNTIME_DST = $$OUT_PWD/release
    }
    CONFIG(debug, debug|release) {
        QMAKE_POST_LINK += $$escape_expand(\\n\\t) windeployqt --no-translations --no-compiler-runtime --debug $(DESTDIR_TARGET)
    } else {
        QMAKE_POST_LINK += $$escape_expand(\\n\\t) windeployqt --no-translations --no-compiler-runtime --release $(DESTDIR_TARGET)
    }
}
