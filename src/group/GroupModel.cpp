// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "GroupModel.h"

#include "../configuration/configuration.h"
#include "../global/Timer.h"
#include "../global/utils.h"
#include "../map/roomid.h"
#include "CGroupChar.h"
#include "enums.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <unordered_set>
#include <vector>

#include <QDataStream>
#include <QMimeData>
#include <QString>
#include <QStringList>

static constexpr const char *GROUP_MIME_TYPE = "application/vnd.mm_groupchar.row";

namespace { // anonymous

NODISCARD static const char *getColumnFriendlyName(const ColumnTypeEnum column)
{
#define X_DECL_COLUMNTYPE(UPPER_CASE, lower_case, CamelCase, friendly) \
    case ColumnTypeEnum::UPPER_CASE: \
        return friendly;

    switch (column) {
        XFOREACH_COLUMNTYPE(X_DECL_COLUMNTYPE)
    default:
        qWarning() << "Unsupported column type" << static_cast<int>(column);
        return "";
    }
#undef X_DECL_COLUMNTYPE
}
} // namespace

GroupProxyModel::GroupProxyModel(QObject *const parent)
    : QSortFilterProxyModel(parent)
{}

GroupProxyModel::~GroupProxyModel() = default;

void GroupProxyModel::refresh()
{
    // Triggers re-filter and re-sort
    invalidate();
}

SharedGroupChar GroupProxyModel::getCharacterFromSource(const QModelIndex &source_index) const
{
    const GroupModel *srcModel = qobject_cast<const GroupModel *>(sourceModel());
    if (!srcModel || !source_index.isValid()) {
        return nullptr;
    }

    return srcModel->getCharacter(source_index.row());
}

bool GroupProxyModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
    if (!getConfig().groupManager.npcHide) {
        return true;
    }

    QModelIndex sourceIndex = sourceModel()->index(source_row, 0, source_parent);
    SharedGroupChar character = getCharacterFromSource(sourceIndex);
    if (character && character->isNpc()) {
        return false;
    }

    return true;
}

GroupStateData::GroupStateData(const QColor &color,
                               const CharacterPositionEnum position,
                               const CharacterAffectFlags affects)
    : m_color(color)
    , m_position(position)
    , m_affects(affects)
{
    if (position != CharacterPositionEnum::UNDEFINED) {
        ++m_count;
    }
    // Increment imageCount for each active affect
    for (const CharacterAffectEnum affect : ALL_CHARACTER_AFFECTS) {
        if (affects.contains(affect)) {
            ++m_count;
        }
    }
    // Users spam search/reveal/flush so pad an extra position to reduce eye strain
    if (!affects.contains(CharacterAffectEnum::WAITING)) {
        ++m_count;
    }
}

GroupModel::GroupModel(QObject *const parent)
    : QAbstractTableModel(parent)
{}

void GroupModel::setCharacters(const GroupVector &newGameChars)
{
    DECL_TIMER(t, __FUNCTION__);

    std::unordered_set<GroupId> newGameCharIds;
    newGameCharIds.reserve(newGameChars.size());
    for (const auto &pGameChar : newGameChars) {
        const auto &gameChar = deref(pGameChar);
        newGameCharIds.insert(gameChar.getId());
    }

    GroupVector resultingCharacterList;
    resultingCharacterList.reserve(m_characters.size() + newGameChars.size());

    // Temporary vectors to hold truly new players, NPCs, and all truly new characters
    GroupVector trulyNewPlayers;
    trulyNewPlayers.reserve(newGameChars.size());
    GroupVector trulyNewNpcs;
    trulyNewNpcs.reserve(newGameChars.size());
    GroupVector allTrulyNewCharsInOriginalOrder;
    allTrulyNewCharsInOriginalOrder.reserve(newGameChars.size());

    // Preserve existing characters
    std::unordered_set<GroupId> existingIds;
    existingIds.reserve(m_characters.size());
    for (const auto &pExistingChar : m_characters) {
        const auto &existingChar = deref(pExistingChar);
        existingIds.insert(existingChar.getId());
        if (newGameCharIds.count(existingChar.getId())) {
            resultingCharacterList.push_back(pExistingChar);
        }
    }

    // Identify truly new characters and categorize them as NPC or player
    for (const auto &pGameChar : newGameChars) {
        const auto &gameChar = deref(pGameChar);
        if (existingIds.find(gameChar.getId()) == existingIds.end()) {
            allTrulyNewCharsInOriginalOrder.push_back(pGameChar);
            if (gameChar.isNpc()) {
                trulyNewNpcs.push_back(pGameChar);
            } else {
                trulyNewPlayers.push_back(pGameChar);
            }
        }
    }

    // Insert the newly identified characters into the resulting list based on configuration.
    if (getConfig().groupManager.npcSortBottom) {
        // Find the insertion point for new players: before the first NPC in the preserved list.
        auto itPlayerInsertPos = resultingCharacterList.begin();
        while (itPlayerInsertPos != resultingCharacterList.end()) {
            if (*itPlayerInsertPos && (*itPlayerInsertPos)->isNpc()) {
                break;
            }
            ++itPlayerInsertPos;
        }

        // Insert truly new players at the determined position.
        if (!trulyNewPlayers.empty()) {
            resultingCharacterList.insert(itPlayerInsertPos,
                                          trulyNewPlayers.begin(),
                                          trulyNewPlayers.end());
        }
        // Insert truly new NPCs at the end of the list.
        if (!trulyNewNpcs.empty()) {
            resultingCharacterList.insert(resultingCharacterList.end(),
                                          trulyNewNpcs.begin(),
                                          trulyNewNpcs.end());
        }
    } else {
        // If no special NPC sorting, just append all truly new characters in their original order.
        if (!allTrulyNewCharsInOriginalOrder.empty()) {
            resultingCharacterList.insert(resultingCharacterList.end(),
                                          allTrulyNewCharsInOriginalOrder.begin(),
                                          allTrulyNewCharsInOriginalOrder.end());
        }
    }

    beginResetModel();
    m_characters = std::move(resultingCharacterList);
    endResetModel();
}

int GroupModel::findIndexById(const GroupId charId) const
{
    if (charId == INVALID_GROUPID) {
        return -1;
    }
    auto it = std::find_if(m_characters.begin(),
                           m_characters.end(),
                           [charId](const SharedGroupChar &c) { return c && c->getId() == charId; });
    if (it != m_characters.end()) {
        return static_cast<int>(std::distance(m_characters.begin(), it));
    }
    return -1;
}

void GroupModel::insertCharacter(const SharedGroupChar &newCharacter)
{
    assert(newCharacter);
    if (newCharacter->getId() == INVALID_GROUPID) {
        return;
    }

    auto calculateInsertIndex = [this, &newCharacter]() -> int {
        if (getConfig().groupManager.npcSortBottom && !newCharacter->isNpc()) {
            auto it = std::find_if(m_characters.begin(),
                                   m_characters.end(),
                                   [](const SharedGroupChar &c) { return c && c->isNpc(); });
            return static_cast<int>(std::distance(m_characters.begin(), it));
        }
        return static_cast<int>(m_characters.size());
    };

    const int newIndex = calculateInsertIndex();
    assert(newIndex >= 0 && newIndex <= static_cast<int>(m_characters.size()));

    beginInsertRows(QModelIndex(), newIndex, newIndex);
    m_characters.insert(m_characters.begin() + newIndex, newCharacter);
    endInsertRows();
}

void GroupModel::removeCharacterById(const GroupId charId)
{
    const int index = findIndexById(charId);
    if (index == -1) {
        return;
    }

    beginRemoveRows(QModelIndex(), index, index);
    m_characters.erase(m_characters.begin() + index);
    endRemoveRows();
}

void GroupModel::updateCharacter(const SharedGroupChar &updatedCharacter)
{
    assert(updatedCharacter);
    const GroupId charId = updatedCharacter->getId();
    const int index = findIndexById(charId);
    if (index == -1) {
        insertCharacter(updatedCharacter);
        return;
    }

    SharedGroupChar &existingChar = m_characters[static_cast<size_t>(index)];
    existingChar = updatedCharacter;

    emit dataChanged(this->index(index, 0),
                     this->index(index, columnCount(QModelIndex()) - 1),
                     {Qt::DisplayRole,
                      Qt::BackgroundRole,
                      Qt::ForegroundRole,
                      Qt::ToolTipRole,
                      Qt::UserRole + 1});
}

SharedGroupChar GroupModel::getCharacter(int row) const
{
    if (row >= 0 && row < static_cast<int>(m_characters.size())) {
        return m_characters.at(static_cast<size_t>(row));
    }
    return nullptr;
}

void GroupModel::resetModel()
{
    beginResetModel();
    endResetModel();
}

int GroupModel::rowCount(const QModelIndex & /* parent */) const
{
    return static_cast<int>(m_characters.size());
}

int GroupModel::columnCount(const QModelIndex & /* parent */) const
{
    return GROUP_COLUMN_COUNT;
}

NODISCARD static QString getPrettyName(const CharacterPositionEnum position)
{
#define X_CASE(UPPER_CASE, lower_case, CamelCase, friendly) \
    do { \
    case CharacterPositionEnum::UPPER_CASE: \
        return QStringLiteral(friendly); \
    } while (false);
    switch (position) {
        XFOREACH_CHARACTER_POSITION(X_CASE)
    }
    return QString::asprintf("(CharacterPositionEnum)%d", static_cast<int>(position));
#undef X_CASE
}
NODISCARD static QString getPrettyName(const CharacterAffectEnum affect)
{
#define X_CASE(UPPER_CASE, lower_case, CamelCase, friendly) \
    do { \
    case CharacterAffectEnum::UPPER_CASE: \
        return QStringLiteral(friendly); \
    } while (false);
    switch (affect) {
        XFOREACH_CHARACTER_AFFECT(X_CASE)
    }
    return QString::asprintf("(CharacterAffectEnum)%d", static_cast<int>(affect));
#undef X_CASE
}

QVariant GroupModel::dataForCharacter(const SharedGroupChar &pCharacter,
                                      const ColumnTypeEnum column,
                                      const int role) const
{
    const CGroupChar &character = deref(pCharacter);

    const auto formatStat = [&character](int numerator, int denomenator) -> QString {
        if (numerator == 0 && denomenator == 0) {
            return QLatin1String("");
        }

        if (character.getType() == CharacterTypeEnum::NPC) {
            const double pct = denomenator > 0 ? static_cast<double>(numerator) / denomenator : 0.0;
            return QString("%1%").arg(static_cast<int>(std::round(pct * 100.0)));
        }

        return QString("%1 / %2").arg(numerator).arg(denomenator);
    };

    // Map column to data
    switch (role) {
    case Qt::DisplayRole:
        switch (column) {
        case ColumnTypeEnum::NAME:
            if (character.getLabel().isEmpty()
                || character.getName().getStdStringViewUtf8()
                       == character.getLabel().getStdStringViewUtf8()) {
                return character.getName().toQString();
            } else {
                return QString("%1 (%2)").arg(character.getName().toQString(),
                                              character.getLabel().toQString());
            }
        case ColumnTypeEnum::HP:
            return formatStat(character.getHits(), character.getMaxHits());
        case ColumnTypeEnum::MANA:
            return formatStat(character.getMana(), character.getMaxMana());
        case ColumnTypeEnum::MOVES:
            return formatStat(character.getMoves(), character.getMaxMoves());
        case ColumnTypeEnum::STATE:
            return QVariant::fromValue(GroupStateData(character.getColor(),
                                                      character.getPosition(),
                                                      character.getAffects()));
        case ColumnTypeEnum::ROOM_NAME:
            if (character.getRoomName().isEmpty()) {
                return QStringLiteral("Somewhere");
            } else {
                return character.getRoomName().toQString();
            }
        default:
            qWarning() << "Unsupported column" << static_cast<int>(column);
            break;
        }
        break;

    case Qt::BackgroundRole:
        return QVariant();

    case Qt::ForegroundRole:
        return QVariant();

    case Qt::TextAlignmentRole:
        if (column != ColumnTypeEnum::NAME && column != ColumnTypeEnum::ROOM_NAME) {
            // NOTE: There's no QVariant(AlignmentFlag) constructor.
            return static_cast<int>(Qt::AlignCenter);
        }
        break;

    case Qt::ToolTipRole: {
        switch (column) {
        case ColumnTypeEnum::HP:
        case ColumnTypeEnum::MANA:
        case ColumnTypeEnum::MOVES: {
            const int cur = (column == ColumnTypeEnum::HP)     ? character.getHits()
                            : (column == ColumnTypeEnum::MANA) ? character.getMana()
                                                               : character.getMoves();
            const int max = (column == ColumnTypeEnum::HP)     ? character.getMaxHits()
                            : (column == ColumnTypeEnum::MANA) ? character.getMaxMana()
                                                               : character.getMaxMoves();
            if (max > 0) {
                const double pct = static_cast<double>(cur) / max;
                return QString("%1%").arg(static_cast<int>(std::round(pct * 100.0)));
            }
            return QVariant();
        }
        case ColumnTypeEnum::STATE: {
            QString prettyName;
            prettyName += getPrettyName(character.getPosition());
            for (const CharacterAffectEnum affect : ALL_CHARACTER_AFFECTS) {
                if (character.getAffects().contains(affect)) {
                    prettyName.append(QStringLiteral(", ")).append(getPrettyName(affect));
                }
            }
            return prettyName;
        }
        case ColumnTypeEnum::NAME:
            break;
        case ColumnTypeEnum::ROOM_NAME:
            if (character.getServerId() != INVALID_SERVER_ROOMID) {
                return QString("%1").arg(character.getServerId().asUint32());
            }
            break;
        }
        break;
    }

    default:
        break;
    }

    return QVariant();
}

QVariant GroupModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    if (index.row() >= 0 && index.row() < static_cast<int>(m_characters.size())) {
        const SharedGroupChar &character = m_characters.at(static_cast<size_t>(index.row()));
        return dataForCharacter(character, static_cast<ColumnTypeEnum>(index.column()), role);
    }

    return QVariant();
}

QVariant GroupModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    switch (role) {
    case Qt::DisplayRole:
        if (orientation == Qt::Orientation::Horizontal) {
            return getColumnFriendlyName(static_cast<ColumnTypeEnum>(section));
        }
        break;
    }
    return QVariant();
}

Qt::ItemFlags GroupModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;
}

Qt::DropActions GroupModel::supportedDropActions() const
{
    return Qt::MoveAction;
}

QStringList GroupModel::mimeTypes() const
{
    QStringList types;
    types << GROUP_MIME_TYPE;
    return types;
}

QMimeData *GroupModel::mimeData(const QModelIndexList &indexes) const
{
    QMimeData *mimeDataObj = new QMimeData();
    QByteArray encodedData;
    QDataStream stream(&encodedData, QIODevice::WriteOnly);
    if (!indexes.isEmpty() && indexes.first().isValid()) {
        stream << indexes.first().row();
    }
    mimeDataObj->setData(GROUP_MIME_TYPE, encodedData);
    return mimeDataObj;
}

bool GroupModel::dropMimeData(
    const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent)
{
    if (action == Qt::IgnoreAction) {
        return true;
    }
    if (!data->hasFormat(GROUP_MIME_TYPE) || column > 0) {
        return false;
    }

    QByteArray encodedData = data->data(GROUP_MIME_TYPE);
    QDataStream stream(&encodedData, QIODevice::ReadOnly);
    if (stream.atEnd()) {
        return false;
    }
    int sourceRow;
    stream >> sourceRow;

    int targetInsertionIndex;
    if (parent.isValid()) {
        targetInsertionIndex = parent.row();
    } else if (row != -1) {
        targetInsertionIndex = row;
    } else {
        targetInsertionIndex = static_cast<int>(m_characters.size());
    }

    if (sourceRow == targetInsertionIndex
        || (sourceRow == targetInsertionIndex - 1 && targetInsertionIndex > sourceRow)) {
        return false;
    }

    if (targetInsertionIndex < 0) {
        targetInsertionIndex = 0;
    }
    if (targetInsertionIndex > static_cast<int>(m_characters.size())) {
        targetInsertionIndex = static_cast<int>(m_characters.size());
    }

    if (!beginMoveRows(QModelIndex(), sourceRow, sourceRow, QModelIndex(), targetInsertionIndex)) {
        return false;
    }

    SharedGroupChar movedChar = m_characters[static_cast<size_t>(sourceRow)];
    m_characters.erase(m_characters.begin() + sourceRow);

    int actualInsertionIdx = targetInsertionIndex;
    if (sourceRow < targetInsertionIndex) {
        actualInsertionIdx--;
    }

    if (actualInsertionIdx < 0) {
        actualInsertionIdx = 0;
    }
    if (actualInsertionIdx > static_cast<int>(m_characters.size())) {
        actualInsertionIdx = static_cast<int>(m_characters.size());
    }

    m_characters.insert(m_characters.begin() + actualInsertionIdx, movedChar);

    endMoveRows();
    return true;
}
