#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "teimagewidget.h"
#include"tepicturefile.h"
#include"teeditor.h"
#include "ui_mainwindow.h"
#include "teeditorlistpageview.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE
bool is_image_file(const std::filesystem::path& file);
extern std::vector<ctag> ctags;
extern QWidget* global_window;
class tePictureFileModel;
class teMultitagListView;
class teMultiTagListModel;
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    Ui::MainWindow *ui;

    QPushButton*close_btn;
    QPushButton*maximize_btn;
    QPushButton*minimize_btn;
    int boundaryWidth=6;
    QSplitter* splitter;
    tePictureFileModel*picturefileModel;
    tePictureListView* picturefileListView;
    teImageWidget*imageWidget=nullptr;
    teMultiTagListModel* multitaglistmodel;
    teMultitagListView*multitaglist;
    filterWidget*filterWindow=nullptr;
    EditorListLayoutWidget* editorlistlayoutwidget;
    ~MainWindow();
    void save();
    int saveState(bool ifRunning=true);
    int loadState();
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result)override;
    void emitloadlists(const QItemSelection &selected, const QItemSelection &deselected);
    LRESULT OnTestBorder(const QPoint &pt);
    void checkForUpdate();
    int checkSave();
    void onApplicationClose(){
        if(checkSave())
            QCoreApplication::quit();
    };
public slots:
    void dialog_LoadPath(bool clear=true);

};

#endif // MAINWINDOW_H
