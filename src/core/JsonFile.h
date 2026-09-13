#pragma once

#include <QJsonDocument>
#include <QString>

namespace core::JsonFile {

/// Issue d'une lecture. Un fichier illisible ou corrompu n'est jamais supprimé :
/// il est mis de côté sous `backupPath` et l'appelant démarre sur du vide (§4.3).
struct ReadResult {
    QJsonDocument document;
    bool existed = false;
    bool corrupt = false;
    QString error;
    QString backupPath;

    bool ok() const { return error.isEmpty(); }
};

ReadResult read(const QString& path);

/// Écriture atomique : QSaveFile écrit dans un temporaire puis remplace la cible,
/// ce qui interdit de laisser un fichier à moitié écrit.
bool write(const QString& path, const QJsonDocument& document, QString* error = nullptr);

} // namespace core::JsonFile
