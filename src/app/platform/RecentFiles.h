#pragma once

#include <QObject>
#include <QStringList>

// The images opened or saved last, newest first, remembered between sessions (File ▸ Recent).
class RecentFiles : public QObject
{
    Q_OBJECT

public:
    static constexpr int Limit = 10;

    explicit RecentFiles(QObject *parent = nullptr);

    // The remembered files that still exist.
    QStringList files() const;
    void add(const QString &path);
    void clear();

Q_SIGNALS:
    void changed();

private:
    void store(const QStringList &files);
};
