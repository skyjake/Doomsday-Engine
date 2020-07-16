/**
 * @file str.c
 * Dynamic text string.
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

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#if WIN32
# define strcasecmp _stricmp
#else
# include <strings.h>
#endif

#include "de/legacy/str.h"
#include "de/legacy/memory.h"
#include "de/legacy/memoryzone.h"
#include "de/legacy/strutil.h"
#include "de/garbage.h"
#include <de/c_wrapper.h>

static void *zoneAlloc(size_t n) {
    return Z_Malloc(n, PU_APPSTATIC, 0);
}

static void *zoneCalloc(size_t n) {
    return Z_Calloc(n, PU_APPSTATIC, 0);
}

static void *stdCalloc(size_t n) {
    return M_Calloc(n);
}

static void autoselectMemoryManagement(ddstring_t *str)
{
    if (!str->memFree && !str->memAlloc && !str->memCalloc)
    {
        // If the memory model is unspecified, default to the standard,
        // it is safer for threading.
        str->memFree = M_Free;
        str->memAlloc = M_Malloc;
        str->memCalloc = stdCalloc;
    }
    DE_ASSERT(str->memFree);
    DE_ASSERT(str->memAlloc);
    DE_ASSERT(str->memCalloc);
}

static void allocateString(ddstring_t *str, size_t forLength, int preserve)
{
    size_t oldSize = str->size;
    char *buf;

    // Include the terminating null character.
    forLength++;

    if (str->size >= forLength)
        return; // We're OK.

    autoselectMemoryManagement(str);

    // Determine the new size of the memory buffer.
    if (!str->size) str->size = 1;
    while (str->size < forLength)
    {
        str->size *= 2;
    }

    DE_ASSERT(str->memCalloc);
    buf = str->memCalloc(str->size);

    if (preserve && str->str && oldSize)
    {
        // Copy the old contents of the string.
        DE_ASSERT(oldSize <= str->size);
        memcpy(buf, str->str, oldSize);
    }

    // Replace the old string with the new buffer.
    if (oldSize)
    {
        DE_ASSERT(str->memFree);
        str->memFree(str->str);
    }
    str->str = buf;
}

/**
 * Call this for uninitialized strings. Global variables are
 * automatically cleared, so they don't need initialization.
 * The string will use the memory zone.
 */
ddstring_t *Str_Init(ddstring_t *str)
{
    DE_ASSERT(str);
    if (!str) return 0;

    if (!Z_IsInited())
    {
        // The memory zone is not available at the moment.
        return Str_InitStd(str);
    }

    memset(str, 0, sizeof(*str));

    // Init the memory management.
    str->memFree = Z_Free;
    str->memAlloc = zoneAlloc;
    str->memCalloc = zoneCalloc;
    return str;
}

/**
 * The string will use standard memory allocation.
 */
ddstring_t *Str_InitStd(ddstring_t *str)
{
    memset(str, 0, sizeof(*str));

    // Init the memory management.
    str->memFree = free;
    str->memAlloc = malloc;
    str->memCalloc = stdCalloc;
    return str;
}

ddstring_t *Str_InitStatic(ddstring_t *str, char const *staticConstStr)
{
    memset(str, 0, sizeof(*str));
    str->str = (char *) staticConstStr;
    str->length = (staticConstStr? strlen(staticConstStr) : 0);
    return str;
}

void Str_Free(ddstring_t *str)
{
    DE_ASSERT(str);
    if (!str) return;

    autoselectMemoryManagement(str);

    if (str->size)
    {
        // The string has memory allocated, free it.
        str->memFree(str->str);
    }

    // Memory model left unchanged.
    str->length = 0;
    str->size = 0;
    str->str = 0;
}

ddstring_t *Str_NewStd(void)
{
    ddstring_t *str = (ddstring_t *) M_Calloc(sizeof(ddstring_t));
    Str_InitStd(str);
    return str;
}

ddstring_t *Str_New(void)
{
    ddstring_t *str = (ddstring_t *) M_Calloc(sizeof(ddstring_t));
    Str_Init(str);
    return str;
}

ddstring_t *Str_NewFromReader(Reader1 *reader)
{
    ddstring_t *str = Str_New();
    Str_Read(str, reader);
    return str;
}

static void deleteString(Str *str)
{
    DE_ASSERT(str);
    if (!str) return;

    Str_Free(str);
    M_Free(str);
}

void Str_Delete(Str *str)
{
    if (!str) return;

    DE_ASSERT(!Garbage_IsTrashed(str));

#if 0 // use this is release builds if encountering Str/AutoStr errors
    if (Garbage_IsTrashed(str))
    {
        App_FatalError("Str_Delete: Trying to manually delete an AutoStr!");
    }
#endif

    deleteString(str);
}

ddstring_t *Str_Clear(ddstring_t *str)
{
    return Str_Set(str, "");
}

ddstring_t *Str_Reserve(ddstring_t *str, int length)
{
    DE_ASSERT(str);
    if (!str) return 0;

    if (length > 0)
    {
        allocateString(str, length, true);
    }
    return str;
}

ddstring_t *Str_ReserveNotPreserving(ddstring_t *str, int length)
{
    DE_ASSERT(str);
    if (!str) return 0;

    if (length > 0)
    {
        allocateString(str, length, false);
    }
    return str;
}

ddstring_t *Str_Set(ddstring_t *str, char const *text)
{
    DE_ASSERT(str);
    if (!str) return 0;

    {
    size_t incoming = strlen(text);
    char *copied = M_Malloc(incoming + 1); // take a copy in case text points to (a part of) str->str
    strcpy(copied, text);
    allocateString(str, incoming, false);
    strcpy(str->str, copied);
    str->length = incoming;
    M_Free(copied);
    return str;
    }
}

ddstring_t *Str_AppendWithoutAllocs(ddstring_t *str, const ddstring_t *append)
{
    DE_ASSERT(str);
    DE_ASSERT(append);
    DE_ASSERT(str->length + append->length + 1 <= str->size); // including the null

    if (!str) return 0;

    memcpy(str->str + str->length, append->str, append->length);
    str->length += append->length;
    str->str[str->length] = 0;
    return str;
}

ddstring_t *Str_AppendCharWithoutAllocs(ddstring_t *str, char ch)
{
    DE_ASSERT(str);
    DE_ASSERT(ch); // null not accepted
    DE_ASSERT(str->length + 2 <= str->size); // including a terminating null

    if (!str) return 0;

    str->str[str->length++] = ch;
    str->str[str->length] = 0;
    return str;
}

ddstring_t *Str_Append(ddstring_t *str, char const *append)
{
    DE_ASSERT(str);
    if (!str) return 0;

    if (append && append[0])
    {
        size_t incoming = strlen(append);
        // Take a copy in case append_text points to (a part of) ds->str, which may
        // be invalidated by allocateString.
        char *copied = M_Malloc(incoming + 1);

        strcpy(copied, append);
        allocateString(str, str->length + incoming, true);
        strcpy(str->str + str->length, copied);
        str->length += incoming;

        M_Free(copied);
    }
    return str;
}

ddstring_t *Str_AppendChar(ddstring_t *str, char ch)
{
    char append[2];
    append[0] = ch;
    append[1] = '\0';
    return Str_Append(str, append);
}

ddstring_t *Str_Appendf(ddstring_t *str, char const *format, ...)
{
    DE_ASSERT(str);
    if (!str) return 0;

    { char buf[4096];
    va_list args;

    // Print the message into the buffer.
    va_start(args, format);
    dd_vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    Str_Append(str, buf);
    return str;
    }
}

ddstring_t *Str_PartAppend(ddstring_t *str, char const *append, int start, int count)
{
    int partLen;
    char *copied;

    DE_ASSERT(str);
    DE_ASSERT(append);

    if (!str || !append) return str;
    if (start < 0 || count <= 0) return str;

    copied = M_Malloc(count + 1);
    copied[0] = 0; // empty string

    strncat(copied, append + start, count);
    partLen = strlen(copied);

    allocateString(str, str->length + partLen + 1, true);
    memcpy(str->str + str->length, copied, partLen);
    str->length += partLen;

    // Terminate the appended part.
    str->str[str->length] = 0;

    M_Free(copied);
    return str;
}

ddstring_t *Str_Prepend(ddstring_t *str, char const *prepend)
{
    char *copied;
    size_t incoming;

    DE_ASSERT(str);
    DE_ASSERT(prepend);

    if (!str || !prepend) return str;

    incoming = strlen(prepend);
    if (incoming == 0)
        return str;

    copied = M_Malloc(incoming);
    memcpy(copied, prepend, incoming);

    allocateString(str, str->length + incoming, true);
    memmove(str->str + incoming, str->str, str->length + 1);
    memcpy(str->str, copied, incoming);
    str->length += incoming;

    M_Free(copied);
    return str;
}

ddstring_t *Str_PrependChar(ddstring_t *str, char ch)
{
    char prepend[2];
    prepend[0] = ch;
    prepend[1] = '\0';
    return Str_Prepend(str, prepend);
}

char *Str_Text(const ddstring_t *str)
{
    if (!str) return "[null]";
    return str->str ? str->str : "";
}

int Str_Length(const ddstring_t *str)
{
    return (int) Str_Size(str);
}

size_t Str_Size(Str const *str)
{
    DE_ASSERT(str);

    if (!str) return 0;
    if (str->length)
    {
        return str->length;
    }
    return strlen(Str_Text(str));
}

dd_bool Str_IsEmpty(const ddstring_t *str)
{
    DE_ASSERT(str);
    return Str_Length(str) == 0;
}

ddstring_t *Str_Copy(ddstring_t *str, const ddstring_t *other)
{
    DE_ASSERT(str);
    DE_ASSERT(other);
    if (!str || !other) return str;

    Str_Free(str);

    if (!other->size)
    {
        // The original string has no memory allocated; it's a static string.
        allocateString(str, other->length, false);
        if (other->str)
            strcpy(str->str, other->str);
        str->length = other->length;
    }
    else
    {
        // Duplicate the other string's buffer in its entirety.
        str->str = str->memAlloc(other->size);
        memcpy(str->str, other->str, other->size);
        str->size = other->size;
        str->length = other->length;
    }
    return str;
}

ddstring_t *Str_CopyOrClear(ddstring_t *dest, const ddstring_t *src)
{
    DE_ASSERT(dest);
    if (!dest) return 0;

    if (src)
    {
        return Str_Copy(dest, src);
    }
    return Str_Clear(dest);
}

ddstring_t *Str_StripLeft2(ddstring_t *str, int *count)
{
    int i, num;
    dd_bool isDone;

    DE_ASSERT(str);
    if (!str) return 0;

    if (!str->length)
    {
        if (count) *count = 0;
        return str;
    }

    // Find out how many whitespace chars are at the beginning.
    isDone = false;
    i = 0;
    num = 0;
    while (i < (int)str->length && !isDone)
    {
        if (isspace(str->str[i]))
        {
            num++;
            i++;
        }
        else
            isDone = true;
    }

    if (num)
    {
        // Remove 'num' chars.
        memmove(str->str, str->str + num, str->length - num);
        str->length -= num;
        str->str[str->length] = 0;
    }

    if (count) *count = num;
    return str;
}

ddstring_t *Str_StripLeft(ddstring_t *str)
{
    return Str_StripLeft2(str, NULL/*not interested in the stripped character count*/);
}

ddstring_t *Str_StripRight2(ddstring_t *str, int *count)
{
    int i, num;

    DE_ASSERT(str);
    if (!str) return 0;

    if (str->length == 0)
    {
        if (count) *count = 0;
        return str;
    }

    i = str->length - 1;
    num = 0;
    if (isspace(str->str[i]))
    do
    {
        // Remove this char.
        num++;
        str->str[i] = '\0';
        str->length--;
    } while (i != 0 && isspace(str->str[--i]));

    if (count) *count = num;
    return str;
}

ddstring_t *Str_StripRight(ddstring_t *str)
{
    return Str_StripRight2(str, NULL/*not interested in the stripped character count*/);
}

ddstring_t *Str_Strip2(ddstring_t *str, int *count)
{
    int right_count, left_count;
    Str_StripLeft2(Str_StripRight2(str, &right_count), &left_count);
    if (count) *count = right_count + left_count;
    return str;
}

ddstring_t *Str_Strip(ddstring_t *str)
{
    return Str_Strip2(str, NULL/*not interested in the stripped character count*/);
}

dd_bool Str_StartsWith(Str const *ds, char const *text)
{
    size_t len = strlen(text);
    DE_ASSERT(ds);
    if (!ds->str || !text || Str_Size(ds) < len) return false;
    return !strncmp(ds->str, text, len);
}

dd_bool Str_EndsWith(Str const *ds, char const *text)
{
    size_t len = strlen(text);
    DE_ASSERT(ds);
    if (!ds->str || !text || Str_Size(ds) < len) return false;
    return !strcmp(ds->str + Str_Size(ds) - len, text);
}

Str *Str_ReplaceAll(Str *ds, char from, char to)
{
    size_t i;
    size_t len = Str_Length(ds);

    if (!ds || !ds->str) return ds;

    for (i = 0; i < len; ++i)
    {
        if (ds->str[i] == from)
            ds->str[i] = to;
    }

    return ds;
}

char const *Str_GetLine(ddstring_t *str, char const *src)
{
    DE_ASSERT(str);
    if (!str) return 0;

    if (src != 0)
    {
        // We'll append the chars one by one.
        char buf[2];
        memset(buf, 0, sizeof(buf));
        for (Str_Clear(str); *src && *src != '\n'; src++)
        {
            if (*src != '\r')
            {
                buf[0] = *src;
                Str_Append(str, buf);
            }
        }

        // Strip whitespace around the line.
        Str_Strip(str);

        // The newline is excluded.
        if (*src == '\n')
            src++;
    }
    return src;
}

int Str_Compare(const ddstring_t *str, char const *text)
{
    DE_ASSERT(str);
    return strcmp(Str_Text(str), text);
}

int Str_CompareIgnoreCase(const ddstring_t *str, char const *text)
{
    DE_ASSERT(str);
    return strcasecmp(Str_Text(str), text);
}

char const *Str_CopyDelim2(ddstring_t *str, char const *src, char delimiter, int cdflags)
{
    DE_ASSERT(str);
    if (!str) return 0;

    Str_Clear(str);

    if (!src) return 0;

    { char const *cursor;
    ddstring_t buf; Str_Init(&buf);
    for (cursor = src; *cursor && *cursor != delimiter; ++cursor)
    {
        if ((cdflags & CDF_OMIT_WHITESPACE) && isspace(*cursor))
            continue;
        Str_PartAppend(&buf, cursor, 0, 1);
    }
    if (!Str_IsEmpty(&buf))
        Str_Copy(str, &buf);
    Str_Free(&buf);

    if (!*cursor)
        return 0; // It ended.

    if (!(cdflags & CDF_OMIT_DELIMITER))
        Str_PartAppend(str, cursor, 0, 1);

    // Skip past the delimiter.
    return cursor + 1;
    }
}

char const *Str_CopyDelim(ddstring_t *dest, char const *src, char delimiter)
{
    return Str_CopyDelim2(dest, src, delimiter, CDF_OMIT_DELIMITER | CDF_OMIT_WHITESPACE);
}

char Str_At(const ddstring_t *str, int index)
{
    DE_ASSERT(str);
    if (!str) return 0;

    if (index < 0 || index >= (int)str->length)
        return 0;
    return str->str[index];
}

char Str_RAt(const ddstring_t *str, int reverseIndex)
{
    DE_ASSERT(str);
    if (!str) return 0;

    if (reverseIndex < 0 || reverseIndex >= (int)str->length)
        return 0;
    return str->str[str->length - 1 - reverseIndex];
}

void Str_Truncate(ddstring_t *str, int position)
{
    DE_ASSERT(str);
    if (!str) return;

    if (position < 0)
        position = 0;
    if (!(position < Str_Length(str)))
        return;
    str->length = position;
    str->str[str->length] = '\0';
}

/// @note Derived from Qt's QByteArray q_toPercentEncoding
ddstring_t *Str_PercentEncode2(ddstring_t *str, char const *excludeChars, char const *includeChars)
{
    dd_bool didEncode = false;
    int i, span, begin, len;
    ddstring_t buf;

    DE_ASSERT(str);
    if (!str) return 0;

    if (Str_IsEmpty(str)) return str;

    len = Str_Length(str);
    begin = span = 0;
    for (i = 0; i < len; ++i)
    {
        char ch = str->str[i];

        // Are we encoding this?
        if (((ch >= 0x61 && ch <= 0x7A) // ALPHA
            || (ch >= 0x41 && ch <= 0x5A) // ALPHA
            || (ch >= 0x30 && ch <= 0x39) // DIGIT
            || ch == 0x2D // -
            || ch == 0x2E // .
            || ch == 0x5F // _
            || ch == 0x7E // ~
            || (excludeChars && strchr(excludeChars, ch)))
           && !(includeChars && strchr(includeChars, ch)))
        {
            // Not an encodeable. Span grows.
            span++;
        }
        else
        {
            // Found an encodeable.
            if (!didEncode)
            {
                Str_InitStd(&buf);
                Str_Reserve(&buf, len*3); // Worst case.
                didEncode = true;
            }

            Str_PartAppend(&buf, str->str, begin, span);
            Str_Appendf(&buf, "%%%X", (uint)ch);

            // Start a new span.
            begin += span + 1;
            span = 0;
        }
    }

    if (didEncode)
    {
        // Copy anything remaining.
        if (span)
        {
            Str_PartAppend(&buf, str->str, begin, span);
        }

        Str_Set(str, Str_Text(&buf));
        Str_Free(&buf);
    }

    return str;
}

ddstring_t *Str_PercentEncode(ddstring_t *str)
{
    return Str_PercentEncode2(str, 0/*no exclusions*/, 0/*no forced inclussions*/);
}

/// @note Derived from Qt's QByteArray q_fromPercentEncoding
ddstring_t *Str_PercentDecode(ddstring_t *str)
{
    int i, len, outlen, a, b;
    char const *inputPtr;
    char *data;
    char c;

    DE_ASSERT(str);
    if (!str) return 0;

    if (Str_IsEmpty(str)) return str;

    data = str->str;
    inputPtr = data;

    i = 0;
    len = Str_Length(str);
    outlen = 0;

    while (i < len)
    {
        c = inputPtr[i];
        if (c == '%' && i + 2 < len)
        {
            a = inputPtr[++i];
            b = inputPtr[++i];

            if (a >= '0' && a <= '9') a -= '0';
            else if (a >= 'a' && a <= 'f') a = a - 'a' + 10;
            else if (a >= 'A' && a <= 'F') a = a - 'A' + 10;

            if (b >= '0' && b <= '9') b -= '0';
            else if (b >= 'a' && b <= 'f') b  = b - 'a' + 10;
            else if (b >= 'A' && b <= 'F') b  = b - 'A' + 10;

            *data++ = (char)((a << 4) | b);
        }
        else
        {
            *data++ = c;
        }

        ++i;
        ++outlen;
    }

    if (outlen != len)
        Str_Truncate(str, outlen);

    return str;
}

void Str_Write(const ddstring_t *str, Writer1 *writer)
{
    size_t len = Str_Length(str);

    DE_ASSERT(str);

    Writer_WriteUInt32(writer, len);
    Writer_Write(writer, Str_Text(str), len);
}

void Str_Read(ddstring_t *str, Reader1 *reader)
{
    size_t len = Reader_ReadUInt32(reader);
    char *buf = malloc(len + 1);
    Reader_Read(reader, buf, len);
    buf[len] = 0;
    Str_Set(str, buf);
    free(buf);
}

AutoStr *AutoStr_New(void)
{
    return AutoStr_FromStr(Str_New());
}

AutoStr *AutoStr_NewStd(void)
{
    return AutoStr_FromStr(Str_NewStd());
}

void AutoStr_Delete(AutoStr *as)
{
    deleteString(as);
}

AutoStr *AutoStr_FromStr(Str *str)
{
    DE_ASSERT(str);
    Garbage_TrashInstance(str, (GarbageDestructor) AutoStr_Delete);
    return str;
}

AutoStr *AutoStr_FromText(char const *text)
{
    return Str_Set(AutoStr_New(), text);
}

AutoStr *AutoStr_FromTextStd(const char *text)
{
    return Str_Set(AutoStr_NewStd(), text);
}

ddstring_t *Str_FromAutoStr(AutoStr *as)
{
    DE_ASSERT(as);
    Garbage_Untrash(as);
    return as;
}
