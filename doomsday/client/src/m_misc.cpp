/** @file m_misc.cpp  Miscellanous utility routines.
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

#define DENG_NO_API_MACROS_FILESYS

#include "de_platform.h"
#include "de_console.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

#if defined(WIN32)
#  include <direct.h>
#  include <io.h>
#  include <conio.h>
#  define open _open
#endif

#if defined(UNIX)
#  include <unistd.h>
#  include <string.h>
#endif

#ifndef O_BINARY
#  define O_BINARY 0
#endif

#include "de_base.h"
#include "de_system.h"
#include "de_filesys.h"
#include "de_graphics.h"
#include "de_misc.h"
#include "de_play.h"

#include "lzss.h"

#include <de/str.h>
#include <cstdlib>
#include <cctype>
#include <cmath>

#undef M_WriteFile
#undef M_ReadFile

#define SLOPERANGE      2048
#define SLOPEBITS       11
#define DBITS           (FRACBITS-SLOPEBITS)

using namespace de;

static size_t FileReader(char const* name, char** buffer);

extern int tantoangle[SLOPERANGE + 1];  // get from tables.c

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
    int a = 0, b = 0;

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

/**
 * Read a file into a buffer allocated using M_Malloc().
 */
DENG_EXTERN_C size_t M_ReadFile(const char* name, char** buffer)
{
    return FileReader(name, buffer);
}

DENG_EXTERN_C AutoStr *M_ReadFileIntoString(ddstring_t const *path, dd_bool *isCustom)
{
    if(isCustom) *isCustom = false;

    if(Str_StartsWith(path, "LumpIndex:"))
    {
        bool isNumber;
        lumpnum_t const lumpNum    = String(Str_Text(path) + 10).toInt(&isNumber);
        LumpIndex const &lumpIndex = App_FileSystem().nameIndex();
        if(isNumber && lumpIndex.hasLump(lumpNum))
        {
            File1 &lump = lumpIndex.lump(lumpNum);
            if(isCustom) *isCustom = lump.hasCustom();

            // Ignore zero-length lumps.
            if(!lump.size()) return 0;

            // Ensure the resulting string is terminated.
            AutoStr *string = Str_PartAppend(AutoStr_NewStd(), (char const *)lump.cache(), 0, lump.size());
            lump.unlock();

            if(Str_IsEmpty(string))
                return 0;

            return string;
        }

        return 0;
    }

    if(Str_StartsWith(path, "Lumps:"))
    {
        char const *lumpName       = Str_Text(path) + 6;
        LumpIndex const &lumpIndex = App_FileSystem().nameIndex();
        if(!lumpIndex.contains(String(lumpName) + ".lmp"))
            return 0;

        File1 &lump = lumpIndex[lumpIndex.findLast(String(lumpName) + ".lmp")];
        if(isCustom) *isCustom = lump.hasCustom();

        // Ignore zero-length lumps.
        if(!lump.size()) return 0;

        // Ensure the resulting string is terminated.
        AutoStr *string = Str_PartAppend(AutoStr_NewStd(), (char const *)lump.cache(), 0, lump.size());
        lump.unlock();

        if(Str_IsEmpty(string))
            return 0;

        return string;
    }

    // Try the virtual file system.
    try
    {
        QScopedPointer<FileHandle> hndl(&App_FileSystem().openFile(Str_Text(path), "rb"));

        if(isCustom) *isCustom = hndl->file().hasCustom();

        // Ignore zero-length lumps.
        AutoStr *string = nullptr;
        if(size_t lumpLength = hndl->length())
        {
            // Read in the whole thing and ensure the resulting string is terminated.
            Block buffer;
            buffer.resize(lumpLength);
            hndl->read((uint8_t *)buffer.data(), lumpLength);
            string = Str_PartAppend(AutoStr_NewStd(), buffer.constData(), 0, lumpLength);
        }

        App_FileSystem().releaseFile(hndl->file());

        if(!string || Str_IsEmpty(string))
            return 0;

        return string;
    }
    catch(FS1::NotFoundError const &)
    {} // Ignore this error.


    // Perhaps a local file known to the native file system?
    char *readBuf = 0;
    if(size_t bytesRead = M_ReadFile(Str_Text(path), &readBuf))
    {
        // Ensure the resulting string is terminated.
        AutoStr *string = Str_PartAppend(AutoStr_New(), readBuf, 0, int(bytesRead));
        Z_Free(readBuf);

        // Ignore zero-length files.
        if(Str_IsEmpty(string))
            return 0;

        return string;
    }

    return 0;
}

#if defined(WIN32)
#define close _close
#define read _read
#define write _write
#endif

static size_t FileReader(const char* name, char** buffer)
{
    struct stat fileinfo;
    char* buf = NULL;
    size_t length = 0;
    int handle;

    LOG_AS("FileReader");

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
            if(buf != NULL)
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
        LOG_RES_WARNING("Failed opening \"%s\" for reading") << name;
        return length;
    }

    if(-1 == fstat(handle, &fileinfo))
    {
        LOG_RES_ERROR("Couldn't read file \"%s\"") << name;
        return 0;
    }

    length = fileinfo.st_size;
    if(!length)
    {
        *buffer = 0;
        return 0;
    }

    buf = (char *) Z_Malloc(length, PU_APPSTATIC, 0);
    DENG_ASSERT(buf != 0);

    size_t bytesRead = read(handle, buf, length);
    close(handle);
    if(bytesRead < length)
    {
        LOG_RES_ERROR("Couldn't read file \"%s\"") << name;
    }
    *buffer = buf;

    return length;
}

DENG_EXTERN_C dd_bool M_WriteFile(const char* name, const char* source, size_t length)
{
    int handle = open(name, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0666);
    size_t count;

    if(handle == -1)
        return false;

    count = write(handle, source, length);
    close(handle);

    return (count >= length);
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
    DENG_ASSERT(file && text);

    size_t i;
    for(i = 0; i < strlen(text) && text[i]; ++i)
    {
        if(text[i] == '"' || text[i] == '\\')
            fprintf(file, "\\");
        fprintf(file, "%c", text[i]);
    }
}

DENG_EXTERN_C int M_ScreenShot(char const *name, int bits)
{
#ifdef __CLIENT__
    DENG2_UNUSED(bits);

    de::String fullName(name);
    if(fullName.fileNameExtension().isEmpty())
    {
        fullName += ".png"; // Default format.
    }

    return ClientWindow::main().grabToFile(fullName)? 1 : 0;
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

dd_bool M_RunTrigger(trigger_t *trigger, timespan_t advanceTime)
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

dd_bool M_CheckTrigger(const trigger_t *trigger, timespan_t advanceTime)
{
    // Either use the trigger's duration, or fall back to the default.
    timespan_t duration = (trigger->duration? trigger->duration : 1.0f/35);
    return (trigger->accum + advanceTime>= duration);
}
