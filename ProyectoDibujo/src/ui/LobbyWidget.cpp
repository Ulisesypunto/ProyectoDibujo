#include "ui/LobbyWidget.h"
#include "core/RecentFiles.h"

#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>

LobbyWidget::LobbyWidget(RecentFiles* recent, QWidget* parent) : QWidget(parent), m_recent(recent) {
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(32, 32, 32, 32);
    root->setSpacing(32);

    // ---- columna izquierda: acciones ----
    auto* left = new QVBoxLayout;
    auto* title = new QLabel(tr("<h1>Programa de Dibujo</h1>"), this);
    left->addWidget(title);

    auto* newBtn = new QPushButton(tr("Nuevo documento..."), this);
    auto* openBtn = new QPushButton(tr("Abrir..."), this);
    auto* importBtn = new QPushButton(tr("Importar imagen..."), this);
    auto* clearBtn = new QPushButton(tr("Vaciar recientes"), this);
    for (auto* b : {newBtn, openBtn, importBtn}) {
        b->setMinimumHeight(34);
        left->addWidget(b);
    }
    left->addSpacing(12);
    left->addWidget(clearBtn);
    left->addStretch(1);
    root->addLayout(left, 0);

    // ---- columna derecha: recientes ----
    auto* right = new QVBoxLayout;
    right->addWidget(new QLabel(tr("<h3>Recientes</h3>"), this));

    m_list = new QListWidget(this);
    m_list->setViewMode(QListView::IconMode);
    m_list->setIconSize(QSize(160, 110));
    m_list->setGridSize(QSize(190, 160));
    m_list->setResizeMode(QListView::Adjust);
    m_list->setMovement(QListView::Static);
    m_list->setWordWrap(true);
    m_list->setUniformItemSizes(true);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    right->addWidget(m_list, 1);

    m_empty = new QLabel(tr("Aun no hay dibujos recientes.\nCrea uno nuevo o abre un archivo."), this);
    m_empty->setAlignment(Qt::AlignCenter);
    right->addWidget(m_empty, 1);
    root->addLayout(right, 1);

    connect(newBtn, &QPushButton::clicked, this, &LobbyWidget::newRequested);
    connect(openBtn, &QPushButton::clicked, this, &LobbyWidget::openRequested);
    connect(importBtn, &QPushButton::clicked, this, &LobbyWidget::importRequested);
    connect(clearBtn, &QPushButton::clicked, m_recent, &RecentFiles::clear);

    connect(m_list, &QListWidget::itemActivated, this, [this](QListWidgetItem* item) {
        emit openFileRequested(item->data(Qt::UserRole).toString());
    });
    connect(m_list, &QListWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        const QListWidgetItem* item = m_list->itemAt(pos);
        if (!item) return;
        const QString path = item->data(Qt::UserRole).toString(); // copia: la lista se reconstruye
        QMenu menu(this);
        menu.addAction(tr("Abrir"), this, [this, path] { emit openFileRequested(path); });
        menu.addAction(tr("Quitar de la lista"), this, [this, path] { m_recent->remove(path); });
        menu.exec(m_list->viewport()->mapToGlobal(pos));
    });

    connect(m_recent, &RecentFiles::changed, this, &LobbyWidget::refresh);
    refresh();
}

void LobbyWidget::refresh() {
    m_list->clear();
    for (const QString& path : m_recent->files()) {
        QPixmap thumb(m_recent->thumbnailPath(path));
        if (thumb.isNull()) {
            thumb = QPixmap(160, 110);
            thumb.fill(QColor(70, 70, 74));
        }
        auto* item = new QListWidgetItem(QIcon(thumb), QFileInfo(path).fileName(), m_list);
        item->setToolTip(QDir::toNativeSeparators(path));
        item->setData(Qt::UserRole, path);
        if (!QFileInfo::exists(path)) item->setForeground(Qt::gray);
    }
    const bool empty = m_list->count() == 0;
    m_list->setVisible(!empty);
    m_empty->setVisible(empty);
}
