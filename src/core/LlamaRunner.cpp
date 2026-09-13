#include "core/LlamaRunner.h"

#include <QFileInfo>
#include <QRegularExpression>

namespace core {

QString runStateToString(RunState state)
{
    switch (state) {
    case RunState::Idle:     return QStringLiteral("idle");
    case RunState::Starting: return QStringLiteral("starting");
    case RunState::Running:  return QStringLiteral("running");
    case RunState::Stopping: return QStringLiteral("stopping");
    case RunState::Exited:   return QStringLiteral("exited");
    case RunState::Failed:   return QStringLiteral("failed");
    }
    return QStringLiteral("idle");
}

namespace ServerLog {

QString listeningUrl(const QString& line)
{
    static const QRegularExpression pattern(QStringLiteral("listening on\\s+(\\S+)"),
                                            QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = pattern.match(line);
    if (!match.hasMatch())
        return {};

    QString url = match.captured(1);
    // Une ponctuation de fin de phrase n'appartient pas à l'URL.
    while (url.endsWith(u'.') || url.endsWith(u',') || url.endsWith(u';'))
        url.chop(1);
    return url;
}

bool isBindFailure(const QString& line)
{
    return line.contains(QLatin1String("couldn't bind"), Qt::CaseInsensitive);
}

QString browsableUrl(const QString& url)
{
    QString result = url;
    result.replace(QLatin1String("//0.0.0.0"), QLatin1String("//127.0.0.1"));
    result.replace(QLatin1String("//[::]"), QLatin1String("//[::1]"));
    return result;
}

} // namespace ServerLog

LlamaRunner::LlamaRunner(QObject* parent)
    : QObject(parent)
    , m_process(new QProcess(this))
{
    // Un flux unique : stdout et stderr entrelacés dans l'ordre d'écriture (§10).
    m_process->setProcessChannelMode(QProcess::MergedChannels);

    m_flushTimer.setSingleShot(true);
    m_flushTimer.setInterval(kFlushMs);
    connect(&m_flushTimer, &QTimer::timeout, this, &LlamaRunner::flush);

    m_graceTimer.setSingleShot(true);
    connect(&m_graceTimer, &QTimer::timeout, this, [this] {
        if (m_state == RunState::Stopping)
            killNow();
    });

    connect(m_process, &QProcess::readyRead, this, &LlamaRunner::onReadyRead);
    connect(m_process, &QProcess::started, this, [this] { setState(RunState::Running); });
    connect(m_process, &QProcess::finished, this, &LlamaRunner::onFinished);
    connect(m_process, &QProcess::errorOccurred, this, &LlamaRunner::onErrorOccurred);
}

LlamaRunner::~LlamaRunner()
{
    if (m_process->state() == QProcess::NotRunning)
        return;
    // Filet du §10 : pas de processus orphelin, même sur un chemin de sortie qui
    // n'est pas passé par la confirmation de fermeture.
    m_process->kill();
    m_process->waitForFinished(2000);
}

bool LlamaRunner::isRunning() const
{
    return m_state == RunState::Starting || m_state == RunState::Running
        || m_state == RunState::Stopping;
}

qint64 LlamaRunner::pid() const
{
    return m_process->processId();
}

bool LlamaRunner::start(const BuiltCommand& command)
{
    if (isRunning()) {
        m_lastError = QStringLiteral("Un processus est déjà en cours.");
        return false;
    }
    if (command.program.trimmed().isEmpty()) {
        m_lastError =
            QStringLiteral("Aucun exécutable : renseigne son chemin dans les Réglages.");
        setState(RunState::Failed);
        emit failedToStart(m_lastError);
        return false;
    }

    m_pending.clear();
    m_buffered.clear();
    m_lastExitCode = 0;
    m_lastError.clear();
    m_stopRequested = false;

    m_process->setProgram(command.program);
    m_process->setArguments(command.arguments);
    // Répertoire de travail : le dossier de l'exécutable (§10). C'est là que
    // llama.cpp trouve ses DLL et que ses chemins relatifs se résolvent.
    m_process->setWorkingDirectory(QFileInfo(command.program).absolutePath());

    setState(RunState::Starting);
    m_process->start();
    return true;
}

void LlamaRunner::requestStop()
{
    if (m_state == RunState::Stopping) {
        // Deuxième demande pendant le délai de grâce : on n'attend plus.
        killNow();
        return;
    }
    if (!isRunning())
        return;

    m_stopRequested = true;
    setState(RunState::Stopping);
    m_process->terminate();
    m_graceTimer.start(m_graceMs);
}

void LlamaRunner::killNow()
{
    m_graceTimer.stop();
    if (m_process->state() == QProcess::NotRunning)
        return;
    m_stopRequested = true;
    setState(RunState::Stopping);
    m_process->kill();
}

QStringList LlamaRunner::splitChunk(const QByteArray& chunk, QByteArray& pending)
{
    pending.append(chunk);

    QStringList lines;
    qsizetype start = 0;
    qsizetype i = 0;
    while (i < pending.size()) {
        const char c = pending.at(i);
        if (c != '\n' && c != '\r') {
            ++i;
            continue;
        }
        // Un \r en dernière position est indécidable : le \n d'un \r\n peut
        // arriver dans le morceau suivant. On attend.
        if (c == '\r' && i + 1 == pending.size())
            break;

        lines.append(stripAnsi(QString::fromUtf8(pending.constData() + start, i - start)));
        i += (c == '\r' && pending.at(i + 1) == '\n') ? 2 : 1;
        start = i;
    }
    pending.remove(0, start);
    return lines;
}

QString LlamaRunner::stripAnsi(const QString& line)
{
    if (!line.contains(QChar(0x1B)))
        return line;

    static const QRegularExpression osc(
        QStringLiteral("\\x1B\\][^\\x07\\x1B]*(?:\\x07|\\x1B\\\\)"));
    static const QRegularExpression csi(QStringLiteral("\\x1B\\[[0-9;?]*[ -/]*[@-~]"));

    QString cleaned = line;
    cleaned.remove(osc);
    cleaned.remove(csi);
    return cleaned;
}

void LlamaRunner::setState(RunState state)
{
    if (m_state == state)
        return;
    m_state = state;
    emit stateChanged();
}

void LlamaRunner::onReadyRead()
{
    m_buffered += splitChunk(m_process->readAll(), m_pending);
    if (!m_flushTimer.isActive())
        m_flushTimer.start();
}

void LlamaRunner::flush()
{
    if (m_buffered.isEmpty())
        return;
    const QStringList lines = m_buffered;
    m_buffered.clear();
    emit linesProduced(lines);
}

void LlamaRunner::onFinished(int exitCode, QProcess::ExitStatus status)
{
    m_graceTimer.stop();
    m_flushTimer.stop();

    // Ce qui reste sans fin de ligne est une ligne à part entière : une erreur
    // fatale n'est pas toujours suivie d'un saut de ligne.
    m_buffered += splitChunk(m_process->readAll(), m_pending);
    if (!m_pending.isEmpty()) {
        m_buffered.append(stripAnsi(QString::fromUtf8(m_pending)));
        m_pending.clear();
    }
    flush();

    m_lastExitCode = exitCode;
    const bool crashed = (status == QProcess::CrashExit) && !m_stopRequested;
    setState(RunState::Exited);
    emit finished(exitCode, crashed, m_stopRequested);
}

void LlamaRunner::onErrorOccurred(QProcess::ProcessError error)
{
    if (error != QProcess::FailedToStart) {
        // Les autres erreurs sont suivies de finished() : ne pas doubler l'état.
        m_lastError = m_process->errorString();
        return;
    }
    m_lastError = QStringLiteral("Le processus n'a pas pu démarrer : %1")
                      .arg(m_process->errorString());
    setState(RunState::Failed);
    emit failedToStart(m_lastError);
}

} // namespace core
