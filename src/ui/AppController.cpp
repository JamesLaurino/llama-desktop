#include "ui/AppController.h"

#include "core/AppPaths.h"
#include "core/CommandBuilder.h"
#include "core/CommandParser.h"
#include "ui/Fonts.h"
#include "ui/ParamFormModel.h"
#include "ui/ProfileListModel.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QProcess>
#include <QUrl>

namespace ui {
namespace {

core::BinaryKind kindFromString(const QString& text)
{
    return core::binaryKindFromString(text).value_or(core::BinaryKind::Server);
}

} // namespace

AppController::AppController(QObject* parent)
    : AppController(core::AppPaths::resolveParamsFile(), core::AppPaths::profilesFile(),
                    core::AppPaths::settingsFile(), parent)
{
}

AppController::AppController(const QString& paramsPath, const QString& profilesPath,
                            const QString& settingsPath, QObject* parent)
    : QObject(parent)
    , m_profileStore(profilesPath)
    , m_settingsStore(settingsPath)
{
    initialise(paramsPath);
}

AppController* AppController::create(QQmlEngine* engine, QJSEngine* scriptEngine)
{
    Q_UNUSED(engine);
    Q_UNUSED(scriptEngine);
    return new AppController;
}

void AppController::initialise(const QString& paramsPath)
{
    m_paramsPath = paramsPath;
    QString registryError;
    m_registry = core::ParamRegistry::fromFile(paramsPath, &registryError);
    if (m_registry.isEmpty()) {
        m_startupWarning = QStringLiteral("params.json inutilisable (%1) : %2")
                               .arg(QDir::toNativeSeparators(paramsPath), registryError);
    }

    if (!m_profileStore.load()) {
        QString warning = m_profileStore.lastError();
        if (!m_profileStore.corruptBackupPath().isEmpty()) {
            warning += QStringLiteral(" Fichier mis de côté : %1")
                           .arg(QDir::toNativeSeparators(m_profileStore.corruptBackupPath()));
        }
        m_startupWarning =
            m_startupWarning.isEmpty() ? warning : m_startupWarning + QChar(u'\n') + warning;
    }
    m_profileStore.sortByRecentUse();
    m_settingsStore.load();

    m_profiles = new ProfileListModel(&m_profileStore, this);
    m_form = new ParamFormModel(&m_registry, this);
    m_monitor = new MonitorController(this);
    m_monitor->setIntervalMs(m_settingsStore.settings().monitorIntervalMs);
    m_logs = new LogModel(this);
    connectRunner();

    connect(m_form, &ParamFormModel::valuesChanged, this, [this] {
        m_current.params = m_form->values();
        persistCurrent();
        recompute();
        emit paramsRevisionChanged();
    });
    connect(&m_settingsStore, &core::SettingsStore::settingsChanged, this, [this] {
        // L'intervalle des jauges suit les Réglages sans redémarrage, comme les
        // chemins d'exécutables (critère d'acceptation n°5).
        m_monitor->setIntervalMs(m_settingsStore.settings().monitorIntervalMs);
        emit settingsChanged();
        recompute();
    });

    if (m_profileStore.count() > 0)
        selectProfile(m_profileStore.profiles().constFirst().id);
    else
        recompute();
}

QVariantList AppController::sections() const
{
    QVariantList list;
    for (const core::SectionDef& section : m_registry.sections()) {
        list.append(QVariantMap{
            { QStringLiteral("id"), section.id },
            { QStringLiteral("label"), section.label },
            { QStringLiteral("expanded"), section.defaultExpanded },
        });
    }
    return list;
}

QVariantMap AppController::sectionSetCounts() const
{
    QVariantMap counts;
    for (const core::SectionDef& section : m_registry.sections())
        counts.insert(section.id, 0);
    for (const core::ParamDef& param : m_registry.params()) {
        if (!param.appliesToBinary(m_current.binary))
            continue;
        if (m_form->value(param.key).isEmpty())
            continue;
        counts[param.section] = counts.value(param.section).toInt() + 1;
    }
    return counts;
}

QString AppController::binary() const
{
    return core::binaryKindToString(m_current.binary);
}

void AppController::selectProfile(const QString& id)
{
    const core::Profile* profile = m_profileStore.byId(id);
    m_hasCurrent = (profile != nullptr);
    m_current = profile ? *profile : core::Profile{};
    m_form->setSource(m_current.binary, m_current.params);

    emit currentProfileChanged();
    emit modelPathChanged();
    emit binaryChanged();
    emit extraArgsChanged();
    emit notesChanged();
    emit paramsRevisionChanged();
    recompute();
}

void AppController::setModelPath(const QString& path)
{
    if (!m_hasCurrent || m_current.modelPath == path)
        return;
    m_current.modelPath = path;
    persistCurrent();
    m_profiles->notifyChanged(m_current.id);
    emit modelPathChanged();
    recompute();
}

void AppController::setBinary(const QString& kind)
{
    const core::BinaryKind value = kindFromString(kind);
    if (!m_hasCurrent || m_current.binary == value)
        return;
    m_current.binary = value;
    m_form->setBinary(value);
    persistCurrent();
    m_profiles->notifyChanged(m_current.id);
    emit binaryChanged();
    emit paramsRevisionChanged();
    recompute();
}

void AppController::setExtraArgs(const QString& args)
{
    if (!m_hasCurrent || m_current.extraArgs == args)
        return;
    m_current.extraArgs = args;
    persistCurrent();
    emit extraArgsChanged();
    recompute();
}

void AppController::setNotes(const QString& notes)
{
    if (!m_hasCurrent || m_current.notes == notes)
        return;
    m_current.notes = notes;
    persistCurrent();
    emit notesChanged();
}

void AppController::setParamValue(const QString& key, const QString& value)
{
    if (m_hasCurrent)
        m_form->setValue(key, value);
}

void AppController::resetSection(const QString& sectionId)
{
    if (m_hasCurrent)
        m_form->clearSection(sectionId);
}

QString AppController::createProfile()
{
    core::Profile profile = core::Profile::createNew();
    profile.name = QStringLiteral("Nouveau profil");
    m_profileStore.upsert(profile);
    m_profileStore.sortByRecentUse();
    m_profiles->refresh();
    selectProfile(profile.id);
    return profile.id;
}

QString AppController::duplicateProfile(const QString& id)
{
    const core::Profile* source = m_profileStore.byId(id);
    if (!source)
        return {};

    const core::Profile fresh = core::Profile::createNew();
    core::Profile copy = *source;
    copy.id = fresh.id;
    copy.createdAt = fresh.createdAt;
    copy.lastUsedAt = {};
    copy.name = source->name + QStringLiteral(" (copie)");
    m_profileStore.upsert(copy);
    m_profileStore.sortByRecentUse();
    m_profiles->refresh();
    selectProfile(copy.id);
    return copy.id;
}

void AppController::renameProfile(const QString& id, const QString& name)
{
    const QString trimmed = name.trimmed();
    const core::Profile* profile = m_profileStore.byId(id);
    if (trimmed.isEmpty() || !profile || profile->name == trimmed)
        return;

    core::Profile updated = *profile;
    updated.name = trimmed;
    m_profileStore.upsert(updated);
    m_profiles->notifyChanged(id);
    if (m_hasCurrent && m_current.id == id) {
        m_current.name = trimmed;
        emit currentProfileChanged();
        recompute();
    }
}

void AppController::removeProfile(const QString& id)
{
    const bool wasCurrent = m_hasCurrent && m_current.id == id;
    if (!m_profileStore.remove(id))
        return;
    m_profiles->refresh();
    if (wasCurrent) {
        selectProfile(m_profileStore.count() > 0 ? m_profileStore.profiles().constFirst().id
                                                : QString());
    }
}

void AppController::copyCommand()
{
    if (QClipboard* clipboard = QGuiApplication::clipboard())
        clipboard->setText(m_commandLine);
}

void AppController::revealModelFile(const QString& id)
{
    const core::Profile* profile = m_profileStore.byId(id);
    if (!profile || profile->modelPath.isEmpty())
        return;

    const QString native = QDir::toNativeSeparators(profile->modelPath);
    if (QFileInfo::exists(native)) {
        QProcess::startDetached(QStringLiteral("explorer.exe"),
                                { QStringLiteral("/select,") + native });
    } else {
        // Le fichier a disparu : ouvrir le dossier parent reste utile.
        QProcess::startDetached(
            QStringLiteral("explorer.exe"),
            { QDir::toNativeSeparators(QFileInfo(native).absolutePath()) });
    }
}

void AppController::applySettings(const QString& serverPath, const QString& cliPath,
                                  const QString& modelsDir, int intervalMs, const QString& theme)
{
    core::Settings settings = m_settingsStore.settings();
    settings.llamaServerPath = serverPath.trimmed();
    settings.llamaCliPath = cliPath.trimmed();
    settings.defaultModelsDir = modelsDir.trimmed();
    settings.monitorIntervalMs = qBound(250, intervalMs, 10000);
    settings.theme = theme.isEmpty() ? QStringLiteral("dark") : theme;
    // Écriture immédiate : un changement de chemin est pris en compte sans
    // redémarrage (critère d'acceptation n°5).
    m_settingsStore.setSettings(settings);
}

bool AppController::pathExists(const QString& path) const
{
    const QString trimmed = path.trimmed();
    return !trimmed.isEmpty() && QFileInfo::exists(trimmed);
}

QString AppController::localFile(const QUrl& url) const
{
    return url.isLocalFile() ? url.toLocalFile() : url.toString();
}

QUrl AppController::fileUrl(const QString& path) const
{
    return path.trimmed().isEmpty() ? QUrl() : QUrl::fromLocalFile(path.trimmed());
}

QString AppController::nativePath(const QString& path) const
{
    return QDir::toNativeSeparators(path);
}

QString AppController::fileName(const QString& path) const
{
    return QFileInfo(path).fileName();
}

QString AppController::uiFontFamily() const
{
    return Fonts::uiFamily();
}

QString AppController::monoFontFamily() const
{
    return Fonts::monoFamily();
}

QString AppController::dataDir() const
{
    return QDir::toNativeSeparators(core::AppPaths::dataDir());
}

QVariantMap AppController::analyseCommand(const QString& text) const
{
    const core::ParsedCommand parsed = core::CommandParser::parse(text, m_registry);

    QVariantMap result;
    result.insert(QStringLiteral("ok"), parsed.isValid());
    result.insert(QStringLiteral("error"), parsed.error);
    result.insert(QStringLiteral("empty"), parsed.isEmpty());
    result.insert(QStringLiteral("executable"), parsed.executablePath);
    result.insert(QStringLiteral("modelPath"), parsed.modelPath);
    result.insert(QStringLiteral("extraArgs"), parsed.extraArgs);
    result.insert(QStringLiteral("notes"), parsed.notes);
    result.insert(QStringLiteral("binary"),
                  parsed.binaryDetected ? core::binaryKindToString(parsed.binary) : QString());

    QVariantList entries;
    entries.reserve(parsed.entries.size());
    for (const core::ParsedEntry& entry : parsed.entries) {
        entries.append(QVariantMap{
            { QStringLiteral("label"), entry.label },
            { QStringLiteral("flag"), entry.flag },
            { QStringLiteral("value"), entry.value },
            { QStringLiteral("applies"), entry.appliesToBinary },
        });
    }
    result.insert(QStringLiteral("entries"), entries);

    // La commande que l'application produira, pour la comparer à celle collée :
    // c'est la seule façon de voir d'un coup d'œil ce que l'import normalise.
    core::Profile preview = m_hasCurrent ? m_current : core::Profile::createNew();
    core::CommandParser::applyTo(parsed, preview);
    result.insert(
        QStringLiteral("preview"),
        core::CommandBuilder::build(preview, m_settingsStore.settings(), m_registry).displayLine);

    return result;
}

bool AppController::importCommandIntoCurrent(const QString& text)
{
    if (!m_hasCurrent)
        return false;
    const core::ParsedCommand parsed = core::CommandParser::parse(text, m_registry);
    if (!parsed.isValid())
        return false;

    core::CommandParser::applyTo(parsed, m_current);
    // Le formulaire est reconstruit depuis la nouvelle table : les paramètres
    // absents de la ligne collée doivent disparaître, pas subsister de l'état
    // précédent.
    m_form->setSource(m_current.binary, m_current.params);
    persistCurrent();
    m_profiles->notifyChanged(m_current.id);

    emit modelPathChanged();
    emit binaryChanged();
    emit extraArgsChanged();
    emit paramsRevisionChanged();
    recompute();
    return true;
}

QString AppController::importCommandAsNewProfile(const QString& text, const QString& name)
{
    const core::ParsedCommand parsed = core::CommandParser::parse(text, m_registry);
    if (!parsed.isValid())
        return {};

    core::Profile profile = core::Profile::createNew();
    profile.name = name.trimmed().isEmpty() ? QStringLiteral("Commande importée") : name.trimmed();
    core::CommandParser::applyTo(parsed, profile);

    m_profileStore.upsert(profile);
    m_profileStore.sortByRecentUse();
    m_profiles->refresh();
    selectProfile(profile.id);
    return profile.id;
}

// --- Exécution (§10) ---------------------------------------------------------

void AppController::connectRunner()
{
    connect(&m_runner, &core::LlamaRunner::linesProduced, this, &AppController::onRunnerLines);
    connect(&m_runner, &core::LlamaRunner::finished, this, &AppController::onRunnerFinished);
    connect(&m_runner, &core::LlamaRunner::failedToStart, this, [this](const QString& reason) {
        m_profiles->setRunningProfileId(QString());
        m_logs->appendMeta(reason);
        setRunStatus(reason, true);
        emit runStateChanged();
    });
    connect(&m_runner, &core::LlamaRunner::stateChanged, this, [this] {
        if (m_runner.state() == core::RunState::Running) {
            // Le PID n'existe qu'une fois le processus démarré : c'est ici, et
            // pas au lancement, que le moniteur apprend qui suivre (§9).
            m_monitor->setTrackedPid(m_runner.pid());
            setRunStatus(QStringLiteral("En cours — PID %1").arg(m_runner.pid()), false);
        } else if (m_runner.state() == core::RunState::Stopping) {
            setRunStatus(QStringLiteral("Arrêt demandé…"), m_runFailed);
        }
        emit runStateChanged();
    });

    // Un processus qui survivrait à l'application serait invisible et
    // increvable (§10). La confirmation de fermeture est le chemin normal ;
    // ceci en est le filet.
    if (QCoreApplication* app = QCoreApplication::instance())
        connect(app, &QCoreApplication::aboutToQuit, this, &AppController::killProcess);
}

bool AppController::launch()
{
    if (!m_hasCurrent || !canLaunch() || m_runner.isRunning())
        return false;

    const core::BuiltCommand command =
        core::CommandBuilder::build(m_current, m_settingsStore.settings(), m_registry);

    m_logs->clear();
    setServerUrl({});
    m_runFailed = false;
    // La commande ouvre le journal : c'est elle qu'on relit quand on cherche
    // pourquoi une exécution s'est mal passée.
    m_logs->appendMeta(command.displayLine);

    // Avant start() : sous Windows, QProcess émet started() sans repasser par la
    // boucle d'événements, et « Démarrage… » écraserait « En cours ».
    setLogsVisible(true);
    setRunStatus(QStringLiteral("Démarrage…"), false);

    if (!m_runner.start(command))
        return false;

    // La pastille de la liste désigne le profil qui tourne (§5.2).
    m_profiles->setRunningProfileId(m_current.id);

    // §10 : lastUsedAt est mis à jour au lancement. La liste n'est délibérément
    // pas retriée dans la foulée — voir §5.2, le tri s'applique au chargement :
    // déplacer la ligne sous le curseur au moment du clic serait déroutant.
    m_current.lastUsedAt = QDateTime::currentDateTimeUtc();
    persistCurrent();
    m_profiles->notifyChanged(m_current.id);

    emit runStateChanged();
    return true;
}

void AppController::stopThenLaunch()
{
    if (!m_runner.isRunning()) {
        launch();
        return;
    }
    m_relaunchPending = true;
    m_runner.requestStop();
}

void AppController::stopProcess()
{
    m_runner.requestStop();
}

void AppController::killProcess()
{
    m_relaunchPending = false;
    m_runner.killNow();
}

void AppController::openServerInBrowser()
{
    if (!m_serverUrl.isEmpty())
        QDesktopServices::openUrl(QUrl(m_serverUrl));
}

void AppController::copyLogs()
{
    if (QClipboard* clipboard = QGuiApplication::clipboard())
        clipboard->setText(m_logs->allText());
}

void AppController::onRunnerLines(const QStringList& lines)
{
    m_logs->appendLines(lines);

    if (m_current.binary != core::BinaryKind::Server)
        return;

    for (const QString& line : lines) {
        if (core::ServerLog::isBindFailure(line)) {
            setRunStatus(QStringLiteral("Le socket HTTP n'a pas pu être lié — port déjà pris ?"),
                         true);
            emit runStateChanged();
            continue;
        }
        // L'URL est prise dans le journal, pas recomposée depuis --host et
        // --port : c'est celle que le serveur a réellement liée (§5.5).
        const QString url = core::ServerLog::listeningUrl(line);
        if (!url.isEmpty())
            setServerUrl(core::ServerLog::browsableUrl(url));
    }
}

void AppController::onRunnerFinished(int exitCode, bool crashed, bool requested)
{
    m_monitor->setTrackedPid(0);
    m_profiles->setRunningProfileId(QString());
    setServerUrl({});

    QString message;
    bool failed = false;
    if (requested) {
        message = QStringLiteral("Arrêté.");
    } else if (crashed) {
        message = QStringLiteral("Le processus s'est interrompu.");
        failed = true;
    } else {
        message = QStringLiteral("Terminé — code de sortie %1").arg(exitCode);
        failed = (exitCode != 0);
    }

    m_logs->appendMeta(message);
    setRunStatus(message, failed);
    emit runStateChanged();

    if (m_relaunchPending) {
        m_relaunchPending = false;
        launch();
    }
}

void AppController::setRunStatus(const QString& status, bool failed)
{
    if (m_runStatus == status && m_runFailed == failed)
        return;
    m_runStatus = status;
    m_runFailed = failed;
    emit runStateChanged();
}

void AppController::setServerUrl(const QString& url)
{
    if (m_serverUrl == url)
        return;
    m_serverUrl = url;
    emit serverUrlChanged();
}

void AppController::setLogsVisible(bool visible)
{
    if (m_logsVisible == visible)
        return;
    m_logsVisible = visible;
    emit logsVisibleChanged();
}

void AppController::persistCurrent()
{
    if (m_hasCurrent)
        m_profileStore.upsert(m_current);
}

void AppController::recompute()
{
    const QString previousCommand = m_commandLine;
    const QString previousError = m_validationError;

    if (m_hasCurrent) {
        m_commandLine =
            core::CommandBuilder::build(m_current, m_settingsStore.settings(), m_registry)
                .displayLine;
    } else {
        m_commandLine.clear();
    }
    m_validationError = computeValidationError();

    if (m_commandLine != previousCommand)
        emit commandLineChanged();
    if (m_validationError != previousError)
        emit validationChanged();
}

QString AppController::computeValidationError() const
{
    if (m_registry.isEmpty())
        return m_startupWarning;
    if (!m_hasCurrent)
        return QStringLiteral("Aucun profil sélectionné.");
    if (m_current.name.trimmed().isEmpty())
        return QStringLiteral("Le profil doit porter un nom.");
    if (m_current.modelPath.isEmpty())
        return QStringLiteral("Aucun modèle choisi pour ce profil.");
    if (!QFileInfo::exists(m_current.modelPath)) {
        return QStringLiteral("Fichier modèle introuvable : %1")
            .arg(QDir::toNativeSeparators(m_current.modelPath));
    }

    const QString executable = m_settingsStore.settings().executableFor(m_current.binary);
    if (executable.trimmed().isEmpty()) {
        return QStringLiteral("Le chemin de %1 n'est pas renseigné dans les Réglages.")
            .arg(core::CommandBuilder::defaultExecutableName(m_current.binary));
    }
    if (!QFileInfo::exists(executable)) {
        return QStringLiteral("Exécutable introuvable : %1")
            .arg(QDir::toNativeSeparators(executable));
    }
    return {};
}

} // namespace ui
