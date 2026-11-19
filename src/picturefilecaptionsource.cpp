#include "picturefilecaptionsource.h"
#include "tepicturelistview.h"

PictureFileCaptionSource::PictureFileCaptionSource(tePictureFileModel* model)
    :m_model(model)
{
}

PictureFileCaptionSource::~PictureFileCaptionSource()
{
    // The files outlive this source (the picture list owns them), so the
    // connections have to be dropped from this side.
    if(m_model)
        for(tePictureFile* file:m_watched)
            file->teDisconnect(this);
}

tePictureFile* PictureFileCaptionSource::fileFor(const QString& imagePath) const
{
    if(!m_model)
        return nullptr;
    for(tePictureFile* file:m_model->picturefiles)
        if(file&&file->filepath.qstring==imagePath)
            return file;
    return nullptr;
}

teTagList* PictureFileCaptionSource::open(const QString& imagePath)
{
    tePictureFile* file=fileFor(imagePath);
    if(!file)
        return nullptr;
    // Only the file being shown has to be watched. Keeping a connection per
    // opened image would accumulate over a session (one per picture visited),
    // and the view only ever shows one list anyway.
    for(auto it=m_watched.begin();it!=m_watched.end();){
        if(*it!=file){
            (*it)->teDisconnect(this);
            it=m_watched.erase(it);
        }else{
            ++it;
        }
    }
    if(!m_watched.contains(file)){
        // The view shows this file's list by pointer, so losing the file has to
        // reach the view before the list is gone (see setListGoneHandler()).
        file->teConnect(teCallbackType::destroy,this,&PictureFileCaptionSource::onFileDestroyed,file);
        m_watched.insert(file);
    }
    return &file->taglist;
}

bool PictureFileCaptionSource::save(const QString& imagePath,teTagList& list)
{
    tePictureFile* file=fileFor(imagePath);
    if(!file||&file->taglist!=&list)
        return false;
    file->save();
    return file->taglist.isSaved;
}

QString PictureFileCaptionSource::captionPathFor(const QString& imagePath) const
{
    const QFileInfo info(imagePath);
    return info.absolutePath()+QLatin1Char('/')+info.completeBaseName()+QStringLiteral(".txt");
}

void PictureFileCaptionSource::onFileDestroyed(tePictureFile* file)
{
    m_watched.remove(file);
    // During ~tePictureFile: the tag list is still alive here, but it is about
    // to die, so the handler only drops the pointer.
    notifyListGone(&file->taglist);
}
