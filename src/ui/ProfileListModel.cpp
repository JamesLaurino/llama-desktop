#include "ui/ProfileListModel.h"

#include "core/Profile.h"
#include "core/ProfileStore.h"

#include <QFileInfo>

namespace ui {

ProfileListModel::ProfileListModel(core::ProfileStore* store, QObject* parent)
    : QAbstractListModel(parent)
    , m_store(store)
{
}

int ProfileListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_store->count();
}

QVariant ProfileListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_store->count())
        return {};
    const core::Profile& profile = m_store->profiles().at(index.row());

    switch (role) {
    case IdRole:
        return profile.id;
    case NameRole:
        return profile.name.isEmpty() ? QStringLiteral("(sans nom)") : profile.name;
    case ModelFileNameRole:
        return profile.modelPath.isEmpty() ? QStringLiteral("aucun modèle")
                                           : QFileInfo(profile.modelPath).fileName();
    case ModelPathRole:
        return profile.modelPath;
    case BinaryRole:
        return core::binaryKindToString(profile.binary);
    case LastUsedRole:
        return profile.lastUsedAt.isValid()
            ? profile.lastUsedAt.toLocalTime().toString(QStringLiteral("dd/MM/yyyy HH:mm"))
            : QStringLiteral("jamais utilisé");
    case RunningRole:
        return false;
    default:
        return {};
    }
}

QHash<int, QByteArray> ProfileListModel::roleNames() const
{
    return {
        { IdRole, "profileId" },
        { NameRole, "name" },
        { ModelFileNameRole, "modelFileName" },
        { ModelPathRole, "modelPath" },
        { BinaryRole, "binary" },
        { LastUsedRole, "lastUsed" },
        { RunningRole, "running" },
    };
}

void ProfileListModel::refresh()
{
    beginResetModel();
    endResetModel();
}

void ProfileListModel::notifyChanged(const QString& id)
{
    const int row = rowOfId(id);
    if (row < 0)
        return;
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx);
}

int ProfileListModel::rowOfId(const QString& id) const
{
    return m_store->indexOfId(id);
}

QString ProfileListModel::idAt(int row) const
{
    if (row < 0 || row >= m_store->count())
        return {};
    return m_store->profiles().at(row).id;
}

} // namespace ui
