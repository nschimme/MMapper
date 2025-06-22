// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "MudTelnet.h"

#include "../clock/mumeclock.h"
#include "../display/MapCanvasConfig.h"
#include "../global/Consts.h"
#include "../global/LineUtils.h"
#include "../global/SendToUser.h"
#include "../global/TextUtils.h"
#include "../global/Version.h"
#include "../global/emojis.h"
#include "GmcpUtils.h"

#include <charconv>
#include <list>
#include <map>
#include <optional>
#include <sstream>
#include <string_view>

#include <QByteArray>
#include <QOperatingSystemVersion>
#include <QSysInfo>

#include <map>

namespace { // anonymous

constexpr const auto GAME_YEAR = "GAME YEAR";
constexpr const auto GAME_MONTH = "GAME MONTH";
constexpr const auto GAME_DAY = "GAME DAY";
constexpr const auto GAME_HOUR = "GAME HOUR";

NODISCARD std::optional<std::string> getMajorMinor()
{
    const auto major = QOperatingSystemVersion::current().majorVersion();
    if (major < 0) {
        return std::nullopt;
    }
    const auto minor = QOperatingSystemVersion::current().minorVersion();
    if (minor < 0) {
        return std::to_string(major);
    }
    return std::to_string(major) + "." + std::to_string(minor);
}

NODISCARD std::string getOsName()
{
#define X_CASE(X) \
    case (PlatformEnum::X): \
        return #X

    switch (CURRENT_PLATFORM) {
        X_CASE(Linux);
        X_CASE(Mac);
        X_CASE(Windows);
        X_CASE(Unknown);
    }
    std::abort();
#undef X_CASE
}

NODISCARD std::string getOs()
{
    if (auto ver = getMajorMinor()) {
        return getOsName() + ver.value();
    }
    return getOsName();
}

NODISCARD TelnetTermTypeBytes addTerminalTypeSuffix(const std::string_view prefix)
{
    // It's probably required to be ASCII.
    const auto arch = mmqt::toStdStringUtf8(QSysInfo::currentCpuArchitecture().toUtf8());

    std::ostringstream ss;
    ss << prefix << "/MMapper-" << getMMapperVersion() << "/"
       << MapCanvasConfig::getCurrentOpenGLVersion() << "/" << getOs() << "/" << arch;
    auto str = std::move(ss).str();

    return TelnetTermTypeBytes{mmqt::toQByteArrayUtf8(str)};
}

using OptStdString = std::optional<std::string>;
struct MsspMap final
{
private:
    // REVISIT: why is this a list? always prefer vector over list, unless there's a good reason.
    // (And the good reason needs to be documented.)
    std::map<std::string, std::list<std::string>> m_map;

public:
    // Parse game time from MSSP
    NODISCARD OptStdString lookup(const std::string &key) const
    {
        const auto it = m_map.find(key);
        if (it == m_map.end()) {
            MMLOG_WARNING() << "MSSP missing key " << key;
            return std::nullopt;
        }
        const auto &elements = it->second;
        if (elements.empty()) {
            MMLOG_WARNING() << "MSSP empty key " << key;
            return std::nullopt;
        }
        // REVISIT: protocols that allow duplicates usually declare that the LAST one is correct,
        // but we're taking the first one here.
        return elements.front();
    }

public:
    NODISCARD static auto parseMssp(const TelnetMsspBytes &data, const bool debug)
    {
        MsspMap result;
        auto &map = result.m_map;

        std::optional<std::string> varName = std::nullopt;
        std::list<std::string> vals;

        enum class NODISCARD MSSPStateEnum {
            ///
            BEGIN,
            /// VAR
            IN_VAR,
            /// VAL
            IN_VAL
        } state
            = MSSPStateEnum::BEGIN;

        AppendBuffer buffer;

        const auto addValue([&map, &vals, &varName, &buffer, debug]() {
            // Put it into the map.
            if (debug) {
                qDebug() << "MSSP received value" << buffer << "for variable"
                         << mmqt::toQByteArrayRaw(varName.value());
            }

            vals.push_back(buffer.getQByteArray().toStdString());
            map[varName.value()] = vals;

            buffer.clear();
        });

        for (const char c : data) {
            switch (state) {
            case MSSPStateEnum::BEGIN:
                if (c != TNSB_MSSP_VAR) {
                    continue;
                }
                state = MSSPStateEnum::IN_VAR;
                break;

            case MSSPStateEnum::IN_VAR:
                switch (static_cast<uint8_t>(c)) {
                case TNSB_MSSP_VAR:
                case TN_IAC:
                case 0:
                    continue;

                case TNSB_MSSP_VAL: {
                    if (buffer.isEmpty()) {
                        if (debug) {
                            qDebug() << "MSSP received variable without any name; ignoring it";
                        }
                        continue;
                    }

                    if (debug) {
                        qDebug() << "MSSP received variable" << buffer;
                    }

                    varName = buffer.getQByteArray().toStdString();
                    state = MSSPStateEnum::IN_VAL;

                    vals.clear(); // Which means this is a new value, so clear the list.
                    buffer.clear();
                } break;

                default:
                    buffer.append(static_cast<uint8_t>(c));
                }
                break;

            case MSSPStateEnum::IN_VAL: {
                assert(varName.has_value());

                switch (static_cast<uint8_t>(c)) {
                case TN_IAC:
                case 0:
                    continue;

                case TNSB_MSSP_VAR:
                    state = MSSPStateEnum::IN_VAR;
                    FALLTHROUGH;
                case TNSB_MSSP_VAL:
                    addValue();
                    break;

                default:
                    buffer.append(static_cast<uint8_t>(c));
                    break;
                }
                break;
            }
            }
        }

        if (varName.has_value() && !buffer.isEmpty()) {
            addValue();
        }

        return result;
    }
};

NODISCARD bool isOneLineCrlf(const QString &s)
{
    if (s.isEmpty() || s.back() != char_consts::C_NEWLINE) {
        return false;
    }
    QStringView sv = s;
    sv.chop(1);
    return !sv.empty() && sv.back() == char_consts::C_CARRIAGE_RETURN
           && !sv.contains(char_consts::C_NEWLINE);
}

} // namespace

MudTelnetOutputs::~MudTelnetOutputs() = default;

MudTelnet::MudTelnet(MudTelnetOutputs &outputs)
    : AbstractTelnet(TextCodecStrategyEnum::FORCE_UTF_8, addTerminalTypeSuffix("unknown"))
    , m_outputs{outputs}
{
    // RFC 2066 states we can provide many character sets but we force UTF-8 when
    // communicating with MUME
    resetGmcpModules();
}

void MudTelnet::onDisconnected()
{
    // Reset Telnet options but retain GMCP modules
    reset();
}

void MudTelnet::onAnalyzeMudStream(const TelnetIacBytes &data)
{
    onReadInternal(data);
}

void MudTelnet::onSubmitGmcpMumeClient(const GmcpMessage &m)
{
    assert(m.getName().toQString().startsWith("MUME.Client."));
    sendGmcpMessage(m);
}

void MudTelnet::submitOneLine(const QString &inputLine)
{
    auto submit = [this](const QString &s) {
        if (getConfig().parser.encodeEmoji && mmqt::containsNonLatin1Codepoints(s)) {
            submitOverTelnet(mmqt::encodeEmojiShortCodes(s), false);
        } else {
            submitOverTelnet(s, false);
        }
    };

    assert(isOneLineCrlf(inputLine));

    submit(inputLine);
}

void MudTelnet::onSendToMud(const QString &s)
{
    if (s.isEmpty()) {
        assert(false);
        return;
    }

    if (m_lineBuffer.isEmpty() && isOneLineCrlf(s)) {
        submitOneLine(s);
        return;
    }

    // fallback: buffering
    mmqt::foreachLine(s, [this](QStringView line, const bool hasNewline) {
        if (hasNewline && !line.empty() && line.back() == char_consts::C_CARRIAGE_RETURN) {
            line.chop(1);
        }

        m_lineBuffer += line;
        if (!hasNewline) {
            return;
        }

        const auto oneline = std::exchange(m_lineBuffer, {}) + string_consts::S_CRLF;
        submitOneLine(oneline);
    });
}

void MudTelnet::onGmcpToMud(const GmcpMessage &msg)
{
    const auto &hisOptionState = getOptions().hisOptionState;

    // Remember Core.Supports.[Add|Set|Remove] modules
    if (msg.getJson()
        && (msg.isCoreSupportsAdd() || msg.isCoreSupportsSet() || msg.isCoreSupportsRemove())
        && msg.getJsonDocument().has_value() && msg.getJsonDocument()->getArray().has_value()) {
        // Handle the various messages
        if (msg.isCoreSupportsSet()) {
            resetGmcpModules();
        }
        if (const auto optArray = msg.getJsonDocument()->getArray()) {
            for (const auto &e : optArray.value()) {
                if (auto optString = e.getString()) {
                    const auto &module = optString.value();
                    try {
                        const GmcpModule mod(mmqt::toStdStringUtf8(module));
                        receiveGmcpModule(mod, !msg.isCoreSupportsRemove());

                    } catch (const std::exception &e) {
                        qWarning()
                            << "Module" << module << (msg.isCoreSupportsRemove() ? "remove" : "add")
                            << "error because:" << e.what();
                    }
                }
            }
        }

        // Send it now if GMCP has been negotiated
        if (hisOptionState[OPT_GMCP]) {
            sendCoreSupports();
        }
        return;
    }

    if (msg.isExternalDiscordHello()) {
        m_receivedExternalDiscordHello = true;
    }

    if (!hisOptionState[OPT_GMCP]) {
        qDebug() << "MUME did not request GMCP yet";
        return;
    }

    sendGmcpMessage(msg);
}

void MudTelnet::onRelayNaws(const int width, const int height)
{
    // remember the size - we'll need it if NAWS is currently disabled but will
    // be enabled. Also remember it if no connection exists at the moment;
    // we won't be called again when connecting
    m_currentNaws.width = width;
    m_currentNaws.height = height;

    if (getOptions().myOptionState[OPT_NAWS]) {
        // only if we have negotiated this option
        sendWindowSizeChanged(width, height);
    }
}

void MudTelnet::onRelayTermType(const TelnetTermTypeBytes &terminalType)
{
    // This is typically the client's name, relayed from UserTelnet's first TTYPE response.
    m_relayedClientName = terminalType;

    // The actual terminal type (like "MMAPPER-XTERM") that MudTelnet itself reports
    // if it were initiating TTYPE or responding to a simple TTYPE request without full MTTS cycle
    // is set via `setTerminalType`.
    // For the MTTS cycle, we'll use m_relayedClientName, then m_relayedClientTerminal, then calculated MTTS.
    // The `addTerminalTypeSuffix` is more for the `NEW-ENVIRON` `TERMINAL_TYPE` variable.
    // Let's ensure `m_termType` (from AbstractTelnet) is a reasonable default for MMapper itself.
    // It's initialized in MudTelnet constructor: AbstractTelnet(..., addTerminalTypeSuffix("unknown"))
    // This is fine. If the MUD asks for TTYPE and we haven't got client info yet, it sends this.
}

void MudTelnet::onLoginCredentials(const QString &name, const QString &password)
{
    sendGmcpMessage(
        GmcpMessage(GmcpMessageTypeEnum::CHAR_LOGIN,
                    GmcpJson{QString(R"({ "name": "%1", "password": "%2" })").arg(name, password)}));
}

void MudTelnet::virt_sendToMapper(const RawBytes &data, const bool goAhead)
{
    if (getDebug()) {
        qDebug() << "MudTelnet::virt_sendToMapper" << data;
    }

    m_outputs.onAnalyzeMudStream(data, goAhead);
}

void MudTelnet::virt_receiveEchoMode(const bool toggle)
{
    m_outputs.onRelayEchoMode(toggle);
}

void MudTelnet::virt_receiveGmcpMessage(const GmcpMessage &msg)
{
    if (getDebug()) {
        qDebug() << "Receiving GMCP from MUME" << msg.toRawBytes();
    }

    if (msg.isMumeClientError()) {
        if (auto optJson = msg.getJson()) {
            m_outputs.onMumeClientError(optJson->toQString());
        }
        return;
    }

    if (msg.isMumeClientView() || msg.isMumeClientEdit()) {
        if (!msg.getJsonDocument()) {
            return;
        }
        auto optObj = msg.getJsonDocument()->getObject();
        if (!optObj) {
            return;
        }
        auto &obj = optObj.value();

        const auto optTitle = obj.getString("title");
        const auto optText = obj.getString("text");

        if (msg.isMumeClientView()) {
            m_outputs.onMumeClientView(optTitle.value_or("View text..."), optText.value_or(""));
        } else if (msg.isMumeClientEdit()) {
            if (auto optId = obj.getInt("id")) {
                m_outputs.onMumeClientEdit(RemoteSessionId{optId.value()},
                                           optTitle.value_or("Edit text..."),
                                           optText.value_or(""));
            }
        }
        return;
    }

    if (msg.isMumeClientWrite() || msg.isMumeClientCancelEdit()) {
        if (!msg.getJsonDocument()) {
            return;
        }
        auto optObj = msg.getJsonDocument()->getObject();
        if (!optObj) {
            return;
        }
        auto &obj = optObj.value();

        const auto id = obj.getInt("id").value_or(REMOTE_VIEW_SESSION_ID.asInt32());
        const auto optBool = obj.getBool("result");
        const auto optString = obj.getString("result");

        if (optBool && optBool.value()) {
            qDebug() << "[success] Successfully" << (msg.isMumeClientWrite() ? "sent" : "cancelled")
                     << "remote edit" << id;
        } else {
            const auto action = (msg.isMumeClientWrite() ? "sending" : "canceling");
            const auto result = optString.value_or("missing text");
            qDebug() << "Failure" << action << "remote message" << id << result;
            // Mume doesn't send anything, so we have to make our own message.
            global::sendToUser(QString("Failure %1 remote message: %1").arg(action, result));
        }
        return;
    }

    m_outputs.onRelayGmcpFromMudToUser(msg);
}

void MudTelnet::virt_receiveMudServerStatus(const TelnetMsspBytes &ba)
{
    parseMudServerStatus(ba);
    m_outputs.onSendMSSPToUser(ba);
}

void MudTelnet::virt_onGmcpEnabled()
{
    if (getDebug()) {
        qDebug() << "Requesting GMCP from MUME";
    }

    sendGmcpMessage(
        GmcpMessage(GmcpMessageTypeEnum::CORE_HELLO,
                    GmcpJson{QString(R"({ "client": "MMapper", "version": "%1" })")
                                 .arg(GmcpUtils::escapeGmcpStringData(getMMapperVersion()))}));

    // Request GMCP modules that might have already been sent by the local client
    sendCoreSupports();

    if (m_receivedExternalDiscordHello) {
        sendGmcpMessage(GmcpMessage{GmcpMessageTypeEnum::EXTERNAL_DISCORD_HELLO});
    }

    // Request XML mode
    sendGmcpMessage(GmcpMessage{GmcpMessageTypeEnum::MUME_CLIENT_XML,
                                GmcpJson{R"({ "enable": true, "silent": true })"}});

    // Check if user has requested we remember our login credentials
    m_outputs.onTryCharLogin();
}

void MudTelnet::virt_sendRawData(const TelnetIacBytes &data)
{
    m_outputs.onSendToSocket(data);
}

void MudTelnet::receiveGmcpModule(const GmcpModule &mod, const bool enabled)
{
    if (enabled) {
        m_gmcp.insert(mod);
    } else {
        m_gmcp.erase(mod);
    }
}

void MudTelnet::resetGmcpModules()
{
    m_gmcp.clear();

    // Following modules are enabled by default
    receiveGmcpModule(GmcpModule{GmcpModuleTypeEnum::CHAR, GmcpModuleVersion{1}}, true);
    receiveGmcpModule(GmcpModule{GmcpModuleTypeEnum::EVENT, GmcpModuleVersion{1}}, true);
    receiveGmcpModule(GmcpModule{GmcpModuleTypeEnum::EXTERNAL_DISCORD, GmcpModuleVersion{1}}, true);
    receiveGmcpModule(GmcpModule{GmcpModuleTypeEnum::GROUP, GmcpModuleVersion{1}}, true);
    receiveGmcpModule(GmcpModule{GmcpModuleTypeEnum::ROOM_CHARS, GmcpModuleVersion{1}}, true);
    receiveGmcpModule(GmcpModule{GmcpModuleTypeEnum::ROOM, GmcpModuleVersion{1}}, true);
    receiveGmcpModule(GmcpModule{GmcpModuleTypeEnum::MUME_CLIENT, GmcpModuleVersion{1}}, true);
}

void MudTelnet::sendCoreSupports()
{
    if (m_gmcp.empty()) {
        qWarning() << "No GMCP modules can be requested";
        return;
    }

    std::ostringstream oss;
    oss << "[ ";
    bool comma = false;
    for (const GmcpModule &mod : m_gmcp) {
        if (comma) {
            oss << ", ";
        }
        oss << char_consts::C_DQUOTE << mod.toStdString() << char_consts::C_DQUOTE;
        comma = true;
    }
    oss << " ]";
    const std::string set = oss.str();

    if (getDebug()) {
        qDebug() << "Sending GMCP Core.Supports to MUME" << mmqt::toQByteArrayUtf8(set);
    }
    sendGmcpMessage(GmcpMessage(GmcpMessageTypeEnum::CORE_SUPPORTS_SET, GmcpJson{set}));
}

void MudTelnet::parseMudServerStatus(const TelnetMsspBytes &data)
{
    const auto map = MsspMap::parseMssp(data, getDebug());

    // REVISIT: try to read minute, in case mume ever supports it?
    const OptStdString yearStr = map.lookup(GAME_YEAR);
    const OptStdString monthStr = map.lookup(GAME_MONTH);
    const OptStdString dayStr = map.lookup(GAME_DAY);
    const OptStdString hourStr = map.lookup(GAME_HOUR);

    MMLOG() << "MSSP game time received with"
            << " year:" << yearStr.value_or("unknown") << " month:" << monthStr.value_or("unknown")
            << " day:" << dayStr.value_or("unknown") << " hour:" << hourStr.value_or("unknown");

    if (!yearStr.has_value() || !monthStr.has_value() || !dayStr.has_value()
        || !hourStr.has_value()) {
        MMLOG_WARNING() << "missing one or more MSSP keys";
        return;
    }

    using OptInt = std::optional<int>;
    auto my_stoi = [](const OptStdString &optString) -> OptInt {
        if (!optString.has_value()) {
            return std::nullopt;
        }
        const auto &s = optString.value();
        const auto beg = s.data();
        // NOLINTNEXTLINE (pointer arithmetic)
        const auto end = beg + static_cast<ptrdiff_t>(s.size());
        int result = 0;
        auto [ptr, ec] = std::from_chars(beg, end, result);
        if (ec != std::errc{} || (ptr != nullptr && *ptr != char_consts::C_NUL)) {
            return std::nullopt;
        }
        return result;
    };

    const OptInt year = my_stoi(yearStr);
    const OptInt month = MumeClock::getMumeMonth(mmqt::toQStringUtf8(monthStr.value()));
    const OptInt day = my_stoi(dayStr);
    const OptInt hour = my_stoi(hourStr);

    if (!year.has_value() || !month.has_value() || !day.has_value() || !hour.has_value()) {
        MMLOG_WARNING() << "invalid date values";
        return;
    }

    const auto msspTime = MsspTime{year.value(), month.value(), day.value(), hour.value()};

    auto warn_if_invalid = [](const std::string_view what, const int n, const int lo, const int hi) {
        if (n < lo || n > hi) {
            MMLOG_WARNING() << "invalid " << what << ": " << n;
        }
    };

    // MUME's official start is 2850, and the end is 3018 at the start of the fellowship.
    // However, the historical average reset time has been around 3023 (about a RL month late).
    //
    // (note: 3018 - 2850 = 168 game years = 1008 RL days = ~2.76 RL years, and
    //        3023 - 2850 = 173 game years = 1038 RL days = ~2.84 RL years).
    //
    // Let's err on the side of caution in case someone forgets to reset the time.
    const int max_rl_years = 6;
    const int mud_years_per_rl_year = MUME_MINUTES_PER_HOUR;
    const int max_year = MUME_START_YEAR + mud_years_per_rl_year * max_rl_years;

    // TODO: stronger validation of the integers here;
    warn_if_invalid("year", msspTime.year, MUME_START_YEAR, max_year);
    warn_if_invalid("month", msspTime.month, 0, MUME_MONTHS_PER_YEAR - 1);
    warn_if_invalid("day", msspTime.day, 0, MUME_DAYS_PER_MONTH - 1);
    warn_if_invalid("hour", msspTime.hour, 0, MUME_MINUTES_PER_HOUR - 1);

    m_outputs.onSendGameTimeToClock(msspTime);
}

// Helper to parse NEW-ENVIRON variables from a QByteArray
// This function is being moved to AbstractTelnet.cpp
// static std::map<QString, QString> parseNewEnvironVariables(const QByteArray &data, bool debug) {
//     std::map<QString, QString> variables;
//     QString currentVarName;
//     QString currentValue;
//     enum class State { VAR, VAL, ESC_IN_VAL };
//     State state = State::VAR;
//     bool nextIsVar = true; // Expect VAR or USERVAR first
//
//     for (int i = 0; i < data.size(); ++i) {
//         const auto byte = static_cast<uint8_t>(data[i]);
//         if (nextIsVar) {
//             if (byte == TNEV_VAR || byte == TNEV_USERVAR) {
//                 if (!currentVarName.isEmpty() && !currentValue.isEmpty()) {
//                     if (debug) {
//                         qDebug() << "Parsed NEW-ENVIRON variable" << currentVarName << "with value" << currentValue;
//                     }
//                     variables[currentVarName] = currentValue;
//                 }
//                 currentVarName.clear();
//                 currentValue.clear();
//                 state = State::VAR;
//                 nextIsVar = false;
//             } else {
//                 // Protocol error or unexpected data
//                 if (debug) {
//                     qDebug() << "NEW-ENVIRON parsing error: Expected VAR or USERVAR, got" << byte;
//                 }
//                 return variables; // Or handle error more gracefully
//             }
//             continue;
//         }
//
//         switch (state) {
//         case State::VAR:
//             if (byte == TNEV_VAL) {
//                 state = State::VAL;
//             } else if (byte == TNEV_ESC) {
//                 // According to RFC 1572, ESC is only for VAL, not VAR.
//                 // However, some clients might send it. We'll treat it as part of the name.
//                 currentVarName.append(QChar(byte));
//             } else {
//                 currentVarName.append(QChar(byte));
//             }
//             break;
//         case State::VAL:
//             if (byte == TNEV_ESC) {
//                 state = State::ESC_IN_VAL;
//             } else if (byte == TNEV_VAR || byte == TNEV_USERVAR) {
//                 // Start of a new variable definition
//                 if (!currentVarName.isEmpty()) {
//                      if (debug) {
//                         qDebug() << "Parsed NEW-ENVIRON variable" << currentVarName << "with value" << currentValue;
//                     }
//                     variables[currentVarName] = currentValue;
//                 }
//                 currentVarName.clear();
//                 currentValue.clear();
//                 state = State::VAR; // Set up for the new var name
//                 // Reprocess this byte as it's a TNEV_VAR/USERVAR
//                 i--;
//                 nextIsVar = true;
//             } else {
//                 currentValue.append(QChar(byte));
//             }
//             break;
//         case State::ESC_IN_VAL:
//             currentValue.append(QChar(byte));
//             state = State::VAL;
//             break;
//         }
//     }
//     if (!currentVarName.isEmpty()) {
//         if (debug) {
//             qDebug() << "Parsed NEW-ENVIRON variable" << currentVarName << "with value" << currentValue;
//         }
//         variables[currentVarName] = currentValue;
//     }
//     return variables;
// }

void MudTelnet::virt_receiveNewEnvironIs(const QByteArray &data) {
    if (getDebug()) {
        qDebug() << "MudTelnet: Received NEW-ENVIRON IS:" << data.toHex();
    }
    auto received_vars = AbstractTelnet::parseNewEnvironVariables(data, getDebug());
    for(auto const& [key, val] : received_vars) {
        if (getDebug()) {
            qDebug() << "MUD provided NEW-ENVIRON variable:" << key << "=" << val;
        }
        // Potentially store or act on these variables if needed.
        // For now, just logging.
    }
}

void MudTelnet::virt_receiveNewEnvironSend(const QByteArray &data) {
    if (getDebug()) {
        qDebug() << "MudTelnet: Received NEW-ENVIRON SEND:" << data.toHex();
    }

    // Parse requested variables
    QStringList requestedVars;
    QString currentVarName;
    bool expectVarType = true;
    for (char byte : data) {
        uint8_t u_byte = static_cast<uint8_t>(byte);
        if (expectVarType) {
            if (u_byte == TNEV_VAR || u_byte == TNEV_USERVAR) {
                // Valid start of a variable
                expectVarType = false;
                currentVarName.clear();
            } else {
                // Protocol error or "SEND VAR" to request all
                if (u_byte == TNEV_VAR && data.size() == 1) { // SEND VAR (request all)
                     requestedVars.append("*ALL*"); // Special marker for all
                } else if (data.isEmpty()){ // SEND (also request all as per RFC1572)
                     requestedVars.append("*ALL*");
                }
                else if (getDebug()) {
                    qDebug() << "NEW-ENVIRON SEND: Protocol error or unhandled request format.";
                }
                break;
            }
        } else {
            if (u_byte == TNEV_VAR || u_byte == TNEV_USERVAR) {
                // Start of a new variable in the SEND request
                if (!currentVarName.isEmpty()) {
                    requestedVars.append(currentVarName);
                }
                currentVarName.clear();
                // This byte is TNEV_VAR/USERVAR, so we are still expecting the var name bytes next
            } else {
                currentVarName.append(QChar(u_byte));
            }
        }
    }
    if (!currentVarName.isEmpty()) {
        requestedVars.append(currentVarName);
    }

    if (data.isEmpty() && !requestedVars.contains("*ALL*")) { // SEND (request all)
        requestedVars.append("*ALL*");
    }


    TelnetFormatter s{*this};
    s.addSubnegBegin(OPT_NEW_ENVIRON);
    s.addRaw(TNSB_IS);

    auto addVarVal = [&](const QString& var, const QString& val) {
        s.addRaw(TNEV_VAR);
        s.addEscapedBytes(var.toUtf8());
        s.addRaw(TNEV_VAL);
        s.addEscapedBytes(val.toUtf8());
    };

    bool wantsAll = requestedVars.contains("*ALL*");

    if (wantsAll || requestedVars.contains("CLIENT_NAME")) {
        addVarVal("CLIENT_NAME", "MMapper");
    }
    if (wantsAll || requestedVars.contains("CLIENT_VERSION")) {
        addVarVal("CLIENT_VERSION", getMMapperVersion());
    }
    if (wantsAll || requestedVars.contains("TERMINAL_TYPE")) {
        // The terminal type sent here should be the one reported by the actual MUD client,
        // potentially without MMapper's own suffix, or with it if that's what we want the MUD to see via MNES.
        // For now, send the one AbstractTelnet knows, which might include MMapper's suffix.
        addVarVal("TERMINAL_TYPE", QString::fromUtf8(getTerminalType().getQByteArray()));
    }
    if (wantsAll || requestedVars.contains("MTTS")) {
        int clientMtts = 0;
        if (!m_relayedClientMttsValue.isEmpty()) {
            bool ok;
            clientMtts = m_relayedClientMttsValue.toInt(&ok);
            if (!ok) clientMtts = 0;
        }
        int mmapperMtts = clientMtts;
        mmapperMtts |= MttsBits::PROXY;
        mmapperMtts |= MttsBits::MNES;
        mmapperMtts |= MttsBits::UTF_8;
        addVarVal("MTTS", QString::number(mmapperMtts));
    }
    // IPADDRESS: This is tricky. MudTelnet doesn't know the user's real IP.
    // UserTelnet would know it when the user connects to the proxy.
    // This needs to be relayed from UserTelnet to MudTelnet if we are to support it.
    // For now, we won't send it.
    if (wantsAll || requestedVars.contains("IPADDRESS")) {
        auto it = m_clientProvidedEnvironVariables.find("IPADDRESS");
        if (it != m_clientProvidedEnvironVariables.end()) {
            addVarVal("IPADDRESS", it->second);
        }
    }
     if (wantsAll || requestedVars.contains("CHARSET")) {
        auto it = m_clientProvidedEnvironVariables.find("CHARSET");
        if (it != m_clientProvidedEnvironVariables.end()) {
            addVarVal("CHARSET", it->second);
        }
    }
    // Add other MNES variables here if they are stored in m_clientProvidedEnvironVariables

    s.addSubnegEnd();
}

void MudTelnet::virt_receiveNewEnvironInfo(const QByteArray &data) {
    if (getDebug()) {
        qDebug() << "MudTelnet: Received NEW-ENVIRON INFO:" << data.toHex();
    }
    auto received_vars = AbstractTelnet::parseNewEnvironVariables(data, getDebug());
    for(auto const& [key, val] : received_vars) {
        if (getDebug()) {
            qDebug() << "MUD provided NEW-ENVIRON INFO variable:" << key << "=" << val;
        }
        // Potentially store or act on these variables if needed.
    }
}

void MudTelnet::onSetClientEnvironVariable(const QString &key, const QString &value) {
    if (getDebug()) {
        qDebug() << "MudTelnet: Setting client provided ENV var:" << key << "=" << value;
    }
    m_clientProvidedEnvironVariables[key] = value;

    // If the MUD has already indicated it WILL NEW-ENVIRON and we are DO NEW-ENVIRON,
    // we might want to send an unsolicited INFO update for certain variables if they change.
    // For example, if CHARSET changes mid-session.
    // For now, these are primarily used when the MUD sends a SEND request.
}

void MudTelnet::onSetMttsValue(const QString &mttsValue) {
    if (getDebug()) {
        qDebug() << "MudTelnet: Setting MTTS value from client:" << mttsValue;
    }
    m_clientProvidedEnvironVariables["MTTS"] = mttsValue; // For NEW-ENVIRON
    m_relayedClientMttsValue = mttsValue; // For TTYPE negotiation
    // Potentially send an IAC SB NEW-ENVIRON INFO VAR MTTS VAL <mttsValue> IAC SE
    // if NEW-ENVIRON is active with the MUD.
}

void MudTelnet::onRelayClientTerminalName(const TelnetTermTypeBytes &clientTerminalName) {
    if (getDebug()) {
        qDebug() << "MudTelnet: Received client's reported terminal name:" << clientTerminalName.getQByteArray().constData();
    }
    m_relayedClientTerminal = clientTerminalName;
}

void MudTelnet::reset() {
    AbstractTelnet::reset();
    m_ttypeToMudState = TtypeToMudState::Idle;
    m_clientProvidedEnvironVariables.clear();
    m_relayedClientName.clear();
    m_relayedClientTerminal.clear();
    m_relayedClientMttsValue.clear();
    m_lineBuffer.clear(); // Also reset line buffer
    // m_gmcp modules are reset by AbstractTelnet::reset() if it calls resetGmcpModules,
    // or MudTelnet should handle it if it has its own specific GMCP reset logic.
    // MudTelnet constructor calls resetGmcpModules(). AbstractTelnet::reset() does not.
    // So, we might need to call resetGmcpModules() here too if full reset is desired.
    // For now, let's stick to what onDisconnected does.
    if (getDebug()) {
        qDebug() << "MudTelnet: State reset.";
    }
}
void MudTelnet::virt_handleTerminalTypeSendRequest() {
    // This is called when the MUD sends IAC SB TTYPE SEND IAC SE.
    // We need to respond according to the MTTS sequence.

    if (getDebug()) {
        qDebug() << "MudTelnet: MUD requests TTYPE. State:" << static_cast<int>(m_ttypeToMudState);
    }

    QString mttsString; // Will hold "MTTS <val>"

    switch (m_ttypeToMudState) {
    case TtypeToMudState::Idle:
        // This implies MUD sent DO TTYPE, we sent WILL TTYPE, and now MUD sends first SEND.
        // Or, if TTYPE was already active and MUD sends SEND again (some MUDs might do this).
        // For simplicity, let's assume first SEND after WILL.
        m_ttypeToMudState = TtypeToMudState::AwaitingFirstSend;
        // Fall-through or handle as AwaitingFirstSend explicitly
    case TtypeToMudState::AwaitingFirstSend: {
        TelnetTermTypeBytes name_to_send = m_relayedClientName.isEmpty() ? TelnetTermTypeBytes{"MMAPPER"} : m_relayedClientName;
        if (getDebug()) {
            qDebug() << "MudTelnet: Sending client name to MUD:" << name_to_send.getQByteArray().constData();
        }
        sendTerminalType(name_to_send);
        m_ttypeToMudState = TtypeToMudState::SentClientName;
        break;
    }
    case TtypeToMudState::SentClientName: {
        TelnetTermTypeBytes term_to_send = m_relayedClientTerminal.isEmpty() ? TelnetTermTypeBytes{"XTERM"} : m_relayedClientTerminal;
        if (getDebug()) {
            qDebug() << "MudTelnet: Sending terminal name to MUD:" << term_to_send.getQByteArray().constData();
        }
        sendTerminalType(term_to_send);
        m_ttypeToMudState = TtypeToMudState::SentTerminalName;
        break;
    }
    case TtypeToMudState::SentTerminalName: {
        int clientMtts = 0;
        if (!m_relayedClientMttsValue.isEmpty()) {
            bool ok;
            clientMtts = m_relayedClientMttsValue.toInt(&ok);
            if (!ok) clientMtts = 0;
        }

        int mmapperMtts = clientMtts;
        mmapperMtts |= MttsBits::PROXY; // MMapper is a proxy
        mmapperMtts |= MttsBits::MNES;  // MMapper supports MNES
        mmapperMtts |= MttsBits::UTF_8; // MMapper forces UTF-8 to MUD

        // Potentially add other MMapper specific bits here if defined
        // mmapperMtts |= MttsBits::MMAPPER_CUSTOM_FEATURE_1;

        mttsString = QString("MTTS %1").arg(mmapperMtts);
        if (getDebug()) {
            qDebug() << "MudTelnet: Sending MTTS to MUD:" << mttsString;
        }
        sendTerminalType(TelnetTermTypeBytes{mttsString.toUtf8()});
        m_ttypeToMudState = TtypeToMudState::SentMtts;
        break;
    }
    case TtypeToMudState::SentMtts: {
        // Re-calculate or retrieve the previously sent MTTS string for confirmation
        int clientMtts = 0;
        if (!m_relayedClientMttsValue.isEmpty()) {
            bool ok;
            clientMtts = m_relayedClientMttsValue.toInt(&ok);
            if (!ok) clientMtts = 0;
        }
        int mmapperMtts = clientMtts;
        mmapperMtts |= MttsBits::PROXY;
        mmapperMtts |= MttsBits::MNES;
        mmapperMtts |= MttsBits::UTF_8;
        // mmapperMtts |= MttsBits::MMAPPER_CUSTOM_FEATURE_1;

        mttsString = QString("MTTS %1").arg(mmapperMtts);
        if (getDebug()) {
            qDebug() << "MudTelnet: Sending MTTS confirmation to MUD:" << mttsString;
        }
        sendTerminalType(TelnetTermTypeBytes{mttsString.toUtf8()});
        m_ttypeToMudState = TtypeToMudState::SentMttsConfirm; // Or Complete
        break;
    }
    case TtypeToMudState::SentMttsConfirm:
    case TtypeToMudState::Complete:
        // MUD requested TTYPE again after completion. Reset and start over or send last known MTTS.
        // For now, let's just resend the last known MTTS value.
        // This also handles cases where a MUD might only ask once or twice.
        // If we don't have an m_relayedClientMttsValue yet, we might send a basic "MTTS 0" or similar.
        if (getDebug()) {
            qDebug() << "MudTelnet: MUD requested TTYPE again after completion. Resending last known MTTS.";
        }
        int clientMtts = 0;
        if (!m_relayedClientMttsValue.isEmpty()) {
            bool ok;
            clientMtts = m_relayedClientMttsValue.toInt(&ok);
            if (!ok) clientMtts = 0;
        }
        int mmapperMtts = clientMtts;
        mmapperMtts |= MttsBits::PROXY;
        mmapperMtts |= MttsBits::MNES;
        mmapperMtts |= MttsBits::UTF_8;

        mttsString = QString("MTTS %1").arg(mmapperMtts);
        sendTerminalType(TelnetTermTypeBytes{mttsString.toUtf8()});
        // Stay in Complete or SentMttsConfirm state.
        break;
    }
}
