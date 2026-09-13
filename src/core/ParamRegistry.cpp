#include "core/ParamRegistry.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

namespace core {
namespace {

struct TypeName {
    ParamType type;
    const char* name;
};

constexpr TypeName kTypeNames[] = {
    { ParamType::Int, "int" },
    { ParamType::Float, "float" },
    { ParamType::Bool, "bool" },
    { ParamType::Tristate, "tristate" },
    { ParamType::Enum, "enum" },
    { ParamType::String, "string" },
    { ParamType::Path, "path" },
    { ParamType::IntOrKeyword, "intOrKeyword" },
};

QStringList toStringList(const QJsonValue& value)
{
    QStringList list;
    const QJsonArray array = value.toArray();
    list.reserve(array.size());
    for (const QJsonValue& item : array) {
        const QString text = item.toString();
        if (!text.isEmpty())
            list.append(text);
    }
    return list;
}

std::optional<double> toOptionalDouble(const QJsonObject& object, const QString& key)
{
    const QJsonValue value = object.value(key);
    if (!value.isDouble())
        return std::nullopt;
    return value.toDouble();
}

/// Le défaut affiché dans l'info-bulle : accepte indifféremment un nombre,
/// un booléen ou une chaîne dans params.json.
QString toDisplayString(const QJsonValue& value)
{
    if (value.isString())
        return value.toString();
    if (value.isBool())
        return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    if (value.isDouble()) {
        const double number = value.toDouble();
        if (qFuzzyCompare(number, qRound(number)))
            return QString::number(qRound(number));
        return QString::number(number, 'g', 12);
    }
    return {};
}

} // namespace

QString paramTypeToString(ParamType type)
{
    for (const TypeName& entry : kTypeNames) {
        if (entry.type == type)
            return QString::fromLatin1(entry.name);
    }
    return QStringLiteral("string");
}

std::optional<ParamType> paramTypeFromString(QStringView text)
{
    for (const TypeName& entry : kTypeNames) {
        if (text == QLatin1String(entry.name))
            return entry.type;
    }
    return std::nullopt;
}

bool ParamDef::appliesToBinary(BinaryKind kind) const
{
    // Un `appliesTo` absent signifie « les deux » : c'est le cas le plus courant
    // et l'omission ne doit pas faire disparaître silencieusement le paramètre.
    if (appliesTo.isEmpty())
        return true;
    return appliesTo.contains(binaryKindToString(kind), Qt::CaseInsensitive);
}

bool ParamDef::isFlagOnly() const
{
    return type == ParamType::Bool || type == ParamType::Tristate;
}

ParamRegistry ParamRegistry::fromJson(const QByteArray& json, QString* error)
{
    const auto fail = [error](const QString& message) {
        if (error)
            *error = message;
        return ParamRegistry{};
    };

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError)
        return fail(QStringLiteral("JSON invalide (offset %1) : %2")
                        .arg(parseError.offset)
                        .arg(parseError.errorString()));
    if (!document.isObject())
        return fail(QStringLiteral("La racine de params.json doit être un objet."));

    const QJsonObject root = document.object();
    ParamRegistry registry;
    registry.m_targetBuild = root.value(QStringLiteral("targetBuild")).toString();

    for (const QJsonValue& value : root.value(QStringLiteral("sections")).toArray()) {
        const QJsonObject object = value.toObject();
        SectionDef section;
        section.id = object.value(QStringLiteral("id")).toString();
        if (section.id.isEmpty())
            return fail(QStringLiteral("Une section est déclarée sans « id »."));
        if (registry.m_sectionIndex.contains(section.id))
            return fail(QStringLiteral("Section « %1 » déclarée deux fois.").arg(section.id));
        section.label = object.value(QStringLiteral("label")).toString(section.id);
        section.defaultExpanded = object.value(QStringLiteral("defaultExpanded")).toBool(false);
        registry.m_sectionIndex.insert(section.id, registry.m_sections.size());
        registry.m_sections.append(section);
    }

    const QJsonArray params = root.value(QStringLiteral("params")).toArray();
    if (params.isEmpty())
        return fail(QStringLiteral("params.json ne déclare aucun paramètre."));

    for (const QJsonValue& value : params) {
        const QJsonObject object = value.toObject();
        ParamDef param;
        param.key = object.value(QStringLiteral("key")).toString();
        if (param.key.isEmpty())
            return fail(QStringLiteral("Un paramètre est déclaré sans « key »."));
        if (registry.m_paramIndex.contains(param.key))
            return fail(QStringLiteral("Paramètre « %1 » déclaré deux fois.").arg(param.key));

        param.flag = object.value(QStringLiteral("flag")).toString();
        if (param.flag.isEmpty())
            return fail(QStringLiteral("Paramètre « %1 » : « flag » manquant.").arg(param.key));

        const QString typeName = object.value(QStringLiteral("type")).toString();
        const auto type = paramTypeFromString(typeName);
        if (!type)
            return fail(QStringLiteral("Paramètre « %1 » : type « %2 » inconnu.")
                            .arg(param.key, typeName));
        param.type = *type;

        param.flagOff = object.value(QStringLiteral("flagOff")).toString();
        if (param.type == ParamType::Tristate && param.flagOff.isEmpty())
            return fail(QStringLiteral("Paramètre « %1 » : un « tristate » exige « flagOff ».")
                            .arg(param.key));

        param.section = object.value(QStringLiteral("section")).toString();
        if (!param.section.isEmpty() && !registry.m_sectionIndex.contains(param.section))
            return fail(QStringLiteral("Paramètre « %1 » : section « %2 » non déclarée.")
                            .arg(param.key, param.section));

        param.label = object.value(QStringLiteral("label")).toString(param.key);
        param.tooltip = object.value(QStringLiteral("tooltip")).toString();
        param.defaultValue = toDisplayString(object.value(QStringLiteral("default")));
        param.minimum = toOptionalDouble(object, QStringLiteral("min"));
        param.maximum = toOptionalDouble(object, QStringLiteral("max"));
        param.step = toOptionalDouble(object, QStringLiteral("step"));
        param.values = toStringList(object.value(QStringLiteral("values")));
        param.keywords = toStringList(object.value(QStringLiteral("keywords")));
        param.appliesTo = toStringList(object.value(QStringLiteral("appliesTo")));

        if (param.type == ParamType::Enum && param.values.isEmpty())
            return fail(QStringLiteral("Paramètre « %1 » : un « enum » exige « values ».")
                            .arg(param.key));

        registry.m_paramIndex.insert(param.key, registry.m_params.size());
        registry.m_params.append(param);
    }

    if (error)
        error->clear();
    return registry;
}

ParamRegistry ParamRegistry::fromFile(const QString& path, QString* error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QStringLiteral("Lecture impossible de %1 : %2").arg(path, file.errorString());
        return {};
    }
    return fromJson(file.readAll(), error);
}

const ParamDef* ParamRegistry::find(const QString& key) const
{
    const auto it = m_paramIndex.constFind(key);
    return it == m_paramIndex.constEnd() ? nullptr : &m_params.at(*it);
}

const SectionDef* ParamRegistry::findSection(const QString& id) const
{
    const auto it = m_sectionIndex.constFind(id);
    return it == m_sectionIndex.constEnd() ? nullptr : &m_sections.at(*it);
}

} // namespace core
