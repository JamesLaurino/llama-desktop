#include "core/AppPaths.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace core::AppPaths {
namespace {

constexpr auto kParamsFileName = "params.json";
constexpr auto kEmbeddedParams = ":/resources/params.json";

} // namespace

QString dataDir()
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dir.isEmpty()) {
        // Sans QCoreApplication configurée (tests, outils), on retombe sur le
        // dossier courant plutôt que de renvoyer une chaîne vide.
        dir = QDir::current().filePath(QStringLiteral("LlamaBuilder"));
    }
    return QDir::cleanPath(dir);
}

QString profilesFile()
{
    return QDir(dataDir()).filePath(QStringLiteral("profiles.json"));
}

QString settingsFile()
{
    return QDir(dataDir()).filePath(QStringLiteral("settings.json"));
}

QString resolveParamsFile()
{
    const QString userOverride = QDir(dataDir()).filePath(QString::fromLatin1(kParamsFileName));
    if (QFileInfo::exists(userOverride))
        return userOverride;

    const QString appDir = QCoreApplication::applicationDirPath();
    if (!appDir.isEmpty()) {
        const QString beside = QDir(appDir).filePath(QString::fromLatin1(kParamsFileName));
        if (QFileInfo::exists(beside))
            return beside;
    }

    return QString::fromLatin1(kEmbeddedParams);
}

} // namespace core::AppPaths
