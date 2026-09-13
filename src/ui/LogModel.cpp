#include "ui/LogModel.h"

namespace ui {

LogModel::LogModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int LogModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : count();
}

QVariant LogModel::data(const QModelIndex& index, int role) const
{
    if (index.row() < 0 || index.row() >= m_lines.size())
        return {};
    const Entry& entry = m_lines.at(index.row());
    switch (role) {
    case LineRole:     return entry.text;
    case SeverityRole: return static_cast<int>(entry.severity);
    default:           return {};
    }
}

QHash<int, QByteArray> LogModel::roleNames() const
{
    // « line » et non « text » : le délégué est un Text, qui a déjà cette
    // propriété, et une collision de nom y passerait inaperçue.
    return { { LineRole, QByteArrayLiteral("line") },
             { SeverityRole, QByteArrayLiteral("severity") } };
}

void LogModel::appendLines(const QStringList& lines)
{
    QList<Entry> entries;
    entries.reserve(lines.size());
    for (const QString& line : lines)
        entries.append({ line, severityOf(line) });
    append(entries);
}

void LogModel::appendMeta(const QString& line)
{
    append({ { line, Meta } });
}

void LogModel::append(const QList<Entry>& entries)
{
    if (entries.isEmpty())
        return;

    // Une salve plus grande que la capacité : seule sa fin est conservée, et
    // elle chasse alors tout ce qui précède.
    const qsizetype keep = qMin<qsizetype>(entries.size(), kCapacity);
    const qsizetype first = entries.size() - keep;

    // Éviction en un seul lot : la ListView reçoit une suppression et une
    // insertion par salve, pas une par ligne.
    const qsizetype drop = qMax<qsizetype>(0, m_lines.size() + keep - kCapacity);
    if (drop > 0) {
        beginRemoveRows({}, 0, static_cast<int>(drop) - 1);
        m_lines.remove(0, drop);
        endRemoveRows();
    }

    beginInsertRows({}, count(), count() + static_cast<int>(keep) - 1);
    for (qsizetype i = first; i < entries.size(); ++i)
        m_lines.append(entries.at(i));
    endInsertRows();
    emit countChanged();
}

void LogModel::clear()
{
    if (m_lines.isEmpty())
        return;
    beginResetModel();
    m_lines.clear();
    endResetModel();
    emit countChanged();
}

QString LogModel::allText() const
{
    QStringList lines;
    lines.reserve(m_lines.size());
    for (const Entry& entry : m_lines)
        lines.append(entry.text);
    return lines.join(u'\n');
}

LogModel::Severity LogModel::severityOf(const QString& line)
{
    // llama.cpp préfixe ses niveaux, mais pas systématiquement : la recherche
    // porte sur le contenu, et reste volontairement grossière — elle ne sert
    // qu'à colorer, jamais à décider quoi que ce soit.
    if (line.contains(QLatin1String("error"), Qt::CaseInsensitive)
        || line.contains(QLatin1String("failed"), Qt::CaseInsensitive)
        || line.contains(QLatin1String("couldn't"), Qt::CaseInsensitive)) {
        return Error;
    }
    if (line.contains(QLatin1String("warn"), Qt::CaseInsensitive))
        return Warning;
    return Normal;
}

} // namespace ui
