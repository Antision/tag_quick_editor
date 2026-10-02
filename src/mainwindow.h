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


extern bool autoMerge;
extern bool MergeSwitch;
extern int nsfwMode;
extern int autoSaveSec;
extern QString defaultPath;
extern QStringList custom_tags;
extern editorListLayout editorlistlayout;
extern int mainWindowSplitterLength[MainWindowWidgetCount];
extern QRect mainwindowGeometry;

bool is_image_file(const std::filesystem::path& file);
extern std::vector<ctag> ctags;
extern QWidget* global_window;

class tePictureFileModel;
class teSelectionTagListView;
class teSelectionTagModel;
class LogWindow;

/// Outcome of teResolveDuplicateStems().
struct teDuplicateStemReport{
    int renamed = 0;
    int skipped = 0;
    QStringList details;    ///< one human readable line per affected file
};

/**
 * @brief Renames images of one folder that share the same base name.
 *
 * When a training folder contains e.g. `a.png`, `a.jpg` and `a.txt`, only one
 * of the images can own `a.txt`. The later images (sorted by path) are renamed
 * to `a_1.jpg`, `a_2.jpeg`, ... and their caption file is renamed as well.
 *
 * Safety rules:
 *  - a target that already exists in the folder is never overwritten; the next
 *    `_num` is tried instead;
 *  - a file name that would push the full path past the Win32 limit is trimmed
 *    from its tail, then re-checked for collisions;
 *  - if the caption file cannot be moved, the image rename is rolled back;
 *  - anything that cannot be handled is left untouched and reported.
 *
 * @param imagePaths updated in place with the new paths.
 */
teDuplicateStemReport teResolveDuplicateStems(QVector<tePath>& imagePaths);

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
    teSelectionTagModel* multitaglistmodel;
    teSelectionTagListView*multitaglist;
    filterWidget*filterWindow=nullptr;
    EditorListLayoutWidget* editorlistlayoutwidget;
    LogWindow* logWindow=nullptr;
    QTimer* autoSaveTimer=nullptr;
    /// Coalesces picture list selection changes into one tag list reload.
    QTimer* selectionTimer=nullptr;
    /// The files the multi tag list currently represents.
    QList<tePictureFile*> shownMultiTagFiles;
    ~MainWindow();
    void save();
    int saveState(bool ifRunning=true);
    int loadState();
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result)override;
    void emitloadlists(const QItemSelection &selected, const QItemSelection &deselected);
    LRESULT OnTestBorder(const QPoint &pt);
    void checkForUpdate();
    int checkSave();
    /// Files currently selected in the picture list (one entry per row).
    QList<tePictureFile*> selectedPictureFiles() const;
    void onApplicationClose(){
        if(checkSave())
            QCoreApplication::quit();
    };

public slots:
    void dialog_LoadPath(bool clear=true);
    /// Applies the pending picture-list selection (debounced).
    void applyPendingSelection();
    /// Runs autoSaveSec after the last change; safely on the GUI thread.
    void autoSaveTick();
    void showLogWindow();
};

#endif // MAINWINDOW_H
