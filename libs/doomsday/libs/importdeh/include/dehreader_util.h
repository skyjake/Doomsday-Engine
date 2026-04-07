/** @file dehreader_util.h  DeHackEd patch parser.
 *
 * @ingroup dehread
 *
 * Miscellaneous utility routines.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef LIBDEHREAD_DEHREADER_UTIL_H
#define LIBDEHREAD_DEHREADER_UTIL_H

#include <de/string.h>
#include <doomsday/defs/mapinfo.h>
#include "dehreader.h"

/// @return Newly composed map URI.
res::Uri composeMapUri(int episode, int map);

int valueDefForPath(const de::String &id, ded_value_t **def = 0);

/**
 * Tokenize a @a string, splitting it into at most @a max tokens.
 *
 * For emulating the behavior of Team TNT's original DEH parser which often
 * uses atoi() for parsing the last number argument on a line.
 *
 * @param str       String to tokenize.
 * @param sep       Token separator character.
 * @param max       Maximum number of tokens to scan for in @a str.
 *                  If @c <0 there is no maximum and therefore the resultant
 *                  list will contain all found tokens (equivalent to using
 *                  de::String::split() ).
 *                  If zero the resultant list will be always be empty.
 *
 * @return Resulting StringList containing the parsed string tokens.
 */
de::StringList splitMax(const de::String &string, de::Char sep, int max = -1);

#endif // LIBDEHREAD_DEHREADER_UTIL_H
