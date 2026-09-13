#pragma once

#include "core/Profile.h"

#include <QString>
#include <QStringList>
#include <QVector>

namespace core {

class ParamRegistry;

/// Un paramètre reconnu sur la ligne analysée, tel que l'aperçu doit le montrer.
struct ParsedEntry {
    QString key;
    QString label;
    QString flag;   ///< le drapeau réellement écrit, pas sa forme canonique
    QString value;
    /// Faux si le paramètre existe mais ne concerne pas le binaire détecté :
    /// il est conservé dans le profil, mais la génération ne l'émettra pas.
    bool appliesToBinary = true;
};

/// Résultat de l'analyse d'une ligne de commande llama.cpp.
///
/// Miroir de `BuiltCommand` : ce que `CommandBuilder::build()` écrit, `parse()`
/// doit savoir le relire. Les écritures acceptées sont plus nombreuses que
/// celles émises — formes longues, `--flag=valeur`, continuations de ligne —
/// parce qu'une ligne collée vient d'un README, pas de cette application.
struct ParsedCommand {
    QString error;   ///< non vide : la ligne n'est pas exploitable, rien n'est importé

    QString executablePath;
    bool binaryDetected = false;
    BinaryKind binary = BinaryKind::Server;

    QString modelPath;
    QMap<QString, QString> params;
    QString extraArgs;

    QVector<ParsedEntry> entries;
    /// Ce qui a été deviné, déplacé ou laissé de côté. Affiché tel quel dans
    /// l'aperçu : un import qui perd un argument sans le dire est un piège.
    QStringList notes;

    bool isValid() const { return error.isEmpty(); }
    /// Vrai si la ligne n'a rien donné d'exploitable, sans être pour autant
    /// syntaxiquement fautive.
    bool isEmpty() const;
};

namespace CommandParser {

/// Analyse une ligne de commande. Fonction pure : aucune entrée-sortie.
ParsedCommand parse(const QString& line, const ParamRegistry& registry);

/// Recopie le résultat dans un profil en préservant ce qui lui appartient :
/// identifiant, nom, notes et horodatages.
void applyTo(const ParsedCommand& parsed, Profile& profile);

} // namespace CommandParser
} // namespace core
