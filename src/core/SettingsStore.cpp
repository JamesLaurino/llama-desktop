#include "core/SettingsStore.h"

#include "core/JsonFile.h"

#include <QJsonObject>

namespace core {

SettingsStore::SettingsStore(QString filePath, QObject* parent)
    : QObject(parent)
    , m_filePath(std::move(filePath))
{
}

bool SettingsStore::load()
{
    m_settings = Settings{};
    m_lastError.clear();
    m_corruptBackupPath.clear();

    const JsonFile::ReadResult result = JsonFile::read(m_filePath);
    if (result.corrupt) {
        m_lastError = result.error;
        m_corruptBackupPath = result.backupPath;
        emit settingsChanged();
        return false;
    }
    if (!result.ok()) {
        m_lastError = result.error;
        emit settingsChanged();
        return false;
    }
    if (result.existed && result.document.isObject())
        m_settings = Settings::fromJson(result.document.object());

    emit settingsChanged();
    return true;
}

bool SettingsStore::save()
{
    QString error;
    if (!JsonFile::write(m_filePath, QJsonDocument(m_settings.toJson()), &error)) {
        m_lastError = error;
        emit saveFailed(error);
        return false;
    }
    m_lastError.clear();
    return true;
}

void SettingsStore::setSettings(const Settings& settings)
{
    m_settings = settings;
    emit settingsChanged();
    save();
}

} // namespace core
