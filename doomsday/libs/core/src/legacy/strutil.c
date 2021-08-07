/** @file strutil.c String and text utilities.
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

#include "de/legacy/strutil.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>

#if WIN32
# define strcasecmp _stricmp
#else
# include <strings.h>
#endif

int dd_vsnprintf(char *str, size_t size, char const *format, va_list ap)
{
    int result = vsnprintf(str, size, format, ap);

#ifdef WIN32
    // Always terminate.
    str[size - 1] = 0;
    return result;
#else
    return result >= (int)size? -1 : (int)size;
#endif
}

int dd_snprintf(char *str, size_t size, char const *format, ...)
{
    int result = 0;

    va_list args;
    va_start(args, format);
    result = dd_vsnprintf(str, size, format, args);
    va_end(args);

    return result;
}

#ifdef WIN32
const char* strcasestr(const char *text, const char *sub)
{
    int textLen = strlen(text);
    int subLen = strlen(sub);
    int i;

    for (i = 0; i <= textLen - subLen; ++i)
    {
        const char* start = text + i;
        if (!_strnicmp(start, sub, subLen)) return start;
    }
    return 0;
}
#endif

#ifdef UNIX
char* strupr(char* string)
{
    char* ch = string;
    for (; *ch; ch++) *ch = toupper(*ch);
    return string;
}
char* strlwr(char* string)
{
    char* ch = string;
    for (; *ch; ch++) *ch = tolower(*ch);
    return string;
}
#endif // UNIX

const char *M_SkipWhite(const char *str)
{
    while (*str && DE_ISSPACE(*str))
        str++;
    return str;
}

const char *M_FindWhite(const char *str)
{
    while (*str && !DE_ISSPACE(*str))
        str++;
    return str;
}

void M_StripLeft(char* str)
{
    size_t len, num;
    if (NULL == str || !str[0]) return;

    len = strlen(str);
    // Count leading whitespace characters.
    num = 0;
    while (num < len && isspace((int)str[num]))
        ++num;
    if (0 == num) return;

    // Remove 'num' characters.
    memmove(str, str + num, len - num);
    str[len] = 0;
}

void M_StripRight(char* str, size_t len)
{
    char* end;
    int numZeroed = 0;
    if (NULL == str || 0 == len) return;

    end = str + strlen(str) - 1;
    while (end >= str && isspace((int)*end))
    {
        end--;
        numZeroed++;
    }
    memset(end + 1, 0, numZeroed);
}

void M_Strip(char* str, size_t len)
{
    M_StripLeft(str);
    M_StripRight(str, len);
}

char* M_SkipLine(char* str)
{
    while (*str && *str != '\n')
        str++;
    // If the newline was found, skip it, too.
    if (*str == '\n')
        str++;
    return str;
}

dd_bool M_IsStringValidInt(const char* str)
{
    size_t i, len;
    const char* c;
    dd_bool isBad;

    if (!str)
        return false;

    len = strlen(str);
    if (len == 0)
        return false;

    for (i = 0, c = str, isBad = false; i < len && !isBad; ++i, c++)
    {
        if (i != 0 && *c == '-')
            isBad = true; // sign is in the wrong place.
        else if (*c < '0' || *c > '9')
            isBad = true; // non-numeric character.
    }

    return !isBad;
}

dd_bool M_IsStringValidByte(const char* str)
{
    if (M_IsStringValidInt(str))
    {
        int val = atoi(str);
        if (!(val < 0 || val > 255))
            return true;
    }
    return false;
}

dd_bool M_IsStringValidFloat(const char* str)
{
    size_t i, len;
    const char* c;
    dd_bool isBad, foundDP = false;

    if (!str)
        return false;

    len = strlen(str);
    if (len == 0)
        return false;

    for (i = 0, c = str, isBad = false; i < len && !isBad; ++i, c++)
    {
        if (i != 0 && *c == '-')
            isBad = true; // sign is in the wrong place.
        else if (*c == '.')
        {
            if (foundDP)
                isBad = true; // multiple decimal places??
            else
                foundDP = true;
        }
        else if (*c < '0' || *c > '9')
            isBad = true; // other non-numeric character.
    }

    return !isBad;
}

char* M_StrCat(char* buf, const char* str, size_t bufSize)
{
    return M_StrnCat(buf, str, strlen(str), bufSize);
}

char* M_StrnCat(char* buf, const char* str, size_t nChars, size_t bufSize)
{
    int n = nChars;
    int destLen = strlen(buf);
    if ((int)bufSize - destLen - 1 < n)
    {
        // Cannot copy more than fits in the buffer.
        // The 1 is for the null character.
        n = bufSize - destLen - 1;
    }
    if (n <= 0) return buf; // No space left.
    return strncat(buf, str, n);
}

char* M_StrCatQuoted(char* dest, const char* src, size_t len)
{
    size_t k = strlen(dest) + 1, i;

    strncat(dest, "\"", len);
    for (i = 0; src[i]; i++)
    {
        if (src[i] == '"')
        {
            strncat(dest, "\\\"", len);
            k += 2;
        }
        else
        {
            dest[k++] = src[i];
            dest[k] = 0;
        }
    }
    strncat(dest, "\"", len);

    return dest;
}

char* M_LimitedStrCat(char* buf, const char* str, size_t maxWidth,
                      char separator, size_t bufLength)
{
    dd_bool         isEmpty = !buf[0];
    size_t          length;

    // How long is this name?
    length = MIN_OF(maxWidth, strlen(str));

    // A separator is included if this is not the first name.
    if (separator && !isEmpty)
        ++length;

    // Does it fit?
    if (strlen(buf) + length < bufLength)
    {
        if (separator && !isEmpty)
        {
            char            sepBuf[2];

            sepBuf[0] = separator;
            sepBuf[1] = 0;

            strcat(buf, sepBuf);
        }
        strncat(buf, str, length);
    }

    return buf;
}

void M_ForceUppercase(char *text)
{
    char c;

    while ((c = *text) != 0)
    {
        if (c >= 'a' && c <= 'z')
        {
            *text++ = c - ('a' - 'A');
        }
        else
        {
            text++;
        }
    }
}

char* M_StrTok(char** cursor, const char* delimiters)
{
    char* begin = *cursor;

    while (**cursor && !strchr(delimiters, **cursor))
        (*cursor)++;

    if (**cursor)
    {
        // Stop here.
        **cursor = 0;

        // Advance one more so we'll start from the right character on
        // the next call.
        (*cursor)++;
    }

    return begin;
}

char* M_TrimmedFloat(float val)
{
    static char trimmedFloatBuffer[32];
    char* ptr = trimmedFloatBuffer;

    sprintf(ptr, "%f", val);
    // Get rid of the extra zeros.
    for (ptr += strlen(ptr) - 1; ptr >= trimmedFloatBuffer; ptr--)
    {
        if (*ptr == '0')
            *ptr = 0;
        else if (*ptr == '.')
        {
            *ptr = 0;
            break;
        }
        else
            break;
    }
    return trimmedFloatBuffer;
}
