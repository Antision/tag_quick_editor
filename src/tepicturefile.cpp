#include "tepicturefile.h"
#include "func.h"



int tePictureFile::openpath(std::string tagfile_extension)
{
    // Ensure the file exists using QFileInfo (Qt way)
    QFileInfo fileInfo(filepath);
    if (!fileInfo.exists()) {
        telog(QString("Couldn't test the existence of file %1").arg(filepath));
        return -1;  // Or any appropriate error code
    }

    thread_pool.add([this]{ loadPicture(); });

    // Replace extension using QFileInfo
    QString tagfilePath = filepath;
    tagfilePath = fileInfo.absolutePath() + "/" + fileInfo.completeBaseName() + QString::fromStdString(tagfile_extension);

    QFile txtfile_read(tagfilePath);
    if (txtfile_read.exists()) {
        if (!txtfile_read.open(QIODevice::ReadOnly | QIODevice::Text)) {
            telog("[tePictureFile::openpath()]: Found file but couldn't open it");
            return -2;
        }
    } else {
        // If the file doesn't exist, attempt to open a non-existing file for logging purposes
        if (txtfile_read.open(QIODevice::ReadOnly | QIODevice::Text)) {
            telog("[tePictureFile::openpath()]: Couldn't find prompt file for the picture");
            return -1;
        } else {
            telog("[tePictureFile::openpath()]: Create File object Error");
            return -2;
        }
    }

    QTextStream in(&txtfile_read);
    QString content = in.readAll();

    const auto pieces = splitTextToPieces(content);
    for (const auto& piece : pieces) {
        if (!piece.text.isEmpty()) {
            taglist.initialize_push_back(piece.text, piece.sentence);
        }
    }

    taglist.isTagsLoaded = true;
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

void tePictureFile::save()
{
    if (taglist.isSaved)
        return;

    txtfile_write.open(std::filesystem::path(filepath.stdpath).replace_extension("txt"));

    QString text = serializePieces(taglist.size(), [&](int i) -> std::shared_ptr<tetagcore> {
        return taglist.tags[i];
    }, true);

    txtfile_write << text.toStdString();

    taglist.isSaved = true;
    txtfile_write.close();
}
