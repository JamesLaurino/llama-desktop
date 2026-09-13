#pragma once

#include "core/Profile.h"

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>
#include <optional>

namespace core {

/// Section repliable du formulaire, déclarée dans params.json.
struct SectionDef {
    QString id;
    QString label;
    bool defaultExpanded = false;
};

enum class ParamType {
    Int,
    Float,
    Bool,          ///< flag seul quand la valeur vaut "true", rien sinon
    Tristate,      ///< "on" émet `flag`, "off" émet `flagOff`, absent n'émet rien
    Enum,
    String,
    Path,
    IntOrKeyword,  ///< entier, ou l'un des `keywords` (ex. -ngl : 32 | auto | all)
};

QString paramTypeToString(ParamType type);
std::optional<ParamType> paramTypeFromString(QStringView text);

/// Définition d'un paramètre llama.cpp, telle que décrite dans params.json.
struct ParamDef {
    QString key;
    QString flag;
    QString flagOff;      ///< uniquement pour Tristate
    QString section;
    QString label;
    QString tooltip;
    ParamType type = ParamType::String;
    QString defaultValue; ///< purement informatif : jamais émis dans la commande
    std::optional<double> minimum;
    std::optional<double> maximum;
    std::optional<double> step;
    QStringList values;   ///< Enum
    QStringList keywords; ///< IntOrKeyword
    QStringList appliesTo;

    bool appliesToBinary(BinaryKind kind) const;
    /// Vrai si le type n'attend pas de valeur sur la ligne de commande.
    bool isFlagOnly() const;
};

/// Catalogue des paramètres, chargé depuis params.json.
///
/// L'ordre de déclaration du fichier est préservé : c'est lui qui fixe l'ordre
/// des arguments dans la commande générée (§7, règle 1) et l'ordre d'affichage
/// du formulaire.
class ParamRegistry
{
public:
    ParamRegistry() = default;

    /// Analyse un document JSON. En cas d'échec, renvoie un registre vide et
    /// renseigne `error`.
    static ParamRegistry fromJson(const QByteArray& json, QString* error = nullptr);
    static ParamRegistry fromFile(const QString& path, QString* error = nullptr);

    const QVector<SectionDef>& sections() const { return m_sections; }
    const QVector<ParamDef>& params() const { return m_params; }

    const ParamDef* find(const QString& key) const;
    const SectionDef* findSection(const QString& id) const;

    bool isEmpty() const { return m_params.isEmpty(); }
    QString targetBuild() const { return m_targetBuild; }

private:
    QVector<SectionDef> m_sections;
    QVector<ParamDef> m_params;
    QHash<QString, int> m_paramIndex;
    QHash<QString, int> m_sectionIndex;
    QString m_targetBuild;
};

} // namespace core
