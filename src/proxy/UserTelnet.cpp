// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "UserTelnet.h"

#include "../global/AnsiTextUtils.h"
#include "../global/CharUtils.h"
#include "../global/Charset.h"
#include "../global/Consts.h"
#include "../global/LineUtils.h"
#include "../global/TextUtils.h"
#include "../global/emojis.h"

#include <ostream>
#include <sstream>
#include <map>

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


// REVISIT: Does this belong somewhere else?
// REVISIT: Should this also normalize ANSI?
static void normalizeForUser(std::ostream &os, const bool goAhead, const std::string_view sv)
{
    // REVISIT: perform ANSI normalization in this function, too?
    foreachLine(sv, [&os, goAhead](std::string_view line) {
        if (line.empty()) {
            return;
        }

        // const bool hasAnsi = containsAnsi(line);
        const bool hasNewline = line.back() == char_consts::C_NEWLINE;
        trim_newline_inplace(line);

        if (!line.empty()) {
            using char_consts::C_CARRIAGE_RETURN;
            foreachCharMulti2(line, C_CARRIAGE_RETURN, [&os, goAhead](std::string_view txt) {
                // so it's only allowed to contain carriage returns if goAhead is true?
                // why not just remove all of the extra carriage returns?
                if (!txt.empty() && (txt.front() != C_CARRIAGE_RETURN || goAhead)) {
                    os << txt;
                }
            });
        }

        if (hasNewline) {
            // REVISIT: add an Ansi reset if the string doesn't contain one?
            // if (hasAnsi) {}
            os << string_consts::SV_CRLF;
        }
    });
}

NODISCARD static QString normalizeForUser(const CharacterEncodingEnum userEncoding,
                                          const QString &s,
                                          const bool goAhead)
{
    auto out = [goAhead, &userEncoding, &s]() -> std::string {
        std::ostringstream oss;
        normalizeForUser(oss,
                         goAhead,
                         mmqt::toStdStringUtf8((getConfig().parser.decodeEmoji
                                                && userEncoding == CharacterEncodingEnum::UTF8
                                                && s.contains(char_consts::C_COLON))
                                                   ? mmqt::decodeEmojiShortCodes(s)
                                                   : s));
        return std::move(oss).str();
    }();

    return mmqt::toQStringUtf8(out);
}

NODISCARD static RawBytes decodeFromUser(const CharacterEncodingEnum userEncoding,
                                         const RawBytes &raw)
{
    if (userEncoding == CharacterEncodingEnum::UTF8) {
        return raw;
    }
    std::ostringstream oss;
    charset::conversion::convert(oss,
                                 mmqt::toStdStringViewRaw(raw.getQByteArray()),
                                 userEncoding,
                                 CharacterEncodingEnum::UTF8);
    return RawBytes{mmqt::toQByteArrayUtf8(oss.str())};
}

UserTelnetOutputs::~UserTelnetOutputs() = default;

UserTelnet::UserTelnet(UserTelnetOutputs &outputs)
    : AbstractTelnet(TextCodecStrategyEnum::AUTO_SELECT_CODEC, TelnetTermTypeBytes{"unknown"})
    , m_outputs{outputs}
{}

void UserTelnet::onConnected()
{
    reset();
    resetGmcpModules();

    // Negotiate options
    requestTelnetOption(TN_DO, OPT_TERMINAL_TYPE);
    requestTelnetOption(TN_DO, OPT_NAWS);
    requestTelnetOption(TN_DO, OPT_CHARSET);
    // Most clients expect the server (i.e. MMapper) to send IAC WILL GMCP
    requestTelnetOption(TN_WILL, OPT_GMCP);
    // Request permission to replace IAC GA with IAC EOR
    requestTelnetOption(TN_WILL, OPT_EOR);
    // Initiate NEW-ENVIRON with the client
    requestTelnetOption(TN_DO, OPT_NEW_ENVIRON);
}

void UserTelnet::onAnalyzeUserStream(const TelnetIacBytes &data)
{
    onReadInternal(data);
}

void UserTelnet::onSendToUser(const QString &s, const bool goAhead)
{
    auto outdata = normalizeForUser(getEncoding(), s, goAhead);
    submitOverTelnet(outdata, goAhead);
}

void UserTelnet::onGmcpToUser(const GmcpMessage &msg)
{
    if (!getOptions().myOptionState[OPT_GMCP]) {
        return;
    }

    const auto name = msg.getName().getStdStringUtf8();
    const std::size_t found = name.find_last_of(char_consts::C_PERIOD);
    try {
        const GmcpModule mod{name.substr(0, found)};
        if (m_gmcp.modules.find(mod) != m_gmcp.modules.end()) {
            sendGmcpMessage(msg);
        }

    } catch (const std::exception &e) {
        qWarning() << "Message" << msg.toRawBytes() << "error because:" << e.what();
    }
}

void UserTelnet::virt_onNewEnvironEnabledByPeer() {
    // This is called when the client (peer) sends WILL NEW-ENVIRON
    // in response to our (UserTelnet, acting as server) DO NEW-ENVIRON.
    // Now we should request the standard MNES variables from the client.
    if (getDebug()) {
        qDebug() << "UserTelnet: Client enabled NEW-ENVIRON. Requesting standard variables.";
    }

    TelnetFormatter s{*this};
    s.addSubnegBegin(OPT_NEW_ENVIRON);
    s.addRaw(TNSB_SEND);
    // Request standard MNES variables
    s.addRaw(TNEV_VAR);
    s.addEscapedBytes("CLIENT_NAME");
    s.addRaw(TNEV_VAR);
    s.addEscapedBytes("CLIENT_VERSION");
    s.addRaw(TNEV_VAR);
    s.addEscapedBytes("TERMINAL_TYPE");
    s.addRaw(TNEV_VAR);
    s.addEscapedBytes("CHARSET");
    s.addRaw(TNEV_VAR);
    s.addEscapedBytes("IPADDRESS");
    s.addRaw(TNEV_VAR);
    s.addEscapedBytes("MTTS"); // For MTTS support later
    // Add any other variables MMapper might be interested in from the client.
    s.addSubnegEnd();
}
void UserTelnet::onSendMSSPToUser(const TelnetMsspBytes &data)
{
    if (!getOptions().myOptionState[OPT_MSSP]) {
        return;
    }

    sendMudServerStatus(data);
}

void UserTelnet::virt_sendToMapper(const RawBytes &data, const bool goAhead)
{
    m_outputs.onAnalyzeUserStream(decodeFromUser(getEncoding(), data), goAhead);
}

void UserTelnet::onRelayEchoMode(const bool isDisabled)
{
    sendTelnetOption(isDisabled ? TN_WONT : TN_WILL, OPT_ECHO);

    // REVISIT: This is he only non-const use of the options variable;
    // it could be refactored so the base class does the writes.
    m_options.myOptionState[OPT_ECHO] = !isDisabled;
    m_options.announcedState[OPT_ECHO] = true;
}

void UserTelnet::virt_receiveGmcpMessage(const GmcpMessage &msg)
{
    // Eat Core.Hello since MMapper sends its own to MUME
    if (msg.isCoreHello()) {
        return;
    }

    const bool requiresRewrite = msg.getJson()
                                 && (msg.isCoreSupportsAdd() || msg.isCoreSupportsSet()
                                     || msg.isCoreSupportsRemove())
                                 && msg.getJsonDocument().has_value()
                                 && msg.getJsonDocument()->getArray().has_value();

    if (!requiresRewrite) {
        m_outputs.onRelayGmcpFromUserToMud(msg);
        return;
    }

    // Eat Core.Supports.[Add|Set|Remove] and proxy a MMapper filtered subset
    // Handle the various messages
    if (msg.isCoreSupportsSet()) {
        resetGmcpModules();
    }
    if (const auto optArray = msg.getJsonDocument()->getArray()) {
        for (const auto &e : optArray.value()) {
            if (auto optString = e.getString()) {
                const auto &module = optString.value();
                try {
                    const GmcpModule mod{mmqt::toStdStringUtf8(module)};
                    receiveGmcpModule(mod, !msg.isCoreSupportsRemove());

                } catch (const std::exception &e) {
                    qWarning() << "Module" << module
                               << (msg.isCoreSupportsRemove() ? "remove" : "add")
                               << "error because:" << e.what();
                }
            }
        }
    }

    // Filter MMapper-internal GMCP modules to proxy on to MUME
    std::ostringstream oss;
    oss << "[ ";
    bool comma = false;
    for (const GmcpModule &mod : m_gmcp.modules) {
        // REVISIT: Are some MMapper supported modules not supposed to be filtered?
        if (mod.isSupported()) {
            continue;
        }
        if (comma) {
            oss << ", ";
        }
        oss << char_consts::C_DQUOTE << mod.toStdString() << char_consts::C_DQUOTE;
        comma = true;
    }
    oss << " ]";
    if (!comma) {
        if (getDebug()) {
            qDebug() << "All modules were supported or nothing was requested";
        }
        return;
    }

    const GmcpMessage filteredMsg(GmcpMessageTypeEnum::CORE_SUPPORTS_SET,
                                  GmcpJson{std::move(oss).str()});
    m_outputs.onRelayGmcpFromUserToMud(filteredMsg);
}

void UserTelnet::virt_receiveTerminalType(const TelnetTermTypeBytes &data)
{
    if (getDebug()) {
        qDebug() << "UserTelnet: Received TTYPE IS from client:" << data.getQByteArray().constData();
    }

    switch (m_ttypeState) {
    case TtypeState::AwaitingClientName:
        m_clientReportedName = data;
        if (getDebug()) {
            qDebug() << "UserTelnet: Client name:" << m_clientReportedName.getQByteArray().constData();
        }
        m_outputs.onRelayTermTypeFromUserToMud(m_clientReportedName); // Relay immediately
        m_ttypeState = TtypeState::AwaitingTerminalName;
        sendTerminalTypeRequest();
        break;
    case TtypeState::AwaitingTerminalName:
        m_clientReportedTerminal = data;
        if (getDebug()) {
            qDebug() << "UserTelnet: Client terminal name:" << m_clientReportedTerminal.getQByteArray().constData();
        }
        // We don't relay this raw terminal name directly if we're going to send MTTS.
        // MudTelnet will construct its TTYPE response based on this and MTTS.
        // However, it might be useful for MudTelnet to know it.
        // The existing onRelayTermTypeFromUserToMud is for the *first* response (client name).
        m_outputs.onClientTerminalNameReceived(m_clientReportedTerminal);
        m_ttypeState = TtypeState::AwaitingMtts;
        sendTerminalTypeRequest();
        break;
    case TtypeState::AwaitingMtts: {
        QString response = QString::fromUtf8(data.getQByteArray()).trimmed();
        if (response.startsWith("MTTS ", Qt::CaseInsensitive)) {
            QString mttsValStr = response.mid(5);
            bool ok;
            int mttsVal = mttsValStr.toInt(&ok);
            if (ok) {
                m_clientReportedMtts = mttsVal;
                if (getDebug()) {
                    qDebug() << "UserTelnet: Client MTTS value:" << m_clientReportedMtts;
                }
                m_outputs.onClientMttsValueReceived(mttsValStr); // Relay to Proxy/MudTelnet
                m_ttypeState = TtypeState::AwaitingMttsConfirm;
                sendTerminalTypeRequest();
            } else {
                if (getDebug()) {
                    qDebug() << "UserTelnet: Invalid MTTS value:" << mttsValStr;
                }
                // What to do on error? Stop TTYPE? Or assume no MTTS?
                m_ttypeState = TtypeState::Complete; // Or Idle to restart?
            }
        } else {
            // Client doesn't support MTTS, or sent something unexpected.
            // Treat TTYPE negotiation as complete for now.
            if (getDebug()) {
                qDebug() << "UserTelnet: Client response is not MTTS, TTYPE negotiation ends. Response:" << response;
            }
            m_ttypeState = TtypeState::Complete;
        }
        break;
    }
    case TtypeState::AwaitingMttsConfirm: {
        QString response = QString::fromUtf8(data.getQByteArray()).trimmed();
        // Client should send the same MTTS string again.
        if (response.startsWith("MTTS ", Qt::CaseInsensitive)) {
            QString mttsValStr = response.mid(5);
            bool ok;
            int mttsVal = mttsValStr.toInt(&ok);
            if (ok && mttsVal == m_clientReportedMtts) {
                if (getDebug()) {
                    qDebug() << "UserTelnet: Client confirmed MTTS value:" << m_clientReportedMtts;
                }
            } else {
                if (getDebug()) {
                    qDebug() << "UserTelnet: Client MTTS confirmation mismatch or invalid. Expected:" << m_clientReportedMtts << "Got:" << response;
                }
            }
        } else {
             if (getDebug()) {
                qDebug() << "UserTelnet: Client did not confirm MTTS. Response:" << response;
            }
        }
        m_ttypeState = TtypeState::Complete; // TTYPE cycle ends here
        // No more SENDs from our side for TTYPE to the client.
        break;
    }
    case TtypeState::Idle:
    case TtypeState::Complete:
        if (getDebug()) {
            qDebug() << "UserTelnet: Received TTYPE IS in unexpected state:" << static_cast<int>(m_ttypeState);
        }
        // Client might be sending unsolicited TTYPE IS. We can choose to ignore or process.
        // For now, ignore if not in an active negotiation sequence initiated by us.
        break;
    }
}

void UserTelnet::virt_receiveWindowSize(const int x, const int y)
{
    m_outputs.onRelayNawsFromUserToMud(x, y);
}

void UserTelnet::virt_sendRawData(const TelnetIacBytes &data)
{
    m_outputs.onSendToSocket(data);
}

bool UserTelnet::virt_isGmcpModuleEnabled(const GmcpModuleTypeEnum &name) const
{
    if (!getOptions().myOptionState[OPT_GMCP]) {
        return false;
    }

    return m_gmcp.supported[name] != DEFAULT_GMCP_MODULE_VERSION;
}

void UserTelnet::receiveGmcpModule(const GmcpModule &mod, const bool enabled)
{
    if (enabled) {
        if (!mod.hasVersion()) {
            throw std::runtime_error("missing version");
        }
        m_gmcp.modules.insert(mod);
        if (mod.isSupported()) {
            m_gmcp.supported[mod.getType()] = mod.getVersion();
        }

    } else {
        m_gmcp.modules.erase(mod);
        if (mod.isSupported()) {
            m_gmcp.supported[mod.getType()] = DEFAULT_GMCP_MODULE_VERSION;
        }
    }
}

void UserTelnet::resetGmcpModules()
{
    if (getDebug()) {
        qDebug() << "Clearing GMCP modules";
    }
#define X_CASE(UPPER_CASE, CamelCase, normalized, friendly) \
    m_gmcp.supported[GmcpModuleTypeEnum::UPPER_CASE] = DEFAULT_GMCP_MODULE_VERSION;
    XFOREACH_GMCP_MODULE_TYPE(X_CASE)
#undef X_CASE
    m_gmcp.modules.clear();
}

void UserTelnet::reset() {
    AbstractTelnet::reset();
    m_ttypeState = TtypeState::Idle;
    m_clientReportedName.clear();
    m_clientReportedTerminal.clear();
    m_clientReportedMtts = 0;
    m_clientEnvironVariables.clear();
    m_clientIpAddress.clear();
    if (getDebug()) {
        qDebug() << "UserTelnet: State reset.";
    }
}
void UserTelnet::virt_receiveNewEnvironIs(const QByteArray &data) {
    if (getDebug()) {
        qDebug() << "UserTelnet: Received NEW-ENVIRON IS:" << data.toHex();
    }
    auto client_vars = AbstractTelnet::parseNewEnvironVariables(data, getDebug());
    for(auto const& [key, val] : client_vars) {
        if (getDebug()) {
            qDebug() << "Client provided NEW-ENVIRON variable:" << key << "=" << val;
        }
        m_clientEnvironVariables[key] = val; // Store them
        if (key == "IPADDRESS") {
            m_clientIpAddress = val;
            m_outputs.onClientEnvironVariableReceived(key, val);
        } else if (key == "CHARSET") {
            m_outputs.onClientEnvironVariableReceived(key, val);
        } else if (key == "MTTS") {
            // This value needs to be communicated to MudTelnet.
            m_outputs.onClientMttsValueReceived(val);
        } else if (key == "CLIENT_NAME" || key == "CLIENT_VERSION" || key == "TERMINAL_TYPE") {
            // Other standard MNES variables reported by client
            m_outputs.onClientEnvironVariableReceived(key, val);
        }
        // Other variables are stored but not specially relayed unless MudTelnet requests them all.
    }
}

void UserTelnet::virt_receiveNewEnvironSend(const QByteArray &data) {
    if (getDebug()) {
        qDebug() << "UserTelnet: Received NEW-ENVIRON SEND from client:" << data.toHex();
    }
    // A client should not typically initiate a SEND unless we (as server) have already done DO/WILL.
    // If the client is requesting variables, these variables are conceptually for the MUD.
    // MMapper itself doesn't have many dynamic variables to offer via NEW-ENVIRON to the client,
    // other than what the MUD provides.
    // For now, we will log this. If specific proxy-level variables were to be exposed,
    // we could answer them here.
    // If the client is asking for MUD variables, this request should be proxied to MudTelnet.
    // This proxying part is more complex as MudTelnet would need to send a SEND to the MUD
    // and then relay the MUD's IS back to this UserTelnet, which then sends an IS to the client.
    // For V1, we will not support client SEND requests for MUD variables.

    // Example of how one might respond if MMapper had its own variables:
    // TelnetFormatter s{*this};
    // s.addSubnegBegin(OPT_NEW_ENVIRON);
    // s.addRaw(TNSB_IS);
    // if (requested_var == "MMAPPER_PROXY_VERSION") {
    //     s.addRaw(TNEV_VAR); s.addEscapedBytes("MMAPPER_PROXY_VERSION");
    //     s.addRaw(TNEV_VAL); s.addEscapedBytes(getMMapperVersion().toUtf8());
    // }
    // s.addSubnegEnd();
}

void UserTelnet::virt_onTerminalTypeEnabledByPeer() {
    // This is called when the client (peer) sends WILL TTYPE
    // in response to our (UserTelnet, acting as server) DO TTYPE.
    // Now we should send the first TTYPE SEND request.
    if (getDebug()) {
        qDebug() << "UserTelnet: Client enabled TERMINAL-TYPE. Requesting client name.";
    }
    m_ttypeState = TtypeState::AwaitingClientName;
    sendTerminalTypeRequest(); // AbstractTelnet::sendTerminalTypeRequest sends IAC SB TTYPE SEND IAC SE
}

void UserTelnet::virt_receiveNewEnvironInfo(const QByteArray &data) {
    if (getDebug()) {
        qDebug() << "UserTelnet: Received NEW-ENVIRON INFO:" << data.toHex();
    }
    // Treat INFO the same as IS for now, as it's an unsolicited update.
    auto client_vars = AbstractTelnet::parseNewEnvironVariables(data, getDebug());
     for(auto const& [key, val] : client_vars) {
        if (getDebug()) {
            qDebug() << "Client provided NEW-ENVIRON INFO variable:" << key << "=" << val;
        }
        m_clientEnvironVariables[key] = val;
        if (key == "IPADDRESS") {
            m_clientIpAddress = val;
            m_outputs.onClientEnvironVariableReceived(key, val);
        } else if (key == "CHARSET") {
            m_outputs.onClientEnvironVariableReceived(key, val);
        } else if (key == "MTTS") {
            m_outputs.onClientMttsValueReceived(val);
        } else if (key == "CLIENT_NAME" || key == "CLIENT_VERSION" || key == "TERMINAL_TYPE") {
            m_outputs.onClientEnvironVariableReceived(key, val);
        }
    }
}
