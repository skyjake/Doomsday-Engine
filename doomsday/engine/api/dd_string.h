/**
 * @file dd_string.h
 * Dynamic text string.
 *
 * Simple dynamic string management. @ingroup base
 *
 * Uses @ref memzone or standard malloc for memory allocation, chosen during
 * initialization of a string. The string instance itself is always allocated
 * with malloc.
 *
 * AutoStr is a variant of ddstring_t that is automatically put up for garbage
 * collection. You may create an AutoStr instance either by converting an
 * existing string to one with AutoStr_FromStr() or by allocating a new
 * instance with AutoStr_New(). You are @em not allowed to manually delete an
 * AutoStr instance; it will be deleted automatically during the next garbage
 * recycling.
 *
 * Using AutoStr instances is recommended when you have a dynamic string as a
 * return value from a function, or when you have strings with a limited scope
 * that need to be deleted after exiting the scope.
 *
 * @todo Rename to Str? (str.h)
 * @todo Make this opaque for better forward compatibility -- prevents initialization
 *       with static C strings, though (which is probably for the better anyway).
 * @todo Derive from Qt::QString
 *
 * @authors Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2008-2010 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_API_STRING_H
#define LIBDENG_API_STRING_H

#include <stddef.h>
#include "reader.h"
#include "writer.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Dynamic string instance. Use Str_New() to allocate one from the heap, or
 * Str_Init() to initialize a string located on the stack.
 *
 * You can init global ddstring_t variables with static string constants,
 * for example: @code ddstring_t mystr = { "Hello world." }; @endcode
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

/**
 * An alias for ddstring_t that is used with the convention of automatically
 * trashing the string during construction so that it gets deleted during the
 * next recycling. Thus it is useful for strings restricted to the local scope
 * or for return values in cases when caller is not always required to take
 * ownership.
 */
typedef ddstring_t AutoStr;

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
 * Constructs a new string by reading it from @a reader.
 * Memory for the string is allocated with de::Zone.
 */
ddstring_t* Str_NewFromReader(Reader* reader);

/**
 * Call this for uninitialized strings. Global variables are
 * automatically cleared, so they don't need initialization.
 */
ddstring_t* Str_Init(ddstring_t* ds);

/**
 * Call this for uninitialized strings. Makes the string use standard
 * malloc for memory allocations.
 */
ddstring_t* Str_InitStd(ddstring_t* ds);

/**
 * Initializes @a ds with a static const C string. No memory allocation
 * model is selected; use this for strings that remain constant.
 * If the string is never modified calling Str_Free() is not needed.
 */
ddstring_t* Str_InitStatic(ddstring_t* ds, const char* staticConstStr);

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
ddstring_t* Str_Clear(ddstring_t* ds);

ddstring_t* Str_Reserve(ddstring_t* ds, int length);

/**
 * Reserves memory for the string. There will be at least @a length bytes
 * allocated for the string after this. If the string needs to be resized, its
 * contents are @em not preserved.
 */
ddstring_t* Str_ReserveNotPreserving(ddstring_t* str, int length);

ddstring_t* Str_Set(ddstring_t* ds, const char* text);
ddstring_t* Str_Append(ddstring_t* ds, const char* appendText);
ddstring_t* Str_AppendChar(ddstring_t* ds, char ch);

/**
 * Appends the contents of another string. Enough memory must already be
 * reserved before calling this. Use in situations where good performance is
 * critical.
 */
ddstring_t* Str_AppendWithoutAllocs(ddstring_t* str, const ddstring_t* append);

/**
 * Appends a single character. Enough memory must already be reserved before
 * calling this. Use in situations where good performance is critical.
 *
 * @param str  String.
 * @param ch   Character to append. Cannot be 0.
 */
ddstring_t* Str_AppendCharWithoutAllocs(ddstring_t* str, char ch);

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
ddstring_t* Str_CopyOrClear(ddstring_t* dest, const ddstring_t* src);

/**
 * Strip whitespace from beginning.
 *
 * @param ds  String instance.
 * @param count  If not @c NULL the number of characters stripped is written here.
 * @return  Same as @a str for caller convenience.
 */
ddstring_t* Str_StripLeft2(ddstring_t* ds, int* count);
ddstring_t* Str_StripLeft(ddstring_t* ds);

/**
 * Strip whitespace from end.
 *
 * @param ds  String instance.
 * @param count  If not @c NULL the number of characters stripped is written here.
 * @return  Same as @a str for caller convenience.
 */
ddstring_t* Str_StripRight2(ddstring_t* ds, int* count);
ddstring_t* Str_StripRight(ddstring_t* ds);

/**
 * Strip whitespace from beginning and end.
 *
 * @param ds  String instance.
 * @param count  If not @c NULL the number of characters stripped is written here.
 * @return  Same as @a str for caller convenience.
 */
ddstring_t* Str_Strip2(ddstring_t* ds, int* count);
ddstring_t* Str_Strip(ddstring_t* ds);

/**
 * Extract a line of text from the source.
 *
 * @param ds   String instance where to put the extracted line.
 * @param src  Source string. Must be @c NULL terminated.
 */
const char* Str_GetLine(ddstring_t* ds, const char* src);

/**
 * @defgroup copyDelimiterFlags Copy Delimiter Flags
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
 * @param cdflags       @ref copyDelimiterFlags
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

/**
 * Percent-encode characters in string. Will encode the default set of
 * characters for the unicode utf8 charset.
 *
 * @param str           String instance.
 * @return              Same as @a str.
 */
ddstring_t* Str_PercentEncode(ddstring_t* str);

/**
 * Percent-encode characters in string.
 *
 * @param str           String instance.
 * @param excludeChars  List of characters that should NOT be encoded. @c 0 terminated.
 * @param includeChars  List of characters that will always be encoded (has precedence over
 *                      @a excludeChars). @c 0 terminated.
 * @return              Same as @a str.
 */
ddstring_t* Str_PercentEncode2(ddstring_t* str, const char* excludeChars, const char* includeChars);

/**
 * Decode the percent-encoded string. Will match codes for the unicode
 * utf8 charset.
 *
 * @param str           String instance.
 * @return              Same as @a str.
 */
ddstring_t* Str_PercentDecode(ddstring_t* str);

void Str_Write(const ddstring_t* str, Writer* writer);

void Str_Read(ddstring_t* str, Reader* reader);

/*
 * AutoStr
 */
AutoStr* AutoStr_New(void);
AutoStr* AutoStr_NewStd(void);

/**
 * Converts a ddstring to an AutoStr. After this you are not allowed
 * to call Str_Delete() manually on the string; the garbage collector
 * will destroy the string during the next recycling.
 *
 * No memory allocations are done in this function.
 *
 * @param str  String instance.
 *
 * @return  AutoStr instance. The returned instance is @a str after
 * having been trashed.
 */
AutoStr* AutoStr_FromStr(ddstring_t* str);

/**
 * Converts an AutoStr to a normal ddstring. You must call Str_Delete()
 * on the returned string manually to destroy it.
 *
 * No memory allocations are done in this function.
 *
 * @param as  AutoStr instance.
 *
 * @return  ddstring instance. The returned instance is @a as after
 * having been taken out of the garbage.
 */
ddstring_t* Str_FromAutoStr(AutoStr* as);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_API_STRING_H */
