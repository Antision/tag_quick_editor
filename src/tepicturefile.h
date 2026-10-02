#ifndef TEPICTUREFILE_H
#define TEPICTUREFILE_H
#include"tetag.h"

extern BS::thread_pool<> thread_pool;

/**
 * @brief A path that keeps both the QString and the std::filesystem::path form.
 *
 * Note: the two representations are only kept in sync by set(); constructing
 * from one and reading the other is fine, but do not mutate qstring directly.
 */
typedef class tePath{
public:
    QString qstring;
    std::filesystem::path stdpath;
    tePath(){
    }
    void set(const QString& path){
        qstring=path;
        stdpath=path.toStdWString();
    }
    void set(const std::filesystem::path& filepath){
        stdpath=filepath;
        qstring=QString::fromStdWString(filepath);
    }
    tePath(const QString& path){set(path);}
    tePath(const std::filesystem::path& filepath){set(filepath);}
    operator QString() const{
        return qstring;
    }
    operator std::filesystem::path() const{
        return stdpath;
    }
    bool operator==(const tePath&in)const{
        return in.stdpath==this->stdpath;
    }
    bool operator<(const tePath&in)const{
        return this->qstring<in.qstring;
    }
    bool isSubpath(const tePath&in)const{
        if(in.qstring.length()>=qstring.length())
            return false;
        return qstring.indexOf(in.qstring)==0;
    }
} tepath;

/**
 * @brief One image of the opened dataset together with its caption (tag file).
 *
 * Threading contract
 * ------------------
 * The thumbnail is decoded on a worker thread. That worker never dereferences
 * the tePictureFile: it writes into a shared ImageSlot and asks the GUI thread
 * to deliver `loading_finished` through a load token. The object therefore may
 * be destroyed while its thumbnail is still being decoded.
 */
class tePictureFile:public teObject
{
public:
    /// Owned jointly by the tePictureFile and its loader task.
    struct ImageSlot{
        std::mutex mtx;
        QImage image;
        std::atomic<bool> cancelled{false};
    };

    tePath filepath;
    int refered_pixels;
    teTagList taglist;

    explicit tePictureFile(const tepath& input_filepath,int r=80);
    ~tePictureFile();

    /// Reads the caption file and kicks off the asynchronous thumbnail decode.
    int openpath(std::string tagfile_extension=std::string(".txt"));

    /// Starts (or restarts) the asynchronous thumbnail decode.
    void startLoadPicture();

    /// Thread-safe copy of the current thumbnail (null until decoded).
    QImage thumbnail() const;
    bool hasThumbnail() const;

    /// GUI thread only. Emits loading_finished.
    void notifyPictureLoaded();

    /// Cancels a pending/in-flight decode and detaches the completion
    /// notification. Must be called before the file is deleted.
    void cancelLoading();

    QString name() const {
        return QString::fromStdWString(filepath.stdpath.filename());
    }
    QString extension()const{
        return QString::fromStdWString(filepath.stdpath.extension());
    }

    /// Path of the caption file belonging to this image.
    QString tagFilePath() const;

    void save();

private:
    struct LoadToken{
        std::atomic<bool> alive{true};
        std::shared_ptr<ImageSlot> slot;
    };
    std::shared_ptr<ImageSlot> image_slot;
    std::shared_ptr<LoadToken> load_token;
};

#endif // TEPICTUREFILE_H
