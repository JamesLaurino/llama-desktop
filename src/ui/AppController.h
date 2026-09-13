#pragma once

#include "core/ParamRegistry.h"
#include "core/Profile.h"
#include "core/ProfileStore.h"
#include "core/SettingsStore.h"
// Les deux modèles sont exposés comme propriétés : moc exige des types complets.
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

private:
    void initialise(const QString& paramsPath);
    void persistCurrent();
    void recompute();
    QString computeValidationError() const;

    core::ParamRegistry m_registry;
    QString m_paramsPath;
    QString m_startupWarning;

    core::ProfileStore m_profileStore;
    core::SettingsStore m_settingsStore;

    ProfileListModel* m_profiles = nullptr;
    ParamFormModel* m_form = nullptr;

    core::Profile m_current;
    bool m_hasCurrent = false;

    QString m_commandLine;
    QString m_validationError;
};

} // namespace ui
