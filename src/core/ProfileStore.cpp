#include "core/ProfileStore.h"

#include "core/JsonFile.h"

#include <QJsonArray>
#include <QJsonObject>
#include <algorithm>

namespace core {

ProfileStore::ProfileStore(QString filePath, QObject* parent)
    : QObject(parent)
    , m_filePath(std::move(filePath))
{
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(kSaveDebounceMs);
    connect(&m_debounce, &QTimer::timeout, this, [this] { saveNow(); });
}

bool ProfileStore::load()
{
    m_profiles.clear();
    m_lastError.clear();
    m_corruptBackupPath.clear();

    const JsonFile::ReadResult result = JsonFile::read(m_filePath);
    if (result.corrupt) {
        m_lastError = result.error;
        m_corruptBackupPath = result.backupPath;
        emit profilesChanged();
        return false;
    }
    if (!result.ok()) {
        m_lastError = result.error;
        emit profilesChanged();
        return false;
    }
    if (!result.existed) {
        emit profilesChanged();
        return true;
    }
    if (!result.document.isArray()) {
        m_lastError = QStringLiteral("%1 ne contient pas un tableau de profils.").arg(m_filePath);
        emit profilesChanged();
        return false;
    }

    const QJsonArray array = result.document.array();
    m_profiles.reserve(array.size());
    for (const QJsonValue& value : array) {
        if (value.isObject())
            m_profiles.append(Profile::fromJson(value.toObject()));
    }

    sortByRecentUse();
    emit profilesChanged();
    return true;
}

int ProfileStore::indexOfId(const QString& id) const
{
    for (qsizetype i = 0; i < m_profiles.size(); ++i) {
        if (m_profiles.at(i).id == id)
            return static_cast<int>(i);
    }
    return -1;
}

const Profile* ProfileStore::byId(const QString& id) const
{
    const int index = indexOfId(id);
    return index < 0 ? nullptr : &m_profiles.at(index);
}

void ProfileStore::upsert(const Profile& profile)
{
    const int index = indexOfId(profile.id);
    if (index < 0)
        m_profiles.append(profile);
    else
        m_profiles[index] = profile;

    emit profilesChanged();
    scheduleSave();
}

bool ProfileStore::remove(const QString& id)
{
    const int index = indexOfId(id);
    if (index < 0)
        return false;

    m_profiles.remove(index);
    emit profilesChanged();
    scheduleSave();
    return true;
}

void ProfileStore::replaceAll(QVector<Profile> profiles)
{
    m_profiles = std::move(profiles);
    emit profilesChanged();
    scheduleSave();
}

void ProfileStore::sortByRecentUse()
{
    std::stable_sort(m_profiles.begin(), m_profiles.end(), [](const Profile& a, const Profile& b) {
        if (a.lastUsedAt.isValid() != b.lastUsedAt.isValid())
            return a.lastUsedAt.isValid();
        if (a.lastUsedAt != b.lastUsedAt)
            return a.lastUsedAt > b.lastUsedAt;
        return a.createdAt > b.createdAt;
    });
}

void ProfileStore::scheduleSave()
{
    m_debounce.start();
}

bool ProfileStore::saveNow()
{
    m_debounce.stop();

    QJsonArray array;
    for (const Profile& profile : m_profiles)
        array.append(profile.toJson());

    QString error;
    if (!JsonFile::write(m_filePath, QJsonDocument(array), &error)) {
        m_lastError = error;
        emit saveFailed(error);
        return false;
    }

    m_lastError.clear();
    emit saved();
    return true;
}

} // namespace core
