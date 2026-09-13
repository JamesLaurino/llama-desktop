#pragma once

#include <QQmlEngine>
#include <QSortFilterProxyModel>

namespace ui {

/// Vue d'une seule section du formulaire.
///
/// Instancié depuis le QML, un par section repliable. Le masquage du §5.3
/// (« Serveur » invisible en mode cli, etc.) n'est pas un `if` dans la vue mais
/// une conséquence du filtre : une section dont `count` tombe à zéro disparaît.
class ParamFilterModel : public QSortFilterProxyModel
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString section READ section WRITE setSection NOTIFY sectionChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    explicit ParamFilterModel(QObject* parent = nullptr);

    QString section() const { return m_section; }
    void setSection(const QString& section);

    int count() const { return rowCount(); }

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

signals:
    void sectionChanged();
    void countChanged();

private:
    QString m_section;
};

} // namespace ui
