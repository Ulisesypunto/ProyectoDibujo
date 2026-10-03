#pragma once
#include <QPointF>
#include <QPointer>
#include <QTransform>
#include <QWidget>

#include "core/Document.h"
class ToolManager;
class Tool;
class QEnterEvent;

// Vista del documento: zoom, desplazamiento, espejo y traduccion de eventos del
// raton a coordenadas del documento. NO dibuja ni edita: delega en la Tool activa.
class CanvasView : public QWidget {
    Q_OBJECT
public:
    explicit CanvasView(ToolManager* tools, QWidget* parent = nullptr);

    void setDocument(Document* document);
    qreal zoom() const { return m_zoom; }
    bool isMirrored() const { return m_mirrored; }
    QPointF viewCenterInDocument() const { return toDocument(QPointF(width() / 2.0, height() / 2.0)); }

public slots:
    void zoomIn();
    void zoomOut();
    void zoomActualSize();
    void zoomToFit();
    void setMirrored(bool mirrored); // volteo horizontal de la VISTA (no toca el dibujo)
    void commitTool();
    void setZoomLevel(qreal zoom);

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

    QTransform viewTransform() const; // documento -> widget (zoom, origen y espejo)
    QPointF toDocument(const QPointF& widgetPos) const;
    QPointF toWidget(const QPointF& docPos) const;
    void shiftOrigin(const QPointF& widgetDelta); // mueve la imagen tal como se ve en pantalla
    void setZoom(qreal zoom, const QPointF& anchor);
    void applyPendingFit();
    void updateCursor();
    void onContentChanged(const QRect& dirty);
    void switchToolHooks(Tool* now);
    void scheduleToolContextChanged();
    void notifyToolContextChanged();

    Tool* m_hookedTool = nullptr;   // herramienta que ya recibio activated()
    bool m_contextPending = false;

    QPointer<Document> m_document;
    ToolManager* m_tools;

    qreal m_zoom = 1.0;
    QPointF m_origin{20, 20}; // posicion del (0,0) del documento (antes del espejo)
    bool m_fitPending = true;
    bool m_mirrored = false;

    bool m_drawing = false;
    bool m_panning = false;
    bool m_spaceDown = false;
    QPointF m_lastPanPos;

    QPointF m_hover;
    bool m_hasHover = false;
};