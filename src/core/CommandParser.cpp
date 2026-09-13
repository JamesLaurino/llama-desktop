#include "core/CommandParser.h"

#include "core/CommandBuilder.h"
#include "core/ParamRegistry.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

namespace core {

bool ParsedCommand::isEmpty() const
{
    return modelPath.isEmpty() && params.isEmpty() && extraArgs.isEmpty();
}

} // namespace core

namespace core::CommandParser {
namespace {

/// Drapeaux du modèle. Ils ne peuvent pas vivre dans params.json : `modelPath`
/// est un champ du profil, pas un paramètre du registre. C'est le pendant exact
/// du `-m` que `CommandBuilder::build()` écrit en tête.
bool isModelFlag(const QString& flag)
{
    return flag == QLatin1String("-m") || flag == QLatin1String("--model");
}

/// Distingue un drapeau d'une valeur négative : `--temp -1` et `-s -1` sont
/// courants, et `-1` n'est pas un drapeau.
bool looksLikeFlag(const QString& token)
{
    if (token.size() < 2 || !token.startsWith(u'-'))
        return false;
    const QChar next = token.at(1);
    return !next.isDigit() && next != u'.';
}

/// Opérateurs de shell laissés seuls. Un jeton qui *contient* `&` peut être une
/// valeur légitime — le découpage a déjà retiré les guillemets — seul l'opérateur
/// isolé signifie que la ligne enchaîne plusieurs commandes.
bool isShellOperator(const QString& token)
{
    static const QStringList operators = { QStringLiteral("|"),  QStringLiteral("||"),
                                           QStringLiteral("&&"), QStringLiteral(">"),
                                           QStringLiteral(">>"), QStringLiteral("<"),
                                           QStringLiteral(";"),  QStringLiteral("2>&1") };
    return operators.contains(token);
}

bool isTruthy(const QString& value)
{
    const QString normalized = value.trimmed().toLower();
    return normalized == QLatin1String("true") || normalized == QLatin1String("1")
        || normalized == QLatin1String("on") || normalized == QLatin1String("yes");
}

/// Réunit une commande écrite sur plusieurs lignes : les trois shells ont leur
/// caractère de continuation, et une ligne copiée d'un README les porte souvent.
QString joinContinuations(const QString& raw)
{
    static const QRegularExpression continuation(QStringLiteral("[\\\\`^][ \\t]*\\r?\\n"));
    QString text = raw;
    text.replace(continuation, QStringLiteral(" "));
    text.replace(u'\n', u' ');
    text.replace(u'\r', u' ');
    return text.trimmed();
}

/// Ramène un chemin à la forme stockée partout ailleurs dans l'application.
///
/// `CommandBuilder::build()` n'écrit des antislashs que dans la ligne affichée ;
/// relire cette ligne rendrait un profil en `D:\x` là où un profil choisi au
/// sélecteur de fichiers contient `D:/x`. Deux écritures du même chemin dans le
/// même champ, c'est une différence que rien ne justifie.
QString normalisePath(const QString& path)
{
    return QDir::fromNativeSeparators(path);
}

std::optional<BinaryKind> binaryFromExecutable(const QString& path)
{
    const QString name = QFileInfo(path).fileName().toLower();
    // « cli » d'abord : un chemin comme « C:/serveurs/llama-cli.exe » contiendrait
    // les deux mots, et c'est le nom du fichier qui tranche.
    if (name.contains(QLatin1String("cli")))
        return BinaryKind::Cli;
    if (name.contains(QLatin1String("server")))
        return BinaryKind::Server;
    return std::nullopt;
}

} // namespace

ParsedCommand parse(const QString& line, const ParamRegistry& registry)
{
    ParsedCommand result;

    const QString normalised = joinContinuations(line);
    QStringList tokens = CommandBuilder::splitArgumentLine(normalised);

    // Opérateur d'appel PowerShell : `& "C:\...\llama-server.exe" -m ...`.
    if (!tokens.isEmpty() && tokens.first() == QLatin1String("&")) {
        tokens.removeFirst();
        result.notes.append(QStringLiteral("Opérateur d'appel « & » ignoré."));
    }
    if (tokens.isEmpty()) {
        result.error = QStringLiteral("Aucune commande à analyser.");
        return result;
    }
    for (const QString& token : tokens) {
        if (isShellOperator(token)) {
            result.error =
                QStringLiteral(
                    "La ligne enchaîne plusieurs commandes (« %1 ») : n'en collez qu'une.")
                    .arg(token);
            return result;
        }
    }

    QStringList extras;
    qsizetype index = 0;

    if (!looksLikeFlag(tokens.first())) {
        result.executablePath = tokens.first();
        index = 1;
        if (const auto kind = binaryFromExecutable(result.executablePath)) {
            result.binary = *kind;
            result.binaryDetected = true;
        } else {
            result.notes.append(
                QStringLiteral("Exécutable « %1 » : ni « server » ni « cli » dans le nom, "
                               "le binaire du profil est conservé.")
                    .arg(QFileInfo(result.executablePath).fileName()));
        }
    }

    const auto takeValue = [&tokens, &index]() -> std::optional<QString> {
        if (index + 1 >= tokens.size() || looksLikeFlag(tokens.at(index + 1)))
            return std::nullopt;
        return tokens.at(++index);
    };

    for (; index < tokens.size(); ++index) {
        const QString token = tokens.at(index);
        if (!looksLikeFlag(token)) {
            extras.append(token);
            result.notes.append(
                QStringLiteral(
                    "« %1 » n'est rattaché à aucun drapeau : conservé en arguments libres.")
                    .arg(token));
            continue;
        }

        // `--ctx-size=16384` : jamais émis par cette application, fréquent ailleurs.
        QString flag = token;
        QString inlineValue;
        bool hasInline = false;
        if (token.startsWith(QLatin1String("--"))) {
            const qsizetype equals = token.indexOf(u'=');
            if (equals > 2) {
                flag = token.left(equals);
                inlineValue = token.mid(equals + 1);
                hasInline = true;
            }
        }

        if (isModelFlag(flag)) {
            const std::optional<QString> value =
                hasInline ? std::optional<QString>(inlineValue) : takeValue();
            if (!value) {
                result.notes.append(
                    QStringLiteral("« %1 » sans chemin de modèle : ignoré.").arg(flag));
                continue;
            }
            result.modelPath = normalisePath(*value);
            continue;
        }

        const auto match = registry.findByFlag(flag);
        if (!match) {
            extras.append(token);
            result.notes.append(
                QStringLiteral("« %1 » est inconnu de ce build : conservé en arguments libres.")
                    .arg(flag));
            continue;
        }

        const ParamDef& def = *match->param;
        QString value;

        if (def.type == ParamType::Bool) {
            // `--metrics` seul, ou `--metrics=true` venu d'ailleurs.
            value = (!hasInline || isTruthy(inlineValue)) ? QStringLiteral("true")
                                                          : QStringLiteral("false");
        } else if (def.type == ParamType::Tristate) {
            const bool on = hasInline ? isTruthy(inlineValue) : true;
            // `--no-jinja` désigne le même paramètre que `--jinja` : la négation
            // du drapeau et celle de la valeur se composent.
            value = (on != match->negated) ? QStringLiteral("on") : QStringLiteral("off");
        } else {
            const std::optional<QString> taken =
                hasInline ? std::optional<QString>(inlineValue) : takeValue();
            if (taken) {
                // Seuls les chemins sont normalisés : une chaîne libre, un gabarit
                // de conversation par exemple, peut contenir des antislashs qui lui
                // appartiennent.
                value = (def.type == ParamType::Path) ? normalisePath(*taken) : *taken;
            } else if (def.type == ParamType::Enum
                       && def.values.contains(QLatin1String("on"), Qt::CaseInsensitive)) {
                // `-fa` était un booléen avant que ce build n'en fasse un enum :
                // c'est encore la forme qu'on trouve partout en ligne.
                value = QStringLiteral("on");
                result.notes.append(
                    QStringLiteral("« %1 » sans valeur : interprété comme « on ».").arg(flag));
            } else {
                extras.append(token);
                result.notes.append(
                    QStringLiteral("« %1 » attend une valeur, aucune ne suit : conservé en "
                                   "arguments libres.")
                        .arg(flag));
                continue;
            }
        }

        if (result.params.contains(def.key)) {
            result.notes.append(
                QStringLiteral("« %1 » apparaît plusieurs fois : la dernière valeur est retenue.")
                    .arg(def.label));
            for (qsizetype i = result.entries.size() - 1; i >= 0; --i) {
                if (result.entries.at(i).key == def.key) {
                    result.entries.remove(i);
                    break;
                }
            }
        }

        ParsedEntry entry;
        entry.key = def.key;
        entry.label = def.label;
        entry.flag = flag;
        entry.value = value;
        entry.appliesToBinary = def.appliesToBinary(result.binary);
        if (!entry.appliesToBinary) {
            result.notes.append(
                QStringLiteral("« %1 » ne concerne pas %2 : conservé dans le profil, mais "
                               "absent de la commande générée.")
                    .arg(flag, binaryKindToString(result.binary)));
        }

        result.params.insert(def.key, value);
        result.entries.append(entry);
    }

    QStringList quoted;
    quoted.reserve(extras.size());
    for (const QString& extra : extras)
        quoted.append(CommandBuilder::quoteArgument(extra));
    result.extraArgs = quoted.join(u' ');

    return result;
}

void applyTo(const ParsedCommand& parsed, Profile& profile)
{
    if (!parsed.isValid())
        return;
    if (parsed.binaryDetected)
        profile.binary = parsed.binary;
    profile.modelPath = parsed.modelPath;
    profile.params = parsed.params;
    profile.extraArgs = parsed.extraArgs;
}

} // namespace core::CommandParser
