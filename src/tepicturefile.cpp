#include "tepicturefile.h"
#include "func.h"

int tePictureFile::openpath(std::string tagfile_extension){
    if(!std::filesystem::exists(filepath)) return -2;
    thread_pool.add([this]{loadPicture();});
    std::filesystem::path stdpath = filepath.stdpath;
    if(std::filesystem::exists(stdpath.replace_extension(tagfile_extension))){
        txtfile_read.open(stdpath.replace_extension(tagfile_extension),std::ios::in);
        if(!txtfile_read.is_open()){
            telog("[tePictureFile::openpath()]:Found file but couldn't open it");
            return -2;
        }
    } else {
        txtfile_read.open(stdpath.replace_extension(tagfile_extension),std::ios::in);
        if(txtfile_read.is_open()){
            telog("[tePictureFile::openpath()]:Couldn't find prompt file of the picture");
            return -1;
        }else{
            telog("[tePictureFile::openpath()]:Create File object Error");
            return -2;
        }
    }
    std::string content;
    content.assign((std::istreambuf_iterator<char>(txtfile_read)), std::istreambuf_iterator<char>());
    QString QStringContent = QString::fromStdString(content);
    QRegularExpression re(R"((?:,|\n|(?<=[a-zA-Z0-9])\.))");
    QStringList tokens = QStringContent.split(re, Qt::SkipEmptyParts);
    for (const QString& token : tokens) {
        if (!token.isEmpty()) {
            taglist.initialize_push_back(token);
        }
    }
    // static std::regex re(R"((,|\n|(?<=[a-zA-Z0-9])\.))");
    // std::sregex_token_iterator iter(content.begin(), content.end(), re, -1);
    // std::sregex_token_iterator end;
    // while (iter != end) {
    //     std::string token = *iter++;
    //     if (!token.empty()) {
    //         taglist.initialize_push_back(token);
    //     }
    // }

    taglist.isTagsLoaded=true;
    txtfile_read.close();
    return 0;
}

int tePictureFile::loadPicture(){
    ++loading_count;
    QImageReader reader(filepath);
    reader.setAutoTransform(true);
    QSize originalSize = reader.size();
    double pixmap_wh_ratio  = (double)originalSize.width()/originalSize.height();
    int labelw=pow(refered_pixels*pixmap_wh_ratio,0.5),labelh=pow(refered_pixels/pixmap_wh_ratio,0.5);
    reader.setScaledSize({labelw,labelh});
    image_mt.lock();
    image = reader.read();
    image_mt.unlock();
    --loading_count;
    teemit(teCallbackType::loading_finished);
    return 0;
}

void tePictureFile::save(){
    if(taglist.isSaved)
        return;
    txtfile_write.open(std::filesystem::path(filepath.stdpath).replace_extension("txt"));
    int tagCount = taglist.size();
    for (int t = 0; t < tagCount; ++t) {
        teTagCore& tag = taglist[t];
        std::string tagStr = joinTag(tag);
        txtfile_write << tagStr;
        if (t < tagCount - 1) {
            txtfile_write << ", ";
        }
    }
    taglist.isSaved=true;
    txtfile_write.close();
}
