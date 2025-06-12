#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "CGroupChar.h"
#include "mmapper2character.h"

#include <QAbstractTableModel>
#include <QString>
#include <QStyledItemDelegate>
#include <QWidget>
#include <QtCore>
#include <QSortFilterProxyModel> // Added include

class QAction;
class MapData;
// CGroupChar.h is already included at the top
class Mmapper2Group;
class QObject;
class QTableView;
// GroupProxyModel will be defined here directly

// Declaration of GroupProxyModel
class NODISCARD_QOBJECT GroupProxyModel final : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit GroupProxyModel(QObject *parent = nullptr);
    ~GroupProxyModel() final;

    void refresh();

protected:
    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;
    bool lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const override;

private:
    SharedGroupChar getCharacterFromSource(const QModelIndex &source_index) const;
};

class NODISCARD GroupStateData final
{
private:
    QColor m_color;
    CharacterPositionEnum m_position = CharacterPositionEnum::UNDEFINED;
    CharacterAffectFlags m_affects;
    int m_count = 0;
    int m_height = 23;

public:
    GroupStateData() = default;
    explicit GroupStateData(const QColor &color,
                            CharacterPositionEnum position,
                            CharacterAffectFlags affects);

public:
    void paint(QPainter *pPainter, const QRect &rect);
    // Shouldn't this be "getArea()"?
    NODISCARD int getWidth() const { return m_count * m_height; }
};
Q_DECLARE_METATYPE(GroupStateData)

class NODISCARD_QOBJECT GroupDelegate final : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit GroupDelegate(QObject *parent);
    ~GroupDelegate() final;

    void paint(QPainter *painter,
               const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    NODISCARD QSize sizeHint(const QStyleOptionViewItem &option,
                             const QModelIndex &index) const override;
};

class NODISCARD_QOBJECT GroupModel final : public QAbstractTableModel
{
    Q_OBJECT

private:
    GroupVector m_characters; // Replaced m_group and m_map
    bool m_mapLoaded = false; // Kept m_mapLoaded for now, though its utility might change

public:
    enum class NODISCARD ColumnTypeEnum {
        NAME = 0,
        HP_PERCENT,
        MANA_PERCENT,
        MOVES_PERCENT,
        HP,
        MANA,
        MOVES,
        STATE,
        ROOM_NAME
    };

    explicit GroupModel(QObject *parent = nullptr); // Changed constructor signature

    void setCharacters(const GroupVector& newChars); // Added new method
    void resetModel(); // Kept resetModel, may not be needed if setCharacters is always used
    NODISCARD QVariant dataForCharacter(const SharedGroupChar &character,
                                        ColumnTypeEnum column,
                                        int role) const;
    // Mmapper2Group* getGroup() const { return m_group; } // Removed getter
    SharedGroupChar getCharacter(int row) const;

    NODISCARD int rowCount(const QModelIndex &parent) const override;
    NODISCARD int columnCount(const QModelIndex &parent) const override;

    NODISCARD QVariant data(const QModelIndex &index, int role) const override;
    NODISCARD QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    // Drag and drop overrides
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    Qt::DropActions supportedDropActions() const override;
    QStringList mimeTypes() const override;
    QMimeData *mimeData(const QModelIndexList &indexes) const override;
    bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) override;

    void setMapLoaded(const bool val) { m_mapLoaded = val; }
};

class NODISCARD_QOBJECT GroupWidget final : public QWidget
{
    Q_OBJECT

private:
    QTableView *m_table = nullptr;
    Mmapper2Group *m_group = nullptr;
    MapData *m_map = nullptr;
    GroupModel m_model;
    GroupProxyModel *m_proxyModel = nullptr;

private:
    QAction *m_center = nullptr;
    QAction *m_recolor = nullptr;
    SharedGroupChar selectedCharacter;

public:
    explicit GroupWidget(Mmapper2Group *group, MapData *md, QWidget *parent);
    ~GroupWidget() final;

protected:
    NODISCARD QSize sizeHint() const override;

signals:
    void sig_kickCharacter(const QString &);
    void sig_center(glm::vec2);

public slots:
    void slot_updateLabels();
    void slot_mapUnloaded() { m_model.setMapLoaded(false); }
    void slot_mapLoaded() { m_model.setMapLoaded(true); }
};
