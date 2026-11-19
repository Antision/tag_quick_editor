#ifndef PICTUREFILECAPTIONSOURCE_H
#define PICTUREFILECAPTIONSOURCE_H

#include "tagviewcontracts.h"
#include "teobject.h"
#include <QSet>

class tePictureFile;
class tePictureFileModel;

/**
 * @brief tqe's CaptionSource: one tePictureFile per image, as before (TV-03).
 *
 * The picture list owns the files, and a file owns its tag list together with
 * that picture's undo history. This adapter only translates a path into the
 * file the model already holds; it does not create or copy lists.
 */
class PictureFileCaptionSource : public CaptionSource, public teObject
{
public:
    explicit PictureFileCaptionSource(tePictureFileModel* model);
    ~PictureFileCaptionSource() override;

    teTagList* open(const QString& imagePath) override;
    bool save(const QString& imagePath,teTagList& list) override;
    QString captionPathFor(const QString& imagePath) const override;

private:
    tePictureFile* fileFor(const QString& imagePath) const;
    void onFileDestroyed(tePictureFile* file);

    tePictureFileModel* m_model=nullptr;
    /// Files this source listens to. A file may be opened again after another
    /// image was shown, so the connection is made once per file, not per open().
    QSet<tePictureFile*> m_watched;
};

#endif // PICTUREFILECAPTIONSOURCE_H
