#pragma once

#include <QAbstractListModel>
#include <QQmlEngine>

namespace core {
class ProfileStore;
}

namespace ui {

/// Vue liste des profils du dépôt (§5.2).
///
/// Le dépôt reste la source de vérité : ce modèle ne détient aucune donnée et
/// se contente de projeter `ProfileStore::profiles()` en rôles.
class ProfileListModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Exposé par App.profiles.")

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        NameRole,
        ModelFileNameRole,
        ModelPathRole,
        BinaryRole,
        LastUsedRole,
        RunningRole, ///< toujours faux tant que la phase 4 n'a pas de processus
    };

    explicit ProfileListModel(core::ProfileStore* store, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    /// À appeler quand la composition ou l'ordre de la liste change.
    void refresh();
    /// À appeler quand un seul profil déjà présent a changé d'apparence.
    void notifyChanged(const QString& id);

    int rowOfId(const QString& id) const;
    QString idAt(int row) const;

private:
    core::ProfileStore* m_store;
};

} // namespace ui
