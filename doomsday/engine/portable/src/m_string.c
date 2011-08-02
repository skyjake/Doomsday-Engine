/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2011 Daniel Swanson <danij@dengine.net>
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
 * m_string.c: Dynamic Strings
 *
 * Simple dynamic string management.
 * Uses either normal mallocs or the memory zone for memory allocation
 * (chosen with the Init function).
 */

#include <ctype.h>
#include <stdio.h>
#include <stdarg.h>

#include "de_base.h"
#include "de_misc.h"

#define MAX_LENGTH  0x4000

static void* zoneAlloc(size_t n) {
    return Z_Malloc(n, PU_STATIC, 0);
}

static void* zoneCalloc(size_t n) {
    return Z_Calloc(n, PU_STATIC, 0);
}

static void* stdCalloc(size_t n) {
    return calloc(1, n);
}

static void Str_AutoInit(ddstring_t* ds)
{
    if(!ds->memFree && !ds->memAlloc && !ds->memCalloc)
    {
        // If the memory model is unspecified, default to the standard,
        // it is safer for threading.
        ds->memFree = free;
        ds->memAlloc = malloc;
        ds->memCalloc = stdCalloc;
    }
    assert(ds->memFree);
    assert(ds->memAlloc);
    assert(ds->memCalloc);
}

/**
 * Call this for uninitialized strings. Global variables are
 * automatically cleared, so they don't need initialization.
 * The string will use the memory zone.
 */
void Str_Init(ddstring_t *ds)
{
    memset(ds, 0, sizeof(*ds));

    // Init the memory management.
    ds->memFree = Z_Free;
    ds->memAlloc = zoneAlloc;
    ds->memCalloc = zoneCalloc;
}

/**
 * The string will use standard memory allocation.
 */
void Str_InitStd(ddstring_t *ds)
{
    memset(ds, 0, sizeof(*ds));

    // Init the memory management.
    ds->memFree = free;
    ds->memAlloc = malloc;
    ds->memCalloc = stdCalloc;
}

/**
 * Empty an existing string. After this the string is in the same
 * state as just after being initialized.
 */
void Str_Free(ddstring_t *ds)
{
    Str_AutoInit(ds);

    if(ds->size)
    {
        // The string has memory allocated, free it.
        ds->memFree(ds->str);
    }

    // Memory model left unchanged.
    ds->length = 0;
    ds->size = 0;
    ds->str = 0;
}

/**
 * Allocate a new uninitialized string. Use Str_Delete() to destroy
 * the returned string.
 *
 * @return New ddstring_t instance.
 *
 * @see Str_Delete
 */
ddstring_t *Str_New(void)
{
    ddstring_t* str = (ddstring_t*) M_Calloc(sizeof(ddstring_t));
    Str_Init(str);
    return str;
}

ddstring_t *Str_NewStd(void)
{
    ddstring_t* str = (ddstring_t*) M_Calloc(sizeof(ddstring_t));
    Str_InitStd(str);
    return str;
}

/**
 * Destroy a string allocated with Str_New(). In addition to freeing
 * the contents of the string, it also unallocates the string instance
 * that was created by Str_New().
 *
 * @param ds  String to delete (that was returned by Str_New()).
 */
void Str_Delete(ddstring_t *ds)
{
    Str_Free(ds);
    M_Free(ds);
}

/**
 * Empties a string, but does not free its memory.
 */
void Str_Clear(ddstring_t *ds)
{
    Str_Set(ds, "");
}

/**
 * Allocates so much memory that for_length characters will fit.
 */
void Str_Alloc(ddstring_t *ds, size_t for_length, int preserve)
{
    boolean old_data = false;
    char   *buf;

    // Include the terminating null character.
    for_length++;

    if(ds->size >= for_length)
        return;                 // We're OK.

    Str_AutoInit(ds);

    // Already some memory allocated?
    if(ds->size)
        old_data = true;
    else
        ds->size = 1;

    while(ds->size < for_length)
        ds->size *= 2;

    assert(ds->memCalloc);
    buf = ds->memCalloc(ds->size);

    if(preserve && ds->str)
        strncpy(buf, ds->str, ds->size - 1);

    // Replace the old string with the new buffer.
    if(old_data)
    {
        assert(ds->memFree);
        ds->memFree(ds->str);
    }
    ds->str = buf;
}

void Str_Reserve(ddstring_t *ds, size_t length)
{
    Str_Alloc(ds, length, true);
}

void Str_Set(ddstring_t *ds, const char *text)
{
    size_t incoming = strlen(text);
    char* copied = M_Malloc(incoming + 1); // take a copy in case text points to (a part of) ds->str

    strcpy(copied, text);
    Str_Alloc(ds, incoming, false);
    strcpy(ds->str, copied);
    ds->length = incoming;
    M_Free(copied);
}

void Str_Append(ddstring_t *ds, const char *append_text)
{
    size_t incoming = strlen(append_text);
    char* copied;

    // Don't allow extremely long strings.
    if(ds->length > MAX_LENGTH)
        return;

    // Take a copy in case append_text points to (a part of) ds->str, which may be invalidated
    // by Str_Alloc.
    copied = M_Malloc(incoming + 1);
    strcpy(copied, append_text);

    Str_Alloc(ds, ds->length + incoming, true);
    strcpy(ds->str + ds->length, copied);
    ds->length += incoming;

    M_Free(copied);
}

void Str_AppendChar(ddstring_t* ds, char ch)
{
    char str[2] = { ch, 0 };
    Str_Append(ds, str);
}

/**
 * Append formated text.
 */
void Str_Appendf(ddstring_t *ds, const char *format, ...)
{
    char buf[1024];
    va_list args;

    // Print the message into the buffer.
    va_start(args, format);
    dd_vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    Str_Append(ds, buf);
}

/**
 * Appends a portion of a string.
 */
void Str_PartAppend(ddstring_t *dest, const char *src, int start, size_t count)
{
    char* copied = M_Malloc(count);

    memcpy(copied, src + start, count);

    Str_Alloc(dest, dest->length + count + 1, true);
    memcpy(dest->str + dest->length, copied, count);
    dest->length += count;

    // Terminate the appended part.
    dest->str[dest->length] = 0;

    M_Free(copied);
}

/**
 * Prepend is not even a word, is it? It should be 'prefix'?
 */
void Str_Prepend(ddstring_t *ds, const char *prepend_text)
{
    size_t incoming = strlen(prepend_text);
    char* copied;

    // Don't allow extremely long strings.
    if(ds->length > MAX_LENGTH)
        return;

    copied = M_Malloc(incoming);
    memcpy(copied, prepend_text, incoming);

    Str_Alloc(ds, ds->length + incoming, true);
    memmove(ds->str + incoming, ds->str, ds->length + 1);
    memcpy(ds->str, copied, incoming);
    ds->length += incoming;

    M_Free(copied);
}

/**
 * This is safe for all strings.
 */
char *Str_Text(ddstring_t *ds)
{
    return ds->str ? ds->str : "";
}

/**
 * This is safe for all strings.
 */
size_t Str_Length(ddstring_t *ds)
{
    if(ds->length)
        return ds->length;
    return strlen(Str_Text(ds));
}

/**
 * Makes a true copy.
 */
void Str_Copy(ddstring_t *dest, ddstring_t *src)
{
    Str_Free(dest);
    dest->size = src->size;
    dest->length = src->length;
    assert(dest->memAlloc);
    dest->str = dest->memAlloc(src->size);
    memcpy(dest->str, src->str, src->size);
}

/**
 * Strip whitespace from beginning.
 */
void Str_StripLeft(ddstring_t *ds)
{
    size_t      i, num;
    boolean     isDone;

    if(!ds->length)
        return;

    // Find out how many whitespace chars are at the beginning.
    isDone = false;
    i = 0;
    num = 0;
    while(i < ds->length && !isDone)
    {
        if(isspace(ds->str[i]))
        {
            num++;
            i++;
        }
        else
            isDone = true;
    }

    if(num)
    {
        // Remove 'num' chars.
        memmove(ds->str, ds->str + num, ds->length - num);
        ds->length -= num;
        ds->str[ds->length] = 0;
    }
}

/**
 * Strip whitespace from end.
 */
void Str_StripRight(ddstring_t *ds)
{
    int             i;
    boolean         isDone;

    if(ds->length == 0)
        return;

    i = ds->length - 1;
    isDone = false;
    while(i >= 0 && !isDone)
    {
        if(isspace(ds->str[i]))
        {
            // Remove this char.
            ds->str[i] = 0;
            ds->length--;
            i--;
        }
        else
        {
            isDone = true;
        }
    }
}

/**
 * Strip whitespace from beginning and end.
 */
void Str_Strip(ddstring_t *ds)
{
    Str_StripLeft(ds);
    Str_StripRight(ds);
}

/**
 * Extract a line of text from the source.
 */
const char *Str_GetLine(ddstring_t *ds, const char *src)
{
    char    buf[2];

    // We'll append the chars one by one.
    memset(buf, 0, sizeof(buf));

    for(Str_Clear(ds); *src && *src != '\n'; src++)
    {
        if(*src != '\r')
        {
            buf[0] = *src;
            Str_Append(ds, buf);
        }
    }

    // Strip whitespace around the line.
    Str_Strip(ds);

    // The newline is excluded.
    if(*src == '\n')
        src++;
    return src;
}

/**
 * Performs a string comparison, ignoring differences in case.
 */
int Str_CompareIgnoreCase(ddstring_t *ds, const char *text)
{
    return strcasecmp(Str_Text(ds), text);
}

/**
 * Copies characters from @c to @dest until a @c delim character is encountered.
 * Also ignores all whitespace characters.
 *
 * @param dest  Destination string.
 * @param src   Source string.
 * @param delim  Delimiter character, where copying will stop.
 *
 * @return  Pointer to the character within @c src past the delimiter, or NULL if the
 *          source data ended.
 */
const char* Str_CopyDelim(ddstring_t* dest, const char* src, char delim)
{
    Str_Clear(dest);

    if(!src)
        return NULL;

    for(; *src && *src != delim; ++src)
    {
        if(!isspace(*src))
            Str_PartAppend(dest, src, 0, 1);
    }

    if(!*src)
        return NULL; // It ended.

    // Skip past the delimiter.
    return src + 1;
}

/**
 * Retrieves a character in the string.
 *
 * @param str    String to get the character from.
 * @param index  Index of the character.
 *
 * @return The character at @c index, or 0 if the index is not in range.
 */
char Str_At(ddstring_t* str, int index)
{
    if(index < 0 || (uint)index >= str->length)
    {
        return 0;
    }
    return str->str[index];
}

/**
 * Retrieves a character in the string. Indices start from the end of the string.
 *
 * @param str    String to get the character from.
 * @param reverseIndex  Index of the character, where 0 is the last character of the string.
 *
 * @return The character at @c index, or 0 if the index is not in range.
 */
char Str_RAt(ddstring_t* str, int reverseIndex)
{
    if(reverseIndex < 0 || (uint)reverseIndex >= str->length)
    {
        return 0;
    }
    return str->str[str->length - 1 - reverseIndex];
}
