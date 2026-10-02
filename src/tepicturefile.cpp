#include "tepicturefile.h"
#include "func.h"
#include <QSaveFile>

namespace {

/// Size that keeps the aspect ratio of `original` while covering `refered_pixels`.
QSize previewSize(const QSize& original, int refered_pixels)
{
    if (original.width() <= 0 || original.height() <= 0)
        return {};
    const double ratio = double(original.width()) / double(original.height());
    int w = int(std::lround(std::sqrt(double(refered_pixels) * ratio)));
    int h = int(std::lround(std::sqrt(double(refered_pixels) / ratio)));
    // Never up-scale, never ask a plugin for a zero-sized image.
    w = std::clamp(w, 1, original.width());
    h = std::clamp(h, 1, original.height());
    return {w, h};
}

/// Decodes a down-scaled preview of `path`.
QImage decodeThumbnail(const QString& path, int refered_pixels)
{
    QImageReader reader(path);
    reader.setAutoTransform(true);

    const QSize target = previewSize(reader.size(), refered_pixels);
    if (target.isValid())
        reader.setScaledSize(target);

    QImage image = reader.read();
    if (!image.isNull())
        return image;

    // Fallback: some plugins report a bogus size() (or cannot do a scaled read)
    // and then fail the first attempt. Read unscaled and scale here instead.
    // This is why a few images used to stay blank in the picture list even
    // though the image viewer could display them.
    QImageReader retry(path);
    retry.setAutoTransform(true);
    image = retry.read();
    if (image.isNull())
        return image;
    const QSize scaled = previewSize(image.size(), refered_pixels);
    if (scaled.isValid() && (scaled.width() < image.width() || scaled.height() < image.height()))
        image = image.scaled(scaled, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    return image;
}

}

tePictureFile::tePictureFile(const tepath& input_filepath, int r)
    : filepath(input_filepath), refered_pixels(r*r)
{
    image_slot = std::make_shared<ImageSlot>();
    load_token = std::make_shared<LoadToken>();
    load_token->slot = image_slot;

    const int ret = openpath();
    if (ret == -1) {
        // Not an error: the image simply has no caption file yet.
    } else if (ret == -2) {
        telog("Could not open picture file");
    }
}

tePictureFile::~tePictureFile()
{
    cancelLoading();
    onDestroy();
}

void tePictureFile::cancelLoading()
{
    if (load_token)
        load_token->alive = false;
    if (image_slot)
        image_slot->cancelled = true;
}

QString tePictureFile::tagFilePath() const
{
    const QFileInfo info(filepath.qstring);
    return info.absolutePath() + QLatin1Char('/') + info.completeBaseName() + QStringLiteral(".txt");
}

int tePictureFile::openpath(std::string tagfile_extension)
{
    const QFileInfo fileInfo(filepath.qstring);
    if (!fileInfo.exists()) {
        telog(QString("Couldn't test the existence of file %1").arg(filepath.qstring));
        return -1;
    }

    startLoadPicture();

    const QString tagfilePath = fileInfo.absolutePath() + QLatin1Char('/')
                                + fileInfo.completeBaseName() + QString::fromStdString(tagfile_extension);

    QFile txtfile(tagfilePath);
    if (!txtfile.exists()) {
        // A missing caption file is perfectly normal for a fresh dataset.
        taglist.isSaved = true;
        return -1;
    }
    if (!txtfile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        telog(QString("[tePictureFile::openpath] found %1 but could not open it").arg(tagfilePath));
        return -2;
    }

    QTextStream in(&txtfile);
    in.setEncoding(QStringConverter::Utf8);
    const QString content = in.readAll();
    txtfile.close();

    const auto pieces = splitTextToPieces(content);
    for (const auto& piece : pieces) {
        if (!piece.text.isEmpty())
            taglist.initialize_push_back(piece.text, piece.sentence);
    }

    taglist.isTagsLoaded = true;
    // Loading is not editing: without this every file counted as modified, so
    // the "save changes?" prompt and the auto-save rewrote the whole dataset.
    taglist.isSaved = true;
    return 0;
}

void tePictureFile::startLoadPicture()
{
    auto token = load_token;
    // `file` is captured by value: the outer lambda must not capture `this`,
    // because it may run after this tePictureFile is gone.
    thread_pool.detach_task([token, path = filepath.qstring, rp = refered_pixels, file = this] {
        if (token->slot->cancelled)
            return;

        QImage image = decodeThumbnail(path, rp);

        if (token->slot->cancelled)
            return;
        {
            std::lock_guard<std::mutex> lg(token->slot->mtx);
            if (!token->slot->cancelled)
                token->slot->image = std::move(image);
        }

        if (!token->alive)
            return;

        // Deliver on the GUI thread. `alive` is only ever written by the
        // destructor, which runs on the GUI thread as well, so this check is
        // race free even if the tePictureFile died while decoding.
        QMetaObject::invokeMethod(qApp, [token, file] {
            if (token->alive)
                file->notifyPictureLoaded();
        }, Qt::QueuedConnection);
    });
}

QImage tePictureFile::thumbnail() const
{
    if (!image_slot)
        return {};
    std::lock_guard<std::mutex> lg(image_slot->mtx);
    return image_slot->image;
}

bool tePictureFile::hasThumbnail() const
{
    if (!image_slot)
        return false;
    std::lock_guard<std::mutex> lg(image_slot->mtx);
    return !image_slot->image.isNull();
}

void tePictureFile::notifyPictureLoaded()
{
    teemit(teCallbackType::loading_finished);
}

void tePictureFile::save()
{
    if (taglist.isSaved)
        return;

    const QString text = serializePieces(taglist.size(), [&](int i) -> std::shared_ptr<tetagcore> {
        return taglist.tags[i];
    }, true);

    // QSaveFile writes to a temporary file and renames it into place, so a
    // crash or a full disk can never truncate an existing caption file.
    QSaveFile file(tagFilePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        telog(QString("[tePictureFile::save] could not open %1 for writing: %2")
                  .arg(tagFilePath(), file.errorString()));
        return;
    }
    file.write(text.toUtf8());
    if (!file.commit()) {
        telog(QString("[tePictureFile::save] could not commit %1: %2")
                  .arg(tagFilePath(), file.errorString()));
        return;
    }

    taglist.isSaved = true;
}
