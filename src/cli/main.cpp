// Harnais console de la phase 1 : valide le noyau sans aucune UI.
//
//   llamabuilder-cli params                 vérifie params.json et le résume
//   llamabuilder-cli list                   liste les profils enregistrés
//   llamabuilder-cli show <nom|id>          imprime la commande d'un profil
//   llamabuilder-cli demo                   imprime la commande d'un profil d'exemple

#include "core/AppPaths.h"
#include "core/CommandBuilder.h"
#include "core/ParamRegistry.h"
#include "core/Profile.h"
#include "core/ProfileStore.h"
#include "core/Settings.h"
#include "core/SettingsStore.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDateTime>
#include <QFileInfo>
#include <QTextStream>

using namespace core;

namespace {

QTextStream& out()
{
    static QTextStream stream(stdout);
    return stream;
}

QTextStream& err()
{
    static QTextStream stream(stderr);
    return stream;
}

int printParams(const ParamRegistry& registry)
{
    out() << "params.json : " << registry.params().size() << " paramètres, "
          << registry.sections().size() << " sections\n";
    if (!registry.targetBuild().isEmpty())
        out() << "build de référence : " << registry.targetBuild() << "\n\n";

    for (const SectionDef& section : registry.sections()) {
        int count = 0;
        for (const ParamDef& param : registry.params())
            count += (param.section == section.id) ? 1 : 0;
        out() << (section.defaultExpanded ? "[+] " : "[-] ") << section.label << "  ("
              << count << ")\n";
        for (const ParamDef& param : registry.params()) {
            if (param.section != section.id)
                continue;
            const QString applies = param.appliesTo.isEmpty()
                ? QStringLiteral("server,cli")
                : param.appliesTo.join(u',');
            out() << "      " << param.flag.leftJustified(20)
                  << paramTypeToString(param.type).leftJustified(14)
                  << applies.leftJustified(12) << param.key << "\n";
        }
    }
    out() << Qt::flush;
    return 0;
}

void printProfileLine(const Profile& profile)
{
    const QString used = profile.lastUsedAt.isValid()
        ? profile.lastUsedAt.toLocalTime().toString(QStringLiteral("dd/MM/yyyy HH:mm"))
        : QStringLiteral("jamais");
    out() << "  " << (profile.name.isEmpty() ? QStringLiteral("(sans nom)") : profile.name) << "\n"
          << "      binaire : " << binaryKindToString(profile.binary) << "   paramètres : "
          << profile.params.size() << "   utilisé : " << used << "\n"
          << "      modèle  : "
          << (profile.modelPath.isEmpty() ? QStringLiteral("(aucun)") : profile.modelPath) << "\n"
          << "      id      : " << profile.id << "\n";
}

const Profile* findProfile(const ProfileStore& store, const QString& needle)
{
    if (const Profile* exact = store.byId(needle))
        return exact;
    for (const Profile& profile : store.profiles()) {
        if (profile.name.compare(needle, Qt::CaseInsensitive) == 0)
            return &profile;
    }
    for (const Profile& profile : store.profiles()) {
        if (profile.name.contains(needle, Qt::CaseInsensitive))
            return &profile;
    }
    return nullptr;
}

void printCommand(const Profile& profile, const Settings& settings, const ParamRegistry& registry)
{
    const BuiltCommand command = CommandBuilder::build(profile, settings, registry);

    out() << "--- commande affichable (collable dans cmd.exe) ---\n"
          << command.displayLine << "\n\n"
          << "--- arguments bruts transmis à QProcess (" << command.arguments.size() << ") ---\n"
          << "  programme : "
          << (command.program.isEmpty() ? QStringLiteral("(non configuré dans les Réglages)")
                                        : command.program)
          << "\n";
    for (const QString& argument : command.arguments)
        out() << "  [" << argument << "]\n";
    out() << Qt::flush;
}

/// Profil d'exemple : couvre un chemin avec espaces, un enum, un booléen,
/// un tristate et des arguments libres cités.
Profile demoProfile()
{
    Profile profile = Profile::createNew();
    profile.name = QStringLiteral("Démo — contexte long");
    profile.modelPath = QStringLiteral("D:/mes modèles/qwen3-27b-q4_k_m.gguf");
    profile.binary = BinaryKind::Server;
    profile.params = {
        { QStringLiteral("n-gpu-layers"), QStringLiteral("all") },
        { QStringLiteral("ctx-size"), QStringLiteral("16384") },
        { QStringLiteral("flash-attn"), QStringLiteral("on") },
        { QStringLiteral("cache-type-k"), QStringLiteral("q8_0") },
        { QStringLiteral("cache-type-v"), QStringLiteral("q8_0") },
        { QStringLiteral("port"), QStringLiteral("8080") },
        { QStringLiteral("jinja"), QStringLiteral("on") },
        { QStringLiteral("verbose"), QStringLiteral("false") },
    };
    profile.extraArgs = QStringLiteral("--no-warmup --alias \"Qwen 27B\"");
    return profile;
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    // Ne pas définir organizationName : voir le contrat dans AppPaths.h.
    QCoreApplication::setApplicationName(QStringLiteral("LlamaBuilder"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Harnais de validation du noyau LlamaBuilder.\n\n"
                       "Commandes : params | list | show <nom|id> | demo"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("commande"),
                                 QStringLiteral("params, list, show ou demo"));
    parser.addPositionalArgument(QStringLiteral("cible"),
                                 QStringLiteral("nom ou identifiant de profil (pour show)"));

    const QCommandLineOption paramsFileOption(
        QStringLiteral("params-file"),
        QStringLiteral("Utilise ce params.json au lieu de celui résolu automatiquement."),
        QStringLiteral("chemin"));
    parser.addOption(paramsFileOption);
    parser.process(app);

    const QStringList positional = parser.positionalArguments();
    const QString command = positional.value(0);
    if (command.isEmpty()) {
        parser.showHelp(1);
        return 1;
    }

    const QString paramsPath = parser.isSet(paramsFileOption) ? parser.value(paramsFileOption)
                                                              : AppPaths::resolveParamsFile();
    QString registryError;
    const ParamRegistry registry = ParamRegistry::fromFile(paramsPath, &registryError);
    if (registry.isEmpty()) {
        err() << "params.json inutilisable (" << paramsPath << ") : " << registryError << "\n"
              << Qt::flush;
        return 2;
    }
    if (command == QLatin1String("params")) {
        out() << "source : " << paramsPath << "\n";
        return printParams(registry);
    }

    if (command == QLatin1String("demo")) {
        SettingsStore settingsStore(AppPaths::settingsFile());
        settingsStore.load();
        printCommand(demoProfile(), settingsStore.settings(), registry);
        return 0;
    }

    ProfileStore profileStore(AppPaths::profilesFile());
    if (!profileStore.load()) {
        err() << "Avertissement : " << profileStore.lastError() << "\n";
        if (!profileStore.corruptBackupPath().isEmpty())
            err() << "Fichier mis de côté : " << profileStore.corruptBackupPath() << "\n";
        err() << Qt::flush;
    }

    SettingsStore settingsStore(AppPaths::settingsFile());
    settingsStore.load();

    if (command == QLatin1String("list")) {
        out() << "dossier : " << AppPaths::dataDir() << "\n"
              << profileStore.count() << " profil(s)\n\n";
        for (const Profile& profile : profileStore.profiles())
            printProfileLine(profile);
        out() << Qt::flush;
        return 0;
    }

    if (command == QLatin1String("show")) {
        const QString needle = positional.value(1);
        if (needle.isEmpty()) {
            err() << "Usage : llamabuilder-cli show <nom|id>\n" << Qt::flush;
            return 1;
        }
        const Profile* profile = findProfile(profileStore, needle);
        if (!profile) {
            err() << "Aucun profil ne correspond à « " << needle << " ».\n" << Qt::flush;
            return 3;
        }
        printProfileLine(*profile);
        out() << "\n";
        printCommand(*profile, settingsStore.settings(), registry);
        return 0;
    }

    err() << "Commande inconnue : « " << command << " ». Attendu : params, list, show ou demo.\n"
          << Qt::flush;
    return 1;
}
