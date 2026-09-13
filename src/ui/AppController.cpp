#include "ui/AppController.h"

#include "core/AppPaths.h"
#include "core/CommandBuilder.h"
#include "ui/Fonts.h"
#include "ui/ParamFormModel.h"
#include "ui/ProfileListModel.h"

#include <QClipboard>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QProcess>

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
