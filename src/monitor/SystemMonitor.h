#pragma once

#include "monitor/MonitorSample.h"

namespace monitor {

/// RAM système et RAM du processus suivi, par l'API Win32 (§9).
///
/// Sans état : deux appels indépendants, rien à initialiser.
class SystemMonitor
{
public:
    /// Complète `ramTotal`, `ramUsed` et, si `trackedPid` est non nul, `ramProcess`.
    static void sample(MonitorSample& out, quint32 trackedPid);

    /// `PrivateUsage` du processus, 0 si le handle ne peut pas être ouvert
    /// (processus mort, ou droits insuffisants sur un processus élevé).
    static quint64 privateUsage(quint32 pid);
};

} // namespace monitor
