#pragma once

#include "monitor/MonitorSample.h"
#include "monitor/NvmlLibrary.h"

#include <memory>

namespace monitor {

/// Occupation VRAM du GPU 0, par NVML.
///
/// `initialise()` doit être appelé depuis le fil qui échantillonnera : `nvmlInit_v2`
/// coûte 100 à 300 ms au premier appel, et les appels NVML se sérialisent avec le
/// pilote — ils bloquent précisément quand le GPU est occupé à charger un modèle.
class NvmlMonitor
{
public:
    explicit NvmlMonitor(QStringList candidates = NvmlLibrary::defaultCandidates());
    ~NvmlMonitor();

    NvmlMonitor(const NvmlMonitor&) = delete;
    NvmlMonitor& operator=(const NvmlMonitor&) = delete;

    /// Charge la bibliothèque, initialise NVML, retient le GPU 0 et son nom.
    /// Idempotent. Faux si aucun GPU NVIDIA n'est exploitable ; `error()` dit
    /// pourquoi et l'application reste utilisable (critère d'acceptation n°6).
    bool initialise();

    bool isAvailable() const { return m_device != nullptr; }
    QString error() const { return m_error; }
    QString gpuName() const { return m_gpuName; }

    /// Complète les champs GPU de `out`. `trackedPid` à 0 n'interroge pas la
    /// liste des processus.
    void sample(MonitorSample& out, quint32 trackedPid) const;

private:
    /// VRAM du processus. `found` dit si le PID figure dans la liste des clients
    /// de calcul, ce qui reste vrai même quand la valeur est indisponible.
    quint64 processMemory(quint32 pid, quint64 total, bool* found) const;

    std::unique_ptr<NvmlLibrary> m_library;
    QStringList m_candidates;
    /// `initialise()` a été tenté — distinct de « une session NVML est ouverte ».
    bool m_initialised = false;
    /// `nvmlInit_v2` a réussi : seul cas où `nvmlShutdown` doit être appelé.
    /// Sans cette distinction, le destructeur déréférence un pointeur nul quand
    /// la bibliothèque n'a jamais pu être chargée.
    bool m_sessionOpen = false;
    NvmlLibrary::Device m_device = nullptr;
    QString m_gpuName;
    QString m_error;
};

} // namespace monitor
