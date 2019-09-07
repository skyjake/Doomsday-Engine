/** @file strutil.h String and text utilities.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DE_STRING_UTIL_H
#define DE_STRING_UTIL_H

#include "types.h"
#include <stdarg.h>

/// @addtogroup legacyData
/// @{

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
DE_PUBLIC int dd_vsnprintf(char *str, size_t size, const char *format, va_list ap);

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
DE_PUBLIC int dd_snprintf(char *str, size_t size, const char *format, ...);

#ifdef WIN32
/**
 * Windows implementation for the Unix strcasestr() function.
 */
DE_PUBLIC const char *strcasestr(const char *text, const char *sub);
#endif

#ifdef UNIX
// Some routines not available on the *nix platform.
DE_PUBLIC char *strupr(char *string);
DE_PUBLIC char *strlwr(char *string);
#endif // UNIX

// String Utilities

DE_PUBLIC const char *M_SkipWhite(const char *str);

DE_PUBLIC const char *M_FindWhite(const char *str);

DE_PUBLIC void M_StripLeft(char* str);

DE_PUBLIC void M_StripRight(char* str, size_t len);

DE_PUBLIC void M_Strip(char* str, size_t len);

DE_PUBLIC char* M_SkipLine(char* str);

DE_PUBLIC char* M_StrCat(char* buf, const char* str, size_t bufSize);

DE_PUBLIC char* M_StrnCat(char* buf, const char* str, size_t nChars, size_t bufSize);

/**
 * Concatenates src to dest as a quoted string. " is escaped to \".
 * Returns dest.
 */
DE_PUBLIC char* M_StrCatQuoted(char* dest, const char* src, size_t len);

DE_PUBLIC char* M_LimitedStrCat(char* buf, const char* str, size_t maxWidth, char separator, size_t bufLength);

/**
 * Somewhat similar to strtok().
 */
DE_PUBLIC char* M_StrTok(char** cursor, const char *delimiters);

DE_PUBLIC char* M_TrimmedFloat(float val);

DE_PUBLIC void M_ForceUppercase(char *text);

/// @return  @c true if @a string can be interpreted as a valid integer.
DE_PUBLIC dd_bool M_IsStringValidInt(const char* str);

/// @return  @c true if @a string can be interpreted as a valid byte.
DE_PUBLIC dd_bool M_IsStringValidByte(const char* str);

/// @return  @c true if @a string can be interpreted as a valid floating-point value.
DE_PUBLIC dd_bool M_IsStringValidFloat(const char* str);

/// @}

#ifdef __cplusplus
} // extern "C"
#endif

#endif // DE_STRING_UTIL_H
