#pragma once

#include "core/CommandBuilder.h"

#include <QByteArray>
#include <QObject>
#include <QProcess>
#include <QStringList>
#include <QTimer>

namespace core {

/// Cycle de vie du processus llama.cpp.
///
/// `Stopping` est un état à part entière : sous Windows l'arrêt demandé n'est
/// jamais instantané (voir requestStop), et l'interface doit pouvoir le dire.
enum class RunState { Idle, Starting, Running, Stopping, Exited, Failed };

QString runStateToString(RunState state);

/// Lecture du journal de llama-server.
///
/// Les formats ne sont pas supposés : ils sont relevés dans
/// `llama-server-impl.dll` du build de référence (llama-server.exe n'est qu'un
/// lanceur de 9 Ko).
///
///     srv  %12.*s: listening on %s
///     srv  %12.*s: couldn't bind HTTP server socket, hostname: %s, port: %d
namespace ServerLog {

/// URL annoncée par la ligne « listening on », vide si la ligne n'annonce rien.
QString listeningUrl(const QString& line);

/// La ligne signale que le socket HTTP n'a pas pu être lié.
bool isBindFailure(const QString& line);

/// URL ouvrable dans un navigateur. 0.0.0.0 veut dire « toutes les interfaces »
/// côté serveur et ne se route pas côté client : le navigateur reçoit 127.0.0.1.
QString browsableUrl(const QString& url);

} // namespace ServerLog

/// Lance et surveille un processus llama.cpp (§10).
///
/// QObject mais sans rien de l'interface : les tests l'exercent avec un faux
/// enfant. Les lignes sortent par lots (voir kFlushMs) parce qu'un signal par
/// ligne sature la boucle d'événements pendant le chargement d'un modèle, qui
/// en produit plusieurs centaines par seconde.
class LlamaRunner : public QObject
{
    Q_OBJECT

public:
    /// Délai de grâce du §10 entre terminate() et kill().
    static constexpr int kGraceMs = 5000;
    /// Période de regroupement des lignes avant émission.
    static constexpr int kFlushMs = 50;

    explicit LlamaRunner(QObject* parent = nullptr);
    ~LlamaRunner() override;

    /// Démarre la commande. Faux si un processus tourne déjà ou si le programme
    /// est vide ; `lastError()` dit lequel des deux.
    bool start(const BuiltCommand& command);

    /// Arrêt propre puis forcé. Un deuxième appel pendant le délai de grâce tue
    /// immédiatement : c'est ce que fait le bouton « forcer ».
    void requestStop();
    /// Tue sans délai. Utilisé à la fermeture de l'application (§10).
    void killNow();

    RunState state() const { return m_state; }
    bool isRunning() const;
    qint64 pid() const;
    int lastExitCode() const { return m_lastExitCode; }
    QString lastError() const { return m_lastError; }
    /// Vrai si l'arrêt en cours ou terminé a été demandé par l'utilisateur.
    bool stopRequested() const { return m_stopRequested; }

    /// Délai de grâce, en millisecondes. Abaissé par les tests : attendre cinq
    /// secondes réelles pour vérifier une escalade n'apprend rien de plus.
    void setGraceMs(int ms) { m_graceMs = ms; }

    /// Découpe un morceau de flux en lignes complètes, `pending` conservant le
    /// reste. Statique et pure : c'est la partie délicate, elle se teste sans
    /// processus.
    ///
    /// `\r` sépare comme `\n` : llama.cpp réécrit la ligne courante pour
    /// afficher sa progression, et sans cela tout le chargement arriverait comme
    /// une seule ligne de plusieurs kilo-octets. Conséquence assumée : une
    /// barre de progression apparaît comme des lignes successives.
    static QStringList splitChunk(const QByteArray& chunk, QByteArray& pending);

    /// Retire les séquences d'échappement ANSI. llama.cpp ne colore pas quand sa
    /// sortie est redirigée, mais `--log-colors` peut traîner dans les arguments
    /// libres.
    static QString stripAnsi(const QString& line);

signals:
    void stateChanged();
    void linesProduced(const QStringList& lines);
    /// `crashed` exclut l'arrêt demandé : tuer un processus à la demande n'est
    /// pas un plantage. `requested` permet de ne pas colorer en rouge un arrêt
    /// volontaire, dont le code de sortie est arbitraire.
    void finished(int exitCode, bool crashed, bool requested);
    void failedToStart(const QString& reason);

private:
    void setState(RunState state);
    void onReadyRead();
    void flush();
    void onFinished(int exitCode, QProcess::ExitStatus status);
    void onErrorOccurred(QProcess::ProcessError error);

    QProcess* m_process = nullptr;
    QTimer m_flushTimer;
    QTimer m_graceTimer;
    QByteArray m_pending;
    QStringList m_buffered;

    RunState m_state = RunState::Idle;
    int m_graceMs = kGraceMs;
    int m_lastExitCode = 0;
    bool m_stopRequested = false;
    QString m_lastError;
};

} // namespace core
