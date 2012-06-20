/**\file m_string.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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

#include <ctype.h>
#include <stdio.h>
#include <stdarg.h>

#include "de_base.h"
#include "de_console.h"
#include "de_misc.h"

static void* zoneAlloc(size_t n) {
    return Z_Malloc(n, PU_APPSTATIC, 0);
}

static void* zoneCalloc(size_t n) {
    return Z_Calloc(n, PU_APPSTATIC, 0);
}

static void* stdCalloc(size_t n) {
    return calloc(1, n);
}

static void autoselectMemoryManagement(ddstring_t* str)
{
    if(!str->memFree && !str->memAlloc && !str->memCalloc)
    {
        // If the memory model is unspecified, default to the standard,
        // it is safer for threading.
        str->memFree = free;
        str->memAlloc = malloc;
        str->memCalloc = stdCalloc;
    }
    assert(str->memFree);
    assert(str->memAlloc);
    assert(str->memCalloc);
}

static void allocateString(ddstring_t *str, size_t for_length, int preserve)
{
    boolean old_data = false;
    char   *buf;

    // Include the terminating null character.
    for_length++;

    if(str->size >= for_length)
        return; // We're OK.

    autoselectMemoryManagement(str);

    // Already some memory allocated?
    if(str->size)
        old_data = true;
    else
        str->size = 1;

    while(str->size < for_length)
        str->size *= 2;

    assert(str->memCalloc);
    buf = str->memCalloc(str->size);

    if(preserve && str->str)
        strncpy(buf, str->str, str->size - 1);

    // Replace the old string with the new buffer.
    if(old_data)
    {
        assert(str->memFree);
        str->memFree(str->str);
    }
    str->str = buf;
}

/**
 * Call this for uninitialized strings. Global variables are
 * automatically cleared, so they don't need initialization.
 * The string will use the memory zone.
 */
ddstring_t* Str_Init(ddstring_t* str)
{
    if(!str)
    {
        Con_Error("Attempted String::Init with invalid reference (this==0).");
        exit(1); // Unreachable.
    }

    if(!Z_IsInited())
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
ddstring_t* Str_InitStd(ddstring_t* str)
{
    memset(str, 0, sizeof(*str));

    // Init the memory management.
    str->memFree = free;
    str->memAlloc = malloc;
    str->memCalloc = stdCalloc;
    return str;
}

ddstring_t* Str_InitStatic(ddstring_t* str, const char* staticConstStr)
{
    memset(str, 0, sizeof(*str));
    str->str = (char*) staticConstStr;
    str->length = (staticConstStr? strlen(staticConstStr) : 0);
    return str;
}

void Str_Free(ddstring_t* str)
{
    if(!str)
    {
        Con_Error("Attempted String::Free with invalid reference (this==0).");
        return; // Unreachable.
    }

    autoselectMemoryManagement(str);

    if(str->size)
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
    ddstring_t* str = (ddstring_t*) M_Calloc(sizeof(ddstring_t));
    Str_InitStd(str);
    return str;
}

ddstring_t* Str_New(void)
{
    ddstring_t* str = (ddstring_t*) M_Calloc(sizeof(ddstring_t));
    Str_Init(str);
    return str;
}

ddstring_t* Str_NewFromReader(Reader* reader)
{
    ddstring_t* str = Str_New();
    Str_Read(str, reader);
    return str;
}

void Str_Delete(ddstring_t* str)
{
    if(!str)
    {
        Con_Error("Attempted String::Delete with invalid reference (this==0).");
        return; // Unreachable.
    }
    Str_Free(str);
    M_Free(str);
}

ddstring_t* Str_Clear(ddstring_t* str)
{
    return Str_Set(str, "");
}

ddstring_t* Str_Reserve(ddstring_t* str, int length)
{
    if(!str)
    {
        Con_Error("Attempted String::Reserve with invalid reference (this==0).");
        exit(1); // Unreachable.
    }
    if(length > 0)
    {
        allocateString(str, length, true);
    }
    return str;
}

ddstring_t* Str_ReserveNotPreserving(ddstring_t* str, int length)
{
    if(!str)
    {
        Con_Error("Attempted String::ReserveNotPreserving with invalid reference (this==0).");
        exit(1); // Unreachable.
    }
    if(length > 0)
    {
        allocateString(str, length, false);
    }
    return str;
}

ddstring_t* Str_Set(ddstring_t* str, const char* text)
{
    if(!str)
    {
        Con_Error("Attempted String::Set with invalid reference (this==0).");
        return str; // Unreachable.
    }

    {
    size_t incoming = strlen(text);
    char* copied = M_Malloc(incoming + 1); // take a copy in case text points to (a part of) str->str
    strcpy(copied, text);
    allocateString(str, incoming, false);
    strcpy(str->str, copied);
    str->length = incoming;
    M_Free(copied);
    return str;
    }
}

ddstring_t* Str_AppendWithoutAllocs(ddstring_t* str, const ddstring_t* append)
{
    assert(str);
    assert(append);
    assert(str->length + append->length + 1 <= str->size); // including the null

    memcpy(str->str + str->length, append->str, append->length);
    str->length += append->length;
    str->str[str->length] = 0;
    return str;
}

ddstring_t* Str_AppendCharWithoutAllocs(ddstring_t* str, char ch)
{
    assert(str);
    assert(ch); // null not accepted
    assert(str->length + 2 <= str->size); // including a terminating null

    str->str[str->length++] = ch;
    str->str[str->length] = 0;
    return str;
}

ddstring_t* Str_Append(ddstring_t* str, const char* append)
{
    size_t incoming;
    char* copied;

    if(!str)
    {
        Con_Error("Attempted String::Append with invalid reference (this==0).");
        return str; // Unreachable.
    }

    incoming = strlen(append);

    // Take a copy in case append_text points to (a part of) ds->str, which may
    // be invalidated by allocateString.
    copied = M_Malloc(incoming + 1);

    strcpy(copied, append);
    allocateString(str, str->length + incoming, true);
    strcpy(str->str + str->length, copied);
    str->length += incoming;

    M_Free(copied);
    return str;
}

ddstring_t* Str_AppendChar(ddstring_t* str, char ch)
{
    char append[2];
    append[0] = ch;
    append[1] = '\0';
    return Str_Append(str, append);
}

ddstring_t* Str_Appendf(ddstring_t* str, const char* format, ...)
{
    if(!str)
    {
        Con_Error("Attempted String::Appendf with invalid reference (this==0).");
        return str; // Unreachable.
    }

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

ddstring_t* Str_PartAppend(ddstring_t* str, const char* append, int start, int count)
{
    int partLen;
    char* copied;

    if(!str)
    {
        Con_Error("Attempted String::PartAppend with invalid reference (this==0).");
        return str; // Unreachable.
    }
    if(!append)
    {
#if _DEBUG
        Con_Message("Attempted String::PartAppend with invalid reference (@a append==0).\n");
#endif
        return str;
    }
    if(start < 0 || count <= 0)
        return str;

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

ddstring_t* Str_Prepend(ddstring_t* str, const char* prepend)
{
    char* copied;
    size_t incoming;
    if(!str)
    {
        Con_Error("Attempted String::Prepend with invalid reference (this==0).");
        return str; // Unreachable.
    }
    if(!prepend)
    {
#if _DEBUG
        Con_Message("Attempted String::PartAppend with invalid reference (@a prepend==0).\n");
#endif
        return str;
    }
    incoming = strlen(prepend);
    if(incoming == 0)
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

ddstring_t* Str_PrependChar(ddstring_t* str, char ch)
{
    char prepend[2];
    prepend[0] = ch;
    prepend[1] = '\0';
    return Str_Prepend(str, prepend);
}

char* Str_Text(const ddstring_t* str)
{
    if(!str)
    {
        Con_Error("Attempted String::Text with invalid reference (this==0).");
        return 0; // Unreachable.
    }
    return str->str ? str->str : "";
}

int Str_Length(const ddstring_t* str)
{
    if(!str)
    {
        Con_Error("Attempted String::Length with invalid reference (this==0).");
        return 0; // Unreachable.
    }
    if(str->length)
        return str->length;
    return (int)strlen(Str_Text(str));
}

boolean Str_IsEmpty(const ddstring_t* str)
{
    return Str_Length(str) == 0;
}

ddstring_t* Str_Copy(ddstring_t* str, const ddstring_t* other)
{
    if(!str)
    {
        Con_Error("Attempted String::Copy with invalid reference (this==0).");
        return str; // Unreachable.
    }
    Str_Free(str);
    if(!other)
    {
#if _DEBUG
        Con_Message("Attempted String::Copy with invalid reference (@a other==0).\n");
#endif
        return str;
    }
    str->size = other->size;
    str->length = other->length;
    assert(str->memAlloc);
    str->str = str->memAlloc(other->size);
    memcpy(str->str, other->str, other->size);
    return str;
}

ddstring_t* Str_CopyOrClear(ddstring_t* dest, const ddstring_t* src)
{
    if(!dest)
    {
        Con_Error("Attempted String::CopyOrClear with invalid reference (this==0).");
        exit(1); // Unreachable.
    }
    if(src)
    {
        return Str_Copy(dest, src);
    }
    return Str_Clear(dest);
}

ddstring_t* Str_StripLeft2(ddstring_t* str, int* count)
{
    int i, num;
    boolean isDone;

    if(!str)
    {
        Con_Error("Attempted String::StripLeft with invalid reference (this==0).");
        exit(1); // Unreachable.
    }

    if(!str->length)
    {
        if(count) *count = 0;
        return str;
    }

    // Find out how many whitespace chars are at the beginning.
    isDone = false;
    i = 0;
    num = 0;
    while(i < (int)str->length && !isDone)
    {
        if(isspace(str->str[i]))
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
        memmove(str->str, str->str + num, str->length - num);
        str->length -= num;
        str->str[str->length] = 0;
    }

    if(count) *count = num;
    return str;
}

ddstring_t* Str_StripLeft(ddstring_t* str)
{
    return Str_StripLeft2(str, NULL/*not interested in the stripped character count*/);
}

ddstring_t* Str_StripRight2(ddstring_t* str, int* count)
{
    int i, num;

    if(!str)
    {
        Con_Error("Attempted String::StripRight with invalid reference (this==0).");
        exit(1); // Unreachable.
    }

    if(str->length == 0)
    {
        if(count) *count = 0;
        return str;
    }

    i = str->length - 1;
    num = 0;
    if(isspace(str->str[i]))
    do
    {
        // Remove this char.
        num++;
        str->str[i] = '\0';
        str->length--;
    } while(i != 0 && isspace(str->str[--i]));

    if(count) *count = num;
    return str;
}

ddstring_t* Str_StripRight(ddstring_t* str)
{
    return Str_StripRight2(str, NULL/*not interested in the stripped character count*/);
}

ddstring_t* Str_Strip2(ddstring_t* str, int* count)
{
    int right_count, left_count;
    Str_StripLeft2(Str_StripRight2(str, &right_count), &left_count);
    if(count) *count = right_count + left_count;
    return str;
}

ddstring_t* Str_Strip(ddstring_t* str)
{
    return Str_Strip2(str, NULL/*not interested in the stripped character count*/);
}

const char* Str_GetLine(ddstring_t* str, const char* src)
{
    if(!str)
    {
        Con_Error("Attempted String::GetLine with invalid reference (this==0).");
        return 0; // Unreachable.
    }
    if(src != 0)
    {
        // We'll append the chars one by one.
        char buf[2];
        memset(buf, 0, sizeof(buf));
        for(Str_Clear(str); *src && *src != '\n'; src++)
        {
            if(*src != '\r')
            {
                buf[0] = *src;
                Str_Append(str, buf);
            }
        }

        // Strip whitespace around the line.
        Str_Strip(str);

        // The newline is excluded.
        if(*src == '\n')
            src++;
    }
    return src;
}

int Str_Compare(const ddstring_t* str, const char* text)
{
    return strcmp(Str_Text(str), text);
}

int Str_CompareIgnoreCase(const ddstring_t* str, const char* text)
{
    return strcasecmp(Str_Text(str), text);
}

const char* Str_CopyDelim2(ddstring_t* str, const char* src, char delimiter, int cdflags)
{
    if(!str)
    {
        Con_Error("Attempted String::CopyDelim2 with invalid reference (this==0).");
        return 0; // Unreachable.
    }
    Str_Clear(str);
    if(!src)
        return 0;

    { const char* cursor;
    ddstring_t buf; Str_Init(&buf);
    for(cursor = src; *cursor && *cursor != delimiter; ++cursor)
    {
        if((cdflags & CDF_OMIT_WHITESPACE) && isspace(*cursor))
            continue;
        Str_PartAppend(&buf, cursor, 0, 1);
    }
    if(!Str_IsEmpty(&buf))
        Str_Copy(str, &buf);
    Str_Free(&buf);

    if(!*cursor)
        return 0; // It ended.

    if(!(cdflags & CDF_OMIT_DELIMITER))
        Str_PartAppend(str, cursor, 0, 1);

    // Skip past the delimiter.
    return cursor + 1;
    }
}

const char* Str_CopyDelim(ddstring_t* dest, const char* src, char delimiter)
{
    return Str_CopyDelim2(dest, src, delimiter, CDF_OMIT_DELIMITER|CDF_OMIT_WHITESPACE);
}

char Str_At(const ddstring_t* str, int index)
{
    if(!str)
    {
        Con_Error("Attempted String::At with invalid reference (=NULL).");
        exit(1); // Unreachable.
    }
    if(index < 0 || index >= (int)str->length)
        return 0;
    return str->str[index];
}

char Str_RAt(const ddstring_t* str, int reverseIndex)
{
    if(!str)
    {
        Con_Error("Attempted String::RAt with invalid reference (=NULL).");
        exit(1); // Unreachable.
    }
    if(reverseIndex < 0 || reverseIndex >= (int)str->length)
        return 0;
    return str->str[str->length - 1 - reverseIndex];
}

void Str_Truncate(ddstring_t* str, int position)
{
    if(position < 0)
        position = 0;
    if(!(position < Str_Length(str)))
        return;
    str->length = position;
    str->str[str->length] = '\0';
}

/// @note Derived from Qt's QByteArray q_toPercentEncoding
ddstring_t* Str_PercentEncode2(ddstring_t* str, const char* excludeChars, const char* includeChars)
{
    boolean didEncode = false;
    int i, span, begin, len;
    ddstring_t buf;

    if(!str)
    {
        Con_Error("Attempted String::PercentEncode with invalid reference (=NULL).");
        exit(1); // Unreachable.
    }

    if(Str_IsEmpty(str)) return str;

    len = Str_Length(str);
    begin = span = 0;
    for(i = 0; i < len; ++i)
    {
        char ch = str->str[i];

        // Are we encoding this?
        if(((ch >= 0x61 && ch <= 0x7A) // ALPHA
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
            if(!didEncode)
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

    if(didEncode)
    {
        // Copy anything remaining.
        if(span)
        {
            Str_PartAppend(&buf, str->str, begin, span);
        }

        Str_Set(str, Str_Text(&buf));
        Str_Free(&buf);
    }

    return str;
}

ddstring_t* Str_PercentEncode(ddstring_t* str)
{
    return Str_PercentEncode2(str, 0/*no exclusions*/, 0/*no forced inclussions*/);
}

/// @note Derived from Qt's QByteArray q_fromPercentEncoding
ddstring_t* Str_PercentDecode(ddstring_t* str)
{
    int i, len, outlen, a, b;
    const char* inputPtr;
    char* data;
    char c;

    if(!str)
    {
        Con_Error("Attempted String::PercentDecode with invalid reference (=NULL).");
        exit(1); // Unreachable.
    }

    if(Str_IsEmpty(str)) return str;

    data = str->str;
    inputPtr = data;

    i = 0;
    len = Str_Length(str);
    outlen = 0;

    while(i < len)
    {
        c = inputPtr[i];
        if(c == '%' && i + 2 < len)
        {
            a = inputPtr[++i];
            b = inputPtr[++i];

            if(a >= '0' && a <= '9') a -= '0';
            else if(a >= 'a' && a <= 'f') a = a - 'a' + 10;
            else if(a >= 'A' && a <= 'F') a = a - 'A' + 10;

            if(b >= '0' && b <= '9') b -= '0';
            else if(b >= 'a' && b <= 'f') b  = b - 'a' + 10;
            else if(b >= 'A' && b <= 'F') b  = b - 'A' + 10;

            *data++ = (char)((a << 4) | b);
        }
        else
        {
            *data++ = c;
        }

        ++i;
        ++outlen;
    }

    if(outlen != len)
        Str_Truncate(str, outlen);

    return str;
}

void Str_Write(const ddstring_t* str, Writer* writer)
{
    size_t len = Str_Length(str);
    Writer_WriteUInt32(writer, len);
    Writer_Write(writer, Str_Text(str), len);
}

void Str_Read(ddstring_t* str, Reader* reader)
{
    size_t len = Reader_ReadUInt32(reader);
    char* buf = malloc(len + 1);
    Reader_Read(reader, buf, len);
    buf[len] = 0;
    Str_Set(str, buf);
    free(buf);
}

AutoStr* AutoStr_New(void)
{
    return AutoStr_FromStr(Str_New());
}

AutoStr* AutoStr_NewStd(void)
{
    return AutoStr_FromStr(Str_NewStd());
}

AutoStr* AutoStr_FromStr(ddstring_t* str)
{
    Garbage_TrashInstance(str, (GarbageDestructor) Str_Delete);
    return str;
}

ddstring_t* Str_FromAutoStr(AutoStr* as)
{
    Garbage_Untrash(as);
    return as;
}
