/** @file charsymbols.h  Character (Unicode) symbols.
 *
 * @authors Copyright © 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef LIBCORE_CHARSYMBOLS_H
#define LIBCORE_CHARSYMBOLS_H

#include "libcore.h"

/**
 * @defgroup unicodeSymbols Unicode Symbols
 * Unicode key symbols. @ingroup types
 * @{
 */
#define DE_CHAR_MDASH               "\u2014"
#define DE_CHAR_COPYRIGHT           "\u00a9"
#define DE_CHAR_UP_ARROW            "\u2191"
#define DE_CHAR_DOWN_ARROW          "\u2193"
#define DE_CHAR_UP_DOWN_ARROW       DE_CHAR_UP_ARROW " / " DE_CHAR_DOWN_ARROW
#define DE_CHAR_RIGHT_DOUBLEARROW   "\u21d2"

#ifdef MACOSX
#  define DE_CHAR_MAC_COMMAND_KEY   "\u2318"
#  define DE_CHAR_MAC_CONTROL_KEY   "\u2303"
#  define DE_CHAR_CONTROL_KEY       DE_CHAR_MAC_COMMAND_KEY
#  define DE_CHAR_SHIFT_KEY         "\u21e7"
#  define DE_CHAR_ALT_KEY           "\u2325"
#elif UNIX
#  define DE_CHAR_CONTROL_KEY       "Ctrl-"
#  define DE_CHAR_SHIFT_KEY         "\u21e7"
#  define DE_CHAR_ALT_KEY           "Alt-"
#else
#  define DE_CHAR_CONTROL_KEY       "Ctrl-"
#  define DE_CHAR_SHIFT_KEY         "Shift-"
#  define DE_CHAR_ALT_KEY           "Alt-"
#endif
/** @} */

namespace de {

/**
 * Converts the IBM PC character set to the Unicode equivalents.
 * See: https://en.wikipedia.org/wiki/Code_page_437
 *
 * @param code  Character code.
 *
 * @return Unicode character.
 */
DE_PUBLIC duint codePage437ToUnicode(dbyte code);

} // namespace de

#endif // LIBCORE_CHARSYMBOLS_H
