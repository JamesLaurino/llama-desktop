#include "ui/ParamFilterModel.h"

#include "ui/ParamFormModel.h"

namespace ui {

ParamFilterModel::ParamFilterModel(QObject* parent)
    : QSortFilterProxyModel(parent)
{
    connect(this, &QAbstractItemModel::rowsInserted, this, &ParamFilterModel::countChanged);
    connect(this, &QAbstractItemModel::rowsRemoved, this, &ParamFilterModel::countChanged);
    connect(this, &QAbstractItemModel::modelReset, this, &ParamFilterModel::countChanged);
}

void ParamFilterModel::setSection(const QString& section)
{
    if (m_section == section)
        return;
    m_section = section;
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    beginFilterChange();
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
#else
    invalidateFilter();
#endif
    emit sectionChanged();
    emit countChanged();
}

bool ParamFilterModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    const QAbstractItemModel* source = sourceModel();
    if (!source)
        return false;
    const QModelIndex index = source->index(sourceRow, 0, sourceParent);
    if (!index.isValid())
        return false;
    if (!index.data(ParamFormModel::ApplicableRole).toBool())
        return false;
    return index.data(ParamFormModel::SectionRole).toString() == m_section;
}

} // namespace ui
