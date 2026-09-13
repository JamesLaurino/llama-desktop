#include "monitor/SystemMonitor.h"

#ifdef Q_OS_WIN
#  define WIN32_LEAN_AND_MEAN
#  define NOMINMAX
#  include <windows.h>
// psapi.h doit suivre windows.h.
#  include <psapi.h>
#endif

namespace monitor {

void SystemMonitor::sample(MonitorSample& out, quint32 trackedPid)
{
#ifdef Q_OS_WIN
    MEMORYSTATUSEX status = {};
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status)) {
        out.ramTotal = status.ullTotalPhys;
        out.ramUsed = status.ullTotalPhys - status.ullAvailPhys;
    }
#else
    Q_UNUSED(out)
#endif
    out.ramProcess = trackedPid == 0 ? 0 : privateUsage(trackedPid);
}

quint64 SystemMonitor::privateUsage(quint32 pid)
{
#ifdef Q_OS_WIN
    // PROCESS_QUERY_LIMITED_INFORMATION suffit et fonctionne sur des processus
    // qu'un droit plus large refuserait d'ouvrir.
    const HANDLE process =
        OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!process)
        return 0;

    PROCESS_MEMORY_COUNTERS_EX counters = {};
    counters.cb = sizeof(counters);
    const BOOL ok = GetProcessMemoryInfo(
        process, reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), sizeof(counters));
    CloseHandle(process);
    return ok ? static_cast<quint64>(counters.PrivateUsage) : 0;
#else
    Q_UNUSED(pid)
    return 0;
#endif
}

} // namespace monitor
