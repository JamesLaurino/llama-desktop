#include "ui/Fonts.h"

#include <QFontDatabase>
#include <QStringList>

namespace ui::Fonts {
namespace {

QString g_uiFamily;
QString g_monoFamily;
bool g_installed = false;

QString addFont(const QString& path)
{
    const int id = QFontDatabase::addApplicationFont(path);
    if (id < 0)
        return {};
    return QFontDatabase::applicationFontFamilies(id).value(0);
}

} // namespace

void install()
{
    if (g_installed)
        return;
    g_installed = true;

    const QString inter = addFont(QStringLiteral(":/resources/fonts/Inter-Regular.ttf"));
    addFont(QStringLiteral(":/resources/fonts/Inter-Medium.ttf"));
    addFont(QStringLiteral(":/resources/fonts/Inter-SemiBold.ttf"));
    const QString mono = addFont(QStringLiteral(":/resources/fonts/JetBrainsMono-Regular.ttf"));

    // Replis : les deux sont livrées avec Windows 11, l'application reste
    // lisible même si la ressource embarquée a été retirée du binaire.
    g_uiFamily = inter.isEmpty() ? QStringLiteral("Segoe UI") : inter;
    g_monoFamily = mono.isEmpty() ? QStringLiteral("Cascadia Mono") : mono;
}

QString uiFamily()
{
    install();
    return g_uiFamily;
}

QString monoFamily()
{
    install();
    return g_monoFamily;
}

} // namespace ui::Fonts
