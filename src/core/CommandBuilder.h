#pragma once

#include "core/Profile.h"

#include <QString>
#include <QStringList>

namespace core {

struct Settings;
class ParamRegistry;

/// Résultat de la génération : les deux représentations de la même commande.
///
/// `arguments` part vers QProcess::setArguments() sans aucun échappement ;
/// `displayLine` est la version citée, collable telle quelle dans cmd.exe.
/// Les deux sont produites depuis la même source, jamais l'une en redécoupant
/// l'autre (§7, règle 5).
struct BuiltCommand {
    QString program;
    QStringList arguments;
    QString displayLine;
};

namespace CommandBuilder {

/// Construit la commande. Fonction pure : aucune entrée-sortie, aucun accès disque.
BuiltCommand build(const Profile& profile, const Settings& settings, const ParamRegistry& registry);

/// Découpe une ligne d'arguments libre en respectant les guillemets doubles,
/// selon les règles de CommandLineToArgvW.
QStringList splitArgumentLine(const QString& line);

/// Cite un argument pour cmd.exe si nécessaire (espace, tabulation, guillemet,
/// ou l'un des métacaractères `&|<>^()`).
QString quoteArgument(const QString& argument);

/// Assemble une ligne affichable. Le programme est normalisé en séparateurs
/// Windows ; les arguments sont cités tels quels.
QString toDisplayLine(const QString& program, const QStringList& arguments);

/// Nom conventionnel de l'exécutable, utilisé à l'affichage tant que le chemin
/// réel n'est pas renseigné dans les Réglages.
QString defaultExecutableName(BinaryKind kind);

} // namespace CommandBuilder
} // namespace core
