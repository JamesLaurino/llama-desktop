#pragma once

#include <QLibrary>
#include <QString>
#include <QStringList>

namespace monitor {

/// Chargement dynamique de `nvml.dll` et déclaration minimale de son ABI.
///
/// Le §9 interdit le lien statique : l'application doit démarrer sans pilote
/// NVIDIA. NVIDIA ne distribue `nvml.h` qu'avec le CUDA Toolkit, absent des
/// postes de développement comme des postes utilisateurs ; les huit signatures
/// que nous utilisons sont donc déclarées ici.
///
/// Les suffixes `_v2` / `_v3` désignent des ABI figées par NVIDIA — c'est
/// exactement ce mécanisme qui rend cette déclaration légitime. Les
/// `static_assert` du .cpp gèlent nos hypothèses de taille : une erreur devient
/// une erreur de compilation, pas une corruption de pile silencieuse.
///
/// `QLibrary` appelle `LoadLibraryW` sous Windows et fournit en plus le message
/// d'erreur du système : c'est la même chose que ce que demande le §9, en moins
/// de lignes.
class NvmlLibrary
{
public:
    /// Poignée opaque de GPU. NVML ne la déréférence jamais côté appelant.
    using Device = void*;

    enum Status { Success = 0, InsufficientSize = 7 };

    /// `nvmlMemory_v2_t`. `version` doit être renseigné **avant** l'appel, sinon
    /// NVML rejette la requête : c'est ce champ qui verrouille l'ABI.
    struct Memory {
        unsigned int version;
        unsigned long long total;
        unsigned long long reserved;
        unsigned long long free;
        unsigned long long used;
    };

    /// `nvmlProcessInfo_t` tel qu'attendu par `..._v3`.
    struct ProcessInfo {
        unsigned int pid;
        unsigned long long usedGpuMemory;
        unsigned int gpuInstanceId;
        unsigned int computeInstanceId;
    };

    struct Utilisation {
        unsigned int gpu;
        unsigned int memory;
    };

    /// Valeur attendue dans `Memory::version` : taille de la structure marquée
    /// de son numéro de version dans les huit bits hauts.
    static unsigned int memoryVersion();

    /// Faux si NVML a répondu `NVML_VALUE_NOT_AVAILABLE`, ou une valeur
    /// manifestement absurde au regard de `limit`. Sans ce filtre, la sentinelle
    /// `0xFFFFFFFFFFFFFFFF` s'afficherait comme 17 592 186 044 415 Mio.
    static bool isReportedMemory(unsigned long long value, unsigned long long limit);

    /// `System32` d'abord — il est dans le chemin de recherche par défaut de
    /// `LoadLibraryW` — puis l'emplacement historique de NVSMI.
    static QStringList defaultCandidates();

    explicit NvmlLibrary(QStringList candidates = defaultCandidates());
    ~NvmlLibrary() = default;

    NvmlLibrary(const NvmlLibrary&) = delete;
    NvmlLibrary& operator=(const NvmlLibrary&) = delete;

    bool isLoaded() const { return m_loaded; }
    QString error() const { return m_error; }
    QString path() const { return m_path; }

    QString errorString(int code) const;

    int init() const { return m_init(); }
    int shutdown() const { return m_shutdown(); }
    int deviceByIndex(unsigned int index, Device* device) const
    {
        return m_deviceByIndex(index, device);
    }
    int deviceName(Device device, char* buffer, unsigned int length) const
    {
        return m_deviceName(device, buffer, length);
    }
    int memoryInfo(Device device, Memory* memory) const { return m_memoryInfo(device, memory); }
    int computeProcesses(Device device, unsigned int* count, ProcessInfo* infos) const
    {
        return m_computeProcesses(device, count, infos);
    }
    int utilisationRates(Device device, Utilisation* rates) const
    {
        return m_utilisation(device, rates);
    }

private:
    using InitFn = int (*)();
    using ShutdownFn = int (*)();
    using DeviceByIndexFn = int (*)(unsigned int, Device*);
    using DeviceNameFn = int (*)(Device, char*, unsigned int);
    using MemoryInfoFn = int (*)(Device, Memory*);
    using ComputeProcessesFn = int (*)(Device, unsigned int*, ProcessInfo*);
    using UtilisationFn = int (*)(Device, Utilisation*);
    using ErrorStringFn = const char* (*)(int);

    bool resolveAll(QLibrary& library);

    bool m_loaded = false;
    QString m_error;
    QString m_path;

    InitFn m_init = nullptr;
    ShutdownFn m_shutdown = nullptr;
    DeviceByIndexFn m_deviceByIndex = nullptr;
    DeviceNameFn m_deviceName = nullptr;
    MemoryInfoFn m_memoryInfo = nullptr;
    ComputeProcessesFn m_computeProcesses = nullptr;
    UtilisationFn m_utilisation = nullptr;
    ErrorStringFn m_errorString = nullptr;
};

} // namespace monitor
