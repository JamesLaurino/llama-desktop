#pragma once

#include <QLocale>
#include <QString>

namespace monitor {

/// Un relevé instantané de RAM et de VRAM.
///
/// Type purement données : copiable, sans QObject. C'est ce qui permet de le
/// faire franchir la frontière de fil par un signal en file d'attente.
struct MonitorSample {
    quint64 ramTotal = 0;
    quint64 ramUsed = 0;
    quint64 ramProcess = 0; ///< `PrivateUsage` du processus suivi, 0 si aucun

    bool gpuAvailable = false;
    QString gpuName;
    QString gpuError; ///< renseigné uniquement quand `gpuAvailable` est faux
    quint64 vramTotal = 0;
    quint64 vramUsed = 0;
    quint64 vramProcess = 0;
    /// Le processus suivi figure dans la liste des clients de calcul du GPU.
    ///
    /// Distinct de `vramProcess != 0` : sous Windows en mode WDDM — donc sur
    /// toute carte GeForce pilotant un écran — NVML répond
    /// `NVML_VALUE_NOT_AVAILABLE` à la question « combien de VRAM ce processus
    /// occupe-t-il ». `nvidia-smi` affiche `[N/A]` au même endroit. On sait donc
    /// que llama.cpp est sur le GPU sans pouvoir chiffrer sa part.
    bool trackedOnGpu = false;
    int gpuUtilisation = -1; ///< pourcentage, -1 si la carte ne le rapporte pas
};

/// Niveaux de charge du §5.7 : vert sous 75 %, ambre jusqu'à 90 %, rouge au-delà.
enum class Load { Ok, Tension, Saturation };

Load loadFor(double ratio);

/// 0 si `total` est nul, sinon `used / total` borné à [0, 1].
double ratioOf(quint64 used, quint64 total);

/// Diviseur 1024³, une décimale. « 15,2 »
///
/// Ce diviseur n'est pas un détail : `nvidia-smi` et les journaux de llama.cpp
/// comptent en Mio. Diviser par 10⁹ afficherait 25,6 « Go » là où nvidia-smi
/// rapporte 24 463 Mio, et le critère d'acceptation n°4 (moins de 200 Mo d'écart)
/// deviendrait invérifiable.
QString formatGigabytes(quint64 bytes, const QLocale& locale = QLocale());

/// « 15,2 / 24,0 Go »
QString formatPair(quint64 used, quint64 total, const QLocale& locale = QLocale());

} // namespace monitor
