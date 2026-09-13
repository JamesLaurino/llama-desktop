#include "core/CommandBuilder.h"
#include "core/ParamRegistry.h"
#include "core/Profile.h"
#include "ui/AppController.h"
#include "ui/ParamFilterModel.h"
#include "ui/ParamFormModel.h"
#include "ui/ProfileListModel.h"

#include <QAbstractItemModelTester>
#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace core;
using namespace ui;

namespace {

QString paramsPath()
{
    return QStringLiteral(LLAMABUILDER_PARAMS_JSON);
}

/// Première clé d'une section donnée, pour ne rien coder en dur depuis
/// params.json : le test doit survivre à l'ajout d'un flag.
QString firstKeyOfSection(const ParamRegistry& registry, const QString& section)
{
    for (const ParamDef& param : registry.params()) {
        if (param.section == section)
            return param.key;
    }
    return {};
}

/// Premier paramètre entier réservé au serveur : son flag suivi de sa valeur
/// est reconnaissable sans ambiguïté dans la ligne générée.
QString firstServerOnlyIntKey(const ParamRegistry& registry)
{
    for (const ParamDef& param : registry.params()) {
        if (param.type != ParamType::Int)
            continue;
        if (param.appliesTo.size() == 1
            && param.appliesTo.constFirst().compare(QLatin1String("server"),
                                                    Qt::CaseInsensitive) == 0) {
            return param.key;
        }
    }
    return {};
}

int rowOf(const ParamRegistry& registry, const ParamDef* param)
{
    return static_cast<int>(param - registry.params().constData());
}

} // namespace

class TestUiModels : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void formModelExposesEveryParamInOrder();
    void formModelTreatsEmptyValueAsAbsent();
    void formModelDistinguishesSetFromModified();
    void formModelClearsOnlyRequestedSection();
    void formModelPassesModelTester();

    void filterModelKeepsOnlyItsSection();
    void filterModelHidesParamsRejectedByBinary();

    void controllerCreatesSelectsAndDeletes();
    void controllerWritesParamValuesIntoTheCommand();
    void controllerReportsWhyLaunchIsImpossible();
    void controllerCountsSetParamsPerSection();

    void controllerPreviewsImportWithoutTouchingAnything();
    void controllerImportReplacesCurrentProfileEntirely();
    void controllerImportCreatesProfileAndSelectsIt();
    void controllerRefusesToImportChainedCommand();

private:
    ParamRegistry m_registry;
};

void TestUiModels::initTestCase()
{
    QString error;
    m_registry = ParamRegistry::fromFile(paramsPath(), &error);
    QVERIFY2(!m_registry.isEmpty(), qPrintable(error));
}

void TestUiModels::formModelExposesEveryParamInOrder()
{
    ParamFormModel model(&m_registry);
    QCOMPARE(model.rowCount(), static_cast<int>(m_registry.params().size()));

    for (int row = 0; row < model.rowCount(); ++row) {
        const QModelIndex index = model.index(row);
        QCOMPARE(index.data(ParamFormModel::KeyRole).toString(),
                 m_registry.params().at(row).key);
        QCOMPARE(index.data(ParamFormModel::FlagRole).toString(),
                 m_registry.params().at(row).flag);
    }
}

void TestUiModels::formModelTreatsEmptyValueAsAbsent()
{
    ParamFormModel model(&m_registry);
    QSignalSpy spy(&model, &ParamFormModel::valuesChanged);

    QVERIFY(model.setValue(QStringLiteral("ctx-size"), QStringLiteral("8192")));
    QCOMPARE(model.values().size(), 1);
    QCOMPARE(spy.count(), 1);

    // Poser deux fois la même valeur ne doit rien signaler.
    QVERIFY(!model.setValue(QStringLiteral("ctx-size"), QStringLiteral("8192")));
    QCOMPARE(spy.count(), 1);

    QVERIFY(model.setValue(QStringLiteral("ctx-size"), QString()));
    QVERIFY(model.values().isEmpty());
    QCOMPARE(spy.count(), 2);

    // Une clé inconnue de params.json est refusée.
    QVERIFY(!model.setValue(QStringLiteral("clé-inexistante"), QStringLiteral("x")));
}

void TestUiModels::formModelDistinguishesSetFromModified()
{
    const ParamDef* param = m_registry.find(QStringLiteral("n-gpu-layers"));
    QVERIFY(param);
    QVERIFY2(!param->defaultValue.isEmpty(), "le test suppose un défaut déclaré");

    ParamFormModel model(&m_registry);
    const int row = rowOf(m_registry, param);
    QVERIFY(row >= 0);

    QVERIFY(!model.index(row).data(ParamFormModel::IsSetRole).toBool());
    QVERIFY(!model.index(row).data(ParamFormModel::IsModifiedRole).toBool());

    // Posé à sa valeur par défaut : émis dans la commande, mais non « modifié ».
    model.setValue(param->key, param->defaultValue);
    QVERIFY(model.index(row).data(ParamFormModel::IsSetRole).toBool());
    QVERIFY(!model.index(row).data(ParamFormModel::IsModifiedRole).toBool());

    model.setValue(param->key, QStringLiteral("24"));
    QVERIFY(model.index(row).data(ParamFormModel::IsSetRole).toBool());
    QVERIFY(model.index(row).data(ParamFormModel::IsModifiedRole).toBool());
}

void TestUiModels::formModelClearsOnlyRequestedSection()
{
    const QString gpuKey = firstKeyOfSection(m_registry, QStringLiteral("gpu"));
    const QString contextKey = firstKeyOfSection(m_registry, QStringLiteral("context"));
    QVERIFY(!gpuKey.isEmpty());
    QVERIFY(!contextKey.isEmpty());

    ParamFormModel model(&m_registry);
    model.setValue(gpuKey, QStringLiteral("1"));
    model.setValue(contextKey, QStringLiteral("2"));

    QVERIFY(model.clearSection(QStringLiteral("gpu")));
    QVERIFY(model.value(gpuKey).isEmpty());
    QCOMPARE(model.value(contextKey), QStringLiteral("2"));

    // Rien à retirer : aucun signal parasite.
    QSignalSpy spy(&model, &ParamFormModel::valuesChanged);
    QVERIFY(!model.clearSection(QStringLiteral("gpu")));
    QCOMPARE(spy.count(), 0);
}

void TestUiModels::formModelPassesModelTester()
{
    ParamFormModel model(&m_registry);
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Warning);
    model.setValue(QStringLiteral("ctx-size"), QStringLiteral("4096"));
    model.setBinary(BinaryKind::Cli);
    model.setSource(BinaryKind::Server, {});
    QCOMPARE(model.rowCount(), static_cast<int>(m_registry.params().size()));
}

void TestUiModels::filterModelKeepsOnlyItsSection()
{
    ParamFormModel model(&m_registry);
    ParamFilterModel filter;
    filter.setSourceModel(&model);
    filter.setSection(QStringLiteral("gpu"));

    int expected = 0;
    for (const ParamDef& param : m_registry.params()) {
        if (param.section == QStringLiteral("gpu")
            && param.appliesToBinary(BinaryKind::Server)) {
            ++expected;
        }
    }
    QCOMPARE(filter.count(), expected);
    for (int row = 0; row < filter.rowCount(); ++row) {
        QCOMPARE(filter.index(row, 0).data(ParamFormModel::SectionRole).toString(),
                 QStringLiteral("gpu"));
    }
}

void TestUiModels::filterModelHidesParamsRejectedByBinary()
{
    const QString serverOnly = firstServerOnlyIntKey(m_registry);
    QVERIFY(!serverOnly.isEmpty());
    const ParamDef* param = m_registry.find(serverOnly);
    QVERIFY(param);

    ParamFormModel model(&m_registry);
    ParamFilterModel filter;
    filter.setSourceModel(&model);
    filter.setSection(param->section);

    const auto contains = [&filter, &serverOnly] {
        for (int row = 0; row < filter.rowCount(); ++row) {
            if (filter.index(row, 0).data(ParamFormModel::KeyRole).toString() == serverOnly)
                return true;
        }
        return false;
    };

    QVERIFY(contains());
    model.setBinary(BinaryKind::Cli);
    QVERIFY(!contains());
    model.setBinary(BinaryKind::Server);
    QVERIFY(contains());
}

void TestUiModels::controllerCreatesSelectsAndDeletes()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    AppController controller(paramsPath(), dir.filePath(QStringLiteral("profiles.json")),
                             dir.filePath(QStringLiteral("settings.json")));

    QVERIFY(!controller.hasProfile());
    QCOMPARE(controller.profiles()->rowCount(), 0);

    const QString first = controller.createProfile();
    QVERIFY(!first.isEmpty());
    QVERIFY(controller.hasProfile());
    QCOMPARE(controller.currentProfileId(), first);
    QCOMPARE(controller.profiles()->rowCount(), 1);

    controller.renameProfile(first, QStringLiteral("  Qwen 27B  "));
    QCOMPARE(controller.profileName(), QStringLiteral("Qwen 27B"));

    const QString copy = controller.duplicateProfile(first);
    QCOMPARE(controller.profiles()->rowCount(), 2);
    QCOMPARE(controller.currentProfileId(), copy);
    QVERIFY(controller.profileName().endsWith(QStringLiteral("(copie)")));

    controller.removeProfile(copy);
    QCOMPARE(controller.profiles()->rowCount(), 1);
    QCOMPARE(controller.currentProfileId(), first);

    controller.removeProfile(first);
    QCOMPARE(controller.profiles()->rowCount(), 0);
    QVERIFY(!controller.hasProfile());
}

void TestUiModels::controllerWritesParamValuesIntoTheCommand()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    AppController controller(paramsPath(), dir.filePath(QStringLiteral("profiles.json")),
                             dir.filePath(QStringLiteral("settings.json")));
    controller.createProfile();

    QSignalSpy commandSpy(&controller, &AppController::commandLineChanged);
    controller.setModelPath(QStringLiteral("D:/mes modèles/qwen3.gguf"));
    QVERIFY(commandSpy.count() >= 1);
    QVERIFY(controller.commandLine().contains(QStringLiteral("-m \"D:\\mes modèles\\qwen3.gguf\"")));

    controller.setParamValue(QStringLiteral("ctx-size"), QStringLiteral("16384"));
    QVERIFY(controller.commandLine().contains(QStringLiteral("-c 16384")));

    // Retirer la valeur retire le flag.
    controller.setParamValue(QStringLiteral("ctx-size"), QString());
    QVERIFY(!controller.commandLine().contains(QStringLiteral("-c 16384")));

    // Un paramètre réservé au serveur disparaît de la commande en mode cli.
    const QString serverOnly = firstServerOnlyIntKey(m_registry);
    const ParamDef* param = m_registry.find(serverOnly);
    QVERIFY(param);
    const QString emitted = param->flag + QStringLiteral(" 7");
    controller.setParamValue(serverOnly, QStringLiteral("7"));
    QVERIFY(controller.commandLine().contains(emitted));
    controller.setBinary(QStringLiteral("cli"));
    QVERIFY(!controller.commandLine().contains(emitted));
    // La valeur n'est pas perdue : elle revient avec le binaire.
    controller.setBinary(QStringLiteral("server"));
    QVERIFY(controller.commandLine().contains(emitted));
}

void TestUiModels::controllerReportsWhyLaunchIsImpossible()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    AppController controller(paramsPath(), dir.filePath(QStringLiteral("profiles.json")),
                             dir.filePath(QStringLiteral("settings.json")));

    QVERIFY(!controller.canLaunch());
    QVERIFY(controller.validationError().contains(QStringLiteral("Aucun profil")));

    controller.createProfile();
    QVERIFY(controller.validationError().contains(QStringLiteral("Aucun modèle")));

    controller.setModelPath(dir.filePath(QStringLiteral("absent.gguf")));
    QVERIFY(controller.validationError().contains(QStringLiteral("introuvable")));

    const QString model = dir.filePath(QStringLiteral("modèle réel.gguf"));
    QFile file(model);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("gguf");
    file.close();
    controller.setModelPath(model);
    QVERIFY(controller.validationError().contains(QStringLiteral("Réglages")));

    const QString exe = dir.filePath(QStringLiteral("llama-server.exe"));
    QFile fakeExe(exe);
    QVERIFY(fakeExe.open(QIODevice::WriteOnly));
    fakeExe.write("MZ");
    fakeExe.close();
    controller.applySettings(exe, exe, dir.path(), 1000, QStringLiteral("dark"));
    QVERIFY2(controller.canLaunch(), qPrintable(controller.validationError()));
}

void TestUiModels::controllerCountsSetParamsPerSection()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    AppController controller(paramsPath(), dir.filePath(QStringLiteral("profiles.json")),
                             dir.filePath(QStringLiteral("settings.json")));
    controller.createProfile();

    const QString gpuKey = firstKeyOfSection(m_registry, QStringLiteral("gpu"));
    QVERIFY(!gpuKey.isEmpty());
    QCOMPARE(controller.sectionSetCounts().value(QStringLiteral("gpu")).toInt(), 0);

    controller.setParamValue(gpuKey, QStringLiteral("all"));
    QCOMPARE(controller.sectionSetCounts().value(QStringLiteral("gpu")).toInt(), 1);

    controller.resetSection(QStringLiteral("gpu"));
    QCOMPARE(controller.sectionSetCounts().value(QStringLiteral("gpu")).toInt(), 0);
}

void TestUiModels::controllerPreviewsImportWithoutTouchingAnything()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    AppController controller(paramsPath(), dir.filePath(QStringLiteral("profiles.json")),
                             dir.filePath(QStringLiteral("settings.json")));
    controller.createProfile();
    const QString before = controller.commandLine();

    const QVariantMap preview = controller.analyseCommand(
        QStringLiteral("llama-server.exe -m C:/m.gguf --ctx-size 16384 -ngl 99"));

    QVERIFY(preview.value(QStringLiteral("ok")).toBool());
    QCOMPARE(preview.value(QStringLiteral("modelPath")).toString(), QStringLiteral("C:/m.gguf"));
    QCOMPARE(preview.value(QStringLiteral("binary")).toString(), QStringLiteral("server"));
    QCOMPARE(preview.value(QStringLiteral("entries")).toList().size(), 2);
    QVERIFY(preview.value(QStringLiteral("preview")).toString().contains(QStringLiteral("16384")));

    // L'aperçu est une analyse, pas une application : le profil n'a pas bougé.
    QCOMPARE(controller.commandLine(), before);
    QVERIFY(controller.modelPath().isEmpty());
}

void TestUiModels::controllerImportReplacesCurrentProfileEntirely()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString profilesPath = dir.filePath(QStringLiteral("profiles.json"));
    AppController controller(paramsPath(), profilesPath,
                             dir.filePath(QStringLiteral("settings.json")));

    const QString id = controller.createProfile();
    controller.renameProfile(id, QStringLiteral("Mon profil"));
    controller.setNotes(QStringLiteral("À conserver"));
    controller.setParamValue(QStringLiteral("port"), QStringLiteral("9999"));
    QVERIFY(controller.commandLine().contains(QStringLiteral("9999")));

    QSignalSpy commandSpy(&controller, &AppController::commandLineChanged);
    QVERIFY(controller.importCommandIntoCurrent(
        QStringLiteral("llama-server.exe -m \"D:/a b/q.gguf\" --ctx-size 4096 --flash-attn on")));

    // Ce qui appartient au profil survit ; ce qui décrit la commande est remplacé.
    QCOMPARE(controller.currentProfileId(), id);
    QCOMPARE(controller.profileName(), QStringLiteral("Mon profil"));
    QCOMPARE(controller.notes(), QStringLiteral("À conserver"));
    QCOMPARE(controller.modelPath(), QStringLiteral("D:/a b/q.gguf"));
    QVERIFY(controller.commandLine().contains(QStringLiteral("4096")));
    // Le port posé avant l'import a disparu : l'import remplace, il ne fusionne pas.
    QVERIFY(!controller.commandLine().contains(QStringLiteral("9999")));
    QVERIFY(commandSpy.count() > 0);

    // Et le profil est bien écrit sur disque, pas seulement en mémoire. L'écriture
    // est anti-rebondie (ProfileStore::kSaveDebounceMs) : il faut la laisser venir.
    QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(profilesPath), 3000);
    QTest::qWait(ProfileStore::kSaveDebounceMs);
    AppController reopened(paramsPath(), profilesPath,
                           dir.filePath(QStringLiteral("settings.json")));
    QCOMPARE(reopened.modelPath(), QStringLiteral("D:/a b/q.gguf"));
}

void TestUiModels::controllerImportCreatesProfileAndSelectsIt()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    AppController controller(paramsPath(), dir.filePath(QStringLiteral("profiles.json")),
                             dir.filePath(QStringLiteral("settings.json")));

    const QString id = controller.importCommandAsNewProfile(
        QStringLiteral("llama-cli.exe -m C:/m.gguf -c 2048"), QStringLiteral("  Importé  "));

    QVERIFY(!id.isEmpty());
    QCOMPARE(controller.currentProfileId(), id);
    QCOMPARE(controller.profileName(), QStringLiteral("Importé"));
    QCOMPARE(controller.binary(), QStringLiteral("cli"));
    QCOMPARE(controller.profiles()->rowCount(), 1);

    // Nom vide : un profil sans nom ne serait pas enregistrable (§4.1).
    const QString second = controller.importCommandAsNewProfile(
        QStringLiteral("llama-server.exe -c 512"), QString());
    QVERIFY(!second.isEmpty());
    QVERIFY(!controller.profileName().isEmpty());
}

void TestUiModels::controllerRefusesToImportChainedCommand()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    AppController controller(paramsPath(), dir.filePath(QStringLiteral("profiles.json")),
                             dir.filePath(QStringLiteral("settings.json")));
    controller.createProfile();
    controller.setModelPath(QStringLiteral("C:/intact.gguf"));

    const QString chained = QStringLiteral("llama-server.exe -c 512 && rd /s /q C:\\");
    const QVariantMap preview = controller.analyseCommand(chained);
    QVERIFY(!preview.value(QStringLiteral("ok")).toBool());
    QVERIFY(!preview.value(QStringLiteral("error")).toString().isEmpty());

    QVERIFY(!controller.importCommandIntoCurrent(chained));
    QVERIFY(controller.importCommandAsNewProfile(chained, QStringLiteral("x")).isEmpty());
    // Un refus ne doit rien laisser derrière lui.
    QCOMPARE(controller.modelPath(), QStringLiteral("C:/intact.gguf"));
    QCOMPARE(controller.profiles()->rowCount(), 1);
}

QTEST_GUILESS_MAIN(TestUiModels)
#include "test_uimodels.moc"
