#include "ui/MonitorController.h"

#include "monitor/MonitorWorker.h"

#include <QThread>

namespace ui {

using monitor::MonitorWorker;

MonitorController::MonitorController(QObject* parent)
    : QObject(parent)
{
    initialise(monitor::NvmlLibrary::defaultCandidates());
}

MonitorController::MonitorController(QStringList nvmlCandidates, QObject* parent)
    : QObject(parent)
{
    initialise(std::move(nvmlCandidates));
}

void MonitorController::initialise(QStringList nvmlCandidates)
{
    m_thread = new QThread(this);
    m_thread->setObjectName(QStringLiteral("monitor"));

    // Sans parent : l'objet appartient au fil du worker et se détruit avec lui.
    m_worker = new MonitorWorker(std::move(nvmlCandidates));
    m_worker->moveToThread(m_thread);
    connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);

    connect(m_worker, &MonitorWorker::sampled, this, &MonitorController::onSampled);
    connect(m_worker, &MonitorWorker::gpuStatus, this, &MonitorController::onGpuStatus);

    connect(this, &MonitorController::requestStart, m_worker, &MonitorWorker::start);
    connect(this, &MonitorController::requestStop, m_worker, &MonitorWorker::stop);
    connect(this, &MonitorController::requestInterval, m_worker, &MonitorWorker::setInterval);
    connect(this, &MonitorController::requestTrackedPid, m_worker, &MonitorWorker::setTrackedPid);

    m_thread->start();
}

MonitorController::~MonitorController()
{
    m_thread->quit();
    // NVML est libéré dans le destructeur du worker, donc dans ce fil : l'attendre
    // évite de laisser une session NVML ouverte derrière nous.
    m_thread->wait();
}

void MonitorController::setActive(bool active)
{
    if (m_active == active)
        return;
    m_active = active;
    if (active)
        emit requestStart(m_intervalMs);
    else
        emit requestStop();
    emit activeChanged();
}

void MonitorController::setIntervalMs(int intervalMs)
{
    if (m_intervalMs == intervalMs)
        return;
    m_intervalMs = intervalMs;
    emit requestInterval(intervalMs);
    emit intervalChanged();
}

void MonitorController::setTrackedPid(qint64 pid)
{
    if (m_trackedPid == pid)
        return;
    m_trackedPid = pid;
    emit requestTrackedPid(pid);
    emit trackedPidChanged();
}

void MonitorController::onSampled(const monitor::MonitorSample& sample)
{
    m_sample = sample;
    m_hasSample = true;
    emit sampleChanged();
}

void MonitorController::onGpuStatus(bool available, const QString& name, const QString& error)
{
    m_gpuAvailable = available;
    m_gpuName = name;
    m_gpuError = error;
    emit gpuStatusChanged();
}

double MonitorController::ramRatio() const
{
    return monitor::ratioOf(m_sample.ramUsed, m_sample.ramTotal);
}

double MonitorController::ramProcessRatio() const
{
    return monitor::ratioOf(m_sample.ramProcess, m_sample.ramTotal);
}

int MonitorController::ramLevel() const
{
    return static_cast<int>(monitor::loadFor(ramRatio()));
}

QString MonitorController::ramText() const
{
    return monitor::formatPair(m_sample.ramUsed, m_sample.ramTotal);
}

QString MonitorController::ramProcessText() const
{
    if (m_sample.ramProcess == 0)
        return {};
    return QStringLiteral("llama : %1 Go").arg(monitor::formatGigabytes(m_sample.ramProcess));
}

double MonitorController::vramRatio() const
{
    return monitor::ratioOf(m_sample.vramUsed, m_sample.vramTotal);
}

double MonitorController::vramProcessRatio() const
{
    return monitor::ratioOf(m_sample.vramProcess, m_sample.vramTotal);
}

int MonitorController::vramLevel() const
{
    return static_cast<int>(monitor::loadFor(vramRatio()));
}

QString MonitorController::vramText() const
{
    return monitor::formatPair(m_sample.vramUsed, m_sample.vramTotal);
}

QString MonitorController::vramProcessText() const
{
    if (m_sample.vramProcess > 0)
        return QStringLiteral("llama : %1 Go").arg(monitor::formatGigabytes(m_sample.vramProcess));
    // Processus reconnu sur le GPU mais consommation non chiffrable (WDDM) :
    // le dire vaut mieux qu'afficher « llama : 0,0 Go », qui serait faux.
    return m_sample.trackedOnGpu ? QStringLiteral("llama sur le GPU") : QString();
}

} // namespace ui
