#ifndef TYPE_SEARCH_H
#define TYPE_SEARCH_H
#include <QApplication>
#include <QElapsedTimer>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QListView>
#include <QString>
#include <functional>

/**
 * @brief The state behind "type a few letters, jump to the tag they start with".
 *
 * Both tag lists cut Qt's own keyboard search off - each view overrides
 * keyPressEvent() to handle its own shortcuts and never calls the base class,
 * and QAbstractItemView::keyboardSearch() could not work on the multi tag list
 * anyway, because its Qt::DisplayRole carries the tag object rather than a
 * string. So the search is done here, over the rows the view is showing.
 *
 * Qt's rules are kept, because they are what a user expects from a list:
 *  - the typed characters only accumulate while they keep arriving faster than
 *    QApplication::keyboardInputInterval(), so a pause starts a new search;
 *  - typing the same letter over and over cycles through the matches of that
 *    one letter instead of staying on the first one;
 *  - the row that is already current is stepped over when a search starts, so a
 *    search never "finds" the row it started from.
 *
 * The timeout in the first rule is the whole point of keeping this in one place:
 * a buffer that is never dropped is exactly what made the per-image tag list
 * stop finding anything after its first search.
 */
class teTypeSearch{
public:
    /// True when `event` is a plain printable key, i.e. something to search for
    /// rather than a shortcut. Ctrl/Alt/Meta combinations are never a search.
    static bool isSearchKey(QKeyEvent* event){
        if(event->modifiers()&(Qt::ControlModifier|Qt::AltModifier|Qt::MetaModifier))
            return false;
        const QString text=event->text();
        return !text.isEmpty()&&text.at(0).isPrint();
    }

    /**
     * @brief Feeds one typed chunk and moves the view to the matching row.
     *
     * @param view       the list to move; only its current index, its selection
     *                   and its scroll position are touched
     * @param rowCount   number of rows in the list
     * @param currentRow the row that is current, or -1
     * @param textAt     text of a row, used for the prefix match
     * @param typed      the characters `event` carried
     * @return true when a matching row was found (and made current).
     */
    bool search(QListView* view,int rowCount,int currentRow,
                const std::function<QString(int)>& textAt,const QString& typed){
        if(!view||rowCount<=0||typed.isEmpty())
            return false;

        bool fresh=true;
        if(time.isValid()){
            // restart() both reports the gap since the previous key and starts
            // the next one.
            if(time.restart()<=QApplication::keyboardInputInterval())
                fresh=false;
        }else{
            time.start();
        }
        if(fresh||buffer.isEmpty())
            buffer=typed;
        else
            buffer+=typed;
        if(buffer.isEmpty())
            return false;

        bool sameKey=buffer.size()>1;
        if(sameKey){
            for(const QChar c:buffer){
                if(c!=buffer.at(0)){
                    sameKey=false;
                    break;
                }
            }
        }

        int start=0;
        if(currentRow>=0){
            start=currentRow;
            if(fresh||sameKey)
                start=(start+1)%rowCount;
        }

        for(int offset=0;offset<rowCount;++offset){
            const int row=(start+offset)%rowCount;
            if(!textAt(row).startsWith(buffer,Qt::CaseInsensitive))
                continue;
            const QModelIndex index=view->model()->index(row,0);
            view->selectionModel()->setCurrentIndex(
                index,QItemSelectionModel::ClearAndSelect|QItemSelectionModel::Rows);
            view->scrollTo(index,QAbstractItemView::PositionAtCenter);
            return true;
        }
        return false;
    }

    /// Forgets what has been typed, so the next key starts a new search.
    void reset(){
        buffer.clear();
        time.invalidate();
    }

private:
    /// What has been typed since the last pause.
    QString buffer;
    /// When the last search key arrived.
    QElapsedTimer time;
};

#endif // TYPE_SEARCH_H
