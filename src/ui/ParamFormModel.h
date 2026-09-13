#pragma once

#include "core/ParamRegistry.h"

#include <QAbstractListModel>
#include <QMap>
#include <QQmlEngine>

namespace ui {

/// Formulaire dynamique à plat, dans l'ordre de params.json (§5.3).
///
/// Un seul modèle porte les 47 paramètres ; le regroupement par section et le
/// masquage selon le binaire sont assurés par ParamFilterModel. Toute écriture
/// de valeur passe donc par un point unique.
///
/// Convention d'absence : une valeur vide signifie « paramètre non posé », donc
/// retiré de la table du profil et absent de la commande générée.
class ParamFormModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Exposé par App.params.")

public:
    enum Role {
        KeyRole = Qt::UserRole + 1,
        FlagRole,
        FlagOffRole,
        SectionRole,
        LabelRole,
        TooltipRole,
        TypeRole,
        DefaultValueRole,
        MinimumRole,
        MaximumRole,
        StepRole,
        ValuesRole,
        KeywordsRole,
        ValueRole,
        IsSetRole,      ///< posé par l'utilisateur, donc émis dans la commande
        IsModifiedRole, ///< posé ET différent du défaut du binaire
        ApplicableRole, ///< accepté par le binaire courant
    };

    explicit ParamFormModel(const core::ParamRegistry* registry, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    /// Recharge le formulaire pour un profil. Réinitialise le modèle : les
    /// proxys de section recalculent alors leur filtre.
    void setSource(core::BinaryKind binary, const QMap<QString, QString>& values);
    void setBinary(core::BinaryKind binary);

    const QMap<QString, QString>& values() const { return m_values; }
    QString value(const QString& key) const { return m_values.value(key); }

    /// Pose ou retire une valeur. Renvoie vrai si la table a changé.
    bool setValue(const QString& key, const QString& value);
    /// Retire toutes les valeurs d'une section (§5.3, « Réinitialiser »).
    bool clearSection(const QString& sectionId);

signals:
    /// Émis après toute mutation, pour que le contrôleur persiste et recalcule.
    void valuesChanged();

private:
    void emitRowChanged(int row);

    const core::ParamRegistry* m_registry;
    core::BinaryKind m_binary = core::BinaryKind::Server;
    QMap<QString, QString> m_values;
};

} // namespace ui
