/** @file m_misc.cpp
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

/**
 * Miscellanous Routines.
 */

// HEADER FILES ------------------------------------------------------------

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

#if defined(WIN32)
#  include <direct.h>
#  include <io.h>
#  include <conio.h>
#endif

#if defined(UNIX)
#  include <unistd.h>
#  include <string.h>
#endif

#include <stdlib.h>
#include <ctype.h>
#include <math.h>

#include "de_platform.h"
#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_filesys.h"
#include "de_graphics.h"
#include "de_misc.h"
#include "de_play.h"

#include "lzss.h"

#undef M_WriteFile
#undef M_ReadFile

// MACROS ------------------------------------------------------------------

#define SLOPERANGE      2048
#define SLOPEBITS       11
#define DBITS           (FRACBITS-SLOPEBITS)

#if defined(WIN32)
#define close _close
#define read _read
#define write _write
#endif

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static size_t FileReader(char const* name, char** buffer);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int tantoangle[SLOPERANGE + 1];  // get from tables.c

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

void M_ReadLine(char* buffer, size_t len, FileHandle* file)
{
    size_t p;
    char ch;
    boolean isDone;

    memset(buffer, 0, len);
    p = 0;
    isDone = false;
    while(p < len - 1 && !isDone)    // Make the last null stay there.
    {
        ch = FileHandle_GetC(file);
        if(ch != '\r')
        {
            if(FileHandle_AtEnd(file) || ch == '\n')
                isDone = true;
            else
                buffer[p++] = ch;
        }
    }
}

int M_BoxOnLineSide(const AABoxd* box, double const linePoint[], double const lineDirection[])
{
    int a, b;

    switch(M_SlopeType(lineDirection))
    {
    default: // Shut up compiler.
    case ST_HORIZONTAL:
        a = box->maxY > linePoint[VY]? -1 : 1;
        b = box->minY > linePoint[VY]? -1 : 1;
        if(lineDirection[VX] < 0)
        {
            a = -a;
            b = -b;
        }
        break;

    case ST_VERTICAL:
        a = box->maxX < linePoint[VX]? -1 : 1;
        b = box->minX < linePoint[VX]? -1 : 1;
        if(lineDirection[VY] < 0)
        {
            a = -a;
            b = -b;
        }
        break;

    case ST_POSITIVE: {
        double topLeft[2]     = { box->minX, box->maxY };
        double bottomRight[2] = { box->maxX, box->minY };
        a = V2d_PointOnLineSide(topLeft,     linePoint, lineDirection) < 0 ? -1 : 1;
        b = V2d_PointOnLineSide(bottomRight, linePoint, lineDirection) < 0 ? -1 : 1;
        break; }

    case ST_NEGATIVE:
        a = V2d_PointOnLineSide(box->max, linePoint, lineDirection) < 0 ? -1 : 1;
        b = V2d_PointOnLineSide(box->min, linePoint, lineDirection) < 0 ? -1 : 1;
        break;
    }

    if(a == b) return a;
    return 0;
}

int M_BoxOnLineSide_FixedPrecision(const fixed_t box[], const fixed_t linePoint[], const fixed_t lineDirection[])
{
    int a, b;

    switch(M_SlopeTypeXY_FixedPrecision(lineDirection[0], lineDirection[1]))
    {
    case ST_HORIZONTAL:
        a = box[BOXTOP]    > linePoint[VY]? -1 : 1;
        b = box[BOXBOTTOM] > linePoint[VY]? -1 : 1;
        if(lineDirection[VX] < 0)
        {
            a = -a;
            b = -b;
        }
        break;

    case ST_VERTICAL:
        a = box[BOXRIGHT] < linePoint[VX]? -1 : 1;
        b = box[BOXLEFT]  < linePoint[VX]? -1 : 1;
        if(lineDirection[VY] < 0)
        {
            a = -a;
            b = -b;
        }
        break;

    case ST_POSITIVE: {
        fixed_t topLeft[2]     = { box[BOXLEFT],  box[BOXTOP]    };
        fixed_t bottomRight[2] = { box[BOXRIGHT], box[BOXBOTTOM] };
        a = V2x_PointOnLineSide(topLeft,     linePoint, lineDirection)? -1 : 1;
        b = V2x_PointOnLineSide(bottomRight, linePoint, lineDirection)? -1 : 1;
        break; }

    case ST_NEGATIVE: {
        fixed_t boxMax[2] = { box[BOXRIGHT], box[BOXTOP]    };
        fixed_t boxMin[2] = { box[BOXLEFT],  box[BOXBOTTOM] };
        a = V2x_PointOnLineSide(boxMax, linePoint, lineDirection)? -1 : 1;
        b = V2x_PointOnLineSide(boxMin, linePoint, lineDirection)? -1 : 1;
        break; }
    }

    if(a == b) return a;
    return 0; // on the line
}

int M_BoxOnLineSide2(const AABoxd* box, double const linePoint[], double const lineDirection[],
    double linePerp, double lineLength, double epsilon)
{
#define NORMALIZE(v)        ((v) < 0? -1 : (v) > 0? 1 : 0)

    double delta;
    int a, b;

    switch(M_SlopeType(lineDirection))
    {
    default: // Shut up compiler.
    case ST_HORIZONTAL:
        a = box->maxY > linePoint[VY]? -1 : 1;
        b = box->minY > linePoint[VY]? -1 : 1;
        if(lineDirection[VX] < 0)
        {
            a = -a;
            b = -b;
        }
        break;

    case ST_VERTICAL:
        a = box->maxX < linePoint[VX]? -1 : 1;
        b = box->minX < linePoint[VX]? -1 : 1;
        if(lineDirection[VY] < 0)
        {
            a = -a;
            b = -b;
        }
        break;

    case ST_POSITIVE: {
        double topLeft[2]       = { box->minX, box->maxY };
        double bottomRight[2]   = { box->maxX, box->minY };

        delta = V2d_PointOnLineSide2(topLeft,     lineDirection, linePerp, lineLength, epsilon);
        a = NORMALIZE(delta);

        delta = V2d_PointOnLineSide2(bottomRight, lineDirection, linePerp, lineLength, epsilon);
        b = NORMALIZE(delta);
        break; }

    case ST_NEGATIVE:
        delta = V2d_PointOnLineSide2(box->max, lineDirection, linePerp, lineLength, epsilon);
        a = NORMALIZE(delta);

        delta = V2d_PointOnLineSide2(box->min, lineDirection, linePerp, lineLength, epsilon);
        b = NORMALIZE(delta);
        break;
    }

    if(a == b) return a;
    return 0;

#undef NORMALIZE
}

#ifndef O_BINARY
#  define O_BINARY 0
#endif

DENG_EXTERN_C boolean M_WriteFile(const char* name, const char* source, size_t length)
{
    int handle = open(name, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0666);
    size_t count;

    if(handle == -1)
        return false;

    count = write(handle, source, length);
    close(handle);

    return (count >= length);
}

/**
 * Read a file into a buffer allocated using M_Malloc().
 */
DENG_EXTERN_C size_t M_ReadFile(const char* name, char** buffer)
{
    return FileReader(name, buffer);
}

static size_t FileReader(const char* name, char** buffer)
{
    struct stat fileinfo;
    char* buf = NULL;
    size_t length = 0;
    int handle;

    // First try with LZSS.
    LZFILE *file = lzOpen((char*) name, "rp");

    if(NULL != file)
    {
#define BSIZE 1024

        char readBuf[BSIZE];

        // Read 1kb pieces until file ends.
        while(!lzEOF(file))
        {
            size_t bytesRead = lzRead(readBuf, BSIZE, file);
            char* newBuf;

            // Allocate more memory.
            newBuf = (char*) Z_Malloc(length + bytesRead, PU_APPSTATIC, 0);
            if(NULL == newBuf)
                Con_Error("FileReader: realloc failed.");
            if(NULL != buf)
            {
                memcpy(newBuf, buf, length);
                Z_Free(buf);
            }
            buf = newBuf;

            // Copy new data to buffer.
            memcpy(buf + length, readBuf, bytesRead);
            length += bytesRead;
        }

        lzClose(file);
        *buffer = (char*)buf;
        return length;

#undef BSIZE
    }

    handle = open(name, O_RDONLY | O_BINARY, 0666);
    if(handle == -1)
    {
#if _DEBUG
        Con_Message("Warning:FileReader: Failed opening \"%s\" for reading.", name);
#endif
        return length;
    }

    if(-1 == fstat(handle, &fileinfo))
    {
        Con_Error("FileReader: Couldn't read file %s\n", name);
    }

    length = fileinfo.st_size;
    if(!length)
    {
        *buffer = 0;
        return 0;
    }

    buf = (char *) Z_Malloc(length, PU_APPSTATIC, 0);
    if(buf == NULL)
    {
        Con_Error("FileReader: Failed on allocation of %lu bytes for file \"%s\".\n",
                  (unsigned long) length, name);
    }

    { size_t bytesRead = read(handle, buf, length);
    close(handle);
    if(bytesRead < length)
    {
        Con_Error("FileReader: Couldn't read file \"%s\".\n", name);
    }}
    *buffer = buf;

    return length;
}

void M_WriteCommented(FILE *file, const char* text)
{
    char *buff = (char *) M_Malloc(strlen(text) + 1), *line;

    strcpy(buff, text);
    line = strtok(buff, "\n");
    while(line)
    {
        fprintf(file, "# %s\n", line);
        line = strtok(NULL, "\n");
    }
    M_Free(buff);
}

/**
 * The caller must provide the opening and closing quotes.
 */
void M_WriteTextEsc(FILE* file, const char* text)
{
    if(!file || !text)
    {
        Con_Error("Attempted M_WriteTextEsc with invalid reference (%s==0).", !file? "file":"text");
        return; // Unreachable.
    }

    { size_t i;
    for(i = 0; i < strlen(text) && text[i]; ++i)
    {
        if(text[i] == '"' || text[i] == '\\')
            fprintf(file, "\\");
        fprintf(file, "%c", text[i]);
    }}
}

DENG_EXTERN_C int M_ScreenShot(const char* name, int bits)
{
#ifdef __CLIENT__
    ddstring_t fullName;
    boolean result;

    DENG_UNUSED(bits);

    Str_Init(&fullName);
    Str_Set(&fullName, name);
    if(!_api_F.FindFileExtension(name))
    {
        Str_Append(&fullName, ".png"); // Default format.
    }
    F_ToNativeSlashes(&fullName, &fullName);
    result = Window_GrabToFile(Window_Main(), Str_Text(&fullName));
    Str_Free(&fullName);
    return result;
#else
    DENG2_UNUSED2(name, bits);
    return false;
#endif
}

void M_ReadBits(uint numBits, const uint8_t** src, uint8_t* cb, uint8_t* out)
{
    assert(src && cb && out);
    {
    int offset = 0, unread = numBits;

    // Read full bytes.
    if(unread >= 8)
    {
        do
        {
            out[offset++] = **src, (*src)++;
        } while((unread -= 8) >= 8);
    }

    if(unread != 0)
    {   // Read remaining bits.
        uint8_t fb = 8 - unread;

        if((*cb) == 0)
            (*cb) = 8;

        do
        {
            (*cb)--;
            out[offset] <<= 1;
            out[offset] |= ((**src >> (*cb)) & 0x01);
        } while(--unread > 0);

        out[offset] <<= fb;

        if((*cb) == 0)
            (*src)++;
    }
    }
}

boolean M_RunTrigger(trigger_t *trigger, timespan_t advanceTime)
{
    // Either use the trigger's duration, or fall back to the default.
    timespan_t duration = (trigger->duration? trigger->duration : 1.0f/35);

    trigger->accum += advanceTime;

    if(trigger->accum >= duration)
    {
        trigger->accum -= duration;
        return true;
    }

    // It wasn't triggered.
    return false;
}

boolean M_CheckTrigger(const trigger_t *trigger, timespan_t advanceTime)
{
    // Either use the trigger's duration, or fall back to the default.
    timespan_t duration = (trigger->duration? trigger->duration : 1.0f/35);
    return (trigger->accum + advanceTime>= duration);
}
