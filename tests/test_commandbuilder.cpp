#include "core/CommandBuilder.h"
#include "core/ParamRegistry.h"
#include "core/Profile.h"
#include "core/Settings.h"

#include <QTest>

using namespace core;

namespace {

/// Registre de test volontairement indépendant de resources/params.json : les
/// règles de génération se testent sur un jeu figé, pas sur un fichier qui bouge.
/// L'ordre de déclaration est délibérément l'inverse de l'ordre alphabétique
/// des clés, pour prouver que la commande suit params.json et non la QMap.
const char* kRegistryJson = R"json({
  "sections": [
    { "id": "gpu",     "label": "GPU" },
    { "id": "context", "label": "Contexte" },
    { "id": "server",  "label": "Serveur" }
  ],
  "params": [
    { "key": "zebra",        "flag": "-z",    "section": "gpu",     "type": "int" },
    { "key": "n-gpu-layers", "flag": "-ngl",  "section": "gpu",     "type": "intOrKeyword",
      "keywords": ["auto", "all"], "default": "auto" },
    { "key": "mmproj",       "flag": "-mm",   "section": "gpu",     "type": "path" },
    { "key": "alpha",        "flag": "-a1",   "section": "context", "type": "string" },
    { "key": "ctx-size",     "flag": "-c",    "section": "context", "type": "int", "default": "0" },
    { "key": "flash-attn",   "flag": "-fa",   "section": "context", "type": "enum",
      "values": ["on", "off", "auto"], "default": "auto" },
    { "key": "verbose",      "flag": "-v",    "section": "context", "type": "bool" },
    { "key": "jinja",        "flag": "--jinja", "flagOff": "--no-jinja", "section": "context",
      "type": "tristate", "default": "on" },
    { "key": "port",         "flag": "--port", "section": "server", "type": "int",
      "appliesTo": ["server"] },
    { "key": "chat-file",    "flag": "-cf",   "section": "server",  "type": "path",
      "appliesTo": ["cli"] }
  ]
})json";

ParamRegistry testRegistry()
{
    QString error;
    const ParamRegistry registry = ParamRegistry::fromJson(QByteArray(kRegistryJson), &error);
    if (registry.isEmpty())
        qFatal("Registre de test invalide : %s", qUtf8Printable(error));
    return registry;
}

Settings testSettings()
{
    Settings settings;
    settings.llamaServerPath = QStringLiteral("C:/llama.cpp/bin/llama-server.exe");
    settings.llamaCliPath = QStringLiteral("C:/llama.cpp/bin/llama-cli.exe");
    return settings;
}

Profile baseProfile()
{
    Profile profile = Profile::createNew();
    profile.name = QStringLiteral("Test");
    profile.binary = BinaryKind::Server;
    return profile;
}

} // namespace

class TestCommandBuilder : public QObject
{
    Q_OBJECT

private slots:
    // --- Citation Windows -------------------------------------------------
    void quoting_data();
    void quoting();

    // --- Découpage d'une ligne libre -------------------------------------
    void splitting_data();
    void splitting();

    /// Propriété centrale : citer puis redécouper doit rendre l'argument intact.
    void quoteThenSplitIsIdentity_data();
    void quoteThenSplitIsIdentity();

    // --- Génération ------------------------------------------------------
    void emptyProfileEmitsOnlyExecutableAndModel();
    void profileWithoutModelEmitsNoModelFlag();
    void argumentOrderFollowsRegistryNotMap();
    void booleanFalseEmitsNothing();
    void booleanTrueEmitsBareFlag();
    void tristateEmitsOnOffOrNothing_data();
    void tristateEmitsOnOffOrNothing();
    void valueEqualToDefaultIsStillEmitted();
    void emptyValueIsSkipped();
    void appliesToFiltersPerBinary();
    void pathWithSpacesIsQuotedOnceAndRawInArguments();
    void onlyPathsAreNormalizedInDisplayLine();
    void extraArgsRespectQuotes();
    void missingExecutableFallsBackToConventionalName();
    void cliBinaryUsesCliExecutable();

    // --- Le vrai fichier de données --------------------------------------
    void shippedParamsJsonIsValid();
};

void TestCommandBuilder::quoting_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("expected");

    QTest::newRow("simple") << "-ngl" << "-ngl";
    QTest::newRow("chemin sans espace") << "C:\\models\\a.gguf" << "C:\\models\\a.gguf";
    QTest::newRow("vide") << "" << "\"\"";
    QTest::newRow("espace") << "D:\\mes modeles\\a.gguf" << "\"D:\\mes modeles\\a.gguf\"";
    QTest::newRow("tabulation") << "a\tb" << "\"a\tb\"";
    QTest::newRow("esperluette") << "a&b" << "\"a&b\"";
    QTest::newRow("tube") << "a|b" << "\"a|b\"";
    QTest::newRow("chevrons") << "a<b>c" << "\"a<b>c\"";
    QTest::newRow("accent circonflexe") << "a^b" << "\"a^b\"";
    QTest::newRow("parentheses") << "a(b)" << "\"a(b)\"";
    QTest::newRow("guillemet interne") << "dit \"oui\"" << "\"dit \\\"oui\\\"\"";
    // Antislash terminal : doit être doublé, sinon il échappe le guillemet fermant.
    QTest::newRow("antislash terminal") << "C:\\mes modeles\\" << "\"C:\\mes modeles\\\\\"";
    QTest::newRow("antislashs avant guillemet") << "a\\\\\"b" << "\"a\\\\\\\\\\\"b\"";
}

void TestCommandBuilder::quoting()
{
    QFETCH(QString, input);
    QFETCH(QString, expected);
    QCOMPARE(CommandBuilder::quoteArgument(input), expected);
}

void TestCommandBuilder::splitting_data()
{
    QTest::addColumn<QString>("line");
    QTest::addColumn<QStringList>("expected");

    QTest::newRow("vide") << "" << QStringList{};
    QTest::newRow("espaces seuls") << "   \t " << QStringList{};
    QTest::newRow("deux flags") << "--no-warmup -v" << QStringList{ "--no-warmup", "-v" };
    QTest::newRow("espaces multiples") << "  -a   -b  " << QStringList{ "-a", "-b" };
    QTest::newRow("valeur citee")
        << "--alias \"Qwen 27B\"" << QStringList{ "--alias", "Qwen 27B" };
    QTest::newRow("chemin windows cite")
        << "-m \"D:\\mes modeles\\a.gguf\"" << QStringList{ "-m", "D:\\mes modeles\\a.gguf" };
    QTest::newRow("chemin windows nu")
        << "-m D:\\models\\a.gguf" << QStringList{ "-m", "D:\\models\\a.gguf" };
    QTest::newRow("guillemet echappe")
        << "--tpl \"dit \\\"oui\\\"\"" << QStringList{ "--tpl", "dit \"oui\"" };
    QTest::newRow("argument vide cite") << "-p \"\"" << QStringList{ "-p", "" };
    QTest::newRow("citation partielle") << "-m a\"b c\"d" << QStringList{ "-m", "ab cd" };
}

void TestCommandBuilder::splitting()
{
    QFETCH(QString, line);
    QFETCH(QStringList, expected);
    QCOMPARE(CommandBuilder::splitArgumentLine(line), expected);
}

void TestCommandBuilder::quoteThenSplitIsIdentity_data()
{
    QTest::addColumn<QString>("argument");

    QTest::newRow("simple") << "-ngl";
    QTest::newRow("espace") << "D:\\mes modeles\\qwen 27b.gguf";
    QTest::newRow("antislash terminal") << "D:\\mes modeles\\";
    QTest::newRow("guillemets") << "il a dit \"oui\" puis \"non\"";
    QTest::newRow("antislash avant guillemet") << "a\\\"b";
    QTest::newRow("metacaracteres cmd") << "a&b|c<d>e^f(g)";
    QTest::newRow("tabulation") << "avant\tapres";
    QTest::newRow("vide") << "";
    QTest::newRow("accents") << "D:\\modèles\\qwen 27b — long.gguf";
}

void TestCommandBuilder::quoteThenSplitIsIdentity()
{
    QFETCH(QString, argument);
    const QString quoted = CommandBuilder::quoteArgument(argument);
    QCOMPARE(CommandBuilder::splitArgumentLine(quoted), QStringList{ argument });
}

void TestCommandBuilder::emptyProfileEmitsOnlyExecutableAndModel()
{
    Profile profile = baseProfile();
    profile.modelPath = QStringLiteral("C:/models/a.gguf");

    const BuiltCommand command = CommandBuilder::build(profile, testSettings(), testRegistry());

    QCOMPARE(command.program, QStringLiteral("C:/llama.cpp/bin/llama-server.exe"));
    QCOMPARE(command.arguments, QStringList({ "-m", "C:/models/a.gguf" }));
    QCOMPARE(command.displayLine,
             QStringLiteral("C:\\llama.cpp\\bin\\llama-server.exe -m C:\\models\\a.gguf"));
}

void TestCommandBuilder::profileWithoutModelEmitsNoModelFlag()
{
    const BuiltCommand command = CommandBuilder::build(baseProfile(), testSettings(), testRegistry());
    QVERIFY(command.arguments.isEmpty());
    QCOMPARE(command.displayLine, QStringLiteral("C:\\llama.cpp\\bin\\llama-server.exe"));
}

void TestCommandBuilder::argumentOrderFollowsRegistryNotMap()
{
    Profile profile = baseProfile();
    // Ordre alphabétique dans la QMap : alpha, ctx-size, n-gpu-layers, zebra.
    // Ordre de params.json : zebra, n-gpu-layers, alpha, ctx-size.
    profile.params = {
        { QStringLiteral("alpha"), QStringLiteral("A") },
        { QStringLiteral("ctx-size"), QStringLiteral("16384") },
        { QStringLiteral("n-gpu-layers"), QStringLiteral("all") },
        { QStringLiteral("zebra"), QStringLiteral("1") },
    };

    const BuiltCommand command = CommandBuilder::build(profile, testSettings(), testRegistry());
    QCOMPARE(command.arguments,
             QStringList({ "-z", "1", "-ngl", "all", "-a1", "A", "-c", "16384" }));
}

void TestCommandBuilder::booleanFalseEmitsNothing()
{
    Profile profile = baseProfile();
    profile.params.insert(QStringLiteral("verbose"), QStringLiteral("false"));

    const BuiltCommand command = CommandBuilder::build(profile, testSettings(), testRegistry());
    QVERIFY(!command.arguments.contains(QStringLiteral("-v")));
    QVERIFY(command.arguments.isEmpty());
}

void TestCommandBuilder::booleanTrueEmitsBareFlag()
{
    Profile profile = baseProfile();
    profile.params.insert(QStringLiteral("verbose"), QStringLiteral("true"));

    const BuiltCommand command = CommandBuilder::build(profile, testSettings(), testRegistry());
    QCOMPARE(command.arguments, QStringList({ "-v" }));
}

void TestCommandBuilder::tristateEmitsOnOffOrNothing_data()
{
    QTest::addColumn<QString>("value");
    QTest::addColumn<QStringList>("expected");

    QTest::newRow("on") << "on" << QStringList{ "--jinja" };
    QTest::newRow("off") << "off" << QStringList{ "--no-jinja" };
    QTest::newRow("ON majuscules") << "ON" << QStringList{ "--jinja" };
    QTest::newRow("valeur inattendue") << "peut-etre" << QStringList{};
    QTest::newRow("vide") << "" << QStringList{};
}

void TestCommandBuilder::tristateEmitsOnOffOrNothing()
{
    QFETCH(QString, value);
    QFETCH(QStringList, expected);

    Profile profile = baseProfile();
    profile.params.insert(QStringLiteral("jinja"), value);

    const BuiltCommand command = CommandBuilder::build(profile, testSettings(), testRegistry());
    QCOMPARE(command.arguments, expected);
}

void TestCommandBuilder::valueEqualToDefaultIsStillEmitted()
{
    Profile profile = baseProfile();
    // `auto` est le défaut déclaré de -fa : la présence dans le profil signifie
    // « l'utilisateur l'a choisi », donc le flag doit apparaître.
    profile.params.insert(QStringLiteral("flash-attn"), QStringLiteral("auto"));
    profile.params.insert(QStringLiteral("ctx-size"), QStringLiteral("0"));

    const BuiltCommand command = CommandBuilder::build(profile, testSettings(), testRegistry());
    QCOMPARE(command.arguments, QStringList({ "-c", "0", "-fa", "auto" }));
}

void TestCommandBuilder::emptyValueIsSkipped()
{
    Profile profile = baseProfile();
    profile.params.insert(QStringLiteral("alpha"), QString());

    const BuiltCommand command = CommandBuilder::build(profile, testSettings(), testRegistry());
    QVERIFY(command.arguments.isEmpty());
}

void TestCommandBuilder::appliesToFiltersPerBinary()
{
    Profile profile = baseProfile();
    profile.params.insert(QStringLiteral("port"), QStringLiteral("8080"));
    profile.params.insert(QStringLiteral("chat-file"), QStringLiteral("C:/c.txt"));

    const ParamRegistry registry = testRegistry();

    const BuiltCommand asServer = CommandBuilder::build(profile, testSettings(), registry);
    QCOMPARE(asServer.arguments, QStringList({ "--port", "8080" }));

    profile.binary = BinaryKind::Cli;
    const BuiltCommand asCli = CommandBuilder::build(profile, testSettings(), registry);
    QCOMPARE(asCli.arguments, QStringList({ "-cf", "C:/c.txt" }));
}

void TestCommandBuilder::pathWithSpacesIsQuotedOnceAndRawInArguments()
{
    Profile profile = baseProfile();
    profile.modelPath = QStringLiteral("D:/mes modèles/qwen3 27b.gguf");
    profile.params.insert(QStringLiteral("mmproj"), QStringLiteral("D:/mes modèles/mmproj f16.gguf"));

    const BuiltCommand command = CommandBuilder::build(profile, testSettings(), testRegistry());

    // QProcess gère lui-même l'échappement : les arguments restent nus.
    QCOMPARE(command.arguments,
             QStringList({ "-m", "D:/mes modèles/qwen3 27b.gguf", "-mm",
                           "D:/mes modèles/mmproj f16.gguf" }));

    QCOMPARE(command.displayLine,
             QStringLiteral("C:\\llama.cpp\\bin\\llama-server.exe "
                            "-m \"D:\\mes modèles\\qwen3 27b.gguf\" "
                            "-mm \"D:\\mes modèles\\mmproj f16.gguf\""));

    // Et la ligne affichée se redécoupe exactement en les arguments d'origine,
    // séparateurs Windows mis à part.
    const QStringList reparsed = CommandBuilder::splitArgumentLine(command.displayLine);
    QCOMPARE(reparsed.size(), command.arguments.size() + 1);
    QCOMPARE(reparsed.at(2), QStringLiteral("D:\\mes modèles\\qwen3 27b.gguf"));
}

void TestCommandBuilder::onlyPathsAreNormalizedInDisplayLine()
{
    Profile profile = baseProfile();
    // `alpha` est de type string : ses barres obliques ne doivent pas bouger.
    profile.params.insert(QStringLiteral("alpha"), QStringLiteral("a/b/c"));
    profile.params.insert(QStringLiteral("mmproj"), QStringLiteral("D:/x/y.gguf"));

    const BuiltCommand command = CommandBuilder::build(profile, testSettings(), testRegistry());
    QVERIFY(command.displayLine.contains(QStringLiteral("-a1 a/b/c")));
    QVERIFY(command.displayLine.contains(QStringLiteral("-mm D:\\x\\y.gguf")));
}

void TestCommandBuilder::extraArgsRespectQuotes()
{
    Profile profile = baseProfile();
    profile.extraArgs = QStringLiteral("--no-warmup --alias \"Qwen 27B\" -v");

    const BuiltCommand command = CommandBuilder::build(profile, testSettings(), testRegistry());
    QCOMPARE(command.arguments,
             QStringList({ "--no-warmup", "--alias", "Qwen 27B", "-v" }));
    QVERIFY(command.displayLine.endsWith(QStringLiteral("--no-warmup --alias \"Qwen 27B\" -v")));
}

void TestCommandBuilder::missingExecutableFallsBackToConventionalName()
{
    Profile profile = baseProfile();
    profile.modelPath = QStringLiteral("C:/models/a.gguf");

    const BuiltCommand command = CommandBuilder::build(profile, Settings{}, testRegistry());

    // `program` reste vide : c'est la validation qui doit bloquer le lancement.
    QVERIFY(command.program.isEmpty());
    QVERIFY(command.displayLine.startsWith(QStringLiteral("llama-server.exe ")));
}

void TestCommandBuilder::cliBinaryUsesCliExecutable()
{
    Profile profile = baseProfile();
    profile.binary = BinaryKind::Cli;

    const BuiltCommand command = CommandBuilder::build(profile, testSettings(), testRegistry());
    QCOMPARE(command.program, QStringLiteral("C:/llama.cpp/bin/llama-cli.exe"));
}

void TestCommandBuilder::shippedParamsJsonIsValid()
{
    QString error;
    const ParamRegistry registry =
        ParamRegistry::fromFile(QStringLiteral(LLAMABUILDER_PARAMS_JSON), &error);

    QVERIFY2(!registry.isEmpty(), qUtf8Printable(error));

    // Chaque paramètre livré doit être exploitable par l'UI : libellé, info-bulle
    // et section rattachée à une section déclarée.
    for (const ParamDef& param : registry.params()) {
        QVERIFY2(!param.label.isEmpty(), qUtf8Printable(param.key));
        QVERIFY2(!param.tooltip.isEmpty(), qUtf8Printable(param.key));
        QVERIFY2(registry.findSection(param.section) != nullptr, qUtf8Printable(param.key));
    }
    // Le chemin du modèle a son propre champ dans le profil : le dupliquer dans
    // params.json créerait deux sources de vérité.
    QVERIFY(registry.find(QStringLiteral("model")) == nullptr);
}

QTEST_APPLESS_MAIN(TestCommandBuilder)
#include "test_commandbuilder.moc"
