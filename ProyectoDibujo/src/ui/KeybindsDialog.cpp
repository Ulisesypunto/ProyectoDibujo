#include "ui/KeybindsDialog.h"
#include "ui/ShortcutManager.h"

#include <QDialogButtonBox>
#include <QHeaderView>
#include <QKeySequenceEdit>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QVBoxLayout>

KeybindsDialog::KeybindsDialog(ShortcutManager* manager, QWidget* parent)
    : QDialog(parent), m_manager(manager) {
    setWindowTitle(tr("Atajos de teclado"));
    resize(560, 520);

    auto* layout = new QVBoxLayout(this);

    auto* search = new QLineEdit(this);
    search->setPlaceholderText(tr("Buscar accion..."));
    search->setClearButtonEnabled(true);
    layout->addWidget(search);

    const auto& entries = m_manager->entries();
    m_table = new QTableWidget(int(entries.size()), 3, this);
    m_table->setHorizontalHeaderLabels({tr("Accion"), tr("Atajo"), QString()});
    m_table->verticalHeader()->hide();
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->setColumnWidth(1, 170);
    layout->addWidget(m_table, 1);

    for (int i = 0; i < int(entries.size()); ++i) {
        const auto& e = entries[size_t(i)];
        auto* item = new QTableWidgetItem(e.label);
        item->setFlags(Qt::ItemIsEnabled);
        m_table->setItem(i, 0, item);

        const QKeySequence current = m_manager->sequence(e.id);
        auto* edit = new QKeySequenceEdit(current, this);
        edit->setMaximumSequenceLength(1);
        edit->setClearButtonEnabled(true);
        m_table->setCellWidget(i, 1, edit);
        m_edits.push_back(edit);
        m_values.push_back(current);

        auto* reset = new QPushButton(tr("Restablecer"), this);
        m_table->setCellWidget(i, 2, reset);

        connect(edit, &QKeySequenceEdit::editingFinished, this, [this, i] { onEdited(i); });
        connect(reset, &QPushButton::clicked, this, [this, i] { resetRow(i); });
    }

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::RestoreDefaults, this);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, this,
            &KeybindsDialog::resetAllRows);
    connect(search, &QLineEdit::textChanged, this, &KeybindsDialog::filterRows);
}

void KeybindsDialog::setEdit(int row, const QKeySequence& sequence) {
    const QSignalBlocker blocker(m_edits[size_t(row)]);
    m_edits[size_t(row)]->setKeySequence(sequence);
}

void KeybindsDialog::onEdited(int row) {
    const QKeySequence seq = m_edits[size_t(row)]->keySequence();
    const auto& entries = m_manager->entries();

    if (!seq.isEmpty()) {
        for (int j = 0; j < int(m_values.size()); ++j) {
            if (j == row || m_values[size_t(j)] != seq) continue;
            const auto answer = QMessageBox::question(this, tr("Atajo en uso"),
                tr("%1 ya esta asignado a «%2».\n¿Reasignarlo a «%3»?")
                    .arg(seq.toString(QKeySequence::NativeText), entries[size_t(j)].label,
                         entries[size_t(row)].label));
            if (answer != QMessageBox::Yes) {
                setEdit(row, m_values[size_t(row)]); // deshacer el cambio
                return;
            }
            m_values[size_t(j)] = QKeySequence();
            setEdit(j, QKeySequence());
            break;
        }
    }
    m_values[size_t(row)] = seq;
}

void KeybindsDialog::resetRow(int row) {
    setEdit(row, m_manager->defaultSequence(m_manager->entries()[size_t(row)].id));
    onEdited(row); // misma comprobacion de conflictos
}

void KeybindsDialog::resetAllRows() {
    const auto& entries = m_manager->entries();
    for (int i = 0; i < int(entries.size()); ++i) {
        m_values[size_t(i)] = m_manager->defaultSequence(entries[size_t(i)].id);
        setEdit(i, m_values[size_t(i)]);
    }
}

void KeybindsDialog::filterRows(const QString& text) {
    for (int i = 0; i < m_table->rowCount(); ++i)
        m_table->setRowHidden(i, !text.isEmpty() &&
                                 !m_table->item(i, 0)->text().contains(text, Qt::CaseInsensitive));
}

void KeybindsDialog::accept() {
    const auto& entries = m_manager->entries();
    for (int i = 0; i < int(entries.size()); ++i)
        if (m_values[size_t(i)] != m_manager->sequence(entries[size_t(i)].id))
            m_manager->setSequence(entries[size_t(i)].id, m_values[size_t(i)]);
    QDialog::accept();
}
