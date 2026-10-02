#pragma once
#include <QPointF>
#include <QPointer>
#include <QWidget>

#include "core/Document.h"
class ToolManager;
class QEnterEvent;

// Vista del documento: zoom, desplazamiento y traduccion de eventos del
// raton a coordenadas del documento. NO dibuja ni edita: delega en la Tool activa.
class CanvasView : public QWidget {
    Q_OBJECT
public:
    explicit CanvasView(ToolManager* tools, QWidget* parent = nullptr);

    void setDocument(Document* document);
    qreal zoom() const { return m_zoom; }
    QPointF viewCenterInDocument() const { return toDocument(QPointF(width() / 2.0, height() / 2.0)); }

public slots:
    void zoomIn();
    void zoomOut();
    void zoomActualSize();
    void zoomToFit();

signals:
    void zoomChanged(qreal zoom);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    static constexpr qreal MinZoom = 0.05;
    static constexpr qreal MaxZoom = 32.0;

    QPointF toDocument(const QPointF& widgetPos) const { return (widgetPos - m_origin) / m_zoom; }
    QPointF toWidget(const QPointF& docPos) const { return docPos * m_zoom + m_origin; }
    void setZoom(qreal zoom, const QPointF& anchor);
    void applyPendingFit();
    void updateCursor();
    void onContentChanged(const QRect& dirty);

    QPointer<Document> m_document;
    ToolManager* m_tools;

    qreal m_zoom = 1.0;
    QPointF m_origin{20, 20}; // posicion del (0,0) del documento en el widget
    bool m_fitPending = true;

    bool m_drawing = false;
    bool m_panning = false;
    bool m_spaceDown = false;
    QPointF m_lastPanPos;

    QPointF m_hover;
    bool m_hasHover = false;
};
