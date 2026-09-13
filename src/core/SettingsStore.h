#pragma once

#include "core/Settings.h"

#include <QObject>

namespace core {

/// Dépôt des réglages globaux. Les réglages changent rarement : la sauvegarde
/// est immédiate, sans anti-rebond.
class SettingsStore : public QObject
{
    Q_OBJECT

public:
    explicit SettingsStore(QString filePath, QObject* parent = nullptr);

    bool load();
    bool save();

    const Settings& settings() const { return m_settings; }
    /// Remplace les réglages et écrit aussitôt : un changement de chemin doit
    /// être pris en compte sans redémarrage (critère d'acceptation n°5).
    void setSettings(const Settings& settings);

    QString filePath() const { return m_filePath; }
    QString lastError() const { return m_lastError; }
    QString corruptBackupPath() const { return m_corruptBackupPath; }

signals:
    void settingsChanged();
    void saveFailed(const QString& error);

private:
    QString m_filePath;
    Settings m_settings;
    QString m_lastError;
    QString m_corruptBackupPath;
};

} // namespace core
