#include "ui/Fonts.h"

#include <QFont>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);
    // Ne pas définir organizationName : voir le contrat dans AppPaths.h.
    QGuiApplication::setApplicationName(QStringLiteral("LlamaBuilder"));
    QGuiApplication::setApplicationVersion(QStringLiteral("0.1.0"));
    // Pas de setApplicationDisplayName : Qt le concatènerait au titre de fenêtre.


    ui::Fonts::install();
    QFont baseFont(ui::Fonts::uiFamily());
    baseFont.setPixelSize(13);
    QGuiApplication::setFont(baseFont);

    // Le style Basic est le seul qui n'impose pas ses propres couleurs : Fusion
    // et Windows devraient être combattus à chaque contrôle.
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    // Les types C++ (App, ParamFilterModel, les deux modèles) sont enregistrés
    // par qmltyperegistrar depuis les macros QML_* : rien à déclarer ici.
    QQmlApplicationEngine engine;
    engine.loadFromModule("LlamaBuilder", "Main");
    if (engine.rootObjects().isEmpty())
        return 1;

    return QGuiApplication::exec();
}
