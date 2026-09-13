#include "core/JsonFile.h"
#include "core/ParamRegistry.h"
#include "core/Profile.h"
#include "core/ProfileStore.h"
#include "core/Settings.h"
#include "core/SettingsStore.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace core;

class TestStore : public QObject
{
    Q_OBJECT

private slots:
    void init();

    void registryRejectsMalformedJson_data();
    void registryRejectsMalformedJson();
    void registryPreservesDeclarationOrder();
    void registryTreatsMissingAppliesToAsBoth();

    void profileRoundTripsThroughJson();
    void profileWithoutIdGetsOne();

    void storeStartsEmptyWhenFileAbsent();
    void storePersistsAndReloads();
    void storeSortsByRecentUse();
    void storeSetsAsideCorruptFile();
    void storeDebouncesSaves();

    void settingsRoundTripAndClamping();

private:
    QTemporaryDir m_dir;
    QString path(const QString& name) const { return QDir(m_dir.path()).filePath(name); }
};

void TestStore::init()
{
    QVERIFY(m_dir.isValid());
    // Chaque test repart d'un dossier propre.
    const QDir dir(m_dir.path());
    for (const QString& name : dir.entryList(QDir::Files))
        QVERIFY(QFile::remove(dir.filePath(name)));
}

void TestStore::registryRejectsMalformedJson_data()
{
    QTest::addColumn<QByteArray>("json");

    QTest::newRow("json invalide") << QByteArray("{ nope }");
    QTest::newRow("racine tableau") << QByteArray("[]");
    QTest::newRow("aucun parametre") << QByteArray(R"({"sections":[],"params":[]})");
    QTest::newRow("cle manquante")
        << QByteArray(R"({"sections":[],"params":[{"flag":"-x","type":"int"}]})");
    QTest::newRow("flag manquant")
        << QByteArray(R"({"sections":[],"params":[{"key":"x","type":"int"}]})");
    QTest::newRow("type inconnu")
        << QByteArray(R"({"sections":[],"params":[{"key":"x","flag":"-x","type":"colour"}]})");
    QTest::newRow("cle dupliquee")
        << QByteArray(R"({"sections":[],"params":[
              {"key":"x","flag":"-x","type":"int"},
              {"key":"x","flag":"-y","type":"int"}]})");
    QTest::newRow("section inconnue")
        << QByteArray(R"({"sections":[],"params":[
              {"key":"x","flag":"-x","type":"int","section":"nulle-part"}]})");
    QTest::newRow("tristate sans flagOff")
        << QByteArray(R"({"sections":[],"params":[{"key":"x","flag":"-x","type":"tristate"}]})");
    QTest::newRow("enum sans values")
        << QByteArray(R"({"sections":[],"params":[{"key":"x","flag":"-x","type":"enum"}]})");
}

void TestStore::registryRejectsMalformedJson()
{
    QFETCH(QByteArray, json);

    QString error;
    const ParamRegistry registry = ParamRegistry::fromJson(json, &error);
    QVERIFY(registry.isEmpty());
    QVERIFY2(!error.isEmpty(), "un échec doit toujours être expliqué");
}

void TestStore::registryPreservesDeclarationOrder()
{
    const QByteArray json = R"({
      "sections": [{ "id": "s", "label": "S", "defaultExpanded": true }],
      "params": [
        { "key": "zeta",  "flag": "-z", "section": "s", "type": "int" },
        { "key": "alpha", "flag": "-a", "section": "s", "type": "int" }
      ]
    })";

    QString error;
    const ParamRegistry registry = ParamRegistry::fromJson(json, &error);
    QVERIFY2(!registry.isEmpty(), qUtf8Printable(error));
    QCOMPARE(registry.params().size(), 2);
    QCOMPARE(registry.params().at(0).key, QStringLiteral("zeta"));
    QCOMPARE(registry.params().at(1).key, QStringLiteral("alpha"));
    QCOMPARE(registry.find(QStringLiteral("alpha"))->flag, QStringLiteral("-a"));
    QVERIFY(registry.find(QStringLiteral("absent")) == nullptr);
    QVERIFY(registry.findSection(QStringLiteral("s"))->defaultExpanded);
}

void TestStore::registryTreatsMissingAppliesToAsBoth()
{
    const QByteArray json = R"({
      "sections": [],
      "params": [{ "key": "x", "flag": "-x", "type": "int" }]
    })";

    QString error;
    const ParamRegistry registry = ParamRegistry::fromJson(json, &error);
    QVERIFY2(!registry.isEmpty(), qUtf8Printable(error));
    const ParamDef* param = registry.find(QStringLiteral("x"));
    QVERIFY(param);
    QVERIFY(param->appliesToBinary(BinaryKind::Server));
    QVERIFY(param->appliesToBinary(BinaryKind::Cli));
}

void TestStore::profileRoundTripsThroughJson()
{
    Profile original = Profile::createNew();
    original.name = QStringLiteral("Qwen3 27B — contexte long");
    original.modelPath = QStringLiteral("D:/mes modèles/qwen3.gguf");
    original.binary = BinaryKind::Cli;
    original.params = { { QStringLiteral("ctx-size"), QStringLiteral("16384") },
                        { QStringLiteral("flash-attn"), QStringLiteral("on") } };
    original.extraArgs = QStringLiteral("--no-warmup");
    original.notes = QStringLiteral("Tient en 22 Go");
    original.lastUsedAt = QDateTime::currentDateTimeUtc();

    const Profile restored = Profile::fromJson(original.toJson());

    QCOMPARE(restored.id, original.id);
    QCOMPARE(restored.name, original.name);
    QCOMPARE(restored.modelPath, original.modelPath);
    QCOMPARE(restored.binary, original.binary);
    QCOMPARE(restored.params, original.params);
    QCOMPARE(restored.extraArgs, original.extraArgs);
    QCOMPARE(restored.notes, original.notes);
    // ISO 8601 à la seconde : on compare à cette précision.
    QCOMPARE(restored.lastUsedAt.toSecsSinceEpoch(), original.lastUsedAt.toSecsSinceEpoch());
    QVERIFY(restored.hasName());
}

void TestStore::profileWithoutIdGetsOne()
{
    QJsonObject object;
    object.insert(QStringLiteral("name"), QStringLiteral("Écrit à la main"));

    const Profile profile = Profile::fromJson(object);
    QVERIFY(!profile.id.isEmpty());
    QVERIFY(profile.createdAt.isValid());
    QCOMPARE(profile.binary, BinaryKind::Server);
}

void TestStore::storeStartsEmptyWhenFileAbsent()
{
    ProfileStore store(path(QStringLiteral("profiles.json")));
    QVERIFY(store.load());
    QCOMPARE(store.count(), 0);
    QVERIFY(store.lastError().isEmpty());
}

void TestStore::storePersistsAndReloads()
{
    const QString file = path(QStringLiteral("profiles.json"));

    Profile profile = Profile::createNew();
    profile.name = QStringLiteral("Mon Qwen");
    profile.modelPath = QStringLiteral("D:/models/qwen.gguf");
    profile.params.insert(QStringLiteral("ctx-size"), QStringLiteral("8192"));

    {
        ProfileStore store(file);
        QVERIFY(store.load());
        store.upsert(profile);
        QVERIFY(store.saveNow());
    }

    // Critère d'acceptation n°1 : le profil survit à la fermeture.
    ProfileStore reloaded(file);
    QVERIFY(reloaded.load());
    QCOMPARE(reloaded.count(), 1);
    const Profile* restored = reloaded.byId(profile.id);
    QVERIFY(restored);
    QCOMPARE(restored->name, QStringLiteral("Mon Qwen"));
    QCOMPARE(restored->params.value(QStringLiteral("ctx-size")), QStringLiteral("8192"));

    // upsert sur un identifiant existant remplace, il n'ajoute pas.
    Profile renamed = *restored;
    renamed.name = QStringLiteral("Mon Qwen v2");
    reloaded.upsert(renamed);
    QCOMPARE(reloaded.count(), 1);
    QCOMPARE(reloaded.byId(profile.id)->name, QStringLiteral("Mon Qwen v2"));

    QVERIFY(reloaded.remove(profile.id));
    QCOMPARE(reloaded.count(), 0);
    QVERIFY(!reloaded.remove(profile.id));
}

void TestStore::storeSortsByRecentUse()
{
    const QDateTime now = QDateTime::currentDateTimeUtc();

    Profile never = Profile::createNew();
    never.name = QStringLiteral("jamais utilisé");
    Profile old = Profile::createNew();
    old.name = QStringLiteral("ancien");
    old.lastUsedAt = now.addDays(-3);
    Profile recent = Profile::createNew();
    recent.name = QStringLiteral("récent");
    recent.lastUsedAt = now;

    ProfileStore store(path(QStringLiteral("profiles.json")));
    QVERIFY(store.load());
    store.replaceAll({ never, old, recent });
    store.sortByRecentUse();

    QCOMPARE(store.profiles().at(0).name, QStringLiteral("récent"));
    QCOMPARE(store.profiles().at(1).name, QStringLiteral("ancien"));
    QCOMPARE(store.profiles().at(2).name, QStringLiteral("jamais utilisé"));
}

void TestStore::storeSetsAsideCorruptFile()
{
    const QString file = path(QStringLiteral("profiles.json"));
    {
        QFile broken(file);
        QVERIFY(broken.open(QIODevice::WriteOnly));
        broken.write("[ { \"name\": \"tronqu");
    }

    ProfileStore store(file);
    QVERIFY(!store.load());
    QCOMPARE(store.count(), 0);
    QVERIFY(!store.lastError().isEmpty());

    // Ne jamais supprimer silencieusement : le fichier est archivé et l'original
    // a disparu de son emplacement pour ne pas être relu en boucle.
    QVERIFY(!store.corruptBackupPath().isEmpty());
    QVERIFY(QFile::exists(store.corruptBackupPath()));
    QVERIFY(!QFile::exists(file));
}

void TestStore::storeDebouncesSaves()
{
    const QString file = path(QStringLiteral("profiles.json"));
    ProfileStore store(file);
    QVERIFY(store.load());

    QSignalSpy savedSpy(&store, &ProfileStore::saved);

    Profile profile = Profile::createNew();
    profile.name = QStringLiteral("A");
    store.upsert(profile);
    profile.name = QStringLiteral("B");
    store.upsert(profile);
    profile.name = QStringLiteral("C");
    store.upsert(profile);

    QVERIFY(store.hasPendingSave());
    QCOMPARE(savedSpy.count(), 0);

    // Trois mutations rapprochées ne doivent produire qu'une seule écriture.
    QVERIFY(savedSpy.wait(ProfileStore::kSaveDebounceMs * 6));
    QCOMPARE(savedSpy.count(), 1);
    QVERIFY(!store.hasPendingSave());

    const JsonFile::ReadResult result = JsonFile::read(file);
    QVERIFY(result.ok());
    QVERIFY(result.document.isArray());
    QCOMPARE(result.document.array().size(), 1);
    QCOMPARE(result.document.array().at(0).toObject().value(QStringLiteral("name")).toString(),
             QStringLiteral("C"));
}

void TestStore::settingsRoundTripAndClamping()
{
    const QString file = path(QStringLiteral("settings.json"));

    Settings settings;
    settings.llamaServerPath = QStringLiteral("C:/llama.cpp/bin/llama-server.exe");
    settings.llamaCliPath = QStringLiteral("C:/llama.cpp/bin/llama-cli.exe");
    settings.defaultModelsDir = QStringLiteral("D:/mes modèles");
    settings.monitorIntervalMs = 500;
    settings.theme = QStringLiteral("dark");

    {
        SettingsStore store(file);
        QVERIFY(store.load());
        store.setSettings(settings);
    }

    SettingsStore reloaded(file);
    QVERIFY(reloaded.load());
    QCOMPARE(reloaded.settings().llamaServerPath, settings.llamaServerPath);
    QCOMPARE(reloaded.settings().defaultModelsDir, settings.defaultModelsDir);
    QCOMPARE(reloaded.settings().monitorIntervalMs, 500);
    QCOMPARE(reloaded.settings().executableFor(BinaryKind::Cli), settings.llamaCliPath);
    QCOMPARE(reloaded.settings().executableFor(BinaryKind::Server), settings.llamaServerPath);

    // Un intervalle absurde écrit à la main est ramené dans des bornes viables.
    QJsonObject broken;
    broken.insert(QStringLiteral("monitorIntervalMs"), 1);
    QCOMPARE(Settings::fromJson(broken).monitorIntervalMs, 200);
    broken.insert(QStringLiteral("monitorIntervalMs"), 999999);
    QCOMPARE(Settings::fromJson(broken).monitorIntervalMs, 10000);
}

QTEST_MAIN(TestStore)
#include "test_store.moc"
