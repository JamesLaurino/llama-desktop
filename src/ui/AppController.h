#pragma once

#include "core/LlamaRunner.h"
#include "core/ParamRegistry.h"
#include "core/Profile.h"
#include "core/ProfileStore.h"
#include "core/SettingsStore.h"
// Modèles et moniteur sont exposés comme propriétés : moc exige des types complets.
#include "ui/LogModel.h"
#include "ui/MonitorController.h"
#include "ui/ParamFormModel.h"
#include "ui/ProfileListModel.h"

#include <QObject>
#include <QQmlEngine>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

namespace ui {

/// Façade unique entre le noyau et le QML.
///
/// Détient le registre, les deux dépôts et les modèles ; c'est aussi la couture
/// par laquelle la phase 4 branchera LlamaRunner sans toucher aux vues.
///
/// Le profil courant est édité sur une copie de travail (`m_current`), écrite
/// dans le dépôt à chaque mutation : l'anti-rebond de ProfileStore absorbe la
/// frappe, il n'y a donc pas de bouton « Enregistrer » (§5.2 : une sélection
/// charge instantanément et « Lancer » est aussitôt disponible).
class AppController : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(App)
    QML_SINGLETON

    Q_PROPERTY(ui::ProfileListModel* profiles READ profiles CONSTANT)
    Q_PROPERTY(ui::ParamFormModel* params READ params CONSTANT)
    Q_PROPERTY(QVariantList sections READ sections CONSTANT)
    Q_PROPERTY(QVariantMap sectionSetCounts READ sectionSetCounts NOTIFY paramsRevisionChanged)
    Q_PROPERTY(ui::MonitorController* monitor READ monitor CONSTANT)
    Q_PROPERTY(ui::LogModel* logs READ logs CONSTANT)

    Q_PROPERTY(QString currentProfileId READ currentProfileId NOTIFY currentProfileChanged)
    Q_PROPERTY(bool hasProfile READ hasProfile NOTIFY currentProfileChanged)
    Q_PROPERTY(QString profileName READ profileName NOTIFY currentProfileChanged)
    Q_PROPERTY(QString modelPath READ modelPath WRITE setModelPath NOTIFY modelPathChanged)
    Q_PROPERTY(QString binary READ binary WRITE setBinary NOTIFY binaryChanged)
    Q_PROPERTY(QString extraArgs READ extraArgs WRITE setExtraArgs NOTIFY extraArgsChanged)
    Q_PROPERTY(QString notes READ notes WRITE setNotes NOTIFY notesChanged)

    Q_PROPERTY(QString commandLine READ commandLine NOTIFY commandLineChanged)
    Q_PROPERTY(QString validationError READ validationError NOTIFY validationChanged)
    Q_PROPERTY(bool canLaunch READ canLaunch NOTIFY validationChanged)

    // --- Exécution (§10) -----------------------------------------------------
    Q_PROPERTY(bool running READ isRunning NOTIFY runStateChanged)
    Q_PROPERTY(bool stopping READ isStopping NOTIFY runStateChanged)
    Q_PROPERTY(QString runState READ runState NOTIFY runStateChanged)
    Q_PROPERTY(QString runStatus READ runStatus NOTIFY runStateChanged)
    /// La dernière exécution s'est mal terminée : en-tête du panneau en rouge,
    /// logs conservés à l'écran (§10).
    Q_PROPERTY(bool runFailed READ runFailed NOTIFY runStateChanged)
    Q_PROPERTY(QString serverUrl READ serverUrl NOTIFY serverUrlChanged)
    Q_PROPERTY(bool logsVisible READ logsVisible WRITE setLogsVisible NOTIFY logsVisibleChanged)

    Q_PROPERTY(QString llamaServerPath READ llamaServerPath NOTIFY settingsChanged)
    Q_PROPERTY(QString llamaCliPath READ llamaCliPath NOTIFY settingsChanged)
    Q_PROPERTY(QString defaultModelsDir READ defaultModelsDir NOTIFY settingsChanged)
    Q_PROPERTY(int monitorIntervalMs READ monitorIntervalMs NOTIFY settingsChanged)
    Q_PROPERTY(QString theme READ theme NOTIFY settingsChanged)

    Q_PROPERTY(QString uiFontFamily READ uiFontFamily CONSTANT)
    Q_PROPERTY(QString monoFontFamily READ monoFontFamily CONSTANT)
    Q_PROPERTY(QString dataDir READ dataDir CONSTANT)
    Q_PROPERTY(QString paramsSource READ paramsSource CONSTANT)
    Q_PROPERTY(QString targetBuild READ targetBuild CONSTANT)
    Q_PROPERTY(QString startupWarning READ startupWarning CONSTANT)

public:
    explicit AppController(QObject* parent = nullptr);
    /// Fabrique appelée par le moteur QML pour le singleton « App ».
    static AppController* create(QQmlEngine* engine, QJSEngine* scriptEngine);
    /// Chemins explicites : utilisé par les tests pour ne pas toucher %APPDATA%.
    AppController(const QString& paramsPath, const QString& profilesPath,
                  const QString& settingsPath, QObject* parent = nullptr);

    ProfileListModel* profiles() const { return m_profiles; }
    ParamFormModel* params() const { return m_form; }
    MonitorController* monitor() const { return m_monitor; }
    LogModel* logs() const { return m_logs; }
    QVariantList sections() const;
    /// Nombre de paramètres posés par section : alimente le compteur et
    /// l'activation du bouton « Réinitialiser » de chaque section.
    QVariantMap sectionSetCounts() const;

    QString currentProfileId() const { return m_hasCurrent ? m_current.id : QString(); }
    bool hasProfile() const { return m_hasCurrent; }
    QString profileName() const { return m_current.name; }

    QString modelPath() const { return m_current.modelPath; }
    void setModelPath(const QString& path);
    QString binary() const;
    void setBinary(const QString& kind);
    QString extraArgs() const { return m_current.extraArgs; }
    void setExtraArgs(const QString& args);
    QString notes() const { return m_current.notes; }
    void setNotes(const QString& notes);

    QString commandLine() const { return m_commandLine; }
    QString validationError() const { return m_validationError; }
    bool canLaunch() const { return m_validationError.isEmpty(); }

    bool isRunning() const { return m_runner.isRunning(); }
    bool isStopping() const { return m_runner.state() == core::RunState::Stopping; }
    QString runState() const { return core::runStateToString(m_runner.state()); }
    QString runStatus() const { return m_runStatus; }
    bool runFailed() const { return m_runFailed; }
    QString serverUrl() const { return m_serverUrl; }
    bool logsVisible() const { return m_logsVisible; }
    void setLogsVisible(bool visible);

    QString llamaServerPath() const { return m_settingsStore.settings().llamaServerPath; }
    QString llamaCliPath() const { return m_settingsStore.settings().llamaCliPath; }
    QString defaultModelsDir() const { return m_settingsStore.settings().defaultModelsDir; }
    int monitorIntervalMs() const { return m_settingsStore.settings().monitorIntervalMs; }
    QString theme() const { return m_settingsStore.settings().theme; }

    QString uiFontFamily() const;
    QString monoFontFamily() const;
    QString dataDir() const;
    QString paramsSource() const { return m_paramsPath; }
    QString targetBuild() const { return m_registry.targetBuild(); }
    QString startupWarning() const { return m_startupWarning; }

    Q_INVOKABLE QString createProfile();
    Q_INVOKABLE QString duplicateProfile(const QString& id);
    Q_INVOKABLE void renameProfile(const QString& id, const QString& name);
    Q_INVOKABLE void removeProfile(const QString& id);
    Q_INVOKABLE void selectProfile(const QString& id);

    Q_INVOKABLE void setParamValue(const QString& key, const QString& value);
    Q_INVOKABLE void resetSection(const QString& sectionId);

    Q_INVOKABLE void copyCommand();
    Q_INVOKABLE void revealModelFile(const QString& id);

    /// Analyse une ligne collée sans rien modifier : alimente l'aperçu du
    /// dialogue d'import. Rien n'est écrit avant que l'utilisateur ne tranche.
    Q_INVOKABLE QVariantMap analyseCommand(const QString& text) const;
    /// Écrase modèle, paramètres et arguments libres du profil courant.
    /// Le nom, les notes et l'identifiant sont conservés.
    Q_INVOKABLE bool importCommandIntoCurrent(const QString& text);
    /// Crée un profil depuis une ligne collée et le sélectionne.
    /// Renvoie son identifiant, ou une chaîne vide si la ligne est inexploitable.
    Q_INVOKABLE QString importCommandAsNewProfile(const QString& text, const QString& name);

    /// Lance le profil courant. Faux si la validation bloque ou si un processus
    /// tourne déjà : le §10 n'autorise qu'un processus à la fois.
    Q_INVOKABLE bool launch();
    /// Arrête le précédent puis lance le profil courant dès qu'il a rendu la
    /// main. C'est la proposition du §10 quand on lance un autre profil.
    Q_INVOKABLE void stopThenLaunch();
    /// Arrêt propre ; un second appel pendant le délai de grâce force.
    Q_INVOKABLE void stopProcess();
    /// Arrêt immédiat, pour la fermeture de l'application.
    Q_INVOKABLE void killProcess();
    Q_INVOKABLE void openServerInBrowser();
    Q_INVOKABLE void copyLogs();

    Q_INVOKABLE void applySettings(const QString& serverPath, const QString& cliPath,
                                  const QString& modelsDir, int intervalMs, const QString& theme);

    Q_INVOKABLE bool pathExists(const QString& path) const;
    Q_INVOKABLE QString localFile(const QUrl& url) const;
    Q_INVOKABLE QUrl fileUrl(const QString& path) const;
    Q_INVOKABLE QString nativePath(const QString& path) const;
    Q_INVOKABLE QString fileName(const QString& path) const;

signals:
    void currentProfileChanged();
    void modelPathChanged();
    void binaryChanged();
    void extraArgsChanged();
    void notesChanged();
    void commandLineChanged();
    void validationChanged();
    void settingsChanged();
    void paramsRevisionChanged();
    void runStateChanged();
    void serverUrlChanged();
    void logsVisibleChanged();

private:
    void initialise(const QString& paramsPath);
    void persistCurrent();
    void recompute();
    QString computeValidationError() const;

    void connectRunner();
    void onRunnerLines(const QStringList& lines);
    void onRunnerFinished(int exitCode, bool crashed, bool requested);
    void setRunStatus(const QString& status, bool failed);
    void setServerUrl(const QString& url);

    core::ParamRegistry m_registry;
    QString m_paramsPath;
    QString m_startupWarning;

    core::ProfileStore m_profileStore;
    core::SettingsStore m_settingsStore;

    ProfileListModel* m_profiles = nullptr;
    ParamFormModel* m_form = nullptr;
    MonitorController* m_monitor = nullptr;
    LogModel* m_logs = nullptr;

    /// Membre et non pointeur : sa destruction, garantie avant celle des
    /// dépôts, tue le processus (§10, aucun orphelin).
    core::LlamaRunner m_runner;
    QString m_runStatus;
    bool m_runFailed = false;
    QString m_serverUrl;
    bool m_logsVisible = false;
    /// Relance en attente de la fin du processus précédent.
    bool m_relaunchPending = false;

    core::Profile m_current;
    bool m_hasCurrent = false;

    QString m_commandLine;
    QString m_validationError;
};

} // namespace ui
