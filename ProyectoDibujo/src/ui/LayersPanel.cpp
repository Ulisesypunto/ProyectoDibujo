#include "ui/LayersPanel.h"
#include "core/Document.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>

LayersPanel::LayersPanel(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);

    m_list = new QListWidget(this);
    layout->addWidget(m_list, 1);

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
    auto* delBtn = new QPushButton(tr("Borrar"), this);
    auto* upBtn = new QPushButton(QStringLiteral("▲"), this);
    auto* downBtn = new QPushButton(QStringLiteral("▼"), this);
    upBtn->setToolTip(tr("Subir capa"));
    downBtn->setToolTip(tr("Bajar capa"));
    for (auto* b : {addBtn, delBtn, upBtn, downBtn}) buttons->addWidget(b);
    layout->addLayout(buttons);

    connect(addBtn, &QPushButton::clicked, this, [this] { if (m_document) m_document->addLayer(); });
    connect(delBtn, &QPushButton::clicked, this, [this] {
        if (m_document) m_document->removeLayer(currentLayerId());
    });
    connect(upBtn, &QPushButton::clicked, this, [this] {
        if (m_document) m_document->moveLayer(currentLayerId(), +1);
    });
    connect(downBtn, &QPushButton::clicked, this, [this] {
        if (m_document) m_document->moveLayer(currentLayerId(), -1);
    });

    connect(m_list, &QListWidget::currentRowChanged, this, &LayersPanel::onCurrentRowChanged);
    connect(m_list, &QListWidget::itemChanged, this, &LayersPanel::onItemChanged);
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
    const QListWidgetItem* item = m_list->currentItem();
    return item ? item->data(Qt::UserRole).toInt() : Layer::InvalidId;
}

int LayersPanel::rowForLayer(Layer::Id id) const {
    for (int row = 0; row < m_list->count(); ++row)
        if (m_list->item(row)->data(Qt::UserRole).toInt() == id) return row;
    return -1;
}

void LayersPanel::rebuildList() {
    m_updating = true;
    m_list->clear();
    if (m_document) {
        // La capa superior aparece arriba: recorremos en orden inverso.
        for (int i = m_document->layerCount() - 1; i >= 0; --i) {
            const Layer* layer = m_document->layerAt(i);
            auto* item = new QListWidgetItem(layer->name());
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsEditable);
            item->setCheckState(layer->isVisible() ? Qt::Checked : Qt::Unchecked);
            item->setData(Qt::UserRole, layer->id());
            m_list->addItem(item);
        }
        m_list->setCurrentRow(rowForLayer(m_document->activeLayerId()));
    }
    m_updating = false;
    syncOpacitySlider();
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
    m_list->setCurrentRow(rowForLayer(id));
    m_updating = false;
    syncOpacitySlider();
}

void LayersPanel::onLayerPropertiesChanged(Layer::Id id) {
    const Layer* layer = m_document ? m_document->layerById(id) : nullptr;
    const int row = rowForLayer(id);
    if (!layer || row < 0) return;
    // Actualizacion "in situ": reconstruir la lista aqui destruiria el item
    // que puede estar emitiendo la senal itemChanged.
    m_updating = true;
    QListWidgetItem* item = m_list->item(row);
    item->setText(layer->name());
    item->setCheckState(layer->isVisible() ? Qt::Checked : Qt::Unchecked);
    m_updating = false;
    if (id == m_document->activeLayerId()) syncOpacitySlider();
}

void LayersPanel::onCurrentRowChanged(int row) {
    if (m_updating || !m_document || row < 0) return;
    m_document->setActiveLayer(m_list->item(row)->data(Qt::UserRole).toInt());
    syncOpacitySlider();
}

void LayersPanel::onItemChanged(QListWidgetItem* item) {
    if (m_updating || !m_document) return;
    const Layer::Id id = item->data(Qt::UserRole).toInt();
    m_document->setLayerVisible(id, item->checkState() == Qt::Checked);
    m_document->setLayerName(id, item->text().trimmed());
    onLayerPropertiesChanged(id); // restaura el texto si el nombre nuevo era invalido (vacio)
}

void LayersPanel::onOpacityChanged(int value) {
    m_opacityLabel->setText(QStringLiteral("%1%").arg(value));
    if (m_updating || !m_document) return;
    m_document->setLayerOpacity(m_document->activeLayerId(), value / 100.0);
}
