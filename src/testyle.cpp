#include "testyle.h"

#include <QFile>
#include <QHash>

namespace teStyle {

const QString& sheet(const QString& name)
{
    // One read per file, ever. The caller keeps getting the same QString, so
    // setStyleSheet() can be skipped with a pointer/std::cref comparison.
    static QHash<QString,QString> cache;
    auto it = cache.find(name);
    if(it != cache.end())
        return it.value();

    QString body;
    QFile file(QStringLiteral(":/style/") + name + QStringLiteral(".qss"));
    if(file.open(QIODevice::ReadOnly | QIODevice::Text))
        body = QString::fromUtf8(file.readAll());
    // A missing sheet is a build error, not a run-time style: say so once and
    // carry on with an empty sheet instead of crashing in the middle of layout.
    else
        qWarning("teStyle: no stylesheet named \"%s\" in the resources", qPrintable(name));

    return *cache.insert(name,body);
}

QString list(int wordFontSizePt)
{
    // The scrollbar block used to live inside list.qss; it is a separate file now
    // so the main window can take just that part (see scrollbar()).
    return QString(sheet(QStringLiteral("scrollbar")) + sheet(QStringLiteral("list")))
        .arg(wordFontSizePt);
}

QString scrollbar()
{
    return sheet(QStringLiteral("scrollbar"));
}

QString controls()
{
    // Secondary rules last: the sheet relies on "later wins" for the :hover /
    // :pressed overrides of the generic QPushButton rules.
    return sheet(QStringLiteral("controls")) + sheet(QStringLiteral("secondarybutton"));
}

} // namespace teStyle
