#include "monitor/NvmlMonitor.h"

#include <QVarLengthArray>

namespace monitor {

namespace {
/// `NVML_DEVICE_NAME_V2_BUFFER_SIZE`.
constexpr unsigned int kNameBufferSize = 96;
/// Borne de sécurité : la liste des processus de calcul est demandée en deux
/// passes, mais on refuse d'allouer indéfiniment si le pilote ment.
constexpr unsigned int kMaxProcesses = 256;
}

NvmlMonitor::NvmlMonitor(QStringList candidates)
    : m_candidates(std::move(candidates))
{
}

NvmlMonitor::~NvmlMonitor()
{
    if (m_sessionOpen)
        m_library->shutdown();
}

bool NvmlMonitor::initialise()
{
    if (m_initialised)
        return m_device != nullptr;
    m_initialised = true;

    m_library = std::make_unique<NvmlLibrary>(m_candidates);
    if (!m_library->isLoaded()) {
        m_error = QStringLiteral("GPU NVIDIA non détecté (%1).").arg(m_library->error());
        return false;
    }

    if (const int code = m_library->init(); code != NvmlLibrary::Success) {
        m_error = QStringLiteral("NVML refuse de s'initialiser : %1.")
                      .arg(m_library->errorString(code));
        return false;
    }
    m_sessionOpen = true;

    NvmlLibrary::Device device = nullptr;
    if (const int code = m_library->deviceByIndex(0, &device); code != NvmlLibrary::Success) {
        m_error = QStringLiteral("Aucun GPU NVIDIA exploitable : %1.")
                      .arg(m_library->errorString(code));
        m_library->shutdown();
        m_sessionOpen = false;
        return false;
    }

    char name[kNameBufferSize] = {};
    if (m_library->deviceName(device, name, kNameBufferSize) == NvmlLibrary::Success)
        m_gpuName = QString::fromLatin1(name);

    m_device = device;
    return true;
}

void NvmlMonitor::sample(MonitorSample& out, quint32 trackedPid) const
{
    out.gpuAvailable = m_device != nullptr;
    out.gpuName = m_gpuName;
    if (!out.gpuAvailable) {
        out.gpuError = m_error;
        return;
    }

    NvmlLibrary::Memory memory = {};
    memory.version = NvmlLibrary::memoryVersion();
    if (m_library->memoryInfo(m_device, &memory) == NvmlLibrary::Success) {
        out.vramTotal = memory.total;
        out.vramUsed = memory.used;
    }

    NvmlLibrary::Utilisation rates = {};
    out.gpuUtilisation = m_library->utilisationRates(m_device, &rates) == NvmlLibrary::Success
        ? static_cast<int>(rates.gpu)
        : -1;

    if (trackedPid != 0)
        out.vramProcess = processMemory(trackedPid, out.vramTotal, &out.trackedOnGpu);
}

quint64 NvmlMonitor::processMemory(quint32 pid, quint64 total, bool* found) const
{
    *found = false;
    // Deux passes : la première apprend le nombre de processus, la seconde les
    // lit. Un `count` nul est un succès légitime — aucun client CUDA actif.
    unsigned int count = 0;
    const int probe = m_library->computeProcesses(m_device, &count, nullptr);
    if (probe == NvmlLibrary::Success || count == 0)
        return 0;
    if (probe != NvmlLibrary::InsufficientSize || count > kMaxProcesses)
        return 0;

    QVarLengthArray<NvmlLibrary::ProcessInfo, 16> infos(count);
    if (m_library->computeProcesses(m_device, &count, infos.data()) != NvmlLibrary::Success)
        return 0;

    for (unsigned int i = 0; i < count; ++i) {
        const NvmlLibrary::ProcessInfo& info = infos[static_cast<qsizetype>(i)];
        if (info.pid != pid)
            continue;
        *found = true;
        // Sous WDDM, NVML connaît le processus mais pas sa consommation : la
        // sentinelle NVML_VALUE_NOT_AVAILABLE doit devenir 0, pas 16 exaoctets.
        return NvmlLibrary::isReportedMemory(info.usedGpuMemory, total)
            ? info.usedGpuMemory
            : 0;
    }
    return 0;
}

} // namespace monitor
