#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2002-2005 by Tomas Mecir - kmuddy@kmuddy.com
// Copyright (C) 2019 The MMapper Authors
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "../global/Array.h"
#include "GmcpMessage.h"
#include "GmcpModule.h"
#include "TaggedBytes.h"
#include "TextCodec.h"

#include <cassert>
#include <cstdint>

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QtCore>

// telnet command codes (prefixed with TN_ to prevent duplicit #defines
static constexpr const uint8_t TN_EOR = 239;
static constexpr const uint8_t TN_SE = 240;
static constexpr const uint8_t TN_NOP = 241;
static constexpr const uint8_t TN_DM = 242;
static constexpr const uint8_t TN_B = 243;
static constexpr const uint8_t TN_IP = 244;
static constexpr const uint8_t TN_AO = 245;
static constexpr const uint8_t TN_AYT = 246;
static constexpr const uint8_t TN_EC = 247;
static constexpr const uint8_t TN_EL = 248;
static constexpr const uint8_t TN_GA = 249;
static constexpr const uint8_t TN_SB = 250;
static constexpr const uint8_t TN_WILL = 251;
static constexpr const uint8_t TN_WONT = 252;
static constexpr const uint8_t TN_DO = 253;
static constexpr const uint8_t TN_DONT = 254;
static constexpr const uint8_t TN_IAC = 255;

// telnet option codes (supported options only)
static constexpr const uint8_t OPT_ECHO = 1;
static constexpr const uint8_t OPT_SUPPRESS_GA = 3;
static constexpr const uint8_t OPT_STATUS = 5;
static constexpr const uint8_t OPT_TIMING_MARK = 6;
static constexpr const uint8_t OPT_TERMINAL_TYPE = 24;
static constexpr const uint8_t OPT_EOR = 25;
static constexpr const uint8_t OPT_NAWS = 31;
static constexpr const uint8_t OPT_LINEMODE = 34;
static constexpr const uint8_t OPT_CHARSET = 42;
static constexpr const uint8_t OPT_MSSP = 70;
static constexpr const uint8_t OPT_COMPRESS2 = 86;
static constexpr const uint8_t OPT_NEW_ENVIRON = 39; // RFC 1572
static constexpr const uint8_t OPT_GMCP = 201;

// telnet SB suboption types
static constexpr const uint8_t TNSB_IS = 0;
static constexpr const uint8_t TNSB_SEND = 1;
static constexpr const uint8_t TNSB_REQUEST = 1;
static constexpr const uint8_t TNSB_MODE = 1;
static constexpr const uint8_t TNSB_EDIT = 1;
static constexpr const uint8_t TNSB_MSSP_VAR = 1;
static constexpr const uint8_t TNSB_MSSP_VAL = 2;
static constexpr const uint8_t TNSB_ACCEPTED = 2;
static constexpr const uint8_t TNSB_REJECTED = 3;
static constexpr const uint8_t TNSB_TTABLE_IS = 4;
static constexpr const uint8_t TNSB_TTABLE_REJECTED = 5;
static constexpr const uint8_t TNSB_TTABLE_ACK = 6;
static constexpr const uint8_t TNSB_TTABLE_NAK = 7;

// NEW-ENVIRON subnegotiation commands (RFC 1572)
static constexpr const uint8_t TNSB_IS = 0; // Re-used by TTYPE, NEW-ENVIRON
static constexpr const uint8_t TNSB_SEND = 1; // Re-used by TTYPE, NEW-ENVIRON
static constexpr const uint8_t TNSB_INFO = 2;

// NEW-ENVIRON variable types (RFC 1572)
static constexpr const uint8_t TNEV_VAR = 0;
static constexpr const uint8_t TNEV_VAL = 1;
static constexpr const uint8_t TNEV_ESC = 2;
static constexpr const uint8_t TNEV_USERVAR = 3;

// MTTS (Mud Terminal Type Standard) Bits (from https://tintin.mudhalla.net/protocols/mtts/)
namespace MttsBits {
    static constexpr const int ANSI = 1 << 0;              // Standard ANSI codes
    static constexpr const int VT100 = 1 << 1;             // VT100 codes
    static constexpr const int UTF_8 = 1 << 2;             // UTF-8 character encoding
    static constexpr const int COLORS_256 = 1 << 3;        // 256 colors
    static constexpr const int MOUSE_TRACKING = 1 << 4;    // Xterm mouse tracking
    static constexpr const int OSC_COLOR_PALETTE = 1 << 5; // OSC color palette
    static constexpr const int SCREEN_READER = 1 << 6;     // Using a screen reader
    static constexpr const int PROXY = 1 << 7;             // Client is a proxy
    static constexpr const int TRUECOLOR = 1 << 8;         // Truecolor (24-bit)
    static constexpr const int MNES = 1 << 9;              // MNES support
    static constexpr const int MSLP = 1 << 10;             // MSLP support
    static constexpr const int SSL = 1 << 11;              // SSL/TLS support

    // MMapper specific capabilities (start from a higher bit to avoid collision)
    // Example:
    // static constexpr const int MMAPPER_CUSTOM_FEATURE_1 = 1 << 16;
    // static constexpr const int MMAPPER_CUSTOM_FEATURE_2 = 1 << 17;
} // namespace MttsBits

// Forward declaration
namespace std { template<typename K, typename V> class map; }
class QString;
class QByteArray;

struct NODISCARD AppendBuffer : public RawBytes
{
    explicit AppendBuffer(RawBytes &&rhs)
        : RawBytes{std::move(rhs)}
    {}
    explicit AppendBuffer(const RawBytes &rhs)
        : RawBytes{rhs}
    {}

    using RawBytes ::RawBytes;
    using RawBytes ::size;
    using RawBytes ::operator=;

    void append(const uint8_t c) { RawBytes::append(static_cast<char>(c)); }
    void operator+=(const uint8_t c) { RawBytes::operator+=(static_cast<char>(c)); }

    void reserve(const int at_least)
    {
        assert(at_least >= 0);
        RawBytes::getQByteArray().reserve(at_least);
    }

    NODISCARD unsigned char unsigned_at(int pos) const
    {
        assert(size() > pos);
        return static_cast<unsigned char>(RawBytes::at(pos));
    }
};

struct TelnetFormatter;
class NODISCARD AbstractTelnet
{
private:
    friend TelnetFormatter;

protected:
    static constexpr const size_t NUM_OPTS = 256;
    using OptionArray = MMapper::Array<bool, NUM_OPTS>;

    struct NODISCARD Options final
    {
        /** current state of options on our side and on server side */
        OptionArray myOptionState;
        OptionArray hisOptionState;
        /** whether we have announced WILL/WON'T for that option (if we have, we don't
        respond to DO/DON'T sent by the server -- see implementation and RFC 854
        for more information... */
        OptionArray announcedState;
        /** whether the server has already announced his WILL/WON'T */
        OptionArray heAnnouncedState;
        /** whether we tried to request */
        OptionArray triedToEnable;

        void reset();
    };
    struct NODISCARD NawsData final
    {
        int width = 80;
        int height = 24;
    };

protected:
    Options m_options{};
    NawsData m_currentNaws{};

private:
    int64_t m_sentBytes = 0;

private:
    /* Terminal Type */
    const TelnetTermTypeBytes m_defaultTermType;
    TelnetTermTypeBytes m_termType;

    /** amount of bytes sent up to now */
    TextCodec m_textCodec;
    AppendBuffer m_commandBuffer;
    AppendBuffer m_subnegBuffer;

    struct ZstreamPimpl;
    std::unique_ptr<ZstreamPimpl> m_zstream_pimpl;

    enum class NODISCARD TelnetStateEnum : uint8_t {
        /// normal input
        NORMAL,
        /// received IAC
        IAC,
        /// received IAC <WILL|WONT|DO|DONT>
        COMMAND,
        /// received IAC SB
        SUBNEG,
        /// received IAC SB ... IAC
        SUBNEG_IAC,
        /// received IAC SB ... IAC <WILL|WONT|DO|DONT>
        SUBNEG_COMMAND
    };

    TelnetStateEnum m_state = TelnetStateEnum::NORMAL;
    // false if the other side instructed us not to echo
    bool m_echoMode = true;
    /** have we received the GA signal? */
    bool m_recvdGA = false;
    bool m_inflateTelnet = false;
    bool m_recvdCompress = false;
    bool m_debug = false;

public:
    explicit AbstractTelnet(TextCodecStrategyEnum strategy, TelnetTermTypeBytes defaultTermType);
    virtual ~AbstractTelnet();

protected:
    NODISCARD const Options &getOptions() const { return m_options; }
    NODISCARD bool getDebug() const { return m_debug; }

public:
    NODISCARD TelnetTermTypeBytes getTerminalType() const { return m_termType; }
    /* unused */
    NODISCARD int64_t getSentBytes() const { return m_sentBytes; }

    NODISCARD bool isGmcpModuleEnabled(const GmcpModuleTypeEnum &name) const
    {
        return virt_isGmcpModuleEnabled(name);
    }

    // false if the other side instructed us not to echo
    NODISCARD bool getEchoMode() const { return m_echoMode; }

protected:
    void sendCharsetRequest();
    void sendTerminalType(const TelnetTermTypeBytes &terminalType);
    void sendCharsetRejected();
    void sendCharsetAccepted(const TelnetCharsetBytes &characterSet);
    void sendOptionStatus();
    void sendWindowSizeChanged(int, int);
    void sendTerminalTypeRequest();
    void sendGmcpMessage(const GmcpMessage &msg);
    void sendMudServerStatus(const TelnetMsspBytes &);
    void sendLineModeEdit();
    void requestTelnetOption(unsigned char type, unsigned char subnegBuffer);

    /** performs charset conversion and doubles IACs */
    void submitOverTelnet(const QString &s, bool goAhead);
    /** doubles IACs; input must be in the correct charset  */
    void submitOverTelnet(const RawBytes &s, bool goAhead);

private:
    void trySendGoAhead();
    void sendWithDoubledIacs(const RawBytes &raw);

private:
    NODISCARD virtual bool virt_isGmcpModuleEnabled(const GmcpModuleTypeEnum &) const
    {
        return false;
    }
    virtual void virt_onGmcpEnabled() {}
    virtual void virt_onNewEnvironEnabledByPeer() {} // Called when peer sends WILL to our DO for NEW-ENVIRON
    virtual void virt_onTerminalTypeEnabledByPeer() {} // Called when peer sends WILL to our DO for TTYPE
    virtual void virt_handleTerminalTypeSendRequest() { sendTerminalType(m_termType); } // Peer sent SB TTYPE SEND; default is to send our m_termType
    virtual void virt_receiveEchoMode(bool) {}
    virtual void virt_receiveGmcpMessage(const GmcpMessage &) {}
    virtual void virt_receiveTerminalType(const TelnetTermTypeBytes &) {}
    virtual void virt_receiveMudServerStatus(const TelnetMsspBytes &) {}
    virtual void virt_receiveWindowSize(int, int) {}
    virtual void virt_receiveNewEnvironIs(const QByteArray &) {}
    virtual void virt_receiveNewEnvironSend(const QByteArray &) {}
    virtual void virt_receiveNewEnvironInfo(const QByteArray &) {}
    virtual void virt_sendRawData(const TelnetIacBytes &data) = 0;
    virtual void virt_sendToMapper(const RawBytes &, bool goAhead) = 0;

protected:
    void onGmcpEnabled() { virt_onGmcpEnabled(); }
    void receiveEchoMode(const bool b)
    {
        m_echoMode = b;
        virt_receiveEchoMode(b);
    }
    void receiveGmcpMessage(const GmcpMessage &msg) { virt_receiveGmcpMessage(msg); }
    void receiveTerminalType(const TelnetTermTypeBytes &ba) { virt_receiveTerminalType(ba); }
    void receiveMudServerStatus(const TelnetMsspBytes &ba) { virt_receiveMudServerStatus(ba); }
    void receiveWindowSize(int x, int y) { virt_receiveWindowSize(x, y); }

    /// Send out the data. Does not double IACs, this must be done
    /// by caller if needed. This function is suitable for sending
    /// telnet sequences.
    void sendRawData(const TelnetIacBytes &ba)
    {
        m_sentBytes += ba.length();
        virt_sendRawData(ba);
    }
    void sendRawData(const char *s) = delete;

private:
    // DEPRECATED_MSG("use STL functions")
    void sendRawData(const QByteArray &arr) = delete;
    void sendRawData(const QString &s) = delete;

protected:
    void sendToMapper(const RawBytes &ba, const bool goAhead) { virt_sendToMapper(ba, goAhead); }

protected:
    /** send a telnet option */
    void sendTelnetOption(unsigned char type, unsigned char subnegBuffer);
    virtual void reset();
    void onReadInternal(const TelnetIacBytes &);
    void setTerminalType(const TelnetTermTypeBytes &terminalType) { m_termType = terminalType; }

    NODISCARD CharacterEncodingEnum getEncoding() const { return m_textCodec.getEncoding(); }

protected: // Static helpers
    static std::map<QString, QString> parseNewEnvironVariables(const QByteArray &data, bool isDebugEnabled);

private:
    void onReadInternal2(AppendBuffer &, uint8_t);

    /** processes a telnet command (IAC ...) */
    void processTelnetCommand(const AppendBuffer &command);

    /** processes a telnet subcommand payload */
    void processTelnetSubnegotiation(const AppendBuffer &payload);

private:
    NODISCARD int onReadInternalInflate(const char *, int, AppendBuffer &);
    void resetCompress();
    void processGA(AppendBuffer &cleanData);
};
