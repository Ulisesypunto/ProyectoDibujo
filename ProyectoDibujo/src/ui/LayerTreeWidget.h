#pragma once
#include <QDropEvent>
#include <QTreeWidget>

// Arbol de capas. No deja que Qt mueva los items: solo avisa de la intencion
// y el Document (con undo) es quien cambia; la UI se reconstruye despues.
class LayerTreeWidget : public QTreeWidget {
    Q_OBJECT
public:
    explicit LayerTreeWidget(QWidget* parent = nullptr) : QTreeWidget(parent) {
        setColumnCount(2);
        setHeaderHidden(true);
        setIndentation(14);
        setSelectionMode(QAbstractItemView::SingleSelection);
        setExpandsOnDoubleClick(false);
        setEditTriggers(QAbstractItemView::EditKeyPressed);
        setDragEnabled(true);
        setAcceptDrops(true);
        setDropIndicatorShown(true);
        setDragDropMode(QAbstractItemView::InternalMove);
        setDefaultDropAction(Qt::MoveAction);
    }

signals:
    // where: 0 encima, 1 debajo, 2 dentro (carpeta). target = -1: zona vacia
    void layerDropped(int draggedId, int targetId, int where);

protected:
    void dropEvent(QDropEvent* e) override {
        const QTreeWidgetItem* dragged = currentItem();
        const QTreeWidgetItem* target = itemAt(e->position().toPoint());
        const DropIndicatorPosition pos = dropIndicatorPosition();

        // CopyAction: con MoveAction Qt borraria el item origen al terminar el arrastre.
        e->setDropAction(Qt::CopyAction);
        e->accept();
        setState(QAbstractItemView::NoState);
        viewport()->update();
        if (!dragged) return;

        const int draggedId = dragged->data(0, Qt::UserRole).toInt();
        if (!target || pos == OnViewport) { emit layerDropped(draggedId, -1, 0); return; }
        const int where = pos == AboveItem ? 0 : pos == BelowItem ? 1 : 2;
        emit layerDropped(draggedId, target->data(0, Qt::UserRole).toInt(), where);
    }
};