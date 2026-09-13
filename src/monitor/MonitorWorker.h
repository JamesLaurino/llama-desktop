#pragma once

#include "monitor/MonitorSample.h"
#include "monitor/NvmlMonitor.h"

#include <QObject>
#include <QStringList>
#include <QTimer>

namespace monitor {

/// Boucle d'échantillonnage, destinée à vivre dans un QThread dédié (§9).
///
/// Le fil n'est pas là pour le coût moyen d'un tick — quelques centaines de
/// microsecondes — mais pour ses pires cas : `nvmlInit_v2` prend 100 à 300 ms, et
/// les appels NVML se sérialisent avec le pilote, donc bloquent au moment même
/// où l'on veut voir la jauge se remplir : le chargement d'un modèle de 20 Go.
///
/// Toutes les entrées publiques sont des slots : elles sont appelées depuis le fil
/// principal et exécutées dans celui du worker.
class MonitorWorker : public QObject
{
    Q_OBJECT

public:
    explicit MonitorWorker(QStringList nvmlCandidates = NvmlLibrary::defaultCandidates(),
                           QObject* parent = nullptr);

public slots:
    /// Initialise NVML au premier appel, puis démarre le minuteur. Émet un
    /// premier relevé sans attendre l'intervalle.
    void start(int intervalMs);
    /// Arrête le minuteur sans libérer NVML : la reprise doit être immédiate
    /// quand la fenêtre est restaurée.
    void stop();
    void setInterval(int intervalMs);
    void setTrackedPid(qint64 pid);

signals:
    void sampled(const monitor::MonitorSample& sample);
    /// Émis une seule fois, après l'initialisation, pour que l'interface puisse
    /// afficher « GPU NVIDIA non détecté » sans attendre un relevé.
    void gpuStatus(bool available, const QString& name, const QString& error);

private:
    void tick();

    NvmlMonitor m_nvml;
    /// Pointeur enfant, pas un membre par valeur : `moveToThread` n'emporte que
    /// les enfants. Un QTimer sans parent resterait dans le fil principal et ne
    /// tirerait jamais, l'interface n'affichant qu'un unique relevé.
    QTimer* m_timer = nullptr;
    bool m_nvmlReady = false;
    quint32 m_trackedPid = 0;
};

} // namespace monitor
