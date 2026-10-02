#include "ui/ShortcutManager.h"

#include <QAction>
#include <QSettings>
#include <QStringList>

namespace {
const QString Group = QStringLiteral("shortcuts");
const QChar Separator = QLatin1Char('\t'); // no puede aparecer en el texto de una tecla

QString encode(const QList<QKeySequence>& list) {
    QStringList parts;
    for (const QKeySequence& s : list) parts << s.toString(QKeySequence::PortableText);
    return parts.join(Separator);
}

QList<QKeySequence> decode(const QString& text) {
    QList<QKeySequence> out;
    for (const QString& part : text.split(Separator, Qt::SkipEmptyParts)) {
        const QKeySequence s = QKeySequence::fromString(part, QKeySequence::PortableText);
        if (!s.isEmpty()) out << s;
    }
    return out;
}
} // namespace

ShortcutManager::Entry* ShortcutManager::find(const QString& id) {
    for (Entry& e : m_entries)
        if (e.id == id) return &e;
    return nullptr;
}

const ShortcutManager::Entry* ShortcutManager::find(const QString& id) const {
    for (const Entry& e : m_entries)
        if (e.id == id) return &e;
    return nullptr;
}

void ShortcutManager::registerAction(const QString& id, const QString& label, QAction* action) {
    if (!action || find(id)) return;
    Entry e;
    e.id = id;
    e.label = label;
    e.action = action;
    e.defaults = action->shortcuts();
    m_entries.push_back(e);
}

void ShortcutManager::loadFromSettings() {
    QSettings settings;
    settings.beginGroup(Group);
    for (Entry& e : m_entries) {
        if (!e.action || !settings.contains(e.id)) continue;
        e.action->setShortcuts(decode(settings.value(e.id).toString()));
    }
    settings.endGroup();
    emit shortcutsChanged();
}

QKeySequence ShortcutManager::sequence(const QString& id) const {
    const Entry* e = find(id);
    return (e && e->action) ? e->action->shortcut() : QKeySequence();
}

QKeySequence ShortcutManager::defaultSequence(const QString& id) const {
    const Entry* e = find(id);
    return e ? e->defaults.value(0) : QKeySequence();
}

void ShortcutManager::setSequence(const QString& id, const QKeySequence& sequence) {
    Entry* e = find(id);
    if (!e || !e->action) return;

    QSettings settings;
    settings.beginGroup(Group);
    if (sequence == e->defaults.value(0)) { // vuelve al valor de fabrica (incluye alternativos)
        e->action->setShortcuts(e->defaults);
        settings.remove(id);
    } else {
        QList<QKeySequence> list;
        if (!sequence.isEmpty()) list << sequence;
        e->action->setShortcuts(list);
        settings.setValue(id, encode(list));
    }
    settings.endGroup();
    emit shortcutsChanged();
}

void ShortcutManager::resetAll() {
    for (Entry& e : m_entries)
        if (e.action) e.action->setShortcuts(e.defaults);
    QSettings settings;
    settings.remove(Group);
    emit shortcutsChanged();
}
