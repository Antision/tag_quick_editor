#ifndef TEPICTUREFILE_H
#define TEPICTUREFILE_H
#include"tetag.h"

extern ants::ThreadPool thread_pool;
extern QThreadPool* qthreadpool;
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
    tePath (const std::filesystem::path& filepath){set(filepath);}
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

class tePictureFile:public teObject
{
public:
    tePath filepath;
    std::ifstream txtfile_read;
    std::ofstream txtfile_write;
    int refered_pixels;
    QImage image;
    std::mutex image_mt;
    teTagList taglist;
    ~tePictureFile(){
        txtfile_write.close();
        onDestroy();
    }
    tePictureFile(const tepath& input_filepath,QWidget* parent=nullptr,int r=80):
        filepath(input_filepath),refered_pixels(r*r){
        if(int ret = openpath();ret==-1) {

        }else if(ret==-2){
            telog("Could not open picture file");
        }
    }
    int openpath(std::string tagfile_extension=std::string(".txt"));
    int loadPicture();
    int loadtags(){
        taglist.load();
        return 0;
    }
    QString name() const {
        return QString::fromStdWString(filepath.stdpath.filename());
    }
    QString extension()const{
        return QString::fromStdWString(filepath.stdpath.extension());
    }

    void save();
};


#endif // TEPICTUREFILE_H
