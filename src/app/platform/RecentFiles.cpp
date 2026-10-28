#include "platform/RecentFiles.h"

#include <QFileInfo>
#include <QSettings>

namespace {

constexpr QLatin1StringView Key("recentFiles");

} // namespace

RecentFiles::RecentFiles(QObject *parent)
    : QObject(parent)
{}

QStringList RecentFiles::files() const
{
    QStringList existing;
    for (const QString &path : QSettings().value(Key).toStringList()) {
        if (QFileInfo::exists(path)) {
            existing.append(path);
        }
    }
    return existing;
}

void RecentFiles::add(const QString &path)
{
    const QString absolute = QFileInfo(path).absoluteFilePath();
    QStringList list = QSettings().value(Key).toStringList();
    list.removeAll(absolute);
    list.prepend(absolute);
    store(list.mid(0, Limit));
}

void RecentFiles::clear()
{
    store({});
}

void RecentFiles::store(const QStringList &files)
{
    QSettings().setValue(Key, files);
    Q_EMIT changed();
}
