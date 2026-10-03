#pragma once
#include <QImage>
#include <QObject>
#include <QStringList>

// Lista de dibujos abiertos/guardados recientemente (persistida en QSettings)
// + cache de miniaturas en AppDataLocation/thumbs. No conoce la UI.
class RecentFiles : public QObject {
    Q_OBJECT
public:
    static constexpr int MaxEntries = 24;

    explicit RecentFiles(QObject* parent = nullptr);

    const QStringList& files() const { return m_files; } // el mas reciente primero
    void add(const QString& path, const QImage& thumbnail = QImage());
    void remove(const QString& path);
    void clear();
    QString thumbnailPath(const QString& path) const;

signals:
    void changed();

private:
    void load();
    void save() const;
    void removeThumbnail(const QString& path) const;
    static QString normalized(const QString& path);

    QStringList m_files;
};
