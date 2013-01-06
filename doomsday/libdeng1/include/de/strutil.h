/** @file strutil.h String and text utilities.
 * @ingroup base
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef LIBDENG_STRING_UTIL_H
#define LIBDENG_STRING_UTIL_H

#include "types.h"
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Prints a formatted string into a fixed-size buffer. At most @c size
 * characters will be written to the output buffer @c str. The output will
 * always contain a terminating null character.
 *
 * @param str     Output buffer.
 * @param size    Size of the output buffer.
 * @param format  Format of the output.
 * @param ap      Variable-size argument list.
 *
 * @return  Number of characters written to the output buffer if lower than or
 * equal to @c size, else @c -1.
 */
DENG_PUBLIC int dd_vsnprintf(char *str, size_t size, char const *format, va_list ap);

/**
 * Prints a formatted string into a fixed-size buffer. At most @c size
 * characters will be written to the output buffer @c str. The output will
 * always contain a terminating null character.
 *
 * @param str     Output buffer.
 * @param size    Size of the output buffer.
 * @param format  Format of the output.
 *
 * @return        Number of characters written to the output buffer
 *                if lower than or equal to @c size, else @c -1.
 */
DENG_PUBLIC int dd_snprintf(char *str, size_t size, char const *format, ...);

#ifdef UNIX
// Some routines not available on the *nix platform.
DENG_PUBLIC char *strupr(char *string);
DENG_PUBLIC char *strlwr(char *string);
#endif // UNIX

// String Utilities

DENG_PUBLIC char* M_SkipWhite(char* str);

DENG_PUBLIC char* M_FindWhite(char* str);

DENG_PUBLIC void M_StripLeft(char* str);

DENG_PUBLIC void M_StripRight(char* str, size_t len);

DENG_PUBLIC void M_Strip(char* str, size_t len);

DENG_PUBLIC char* M_SkipLine(char* str);

DENG_PUBLIC char* M_StrCat(char* buf, const char* str, size_t bufSize);

DENG_PUBLIC char* M_StrnCat(char* buf, const char* str, size_t nChars, size_t bufSize);

/**
 * Concatenates src to dest as a quoted string. " is escaped to \".
 * Returns dest.
 */
DENG_PUBLIC char* M_StrCatQuoted(char* dest, const char* src, size_t len);

DENG_PUBLIC char* M_LimitedStrCat(char* buf, const char* str, size_t maxWidth, char separator, size_t bufLength);

/**
 * Somewhat similar to strtok().
 */
DENG_PUBLIC char* M_StrTok(char** cursor, const char *delimiters);

DENG_PUBLIC char* M_TrimmedFloat(float val);

DENG_PUBLIC boolean M_IsComment(const char* text);

DENG_PUBLIC void M_ForceUppercase(char *text);

/// @return  @c true if @a string can be interpreted as a valid integer.
DENG_PUBLIC boolean M_IsStringValidInt(const char* str);

/// @return  @c true if @a string can be interpreted as a valid byte.
DENG_PUBLIC boolean M_IsStringValidByte(const char* str);

/// @return  @c true if @a string can be interpreted as a valid floating-point value.
DENG_PUBLIC boolean M_IsStringValidFloat(const char* str);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBDENG_STRING_UTIL_H
