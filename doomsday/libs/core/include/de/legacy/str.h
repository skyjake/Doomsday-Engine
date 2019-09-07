/** @file str.h Dynamic text string.
 *
 * Dynamic string management and other text utilities.
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
 * @todo Make this opaque for better forward compatibility -- prevents initialization
 *       with static C strings, though (which is probably for the better anyway).
 * @todo Derive from Qt::QString
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2008-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DE_API_STRING_H
#define DE_API_STRING_H

#include "de/liblegacy.h"
#include "types.h"
#include "reader.h"
#include "writer.h"
#include <stdarg.h>

/**
 * @defgroup copyDelimiterFlags Copy Delimiter Flags
 * Flags for Str_CopyDelim2().
 * @ingroup legacyFlags
 * @{
 */
#define CDF_OMIT_DELIMITER      0x1 ///< Do not copy delimiters into the dest path.
#define CDF_OMIT_WHITESPACE     0x2 ///< Do not copy whitespace into the dest path.
/** @} */

/// @addtogroup legacyData
/// @{

#define DE_LAST_CHAR(strNullTerminated)  (strNullTerminated[strlen(strNullTerminated) - 1])

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
    char *str;

    /// String length (no terminating nulls).
    size_t length;

    /// Allocated buffer size: includes the terminating null and possibly
    /// some extra space.
    size_t size;

    // Memory management.
    void (*memFree)(void *);
    void *(*memAlloc)(size_t n);
    void *(*memCalloc)(size_t n);
} ddstring_t;

/**
 * The primary alias for the ddstring struct.
 */
typedef struct ddstring_s Str;

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
DE_PUBLIC Str *Str_New(void);

/**
 * Allocate a new uninitialized string. Use Str_Delete() to destroy
 * the returned string. Memory for the string is allocated with malloc.
 *
 * @return New ddstring_t instance.
 *
 * @see Str_Delete
 */
DE_PUBLIC Str *Str_NewStd(void);

/**
 * Constructs a new string by reading it from @a reader.
 * Memory for the string is allocated with de::Zone.
 */
DE_PUBLIC Str *Str_NewFromReader(Reader1 *reader);

/**
 * Call this for uninitialized strings. Global variables are
 * automatically cleared, so they don't need initialization.
 */
DE_PUBLIC Str *Str_Init(Str *ds);

/**
 * Call this for uninitialized strings. Makes the string use standard
 * malloc for memory allocations.
 */
DE_PUBLIC Str *Str_InitStd(Str *ds);

/**
 * Initializes @a ds with a static const C string. No memory allocation
 * model is selected; use this for strings that remain constant.
 * If the string is never modified calling Str_Free() is not needed.
 */
DE_PUBLIC Str *Str_InitStatic(Str *ds, const char *staticConstStr);

/**
 * Empty an existing string. After this the string is in the same
 * state as just after being initialized.
 */
DE_PUBLIC void Str_Free(Str *ds);

/**
 * Destroy a string allocated with Str_New(). In addition to freeing
 * the contents of the string, it also unallocates the string instance
 * that was created by Str_New().
 *
 * @param ds  String to delete (that was returned by Str_New()).
 */
DE_PUBLIC void Str_Delete(Str *ds);

/**
 * Empties a string, but does not free its memory.
 */
DE_PUBLIC Str *Str_Clear(Str *ds);

DE_PUBLIC Str *Str_Reserve(Str *ds, int length);

/**
 * Reserves memory for the string. There will be at least @a length bytes
 * allocated for the string after this. If the string needs to be resized, its
 * contents are @em not preserved.
 */
DE_PUBLIC Str *Str_ReserveNotPreserving(Str *str, int length);

DE_PUBLIC Str *Str_Set(Str *ds, const char *text);

DE_PUBLIC Str *Str_Append(Str *ds, const char *appendText);

DE_PUBLIC Str *Str_AppendChar(Str *ds, char ch);

/**
 * Appends the contents of another string. Enough memory must already be
 * reserved before calling this. Use in situations where good performance is
 * critical.
 */
DE_PUBLIC Str *Str_AppendWithoutAllocs(Str *str, const Str *append);

/**
 * Appends a single character. Enough memory must already be reserved before
 * calling this. Use in situations where good performance is critical.
 *
 * @param str  String.
 * @param ch   Character to append. Cannot be 0.
 */
DE_PUBLIC Str *Str_AppendCharWithoutAllocs(Str *str, char ch);

/**
 * Append formated text.
 */
DE_PUBLIC Str *Str_Appendf(Str *ds, const char *format, ...) PRINTF_F(2,3);

/**
 * Appends a portion of a string.
 */
DE_PUBLIC Str *Str_PartAppend(Str *dest, const char *src, int start, int count);

/**
 * Prepend is not even a word, is it? It should be 'prefix'?
 */
DE_PUBLIC Str *Str_Prepend(Str *ds, const char *prependText);
DE_PUBLIC Str *Str_PrependChar(Str *ds, char ch);

/**
 * Determines the length of the string in characters. This is safe for all
 * strings.
 *
 * @param ds  String instance.
 *
 * @return Length of the string as an integer.
 * @see Str_Size()
 */
DE_PUBLIC int Str_Length(const Str *ds);

/**
 * Determines the length of the string in characters. This is safe for all
 * strings.
 *
 * @param ds  String instance.
 *
 * @return Length of the string.
 * @see Str_Length()
 */
DE_PUBLIC size_t Str_Size(const Str *ds);

DE_PUBLIC dd_bool Str_IsEmpty(const Str *ds);

/**
 * This is safe for all strings.
 */
DE_PUBLIC char *Str_Text(const Str *ds);

/**
 * Makes a copy of @a src and replaces the previous contents of @a dest with
 * it. The copy will have least as much memory reserved in its internal buffer
 * as the original.
 *
 * If @a src is a static string (i.e., no memory allocated for its buffer), new
 * memory will be allocated for the copy.
 *
 * @param dest  String where the copy is stored.
 * @param src   Original string to copy.
 *
 * @return  The @a dest string with the copied content.
 */
DE_PUBLIC Str *Str_Copy(Str *dest, const Str *src);

DE_PUBLIC Str *Str_CopyOrClear(Str *dest, const Str *src);

/**
 * Strip whitespace from beginning.
 *
 * @param ds  String instance.
 * @param count  If not @c NULL the number of characters stripped is written here.
 * @return  Same as @a str for caller convenience.
 */
DE_PUBLIC Str *Str_StripLeft2(Str *ds, int *count);

DE_PUBLIC Str *Str_StripLeft(Str *ds);

/**
 * Strip whitespace from end.
 *
 * @param ds  String instance.
 * @param count  If not @c NULL the number of characters stripped is written here.
 * @return  Same as @a str for caller convenience.
 */
DE_PUBLIC Str *Str_StripRight2(Str *ds, int *count);

DE_PUBLIC Str *Str_StripRight(Str *ds);

/**
 * Strip whitespace from beginning and end.
 *
 * @param ds  String instance.
 * @param count  If not @c NULL the number of characters stripped is written here.
 * @return  Same as @a ds for caller convenience.
 */
DE_PUBLIC Str *Str_Strip2(Str *ds, int *count);

DE_PUBLIC Str *Str_Strip(Str *ds);

/**
 * Replaces all characters @a from to @a to in the string.
 *
 * @param ds    String instance.
 * @param from  Characters to replace.
 * @param to    @a from will be replaced with this.
 *
 * @return Str @a ds.
 */
DE_PUBLIC Str *Str_ReplaceAll(Str *ds, char from, char to);

/**
 * Determines if the string starts with the given substring. The comparison is done
 * case sensitively.
 *
 * @param ds    String instance.
 * @param text  Text to look for at the start of @a ds.
 *
 * @return  @c true, if the string is found.
 */
DE_PUBLIC dd_bool Str_StartsWith(const Str *ds, const char *text);

/**
 * Determines if the string ends with a specific suffic. The comparison is done
 * case sensitively.
 *
 * @param ds    String instance.
 * @param text  Text to look for in the end of @a ds.
 *
 * @return  @c true, if the string is found.
 */
DE_PUBLIC dd_bool Str_EndsWith(const Str *ds, const char *text);

/**
 * Extract a line of text from the source.
 *
 * @param ds   String instance where to put the extracted line.
 * @param src  Source string. Must be @c NULL terminated.
 */
DE_PUBLIC const char *Str_GetLine(Str *ds, const char *src);

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
DE_PUBLIC const char *Str_CopyDelim2(Str *dest, const char *src, char delimiter, int cdflags);

DE_PUBLIC const char *Str_CopyDelim(Str *dest, const char *src, char delimiter);

/**
 * Case sensitive comparison.
 */
DE_PUBLIC int Str_Compare(const Str *str, const char *text);

/**
 * Non case sensitive comparison.
 */
DE_PUBLIC int Str_CompareIgnoreCase(const Str *ds, const char *text);

/**
 * Retrieves a character in the string.
 *
 * @param str           String to get the character from.
 * @param index         Index of the character.
 *
 * @return              The character at @c index, or 0 if the index is not in range.
 */
DE_PUBLIC char Str_At(const Str *str, int index);

/**
 * Retrieves a character in the string. Indices start from the end of the string.
 *
 * @param str           String to get the character from.
 * @param reverseIndex  Index of the character, where 0 is the last character of the string.
 *
 * @return              The character at @c index, or 0 if the index is not in range.
 */
DE_PUBLIC char Str_RAt(const Str *str, int reverseIndex);

DE_PUBLIC void Str_Truncate(Str *str, int position);

/**
 * Percent-encode characters in string. Will encode the default set of
 * characters for the unicode utf8 charset.
 *
 * @param str           String instance.
 * @return              Same as @a str.
 */
DE_PUBLIC Str *Str_PercentEncode(Str *str);

/**
 * Percent-encode characters in string.
 *
 * @param str           String instance.
 * @param excludeChars  List of characters that should NOT be encoded. @c 0 terminated.
 * @param includeChars  List of characters that will always be encoded (has precedence over
 *                      @a excludeChars). @c 0 terminated.
 * @return              Same as @a str.
 */
DE_PUBLIC Str *Str_PercentEncode2(Str *str, const char *excludeChars, const char *includeChars);

/**
 * Decode the percent-encoded string. Will match codes for the unicode
 * utf8 charset.
 *
 * @param str           String instance.
 * @return              Same as @a str.
 */
DE_PUBLIC Str *Str_PercentDecode(Str *str);

DE_PUBLIC void Str_Write(const Str *str, Writer1 *writer);

DE_PUBLIC void Str_Read(Str *str, Reader1 *reader);

/*
 * AutoStr
 */
DE_PUBLIC AutoStr *AutoStr_New(void);

DE_PUBLIC AutoStr *AutoStr_NewStd(void);

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
DE_PUBLIC AutoStr *AutoStr_FromStr(Str *str);

/**
 * Constructs an AutoStr instance (zone allocated) and initializes its
 * contents with @a text.
 *
 * @param text  Text for the new string.
 *
 * @return  AutoStr instance.
 */
DE_PUBLIC AutoStr *AutoStr_FromText(const char *text);

/**
 * Constructs an AutoStr instance (standard malloc) and initializes its
 * contents with @a text.
 *
 * @param text  Text for the new string.
 *
 * @return  AutoStr instance.
 */
DE_PUBLIC AutoStr *AutoStr_FromTextStd(const char *text);

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
DE_PUBLIC Str *Str_FromAutoStr(AutoStr *as);

/// @}

#ifdef __cplusplus
} // extern "C"
#  include "str.hh" // C++ wrapper for ddstring_t
#endif

#endif /* DE_API_STRING_H */
