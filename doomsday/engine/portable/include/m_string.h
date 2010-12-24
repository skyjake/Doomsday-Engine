/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2008-2010 Daniel Swanson <danij@dengine.net>
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, 
 * Boston, MA  02110-1301  USA
 */

/**
 * m_string.h: Dynamic Strings.
 */

#ifndef LIBDENG_STRING_H
#define LIBDENG_STRING_H

// Format checking for Str_Appendf in GCC2
#if defined(__GNUC__) && __GNUC__ >= 2
#   define PRINTF_F(f,v) __attribute__ ((format (printf, f, v)))
#else
#   define PRINTF_F(f,v)
#endif

/**
 * Dynamic String.
 *
 * \note: You can init with static string constants, for example:
 * ddstring_t mystr = { "Hello world." };
 */
typedef struct ddstring_s {
    /// String buffer.
	char* str;

    /// String length (no terminating nulls).
    size_t length;

    /// Allocated buffer size (note: not necessarily equal to ddstring_t::length).
    size_t size;
} ddstring_t;

void            Str_Init(ddstring_t* ds);
void            Str_Free(ddstring_t* ds);
ddstring_t*     Str_New(void);
void            Str_Delete(ddstring_t* ds);
void            Str_Clear(ddstring_t* ds);
void            Str_Reserve(ddstring_t* ds, size_t length);
void            Str_Set(ddstring_t* ds, const char* text);
void            Str_Append(ddstring_t* ds, const char* appendText);
void            Str_AppendChar(ddstring_t* ds, char ch);
void            Str_Appendf(ddstring_t* ds, const char* format, ...) PRINTF_F(2,3);
void            Str_PartAppend(ddstring_t* dest, const char* src, size_t start, size_t count);
void            Str_Prepend(ddstring_t* ds, const char* prependText);
size_t          Str_Length(const ddstring_t* ds);
char*           Str_Text(const ddstring_t* ds);
void            Str_Copy(ddstring_t* dest, const ddstring_t* src);
size_t          Str_StripLeft(ddstring_t* ds);
size_t          Str_StripRight(ddstring_t* ds);
void            Str_Strip(ddstring_t* ds);
const char*     Str_GetLine(ddstring_t* ds, const char* src);

/**
 * @defGroup copyDelimiterFlags Copy Delimiter Flags.
 */
/*@{*/
#define CDF_OMIT_DELIMITER      0x1 // Do not copy delimiters into the dest path.
#define CDF_OMIT_WHITESPACE     0x2 // Do not copy whitespace into the dest path.
/*@}*/

/**
 * Copies characters from @a src to @a dest until a @a delimiter character is encountered.
 *
 * @param dest          Destination string.
 * @param src           Source string.
 * @param delimiter     Delimiter character.
 * @param flags         @see copyDelimiterFlags.
 *
 * @return              Pointer to the character within @a src where copy stopped
 *                      else @c NULL if the end was reached.
 */
const char* Str_CopyDelim2(ddstring_t* dest, const char* src, char delimiter, int cdflags);
const char* Str_CopyDelim(ddstring_t* dest, const char* src, char delimiter);

int             Str_CompareIgnoreCase(const ddstring_t* ds, const char* text);
char            Str_At(const ddstring_t* str, size_t index);
char            Str_RAt(const ddstring_t* str, size_t reverseIndex);

#endif /* LIBDENG_STRING_H */
