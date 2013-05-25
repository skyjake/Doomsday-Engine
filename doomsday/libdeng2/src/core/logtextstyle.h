/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/libdeng2.h"

/**
 * @file logtextstyle.h
 * @internal Predefined text styles for log message text.
 */

namespace de {

char const *TEXT_STYLE_NORMAL          = DENG2_STR_ESCAPE("0"); // normal
char const *TEXT_STYLE_MESSAGE         = DENG2_STR_ESCAPE("0"); // normal
char const *TEXT_STYLE_BAD_MESSAGE     = DENG2_STR_ESCAPE("1"); // major
char const *TEXT_STYLE_DEBUG_MESSAGE   = DENG2_STR_ESCAPE("2"); // minor
char const *TEXT_STYLE_SECTION         = DENG2_STR_ESCAPE("3"); // meta
char const *TEXT_STYLE_BAD_SECTION     = DENG2_STR_ESCAPE("4"); // major meta
char const *TEXT_STYLE_DEBUG_SECTION   = DENG2_STR_ESCAPE("5"); // minor meta
char const *TEXT_STYLE_LOG_TIME        = DENG2_STR_ESCAPE("6"); // aux meta
char const *TEXT_MARK_INDENT           = DENG2_STR_ESCAPE(">");

} // namespace de

#endif // LIBDENG2_LOGTEXTSTYLE_H
