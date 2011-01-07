/**\file m_string.c
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

#include <ctype.h>
#include <stdio.h>
#include <stdarg.h>

#include "de_base.h"
#include "de_console.h"

static void allocateString(ddstring_t *str, size_t for_length, int preserve)
{
    boolean old_data = false;
    char   *buf;

    // Include the terminating null character.
    for_length++;

    if(str->size >= for_length)
        return;                 // We're OK.

    // Already some memory allocated?
    if(str->size)
        old_data = true;
    else
        str->size = 1;

    while(str->size < for_length)
        str->size *= 2;
    buf = Z_Calloc(str->size, PU_APPSTATIC, 0);

    if(preserve && str->str)
        strncpy(buf, str->str, str->size - 1);

    // Replace the old string with the new buffer.
    if(old_data)
        Z_Free(str->str);
    str->str = buf;
}

void Str_Init(ddstring_t* str)
{
    if(!str)
    {
        Con_Error("Attempted String::Init with invalid reference (this==0).");
        return; // Unreachable.
    }
    memset(str, 0, sizeof(*str));
}

void Str_Free(ddstring_t* str)
{
    if(!str)
    {
        Con_Error("Attempted String::Free with invalid reference (this==0).");
        return; // Unreachable.
    }
    if(str->size)
    {
        // The string has memory allocated, free it.
        Z_Free(str->str);
    }
    memset(str, 0, sizeof(*str));
}

ddstring_t* Str_New(void)
{
    ddstring_t* str;
    if((str = Z_Calloc(sizeof(*str), PU_APPSTATIC, 0)) == 0)
    {
        Con_Error("String::New failed on allocation of %lu bytes.", (unsigned long) sizeof(*str));
        return 0; // Unreachable.
    }
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
    Z_Free(str);
}

void Str_Clear(ddstring_t* str)
{
    Str_Set(str, "");
}

void Str_Reserve(ddstring_t* str, size_t length)
{
    if(!str)
    {
        Con_Error("Attempted String::Reserve with invalid reference (this==0).");
        return; // Unreachable.
    }
    allocateString(str, length, true);
}

void Str_Set(ddstring_t* str, const char* text)
{
    if(!str)
    {
        Con_Error("Attempted String::Set with invalid reference (this==0).");
        return; // Unreachable.
    }
    { size_t incoming = strlen(text);
    allocateString(str, incoming, false);
    strcpy(str->str, text);
    str->length = incoming;
    }
}

void Str_Append(ddstring_t* str, const char* append)
{
    size_t incoming;
    if(!str)
    {
        Con_Error("Attempted String::Append with invalid reference (this==0).");
        return; // Unreachable.
    }
    // Don't allow extremely long strings.
    incoming = strlen(append);
    if(incoming == 0)
        return;
    if(str->length + incoming > DDSTRING_MAX_LENGTH)
    {
#if _DEBUG
        Con_Message("Resultant string would be longer than String::MAX_LENGTH (%ul).\n", (unsigned long) DDSTRING_MAX_LENGTH);
#endif
        return;
    }   
    allocateString(str, str->length + incoming, true);
    strcpy(str->str + str->length, append);
    str->length += incoming;
}

void Str_AppendChar(ddstring_t* str, char ch)
{
    char append[2] = { ch, 0 };
    Str_Append(str, append);
}

void Str_Appendf(ddstring_t* str, const char* format, ...)
{
    if(!str)
    {
        Con_Error("Attempted String::Appendf with invalid reference (this==0).");
        return; // Unreachable.
    }
    { char buf[1024];
    va_list args;

    // Print the message into the buffer.
    va_start(args, format);
    dd_vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    Str_Append(str, buf);
    }
}

void Str_PartAppend(ddstring_t* str, const char* append, size_t start, size_t count)
{
    if(!str)
    {
        Con_Error("Attempted String::PartAppend with invalid reference (this==0).");
        return; // Unreachable.
    }
    if(!append)
    {
#if _DEBUG
        Con_Message("Attempted String::PartAppend with invalid reference (@a append==0).\n");
#endif
        return;
    }
    if(count == 0)
        return;
    allocateString(str, str->length + count + 1, true);
    memcpy(str->str + str->length, append + start, count);
    str->length += count;

    // Terminate the appended part.
    str->str[str->length] = 0;
}

void Str_Prepend(ddstring_t* str, const char* prepend)
{
    size_t incoming;
    if(!str)
    {
        Con_Error("Attempted String::Prepend with invalid reference (this==0).");
        return; // Unreachable.
    }
    if(!prepend)
    {
#if _DEBUG
        Con_Message("Attempted String::PartAppend with invalid reference (@a prepend==0).\n");
#endif
        return;
    }
    incoming = strlen(prepend);
    if(incoming == 0)
        return;
    // Don't allow extremely long strings.
    if(str->length + incoming > DDSTRING_MAX_LENGTH)
    {
#if _DEBUG
        Con_Message("Resultant string would be longer than String::MAX_LENGTH (%ul).\n", (unsigned long) DDSTRING_MAX_LENGTH);
#endif
        return;
    }
    allocateString(str, str->length + incoming, true);
    memmove(str->str + incoming, str->str, str->length + 1);
    memcpy(str->str, prepend, incoming);
    str->length += incoming;
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

size_t Str_Length(const ddstring_t* str)
{
    if(!str)
    {
        Con_Error("Attempted String::Length with invalid reference (this==0).");
        return 0; // Unreachable.
    }
    if(str->length)
        return str->length;
    return strlen(Str_Text(str));
}

boolean Str_IsEmpty(const ddstring_t* str)
{
    return Str_Length(str) == 0;
}

void Str_Copy(ddstring_t* str, const ddstring_t* other)
{
    if(!str)
    {
        Con_Error("Attempted String::Copy with invalid reference (this==0).");
        return; // Unreachable.
    }
    Str_Free(str);
    if(!other)
    {
#if _DEBUG
        Con_Message("Attempted String::Copy with invalid reference (@a other==0).\n");
#endif
        return;
    }
    str->size = other->size;
    str->length = other->length;
    str->str = Z_Malloc(other->size, PU_APPSTATIC, 0);
    memcpy(str->str, other->str, other->size);
}

size_t Str_StripLeft(ddstring_t* str)
{
    size_t i, num;
    boolean isDone;

    if(!str)
    {
        Con_Error("Attempted String::StripLeft with invalid reference (this==0).");
        return 0; // Unreachable.
    }

    if(!str->length)
        return 0;

    // Find out how many whitespace chars are at the beginning.
    isDone = false;
    i = 0;
    num = 0;
    while(i < str->length && !isDone)
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
    return num;
}

size_t Str_StripRight(ddstring_t* str)
{
    if(!str)
    {
        Con_Error("Attempted String::StripRight with invalid reference (this==0).");
        return 0; // Unreachable.
    }

    if(str->length == 0)
        return 0;

    { size_t i = str->length - 1, num = 0;
    if(isspace(str->str[i]))
    do
    {
        // Remove this char.
        num++;
        str->str[i] = '\0';
        str->length--;
    } while(i != 0 && isspace(str->str[--i]));
    return num;
    }
}

void Str_Strip(ddstring_t* str)
{
    Str_StripLeft(str);
    Str_StripRight(str);
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

char Str_At(const ddstring_t* str, size_t index)
{
    if(!str)
    {
        Con_Error("Attempted String::At with invalid reference (this==0).");
        return 0; // Unreachable.
    }
    if(index >= str->length)
    {
        return 0;
    }
    return str->str[index];
}

char Str_RAt(const ddstring_t* str, size_t reverseIndex)
{
    if(!str)
    {
        Con_Error("Attempted String::RAt with invalid reference (this==0).");
        return 0; // Unreachable.
    }
    if(reverseIndex >= str->length)
    {
        return 0;
    }
    return str->str[str->length - 1 - reverseIndex];
}
