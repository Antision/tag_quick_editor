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

class QMessageBox;

/**
 * @brief 把 About Qt 那个 QMessageBox 刷成和程序一致的深色。
 *
 * MainWindow 的样式表里有 `QWidget{color:white}`;对话框以 MainWindow 为 parent,
 * 这条规则会顺着 parent 链漏进去,而标准对话框的底是白的 —— 白底白字。
 * `QMessageBox::aboutQt()` 是静态函数,没法只给它一个人套样式,所以 About Qt
 * 改成自己建 QMessageBox 再调这里。
 */
void applyAboutQtStyle(QMessageBox& box);

/**
 * @brief Opens the tag decorations dialog, applies the result and saves it.
 *
 * The one entry point for "this tag needs a look": the Options menu, the
 * decoration page's title and the custom tag window's chip menu all come here.
 * The dialog only edits the list; the registry, config.json and every shown row
 * are updated here, because only the window knows them.
 *
 * @param seedTag tag to pre-select (empty = just open the list).
 * @return true when the user pressed Save.
 */
bool editTagDecorations(QWidget* parent,const QString& seedTag);

class tePictureFileModel;
class PictureFileCaptionSource;
class WindowGlyphButton;
class teSelectionTagListView;
class teSelectionTagModel;
class LogWidget;
class ConnectivityOverlay;

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

    WindowGlyphButton*close_btn;
    WindowGlyphButton*maximize_btn;
    WindowGlyphButton*minimize_btn;
    int boundaryWidth=6;
    QSplitter* splitter;
    tePictureFileModel*picturefileModel;
    tePictureListView* picturefileListView;
    /// tqe's CaptionSource: turns the path the tag list asks about into the
    /// tePictureFile the picture list already holds (TV-03).
    PictureFileCaptionSource* captionSource=nullptr;
    teImageWidget*imageWidget=nullptr;
    teSelectionTagModel* multitaglistmodel;
    teSelectionTagListView*multitaglist;
    filterWidget*filterWindow=nullptr;
    EditorListLayoutWidget* editorlistlayoutwidget;
    LogWidget* logWindow=nullptr;
    /// The capture lines over the splitter area (S7); owned by this window.
    ConnectivityOverlay* connectivityOverlay=nullptr;
    QTimer* autoSaveTimer=nullptr;
    /// Coalesces picture list selection changes into one tag list reload.
    QTimer* selectionTimer=nullptr;
    /// The files the multi tag list currently represents.
    QList<tePictureFile*> shownMultiTagFiles;
    ~MainWindow();
    void save();
    /// 同步 nsfw 开关:改 nsfwMode、把 nsfw 编辑器排进/移出第二栏、保存配置。
    /// rebuildLayout 为 false 时只同步显隐(启动时用,分栏已由 load_config 决定)。
    void applyNsfwMode(bool on, bool rebuildLayout);
    int saveState(bool ifRunning=true);
    int loadState();
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result)override;
    /// Paints a strip in the menu bar colour behind the fake title bar. The
    /// window is translucent and paints no background of its own, so any
    /// device pixel between the menu bar and the central widget that layout
    /// rounding leaves uncovered would show the desktop wallpaper (a bright
    /// seam that only shows up at fractional display scaling).
    void paintEvent(QPaintEvent* event) override;
    /// Toggles the *system* maximized state. Windows has to own that state: it is
    /// what makes dragging the title bar down from the screen edge restore the
    /// window, and what keeps the button / double click / drag-to-top paths doing
    /// the same thing. Nothing here (or anywhere else) may call setGeometry while
    /// the window is maximized — see the comment on the implementation.
    void toggleMaximize();
    /// 最大化时中间那颗按钮换成"还原"图形(LW-06)。
    void updateWindowGlyphs();
    /// 最大化时窗口矩形会比工作区每边多出"看不见的边框"(实测 9 物理像素)。客户区
    /// 不能靠 WM_NCCALCSIZE 收窄(半透明窗口会把 Qt 的画布钉死在旧尺寸、只剩左上角
    /// 一块有画面),所以让内容自己往里让这么多。
    void updateMaximizedContentInset();
    void changeEvent(QEvent* event) override;
    /// Rounded mask for the fake title bar so item hover/selection and the
    /// window buttons cannot paint outside the QMenuBar corner radius.
    void updateMenuBarMask();
    bool eventFilter(QObject* watched, QEvent* event) override;
    void emitloadlists(const QItemSelection &selected, const QItemSelection &deselected);
    LRESULT OnTestBorder(const QPoint &pt);
    void checkForUpdate();
    /// Asks every queued thumbnail decode to return without decoding. Called from
    /// main() before the pool is drained, so closing a folder of images does not
    /// make the process wait for the rest of the folder (the old shutdown waited
    /// for all of them, which is also why a single stuck task hung it forever).
    void cancelPendingWork();
    /// Makes the log window an owned window of this one: above it, and destroyed
    /// with it (see common/logwidget).
    void attachLogWindow();
    int checkSave();
    /// Files currently selected in the picture list (one entry per row).
    QList<tePictureFile*> selectedPictureFiles() const;
    void onApplicationClose(){
        if(checkSave())
            QCoreApplication::quit();
    };

public slots:
    void dialog_LoadPath(bool clear=true);
    /// Options -> NSFW Mode: 开关 nsfw 编辑器并把它排到(或移出)第二栏
    void onNsfwModeToggled(bool on);
    /// Options -> "Tag Edit Popup": the right-hand popup follows every row, or
    /// only previews a selected sentence / over-long tag (see TagListView).
    void onTagEditPopupToggled(bool on);
    /// Applies the pending picture-list selection (debounced).
    void applyPendingSelection();
    /// Runs autoSaveSec after the last change; safely on the GUI thread.
    void autoSaveTick();
    void showLogWindow();
private slots:
    void on_sortByLexButton_clicked();
    void on_sortByOccurrenceButton_clicked();
};

#endif // MAINWINDOW_H
