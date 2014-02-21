/** @file charsymbols.h  Character (Unicode) symbols.
 *
 * @authors Copyright © 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_CHARSYMBOLS_H
#define LIBDENG2_CHARSYMBOLS_H

/*
 * Unicode key symbols.
 */
#ifdef MACOSX
#  define DENG2_CHAR_CONTROL_KEY         "\u2318"
#  define DENG2_CHAR_SHIFT_KEY           "\u21e7"
#  define DENG2_CHAR_UP_ARROW            "\u2191"
#  define DENG2_CHAR_DOWN_ARROW          "\u2193"
#  define DENG2_CHAR_UP_DOWN_ARROW       DENG2_CHAR_UP_ARROW " / " DENG2_CHAR_DOWN_ARROW
#  define DENG2_CHAR_RIGHT_DOUBLEARROW   "\u21d2"
#  define DENG2_CHAR_MDASH               "\u2014"
#elif UNIX
#  define DENG2_CHAR_CONTROL_KEY         "Ctrl-"
#  define DENG2_CHAR_SHIFT_KEY           "\u21e7"
#  define DENG2_CHAR_UP_ARROW            "\u2191"
#  define DENG2_CHAR_DOWN_ARROW          "\u2193"
#  define DENG2_CHAR_UP_DOWN_ARROW       DENG2_CHAR_UP_ARROW " / " DENG2_CHAR_DOWN_ARROW
#  define DENG2_CHAR_RIGHT_DOUBLEARROW   "\u21d2"
#  define DENG2_CHAR_MDASH               "\u2014"
#else
#  define DENG2_CHAR_CONTROL_KEY         "Ctrl-"
#  define DENG2_CHAR_SHIFT_KEY           "Shift-"
#  define DENG2_CHAR_UP_ARROW            "Up Arrow"
#  define DENG2_CHAR_DOWN_ARROW          "Down Arrow"
#  define DENG2_CHAR_UP_DOWN_ARROW       "Up/Down Arrow"
#  define DENG2_CHAR_RIGHT_DOUBLEARROW   "=>"
#  define DENG2_CHAR_MDASH               "-"
#endif

#endif // LIBDENG2_CHARSYMBOLS_H
