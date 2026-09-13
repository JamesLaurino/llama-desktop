#include "core/CommandBuilder.h"

#include "core/ParamRegistry.h"
#include "core/Settings.h"

#include <QDir>
#include <QVector>

namespace core::CommandBuilder {
namespace {

/// Un argument et son statut : seuls les chemins sont normalisés à l'affichage.
struct Token {
    QString value;
    bool isPath = false;
};

bool isWhitespace(QChar c)
{
    return c == u' ' || c == u'\t' || c == u'\n' || c == u'\r' || c == u'\v';
}

bool needsQuoting(const QString& argument)
{
    if (argument.isEmpty())
        return true;
    for (const QChar c : argument) {
        if (isWhitespace(c))
            return true;
        switch (c.unicode()) {
        case u'"':
        case u'&':
        case u'|':
        case u'<':
        case u'>':
        case u'^':
        case u'(':
        case u')':
            return true;
        default:
            break;
        }
    }
    return false;
}

bool isTruthy(const QString& value)
{
    const QString normalized = value.trimmed().toLower();
    return normalized == QLatin1String("true") || normalized == QLatin1String("1")
        || normalized == QLatin1String("on") || normalized == QLatin1String("yes");
}

} // namespace

QString quoteArgument(const QString& argument)
{
    if (!needsQuoting(argument))
        return argument;

    QString result;
    result.reserve(argument.size() + 8);
    result += u'"';

    // Règle CommandLineToArgvW : un antislash n'est spécial que devant un
    // guillemet, et il doit alors être doublé.
    int backslashes = 0;
    for (const QChar c : argument) {
        if (c == u'\\') {
            ++backslashes;
            continue;
        }
        if (c == u'"') {
            result += QString(backslashes * 2 + 1, u'\\');
            result += u'"';
            backslashes = 0;
            continue;
        }
        result += QString(backslashes, u'\\');
        backslashes = 0;
        result += c;
    }
    // Antislashs terminaux : doublés, sinon ils échapperaient le guillemet fermant.
    result += QString(backslashes * 2, u'\\');
    result += u'"';
    return result;
}

QStringList splitArgumentLine(const QString& line)
{
    QStringList tokens;
    const qsizetype length = line.size();
    qsizetype i = 0;

    while (i < length) {
        while (i < length && isWhitespace(line.at(i)))
            ++i;
        if (i >= length)
            break;

        QString token;
        bool inQuotes = false;
        int backslashes = 0;

        while (i < length) {
            const QChar c = line.at(i);
            if (c == u'\\') {
                ++backslashes;
                ++i;
                continue;
            }
            if (c == u'"') {
                token += QString(backslashes / 2, u'\\');
                if (backslashes % 2 == 1)
                    token += u'"';
                else
                    inQuotes = !inQuotes;
                backslashes = 0;
                ++i;
                continue;
            }
            token += QString(backslashes, u'\\');
            backslashes = 0;
            if (!inQuotes && isWhitespace(c))
                break;
            token += c;
            ++i;
        }
        token += QString(backslashes, u'\\');
        tokens.append(token);
    }
    return tokens;
}

QString toDisplayLine(const QString& program, const QStringList& arguments)
{
    QStringList parts;
    parts.reserve(arguments.size() + 1);
    parts.append(quoteArgument(QDir::toNativeSeparators(program)));
    for (const QString& argument : arguments)
        parts.append(quoteArgument(argument));
    return parts.join(u' ');
}

QString defaultExecutableName(BinaryKind kind)
{
    return kind == BinaryKind::Cli ? QStringLiteral("llama-cli.exe")
                                   : QStringLiteral("llama-server.exe");
}

BuiltCommand build(const Profile& profile, const Settings& settings, const ParamRegistry& registry)
{
    QVector<Token> tokens;
    tokens.reserve(profile.params.size() * 2 + 4);

    if (!profile.modelPath.isEmpty()) {
        tokens.append({ QStringLiteral("-m"), false });
        tokens.append({ profile.modelPath, true });
    }

    // L'ordre est celui de params.json, jamais celui de la table du profil.
    for (const ParamDef& param : registry.params()) {
        if (!param.appliesToBinary(profile.binary))
            continue;
        const auto it = profile.params.constFind(param.key);
        if (it == profile.params.constEnd())
            continue;
        const QString& value = it.value();

        switch (param.type) {
        case ParamType::Bool:
            if (isTruthy(value))
                tokens.append({ param.flag, false });
            break;
        case ParamType::Tristate:
            if (value.compare(QLatin1String("on"), Qt::CaseInsensitive) == 0)
                tokens.append({ param.flag, false });
            else if (value.compare(QLatin1String("off"), Qt::CaseInsensitive) == 0)
                tokens.append({ param.flagOff, false });
            break;
        default:
            // Un flag émis sans sa valeur ferait échouer llama.cpp : on préfère
            // ne rien émettre du tout.
            if (value.isEmpty())
                break;
            tokens.append({ param.flag, false });
            tokens.append({ value, param.type == ParamType::Path });
            break;
        }
    }

    for (const QString& extra : splitArgumentLine(profile.extraArgs))
        tokens.append({ extra, false });

    BuiltCommand command;
    command.program = settings.executableFor(profile.binary);
    command.arguments.reserve(tokens.size());

    QStringList displayArguments;
    displayArguments.reserve(tokens.size());
    for (const Token& token : tokens) {
        command.arguments.append(token.value);
        displayArguments.append(token.isPath ? QDir::toNativeSeparators(token.value) : token.value);
    }

    // `program` reste vide si les Réglages sont incomplets — c'est la validation
    // qui doit bloquer le lancement — mais la ligne affichée reste lisible.
    const QString displayProgram = command.program.isEmpty()
        ? defaultExecutableName(profile.binary)
        : command.program;
    command.displayLine = toDisplayLine(displayProgram, displayArguments);
    return command;
}

} // namespace core::CommandBuilder
