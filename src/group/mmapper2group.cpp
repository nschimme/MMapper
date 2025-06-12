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
#include "../map/sanitizer.h"
#include "../proxy/GmcpMessage.h"
#include "CGroupChar.h"
#include "mmapper2character.h"

#include <memory>
#include <algorithm> // For std::find_if

#include <QColor>
#include <QDateTime>
#include <QMessageLogContext>
#include <QThread>
#include <QVariantMap>
#include <QtCore>

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

void Mmapper2Group::characterChanged(bool updateCanvas)
{
    emit sig_updateWidget();
    if (updateCanvas) {
        emit sig_updateMapCanvas();
    }
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
        getSelf()->setName(CharacterName{optName.value()});
        characterChanged(false);
    }
}

void Mmapper2Group::parseGmcpCharStatusVars(const JsonObj &obj)
{
    parseGmcpCharName(obj);
}

void Mmapper2Group::parseGmcpCharVitals(const JsonObj &obj)
{
    // "Char.Vitals {\"hp\":100,\"maxhp\":100,\"mana\":100,\"maxmana\":100,\"mp\":139,\"maxmp\":139}"
    characterChanged(updateChar(getSelf(), obj));
}

void Mmapper2Group::parseGmcpGroupAdd(const JsonObj &obj)
{
    const auto id = getGroupId(obj);
    characterChanged(updateChar(addChar(id), obj));
}

void Mmapper2Group::parseGmcpGroupUpdate(const JsonObj &obj)
{
    const auto id = getGroupId(obj);
    auto sharedCh = getCharById(id);
    if (!sharedCh) {
        sharedCh = addChar(id);
    }
    characterChanged(updateChar(sharedCh, obj));
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
            if (updateChar(addChar(id), obj)) {
                change = true;
            }
        }
    }
    characterChanged(change);
}

void Mmapper2Group::parseGmcpRoomInfo(const JsonObj &obj)
{
    SharedGroupChar self = getSelf();

    if (auto optInt = obj.getInt("id")) {
        const auto srvId = ServerRoomId{static_cast<uint32_t>(optInt.value())};
        if (srvId != self->getServerId()) {
            self->setServerId(srvId);
            // No characterChanged needed here? ServerId update handled by updateChar return value
        }
    }
    if (auto optString = obj.getString("name")) {
        const auto name = CharacterRoomName{optString.value()};
        if (name != self->getRoomName()) {
            self->setRoomName(name);
            characterChanged(false); // Room name change should update widget
        }
    }
}

void Mmapper2Group::slot_parseGmcpInput(const GmcpMessage &msg)
{
    if (!msg.getJsonDocument().has_value()) {
        return;
    }

    auto debug = [&msg]() {
        qDebug() << msg.getName().toQByteArray() << msg.getJson()->toQByteArray();
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
    characterChanged(true);
}

SharedGroupChar Mmapper2Group::addChar(const GroupId id)
{
    removeChar(id);
    auto sharedCh = CGroupChar::alloc();
    sharedCh->setId(id);
    m_charIndex.push_back(sharedCh);
    return sharedCh;
}

void Mmapper2Group::removeChar(const GroupId id)
{
    ABORT_IF_NOT_ON_MAIN_THREAD();

    auto it = std::find_if(m_charIndex.begin(), m_charIndex.end(),
                           [&id](const SharedGroupChar& pChar) {
                               // It's good practice to check if pChar is not null before dereferencing,
                               // though m_charIndex should ideally not store nullptrs.
                               return pChar && pChar->getId() == id;
                           });

    bool characterWasRemoved = false;
    if (it != m_charIndex.end()) {
        // Character found.
        SharedGroupChar charToRemove = *it;

        if (charToRemove) { // Check if the shared_ptr itself is not null
            qDebug() << "Found character to remove in Mmapper2Group::removeChar: ID" << charToRemove->getId().asUint32()
                     << "Name:" << charToRemove->getName().toQString();
            if (!charToRemove->isYou()) {
                m_colorGenerator.releaseColor(charToRemove->getColor());
            }
        } else {
            qDebug() << "Warning: Found a null SharedGroupChar in m_charIndex for ID" << id.asUint32();
        }

        m_charIndex.erase(it); // Actually modify m_charIndex
        characterWasRemoved = true;
        qDebug() << "Character with ID" << id.asUint32() << "erased from m_charIndex.";
    }

    if (characterWasRemoved) {
        // Signal that the group composition (and potentially canvas due to character beacons) has changed.
        // This is called AFTER m_charIndex is modified.
        characterChanged(true);
        qDebug() << "Signaled characterChanged(true) after removal for ID" << id.asUint32();
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

    const auto& groupManagerSettings = getConfig().groupManager;
    if (ch.isNPC() && groupManagerSettings.overrideNpcColor) {
        const QColor npcOverrideColor = groupManagerSettings.npcOverrideColor;
        if (ch.getColor().isValid() && ch.getColor() != npcOverrideColor) {
            // Release previously assigned color if it's different from the override color.
            // This primarily targets colors from m_colorGenerator.
            m_colorGenerator.releaseColor(ch.getColor());
        }
        if (ch.getColor() != npcOverrideColor) { // Avoid redundant setColor if already correct
            ch.setColor(npcOverrideColor);
            change = true; // Color change should trigger UI update
        }
    } else {
        // Original logic for non-NPCs or when NPC override is disabled
        if (!ch.getColor().isValid()) {
            ch.setColor(m_colorGenerator.getNextColor());
            qDebug() << "adding" << id.asUint32() << ch.getName().toQString() << "with generated color";
            // If ch.updateFromGmcp(obj) was false, but we set a new color, 'change' should be true.
            if (!change) change = true;
        }
    }

    // Update canvas only if the character moved
    return change && ch.getServerId() != INVALID_SERVER_ROOMID && ch.getServerId() != oldServerId;
}

void Mmapper2Group::slot_updateSelfColorFromConfig()
{
    ABORT_IF_NOT_ON_MAIN_THREAD(); // If appropriate for your threading model
    if (m_self) {
        const QColor newColor = getConfig().groupManager.color;
        if (m_self->getColor() != newColor) {
            m_self->setColor(newColor);
            m_colorGenerator.init(newColor); // Re-initialize color generator if base color changes
            characterChanged(true); // This emits sig_updateWidget and sig_updateMapCanvas
            log("Updated your character color from preferences.");
        }
    }
}
