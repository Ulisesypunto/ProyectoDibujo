#pragma once
#include <QKeySequence>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <vector>

class QAction;

// Registro de acciones con atajo editable. Guarda SOLO las diferencias respecto
// a los atajos por defecto en QSettings (grupo "shortcuts"), asi que cambiar un
// valor por defecto en el codigo sigue funcionando para quien no lo ha tocado.
class ShortcutManager : public QObject {
    Q_OBJECT
public:
    struct Entry {
        QString id;                  // estable: "file.save", "tool.brush"...
        QString label;               // texto para el usuario
        QPointer<QAction> action;
        QList<QKeySequence> defaults;
    };

    explicit ShortcutManager(QObject* parent = nullptr) : QObject(parent) {}

    // Llamar DESPUES de dar a la accion su atajo por defecto.
    void registerAction(const QString& id, const QString& label, QAction* action);
    void loadFromSettings(); // aplica los atajos personalizados guardados

    const std::vector<Entry>& entries() const { return m_entries; }
    QKeySequence sequence(const QString& id) const;        // atajo actual (principal)
    QKeySequence defaultSequence(const QString& id) const;
    void setSequence(const QString& id, const QKeySequence& sequence); // aplica y persiste
    void resetAll();

signals:
    void shortcutsChanged();

private:
    Entry* find(const QString& id);
    const Entry* find(const QString& id) const;

    std::vector<Entry> m_entries;
};
