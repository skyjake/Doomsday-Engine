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

#ifndef LIBDENG_API_STRING_H
#define LIBDENG_API_STRING_H

#include <stddef.h>

/**
 * Dynamic String. Simple dynamic string management.
 *
 * You can init with static string constants, for example:
 *      ddstring_t mystr = { "Hello world." };
 *
 * \note Uses de::Zone or standard malloc for memory allocation, chosen during
 *       initialization of a string. The string itself is always allocated with
 *       malloc. The Zone is not thread-safe.
 *
 * \todo Derive from Qt::QString
 * \todo Make this opaque for better forward compatibility -- prevents initialization
 *       with static C strings, though.
 */
typedef struct ddstring_s {
    /// String buffer.
	char* str;

    /// String length (no terminating nulls).
    size_t length;

    /// Allocated buffer size (note: not necessarily equal to ddstring_t::length).
    size_t size;

	// Memory management.
	void (*memFree)(void*);
	void* (*memAlloc)(size_t n);
	void* (*memCalloc)(size_t n);
} ddstring_t;

// Format checking for Str_Appendf in GCC2
#if defined(__GNUC__) && __GNUC__ >= 2
#   define PRINTF_F(f,v) __attribute__ ((format (printf, f, v)))
#else
#   define PRINTF_F(f,v)
#endif

/**
 * Allocate a new uninitialized string. Use Str_Delete() to destroy
 * the returned string. Memory for the string is allocated with de::Zone.
 *
 * @return New ddstring_t instance.
 *
 * @see Str_Delete
 */
ddstring_t* Str_New(void);

/**
 * Allocate a new uninitialized string. Use Str_Delete() to destroy
 * the returned string. Memory for the string is allocated with malloc.
 *
 * @return New ddstring_t instance.
 *
 * @see Str_Delete
 */
ddstring_t* Str_NewStd(void);

/**
 * Call this for uninitialized strings. Global variables are
 * automatically cleared, so they don't need initialization.
 */
void Str_Init(ddstring_t* ds);

/**
 * Call this for uninitialized strings. Makes the string use standard
 * malloc for memory allocations.
 */
void Str_InitStd(ddstring_t* ds);

/**
 * Empty an existing string. After this the string is in the same
 * state as just after being initialized.
 */
void Str_Free(ddstring_t* ds);

/**
 * Destroy a string allocated with Str_New(). In addition to freeing
 * the contents of the string, it also unallocates the string instance
 * that was created by Str_New().
 *
 * @param ds  String to delete (that was returned by Str_New()).
 */
void Str_Delete(ddstring_t* ds);

/**
 * Empties a string, but does not free its memory.
 */
void Str_Clear(ddstring_t* ds);

void Str_Reserve(ddstring_t* ds, int length);
ddstring_t* Str_Set(ddstring_t* ds, const char* text);
ddstring_t* Str_Append(ddstring_t* ds, const char* appendText);
ddstring_t* Str_AppendChar(ddstring_t* ds, char ch);

/**
 * Append formated text.
 */
ddstring_t* Str_Appendf(ddstring_t* ds, const char* format, ...) PRINTF_F(2,3);

/**
 * Appends a portion of a string.
 */
ddstring_t* Str_PartAppend(ddstring_t* dest, const char* src, int start, int count);

/**
 * Prepend is not even a word, is it? It should be 'prefix'?
 */
ddstring_t* Str_Prepend(ddstring_t* ds, const char* prependText);
ddstring_t* Str_PrependChar(ddstring_t* ds, char ch);

/**
 * This is safe for all strings.
 */
int Str_Length(const ddstring_t* ds);

boolean Str_IsEmpty(const ddstring_t* ds);

/**
 * This is safe for all strings.
 */
char* Str_Text(const ddstring_t* ds);

/**
 * Makes a true copy.
 */
ddstring_t* Str_Copy(ddstring_t* dest, const ddstring_t* src);

/**
 * Strip whitespace from beginning.
 */
int Str_StripLeft(ddstring_t* ds);

/**
 * Strip whitespace from end.
 */
int Str_StripRight(ddstring_t* ds);

/**
 * Strip whitespace from beginning and end.
 */
int Str_Strip(ddstring_t* ds);

/**
 * Extract a line of text from the source.
 */
const char* Str_GetLine(ddstring_t* ds, const char* src);

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

/**
 * Case sensitive comparison.
 */
int Str_Compare(const ddstring_t* str, const char* text);

/**
 * Non case sensitive comparison.
 */
int Str_CompareIgnoreCase(const ddstring_t* ds, const char* text);

/**
 * Retrieves a character in the string.
 *
 * @param str           String to get the character from.
 * @param index         Index of the character.
 *
 * @return              The character at @c index, or 0 if the index is not in range.
 */
char Str_At(const ddstring_t* str, int index);

/**
 * Retrieves a character in the string. Indices start from the end of the string.
 *
 * @param str           String to get the character from.
 * @param reverseIndex  Index of the character, where 0 is the last character of the string.
 *
 * @return              The character at @c index, or 0 if the index is not in range.
 */
char Str_RAt(const ddstring_t* str, int reverseIndex);

void Str_Truncate(ddstring_t* str, int position);

#endif /* LIBDENG_API_STRING_H */
