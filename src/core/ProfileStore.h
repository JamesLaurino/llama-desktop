#pragma once

#include "core/Profile.h"

#include <QObject>
#include <QTimer>
#include <QVector>

namespace core {

/// Dépôt des profils : chargement, tri, mutation, sauvegarde atomique différée.
///
/// QObject uniquement pour le minuteur d'anti-rebond et les signaux ; aucune
/// dépendance UI.
class ProfileStore : public QObject
{
    Q_OBJECT

public:
    /// Anti-rebond de sauvegarde (§4.3).
    static constexpr int kSaveDebounceMs = 500;

    explicit ProfileStore(QString filePath, QObject* parent = nullptr);

    /// Charge le fichier. Renvoie faux si un fichier existant était illisible :
    /// il a alors été mis de côté (voir corruptBackupPath) et le dépôt démarre vide.
    bool load();

    const QVector<Profile>& profiles() const { return m_profiles; }
    int count() const { return static_cast<int>(m_profiles.size()); }

    int indexOfId(const QString& id) const;
    const Profile* byId(const QString& id) const;

    /// Insère ou remplace selon l'identifiant, puis programme une sauvegarde.
    void upsert(const Profile& profile);
    bool remove(const QString& id);
    void replaceAll(QVector<Profile> profiles);

    /// Tri par `lastUsedAt` décroissant, les jamais utilisés en dernier (§5.2).
    void sortByRecentUse();

    /// Programme une sauvegarde dans kSaveDebounceMs ; les appels rapprochés
    /// sont fusionnés.
    void scheduleSave();
    bool saveNow();
    bool hasPendingSave() const { return m_debounce.isActive(); }

    QString filePath() const { return m_filePath; }
    QString lastError() const { return m_lastError; }
    QString corruptBackupPath() const { return m_corruptBackupPath; }

signals:
    void profilesChanged();
    void saved();
    void saveFailed(const QString& error);

private:
    QString m_filePath;
    QVector<Profile> m_profiles;
    QTimer m_debounce;
    QString m_lastError;
    QString m_corruptBackupPath;
};

} // namespace core
