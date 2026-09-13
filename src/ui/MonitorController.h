#pragma once

#include "monitor/MonitorSample.h"

#include <QObject>
#include <QQmlEngine>
#include <QStringList>

class QThread;

namespace monitor {
class MonitorWorker;
}

namespace ui {

/// Façade QML du monitoring : possède le QThread et traduit les relevés en
/// propriétés prêtes à afficher.
///
/// Exposé par `App.monitor`. Les valeurs brutes ne sortent pas : le QML reçoit
/// des ratios pour les barres et des libellés déjà formatés, parce que le
/// diviseur 1024³ et les seuils du §5.7 doivent être vérifiables par un test C++.
class MonitorController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Exposé par App.monitor.")

    /// Le sondage s'arrête quand la fenêtre est minimisée (§9).
    Q_PROPERTY(bool active READ isActive WRITE setActive NOTIFY activeChanged)
    Q_PROPERTY(int intervalMs READ intervalMs WRITE setIntervalMs NOTIFY intervalChanged)
    /// PID du processus llama.cpp à isoler. Posé par la phase 4.
    Q_PROPERTY(qint64 trackedPid READ trackedPid WRITE setTrackedPid NOTIFY trackedPidChanged)

    Q_PROPERTY(bool hasSample READ hasSample NOTIFY sampleChanged)

    Q_PROPERTY(double ramRatio READ ramRatio NOTIFY sampleChanged)
    Q_PROPERTY(double ramProcessRatio READ ramProcessRatio NOTIFY sampleChanged)
    Q_PROPERTY(int ramLevel READ ramLevel NOTIFY sampleChanged)
    Q_PROPERTY(QString ramText READ ramText NOTIFY sampleChanged)
    Q_PROPERTY(QString ramProcessText READ ramProcessText NOTIFY sampleChanged)

    Q_PROPERTY(bool gpuAvailable READ gpuAvailable NOTIFY gpuStatusChanged)
    Q_PROPERTY(QString gpuName READ gpuName NOTIFY gpuStatusChanged)
    Q_PROPERTY(QString gpuError READ gpuError NOTIFY gpuStatusChanged)
    Q_PROPERTY(int gpuUtilisation READ gpuUtilisation NOTIFY sampleChanged)
    Q_PROPERTY(bool trackedOnGpu READ trackedOnGpu NOTIFY sampleChanged)

    Q_PROPERTY(double vramRatio READ vramRatio NOTIFY sampleChanged)
    Q_PROPERTY(double vramProcessRatio READ vramProcessRatio NOTIFY sampleChanged)
    Q_PROPERTY(int vramLevel READ vramLevel NOTIFY sampleChanged)
    Q_PROPERTY(QString vramText READ vramText NOTIFY sampleChanged)
    Q_PROPERTY(QString vramProcessText READ vramProcessText NOTIFY sampleChanged)

public:
    explicit MonitorController(QObject* parent = nullptr);
    /// Bibliothèques NVML à essayer : les tests passent un nom inexistant pour
    /// exercer le chemin « aucun pilote NVIDIA ».
    MonitorController(QStringList nvmlCandidates, QObject* parent);
    ~MonitorController() override;

    bool isActive() const { return m_active; }
    void setActive(bool active);
    int intervalMs() const { return m_intervalMs; }
    void setIntervalMs(int intervalMs);
    qint64 trackedPid() const { return m_trackedPid; }
    void setTrackedPid(qint64 pid);

    bool hasSample() const { return m_hasSample; }

    double ramRatio() const;
    double ramProcessRatio() const;
    int ramLevel() const;
    QString ramText() const;
    QString ramProcessText() const;

    bool gpuAvailable() const { return m_gpuAvailable; }
    QString gpuName() const { return m_gpuName; }
    QString gpuError() const { return m_gpuError; }
    int gpuUtilisation() const { return m_sample.gpuUtilisation; }
    bool trackedOnGpu() const { return m_sample.trackedOnGpu; }

    double vramRatio() const;
    double vramProcessRatio() const;
    int vramLevel() const;
    QString vramText() const;
    QString vramProcessText() const;

    /// Dernier relevé, pour les tests.
    const monitor::MonitorSample& sample() const { return m_sample; }

signals:
    void activeChanged();
    void intervalChanged();
    void trackedPidChanged();
    void sampleChanged();
    void gpuStatusChanged();

    /// Commandes adressées au worker. Des signaux plutôt que `invokeMethod` avec
    /// un nom de méthode en chaîne : la connexion est vérifiée à la compilation,
    /// et la traversée de fil reste automatique.
    void requestStart(int intervalMs);
    void requestStop();
    void requestInterval(int intervalMs);
    void requestTrackedPid(qint64 pid);

private:
    void initialise(QStringList nvmlCandidates);
    void onSampled(const monitor::MonitorSample& sample);
    void onGpuStatus(bool available, const QString& name, const QString& error);

    QThread* m_thread = nullptr;
    monitor::MonitorWorker* m_worker = nullptr;

    bool m_active = false;
    int m_intervalMs = 1000;
    qint64 m_trackedPid = 0;

    monitor::MonitorSample m_sample;
    bool m_hasSample = false;
    bool m_gpuAvailable = false;
    QString m_gpuName;
    QString m_gpuError;
};

} // namespace ui
