#include "core/JsonFile.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonParseError>
#include <QSaveFile>

namespace core::JsonFile {
namespace {

/// `profiles.json` → `profiles.corrupt-20260913T142200Z.json`
QString corruptNameFor(const QString& path)
{
    const QFileInfo info(path);
    const QString stamp = QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd'T'HHmmss'Z'"));
    const QString name = QStringLiteral("%1.corrupt-%2").arg(info.completeBaseName(), stamp);
    const QString suffix = info.suffix();
    return info.absoluteDir().filePath(suffix.isEmpty() ? name : name + u'.' + suffix);
}

} // namespace

ReadResult read(const QString& path)
{
    ReadResult result;

    QFile file(path);
    if (!file.exists())
        return result;
    result.existed = true;

    if (!file.open(QIODevice::ReadOnly)) {
        result.error = QStringLiteral("Lecture impossible de %1 : %2").arg(path, file.errorString());
        return result;
    }

    const QByteArray content = file.readAll();
    file.close();

    // Un fichier vide est traité comme une absence : c'est le résultat courant
    // d'un arrêt brutal, pas une corruption à archiver.
    if (content.trimmed().isEmpty()) {
        result.existed = false;
        return result;
    }

    QJsonParseError parseError;
    QJsonDocument document = QJsonDocument::fromJson(content, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        result.corrupt = true;
        result.error = QStringLiteral("%1 est illisible (offset %2) : %3")
                           .arg(path)
                           .arg(parseError.offset)
                           .arg(parseError.errorString());
        const QString backup = corruptNameFor(path);
        if (QFile::rename(path, backup))
            result.backupPath = backup;
        return result;
    }

    result.document = document;
    return result;
}

bool write(const QString& path, const QJsonDocument& document, QString* error)
{
    const auto fail = [error](const QString& message) {
        if (error)
            *error = message;
        return false;
    };

    const QFileInfo info(path);
    if (!QDir().mkpath(info.absolutePath()))
        return fail(QStringLiteral("Création impossible du dossier %1").arg(info.absolutePath()));

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return fail(QStringLiteral("Écriture impossible dans %1 : %2").arg(path, file.errorString()));

    const QByteArray payload = document.toJson(QJsonDocument::Indented);
    if (file.write(payload) != payload.size())
        return fail(QStringLiteral("Écriture incomplète dans %1 : %2").arg(path, file.errorString()));
    if (!file.commit())
        return fail(QStringLiteral("Remplacement impossible de %1 : %2").arg(path, file.errorString()));

    if (error)
        error->clear();
    return true;
}

} // namespace core::JsonFile
