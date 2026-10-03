#pragma once
#include <QString>
#include <QStringList>
#include <memory>

#include "core/Document.h"

// Lectura/escritura de proyectos Krita (.kra) y derivados (.krz: mismo ZIP).
//
// Soportado: capas de pintura RGBA 8 bits (nombre, visibilidad, opacidad,
// desplazamiento). Los grupos se aplanan; mascaras, capas vectoriales, filtros
// y otros espacios de color se omiten con un aviso (si no queda ninguna capa
// legible se usa mergedimage.png).
namespace KraFormat {

constexpr int MaxSide = 8192;

struct LoadResult {
    std::unique_ptr<Document> document; // nulo si hubo error
    QString error;
    QStringList warnings;               // cosas omitidas (el documento si se cargo)
};

bool isProjectFile(const QString& path); // .kra / .krz
LoadResult load(const QString& path);
bool save(const Document& document, const QString& path, QString* error = nullptr);

} // namespace KraFormat
