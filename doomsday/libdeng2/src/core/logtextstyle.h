/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LIBDENG2_LOGTEXTSTYLE_H
#define LIBDENG2_LOGTEXTSTYLE_H

/**
 * @file logtextstyle.h
 * @internal Predefined text styles for log message text.
 */

namespace de {

enum LogTextStyle
{
    NORMAL,
    STRONG,
    LOG_TIME,
    LOG_LEVEL,
    LOG_BAD_LEVEL,
    LOG_SECTION,
    LOG_MSG
};

char const *TEXT_STYLE_NORMAL = "$0";
char const *TEXT_STYLE_STRONG = "$1";
char const *TEXT_STYLE_LOG_TIME = "$2";
char const *TEXT_STYLE_LOG_LEVEL = "$3";
char const *TEXT_STYLE_LOG_BAD_LEVEL = "$4";
char const *TEXT_STYLE_SECTION = "$5";
char const *TEXT_STYLE_MESSAGE = "$6";

}

#endif // LIBDENG2_LOGTEXTSTYLE_H
