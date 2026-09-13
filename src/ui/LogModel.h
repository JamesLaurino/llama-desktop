#pragma once

#include <QAbstractListModel>
#include <QQmlEngine>
#include <QStringList>

namespace ui {

/// Journal fusionné du processus llama.cpp (§5.5).
///
/// Un modèle plutôt qu'un TextArea qui accumule : 5 000 lignes dans un document
/// unique font remettre en page tout le document à chaque ajout, alors qu'une
/// ListView ne dispose que ses délégués visibles. La différence se voit pendant
/// le chargement d'un modèle, qui produit des centaines de lignes par seconde.
class LogModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Exposé par App.logs.")

    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    /// Tampon circulaire du §5.5.
    static constexpr int kCapacity = 5000;

    /// Nature d'une ligne, déduite de son contenu. `Meta` désigne les lignes que
    /// l'application insère elle-même (commande lancée, code de sortie) : elles
    /// ne viennent pas de llama.cpp et ne doivent pas se confondre avec lui.
    enum Severity { Normal = 0, Warning = 1, Error = 2, Meta = 3 };
    Q_ENUM(Severity)

    enum Roles { LineRole = Qt::UserRole + 1, SeverityRole };

    explicit LogModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return static_cast<int>(m_lines.size()); }

    /// Ajoute un lot en une seule notification, et évince d'autant par le début
    /// si la capacité est dépassée.
    void appendLines(const QStringList& lines);
    void appendMeta(const QString& line);

    Q_INVOKABLE void clear();
    /// Tout le tampon, pour le bouton « Copier tout » (§5.5).
    Q_INVOKABLE QString allText() const;

    /// Sévérité déduite du contenu. Statique et pure : testable sans modèle.
    static Severity severityOf(const QString& line);

signals:
    void countChanged();

private:
    struct Entry {
        QString text;
        Severity severity = Normal;
    };

    void append(const QList<Entry>& entries);

    QList<Entry> m_lines;
};

} // namespace ui
