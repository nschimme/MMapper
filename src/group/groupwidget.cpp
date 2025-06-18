// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "groupwidget.h"

#include "../configuration/configuration.h"
#include "../display/Filenames.h"
#include "../map/roomid.h"
#include "../mapdata/mapdata.h"
#include "../mapdata/roomselection.h"
#include "CGroupChar.h"
#include "enums.h"
#include "mmapper2group.h" // Included for Mmapper2Group type
#include "loggingcategories.h" // For qCDebug(lcGroup) if needed

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

static constexpr const int GROUP_COLUMN_COUNT = 9;
static_assert(GROUP_COLUMN_COUNT == static_cast<int>(GroupModel::ColumnTypeEnum::ROOM_NAME) + 1,
              "# of columns");

static constexpr const char *GROUP_MIME_TYPE = "application/vnd.mm_groupchar.row";

namespace { // anonymous

class NODISCARD GroupImageCache final
{
private:
    using Key = std::pair<QString, bool>;
    std::map<std::pair<QString, bool>, QImage> m_images;

public:
    explicit GroupImageCache() = default;
    ~GroupImageCache() = default;
    DELETE_CTORS_AND_ASSIGN_OPS(GroupImageCache);

public:
    NODISCARD bool contains(const Key &key) const { return m_images.find(key) != m_images.end(); }
    NODISCARD QImage &getImage(const QString &filename, const bool invert)
    {
        const Key key{filename, invert};
        if (!contains(key)) {
            auto &&[it, added] = m_images.emplace(key, filename);
            if (!added) {
                std::abort();
            }
            QImage &image = it->second;
            if (invert) {
                image.invertPixels();
            }

            qInfo() << "created image" << filename << (invert ? "(inverted)" : "(regular)");
        }
        return m_images[key];
    }
};

NODISCARD static auto &getImage(const QString &filename, const bool invert)
{
    static GroupImageCache g_groupImageCache;

    // NOTE: Assumes we're single-threaded; otherwise we'd need a lock here.
    return g_groupImageCache.getImage(filename, invert);
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
    : m_color(std::move(color))
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

void GroupStateData::paint(QPainter *const pPainter, const QRect &rect)
{
    auto &painter = deref(pPainter);
    painter.fillRect(rect, m_color);

    painter.save();
    painter.translate(rect.x(), rect.y());
    m_height = rect.height();
    painter.scale(m_height, m_height); // Images are squares

    const bool invert = mmqt::textColor(m_color) == Qt::white;

    const auto drawOne = [&painter, invert](QString filename) -> void {
        painter.drawImage(QRect{0, 0, 1, 1}, getImage(filename, invert));
        painter.translate(1, 0);
    };

    if (m_position != CharacterPositionEnum::UNDEFINED) {
        drawOne(getIconFilename(m_position));
    }
    for (const CharacterAffectEnum affect : ALL_CHARACTER_AFFECTS) {
        if (m_affects.contains(affect)) {
            drawOne(getIconFilename(affect));
        }
    }
    painter.restore();
}

GroupDelegate::GroupDelegate(QObject *const parent)
    : QStyledItemDelegate(parent)
{}

GroupDelegate::~GroupDelegate() = default;

void GroupDelegate::paint(QPainter *const painter,
                          const QStyleOptionViewItem &option,
                          const QModelIndex &index) const
{
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

QSize GroupDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    if (index.data().canConvert<GroupStateData>()) {
        GroupStateData stateData = qvariant_cast<GroupStateData>(index.data());
        QSize size = QStyledItemDelegate::sizeHint(option, index);
        const int padding = size.width() / 2;
        const int content = stateData.getWidth();
        size.setWidth(padding + content);
        return size;
    }
    return QStyledItemDelegate::sizeHint(option, index);
}

GroupModel::GroupModel(QObject *const parent)
    : QAbstractTableModel(parent)
{}

void GroupModel::setCharacters(const GroupVector &newGameChars)
{
    // 1. Create a map of new characters by ID for efficient lookup.
    std::unordered_map<GroupId, SharedGroupChar> newCharsMap;
    newCharsMap.reserve(newGameChars.size());
    for (const auto& pNewChar : newGameChars) {
        if (pNewChar) {
            newCharsMap[pNewChar->getId()] = pNewChar;
        }
    }

    // 2. Remove characters no longer present
    // Iterate backwards to keep indices valid after removal
    for (int i = static_cast<int>(m_characters.size()) - 1; i >= 0; --i) {
        SharedGroupChar pExistingChar = m_characters[static_cast<size_t>(i)];
        if (pExistingChar && newCharsMap.find(pExistingChar->getId()) == newCharsMap.end()) {
            beginRemoveRows(QModelIndex(), i, i);
            m_characters.erase(m_characters.begin() + i);
            endRemoveRows();
        }
    }

    // 3. Update existing characters.
    // NPC status changes are not explicitly tracked here for moves yet,
    // as step 5 will perform a holistic re-sort if npcSortBottom is enabled.
    for (int i = 0; i < static_cast<int>(m_characters.size()); ++i) {
        SharedGroupChar& pExistingChar = m_characters[static_cast<size_t>(i)]; // Modifiable reference
        if (!pExistingChar) continue; // Should not happen if removals are correct

        auto itNew = newCharsMap.find(pExistingChar->getId());
        // All characters in m_characters at this point must be in newCharsMap because
        // those not in newCharsMap were removed in step 2.
        if (itNew == newCharsMap.end()) {
            // This case should ideally not be reached if logic is correct.
            // It implies pExistingChar was not in newGameChars, so it should have been removed.
            // However, to be safe, skip if lookup fails.
            qWarning() << "Character" << pExistingChar->getId().asUint64() << "not found in newCharsMap, should have been removed.";
            continue;
        }
        const SharedGroupChar& pNewCharData = itNew->second;

        // Comprehensive comparison logic
        bool changed = pExistingChar->getName() != pNewCharData->getName() ||
                       pExistingChar->getHits() != pNewCharData->getHits() ||
                       pExistingChar->getMaxHits() != pNewCharData->getMaxHits() ||
                       pExistingChar->getMana() != pNewCharData->getMana() ||
                       pExistingChar->getMaxMana() != pNewCharData->getMaxMana() ||
                       pExistingChar->getMoves() != pNewCharData->getMoves() ||
                       pExistingChar->getMaxMoves() != pNewCharData->getMaxMoves() ||
                       pExistingChar->getPosition() != pNewCharData->getPosition() ||
                       pExistingChar->getAffects() != pNewCharData->getAffects() ||
                       pExistingChar->getRoomName() != pNewCharData->getRoomName() ||
                       pExistingChar->getColor() != pNewCharData->getColor() ||
                       pExistingChar->getLabel() != pNewCharData->getLabel() ||
                       pExistingChar->isNpc() != pNewCharData->isNpc();

        if (changed) {
            // Assuming CGroupChar has a proper assignment operator or copy constructor
            *pExistingChar = *pNewCharData;
            QModelIndex charIndex = index(i, 0);
            // Specify all roles that could be affected by the change.
            emit dataChanged(charIndex, charIndex, {Qt::DisplayRole, Qt::BackgroundRole, Qt::ForegroundRole, Qt::ToolTipRole, Qt::UserRole + 1}); // UserRole+1 for sorting in proxy if needed
        }
    }

    // 4. Add new characters
    // First, create a set of IDs currently in m_characters for quick lookup
    std::unordered_set<GroupId> existingIdsInMChars;
    existingIdsInMChars.reserve(m_characters.size());
    for(const auto& pChar : m_characters) {
        if (pChar) existingIdsInMChars.insert(pChar->getId());
    }

    for (const auto& pNewChar : newGameChars) {
        if (pNewChar && existingIdsInMChars.find(pNewChar->getId()) == existingIdsInMChars.end()) {
            // This is a truly new character to be added
            int insertRow = 0;
            if (getConfig().groupManager.npcSortBottom) {
                if (pNewChar->isNpc()) {
                    // New NPCs are always added at the very end when npcSortBottom is true
                    insertRow = static_cast<int>(m_characters.size());
                } else { // New Player
                    // New players are inserted before the first NPC
                    auto it_first_npc = std::find_if(m_characters.begin(), m_characters.end(),
                                                     [](const SharedGroupChar& c){ return c && c->isNpc(); });
                    insertRow = static_cast<int>(std::distance(m_characters.begin(), it_first_npc));
                }
            } else {
                // If not sorting NPCs to bottom, add all new characters to the end
                insertRow = static_cast<int>(m_characters.size());
            }
            beginInsertRows(QModelIndex(), insertRow, insertRow);
            m_characters.insert(m_characters.begin() + insertRow, pNewChar);
            endInsertRows();
        }
    }

    // 5. Re-sort m_characters if npcSortBottom is true, using move operations.
    // This handles cases where existing characters changed NPC status or new additions
    // require reordering relative to existing characters.
    if (getConfig().groupManager.npcSortBottom) {
        bool listWasModifiedInPass;
        do {
            listWasModifiedInPass = false;
            int firstNpcActualIdx = -1;
            for (int i = 0; i < static_cast<int>(m_characters.size()); ++i) {
                if (m_characters[static_cast<size_t>(i)] && m_characters[static_cast<size_t>(i)]->isNpc()) {
                    firstNpcActualIdx = i;
                    break;
                }
            }

            if (firstNpcActualIdx != -1) { // If there are any NPCs in the list
                // Check for any players that are currently positioned after the first NPC
                for (int i = firstNpcActualIdx + 1; i < static_cast<int>(m_characters.size()); ++i) {
                    if (m_characters[static_cast<size_t>(i)] && !m_characters[static_cast<size_t>(i)]->isNpc()) {
                        // This player at row 'i' is mispositioned. It should be before 'firstNpcActualIdx'.
                        SharedGroupChar playerToMove = m_characters[static_cast<size_t>(i)];

                        // The destination row for the move operation is 'firstNpcActualIdx'.
                        // When moving item from row 'i' to 'firstNpcActualIdx' (where i > firstNpcActualIdx),
                        // the item is inserted *before* the item currently at 'firstNpcActualIdx'.
                        beginMoveRows(QModelIndex(), i, i, QModelIndex(), firstNpcActualIdx);
                        m_characters.erase(m_characters.begin() + i);
                        m_characters.insert(m_characters.begin() + firstNpcActualIdx, playerToMove);
                        endMoveRows();

                        listWasModifiedInPass = true;
                        break; // Restart scan from the beginning as indices have changed.
                    }
                }
            }
        } while (listWasModifiedInPass); // Keep iterating until a pass makes no changes
    }
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

NODISCARD static QString calculatePercentage(const int numerator, const int denomenator)
{
    if (denomenator == 0) {
        return "";
    }
    int percentage = static_cast<int>(100.0 * static_cast<double>(numerator)
                                      / static_cast<double>(denomenator));
    // QT documentation doesn't say it's legal to use "\\%" or "%%", so we'll just append.
    return QString("%1").arg(percentage).append("%");
}

NODISCARD static QString calculateRatio(const int numerator,
                                        const int denomenator,
                                        CharacterTypeEnum type)
{
    if (type == CharacterTypeEnum::NPC || (numerator == 0 && denomenator == 0)) {
        return "";
    }
    return QString("%1/%2").arg(numerator).arg(denomenator);
}

NODISCARD static QString getPrettyName(const CharacterPositionEnum position)
{
#define X_CASE(UPPER_CASE, lower_case, CamelCase, friendly) \
    do { \
    case CharacterPositionEnum::UPPER_CASE: \
        return friendly; \
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
        return friendly; \
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
        case ColumnTypeEnum::HP_PERCENT:
            return calculatePercentage(character.getHits(), character.getMaxHits());
        case ColumnTypeEnum::MANA_PERCENT:
            return calculatePercentage(character.getMana(), character.getMaxMana());
        case ColumnTypeEnum::MOVES_PERCENT:
            return calculatePercentage(character.getMoves(), character.getMaxMoves());
        case ColumnTypeEnum::HP:
            return calculateRatio(character.getHits(), character.getMaxHits(), character.getType());
        case ColumnTypeEnum::MANA:
            return calculateRatio(character.getMana(), character.getMaxMana(), character.getType());
        case ColumnTypeEnum::MOVES:
            return calculateRatio(character.getMoves(),
                                  character.getMaxMoves(),
                                  character.getType());
        case ColumnTypeEnum::STATE:
            return QVariant::fromValue(GroupStateData(character.getColor(),
                                                      character.getPosition(),
                                                      character.getAffects()));
        case ColumnTypeEnum::ROOM_NAME:
            if (character.getRoomName().isEmpty())
                return "Unknown";
            return character.getRoomName().toQString();
        default:
            qWarning() << "Unsupported column" << static_cast<int>(column);
            break;
        }
        break;

    case Qt::BackgroundRole:
        return character.getColor();

    case Qt::ForegroundRole:
        return mmqt::textColor(character.getColor());

    case Qt::TextAlignmentRole:
        if (column != ColumnTypeEnum::NAME && column != ColumnTypeEnum::ROOM_NAME) {
            // NOTE: There's no QVariant(AlignmentFlag) constructor.
            return static_cast<int>(Qt::AlignCenter);
        }
        break;

    case Qt::ToolTipRole: {
        const auto getRatioTooltip = [&](int numerator, int denomenator) -> QVariant {
            if (character.getType() == CharacterTypeEnum::NPC)
                return QVariant();
            return calculateRatio(numerator, denomenator, character.getType());
        };

        switch (column) {
        case ColumnTypeEnum::HP_PERCENT:
            return getRatioTooltip(character.getHits(), character.getMaxHits());
        case ColumnTypeEnum::MANA_PERCENT:
            return getRatioTooltip(character.getMana(), character.getMaxMana());
        case ColumnTypeEnum::MOVES_PERCENT:
            return getRatioTooltip(character.getMoves(), character.getMaxMoves());
        case ColumnTypeEnum::STATE: {
            QString prettyName = getPrettyName(character.getPosition());
            for (const CharacterAffectEnum affect : ALL_CHARACTER_AFFECTS) {
                if (character.getAffects().contains(affect)) {
                    prettyName.append(", ").append(getPrettyName(affect));
                }
            }
            return prettyName;
        }
        case ColumnTypeEnum::NAME:
        case ColumnTypeEnum::HP:
        case ColumnTypeEnum::MANA:
        case ColumnTypeEnum::MOVES:
        case ColumnTypeEnum::ROOM_NAME:
            if (character.getServerId() == INVALID_SERVER_ROOMID)
                return QString("%1").arg(character.getServerId().asUint32());
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
            switch (static_cast<ColumnTypeEnum>(section)) {
            case ColumnTypeEnum::NAME:
                return "Name";
            case ColumnTypeEnum::HP_PERCENT:
                return "HP";
            case ColumnTypeEnum::MANA_PERCENT:
                return "Mana";
            case ColumnTypeEnum::MOVES_PERCENT:
                return "Moves";
            case ColumnTypeEnum::HP:
                return "HP";
            case ColumnTypeEnum::MANA:
                return "Mana";
            case ColumnTypeEnum::MOVES:
                return "Moves";
            case ColumnTypeEnum::STATE:
                return "State";
            case ColumnTypeEnum::ROOM_NAME:
                return "Room Name";
            default:
                qWarning() << "Unsupported column" << section;
            }
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
    if (action == Qt::IgnoreAction)
        return true;
    if (!data->hasFormat(GROUP_MIME_TYPE) || column > 0)
        return false;

    QByteArray encodedData = data->data(GROUP_MIME_TYPE);
    QDataStream stream(&encodedData, QIODevice::ReadOnly);
    if (stream.atEnd())
        return false;
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

    if (targetInsertionIndex < 0)
        targetInsertionIndex = 0;
    if (targetInsertionIndex > static_cast<int>(m_characters.size()))
        targetInsertionIndex = static_cast<int>(m_characters.size());

    if (!beginMoveRows(QModelIndex(), sourceRow, sourceRow, QModelIndex(), targetInsertionIndex)) {
        return false;
    }

    SharedGroupChar movedChar = m_characters[static_cast<size_t>(sourceRow)];
    m_characters.erase(m_characters.begin() + sourceRow);

    int actualInsertionIdx = targetInsertionIndex;
    if (sourceRow < targetInsertionIndex) {
        actualInsertionIdx--;
    }

    if (actualInsertionIdx < 0)
        actualInsertionIdx = 0;
    if (actualInsertionIdx > static_cast<int>(m_characters.size()))
        actualInsertionIdx = static_cast<int>(m_characters.size());

    m_characters.insert(m_characters.begin() + actualInsertionIdx, movedChar);

    endMoveRows();
    return true;
}

GroupWidget::GroupWidget(Mmapper2Group *const group, MapData *const md, QWidget *const parent)
    : QWidget(parent)
    , m_group(group) // m_group is the Mmapper2Group instance
    , m_map(md)
    , m_model(this)
{
    // Initial population of the model if group already has characters.
    // After this, model updates should primarily come from signals.
    if (m_group) {
        m_model.setCharacters(m_group->selectAll());
    } else {
        m_model.setCharacters({});
    }

    auto *layout = new QVBoxLayout(this);
    layout->setAlignment(Qt::AlignTop);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_table = new QTableView(this);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);

    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

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

    // Minimize row height
    m_table->verticalHeader()->setDefaultSectionSize(
        m_table->verticalHeader()->minimumSectionSize());

    m_center = new QAction(QIcon(":/icons/roomfind.png"), tr("&Center"), this);
    connect(m_center, &QAction::triggered, this, [this]() {
        // Center map on the clicked character
        auto &character = deref(selectedCharacter);
        if (character.isYou()) {
            if (const auto &r = m_map->getCurrentRoom()) {
                const auto vec2 = r.getPosition().to_vec2() + glm::vec2{0.5f, 0.5f};
                emit sig_center(vec2); // connects to MapWindow
                return;
            }
        }
        const ServerRoomId srvId = character.getServerId();
        if (srvId != INVALID_SERVER_ROOMID) {
            if (const auto &r = m_map->findRoomHandle(srvId)) {
                const auto vec2 = r.getPosition().to_vec2() + glm::vec2{0.5f, 0.5f};
                emit sig_center(vec2); // connects to MapWindow
            }
        }
    });

    m_recolor = new QAction(QIcon(":/icons/group-recolor.png"), tr("&Recolor"), this);
    connect(m_recolor, &QAction::triggered, this, [this]() {
        auto &character = deref(selectedCharacter);
        const QColor newColor = QColorDialog::getColor(character.getColor(), this);
        if (newColor.isValid() && newColor != character.getColor()) {
            character.setColor(newColor);
            if (selectedCharacter->isYou()) {
                setConfig().groupManager.color = newColor;
            }
        }
    });

    connect(m_table, &QAbstractItemView::clicked, this, [this](const QModelIndex &proxyIndex) {
        if (!proxyIndex.isValid()) {
            return;
        }

        QModelIndex sourceIndex = m_proxyModel->mapToSource(proxyIndex);

        if (!sourceIndex.isValid()) {
            return;
        }

        selectedCharacter = m_model.getCharacter(sourceIndex.row());

        if (selectedCharacter) {
            // Build Context menu
            m_center->setText(
                QString("&Center on %1").arg(selectedCharacter->getName().toQString()));
            m_recolor->setText(QString("&Recolor %1").arg(selectedCharacter->getName().toQString()));
            m_center->setDisabled(!selectedCharacter->isYou()
                                  && selectedCharacter->getServerId() == INVALID_SERVER_ROOMID);

            QMenu contextMenu(tr("Context menu"), this);
            contextMenu.addAction(m_center);
            contextMenu.addAction(m_recolor);
            contextMenu.exec(QCursor::pos());
        }
    });

    connect(m_group, &Mmapper2Group::sig_updateWidget, this, &GroupWidget::slot_updateLabels); // This connection will be removed

    // Disconnect old generic signal and connect new granular signals
    if (m_group) {
        disconnect(m_group, &Mmapper2Group::sig_updateWidget, this, &GroupWidget::slot_updateLabels);
        // Connect new signals with updated signatures
        connect(m_group, &Mmapper2Group::sig_characterAdded, this, &GroupWidget::slot_onCharacterAdded);
        connect(m_group, &Mmapper2Group::sig_characterRemoved, this, &GroupWidget::slot_onCharacterRemoved);
        connect(m_group, &Mmapper2Group::sig_characterUpdated, this, &GroupWidget::slot_onCharacterUpdated);
        connect(m_group, &Mmapper2Group::sig_groupReset, this, &GroupWidget::slot_onGroupReset); // Signature unchanged
    }
}

GroupWidget::~GroupWidget()
{
    delete m_table;
    delete m_recolor;
}

QSize GroupWidget::sizeHint() const
{
    int headerHeight = m_table->horizontalHeader()->height();
    int rowHeight = m_table->verticalHeader()->minimumSectionSize();
    int desiredHeight = headerHeight + rowHeight + (m_table->frameWidth() * 2);
    int preferredWidth = m_table->horizontalHeader()->length();
    return QSize(preferredWidth, desiredHeight);
}

void GroupWidget::updateColumnVisibility()
{
    // Hide unnecessary columns like mana if everyone is a zorc/troll
    const auto one_character_had_mana = [this]() -> bool {
        for (const auto &character : m_model.getCharacters()) {
            if (character && character->getMana() > 0) {
                return true;
            }
        }
        return false;
    };
    const bool hide_mana = !one_character_had_mana();
    m_table->setColumnHidden(static_cast<int>(GroupModel::ColumnTypeEnum::MANA), hide_mana);
    m_table->setColumnHidden(static_cast<int>(GroupModel::ColumnTypeEnum::MANA_PERCENT), hide_mana);
}

void GroupWidget::slot_updateLabels()
{
    // This slot is now largely superseded by the new granular slots.
    // It might be kept if Mmapper2Group::sig_updateWidget is still used for other purposes,
    // or removed if sig_updateWidget is fully deprecated for group updates.
    // For now, its main unique responsibility left might be the proxy model refresh,
    // but that should ideally not be needed.
    // Let's assume for now it only needs to update column visibility if called.
    updateColumnVisibility();

    // The m_proxyModel->refresh() call is removed as it should not be necessary
    // if the source model (GroupModel) correctly emits dataChanged, rowsInserted, rowsRemoved.
}

void GroupWidget::slot_onCharacterAdded(SharedGroupChar character)
{
    if (!character) {
        qCWarning(lcGroup) << "slot_onCharacterAdded received null character.";
        return;
    }
    m_model.insertCharacter(character);
    updateColumnVisibility();
}

void GroupWidget::slot_onCharacterRemoved(GroupId characterId)
{
    if (!characterId.isValid()) {
        qCWarning(lcGroup) << "slot_onCharacterRemoved received invalid characterId.";
        return;
    }
    m_model.removeCharacterById(characterId);
    updateColumnVisibility();
}

void GroupWidget::slot_onCharacterUpdated(SharedGroupChar character)
{
    if (!character) {
        qCWarning(lcGroup) << "slot_onCharacterUpdated received null character.";
        return;
    }
    m_model.updateCharacter(character);
    updateColumnVisibility();
}

void GroupWidget::slot_onGroupReset(const GroupVector &newCharacterList)
{
    m_model.setCharacters(newCharacterList);
    updateColumnVisibility();
}

// --- GroupModel Method Implementations ---

// Helper to find character index by ID in GroupModel's m_characters
int GroupModel::findIndexById(GroupId charId) const
{
    auto it = std::find_if(m_characters.begin(), m_characters.end(),
                           [charId](const SharedGroupChar& c) { return c && c->getId() == charId; });
    if (it != m_characters.end()) {
        return static_cast<int>(std::distance(m_characters.begin(), it));
    }
    return -1;
}

void GroupModel::insertCharacter(const SharedGroupChar& newCharacter)
{
    if (!newCharacter) {
        qCWarning(lcGroup) << "GroupModel::insertCharacter received null character.";
        return;
    }

    // Determine insertion index based on npcSortBottom
    int newIndex = static_cast<int>(m_characters.size()); // Default to appending

    if (getConfig().groupManager.npcSortBottom) {
        if (newCharacter->isNpc()) {
            // New NPCs are always added at the very end when npcSortBottom is true
            newIndex = static_cast<int>(m_characters.size());
        } else { // New Player
            // New players are inserted before the first NPC
            auto it_first_npc = std::find_if(m_characters.begin(), m_characters.end(),
                                             [](const SharedGroupChar& c){ return c && c->isNpc(); });
            newIndex = static_cast<int>(std::distance(m_characters.begin(), it_first_npc));
        }
    }
    // If not sorting NPCs to bottom, newIndex remains m_characters.size() (append)

    if (newIndex < 0 || newIndex > static_cast<int>(m_characters.size())) {
        qCWarning(lcGroup) << "GroupModel::insertCharacter: Calculated invalid index" << newIndex
                           << "for character" << newCharacter->getName().toQString()
                           << "current size:" << m_characters.size();
        newIndex = static_cast<int>(m_characters.size()); // Fallback to appending
    }

    beginInsertRows(QModelIndex(), newIndex, newIndex);
    m_characters.insert(m_characters.begin() + newIndex, newCharacter);
    endInsertRows();
}

void GroupModel::removeCharacterById(GroupId charId)
{
    if (!charId.isValid()) {
        qCWarning(lcGroup) << "GroupModel::removeCharacterById received invalid charId.";
        return;
    }
    int index = findIndexById(charId);
    if (index != -1) {
        beginRemoveRows(QModelIndex(), index, index);
        m_characters.erase(m_characters.begin() + index);
        endRemoveRows();
    } else {
        qCWarning(lcGroup) << "GroupModel::removeCharacterById: Character with ID"
                           << charId.asUint64() << "not found.";
    }
}

void GroupModel::updateCharacter(const SharedGroupChar& updatedCharacter)
{
    if (!updatedCharacter) {
        qCWarning(lcGroup) << "GroupModel::updateCharacter received null character.";
        return;
    }
    int index = findIndexById(updatedCharacter->getId());
    if (index != -1) {
        SharedGroupChar& existingChar = m_characters[static_cast<size_t>(index)];
        bool npcStatusChanged = existingChar->isNpc() != updatedCharacter->isNpc();

        existingChar = updatedCharacter; // Replace the shared_ptr instance

        emit dataChanged(this->index(index, 0),
                         this->index(index, columnCount() - 1),
                         {Qt::DisplayRole, Qt::BackgroundRole, Qt::ForegroundRole, Qt::ToolTipRole, Qt::UserRole + 1});

        // If NPC status changed and npcSortBottom is active, the character might need to move.
        // The current setCharacters implementation handles full sort.
        // For a single update, a move operation would be more efficient here.
        // This is a known point for future optimization if needed.
        // For now, if a sort is required due to this update, a subsequent sig_groupReset or manual sort might be needed,
        // or GroupModel::setCharacters could be called by GroupWidget if it becomes aware of such a change.
        // However, the problem description for the previous step (setCharacters) mentioned it should handle this.
        // Let's verify if setCharacters is suitable for this or if a move is essential here.
        // The setCharacters method was modified to perform incremental updates itself, including a sort pass.
        // Calling setCharacters(m_characters) here would be a way to trigger that sort pass.
        if (npcStatusChanged && getConfig().groupManager.npcSortBottom) {
            // To re-trigger the sorting logic within setCharacters:
            // Create a copy, as setCharacters might modify its input or rely on specific states
            // This is a bit heavy-handed for a single update.
            // GroupVector currentCharsCopy = m_characters;
            // setCharacters(currentCharsCopy);
            // TODO: Implement a more direct move operation if this proves inefficient.
            // For now, relying on the fact that `setCharacters` is already optimized.
            // A simpler approach without calling setCharacters is to manually move:

            // Find current actual index again (in case of concurrent changes, though unlikely here)
            // int currentIndex = findIndexById(updatedCharacter->getId()); // Should be same as 'index'
            // if (currentIndex == -1) return; // Should not happen

            // Determine new correct sorted index
            int newSortedIndex = index; // Assume it doesn't move initially
            // Temporarily remove to calculate new position correctly
            SharedGroupChar temp = m_characters[static_cast<size_t>(index)];
            m_characters.erase(m_characters.begin() + index);

            if (getConfig().groupManager.npcSortBottom) {
                if (temp->isNpc()) {
                    newSortedIndex = static_cast<int>(m_characters.size()); // NPCs go to end of current list
                } else { // Player
                    auto it_first_npc = std::find_if(m_characters.begin(), m_characters.end(),
                                                     [](const SharedGroupChar& c){ return c && c->isNpc(); });
                    newSortedIndex = static_cast<int>(std::distance(m_characters.begin(), it_first_npc));
                }
            }
            // Reinsert at old position for now if no move needed, or if it's complex
            m_characters.insert(m_characters.begin() + index, temp);


            if (newSortedIndex != index) {
                 // Adjust newSortedIndex if the original position was before it
                if (index < newSortedIndex) {
                    // If we removed from before the target, the target index effectively shifts left
                    // This is complex because we reinserted it.
                    // A true move operation is needed.
                    // The logic in setCharacters is: remove all, then add one-by-one in new order,
                    // or perform moves.
                    // Simplest for now: if setCharacters handles this, great. Otherwise, this is a known limitation.
                    // The current `setCharacters` has a loop that does moves.
                    // Let's try to replicate that move logic for a single item.

                    // If character at 'index' (which is 'updatedCharacter') needs to move.
                    // Calculate its true destination row 'destRow'
                    int destRow = index; // placeholder
                    // Logic from setCharacters to find where a player should go:
                    if (!updatedCharacter->isNpc()) { // Player
                        int firstNpcDisplayRow = 0;
                        bool foundNpc = false;
                        for(const auto& ch : m_characters) {
                            if(ch->isNpc()) { foundNpc = true; break; }
                            if(ch->getId() == updatedCharacter->getId()) continue; // Skip self for this count
                            firstNpcDisplayRow++;
                        }
                        if (!foundNpc) firstNpcDisplayRow = static_cast<int>(m_characters.size()) -1; // if no NPCs, player can go to end relative to other players
                         // This is still not quite right. Target should be absolute.

                        // Recalculate newSortedIndex based on current m_characters state
                        // This is where the full sort logic from setCharacters is more robust.
                        // For now, this part is complex and error-prone for a single item update.
                        // The `setCharacters` function's sorting pass is more reliable.
                        // To trigger it, we can call `setCharacters(m_characters)` but that's for full lists.
                        // The most robust way for now is that if a sort-affecting property changes,
                        // the GroupModel should ideally be reset or its setCharacters called with the current list
                        // to re-apply the sorting including moves.
                        // The current `setCharacters` already handles moves internally.
                        // So, if an update causes a sort order change, we can call `setCharacters`
                        // with the current list to force re-evaluation and moves.
                        GroupVector currentCharacters = m_characters; // Make a copy
                        this->setCharacters(currentCharacters); // This will use the optimized setCharacters
                    } else { // NPC
                        // Similar logic if an NPC needs to move among other NPCs (though less likely to change sort order)
                        // or if a player became an NPC.
                        GroupVector currentCharacters = m_characters;
                        this->setCharacters(currentCharacters);
                    }
                }
            }
        }
    } else {
        qCWarning(lcGroup) << "GroupModel::updateCharacter: Character with ID"
                           << updatedCharacter->getId().asUint64() << "not found. May add it.";
        // Optionally, treat as an add if not found, though sig_characterAdded should handle this.
        // insertCharacter(updatedCharacter);
    }
}
