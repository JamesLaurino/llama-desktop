#include "monitor/NvmlLibrary.h"

#include <limits>

namespace monitor {

// Hypothèses d'ABI. Un décalage d'alignement corromprait la pile à l'appel :
// mieux vaut échouer ici.
static_assert(sizeof(NvmlLibrary::Memory) == 40, "nvmlMemory_v2_t fait 40 octets");
static_assert(sizeof(NvmlLibrary::ProcessInfo) == 24, "nvmlProcessInfo_t fait 24 octets");
static_assert(sizeof(NvmlLibrary::Utilisation) == 8, "nvmlUtilization_t fait 8 octets");

unsigned int NvmlLibrary::memoryVersion()
{
    return static_cast<unsigned int>(sizeof(Memory)) | (2u << 24);
}

bool NvmlLibrary::isReportedMemory(unsigned long long value, unsigned long long limit)
{
    if (value == std::numeric_limits<unsigned long long>::max())
        return false;
    return limit == 0 || value <= limit;
}

QStringList NvmlLibrary::defaultCandidates()
{
    return { QStringLiteral("nvml"),
             QStringLiteral("C:/Program Files/NVIDIA Corporation/NVSMI/nvml.dll") };
}

NvmlLibrary::NvmlLibrary(QStringList candidates)
{
    QStringList failures;
    for (const QString& candidate : candidates) {
        QLibrary library(candidate);
        if (!library.load()) {
            failures << library.errorString();
            continue;
        }
        if (!resolveAll(library)) {
            failures << QStringLiteral("%1 : symbole NVML manquant").arg(candidate);
            library.unload();
            continue;
        }
        // Volontairement pas de unload() : les pointeurs résolus doivent rester
        // valides pour toute la durée de vie du processus.
        m_loaded = true;
        m_path = library.fileName();
        return;
    }
    m_error = failures.isEmpty() ? QStringLiteral("Aucune bibliothèque NVML à essayer.")
                                 : failures.join(QStringLiteral(" · "));
}

bool NvmlLibrary::resolveAll(QLibrary& library)
{
    m_init = reinterpret_cast<InitFn>(library.resolve("nvmlInit_v2"));
    m_shutdown = reinterpret_cast<ShutdownFn>(library.resolve("nvmlShutdown"));
    m_deviceByIndex =
        reinterpret_cast<DeviceByIndexFn>(library.resolve("nvmlDeviceGetHandleByIndex_v2"));
    m_deviceName = reinterpret_cast<DeviceNameFn>(library.resolve("nvmlDeviceGetName"));
    m_memoryInfo = reinterpret_cast<MemoryInfoFn>(library.resolve("nvmlDeviceGetMemoryInfo_v2"));
    m_computeProcesses = reinterpret_cast<ComputeProcessesFn>(
        library.resolve("nvmlDeviceGetComputeRunningProcesses_v3"));
    m_utilisation =
        reinterpret_cast<UtilisationFn>(library.resolve("nvmlDeviceGetUtilizationRates"));
    m_errorString = reinterpret_cast<ErrorStringFn>(library.resolve("nvmlErrorString"));

    return m_init && m_shutdown && m_deviceByIndex && m_deviceName && m_memoryInfo
        && m_computeProcesses && m_utilisation;
}

QString NvmlLibrary::errorString(int code) const
{
    if (m_errorString) {
        if (const char* text = m_errorString(code))
            return QString::fromLatin1(text);
    }
    return QStringLiteral("erreur NVML %1").arg(code);
}

} // namespace monitor
