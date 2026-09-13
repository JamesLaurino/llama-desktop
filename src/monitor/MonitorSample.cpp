#include "monitor/MonitorSample.h"

namespace monitor {

namespace {
constexpr double kGigabyte = 1024.0 * 1024.0 * 1024.0;
}

Load loadFor(double ratio)
{
    if (ratio > 0.90)
        return Load::Saturation;
    if (ratio >= 0.75)
        return Load::Tension;
    return Load::Ok;
}

double ratioOf(quint64 used, quint64 total)
{
    if (total == 0)
        return 0.0;
    const double value = static_cast<double>(used) / static_cast<double>(total);
    return qBound(0.0, value, 1.0);
}

QString formatGigabytes(quint64 bytes, const QLocale& locale)
{
    return locale.toString(static_cast<double>(bytes) / kGigabyte, 'f', 1);
}

QString formatPair(quint64 used, quint64 total, const QLocale& locale)
{
    return QStringLiteral("%1 / %2 Go").arg(formatGigabytes(used, locale),
                                            formatGigabytes(total, locale));
}

} // namespace monitor
