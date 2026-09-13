#include "core/Profile.h"

#include <QJsonObject>
#include <QUuid>

namespace core {
namespace {

constexpr QLatin1StringView kBinaryServer("server");
constexpr QLatin1StringView kBinaryCli("cli");

QDateTime readDateTime(const QJsonObject& object, const QString& key)
{
    const QDateTime parsed = QDateTime::fromString(object.value(key).toString(), Qt::ISODate);
    return parsed.isValid() ? parsed.toUTC() : QDateTime();
}

} // namespace

QString binaryKindToString(BinaryKind kind)
{
    return kind == BinaryKind::Cli ? QString(kBinaryCli) : QString(kBinaryServer);
}

std::optional<BinaryKind> binaryKindFromString(QStringView text)
{
    if (text == kBinaryServer)
        return BinaryKind::Server;
    if (text == kBinaryCli)
        return BinaryKind::Cli;
    return std::nullopt;
}

Profile Profile::createNew()
{
    Profile profile;
    profile.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    profile.createdAt = QDateTime::currentDateTimeUtc();
    return profile;
}

Profile Profile::fromJson(const QJsonObject& object)
{
    Profile profile;
    profile.id = object.value(QStringLiteral("id")).toString();
    profile.name = object.value(QStringLiteral("name")).toString();
    profile.modelPath = object.value(QStringLiteral("modelPath")).toString();
    profile.binary = binaryKindFromString(object.value(QStringLiteral("binary")).toString())
                         .value_or(BinaryKind::Server);
    profile.extraArgs = object.value(QStringLiteral("extraArgs")).toString();
    profile.notes = object.value(QStringLiteral("notes")).toString();
    profile.createdAt = readDateTime(object, QStringLiteral("createdAt"));
    profile.lastUsedAt = readDateTime(object, QStringLiteral("lastUsedAt"));

    const QJsonObject params = object.value(QStringLiteral("params")).toObject();
    for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
        // Les valeurs sont stockées en texte : c'est params.json qui porte le type,
        // et la commande est de toute façon une suite de chaînes.
        const QJsonValue value = it.value();
        if (value.isString())
            profile.params.insert(it.key(), value.toString());
        else if (value.isBool())
            profile.params.insert(it.key(), value.toBool() ? QStringLiteral("true") : QStringLiteral("false"));
        else if (value.isDouble())
            profile.params.insert(it.key(), QString::number(value.toDouble(), 'g', 12));
    }

    // Un profil écrit à la main peut arriver sans identifiant : on en fabrique un
    // plutôt que de le rejeter, sinon il devient impossible à sélectionner.
    if (profile.id.isEmpty())
        profile.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (!profile.createdAt.isValid())
        profile.createdAt = QDateTime::currentDateTimeUtc();

    return profile;
}

QJsonObject Profile::toJson() const
{
    QJsonObject paramsObject;
    for (auto it = params.constBegin(); it != params.constEnd(); ++it)
        paramsObject.insert(it.key(), it.value());

    QJsonObject object;
    object.insert(QStringLiteral("id"), id);
    object.insert(QStringLiteral("name"), name);
    object.insert(QStringLiteral("modelPath"), modelPath);
    object.insert(QStringLiteral("binary"), binaryKindToString(binary));
    object.insert(QStringLiteral("params"), paramsObject);
    object.insert(QStringLiteral("extraArgs"), extraArgs);
    object.insert(QStringLiteral("notes"), notes);
    object.insert(QStringLiteral("createdAt"), createdAt.toUTC().toString(Qt::ISODate));
    if (lastUsedAt.isValid())
        object.insert(QStringLiteral("lastUsedAt"), lastUsedAt.toUTC().toString(Qt::ISODate));
    return object;
}

bool Profile::hasName() const
{
    return !name.trimmed().isEmpty();
}

} // namespace core
