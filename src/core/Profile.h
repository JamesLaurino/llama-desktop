#pragma once

#include <QDateTime>
#include <QMap>
#include <QString>
#include <optional>

class QJsonObject;

namespace core {

/// Exécutable llama.cpp ciblé par un profil.
enum class BinaryKind { Server, Cli };

QString binaryKindToString(BinaryKind kind);
std::optional<BinaryKind> binaryKindFromString(QStringView text);

/// Un modèle GGUF, une configuration, un nom lisible.
///
/// Type purement données : aucun QObject, aucune dépendance UI, copiable.
/// `params` ne contient que les paramètres explicitement activés par
/// l'utilisateur — un paramètre laissé à son défaut n'y figure pas.
struct Profile {
    QString id;
    QString name;
    QString modelPath;
    BinaryKind binary = BinaryKind::Server;
    QMap<QString, QString> params;
    QString extraArgs;
    QString notes;
    QDateTime createdAt;
    QDateTime lastUsedAt;

    /// Profil neuf : identifiant frais, nom vide (à saisir), horodatage courant.
    static Profile createNew();

    static Profile fromJson(const QJsonObject& object);
    QJsonObject toJson() const;

    /// Vrai si le profil peut être enregistré (§4.1 : le nom est obligatoire).
    bool hasName() const;
};

} // namespace core
