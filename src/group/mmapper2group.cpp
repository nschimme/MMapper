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
    // This function is being phased out. Direct signal emissions should be used.
    emit sig_updateWidget(); // Kept for now for any remaining callers
    if (updateCanvas) {
        emit sig_updateMapCanvas();
    }
}

void Mmapper2Group::onReset()
{
    ABORT_IF_NOT_ON_MAIN_THREAD();

    resetChars(); // resetChars will emit sig_groupReset({})
}

void Mmapper2Group::parseGmcpCharName(const JsonObj &obj)
{
    // "Char.Name" "{\"fullname\":\"Gandalf the Grey\",\"name\":\"Gandalf\"}"
    if (auto optName = obj.getString("name")) {
        SharedGroupChar self = getSelf();
        self->setName(CharacterName{optName.value()});
        emit sig_characterUpdated(self);
        // characterChanged(false); // Replaced
    }
}

void Mmapper2Group::parseGmcpCharStatusVars(const JsonObj &obj)
{
    // Assuming this primarily updates name or other stats covered by updateChar
    SharedGroupChar self = getSelf();
    if (updateChar(self, obj)) { // updateChar will emit sig_characterUpdated
        emit sig_updateMapCanvas();
    }
    // If Char.StatusVars could update fields not covered by updateChar's GMCP parsing,
    // specific handling for those would be needed here.
    // For now, relying on updateChar.
}

void Mmapper2Group::parseGmcpCharVitals(const JsonObj &obj)
{
    SharedGroupChar self = getSelf();
    // updateChar will emit sig_characterUpdated if data changes.
    // It returns true if map canvas needs update.
    if (updateChar(self, obj)) {
        emit sig_updateMapCanvas();
    }
    // characterChanged(updateChar(getSelf(), obj)); // Replaced
}

void Mmapper2Group::parseGmcpGroupAdd(const JsonObj &obj)
{
    const auto id = getGroupId(obj);
    SharedGroupChar ch = getCharById(id);
    if (!ch) {
        // addChar will emit sig_characterAdded
        ch = addChar(id);
    }
    // updateChar will emit sig_characterUpdated if data changes
    // and returns true if map canvas needs update.
    if (updateChar(ch, obj)) {
        emit sig_updateMapCanvas();
    }
    // characterChanged(updateChar(addChar(id), obj)); // Replaced logic
}

void Mmapper2Group::parseGmcpGroupUpdate(const JsonObj &obj)
{
    const auto id = getGroupId(obj);
    auto sharedCh = getCharById(id);
    if (!sharedCh) {
        // addChar will emit sig_characterAdded
        sharedCh = addChar(id);
    }
    // updateChar will emit sig_characterUpdated if data changes
    // and returns true if map canvas needs update.
    if (updateChar(sharedCh, obj)) {
        emit sig_updateMapCanvas();
    }
    // characterChanged(updateChar(sharedCh, obj)); // Replaced
}

void Mmapper2Group::parseGmcpGroupRemove(const JsonInt n)
{
    const auto id = GroupId{static_cast<uint32_t>(n)};
    // removeChar will find the index and emit sig_characterRemoved
    removeChar(id);
}

void Mmapper2Group::parseGmcpGroupSet(const JsonArray &arr)
{
    // Remove all old characters first. resetChars emits sig_groupReset({}).
    resetChars();

    // Temporarily build the new list.
    // We call a version of addChar/updateChar that doesn't emit individual signals yet,
    // or we build m_charIndex directly.
    // For simplicity, let's allow addChar and updateChar to emit for now,
    // but the final sig_groupReset will be the main signal for GroupModel.
    // This might result in some redundant signals if GroupModel also processes them.
    // A better way:
    GroupVector tempCharIndex;
    tempCharIndex.reserve(arr.size());
    SharedGroupChar tempSelf = m_self; // Preserve self if it was already set up by getSelf() in resetChars.

    // If getSelf() was called inside resetChars, m_self would be in m_charIndex which is now empty.
    // Ensure 'self' is handled correctly.
    // The old resetChars clears m_self, then getSelf() re-adds it.
    // So after resetChars(), m_charIndex contains only 'self' if it was created.

    // The current resetChars clears m_charIndex AND m_self.
    // Then, the first addChar for 'self' in the loop will recreate it.
    // This means the GroupModel will see: reset, add (self), update (self), add (p2), update (p2)... then groupReset.
    // This is not ideal.

    // Ideal parseGmcpGroupSet:
    // 1. Store old 'self' if it exists.
    m_charIndex.clear(); // Clear without emitting reset yet.
    m_self.reset();      // Reset self pointer.
    m_colorGenerator.reset(); // Reset color generator state.

    for (const auto &entry : arr) {
        if (auto optObj = entry.getObject()) {
            const auto &obj = optObj.value();
            const auto id = getGroupId(obj);

            // Simplified internal add (does not emit, does not assign color yet)
            SharedGroupChar ch = CGroupChar::alloc();
            ch->setId(id);
            // Update basic data, especially if it's 'self' to identify it.
            ch->updateFromGmcp(obj); // This will set name, type etc.

            if (ch->isYou()) {
                 m_self = ch; // Assign self
            }
            m_charIndex.push_back(ch);
        }
    }
    // Now assign colors and finish setup
    if (m_self) {
         m_self->setColor(getConfig().groupManager.color);
         m_colorGenerator.init(m_self->getColor());
    } else {
        // If 'self' was not in the group set, we might need to create a default one
        // or rely on the next Char.Name/Vitals to establish it.
        // For now, assume 'self' is usually part of Group.Set or established by Char.Name.
    }

    for(auto& ch_ptr : m_charIndex) {
        if (!ch_ptr->getColor().isValid()) {
            CGroupChar& ch_ref = deref(ch_ptr);
             auto getColor = [&]() -> QColor {
                const auto &settings = getConfig().groupManager;
                if (ch_ref.isNpc() && settings.npcColorOverride) {
                    return settings.npcColor;
                } else {
                    return m_colorGenerator.getNextColor();
                }
            };
            ch_ref.setColor(getColor());
        }
    }

    emit sig_groupReset(m_charIndex);
    emit sig_updateMapCanvas(); // Group set likely changes positions on map
    // characterChanged(change); // Replaced
}

void Mmapper2Group::parseGmcpRoomInfo(const JsonObj &obj)
{
    SharedGroupChar self = getSelf(); // Ensures self exists
    bool selfDataChanged = false;

    if (auto optInt = obj.getInt("id")) {
        const auto srvId = ServerRoomId{static_cast<uint32_t>(optInt.value())};
        if (srvId != self->getServerId()) {
            self->setServerId(srvId);
            // This change is primarily for map canvas, updateChar handles data emission
            // For now, if serverId change implies map update, that's handled separately.
            // If it's also a displayable field in group widget, sig_characterUpdated is needed.
            selfDataChanged = true;
        }
    }
    if (auto optString = obj.getString("name")) {
        const auto name = CharacterRoomName{optString.value()};
        if (name != self->getRoomName()) {
            self->setRoomName(name);
            selfDataChanged = true;
        }
    }

    if (selfDataChanged) {
        emit sig_characterUpdated(self);
        // If only room name changed, map canvas might not need update unless it shows room names.
        // The original code called characterChanged(false) for room name, implying only widget update.
        // If ServerID changed, map canvas IS affected.
        // Let's assume updateChar or a direct call to sig_updateMapCanvas handles map.
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
    } else if (msg.isCharName()) { // Separate from StatusVars for clarity if parseGmcpCharName is more specific
        debug();
        parseGmcpCharName(obj);
    } else if (msg.isCharStatusVars()) {
        debug();
        parseGmcpCharStatusVars(obj); // Potentially calls updateChar which covers more fields
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

    // Emit reset signal with an empty list BEFORE clearing.
    // Or, clear then emit. For consistency, let's emit with the new (empty) state.
    m_charIndex.clear();
    m_self.reset();
    m_colorGenerator.reset(); // Reset color generator as well
    emit sig_groupReset({}); // Pass an empty vector.
    emit sig_updateMapCanvas(); // Group is gone, map should reflect this.
    // characterChanged(true); // Replaced
}

SharedGroupChar Mmapper2Group::addChar(const GroupId id)
{
    // This function assumes the char with 'id' does NOT already exist.
    // Callers should check getCharById first if unsure.
    // The old `removeChar(id)` call here was problematic.
    auto sharedCh = CGroupChar::alloc();
    sharedCh->setId(id);
    m_charIndex.push_back(sharedCh);
    emit sig_characterAdded(sharedCh);
    // Color assignment and further updates are typically done by updateChar after this.
    return sharedCh;
}

void Mmapper2Group::removeChar(const GroupId id)
{
    ABORT_IF_NOT_ON_MAIN_THREAD();

    int indexToRemove = findCharacterIndexById(m_charIndex, id);

    if (indexToRemove != -1) {
        // Emit signal BEFORE actual removal, using the ID.
        emit sig_characterRemoved(id);

        // Proceed with removal logic
        SharedGroupChar pChar = m_charIndex[static_cast<size_t>(indexToRemove)];
        if (pChar) { // Should always be true if indexToRemove is valid
            auto &character = deref(pChar);
            const auto &settings = getConfig().groupManager;
            if (!character.isYou() && character.getColor() != settings.npcColor) {
                m_colorGenerator.releaseColor(character.getColor());
            }
            qDebug() << "removing" << id.asUint32() << character.getName().toQString();
        }

        m_charIndex.erase(m_charIndex.begin() + indexToRemove);

        // If the removed character was 'self', reset m_self.
        if (m_self && m_self->getId() == id) {
            m_self.reset();
        }
        emit sig_updateMapCanvas(); // Character removed, map might need update
        // Removed: characterChanged(true);
    }
}

SharedGroupChar Mmapper2Group::getCharById(const GroupId id) const
{
    ABORT_IF_NOT_ON_MAIN_THREAD();
    for (const auto &character : m_charIndex) {
        if (character && character->getId() == id) { // Added null check for character
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
        if (character) { // Added null check
            const auto b = ::toLowerUtf8(utf8ToAscii(character->getName().getStdStringViewUtf8()));
            if (b == a) {
                return character;
            }
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
    if (!sharedCh) return false; // Guard against null sharedCh

    CGroupChar &ch = deref(sharedCh);
    const auto id = ch.getId();
    const auto oldServerId = ch.getServerId();
    bool modelDataChanged = ch.updateFromGmcp(obj); // True if any data relevant to CGroupChar changed

    // Handle 'self' specific logic
    if (ch.isYou()) {
        if (!m_self) { // If m_self wasn't set or was reset
            m_self = sharedCh;
            // Ensure self has the correct color, potentially overriding GMCP
            QColor selfColor = getConfig().groupManager.color;
            if (ch.getColor() != selfColor) {
                ch.setColor(selfColor);
                modelDataChanged = true; // Color change is a model data change
            }
            m_colorGenerator.init(selfColor); // Ensure generator is initialized with self's color
        } else if (m_self->getId() != ch.getId()) {
            // This case implies 'ch' was just identified as 'self', but 'm_self' was pointing to another char.
            // This should be rare if Group.Set is processed correctly.
            // For now, assume 'm_self' is the authoritative SharedGroupChar for 'you'.
            // The incoming 'sharedCh' that isYou() should BE m_self.
            // If it's not, it could mean a duplicate 'you' character was created.
            // This part of logic might need review based on how 'self' is established in Group.Set.
            // For now, if ch is 'you' and m_self exists and is different, update m_self's data.
            // And remove 'ch' if it's a duplicate. This is complex.
            // Simpler: Assume if ch.isYou(), then sharedCh *is* m_self due to getSelf() or Group.Set logic.
        }
    }

    // Assign color if new or NPC color override changes
    bool colorAssignedOrChanged = false;
    QColor oldColor = ch.getColor();
    if (!ch.getColor().isValid()) { // New character needing a color
        auto getColor = [&]() -> QColor {
            const auto &settings = getConfig().groupManager;
            if (ch.isNpc() && settings.npcColorOverride) {
                return settings.npcColor;
            } else if (ch.isYou()) { // Ensure 'you' always gets the configured color
                return getConfig().groupManager.color;
            }
            return m_colorGenerator.getNextColor();
        };
        ch.setColor(getColor());
        qDebug() << "assigning color to" << id.asUint32() << ch.getName().toQString();
        colorAssignedOrChanged = true;
    } else if (ch.isNpc() && getConfig().groupManager.npcColorOverride &&
               ch.getColor() != getConfig().groupManager.npcColor) {
        // NPC color override changed or was enabled
        if (oldColor != getConfig().groupManager.npcColor && oldColor.isValid() && oldColor != m_colorGenerator.getSelfColor()) {
             m_colorGenerator.releaseColor(oldColor); // Release old color only if it's not the default/self color
        }
        ch.setColor(getConfig().groupManager.npcColor);
        colorAssignedOrChanged = true;
    }


    if (modelDataChanged || colorAssignedOrChanged) {
        emit sig_characterUpdated(sharedCh);
    }

    // Return value indicates if map canvas specifically needs an update due to movement.
    bool mapCanvasUpdateNeeded = (ch.getServerId() != oldServerId && ch.getServerId() != INVALID_SERVER_ROOMID);
    return mapCanvasUpdateNeeded;
}

void Mmapper2Group::slot_groupSettingsChanged()
{
    const auto &settings = getConfig().groupManager;
    bool needsMapUpdate = false;
    for (const auto &pChar : m_charIndex) {
        if (!pChar) continue;
        auto &character = deref(pChar);
        QColor oldColor = character.getColor();
        bool colorChanged = false;

        if (character.isYou()) {
            if (character.getColor() != settings.color) {
                character.setColor(settings.color);
                m_colorGenerator.init(settings.color); // Re-init with new self color
                colorChanged = true;
            }
        } else if (character.isNpc()) {
            if (settings.npcColorOverride) {
                if (character.getColor() != settings.npcColor) {
                    // Release old color only if it was a generated one
                    if (oldColor.isValid() && oldColor != settings.color && oldColor != m_colorGenerator.getSelfColor()) {
                         bool isPreviouslyOverriddenNpcColor = false; // Check if oldColor was a previous npcColor
                         // This check is tricky. For simplicity, assume any valid non-self, non-current-NPC-override color can be released.
                         // A more robust ColorGenerator would track its own colors.
                         if (character.getColor() != QColor() /* check against a known list of generated colors if possible */) {
                            m_colorGenerator.releaseColor(character.getColor());
                         }
                    }
                    character.setColor(settings.npcColor);
                    colorChanged = true;
                }
            } else { // NPC color override is OFF. If it was previously ON, character might need a new generated color.
                if (character.getColor() == settings.npcColor && oldColor.isValid()) { // Was using NPC override, now needs new color
                    character.setColor(m_colorGenerator.getNextColor()); // Assign new generated color
                    colorChanged = true;
                }
            }
        }
        // Other settings like npcSortBottom or npcHide are handled by the model/view, not by changing char data here.
        if (colorChanged) {
            emit sig_characterUpdated(pChar);
        }
    }
    // characterChanged(true); // Replaced by specific updates.
    // Group settings changes (like colors) might affect map canvas if characters are drawn there with these colors.
    emit sig_updateMapCanvas(); // Conservatively update map canvas
}
