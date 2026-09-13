#include "monitor/MonitorSample.h"
#include "monitor/NvmlLibrary.h"
#include "monitor/NvmlMonitor.h"
#include "monitor/SystemMonitor.h"
#include "ui/MonitorController.h"

#include <QCoreApplication>
#include <QSignalSpy>
#include <QTest>

#include <limits>

using namespace monitor;

namespace {

/// Nom de bibliothèque garanti introuvable : exerce le chemin « aucun pilote
/// NVIDIA » sur une machine qui en possède un.
QStringList absentLibrary()
{
    return { QStringLiteral("nvml-absente-volontairement") };
}

} // namespace

class TestMonitor : public QObject
{
    Q_OBJECT

private slots:
    // --- Formatage et seuils ---------------------------------------------
    void loadThresholdsAtBoundaries_data();
    void loadThresholdsAtBoundaries();
    void ratioIsBoundedAndSafeOnZeroTotal();
    void gigabytesUseBinaryDivisor();
    void pairIsFormattedWithUnit();

    // --- RAM système -----------------------------------------------------
    void systemRamIsPlausible();
    void privateUsageOfOwnProcessIsNonZero();
    void privateUsageOfAbsentProcessIsZero();

    // --- NVML ------------------------------------------------------------
    void sentinelMemoryValueIsRejected();
    void missingLibraryDegradesCleanly();
    void realGpuReportsConsistentMemory();

    // --- Contrôleur et fil dédié -----------------------------------------
    void controllerSamplesFromAnotherThread();
    void controllerSamplesPeriodically();
    void controllerStopsWhenInactive();
    void controllerWithoutGpuStillReportsRam();
};

void TestMonitor::loadThresholdsAtBoundaries_data()
{
    QTest::addColumn<double>("ratio");
    QTest::addColumn<int>("expected");

    QTest::newRow("0") << 0.0 << int(Load::Ok);
    QTest::newRow("juste sous 75 %") << 0.7499 << int(Load::Ok);
    QTest::newRow("75 % pile") << 0.75 << int(Load::Tension);
    QTest::newRow("juste sous 90 %") << 0.8999 << int(Load::Tension);
    QTest::newRow("90 % pile") << 0.90 << int(Load::Tension);
    QTest::newRow("juste au-dessus de 90 %") << 0.9001 << int(Load::Saturation);
    QTest::newRow("100 %") << 1.0 << int(Load::Saturation);
}

void TestMonitor::loadThresholdsAtBoundaries()
{
    QFETCH(double, ratio);
    QFETCH(int, expected);
    QCOMPARE(int(loadFor(ratio)), expected);
}

void TestMonitor::ratioIsBoundedAndSafeOnZeroTotal()
{
    // Total nul : NVML n'a pas encore répondu, aucune division ne doit avoir lieu.
    QCOMPARE(ratioOf(1024, 0), 0.0);
    QCOMPARE(ratioOf(0, 1024), 0.0);
    QCOMPARE(ratioOf(512, 1024), 0.5);
    // Un « utilisé » supérieur au total ne doit pas faire déborder la barre.
    QCOMPARE(ratioOf(2048, 1024), 1.0);
}

void TestMonitor::gigabytesUseBinaryDivisor()
{
    const QLocale c = QLocale::c();
    QCOMPARE(formatGigabytes(quint64(1024) * 1024 * 1024, c), QStringLiteral("1.0"));
    // 24 463 Mio, ce que rapporte nvidia-smi pour une RTX 5090 Laptop : le
    // diviseur 10⁹ afficherait 25.6 et le critère d'acceptation n°4 tomberait.
    QCOMPARE(formatGigabytes(quint64(24463) * 1024 * 1024, c), QStringLiteral("23.9"));
    QCOMPARE(formatGigabytes(0, c), QStringLiteral("0.0"));
}

void TestMonitor::pairIsFormattedWithUnit()
{
    const QLocale c = QLocale::c();
    const quint64 gib = quint64(1024) * 1024 * 1024;
    QCOMPARE(formatPair(15 * gib, 24 * gib, c), QStringLiteral("15.0 / 24.0 Go"));
}

void TestMonitor::systemRamIsPlausible()
{
    MonitorSample sample;
    SystemMonitor::sample(sample, 0);

    QVERIFY(sample.ramTotal > quint64(1024) * 1024 * 1024);
    QVERIFY(sample.ramUsed > 0);
    QVERIFY(sample.ramUsed < sample.ramTotal);
    QCOMPARE(sample.ramProcess, quint64(0));
}

void TestMonitor::privateUsageOfOwnProcessIsNonZero()
{
    const auto pid = static_cast<quint32>(QCoreApplication::applicationPid());
    QVERIFY(SystemMonitor::privateUsage(pid) > 0);
}

void TestMonitor::privateUsageOfAbsentProcessIsZero()
{
    // Le PID 0 est le processus « System Idle » : OpenProcess le refuse toujours.
    QCOMPARE(SystemMonitor::privateUsage(0), quint64(0));
}

void TestMonitor::sentinelMemoryValueIsRejected()
{
    const quint64 total = quint64(24463) * 1024 * 1024;
    const auto sentinel = std::numeric_limits<unsigned long long>::max();

    // NVML_VALUE_NOT_AVAILABLE : sans ce filtre, la valeur s'affichait comme
    // 17 592 186 044 415 Mio. C'est la réponse normale de NVML sous WDDM.
    QVERIFY(!NvmlLibrary::isReportedMemory(sentinel, total));
    // Une valeur supérieure à la capacité de la carte n'a pas plus de sens.
    QVERIFY(!NvmlLibrary::isReportedMemory(total + 1, total));
    QVERIFY(NvmlLibrary::isReportedMemory(total, total));
    QVERIFY(NvmlLibrary::isReportedMemory(0, total));
    // Total inconnu : on ne peut rien borner, seule la sentinelle est rejetée.
    QVERIFY(NvmlLibrary::isReportedMemory(1024, 0));
    QVERIFY(!NvmlLibrary::isReportedMemory(sentinel, 0));
}

void TestMonitor::missingLibraryDegradesCleanly()
{
    NvmlMonitor nvml(absentLibrary());
    QVERIFY(!nvml.initialise());
    QVERIFY(!nvml.isAvailable());
    QVERIFY(nvml.error().contains(QStringLiteral("GPU NVIDIA non détecté")));

    // Échantillonner malgré tout ne doit ni planter ni inventer de valeurs.
    MonitorSample sample;
    nvml.sample(sample, 4242);
    QVERIFY(!sample.gpuAvailable);
    QCOMPARE(sample.vramTotal, quint64(0));
    QCOMPARE(sample.vramProcess, quint64(0));
    QVERIFY(!sample.gpuError.isEmpty());

    // Idempotence : un second initialise() ne relance rien.
    QVERIFY(!nvml.initialise());
}

void TestMonitor::realGpuReportsConsistentMemory()
{
    NvmlMonitor nvml;
    if (!nvml.initialise())
        QSKIP("Aucun GPU NVIDIA sur cette machine.");

    MonitorSample sample;
    nvml.sample(sample, 0);

    QVERIFY(sample.gpuAvailable);
    QVERIFY(!sample.gpuName.isEmpty());
    QVERIFY(sample.vramTotal > quint64(256) * 1024 * 1024);
    QVERIFY(sample.vramUsed <= sample.vramTotal);
    QVERIFY(sample.gpuUtilisation >= -1 && sample.gpuUtilisation <= 100);

    // Un PID qui n'existe pas ne doit jamais se voir attribuer de VRAM.
    MonitorSample other;
    nvml.sample(other, 0xFFFFFFFEu);
    QCOMPARE(other.vramProcess, quint64(0));
}

void TestMonitor::controllerSamplesFromAnotherThread()
{
    ui::MonitorController controller;
    QSignalSpy spy(&controller, &ui::MonitorController::sampleChanged);

    controller.setIntervalMs(200);
    controller.setActive(true);
    QVERIFY(spy.wait(5000));

    // Le relevé traverse une frontière de fil : le §9 exige que NVML et Win32
    // soient interrogés ailleurs que dans le fil de l'interface.
    QVERIFY(controller.hasSample());
    QVERIFY(controller.sample().ramTotal > 0);
    QVERIFY(!controller.ramText().isEmpty());
}

void TestMonitor::controllerSamplesPeriodically()
{
    // Ce cas existe parce que le premier jet ne le passait pas : `start()` émet
    // un relevé immédiat, donc un minuteur qui ne tire jamais reste invisible si
    // l'on se contente d'attendre un seul signal. Le QTimer doit être un enfant
    // du worker, sinon `moveToThread` ne l'emporte pas et il ne tire plus.
    ui::MonitorController controller;
    controller.setIntervalMs(200);

    QSignalSpy spy(&controller, &ui::MonitorController::sampleChanged);
    controller.setActive(true);
    QTest::qWait(1200);

    QVERIFY2(spy.count() >= 4,
             qPrintable(QStringLiteral("relevés reçus en 1,2 s à 200 ms : %1").arg(spy.count())));
}

void TestMonitor::controllerStopsWhenInactive()
{
    ui::MonitorController controller;
    controller.setIntervalMs(200);
    controller.setActive(true);

    QSignalSpy spy(&controller, &ui::MonitorController::sampleChanged);
    // D'abord prouver que les relevés arrivent, sinon l'assertion d'arrêt
    // ci-dessous serait vraie pour rien.
    QTest::qWait(700);
    QVERIFY(spy.count() >= 2);

    controller.setActive(false);
    // Laisser le temps à un tick d'arriver s'il en restait un en vol, puis
    // vérifier qu'il n'en arrive plus aucun.
    QTest::qWait(300);
    spy.clear();
    QTest::qWait(800);
    QCOMPARE(spy.count(), 0);
}

void TestMonitor::controllerWithoutGpuStillReportsRam()
{
    ui::MonitorController controller(absentLibrary(), nullptr);
    QSignalSpy gpu(&controller, &ui::MonitorController::gpuStatusChanged);
    QSignalSpy samples(&controller, &ui::MonitorController::sampleChanged);

    controller.setIntervalMs(200);
    controller.setActive(true);
    QVERIFY(gpu.wait(5000));
    QVERIFY(!controller.gpuAvailable());
    QVERIFY(!controller.gpuError().isEmpty());

    QVERIFY(samples.count() > 0 || samples.wait(5000));
    // Critère d'acceptation n°6 : la jauge RAM reste alimentée sans NVIDIA.
    QVERIFY(controller.ramRatio() > 0.0);
    QVERIFY(controller.vramText().contains(QStringLiteral("0.0"))
            || controller.vramText().contains(QStringLiteral("0,0")));
}

QTEST_GUILESS_MAIN(TestMonitor)
#include "test_monitor.moc"
