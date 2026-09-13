#include "monitor/MonitorWorker.h"

#include "monitor/SystemMonitor.h"

namespace monitor {

namespace {
constexpr int kMinInterval = 200;
constexpr int kMaxInterval = 10000;
}

MonitorWorker::MonitorWorker(QStringList nvmlCandidates, QObject* parent)
    : QObject(parent)
    , m_nvml(std::move(nvmlCandidates))
{
    m_timer = new QTimer(this);
    m_timer->setTimerType(Qt::CoarseTimer);
    connect(m_timer, &QTimer::timeout, this, &MonitorWorker::tick);
}

void MonitorWorker::start(int intervalMs)
{
    if (!m_nvmlReady) {
        m_nvmlReady = true;
        m_nvml.initialise();
        emit gpuStatus(m_nvml.isAvailable(), m_nvml.gpuName(), m_nvml.error());
    }

    m_timer->setInterval(qBound(kMinInterval, intervalMs, kMaxInterval));
    m_timer->start();
    // Un relevé immédiat : sans lui les jauges resteraient vides pendant une
    // seconde au démarrage comme à chaque restauration de la fenêtre.
    tick();
}

void MonitorWorker::stop()
{
    m_timer->stop();
}

void MonitorWorker::setInterval(int intervalMs)
{
    const int clamped = qBound(kMinInterval, intervalMs, kMaxInterval);
    if (m_timer->interval() == clamped)
        return;
    m_timer->setInterval(clamped);
}

void MonitorWorker::setTrackedPid(qint64 pid)
{
    m_trackedPid = pid > 0 ? static_cast<quint32>(pid) : 0;
}

void MonitorWorker::tick()
{
    MonitorSample sample;
    SystemMonitor::sample(sample, m_trackedPid);
    m_nvml.sample(sample, m_trackedPid);
    emit sampled(sample);
}

} // namespace monitor
