// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Dmitrijs Barbarins <lachupe@gmail.com> (Azazello)
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "mmapper2group.h"

#include "../configuration/configuration.h"
#include "../global/CaseUtils.h"
#include "../global/Charset.h"
#include "../global/JsonArray.h"
#include "../global/JsonObj.h"
#include "../global/thread_utils.h"
#include "../proxy/GmcpMessage.h"
#include "CGroupChar.h"
#include "mmapper2character.h"

#include <memory>

#include <QColor>
#include <QDateTime>
#include <QMessageLogContext>
#include <QThread>
#include <QVariantMap>

static const bool verbose_debugging = false;

Mmapper2Group::Mmapper2Group(QObject *const parent)
    : QObject{parent}
    , m_groupManagerApi{std::make_unique<GroupManagerApi>(*this)}
{}

Mmapper2Group::~Mmapper2Group() {}

SharedGroupChar Mmapper2Group::getSelf()
{
    if (!m_self) {
        m_self = CGroupChar::alloc();
        m_self->setType(CharacterTypeEnum::YOU);
        m_charIndex.push_back(m_self);

        const auto color = getConfig().groupManager.color;
        m_self->setColor(color);
        m_colorGenerator.init(color);
    }
    return m_self;
}

void Mmapper2Group::characterChanged()
{
    emit sig_updateMapCanvas();
    emit sig_characterUpdated(getSelf());
}

void Mmapper2Group::onReset()
{
    ABORT_IF_NOT_ON_MAIN_THREAD();

    resetChars();
}

void Mmapper2Group::parseGmcpCharName(const JsonObj &obj)
{
    // "Char.Name" "{\"fullname\":\"Gandalf the Grey\",\"name\":\"Gandalf\"}"
    if (auto optName = obj.getString("name")) {
        SharedGroupChar self = getSelf();
        self->setName(CharacterName{optName.value()});
        emit sig_characterUpdated(self);
    }
}

void Mmapper2Group::parseGmcpCharStatusVars(const JsonObj &obj)
{
    parseGmcpCharName(obj);
}

void Mmapper2Group::parseGmcpCharVitals(const JsonObj &obj)
{
    // "Char.Vitals {\"hp\":100,\"maxhp\":100,\"mana\":100,\"maxmana\":100,\"mp\":139,\"maxmp\":139}"
    SharedGroupChar self = getSelf();
    if (updateChar(self, obj)) {
        emit sig_updateMapCanvas();
    }
    emit sig_characterUpdated(self);
}

void Mmapper2Group::parseGmcpGroupAdd(const JsonObj &obj)
{
    const auto id = getGroupId(obj);
    auto sharedCh = addChar(id);
    if (updateChar(sharedCh, obj)) {
        emit sig_updateMapCanvas();
    }
    emit sig_characterUpdated(sharedCh);
}

void Mmapper2Group::parseGmcpGroupUpdate(const JsonObj &obj)
{
    const auto id = getGroupId(obj);
    auto sharedCh = getCharById(id);
    if (!sharedCh) {
        sharedCh = addChar(id);
    }
    if (updateChar(sharedCh, obj)) {
        emit sig_updateMapCanvas();
    }
    emit sig_characterUpdated(sharedCh);
}

void Mmapper2Group::parseGmcpGroupRemove(const JsonInt n)
{
    const auto id = GroupId{static_cast<uint32_t>(n)};
    removeChar(id);
}

void Mmapper2Group::parseGmcpGroupSet(const JsonArray &arr)
{
    // Remove old characters (except self)
    resetChars();
    bool change = false;
    for (const auto &entry : arr) {
        if (auto optObj = entry.getObject()) {
            const auto &obj = optObj.value();
            const auto id = getGroupId(obj);
            auto sharedCh = addChar(id);
            if (updateChar(sharedCh, obj)) {
                change = true;
            }
            emit sig_characterUpdated(sharedCh);
        }
    }
    if (change) {
        emit sig_updateMapCanvas();
    }
}

void Mmapper2Group::parseGmcpRoomInfo(const JsonObj &obj)
{
    SharedGroupChar self = getSelf();
    bool change = false;

    if (auto optInt = obj.getInt("id")) {
        const auto srvId = ServerRoomId{static_cast<uint32_t>(optInt.value())};
        if (srvId != self->getServerId()) {
            self->setServerId(srvId);
            // TODO: No change needed here? ServerId update handled by updateChar return value
            change = true;
        }
    }
    if (auto optString = obj.getString("name")) {
        const auto name = CharacterRoomName{optString.value()};
        if (name != self->getRoomName()) {
            self->setRoomName(name);
            change = true;
        }
    }

    if (change) {
        emit sig_characterUpdated(self);
    }
}

void Mmapper2Group::slot_parseGmcpInput(const GmcpMessage &msg)
{
    if (!msg.getJsonDocument().has_value()) {
        return;
    }

    auto debug = [&msg]() {
        if (verbose_debugging) {
            qDebug() << msg.getName().toQByteArray() << msg.getJson()->toQByteArray();
        }
    };

    if (msg.isGroupRemove()) {
        debug();
        if (auto optInt = msg.getJsonDocument()->getInt()) {
            parseGmcpGroupRemove(optInt.value());
        }
        return;
    } else if (msg.isGroupSet()) {
        debug();
        if (auto optArray = msg.getJsonDocument()->getArray()) {
            parseGmcpGroupSet(optArray.value());
        }
        return;
    }

    auto optObj = msg.getJsonDocument()->getObject();
    if (!optObj) {
        return;
    }
    auto &obj = optObj.value();

    if (msg.isCharVitals()) {
        debug();
        parseGmcpCharVitals(obj);
    } else if (msg.isCharName() || msg.isCharStatusVars()) {
        debug();
        parseGmcpCharName(obj);
    } else if (msg.isGroupAdd()) {
        debug();
        parseGmcpGroupAdd(obj);
    } else if (msg.isGroupUpdate()) {
        debug();
        parseGmcpGroupUpdate(obj);
    } else if (msg.isRoomInfo()) {
        debug();
        parseGmcpRoomInfo(obj);
    }
}

void Mmapper2Group::resetChars()
{
    ABORT_IF_NOT_ON_MAIN_THREAD();

    log("You have left the group.");

    m_self.reset();
    m_charIndex.clear();
    emit sig_groupReset({});
    emit sig_updateMapCanvas();
}

SharedGroupChar Mmapper2Group::addChar(const GroupId id)
{
    if (id != INVALID_GROUPID) {
        removeChar(id);
    }
    auto sharedCh = CGroupChar::alloc();
    sharedCh->setId(id);
    m_charIndex.push_back(sharedCh);
    emit sig_characterAdded(sharedCh);
    return sharedCh;
}

void Mmapper2Group::removeChar(const GroupId id)
{
    ABORT_IF_NOT_ON_MAIN_THREAD();

    emit sig_characterRemoved(id);

    const auto &settings = getConfig().groupManager;
    auto erased = utils::erase_if(m_charIndex, [this, &settings, &id](const SharedGroupChar &pChar) {
        auto &character = deref(pChar);
        if (character.getId() != id) {
            return false;
        }
        if (!character.isYou() && character.getColor() != settings.npcColor) {
            m_colorGenerator.releaseColor(character.getColor());
        }
        qDebug() << "removing" << id.asUint32() << character.getName().toQString();
        return true;
    });
    if (erased) {
        emit sig_updateMapCanvas();
    }
}

SharedGroupChar Mmapper2Group::getCharById(const GroupId id) const
{
    ABORT_IF_NOT_ON_MAIN_THREAD();
    for (const auto &character : m_charIndex) {
        if (character->getId() == id) {
            return character;
        }
    }
    return {};
}

SharedGroupChar Mmapper2Group::getCharByName(const CharacterName &name) const
{
    ABORT_IF_NOT_ON_MAIN_THREAD();
    using namespace charset::conversion;
    const auto a = ::toLowerUtf8(utf8ToAscii(name.getStdStringViewUtf8()));
    for (const auto &character : m_charIndex) {
        const auto b = ::toLowerUtf8(utf8ToAscii(character->getName().getStdStringViewUtf8()));
        if (b == a) {
            return character;
        }
    }
    return {};
}

GroupId Mmapper2Group::getGroupId(const JsonObj &obj)
{
    auto optId = obj.getInt("id");
    if (!optId) {
        return INVALID_GROUPID;
    }
    return GroupId{static_cast<uint32_t>(optId.value())};
}

bool Mmapper2Group::updateChar(SharedGroupChar sharedCh, const JsonObj &obj)
{
    CGroupChar &ch = deref(sharedCh);

    const auto id = ch.getId();
    const auto oldServerId = ch.getServerId();
    bool change = ch.updateFromGmcp(obj);
    if (ch.isYou()) {
        if (!m_self) {
            m_self = sharedCh;
            m_self->setColor(getConfig().groupManager.color);
        } else if (m_self->getId() != sharedCh->getId()) {
            m_self->setId(ch.getId());
            change = m_self->updateFromGmcp(obj);
            utils::erase_if(m_charIndex, [&sharedCh](const SharedGroupChar &pChar) -> bool {
                return pChar == sharedCh;
            });
        }
    }

    if (!ch.getColor().isValid()) {
        auto getColor = [&]() -> QColor {
            const auto &settings = getConfig().groupManager;
            if (ch.isNpc() && settings.npcColorOverride) {
                return settings.npcColor;
            } else {
                return m_colorGenerator.getNextColor();
            }
        };
        ch.setColor(getColor());
        qDebug() << "adding" << id.asUint32() << ch.getName().toQString();
    }

    // Update canvas only if the character moved
    return change && ch.getServerId() != INVALID_SERVER_ROOMID && ch.getServerId() != oldServerId;
}

void Mmapper2Group::slot_groupSettingsChanged()
{
    const auto &settings = getConfig().groupManager;
    for (const auto &pChar : m_charIndex) {
        auto &character = deref(pChar);
        if (character.isYou()) {
            character.setColor(settings.color);
        } else if (character.isNpc() && settings.npcColorOverride) {
            if (character.getColor() != settings.npcColor) {
                m_colorGenerator.releaseColor(character.getColor());
            }
            character.setColor(settings.npcColor);
        }
    }
    characterChanged();
}
