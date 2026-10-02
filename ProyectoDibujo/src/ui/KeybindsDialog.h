#pragma once
#include <QDialog>
#include <QKeySequence>
#include <vector>

class QKeySequenceEdit;
class QTableWidget;
class ShortcutManager;

// Editor de atajos: trabaja sobre una copia y solo aplica al pulsar Aceptar.
// Detecta conflictos y ofrece reasignar.
class KeybindsDialog : public QDialog {
    Q_OBJECT
public:
    explicit KeybindsDialog(ShortcutManager* manager, QWidget* parent = nullptr);

    void accept() override;

private:
    void onEdited(int row);
    void resetRow(int row);
    void resetAllRows();
    void filterRows(const QString& text);
    void setEdit(int row, const QKeySequence& sequence);

    ShortcutManager* m_manager;
    QTableWidget* m_table;
    std::vector<QKeySequenceEdit*> m_edits;
    std::vector<QKeySequence> m_values; // copia de trabajo (indice = fila = entrada)
};
