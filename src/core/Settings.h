#pragma once

#include "core/Profile.h"

#include <QString>

class QJsonObject;

namespace core {

/// Réglages globaux de l'application (§4.2).
struct Settings {
    QString llamaServerPath;
    QString llamaCliPath;
    QString defaultModelsDir;
    int monitorIntervalMs = 1000;
    QString theme = QStringLiteral("dark");

    /// Chemin de l'exécutable correspondant au binaire demandé.
    QString executableFor(BinaryKind kind) const;

    static Settings fromJson(const QJsonObject& object);
    QJsonObject toJson() const;
};

} // namespace core
