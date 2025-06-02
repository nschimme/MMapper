// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "parserutils.h"

#include "../map/enums.h"
#include "../map/mmapper2room.h"   // For Mmapper2Exit::dirForChar
#include "../map/ExitDirection.h"  // For ::exitDir
#include "../map/coordinate.h"     // For Coordinate
#include "Charset.h"
#include "Consts.h" // Added for char_consts
#include "TextUtils.h"

#include <QString> // For QString
#include <QChar>   // For QChar
#include <cmath>   // For std::abs
#include <QRegularExpression>
#include <cassert> // For assert

namespace ParserUtils {

using namespace char_consts; // Added for char_consts

NODISCARD char getTerrainSymbol(const RoomTerrainEnum type)
{
    switch (type) {
    case RoomTerrainEnum::UNDEFINED:
        return C_SPACE;
    case RoomTerrainEnum::INDOORS:
        return C_OPEN_BRACKET; // [  // indoors
    case RoomTerrainEnum::CITY:
        return C_POUND_SIGN; // #  // city
    case RoomTerrainEnum::FIELD:
        return C_PERIOD; // .  // field
    case RoomTerrainEnum::FOREST:
        return 'f'; // f  // forest
    case RoomTerrainEnum::HILLS:
        return C_OPEN_PARENS; // (  // hills
    case RoomTerrainEnum::MOUNTAINS:
        return C_LESS_THAN; // <  // mountains
    case RoomTerrainEnum::SHALLOW:
        return C_PERCENT_SIGN; // %  // shallow
    case RoomTerrainEnum::WATER:
        return C_TILDE; // ~  // water
    case RoomTerrainEnum::RAPIDS:
        return 'W'; // W  // rapids
    case RoomTerrainEnum::UNDERWATER:
        return 'U'; // U  // underwater
    case RoomTerrainEnum::ROAD:
        return C_PLUS_SIGN; // +  // road
    case RoomTerrainEnum::TUNNEL:
        return C_EQUALS; // =  // tunnel
    case RoomTerrainEnum::CAVERN:
        return 'O'; // O  // cavern
    case RoomTerrainEnum::BRUSH:
        return C_COLON; // :  // brush
    }

    return C_SPACE;
}

NODISCARD char getLightSymbol(const RoomLightEnum lightType)
{
    switch (lightType) {
    case RoomLightEnum::DARK:
        return 'o';
    case RoomLightEnum::LIT:
    case RoomLightEnum::UNDEFINED:
        return C_ASTERISK;
    }

    return C_QUESTION_MARK;
}

NODISCARD QString compressDirections(const QString &original)
{
    QString ans;
    int curnum = 0;
    QChar curval = char_consts::C_NUL;
    Coordinate delta;
    const auto addDirs = [&ans, &curnum, &curval, &delta]() {
        assert(curnum >= 1);
        assert(curval != char_consts::C_NUL);
        if (curnum > 1) {
            ans.append(QString::number(curnum));
        }
        ans.append(curval);

        const auto dir = Mmapper2Exit::dirForChar(curval.toLatin1());
        delta += ::exitDir(dir) * curnum;
    };

    for (const QChar c : original) {
        if (curnum != 0 && curval == c) {
            ++curnum;
        } else {
            if (curnum != 0) {
                addDirs();
            }
            curnum = 1;
            curval = c;
        }
    }
    if (curnum != 0) {
        addDirs();
    }

    bool wantDelta = true;
    if (wantDelta) {
        auto addNumber =
            [&curnum, &curval, &addDirs, &ans](const int n, const char pos, const char neg) {
                if (n == 0) {
                    return;
                }
                curnum = std::abs(n);
                curval = (n < 0) ? neg : pos;
                ans += char_consts::C_SPACE;
                addDirs();
            };

        if (delta.isNull()) {
            ans += " (here)";
        } else {
            ans += " (total:";
            addNumber(delta.x, 'e', 'w');
            addNumber(delta.y, 'n', 's');
            addNumber(delta.z, 'u', 'd');
            ans += ")";
        }
    }

    return ans;
}

QString &removeAnsiMarksInPlace(QString &str)
{
    static const QRegularExpression ansi("\x1B\\[[0-9;:]*[A-Za-z]");
    str.remove(ansi);
    return str;
}

NODISCARD bool isWhitespaceNormalized(const std::string_view sv)
{
    bool last_was_space = false;
    for (char c : sv) {
        if (c == char_consts::C_SPACE) {
            if (last_was_space) {
                return false;
            } else {
                last_was_space = true;
            }
        } else if (ascii::isSpace(c)) {
            return false;
        } else {
            last_was_space = false;
        }
    }

    return true;
}

std::string normalizeWhitespace(std::string str)
{
    if (!isWhitespaceNormalized(str)) {
        const size_t len = str.size();
        bool last_was_space = false;
        size_t out = 0;
        for (size_t in = 0; in < len; ++in) {
            const char c = str[in];
            if (ascii::isSpace(c)) {
                if (!last_was_space) {
                    last_was_space = true;
                    str[out++] = char_consts::C_SPACE;
                }
            } else {
                last_was_space = false;
                str[out++] = c;
            }
        }
        str.resize(out);
        assert(isWhitespaceNormalized(str));
    }
    return str;
}

} // namespace ParserUtils
