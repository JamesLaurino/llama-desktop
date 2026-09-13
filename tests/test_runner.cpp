#include "core/LlamaRunner.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QSignalSpy>
#include <QTest>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

using namespace core;

namespace {

/// Le binaire de test se relance lui-même comme enfant à surveiller.
///
/// La bascule passe par une variable d'environnement et non par un argument :
/// LlamaRunner transmet les arguments du profil, pas les nôtres, et un enfant
/// reconnu à son argv relancerait toute la suite de tests sur lui-même.
constexpr const char* kFakeChildVar = "LLAMABUILDER_FAKE_CHILD";

void writeOut(FILE* stream, const char* text)
{
    std::fputs(text, stream);
    // Les deux flux arrivent dans le même tube : sans vidage explicite, stdout
    // tamponné se déverserait après stderr et l'ordre serait faux.
    std::fflush(stream);
}

int fakeChild(int argc, char* argv[])
{
    const QString mode = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QString();

    if (mode == QLatin1String("lines")) {
        writeOut(stdout, "main: bonjour\n");
        writeOut(stdout, "srv    start_server: listening on http://127.0.0.1:8099\n");
        writeOut(stderr, "load: chargement\n");
        writeOut(stdout, "progress: 10%\rprogress: 100%\n");
        writeOut(stdout, "\x1B[32mcolore\x1B[0m\n");
        return 0;
    }
    if (mode == QLatin1String("partial")) {
        writeOut(stdout, "erreur fatale sans saut de ligne");
        return 0;
    }
    if (mode == QLatin1String("exit"))
        return argc > 2 ? std::atoi(argv[2]) : 0;
    if (mode == QLatin1String("hang")) {
        writeOut(stdout, "pret\n");
        for (;;)
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return 0;
}

#ifdef Q_OS_WIN
bool processIsAlive(qint64 pid)
{
    const HANDLE handle = OpenProcess(SYNCHRONIZE, FALSE, static_cast<DWORD>(pid));
    if (handle == nullptr)
        return false;
    const bool alive = WaitForSingleObject(handle, 0) == WAIT_TIMEOUT;
    CloseHandle(handle);
    return alive;
}
#endif

BuiltCommand childCommand(const QStringList& arguments)
{
    BuiltCommand command;
    command.program = QCoreApplication::applicationFilePath();
    command.arguments = arguments;
    return command;
}

} // namespace

class TestRunner : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    // --- Découpage du flux, sans processus -----------------------------------
    void splitChunkJoinsPartialLines();
    void splitChunkTreatsCarriageReturnAsSeparator();
    void splitChunkKeepsCrLfAsOneBreak();
    void splitChunkHoldsTrailingCarriageReturn();
    void splitChunkDecodesUtf8AcrossChunks();
    void stripAnsiRemovesColourCodes();
    void stripAnsiLeavesPlainLinesUntouched();

    // --- Lecture du journal de llama-server ----------------------------------
    void listeningUrlUsesTheFormatOfTheReferenceBuild();
    void listeningUrlIgnoresUnrelatedLines();
    void bindFailureIsRecognised();
    void wildcardHostIsRewrittenForTheBrowser();

    // --- Processus réel ------------------------------------------------------
    void runStreamsMergedOutputAndExitsZero();
    void nonZeroExitCodeIsReported();
    void lastLineWithoutNewlineIsNotLost();
    void emptyProgramIsRefusedWithoutStarting();
    void missingExecutableFailsToStart();
    void pidIsExposedWhileRunning();
    void startIsRefusedWhileAProcessRuns();
    void requestStopEscalatesToKillAfterTheGracePeriod();
    void secondStopRequestDoesNotWaitForTheGracePeriod();
    void destroyingTheRunnerLeavesNoOrphan();
};

void TestRunner::initTestCase()
{
    // Posée une fois : tous les enfants lancés ensuite en héritent.
    qputenv(kFakeChildVar, "1");
}

void TestRunner::splitChunkJoinsPartialLines()
{
    QByteArray pending;
    QCOMPARE(LlamaRunner::splitChunk("une li", pending), QStringList());
    QCOMPARE(LlamaRunner::splitChunk("gne\n", pending),
             QStringList{ QStringLiteral("une ligne") });
    QVERIFY(pending.isEmpty());
}

void TestRunner::splitChunkTreatsCarriageReturnAsSeparator()
{
    QByteArray pending;
    // La progression de llama.cpp réécrit la ligne courante : sans cela, tout
    // le chargement arriverait comme une seule ligne de plusieurs kilo-octets.
    const QStringList lines = LlamaRunner::splitChunk("10%\r50%\r100%\n", pending);
    QCOMPARE(lines,
             (QStringList{ QStringLiteral("10%"), QStringLiteral("50%"),
                           QStringLiteral("100%") }));
}

void TestRunner::splitChunkKeepsCrLfAsOneBreak()
{
    QByteArray pending;
    QCOMPARE(LlamaRunner::splitChunk("a\r\nb\r\n", pending),
             (QStringList{ QStringLiteral("a"), QStringLiteral("b") }));
}

void TestRunner::splitChunkHoldsTrailingCarriageReturn()
{
    QByteArray pending;
    // Indécidable : le saut de ligne d'un CRLF peut arriver au morceau suivant.
    QCOMPARE(LlamaRunner::splitChunk("a\r", pending), QStringList());
    QCOMPARE(LlamaRunner::splitChunk("\nb\n", pending),
             (QStringList{ QStringLiteral("a"), QStringLiteral("b") }));
}

void TestRunner::splitChunkDecodesUtf8AcrossChunks()
{
    QByteArray pending;
    // Un caractère accentué coupé en deux morceaux : le décodage n'a lieu qu'une
    // fois la ligne complète, il ne doit pas produire de caractère de
    // remplacement.
    QCOMPARE(LlamaRunner::splitChunk(QByteArray("mod\xC3"), pending), QStringList());
    QCOMPARE(LlamaRunner::splitChunk(QByteArray("\xA8le\n"), pending),
             QStringList{ QString::fromUtf8("mod\xC3\xA8le") });
}

void TestRunner::stripAnsiRemovesColourCodes()
{
    QCOMPARE(LlamaRunner::stripAnsi(QStringLiteral("\x1B[32mvert\x1B[0m")),
             QStringLiteral("vert"));
    QCOMPARE(LlamaRunner::stripAnsi(QStringLiteral("\x1B]0;titre\x07reste")),
             QStringLiteral("reste"));
}

void TestRunner::stripAnsiLeavesPlainLinesUntouched()
{
    const QString line = QStringLiteral("srv  load_model: [ok] 100%");
    QCOMPARE(LlamaRunner::stripAnsi(line), line);
}

void TestRunner::listeningUrlUsesTheFormatOfTheReferenceBuild()
{
    // Format relevé dans llama-server-impl.dll du build de référence :
    //     srv  %12.*s: listening on %s
    QCOMPARE(ServerLog::listeningUrl(
                 QStringLiteral("srv    start_server: listening on http://127.0.0.1:8080")),
             QStringLiteral("http://127.0.0.1:8080"));
}

void TestRunner::listeningUrlIgnoresUnrelatedLines()
{
    QVERIFY(ServerLog::listeningUrl(QStringLiteral("main: loading model")).isEmpty());
    QVERIFY(ServerLog::listeningUrl(QString()).isEmpty());
}

void TestRunner::bindFailureIsRecognised()
{
    QVERIFY(ServerLog::isBindFailure(QStringLiteral(
        "srv    start_server: couldn't bind HTTP server socket, hostname: 0.0.0.0, port: 8080")));
    QVERIFY(!ServerLog::isBindFailure(
        QStringLiteral("srv    start_server: listening on http://127.0.0.1:8080")));
}

void TestRunner::wildcardHostIsRewrittenForTheBrowser()
{
    // 0.0.0.0 veut dire « toutes les interfaces » côté serveur ; côté client il
    // ne se route pas.
    QCOMPARE(ServerLog::browsableUrl(QStringLiteral("http://0.0.0.0:8080")),
             QStringLiteral("http://127.0.0.1:8080"));
    QCOMPARE(ServerLog::browsableUrl(QStringLiteral("http://192.168.1.4:8080")),
             QStringLiteral("http://192.168.1.4:8080"));
}

void TestRunner::runStreamsMergedOutputAndExitsZero()
{
    LlamaRunner runner;
    QStringList collected;
    connect(&runner, &LlamaRunner::linesProduced, this,
            [&collected](const QStringList& lines) { collected += lines; });
    QSignalSpy finished(&runner, &LlamaRunner::finished);

    QVERIFY(runner.start(childCommand({ QStringLiteral("lines") })));
    QVERIFY(finished.wait(5000));

    QCOMPARE(collected,
             (QStringList{
                 QStringLiteral("main: bonjour"),
                 QStringLiteral("srv    start_server: listening on http://127.0.0.1:8099"),
                 QStringLiteral("load: chargement"), QStringLiteral("progress: 10%"),
                 QStringLiteral("progress: 100%"), QStringLiteral("colore") }));
    QCOMPARE(finished.at(0).at(0).toInt(), 0);
    QCOMPARE(finished.at(0).at(1).toBool(), false);
    QVERIFY(runner.state() == RunState::Exited);
}

void TestRunner::nonZeroExitCodeIsReported()
{
    LlamaRunner runner;
    QSignalSpy finished(&runner, &LlamaRunner::finished);

    QVERIFY(runner.start(childCommand({ QStringLiteral("exit"), QStringLiteral("3") })));
    QVERIFY(finished.wait(5000));

    QCOMPARE(finished.at(0).at(0).toInt(), 3);
    QCOMPARE(finished.at(0).at(2).toBool(), false); // arrêt non demandé
    QCOMPARE(runner.lastExitCode(), 3);
}

void TestRunner::lastLineWithoutNewlineIsNotLost()
{
    LlamaRunner runner;
    QStringList collected;
    connect(&runner, &LlamaRunner::linesProduced, this,
            [&collected](const QStringList& lines) { collected += lines; });
    QSignalSpy finished(&runner, &LlamaRunner::finished);

    QVERIFY(runner.start(childCommand({ QStringLiteral("partial") })));
    QVERIFY(finished.wait(5000));

    // Une erreur fatale n'est pas toujours suivie d'un saut de ligne.
    QCOMPARE(collected, QStringList{ QStringLiteral("erreur fatale sans saut de ligne") });
}

void TestRunner::emptyProgramIsRefusedWithoutStarting()
{
    LlamaRunner runner;
    QSignalSpy failed(&runner, &LlamaRunner::failedToStart);

    QVERIFY(!runner.start(BuiltCommand{}));
    QCOMPARE(failed.size(), 1);
    QVERIFY(runner.state() == RunState::Failed);
}

void TestRunner::missingExecutableFailsToStart()
{
    LlamaRunner runner;
    QSignalSpy failed(&runner, &LlamaRunner::failedToStart);

    BuiltCommand command;
    command.program = QDir::tempPath() + QStringLiteral("/aucun-llama-server.exe");
    QVERIFY(runner.start(command));

    // Windows peut rapporter l'échec sans repasser par la boucle d'événements :
    // attendre le signal sans compter les émissions déjà arrivées échouerait.
    QTRY_COMPARE_WITH_TIMEOUT(failed.size(), 1, 5000);
    QVERIFY(runner.state() == RunState::Failed);
    QVERIFY(!runner.lastError().isEmpty());
}

void TestRunner::pidIsExposedWhileRunning()
{
    LlamaRunner runner;
    QVERIFY(runner.start(childCommand({ QStringLiteral("hang") })));
    QTRY_VERIFY_WITH_TIMEOUT(runner.state() == RunState::Running, 5000);

    // Le moniteur a besoin de ce PID pour isoler la RAM et la VRAM (§9).
    QVERIFY(runner.pid() > 0);

    QSignalSpy finished(&runner, &LlamaRunner::finished);
    runner.killNow();
    QVERIFY(finished.wait(5000));
}

void TestRunner::startIsRefusedWhileAProcessRuns()
{
    LlamaRunner runner;
    QVERIFY(runner.start(childCommand({ QStringLiteral("hang") })));
    QTRY_VERIFY_WITH_TIMEOUT(runner.state() == RunState::Running, 5000);

    // Un seul processus à la fois (§10).
    QVERIFY(!runner.start(childCommand({ QStringLiteral("hang") })));
    QVERIFY(!runner.lastError().isEmpty());

    QSignalSpy finished(&runner, &LlamaRunner::finished);
    runner.killNow();
    QVERIFY(finished.wait(5000));
}

void TestRunner::requestStopEscalatesToKillAfterTheGracePeriod()
{
    LlamaRunner runner;
    // terminate() poste WM_CLOSE : un programme console ne le voit pas, l'arrêt
    // se solde donc toujours par le kill. Cinq secondes réelles n'apprendraient
    // rien de plus que deux cents millisecondes.
    runner.setGraceMs(200);
    QVERIFY(runner.start(childCommand({ QStringLiteral("hang") })));
    QTRY_VERIFY_WITH_TIMEOUT(runner.state() == RunState::Running, 5000);

    QSignalSpy finished(&runner, &LlamaRunner::finished);
    runner.requestStop();
    QVERIFY(runner.state() == RunState::Stopping);

    QVERIFY(finished.wait(5000));
    // Arrêt demandé : ce n'est pas un plantage, même si le processus a été tué.
    QCOMPARE(finished.at(0).at(1).toBool(), false);
    QCOMPARE(finished.at(0).at(2).toBool(), true);
}

void TestRunner::secondStopRequestDoesNotWaitForTheGracePeriod()
{
    LlamaRunner runner;
    runner.setGraceMs(30000);
    QVERIFY(runner.start(childCommand({ QStringLiteral("hang") })));
    QTRY_VERIFY_WITH_TIMEOUT(runner.state() == RunState::Running, 5000);

    QSignalSpy finished(&runner, &LlamaRunner::finished);
    QElapsedTimer elapsed;
    elapsed.start();
    runner.requestStop();
    runner.requestStop(); // « Forcer »

    QVERIFY(finished.wait(5000));
    QVERIFY(elapsed.elapsed() < 5000);
}

void TestRunner::destroyingTheRunnerLeavesNoOrphan()
{
    qint64 pid = 0;
    {
        LlamaRunner runner;
        QVERIFY(runner.start(childCommand({ QStringLiteral("hang") })));
        QTRY_VERIFY_WITH_TIMEOUT(runner.state() == RunState::Running, 5000);
        pid = runner.pid();
        QVERIFY(pid > 0);
    }
#ifdef Q_OS_WIN
    // §10 : aucun processus orphelin, y compris sur un chemin de sortie qui ne
    // passe pas par la confirmation de fermeture.
    QVERIFY(!processIsAlive(pid));
#endif
}

int main(int argc, char* argv[])
{
    if (qEnvironmentVariableIsSet(kFakeChildVar))
        return fakeChild(argc, argv);

    // QProcess exige une boucle d'événements : pas de QTEST_APPLESS_MAIN ici.
    QCoreApplication app(argc, argv);
    TestRunner test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_runner.moc"
