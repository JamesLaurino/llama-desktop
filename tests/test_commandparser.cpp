#include "core/CommandBuilder.h"
#include "core/CommandParser.h"
#include "core/ParamRegistry.h"
#include "core/Profile.h"
#include "core/Settings.h"

#include <QTest>

using namespace core;

namespace {

/// Registre figé : les règles d'analyse ne doivent pas dépendre d'un fichier
/// qui bouge. Les alias reproduisent ceux du vrai params.json, formes longues
/// comprises, parce que c'est précisément ce que l'import doit reconnaître.
const char* kRegistryJson = R"json({
  "sections": [
    { "id": "gpu",     "label": "GPU" },
    { "id": "context", "label": "Contexte" },
    { "id": "server",  "label": "Serveur" }
  ],
  "params": [
    { "key": "n-gpu-layers", "flag": "-ngl", "aliases": ["--gpu-layers", "--n-gpu-layers"],
      "section": "gpu", "type": "intOrKeyword", "keywords": ["auto", "all"], "label": "Couches GPU" },
    { "key": "ctx-size",   "flag": "-c",  "aliases": ["--ctx-size"], "section": "context",
      "type": "int", "label": "Contexte" },
    { "key": "temp",       "flag": "--temp", "aliases": ["--temperature"], "section": "context",
      "type": "float", "label": "Température" },
    { "key": "flash-attn", "flag": "-fa", "aliases": ["--flash-attn"], "section": "context",
      "type": "enum", "values": ["on", "off", "auto"], "label": "Flash Attention" },
    { "key": "mmproj",     "flag": "-mm", "aliases": ["--mmproj"], "section": "gpu",
      "type": "path", "label": "Projecteur" },
    { "key": "verbose",    "flag": "-v",  "aliases": ["--verbose"], "section": "context",
      "type": "bool", "label": "Verbeux" },
    { "key": "jinja",      "flag": "--jinja", "flagOff": "--no-jinja", "section": "context",
      "type": "tristate", "label": "Jinja" },
    { "key": "webui",      "flag": "--webui", "aliases": ["--ui"],
      "flagOff": "--no-webui", "aliasesOff": ["--no-ui"], "section": "server",
      "type": "tristate", "label": "Interface web" },
    { "key": "port",       "flag": "--port", "section": "server", "type": "int",
      "label": "Port", "appliesTo": ["server"] }
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

ParsedCommand parse(const QString& line, const ParamRegistry& registry)
{
    return CommandParser::parse(line, registry);
}

} // namespace

class TestCommandParser : public QObject
{
    Q_OBJECT

private slots:
    // --- Reconnaissance des drapeaux --------------------------------------
    void shortAndLongFormsAreEquivalent_data();
    void shortAndLongFormsAreEquivalent();
    void inlineValueFormIsAccepted();
    void modelFlagFeedsModelPathNotParams();
    void executableDecidesBinaryKind_data();
    void executableDecidesBinaryKind();

    // --- Types particuliers ----------------------------------------------
    void booleanFlagNeedsNoValue();
    void tristateNegationIsRecognised();
    void enumWithoutValueFallsBackToOn();
    void negativeNumbersAreValuesNotFlags();
    void duplicateFlagKeepsLastValue();

    // --- Ce qui n'est pas reconnu ----------------------------------------
    void unknownFlagGoesToExtraArgsWithNote();
    void valuelessFlagGoesToExtraArgsWithNote();
    void strayTokenGoesToExtraArgsWithNote();
    void chainedCommandsAreRefused_data();
    void chainedCommandsAreRefused();
    void emptyLineIsRefused();

    // --- Confort de collage ----------------------------------------------
    void lineContinuationsAreJoined_data();
    void lineContinuationsAreJoined();
    void powershellCallOperatorIsIgnored();
    void quotedPathWithSpacesSurvives();

    // --- Symétrie avec le générateur -------------------------------------
    void roundTripPreservesGeneratedCommand();
    void roundTripCoversEveryParamType();
    void knownFlagInExtraArgsIsNormalised();
    void appliesToMismatchIsKeptButFlagged();

    // --- Contrats du registre livré --------------------------------------
    void shippedRegistryResolvesCommonLongForms_data();
    void shippedRegistryResolvesCommonLongForms();
    void shippedRegistryHasNoAmbiguousFlag();
    void oppositeFlagsAreNotAliases();
    void duplicateFlagInRegistryIsRefused();
};

void TestCommandParser::shortAndLongFormsAreEquivalent_data()
{
    QTest::addColumn<QString>("line");

    QTest::newRow("courte") << QStringLiteral("-c 16384 -ngl 99");
    QTest::newRow("longue") << QStringLiteral("--ctx-size 16384 --n-gpu-layers 99");
    QTest::newRow("longue alternative") << QStringLiteral("--ctx-size 16384 --gpu-layers 99");
    QTest::newRow("mélangée") << QStringLiteral("--ctx-size 16384 -ngl 99");
}

void TestCommandParser::shortAndLongFormsAreEquivalent()
{
    QFETCH(QString, line);
    const ParsedCommand parsed = parse(line, testRegistry());

    QVERIFY2(parsed.isValid(), qUtf8Printable(parsed.error));
    QCOMPARE(parsed.params.value(QStringLiteral("ctx-size")), QStringLiteral("16384"));
    QCOMPARE(parsed.params.value(QStringLiteral("n-gpu-layers")), QStringLiteral("99"));
    QVERIFY(parsed.extraArgs.isEmpty());
}

void TestCommandParser::inlineValueFormIsAccepted()
{
    // Forme jamais émise par cette application, mais omniprésente ailleurs.
    const ParsedCommand parsed = parse(QStringLiteral("--ctx-size=16384 --temperature=0.7"),
                                       testRegistry());
    QCOMPARE(parsed.params.value(QStringLiteral("ctx-size")), QStringLiteral("16384"));
    QCOMPARE(parsed.params.value(QStringLiteral("temp")), QStringLiteral("0.7"));
    QVERIFY(parsed.extraArgs.isEmpty());
}

void TestCommandParser::modelFlagFeedsModelPathNotParams()
{
    const ParamRegistry registry = testRegistry();
    // `model` n'est pas un paramètre du registre : c'est un champ du profil.
    QVERIFY(registry.find(QStringLiteral("model")) == nullptr);

    for (const QString& line : { QStringLiteral("-m C:/modeles/q.gguf"),
                                 QStringLiteral("--model C:/modeles/q.gguf"),
                                 QStringLiteral("--model=C:/modeles/q.gguf") }) {
        const ParsedCommand parsed = parse(line, registry);
        QCOMPARE(parsed.modelPath, QStringLiteral("C:/modeles/q.gguf"));
        QVERIFY(parsed.params.isEmpty());
        QVERIFY(parsed.extraArgs.isEmpty());
    }
}

void TestCommandParser::executableDecidesBinaryKind_data()
{
    QTest::addColumn<QString>("line");
    QTest::addColumn<bool>("detected");
    QTest::addColumn<int>("kind");

    QTest::newRow("server") << QStringLiteral("llama-server.exe -c 1") << true
                            << int(BinaryKind::Server);
    QTest::newRow("cli") << QStringLiteral("llama-cli.exe -c 1") << true << int(BinaryKind::Cli);
    QTest::newRow("chemin complet cité")
        << QStringLiteral("\"C:/mes outils/llama-server.exe\" -c 1") << true
        << int(BinaryKind::Server);
    // Le mot « serveurs » est dans le dossier, « cli » dans le nom : c'est le nom
    // du fichier qui tranche.
    QTest::newRow("dossier trompeur") << QStringLiteral("C:/serveurs/llama-cli.exe -c 1") << true
                                      << int(BinaryKind::Cli);
    QTest::newRow("nom inconnu") << QStringLiteral("truc.exe -c 1") << false
                                 << int(BinaryKind::Server);
    QTest::newRow("aucun exécutable") << QStringLiteral("-c 1") << false
                                      << int(BinaryKind::Server);
}

void TestCommandParser::executableDecidesBinaryKind()
{
    QFETCH(QString, line);
    QFETCH(bool, detected);
    QFETCH(int, kind);

    const ParsedCommand parsed = parse(line, testRegistry());
    QCOMPARE(parsed.binaryDetected, detected);
    QCOMPARE(int(parsed.binary), kind);
    QCOMPARE(parsed.params.value(QStringLiteral("ctx-size")), QStringLiteral("1"));
}

void TestCommandParser::booleanFlagNeedsNoValue()
{
    const ParsedCommand parsed = parse(QStringLiteral("-v -c 512"), testRegistry());
    QCOMPARE(parsed.params.value(QStringLiteral("verbose")), QStringLiteral("true"));
    // Le drapeau booléen ne doit pas avaler la valeur du paramètre suivant.
    QCOMPARE(parsed.params.value(QStringLiteral("ctx-size")), QStringLiteral("512"));

    const ParsedCommand negated = parse(QStringLiteral("--verbose=false"), testRegistry());
    QCOMPARE(negated.params.value(QStringLiteral("verbose")), QStringLiteral("false"));
}

void TestCommandParser::tristateNegationIsRecognised()
{
    const ParamRegistry registry = testRegistry();
    QCOMPARE(parse(QStringLiteral("--jinja"), registry).params.value(QStringLiteral("jinja")),
             QStringLiteral("on"));
    QCOMPARE(parse(QStringLiteral("--no-jinja"), registry).params.value(QStringLiteral("jinja")),
             QStringLiteral("off"));
    // Alias des deux formes : `--ui` et `--no-ui` désignent le même paramètre.
    QCOMPARE(parse(QStringLiteral("--ui"), registry).params.value(QStringLiteral("webui")),
             QStringLiteral("on"));
    QCOMPARE(parse(QStringLiteral("--no-ui"), registry).params.value(QStringLiteral("webui")),
             QStringLiteral("off"));
    // Négation du drapeau et négation de la valeur se composent.
    QCOMPARE(parse(QStringLiteral("--no-jinja=false"), registry)
                 .params.value(QStringLiteral("jinja")),
             QStringLiteral("on"));
}

void TestCommandParser::enumWithoutValueFallsBackToOn()
{
    // `-fa` était un booléen dans les versions antérieures de llama.cpp, et c'est
    // encore la forme la plus répandue en ligne.
    const ParsedCommand parsed = parse(QStringLiteral("-fa -c 2048"), testRegistry());
    QCOMPARE(parsed.params.value(QStringLiteral("flash-attn")), QStringLiteral("on"));
    QCOMPARE(parsed.params.value(QStringLiteral("ctx-size")), QStringLiteral("2048"));
    QVERIFY(!parsed.notes.isEmpty());

    // Avec valeur explicite, rien n'est deviné.
    const ParsedCommand explicitValue = parse(QStringLiteral("-fa auto"), testRegistry());
    QCOMPARE(explicitValue.params.value(QStringLiteral("flash-attn")), QStringLiteral("auto"));
}

void TestCommandParser::negativeNumbersAreValuesNotFlags()
{
    const ParsedCommand parsed = parse(QStringLiteral("--temp -1 -c 4096"), testRegistry());
    QCOMPARE(parsed.params.value(QStringLiteral("temp")), QStringLiteral("-1"));
    QCOMPARE(parsed.params.value(QStringLiteral("ctx-size")), QStringLiteral("4096"));
    QVERIFY(parsed.extraArgs.isEmpty());

    const ParsedCommand decimal = parse(QStringLiteral("--temp -.5"), testRegistry());
    QCOMPARE(decimal.params.value(QStringLiteral("temp")), QStringLiteral("-.5"));
}

void TestCommandParser::duplicateFlagKeepsLastValue()
{
    const ParsedCommand parsed = parse(QStringLiteral("-c 1024 --ctx-size 8192"), testRegistry());
    QCOMPARE(parsed.params.value(QStringLiteral("ctx-size")), QStringLiteral("8192"));
    // Une seule entrée dans l'aperçu, sinon l'utilisateur croirait à deux réglages.
    int occurrences = 0;
    for (const ParsedEntry& entry : parsed.entries)
        occurrences += (entry.key == QLatin1String("ctx-size")) ? 1 : 0;
    QCOMPARE(occurrences, 1);
    QVERIFY(!parsed.notes.isEmpty());
}

void TestCommandParser::unknownFlagGoesToExtraArgsWithNote()
{
    // `--mlock` existe dans llama.cpp mais pas dans notre registre : il doit
    // survivre à l'import, pas disparaître.
    const ParsedCommand parsed = parse(QStringLiteral("-c 512 --mlock --rpc 127.0.0.1:1234"),
                                       testRegistry());
    QCOMPARE(parsed.params.value(QStringLiteral("ctx-size")), QStringLiteral("512"));
    QVERIFY(parsed.extraArgs.contains(QStringLiteral("--mlock")));
    QVERIFY(parsed.extraArgs.contains(QStringLiteral("--rpc")));
    QVERIFY(parsed.extraArgs.contains(QStringLiteral("127.0.0.1:1234")));
    QVERIFY(parsed.notes.size() >= 2);
}

void TestCommandParser::valuelessFlagGoesToExtraArgsWithNote()
{
    // `-c` en fin de ligne : inventer une valeur serait pire que la conserver.
    const ParsedCommand parsed = parse(QStringLiteral("-ngl 99 -c"), testRegistry());
    QVERIFY(!parsed.params.contains(QStringLiteral("ctx-size")));
    QCOMPARE(parsed.params.value(QStringLiteral("n-gpu-layers")), QStringLiteral("99"));
    QCOMPARE(parsed.extraArgs, QStringLiteral("-c"));
    QVERIFY(!parsed.notes.isEmpty());
}

void TestCommandParser::strayTokenGoesToExtraArgsWithNote()
{
    const ParsedCommand parsed = parse(QStringLiteral("-c 512 orphelin"), testRegistry());
    QCOMPARE(parsed.extraArgs, QStringLiteral("orphelin"));
    QVERIFY(!parsed.notes.isEmpty());
}

void TestCommandParser::chainedCommandsAreRefused_data()
{
    QTest::addColumn<QString>("line");

    QTest::newRow("et logique") << QStringLiteral("llama-server.exe -c 1 && echo fini");
    QTest::newRow("tube") << QStringLiteral("llama-server.exe -c 1 | more");
    QTest::newRow("redirection") << QStringLiteral("llama-server.exe -c 1 > sortie.log");
    QTest::newRow("stderr") << QStringLiteral("llama-server.exe -c 1 2>&1");
}

void TestCommandParser::chainedCommandsAreRefused()
{
    QFETCH(QString, line);
    const ParsedCommand parsed = parse(line, testRegistry());
    // Refus explicite : avaler la seconde commande donnerait un profil faux.
    QVERIFY(!parsed.isValid());
    QVERIFY(parsed.params.isEmpty());
}

void TestCommandParser::emptyLineIsRefused()
{
    for (const QString& line : { QString(), QStringLiteral("   "), QStringLiteral("\n\t ") }) {
        const ParsedCommand parsed = parse(line, testRegistry());
        QVERIFY(!parsed.isValid());
    }
}

void TestCommandParser::lineContinuationsAreJoined_data()
{
    QTest::addColumn<QString>("line");

    QTest::newRow("antislash (sh)") << QStringLiteral("llama-server.exe \\\n  -c 16384 \\\n  -ngl 99");
    QTest::newRow("accent grave (PowerShell)")
        << QStringLiteral("llama-server.exe `\n  -c 16384 `\n  -ngl 99");
    QTest::newRow("caret (cmd)") << QStringLiteral("llama-server.exe ^\n  -c 16384 ^\n  -ngl 99");
    QTest::newRow("simples retours à la ligne")
        << QStringLiteral("llama-server.exe\n  -c 16384\n  -ngl 99");
}

void TestCommandParser::lineContinuationsAreJoined()
{
    QFETCH(QString, line);
    const ParsedCommand parsed = parse(line, testRegistry());

    QVERIFY2(parsed.isValid(), qUtf8Printable(parsed.error));
    QCOMPARE(parsed.params.value(QStringLiteral("ctx-size")), QStringLiteral("16384"));
    QCOMPARE(parsed.params.value(QStringLiteral("n-gpu-layers")), QStringLiteral("99"));
    QVERIFY2(parsed.extraArgs.isEmpty(), qUtf8Printable(parsed.extraArgs));
}

void TestCommandParser::powershellCallOperatorIsIgnored()
{
    const ParsedCommand parsed =
        parse(QStringLiteral("& \"C:/mes outils/llama-server.exe\" -c 512"), testRegistry());
    QVERIFY2(parsed.isValid(), qUtf8Printable(parsed.error));
    QCOMPARE(parsed.executablePath, QStringLiteral("C:/mes outils/llama-server.exe"));
    QCOMPARE(parsed.params.value(QStringLiteral("ctx-size")), QStringLiteral("512"));
}

void TestCommandParser::quotedPathWithSpacesSurvives()
{
    const ParsedCommand parsed =
        parse(QStringLiteral("-m \"D:/mes modèles/qwen3 27b q5.gguf\" -mm \"C:/a b/mmproj.gguf\""),
              testRegistry());
    QCOMPARE(parsed.modelPath, QStringLiteral("D:/mes modèles/qwen3 27b q5.gguf"));
    QCOMPARE(parsed.params.value(QStringLiteral("mmproj")), QStringLiteral("C:/a b/mmproj.gguf"));
}

void TestCommandParser::roundTripPreservesGeneratedCommand()
{
    const ParamRegistry registry = testRegistry();
    const Settings settings = testSettings();

    Profile profile = Profile::createNew();
    profile.name = QStringLiteral("Aller-retour");
    profile.modelPath = QStringLiteral("D:/mes modèles/qwen3 27b q5.gguf");
    profile.binary = BinaryKind::Server;
    profile.params = {
        { QStringLiteral("n-gpu-layers"), QStringLiteral("all") },
        { QStringLiteral("ctx-size"), QStringLiteral("16384") },
        { QStringLiteral("flash-attn"), QStringLiteral("on") },
        { QStringLiteral("temp"), QStringLiteral("0.7") },
        { QStringLiteral("verbose"), QStringLiteral("true") },
        { QStringLiteral("jinja"), QStringLiteral("off") },
        { QStringLiteral("port"), QStringLiteral("8080") },
    };
    // Volontairement sans drapeau connu du registre : voir
    // knownFlagInExtraArgsIsNormalised pour ce cas, qui déplace l'argument.
    profile.extraArgs = QStringLiteral("--rpc 127.0.0.1:1234");

    const BuiltCommand original = CommandBuilder::build(profile, settings, registry);
    const ParsedCommand parsed = parse(original.displayLine, registry);
    QVERIFY2(parsed.isValid(), qUtf8Printable(parsed.error));

    Profile restored = Profile::createNew();
    restored.name = profile.name;
    CommandParser::applyTo(parsed, restored);

    // Les champs qui appartiennent au profil, pas à la commande, sont préservés.
    QCOMPARE(restored.name, profile.name);
    QVERIFY(restored.id != profile.id);

    QCOMPARE(restored.modelPath, profile.modelPath);
    QCOMPARE(int(restored.binary), int(profile.binary));
    QCOMPARE(restored.params, profile.params);

    // La propriété qui compte : la commande régénérée est identique, au caractère.
    const BuiltCommand again = CommandBuilder::build(restored, settings, registry);
    QCOMPARE(again.arguments, original.arguments);
    QCOMPARE(again.displayLine, original.displayLine);
}

void TestCommandParser::roundTripCoversEveryParamType()
{
    const ParamRegistry registry = testRegistry();
    const Settings settings = testSettings();

    // Un cas par type déclarable, pour qu'aucun ne puisse régresser en silence.
    const QVector<QPair<QString, QString>> byType = {
        { QStringLiteral("ctx-size"), QStringLiteral("8192") },      // int
        { QStringLiteral("temp"), QStringLiteral("0.65") },          // float
        { QStringLiteral("verbose"), QStringLiteral("true") },       // bool
        { QStringLiteral("jinja"), QStringLiteral("off") },          // tristate
        { QStringLiteral("flash-attn"), QStringLiteral("auto") },    // enum
        { QStringLiteral("mmproj"), QStringLiteral("C:/a b/m.gguf") }, // path
        { QStringLiteral("n-gpu-layers"), QStringLiteral("all") },   // intOrKeyword
    };

    for (const auto& [key, value] : byType) {
        Profile profile = Profile::createNew();
        profile.binary = BinaryKind::Server;
        profile.modelPath = QStringLiteral("C:/m.gguf");
        profile.params = { { key, value } };

        const BuiltCommand original = CommandBuilder::build(profile, settings, registry);
        const ParsedCommand parsed = parse(original.displayLine, registry);
        QVERIFY2(parsed.isValid(), qUtf8Printable(parsed.error));
        QVERIFY2(parsed.extraArgs.isEmpty(),
                 qUtf8Printable(QStringLiteral("%1 : %2").arg(key, parsed.extraArgs)));
        QCOMPARE(parsed.params.value(key), value);

        Profile restored = Profile::createNew();
        CommandParser::applyTo(parsed, restored);
        const BuiltCommand again = CommandBuilder::build(restored, settings, registry);
        QVERIFY2(again.displayLine == original.displayLine, qUtf8Printable(key));
    }
}

void TestCommandParser::knownFlagInExtraArgsIsNormalised()
{
    // Un drapeau du registre écrit à la main dans les arguments libres remonte
    // dans le formulaire. La commande change d'ordre mais pas de sens : c'est
    // une normalisation assumée, et le test la fixe pour qu'elle reste voulue.
    const ParamRegistry registry = testRegistry();
    Profile profile = Profile::createNew();
    profile.extraArgs = QStringLiteral("-c 4096");

    const BuiltCommand original = CommandBuilder::build(profile, testSettings(), registry);
    const ParsedCommand parsed = parse(original.displayLine, registry);

    QCOMPARE(parsed.params.value(QStringLiteral("ctx-size")), QStringLiteral("4096"));
    QVERIFY(parsed.extraArgs.isEmpty());
}

void TestCommandParser::appliesToMismatchIsKeptButFlagged()
{
    // `--port` ne concerne que le serveur. Collé avec llama-cli, il est conservé
    // dans le profil — on ne perd pas l'intention — mais signalé.
    const ParsedCommand parsed =
        parse(QStringLiteral("llama-cli.exe --port 8080 -c 512"), testRegistry());

    QCOMPARE(int(parsed.binary), int(BinaryKind::Cli));
    QCOMPARE(parsed.params.value(QStringLiteral("port")), QStringLiteral("8080"));

    bool flagged = false;
    for (const ParsedEntry& entry : parsed.entries) {
        if (entry.key == QLatin1String("port"))
            flagged = !entry.appliesToBinary;
    }
    QVERIFY(flagged);
    QVERIFY(!parsed.notes.isEmpty());

    // La génération, elle, ne l'émet pas : les deux comportements sont cohérents.
    Profile profile = Profile::createNew();
    CommandParser::applyTo(parsed, profile);
    const BuiltCommand command = CommandBuilder::build(profile, testSettings(), testRegistry());
    QVERIFY(!command.arguments.contains(QStringLiteral("--port")));
}

void TestCommandParser::shippedRegistryResolvesCommonLongForms_data()
{
    QTest::addColumn<QString>("flag");
    QTest::addColumn<QString>("key");

    // Les écritures qu'on trouve dans les README et les billets de blog : si
    // l'une d'elles cessait d'être reconnue, l'import perdrait son intérêt.
    QTest::newRow("--ctx-size") << QStringLiteral("--ctx-size") << QStringLiteral("ctx-size");
    QTest::newRow("--n-gpu-layers")
        << QStringLiteral("--n-gpu-layers") << QStringLiteral("n-gpu-layers");
    QTest::newRow("--gpu-layers") << QStringLiteral("--gpu-layers")
                                  << QStringLiteral("n-gpu-layers");
    QTest::newRow("--flash-attn") << QStringLiteral("--flash-attn")
                                  << QStringLiteral("flash-attn");
    QTest::newRow("--threads") << QStringLiteral("--threads") << QStringLiteral("threads");
    QTest::newRow("--temperature") << QStringLiteral("--temperature") << QStringLiteral("temp");
    QTest::newRow("--cache-type-k") << QStringLiteral("--cache-type-k")
                                    << QStringLiteral("cache-type-k");
    QTest::newRow("--batch-size") << QStringLiteral("--batch-size")
                                  << QStringLiteral("batch-size");
    QTest::newRow("--ubatch-size") << QStringLiteral("--ubatch-size")
                                   << QStringLiteral("ubatch-size");
    QTest::newRow("--parallel") << QStringLiteral("--parallel") << QStringLiteral("parallel");
    QTest::newRow("--n-predict") << QStringLiteral("--n-predict") << QStringLiteral("n-predict");
    QTest::newRow("--predict") << QStringLiteral("--predict") << QStringLiteral("n-predict");
    QTest::newRow("--seed") << QStringLiteral("--seed") << QStringLiteral("seed");
    QTest::newRow("--split-mode") << QStringLiteral("--split-mode")
                                  << QStringLiteral("split-mode");
    QTest::newRow("--alias") << QStringLiteral("--alias") << QStringLiteral("alias");
}

void TestCommandParser::shippedRegistryResolvesCommonLongForms()
{
    QFETCH(QString, flag);
    QFETCH(QString, key);

    QString error;
    const ParamRegistry registry =
        ParamRegistry::fromFile(QStringLiteral(LLAMABUILDER_PARAMS_JSON), &error);
    QVERIFY2(!registry.isEmpty(), qUtf8Printable(error));

    const auto match = registry.findByFlag(flag);
    QVERIFY2(match.has_value(), qUtf8Printable(flag));
    QCOMPARE(match->param->key, key);
    QVERIFY(!match->negated);
}

void TestCommandParser::shippedRegistryHasNoAmbiguousFlag()
{
    QString error;
    const ParamRegistry registry =
        ParamRegistry::fromFile(QStringLiteral(LLAMABUILDER_PARAMS_JSON), &error);
    // Le chargement échouerait sur un doublon : ce test vérifie surtout que
    // chaque forme déclarée retrouve bien son propre paramètre.
    QVERIFY2(!registry.isEmpty(), qUtf8Printable(error));

    for (const ParamDef& param : registry.params()) {
        const auto positive = registry.findByFlag(param.flag);
        QVERIFY2(positive.has_value(), qUtf8Printable(param.flag));
        QCOMPARE(positive->param->key, param.key);
        QVERIFY(!positive->negated);

        for (const QString& alias : param.aliases) {
            const auto match = registry.findByFlag(alias);
            QVERIFY2(match.has_value(), qUtf8Printable(alias));
            QCOMPARE(match->param->key, param.key);
            QVERIFY2(!match->negated, qUtf8Printable(alias));
        }
        if (!param.flagOff.isEmpty()) {
            const auto negative = registry.findByFlag(param.flagOff);
            QVERIFY2(negative.has_value(), qUtf8Printable(param.flagOff));
            QCOMPARE(negative->param->key, param.key);
            QVERIFY2(negative->negated, qUtf8Printable(param.flagOff));
        }
        for (const QString& alias : param.aliasesOff) {
            const auto match = registry.findByFlag(alias);
            QVERIFY2(match.has_value(), qUtf8Printable(alias));
            QCOMPARE(match->param->key, param.key);
            QVERIFY2(match->negated, qUtf8Printable(alias));
        }
    }
}

void TestCommandParser::oppositeFlagsAreNotAliases()
{
    QString error;
    const ParamRegistry registry =
        ParamRegistry::fromFile(QStringLiteral(LLAMABUILDER_PARAMS_JSON), &error);
    QVERIFY2(!registry.isEmpty(), qUtf8Printable(error));

    // Pièges relevés dans le --help du build de référence : ces drapeaux sont
    // listés sur la même ligne que les nôtres alors qu'ils en sont l'inverse.
    // Les prendre pour des alias inverserait silencieusement le réglage.
    for (const QString& opposite : { QStringLiteral("-kvo"), QStringLiteral("--kv-offload"),
                                    QStringLiteral("--warmup"), QStringLiteral("--mmap"),
                                    QStringLiteral("--mlock") }) {
        QVERIFY2(!registry.findByFlag(opposite).has_value(), qUtf8Printable(opposite));
    }

    // `-nkvo` et `--no-kv-offload`, en revanche, sont bien le même paramètre.
    const auto nkvo = registry.findByFlag(QStringLiteral("-nkvo"));
    const auto longForm = registry.findByFlag(QStringLiteral("--no-kv-offload"));
    QVERIFY(nkvo.has_value() && longForm.has_value());
    QCOMPARE(nkvo->param->key, longForm->param->key);
}

void TestCommandParser::duplicateFlagInRegistryIsRefused()
{
    // Un alias qui recouvrirait le drapeau d'un autre paramètre ferait importer
    // le mauvais réglage. Le registre doit refuser le fichier.
    const char* json = R"json({
      "sections": [ { "id": "s", "label": "S" } ],
      "params": [
        { "key": "a", "flag": "-a", "section": "s", "type": "int" },
        { "key": "b", "flag": "-b", "aliases": ["-a"], "section": "s", "type": "int" }
      ]
    })json";

    QString error;
    const ParamRegistry registry = ParamRegistry::fromJson(QByteArray(json), &error);
    QVERIFY(registry.isEmpty());
    QVERIFY2(error.contains(QStringLiteral("-a")), qUtf8Printable(error));

    // Et « aliasesOff » sans « flagOff » n'a pas de sens.
    const char* orphan = R"json({
      "sections": [ { "id": "s", "label": "S" } ],
      "params": [
        { "key": "a", "flag": "-a", "aliasesOff": ["--no-a"], "section": "s", "type": "bool" }
      ]
    })json";
    QVERIFY(ParamRegistry::fromJson(QByteArray(orphan), &error).isEmpty());
}

QTEST_APPLESS_MAIN(TestCommandParser)
#include "test_commandparser.moc"
