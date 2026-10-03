// LayersPanel.cpp
#include "ui/LayersPanel.h"
#include "core/Document.h"
#include "ui/LayerTreeWidget.h"

#include <QBuffer>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QHelpEvent>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QStyle>
#include <QToolButton>
#include <QToolTip>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>

LayersPanel::LayersPanel(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);

    m_tree = new LayerTreeWidget(this);
    m_tree->header()->setStretchLastSection(false);
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_tree->viewport()->installEventFilter(this);
    layout->addWidget(m_tree, 1);

    auto* opacityRow = new QHBoxLayout;
    opacityRow->addWidget(new QLabel(tr("Opacidad:"), this));
    m_opacity = new QSlider(Qt::Horizontal, this);
    m_opacity->setRange(0, 100);
    m_opacity->setValue(100);
    m_opacityLabel = new QLabel("100%", this);
    m_opacityLabel->setMinimumWidth(36);
    opacityRow->addWidget(m_opacity, 1);
    opacityRow->addWidget(m_opacityLabel);
    layout->addLayout(opacityRow);

    auto* buttons = new QHBoxLayout;
    auto* addBtn = new QPushButton(tr("+ Capa"), this);
    auto* folderBtn = new QPushButton(tr("+ Carpeta"), this);
    auto* delBtn = new QPushButton(tr("Borrar"), this);
    auto* upBtn = new QPushButton(QStringLiteral("▲"), this);
    auto* downBtn = new QPushButton(QStringLiteral("▼"), this);
    upBtn->setToolTip(tr("Subir capa"));
    downBtn->setToolTip(tr("Bajar capa"));
    for (auto* b : {addBtn, folderBtn, delBtn, upBtn, downBtn}) buttons->addWidget(b);
    layout->addLayout(buttons);

    connect(addBtn, &QPushButton::clicked, this, [this] { if (m_document) m_document->addLayer(); });
    connect(folderBtn, &QPushButton::clicked, this, [this] { if (m_document) m_document->addGroup(); });
    connect(delBtn, &QPushButton::clicked, this, [this] {
        if (m_document) m_document->removeLayer(currentLayerId());
    });
    connect(upBtn, &QPushButton::clicked, this, [this] {
        if (m_document) m_document->moveLayer(currentLayerId(), +1);
    });
    connect(downBtn, &QPushButton::clicked, this, [this] {
        if (m_document) m_document->moveLayer(currentLayerId(), -1);
    });

    connect(m_tree, &QTreeWidget::currentItemChanged, this,
            [this](QTreeWidgetItem* cur) { onCurrentItemChanged(cur); });
    connect(m_tree, &QTreeWidget::itemChanged, this, &LayersPanel::onItemChanged);
    connect(m_tree, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem* it, int col) {
        if (col == 0) m_tree->editItem(it, 0);
    });
    auto setExpandedFlag = [this](QTreeWidgetItem* it, bool expanded) {
        if (m_updating || !m_document) return;
        if (Layer* l = m_document->layerById(it->data(0, Qt::UserRole).toInt())) l->setExpanded(expanded);
    };
    connect(m_tree, &QTreeWidget::itemExpanded, this, [=](QTreeWidgetItem* it) { setExpandedFlag(it, true); });
    connect(m_tree, &QTreeWidget::itemCollapsed, this, [=](QTreeWidgetItem* it) { setExpandedFlag(it, false); });
    // En cola: la lista se reconstruye cuando el arrastre ya termino.
    connect(m_tree, &LayerTreeWidget::layerDropped, this, &LayersPanel::onLayerDropped, Qt::QueuedConnection);
    connect(m_opacity, &QSlider::valueChanged, this, &LayersPanel::onOpacityChanged);
}

void LayersPanel::setDocument(Document* document) {
    if (m_document) m_document->disconnect(this);
    m_document = document;
    if (m_document) {
        connect(m_document, &Document::layerListChanged, this, [this] { rebuildList(); });
        connect(m_document, &Document::activeLayerChanged, this, &LayersPanel::onActiveLayerChanged);
        connect(m_document, &Document::layerPropertiesChanged, this, &LayersPanel::onLayerPropertiesChanged);
    }
    rebuildList();
}

Layer::Id LayersPanel::currentLayerId() const {
    const QTreeWidgetItem* item = m_tree->currentItem();
    return item ? item->data(0, Qt::UserRole).toInt() : Layer::InvalidId;
}

QTreeWidgetItem* LayersPanel::itemForLayer(Layer::Id id) const {
    for (QTreeWidgetItemIterator it(m_tree); *it; ++it)
        if ((*it)->data(0, Qt::UserRole).toInt() == id) return *it;
    return nullptr;
}

void LayersPanel::rebuildList() {
    m_updating = true;
    m_tree->clear();
    if (m_document) {
        addChildren(nullptr, Layer::InvalidId);
        m_tree->setCurrentItem(itemForLayer(m_document->activeLayerId()));
    }
    m_updating = false;
    syncOpacitySlider();
}

// La capa superior aparece arriba: recorremos la lista en orden inverso.
void LayersPanel::addChildren(QTreeWidgetItem* parentItem, Layer::Id parentId) {
    for (int i = m_document->layerCount() - 1; i >= 0; --i) {
        const Layer* layer = m_document->layerAt(i);
        if (layer->parentId() != parentId) continue;

        auto* item = parentItem ? new QTreeWidgetItem(parentItem) : new QTreeWidgetItem(m_tree);
        Qt::ItemFlags flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable |
                              Qt::ItemIsEditable | Qt::ItemIsDragEnabled;
        if (layer->isGroup()) flags |= Qt::ItemIsDropEnabled; // solo las carpetas admiten "dentro"
        item->setFlags(flags);
        item->setText(0, layer->name());
        item->setCheckState(0, layer->isVisible() ? Qt::Checked : Qt::Unchecked);
        item->setData(0, Qt::UserRole, layer->id());
        if (layer->isGroup()) {
            item->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
            QFont f = item->font(0);
            f.setBold(true);
            item->setFont(0, f);
        }

        // Boton "alpha" dentro de la fila
        auto* btn = new QToolButton(m_tree);
        btn->setText(QStringLiteral("α"));
        btn->setCheckable(true);
        btn->setChecked(layer->isClipped());
        btn->setAutoRaise(true);
        btn->setToolTip(tr("Bloquear alpha: limita la capa a lo que hay debajo"));
        btn->setStyleSheet(QStringLiteral("QToolButton:checked { background:#d9a520; color:black; }"));
        connect(btn, &QToolButton::toggled, this, [this, id = layer->id()](bool on) {
            if (!m_updating && m_document) m_document->setLayerClipped(id, on);
        });
        m_tree->setItemWidget(item, 1, btn);

        if (layer->isGroup()) {
            addChildren(item, layer->id());
            item->setExpanded(layer->isExpanded());
        }
    }
}

void LayersPanel::syncOpacitySlider() {
    const Layer* layer = m_document ? m_document->activeLayer() : nullptr;
    const int value = layer ? qRound(layer->opacity() * 100) : 100;
    m_updating = true;
    m_opacity->setValue(value);
    m_opacity->setEnabled(layer != nullptr);
    m_opacityLabel->setText(QStringLiteral("%1%").arg(value));
    m_updating = false;
}

void LayersPanel::onActiveLayerChanged(Layer::Id id) {
    m_updating = true;
    m_tree->setCurrentItem(itemForLayer(id));
    m_updating = false;
    syncOpacitySlider();
}

void LayersPanel::onLayerPropertiesChanged(Layer::Id id) {
    const Layer* layer = m_document ? m_document->layerById(id) : nullptr;
    QTreeWidgetItem* item = itemForLayer(id);
    if (!layer || !item) return;
    // "In situ": reconstruir aqui destruiria el item que puede estar emitiendo itemChanged.
    m_updating = true;
    item->setText(0, layer->name());
    item->setCheckState(0, layer->isVisible() ? Qt::Checked : Qt::Unchecked);
    if (auto* btn = qobject_cast<QToolButton*>(m_tree->itemWidget(item, 1))) {
        const QSignalBlocker b(btn);
        btn->setChecked(layer->isClipped());
    }
    m_updating = false;
    if (id == m_document->activeLayerId()) syncOpacitySlider();
}

void LayersPanel::onCurrentItemChanged(QTreeWidgetItem* current) {
    if (m_updating || !m_document || !current) return;
    m_document->setActiveLayer(current->data(0, Qt::UserRole).toInt());
    syncOpacitySlider();
}

void LayersPanel::onItemChanged(QTreeWidgetItem* item, int column) {
    if (m_updating || !m_document || column != 0) return;
    const Layer::Id id = item->data(0, Qt::UserRole).toInt();
    m_document->setLayerVisible(id, item->checkState(0) == Qt::Checked);
    m_document->setLayerName(id, item->text(0).trimmed());
    onLayerPropertiesChanged(id); // restaura el texto si el nombre nuevo era invalido
}

void LayersPanel::onOpacityChanged(int value) {
    m_opacityLabel->setText(QStringLiteral("%1%").arg(value));
    if (m_updating || !m_document) return;
    m_document->setLayerOpacity(m_document->activeLayerId(), value / 100.0);
}

void LayersPanel::onLayerDropped(int draggedId, int targetId, int where) {
    if (!m_document) return;
    if (targetId == Layer::InvalidId) { // soltar en zona vacia: a la raiz, arriba del todo
        m_document->moveLayerTo(draggedId, m_document->layerCount() - 1, Layer::InvalidId);
        return;
    }
    m_document->moveLayerRelative(draggedId, targetId, static_cast<Document::LayerDrop>(where));
}

// Miniatura al pasar el raton por una capa
bool LayersPanel::eventFilter(QObject* obj, QEvent* event) {
    if (obj == m_tree->viewport() && event->type() == QEvent::ToolTip && m_document) {
        const auto* he = static_cast<QHelpEvent*>(event);
        const QTreeWidgetItem* item = m_tree->itemAt(he->pos());
        if (item) {
            const Layer::Id id = item->data(0, Qt::UserRole).toInt();
            const Layer* layer = m_document->layerById(id);
            const QImage preview = m_document->layerPreview(id, QSize(220, 150));
            if (layer && !preview.isNull()) {
                QByteArray png;
                QBuffer buffer(&png);
                buffer.open(QIODevice::WriteOnly);
                preview.save(&buffer, "PNG");
                QToolTip::showText(he->globalPos(),
                    QStringLiteral("<b>%1</b><br><img src=\"data:image/png;base64,%2\">")
                        .arg(layer->name().toHtmlEscaped(), QString::fromLatin1(png.toBase64())),
                    m_tree->viewport());
                return true;
            }
        }
        QToolTip::hideText();
        event->ignore();
        return true;
    }
    return QWidget::eventFilter(obj, event);
}