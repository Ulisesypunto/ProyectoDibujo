# Proyecto-3er-Trimestre — Programa de Dibujo

## Dependencias
- C++17
- Qt6 6.11.2 (Widgets / Gui / Core; Test opcional)
- CMake >= 3.16

Opcionales (futuro): OpenGL / GLEW / Glad, libwacom / WacomSDK, Eigen / GLM.

## Compilar
Windows: ejecutar `set-up-and-build.bat` (`/clean` para rehacer desde cero).
Manual:

    cmake -S ProyectoDibujo -B build -DCMAKE_PREFIX_PATH=<ruta a Qt>
    cmake --build build

Tests: `-DBUILD_TESTS=ON` y luego `ctest --test-dir build`.

## Arquitectura

    ProyectoDibujo/src
    ├── core/            Modelo. NO depende de la UI ni de las herramientas.
    │   ├── Layer        Capa (imagen + nombre/visibilidad/opacidad)
    │   ├── Document     Capas + capa activa + QUndoStack + render/flatten/colorAt
    │   └── commands/    QUndoCommand: StrokeCommand, Add/Remove/Move/Property de capa
    ├── tools/           Herramientas. Dependen de core, no de la UI.
    │   ├── Tool         Interfaz (eventos en coordenadas del documento)
    │   ├── PaintTool    Base de herramientas que pintan (gestiona trazo + undo)
    │   ├── Brush/Eraser/Eyedropper
    │   ├── ToolManager  Registro + herramienta activa + color + tamaño
    │   └── BuiltinTools Lista de herramientas registradas
    └── ui/              Widgets. Solo conectan piezas.
        ├── MainWindow, CanvasView (zoom/pan/eventos), LayersPanel, ToolOptionsWidget

Regla de dependencias: `ui → tools → core`. Nunca al revés.

### Cómo añadir una herramienta
1. Crear `tools/MiTool.h` heredando de `PaintTool` (pinta) o `Tool` (otra cosa).
2. Añadir `manager.registerTool(std::make_unique<MiTool>());` en `BuiltinTools.cpp`.
Aparece sola en la barra, con su atajo. CMake detecta los archivos nuevos (`GLOB CONFIGURE_DEPENDS`).

### Cómo añadir una operación sobre capas / documento
Crear un `QUndoCommand` en `core/commands/` y exponerlo desde `Document`.
Así todo entra en undo/redo automáticamente.

## Controles
B pincel · E goma · I gotero · `[` `]` tamaño · rueda: zoom · botón central o Espacio+arrastrar: mover vista ·
Ctrl+Z / Ctrl+Y (o Ctrl+Shift+Z) · Ctrl+0 ajustar · Ctrl+1 100%

## Hoja de ruta sugerida
Cubo de relleno, líneas/formas, selección + mover, modos de fusión, presión de tableta (`ToolEvent::pressure`
ya existe), formato de proyecto con capas (.zip/.json + PNGs), pinceles con dureza/espaciado, filtros.
