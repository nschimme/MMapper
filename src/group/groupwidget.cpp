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
#include "mmapper2group.h"

#include <algorithm> // For std::find_if or other algorithms if needed by iterators
#include <iterator>  // For std::distance
#include <map>
#include <set>    // For std::set
#include <vector> // For GroupVector / std::vector

#include <QAction>
#include <QDebug> // For qDebug
#include <QHeaderView>
#include <QMessageLogContext>
#include <QString>
#include <QStringList> // For QStringList
#include <QStyledItemDelegate>
#include <QtWidgets>

static constexpr const int GROUP_COLUMN_COUNT = 9;
static_assert(GROUP_COLUMN_COUNT == static_cast<int>(GroupModel::ColumnTypeEnum::ROOM_NAME) + 1,
              "# of columns");

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

// GroupProxyModel Implementation
GroupProxyModel::GroupProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{}

GroupProxyModel::~GroupProxyModel() = default;

void GroupProxyModel::refresh()
{
    invalidate(); // Triggers re-filter and re-sort
}

SharedGroupChar GroupProxyModel::getCharacterFromSource(const QModelIndex &source_index) const
{
    const GroupModel *srcModel = qobject_cast<const GroupModel *>(sourceModel());
    if (!srcModel || !source_index.isValid()) {
        return nullptr;
    }

    // Mmapper2Group* group = srcModel->getGroup(); // getGroup() removed from GroupModel
    // if (!group) {
    //     return nullptr;
    // }
    // GroupVector characters = group->selectAll();
    // The line below directly uses the GroupModel's new way of providing characters
    return srcModel->getCharacter(source_index.row());
}

bool GroupProxyModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
    Q_UNUSED(source_parent);

    const auto &groupManagerSettings = getConfig().groupManager;
    if (!groupManagerSettings.filterNPCs) {
        return true;
    }

    QModelIndex sourceIndex = sourceModel()->index(source_row, 0, source_parent);
    SharedGroupChar character = getCharacterFromSource(sourceIndex);

    if (character && character->isNpc()) {
        return false;
    }

    return true;
}

bool GroupProxyModel::lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const
{
    Q_UNUSED(source_left);
    Q_UNUSED(source_right);
    // The source model (GroupModel) now dictates the order through its internal
    // m_characters list, which is affected by drag-and-drop and the setCharacters logic.
    // Returning false tells QSortFilterProxyModel to respect the source model's order.
    return false;
}
// End of GroupProxyModel Implementation

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

GroupDelegate::GroupDelegate(QObject *parent)
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
        QStyledItemDelegate::paint(painter, option, index);
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

GroupModel::GroupModel(QObject *parent)
    : QAbstractTableModel(parent)
// m_characters is default-initialized (empty std::vector)
// m_mapLoaded is also default-initialized or can be set here if needed
{}

void GroupModel::setCharacters(const GroupVector &newGameChars)
{
    qDebug() << "[GroupModel::setCharacters] Called.";

    QStringList oldCharNames;
    for (const auto &ch : m_characters) {
        if (ch)
            oldCharNames << ch->getName().toQString();
        else
            oldCharNames << "NULL";
    }
    qDebug() << "  Old m_characters (" << m_characters.size() << "):" << oldCharNames.join(", ");

    QStringList newGameCharNames;
    for (const auto &ch : newGameChars) {
        if (ch)
            newGameCharNames << ch->getName().toQString();
        else
            newGameCharNames << "NULL";
    }
    qDebug() << "  Input newGameChars (" << newGameChars.size()
             << "):" << newGameCharNames.join(", ");

    GroupVector resultingCharacterList;
    std::vector<SharedGroupChar> trulyNewPlayers;
    std::vector<SharedGroupChar> trulyNewNpcs;
    std::vector<SharedGroupChar> allTrulyNewCharsInOriginalOrder;

    std::set<SharedGroupChar> newGameCharsSet(newGameChars.begin(), newGameChars.end());

    // 1. Preserve existing characters
    for (const auto &existingChar : m_characters) {
        if (newGameCharsSet.count(existingChar)) {
            if (existingChar) { // Ensure not null before adding
                resultingCharacterList.push_back(existingChar);
            }
        }
    }
    QStringList preservedCharNames;
    for (const auto &ch : resultingCharacterList) {
        if (ch)
            preservedCharNames << ch->getName().toQString();
        else
            preservedCharNames << "NULL";
    }
    qDebug() << "  1. Preserved characters in resultingCharacterList ("
             << resultingCharacterList.size() << "):" << preservedCharNames.join(", ");

    // 2. Identify truly new characters and categorize them
    std::set<SharedGroupChar> preservedCharsSet(resultingCharacterList.begin(),
                                                resultingCharacterList.end());
    for (const auto &gameChar : newGameChars) {
        if (!preservedCharsSet.count(gameChar)) {
            if (gameChar) { // Ensure not null before processing
                allTrulyNewCharsInOriginalOrder.push_back(gameChar);
                if (gameChar->isNpc()) {
                    trulyNewNpcs.push_back(gameChar);
                } else {
                    trulyNewPlayers.push_back(gameChar);
                }
            }
        }
    }
    QStringList newPlayerNames, newNpcNames;
    for (const auto &ch : trulyNewPlayers) {
        if (ch)
            newPlayerNames << ch->getName().toQString();
    }
    for (const auto &ch : trulyNewNpcs) {
        if (ch)
            newNpcNames << ch->getName().toQString();
    }
    qDebug() << "  2. Identified Truly New Players (" << trulyNewPlayers.size()
             << "): " << newPlayerNames.join(", ");
    qDebug() << "     Identified Truly New NPCs (" << trulyNewNpcs.size()
             << "): " << newNpcNames.join(", ");

    const auto &groupManagerSettings = getConfig().groupManager;
    qDebug() << "  sortNpcsToBottom setting is:" << groupManagerSettings.sortNpcsToBottom;

    if (groupManagerSettings.sortNpcsToBottom) {
        auto itPlayerInsertPos = resultingCharacterList.begin();
        while (itPlayerInsertPos != resultingCharacterList.end()) {
            if (*itPlayerInsertPos && (*itPlayerInsertPos)->isNpc()) {
                break;
            }
            ++itPlayerInsertPos;
        }
        qDebug() << "  3. (Sort ON) Player insert position found at index:"
                 << std::distance(resultingCharacterList.begin(), itPlayerInsertPos);
        if (!trulyNewPlayers.empty()) {
            resultingCharacterList.insert(itPlayerInsertPos,
                                          trulyNewPlayers.begin(),
                                          trulyNewPlayers.end());
        }
        if (!trulyNewNpcs.empty()) {
            resultingCharacterList.insert(resultingCharacterList.end(),
                                          trulyNewNpcs.begin(),
                                          trulyNewNpcs.end());
        }
    } else {
        qDebug() << "  3. (Sort OFF) Appending all truly new characters in original order.";
        if (!allTrulyNewCharsInOriginalOrder.empty()) {
            resultingCharacterList.insert(resultingCharacterList.end(),
                                          allTrulyNewCharsInOriginalOrder.begin(),
                                          allTrulyNewCharsInOriginalOrder.end());
        }
    }

    QStringList finalCharNames;
    for (const auto &ch : resultingCharacterList) {
        if (ch)
            finalCharNames << ch->getName().toQString();
        else
            finalCharNames << "NULL";
    }
    qDebug() << "  FINAL resultingCharacterList before model reset ("
             << resultingCharacterList.size() << "):" << finalCharNames.join(", ");

    beginResetModel();
    m_characters = resultingCharacterList;
    endResetModel();
    qDebug() << "[GroupModel::setCharacters] Finished.";
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

int GroupModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
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

    case Qt::ToolTipRole:
        switch (column) {
        case ColumnTypeEnum::HP_PERCENT:
            if (character.getType() == CharacterTypeEnum::NPC)
                break;
            return calculateRatio(character.getHits(), character.getMaxHits(), character.getType());
        case ColumnTypeEnum::MANA_PERCENT:
            if (character.getType() == CharacterTypeEnum::NPC)
                break;
            return calculateRatio(character.getMana(), character.getMaxMana(), character.getType());
        case ColumnTypeEnum::MOVES_PERCENT:
            if (character.getType() == CharacterTypeEnum::NPC)
                break;
            return calculateRatio(character.getMoves(),
                                  character.getMaxMoves(),
                                  character.getType());
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
        // dataForCharacter is an existing helper that takes a character and returns QVariant for column/role
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
    // All items are enabled, selectable, draggable, and are drop targets.
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;
}

Qt::DropActions GroupModel::supportedDropActions() const
{
    return Qt::MoveAction; // Only support moving items.
}

QStringList GroupModel::mimeTypes() const
{
    QStringList types;
    types << "application/vnd.mm_groupchar.row"; // Custom MIME type
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
    mimeDataObj->setData("application/vnd.mm_groupchar.row", encodedData);
    return mimeDataObj;
}

bool GroupModel::dropMimeData(
    const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent)
{
    if (action == Qt::IgnoreAction)
        return true;
    if (!data->hasFormat("application/vnd.mm_groupchar.row") || column > 0)
        return false;

    QByteArray encodedData = data->data("application/vnd.mm_groupchar.row");
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
    , m_group(group)
    , m_map(md)
    , m_model(this) // Changed m_model initialization
{
    if (m_group) { // Populate model after m_group is set
        m_model.setCharacters(m_group->selectAll());
    }

    auto *layout = new QVBoxLayout(this);
    layout->setAlignment(Qt::AlignTop);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_table = new QTableView(this);
    // m_table->setSelectionMode(QAbstractItemView::NoSelection); // Changed for D&D
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows); // Ensure whole rows are selected

    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

    m_proxyModel = new GroupProxyModel(this);
    m_proxyModel->setSourceModel(&m_model);
    m_table->setModel(m_proxyModel);

    // Configure m_table for drag-and-drop
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

    connect(m_table,
            &QAbstractItemView::clicked,
            this,
            [this](const QModelIndex &proxyIndex) { // Renamed index to proxyIndex
                if (!proxyIndex.isValid()) {
                    return;
                }

                // Map the proxy index to the source model index
                QModelIndex sourceIndex = m_proxyModel->mapToSource(proxyIndex);

                if (!sourceIndex.isValid()) { // Check if mapping was successful
                    return;
                }

                // Get the character from the source model (GroupModel) using the source index's row
                // m_model is the GroupModel instance in GroupWidget
                selectedCharacter = m_model.getCharacter(sourceIndex.row());

                if (selectedCharacter) { // Ensure character was found
                    m_center->setText(
                        QString("&Center on %1").arg(selectedCharacter->getName().toQString()));
                    m_recolor->setText(
                        QString("&Recolor %1").arg(selectedCharacter->getName().toQString()));
                    m_center->setDisabled(!selectedCharacter->isYou()
                                          && selectedCharacter->getServerId()
                                                 == INVALID_SERVER_ROOMID);

                    QMenu contextMenu(tr("Context menu"), this);
                    contextMenu.addAction(m_center);
                    contextMenu.addAction(m_recolor);
                    contextMenu.exec(QCursor::pos());
                }
            });

    connect(m_group, &Mmapper2Group::sig_updateWidget, this, &GroupWidget::slot_updateLabels);
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

void GroupWidget::slot_updateLabels()
{
    // m_model.resetModel(); // Replaced by setCharacters
    if (m_group) {
        m_model.setCharacters(m_group->selectAll());
    } else {
        m_model.setCharacters({}); // Clear the model if m_group is null
    }

    if (m_proxyModel) {
        m_proxyModel->refresh(); // Tell proxy to re-evaluate filtering/sorting
    }

    // Hide unnecessary columns like mana if everyone is a zorc/troll
    // This logic now needs to access characters from m_model or m_group
    const auto one_character_had_mana = [this]() -> bool {
        if (!m_group)
            return false;                                // Or iterate m_model.m_characters
        auto selection = m_group->selectAll();           // Or iterate m_model.m_characters
        for (const auto &character : selection) {        // Or iterate m_model.m_characters
            if (character && character->getMana() > 0) { // Add null check for character
                return true;
            }
        }
        return false;
    };
    const bool hide_mana = !one_character_had_mana();
    m_table->setColumnHidden(static_cast<int>(GroupModel::ColumnTypeEnum::MANA), hide_mana);
    m_table->setColumnHidden(static_cast<int>(GroupModel::ColumnTypeEnum::MANA_PERCENT), hide_mana);
}
