#include "core/Settings.h"

#include <QJsonObject>

namespace core {

QString Settings::executableFor(BinaryKind kind) const
{
    return kind == BinaryKind::Cli ? llamaCliPath : llamaServerPath;
}

Settings Settings::fromJson(const QJsonObject& object)
{
    Settings settings;
    settings.llamaServerPath = object.value(QStringLiteral("llamaServerPath")).toString();
    settings.llamaCliPath = object.value(QStringLiteral("llamaCliPath")).toString();
    settings.defaultModelsDir = object.value(QStringLiteral("defaultModelsDir")).toString();
    settings.monitorIntervalMs = object.value(QStringLiteral("monitorIntervalMs")).toInt(1000);
    settings.theme = object.value(QStringLiteral("theme")).toString(QStringLiteral("dark"));

    // Un intervalle absurde rendrait l'application inutilisable : on borne.
    settings.monitorIntervalMs = qBound(200, settings.monitorIntervalMs, 10000);
    return settings;
}

QJsonObject Settings::toJson() const
{
    QJsonObject object;
    object.insert(QStringLiteral("llamaServerPath"), llamaServerPath);
    object.insert(QStringLiteral("llamaCliPath"), llamaCliPath);
    object.insert(QStringLiteral("defaultModelsDir"), defaultModelsDir);
    object.insert(QStringLiteral("monitorIntervalMs"), monitorIntervalMs);
    object.insert(QStringLiteral("theme"), theme);
    return object;
}

} // namespace core
