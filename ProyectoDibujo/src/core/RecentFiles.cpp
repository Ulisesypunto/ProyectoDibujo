#include "core/RecentFiles.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

namespace {
const QString Key = QStringLiteral("recent/files");
}

RecentFiles::RecentFiles(QObject* parent) : QObject(parent) { load(); }

QString RecentFiles::normalized(const QString& path) {
    return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
}

QString RecentFiles::thumbnailPath(const QString& path) const {
    const QByteArray hash =
        QCryptographicHash::hash(normalized(path).toUtf8(), QCryptographicHash::Sha1).toHex();
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
           QStringLiteral("/thumbs/") + QString::fromLatin1(hash) + QStringLiteral(".png");
}

void RecentFiles::load() {
    QSettings settings;
    m_files = settings.value(Key).toStringList();
    m_files.removeDuplicates();
    while (m_files.size() > MaxEntries) m_files.removeLast();
}

void RecentFiles::save() const {
    QSettings settings;
    settings.setValue(Key, m_files);
}

void RecentFiles::removeThumbnail(const QString& path) const { QFile::remove(thumbnailPath(path)); }

void RecentFiles::add(const QString& path, const QImage& thumbnail) {
    const QString p = normalized(path);
    m_files.removeAll(p);
    m_files.prepend(p);
    while (m_files.size() > MaxEntries) removeThumbnail(m_files.takeLast());

    if (!thumbnail.isNull()) {
        const QString thumbPath = thumbnailPath(p);
        QDir().mkpath(QFileInfo(thumbPath).absolutePath());
        thumbnail.save(thumbPath, "PNG");
    }
    save();
    emit changed();
}

void RecentFiles::remove(const QString& path) {
    const QString p = normalized(path);
    if (!m_files.removeAll(p)) return;
    removeThumbnail(p);
    save();
    emit changed();
}

void RecentFiles::clear() {
    for (const QString& p : std::as_const(m_files)) removeThumbnail(p);
    m_files.clear();
    save();
    emit changed();
}
