// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "groupwidget.h"
#include "../configuration/configuration.h"
#include "../display/Filenames.h"
#include "../global/Timer.h"
#include "../map/roomid.h"
#include "../mapdata/mapdata.h"
#include "../mapdata/roomselection.h"
#include "CGroupChar.h"
#include "enums.h"
#include "mmapper2group.h"
#include <map>
#include <vector>
#include <QAction>
#include <QColorDialog>
#include <QDebug>
#include <QHeaderView>
#include <QMenu>
#include <QMessageLogContext>
#include <QPainter>
#include <QString>
#include <QStringList>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QVBoxLayout>
#include <QMimeData>
#include <QDataStream>

static constexpr const char *GROUP_MIME_TYPE = "application/vnd.mm_groupchar.row";

namespace { // anonymous
class NODISCARD GroupImageCache final {
private:
    using Key = std::pair<QString, bool>;
    std::map<std::pair<QString, bool>, QImage> m_images;
public:
    explicit GroupImageCache() = default;
    NODISCARD bool contains(const Key &key) const { return m_images.find(key) != m_images.end(); }
    NODISCARD QImage &getImage(const QString &filename, const bool invert) {
        const Key key{filename, invert};
        if (!contains(key)) {
            auto &&[it, added] = m_images.emplace(key, filename);
            QImage &image = it->second;
            if (invert) image.invertPixels();
        }
        return m_images[key];
    }
};

NODISCARD static auto &getImage(const QString &filename, const bool invert) {
    static GroupImageCache g_groupImageCache;
    return g_groupImageCache.getImage(filename, invert);
}

NODISCARD static const char *getColumnFriendlyName(const ColumnTypeEnum column) {
#define X_DECL_COLUMNTYPE(UPPER_CASE, lower_case, CamelCase, friendly) \
    case ColumnTypeEnum::UPPER_CASE: return friendly;
    switch (column) {
        XFOREACH_COLUMNTYPE(X_DECL_COLUMNTYPE)
    default: qWarning() << "Unsupported column type" << static_cast<int>(column); return "";
    }
#undef X_DECL_COLUMNTYPE
}
} // namespace

GroupProxyModel::GroupProxyModel(QObject *const parent) : QSortFilterProxyModel(parent) {}
GroupProxyModel::~GroupProxyModel() = default;
void GroupProxyModel::refresh() { invalidate(); }
SharedGroupChar GroupProxyModel::getCharacterFromSource(const QModelIndex &source_index) const {
    const GroupModel *srcModel = qobject_cast<const GroupModel *>(sourceModel());
    if (!srcModel || !source_index.isValid()) return nullptr;
    return srcModel->getCharacter(source_index.row());
}
bool GroupProxyModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const {
    if (!getConfig().groupManager.npcHide) return true;
    QModelIndex sourceIndex = sourceModel()->index(source_row, 0, source_parent);
    SharedGroupChar character = getCharacterFromSource(sourceIndex);
    return !(character && character->isNpc());
}

GroupStateData::GroupStateData(const QColor &color, const CharacterPositionEnum position, const CharacterAffectFlags affects)
    : m_color(color), m_position(position), m_affects(affects) {
    if (position != CharacterPositionEnum::UNDEFINED) ++m_count;
    for (const CharacterAffectEnum affect : ALL_CHARACTER_AFFECTS) if (affects.contains(affect)) ++m_count;
    if (!affects.contains(CharacterAffectEnum::WAITING)) ++m_count;
}
void GroupStateData::paint(QPainter *const pPainter, const QRect &rect) {
    auto &painter = *pPainter;
    painter.fillRect(rect, m_color);
    painter.save();
    painter.translate(rect.x(), rect.y());
    m_height = rect.height();
    painter.scale(m_height, m_height);
    const bool invert = mmqt::textColor(m_color) == Qt::white;
    const auto drawOne = [&painter, invert](QString filename) {
        painter.drawImage(QRect{0, 0, 1, 1}, getImage(filename, invert));
        painter.translate(1, 0);
    };
    if (m_position != CharacterPositionEnum::UNDEFINED) drawOne(getIconFilename(m_position));
    for (const CharacterAffectEnum affect : ALL_CHARACTER_AFFECTS) if (m_affects.contains(affect)) drawOne(getIconFilename(affect));
    painter.restore();
}

GroupDelegate::GroupDelegate(QObject *const parent) : QStyledItemDelegate(parent) {}
GroupDelegate::~GroupDelegate() = default;
void GroupDelegate::paint(QPainter *const painter, const QStyleOptionViewItem &option, const QModelIndex &index) const {
    if (index.data().canConvert<GroupStateData>()) {
        GroupStateData stateData = qvariant_cast<GroupStateData>(index.data());
        stateData.paint(painter, option.rect);
    } else {
        QStyleOptionViewItem opt = option;
        opt.state &= ~QStyle::State_HasFocus;
        opt.state &= ~QStyle::State_Selected;
        QStyledItemDelegate::paint(painter, opt, index);
    }
}
QSize GroupDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const {
    if (index.data().canConvert<GroupStateData>()) {
        GroupStateData stateData = qvariant_cast<GroupStateData>(index.data());
        QSize size = QStyledItemDelegate::sizeHint(option, index);
        size.setWidth(size.width() / 2 + stateData.getWidth());
        return size;
    }
    return QStyledItemDelegate::sizeHint(option, index);
}

GroupModel::GroupModel(QObject *const parent) : QAbstractTableModel(parent) {}

void GroupModel::setCharacters(const GroupVector &newGameChars) {
    DECL_TIMER(t, __FUNCTION__);
    std::unordered_set<GroupId> newGameCharIds;
    for (const auto &pGameChar : newGameChars) newGameCharIds.insert(pGameChar->getId());

    GroupVector resultingList;
    GroupVector trulyNewPlayers, trulyNewNpcs;
    std::unordered_set<GroupId> existingIds;

    for (const auto &pExistingChar : m_characters) {
        existingIds.insert(pExistingChar->getId());
        if (newGameCharIds.count(pExistingChar->getId())) resultingList.push_back(pExistingChar);
    }

    for (const auto &pGameChar : newGameChars) {
        if (existingIds.find(pGameChar->getId()) == existingIds.end()) {
            if (pGameChar->isNpc()) trulyNewNpcs.push_back(pGameChar);
            else trulyNewPlayers.push_back(pGameChar);
        }
    }

    if (getConfig().groupManager.npcSortBottom) {
        auto it = resultingList.begin();
        while (it != resultingList.end() && !(*it)->isNpc()) ++it;
        resultingList.insert(it, trulyNewPlayers.begin(), trulyNewPlayers.end());
        resultingList.insert(resultingList.end(), trulyNewNpcs.begin(), trulyNewNpcs.end());
    } else {
        resultingList.insert(resultingList.end(), trulyNewPlayers.begin(), trulyNewPlayers.end());
        resultingList.insert(resultingList.end(), trulyNewNpcs.begin(), trulyNewNpcs.end());
    }

    beginResetModel();
    m_characters = std::move(resultingList);
    endResetModel();
}

int GroupModel::findIndexById(const GroupId charId) const {
    if (charId == INVALID_GROUPID) return -1;
    auto it = std::find_if(m_characters.begin(), m_characters.end(), [charId](const SharedGroupChar &c) { return c && c->getId() == charId; });
    return (it != m_characters.end()) ? static_cast<int>(std::distance(m_characters.begin(), it)) : -1;
}

void GroupModel::insertCharacter(const SharedGroupChar &newCharacter) {
    assert(newCharacter);
    if (newCharacter->getId() == INVALID_GROUPID) return;
    int newIndex = static_cast<int>(m_characters.size());
    if (getConfig().groupManager.npcSortBottom && !newCharacter->isNpc()) {
        auto it = std::find_if(m_characters.begin(), m_characters.end(), [](const SharedGroupChar &c) { return c && c->isNpc(); });
        newIndex = static_cast<int>(std::distance(m_characters.begin(), it));
    }
    beginInsertRows(QModelIndex(), newIndex, newIndex);
    m_characters.insert(m_characters.begin() + newIndex, newCharacter);
    endInsertRows();
}

void GroupModel::removeCharacterById(const GroupId charId) {
    const int index = findIndexById(charId);
    if (index != -1) {
        beginRemoveRows(QModelIndex(), index, index);
        m_characters.erase(m_characters.begin() + index);
        endRemoveRows();
    }
}

void GroupModel::updateCharacter(const SharedGroupChar &updatedCharacter) {
    assert(updatedCharacter);
    const int index = findIndexById(updatedCharacter->getId());
    if (index == -1) insertCharacter(updatedCharacter);
    else {
        m_characters[static_cast<size_t>(index)] = updatedCharacter;
        emit dataChanged(this->index(index, 0), this->index(index, columnCount() - 1));
    }
}

SharedGroupChar GroupModel::getCharacter(int row) const {
    return (row >= 0 && row < static_cast<int>(m_characters.size())) ? m_characters.at(static_cast<size_t>(row)) : nullptr;
}

void GroupModel::resetModel() { beginResetModel(); endResetModel(); }
int GroupModel::rowCount(const QModelIndex &) const { return static_cast<int>(m_characters.size()); }
int GroupModel::columnCount(const QModelIndex &) const { return GROUP_COLUMN_COUNT; }

NODISCARD static QStringView getPrettyName(const CharacterPositionEnum position) {
#define X_CASE(UPPER_CASE, lower_case, CamelCase, friendly) case CharacterPositionEnum::UPPER_CASE: return QStringLiteral(friendly);
    switch (position) { XFOREACH_CHARACTER_POSITION(X_CASE) }
    return QStringView();
#undef X_CASE
}
NODISCARD static QStringView getPrettyName(const CharacterAffectEnum affect) {
#define X_CASE(UPPER_CASE, lower_case, CamelCase, friendly) case CharacterAffectEnum::UPPER_CASE: return QStringLiteral(friendly);
    switch (affect) { XFOREACH_CHARACTER_AFFECT(X_CASE) }
    return QStringView();
#undef X_CASE
}

QVariant GroupModel::dataForCharacter(const SharedGroupChar &pCharacter, const ColumnTypeEnum column, const int role) const {
    const CGroupChar &character = *pCharacter;
    const auto formatStat = [](int num, int den, ColumnTypeEnum col) -> QString {
        if (den == 0 && (col == ColumnTypeEnum::HP_PERCENT || col == ColumnTypeEnum::MANA_PERCENT || col == ColumnTypeEnum::MOVES_PERCENT)) return "";
        if (num == 0 && den == 0) return "";
        switch (col) {
            case ColumnTypeEnum::HP_PERCENT:
            case ColumnTypeEnum::MANA_PERCENT:
            case ColumnTypeEnum::MOVES_PERCENT: return QString("%1%").arg(static_cast<int>(100.0 * num / den));
            case ColumnTypeEnum::HP:
            case ColumnTypeEnum::MANA:
            case ColumnTypeEnum::MOVES: return QString("%1/%2").arg(num).arg(den);
            default: return "";
        }
    };

    switch (role) {
    case Qt::DisplayRole:
        switch (column) {
        case ColumnTypeEnum::NAME:
            if (character.getLabel().isEmpty() || character.getName().getStdStringViewUtf8() == character.getLabel().getStdStringViewUtf8()) return character.getName().toQString();
            else return QString("%1 (%2)").arg(character.getName().toQString(), character.getLabel().toQString());
        case ColumnTypeEnum::HP_PERCENT: return formatStat(character.getHits(), character.getMaxHits(), column);
        case ColumnTypeEnum::MANA_PERCENT: return formatStat(character.getMana(), character.getMaxMana(), column);
        case ColumnTypeEnum::MOVES_PERCENT: return formatStat(character.getMoves(), character.getMaxMoves(), column);
        case ColumnTypeEnum::HP: return (character.getType() == CharacterTypeEnum::NPC) ? "" : formatStat(character.getHits(), character.getMaxHits(), column);
        case ColumnTypeEnum::MANA: return (character.getType() == CharacterTypeEnum::NPC) ? "" : formatStat(character.getMana(), character.getMaxMana(), column);
        case ColumnTypeEnum::MOVES: return (character.getType() == CharacterTypeEnum::NPC) ? "" : formatStat(character.getMoves(), character.getMaxMoves(), column);
        case ColumnTypeEnum::STATE: return QVariant::fromValue(GroupStateData(character.getColor(), character.getPosition(), character.getAffects()));
        case ColumnTypeEnum::ROOM_NAME: return character.getRoomName().isEmpty() ? QStringLiteral("Somewhere") : character.getRoomName().toQString();
        }
        break;
    case Qt::BackgroundRole: return character.getColor();
    case Qt::ForegroundRole: return mmqt::textColor(character.getColor());
    case Qt::TextAlignmentRole: if (column != ColumnTypeEnum::NAME && column != ColumnTypeEnum::ROOM_NAME) return static_cast<int>(Qt::AlignCenter); break;
    case Qt::ToolTipRole:
        switch (column) {
        case ColumnTypeEnum::HP_PERCENT: return (character.getType() == CharacterTypeEnum::NPC) ? QVariant() : formatStat(character.getHits(), character.getMaxHits(), column);
        case ColumnTypeEnum::STATE: {
            QString pn = getPrettyName(character.getPosition()).toString();
            for (const CharacterAffectEnum a : ALL_CHARACTER_AFFECTS) if (character.getAffects().contains(a)) pn += ", " + getPrettyName(a).toString();
            return pn;
        }
        case ColumnTypeEnum::ROOM_NAME: if (character.getServerId() != INVALID_SERVER_ROOMID) return QString("%1").arg(character.getServerId().asUint32()); break;
        default: break;
        }
        break;
    }
    return QVariant();
}

QVariant GroupModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid()) return QVariant();
    const SharedGroupChar &character = m_characters.at(static_cast<size_t>(index.row()));
    return dataForCharacter(character, static_cast<ColumnTypeEnum>(index.column()), role);
}

QVariant GroupModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) return getColumnFriendlyName(static_cast<ColumnTypeEnum>(section));
    return QVariant();
}

Qt::ItemFlags GroupModel::flags(const QModelIndex &index) const {
    return index.isValid() ? (Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled) : Qt::NoItemFlags;
}
Qt::DropActions GroupModel::supportedDropActions() const { return Qt::MoveAction; }
QStringList GroupModel::mimeTypes() const { return {GROUP_MIME_TYPE}; }
QMimeData *GroupModel::mimeData(const QModelIndexList &indexes) const {
    QMimeData *mimeDataObj = new QMimeData();
    QByteArray encodedData;
    QDataStream stream(&encodedData, QIODevice::WriteOnly);
    if (!indexes.isEmpty()) stream << indexes.first().row();
    mimeDataObj->setData(GROUP_MIME_TYPE, encodedData);
    return mimeDataObj;
}
bool GroupModel::dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) {
    if (action == Qt::IgnoreAction) return true;
    if (!data->hasFormat(GROUP_MIME_TYPE) || column > 0) return false;
    QByteArray encodedData = data->data(GROUP_MIME_TYPE);
    QDataStream stream(&encodedData, QIODevice::ReadOnly);
    int sourceRow; stream >> sourceRow;
    int targetIdx = parent.isValid() ? parent.row() : (row != -1 ? row : static_cast<int>(m_characters.size()));
    if (sourceRow == targetIdx || (sourceRow == targetIdx - 1 && targetIdx > sourceRow)) return false;
    targetIdx = std::clamp(targetIdx, 0, static_cast<int>(m_characters.size()));
    if (!beginMoveRows(QModelIndex(), sourceRow, sourceRow, QModelIndex(), targetIdx)) return false;
    SharedGroupChar movedChar = m_characters[static_cast<size_t>(sourceRow)];
    m_characters.erase(m_characters.begin() + sourceRow);
    int actualIdx = (sourceRow < targetIdx) ? targetIdx - 1 : targetIdx;
    actualIdx = std::clamp(actualIdx, 0, static_cast<int>(m_characters.size()));
    m_characters.insert(m_characters.begin() + actualIdx, movedChar);
    endMoveRows();
    return true;
}

GroupWidget::GroupWidget(Mmapper2Group *group, MapData *md, QWidget *parent)
    : QWidget(parent), m_group(group), m_map(md), m_model(this)
{
    m_viewModel = std::make_unique<GroupWidgetViewModel>(*group, this);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    m_table = new QTableView(this);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_proxyModel = new GroupProxyModel(this);
    m_proxyModel->setSourceModel(&m_model);
    m_table->setModel(m_proxyModel);

    m_table->setDragEnabled(true);
    m_table->setAcceptDrops(true);
    m_table->setDragDropMode(QAbstractItemView::InternalMove);
    m_table->setDefaultDropAction(Qt::MoveAction);
    m_table->setDropIndicatorShown(true);

    m_table->setItemDelegate(new GroupDelegate(this));
    layout->addWidget(m_table);

    m_center = new QAction(tr("&Center"), this);
    connect(m_center, &QAction::triggered, [this]() { m_viewModel->centerOnCharacter(selectedCharacter, *m_map); });
    m_recolor = new QAction(tr("&Recolor"), this);
    connect(m_recolor, &QAction::triggered, [this]() {
        QColor newColor = QColorDialog::getColor(selectedCharacter->getColor(), this);
        m_viewModel->recolorCharacter(selectedCharacter, newColor);
    });

    connect(m_table, &QAbstractItemView::clicked, this, [this](const QModelIndex &proxyIndex) {
        if (!proxyIndex.isValid()) return;
        QModelIndex sourceIndex = m_proxyModel->mapToSource(proxyIndex);
        selectedCharacter = m_model.getCharacter(sourceIndex.row());
        if (selectedCharacter) {
            auto *menu = new QMenu(this);
            menu->setAttribute(Qt::WA_DeleteOnClose);
            menu->addAction(m_center);
            menu->addAction(m_recolor);
            menu->popup(QCursor::pos());
        }
    });

    connect(m_viewModel.get(), &GroupWidgetViewModel::sig_center, this, &GroupWidget::sig_center);
    connect(group, &Mmapper2Group::sig_characterAdded, this, &GroupWidget::slot_onCharacterAdded);
    connect(group, &Mmapper2Group::sig_characterRemoved, this, &GroupWidget::slot_onCharacterRemoved);
    connect(group, &Mmapper2Group::sig_characterUpdated, this, &GroupWidget::slot_onCharacterUpdated);
    connect(group, &Mmapper2Group::sig_groupReset, this, &GroupWidget::slot_onGroupReset);

    m_model.setCharacters(group->selectAll());
}
GroupWidget::~GroupWidget() = default;
QSize GroupWidget::sizeHint() const { return QSize(400, 200); }
void GroupWidget::updateColumnVisibility() {}
void GroupWidget::slot_onCharacterAdded(SharedGroupChar character) { m_model.insertCharacter(character); }
void GroupWidget::slot_onCharacterRemoved(const GroupId characterId) { m_model.removeCharacterById(characterId); }
void GroupWidget::slot_onCharacterUpdated(SharedGroupChar character) { m_model.updateCharacter(character); }
void GroupWidget::slot_onGroupReset(const GroupVector &newCharacterList) { m_model.setCharacters(newCharacterList); }
