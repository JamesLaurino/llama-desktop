#include "ui/ParamFormModel.h"

namespace ui {

ParamFormModel::ParamFormModel(const core::ParamRegistry* registry, QObject* parent)
    : QAbstractListModel(parent)
    , m_registry(registry)
{
}

int ParamFormModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_registry->params().size());
}

QVariant ParamFormModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount())
        return {};
    const core::ParamDef& param = m_registry->params().at(index.row());
    const QString value = m_values.value(param.key);

    switch (role) {
    case KeyRole:
        return param.key;
    case FlagRole:
        return param.flag;
    case FlagOffRole:
        return param.flagOff;
    case SectionRole:
        return param.section;
    case LabelRole:
        return param.label;
    case TooltipRole:
        return param.tooltip;
    case TypeRole:
        return core::paramTypeToString(param.type);
    case DefaultValueRole:
        return param.defaultValue;
    case MinimumRole:
        return param.minimum ? QVariant(*param.minimum) : QVariant();
    case MaximumRole:
        return param.maximum ? QVariant(*param.maximum) : QVariant();
    case StepRole:
        return param.step ? QVariant(*param.step) : QVariant();
    case ValuesRole:
        return param.values;
    case KeywordsRole:
        return param.keywords;
    case ValueRole:
        return value;
    case IsSetRole:
        return !value.isEmpty();
    case IsModifiedRole:
        return !value.isEmpty() && value != param.defaultValue;
    case ApplicableRole:
        return param.appliesToBinary(m_binary);
    default:
        return {};
    }
}

QHash<int, QByteArray> ParamFormModel::roleNames() const
{
    return {
        { KeyRole, "key" },
        { FlagRole, "flag" },
        { FlagOffRole, "flagOff" },
        { SectionRole, "section" },
        { LabelRole, "label" },
        { TooltipRole, "tooltip" },
        { TypeRole, "type" },
        { DefaultValueRole, "defaultValue" },
        { MinimumRole, "minimum" },
        { MaximumRole, "maximum" },
        { StepRole, "step" },
        { ValuesRole, "values" },
        { KeywordsRole, "keywords" },
        { ValueRole, "value" },
        { IsSetRole, "isSet" },
        { IsModifiedRole, "isModified" },
        { ApplicableRole, "applicable" },
    };
}

void ParamFormModel::setSource(core::BinaryKind binary, const QMap<QString, QString>& values)
{
    beginResetModel();
    m_binary = binary;
    m_values = values;
    endResetModel();
}

void ParamFormModel::setBinary(core::BinaryKind binary)
{
    if (m_binary == binary)
        return;
    // Réinitialisation plutôt que dataChanged : le rôle `applicable` change pour
    // toutes les lignes et les proxys doivent refiltrer. 47 lignes, coût nul.
    beginResetModel();
    m_binary = binary;
    endResetModel();
}

bool ParamFormModel::setValue(const QString& key, const QString& value)
{
    const core::ParamDef* param = m_registry->find(key);
    if (!param)
        return false;

    const QString previous = m_values.value(key);
    if (previous == value)
        return false;

    if (value.isEmpty())
        m_values.remove(key);
    else
        m_values.insert(key, value);

    emitRowChanged(static_cast<int>(param - m_registry->params().constData()));
    emit valuesChanged();
    return true;
}

bool ParamFormModel::clearSection(const QString& sectionId)
{
    bool changed = false;
    int first = -1;
    int last = -1;
    const auto& params = m_registry->params();
    for (int row = 0; row < params.size(); ++row) {
        const core::ParamDef& param = params.at(row);
        if (param.section != sectionId)
            continue;
        if (m_values.remove(param.key) > 0) {
            changed = true;
            first = (first < 0) ? row : first;
            last = row;
        }
    }
    if (!changed)
        return false;

    emit dataChanged(index(first), index(last));
    emit valuesChanged();
    return true;
}

void ParamFormModel::emitRowChanged(int row)
{
    if (row < 0 || row >= rowCount())
        return;
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx);
}

} // namespace ui
