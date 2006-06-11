/* DE1: $Id$
 * Copyright (C) 2003, 2004 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * m_misc.c: Miscellanous Routines
 */

// HEADER FILES ------------------------------------------------------------

#include "de_platform.h"

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
#include <LZSS.h>

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_graphics.h"
#include "de_refresh.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

#define MALLOC_CLIB 1
#define MALLOC_ZONE 2

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static int FileReader(char const *name, byte **buffer, int mallocType);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int     read_count;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int read_ids[MAX_READ];

// CODE --------------------------------------------------------------------

void   *M_Malloc(size_t size)
{
    return malloc(size);
}

void   *M_Calloc(size_t size)
{
    return calloc(size, 1);
}

void   *M_Realloc(void *ptr, size_t size)
{
    return realloc(ptr, size);
}

void M_Free(void *ptr)
{
    free(ptr);
}

/*
 * Returns true if the given file can be read, or false if it has already
 * been read.
 */
boolean M_CheckFileID(const char *path)
{
    int     id = Dir_FileID(path);
    int     i;

    if(read_count >= MAX_READ)
    {
        Con_Message("CheckFile: Too many files.\n");
        return false;
    }
    if(!F_Access(path))
    {
        if(verbose)
            Con_Message("CheckFile: %s not found.\n", path);
        return false;
    }
    for(i = 0; i < read_count; i++)
        if(read_ids[i] == id)
            return false;
    read_ids[read_count++] = id;
    return true;
}

char   *M_SkipWhite(char *str)
{
    while(*str && ISSPACE(*str))
        str++;
    return str;
}

char   *M_FindWhite(char *str)
{
    while(*str && !ISSPACE(*str))
        str++;
    return str;
}

char   *M_SkipLine(char *str)
{
    while(*str && *str != '\n')
        str++;
    // If the newline was found, skip it, too.
    if(*str == '\n')
        str++;
    return str;
}

char   *M_LimitedStrCat(const char *str, unsigned int maxWidth, char separator,
                        char *buf, unsigned int bufLength)
{
    unsigned int isEmpty = !buf[0], length;

    // How long is this name?
    length = MIN_OF(maxWidth, strlen(str));

    // A separator is included if this is not the first name.
    if(separator && !isEmpty)
        ++length;

    // Does it fit?
    if(strlen(buf) + length < bufLength)
    {
        if(separator && !isEmpty)
        {
            char    sepBuf[2] = { separator, 0 };
            strcat(buf, sepBuf);
        }
        strncat(buf, str, length);
    }
    return buf;
}

/*
 * A limit has not been specified for the maximum length of the base,
 * so let's assume it can be a long one.
 */
void M_ExtractFileBase(const char *path, char *dest)
{
    M_ExtractFileBase2(path, dest, 255, 0);
}

/*
 * This has been modified to work with filenames of all sizes.
 */
void M_ExtractFileBase2(const char *path, char *dest, int max, int ignore)
{
    const char *src;

    src = path + strlen(path) - 1;

    // Back up until a \ or the start.
    while(src != path && *(src - 1) != '\\' && *(src - 1) != '/')
    {
        src--;
    }

    // Copy up to eight characters.
    while(*src && *src != '.' && max-- > 0)
    {
        if(ignore-- > 0)
        {
            src++; // Skip chars.
            max++; // Doesn't count.
        }
        else
            *dest++ = toupper((int) *src++);
    }

    // End with a terminating null.
    *dest++ = 0;
}

void M_ReadLine(char *buffer, int len, DFILE * file)
{
    int     p;
    char    ch;

    memset(buffer, 0, len);
    for(p = 0; p < len - 1;)    // Make the last null stay there.
    {
        ch = F_GetC(file);
        if(ch == '\r')
            continue;
        if(deof(file) || ch == '\n')
            break;
        buffer[p++] = ch;
    }
}

boolean M_IsComment(char *buffer)
{
    int     i = 0;

    while(isspace((unsigned char) buffer[i]) && buffer[i])
        i++;
    if(buffer[i] == '#')
        return true;
    return false;
}

// This is the new flat distribution table
unsigned char rndtable[256] = {
    201, 1, 243, 19, 18, 42, 183, 203, 101, 123, 154, 137, 34, 118, 10, 216,
    135, 246, 0, 107, 133, 229, 35, 113, 177, 211, 110, 17, 139, 84, 251, 235,
    182, 166, 161, 230, 143, 91, 24, 81, 22, 94, 7, 51, 232, 104, 122, 248,
    175, 138, 127, 171, 222, 213, 44, 16, 9, 33, 88, 102, 170, 150, 136, 114,
    62, 3, 142, 237, 6, 252, 249, 56, 74, 30, 13, 21, 180, 199, 32, 132,
    187, 234, 78, 210, 46, 131, 197, 8, 206, 244, 73, 4, 236, 178, 195, 70,
    121, 97, 167, 217, 103, 40, 247, 186, 105, 39, 95, 163, 99, 149, 253, 29,
    119, 83, 254, 26, 202, 65, 130, 155, 60, 64, 184, 106, 221, 93, 164, 196,
    112, 108, 179, 141, 54, 109, 11, 126, 75, 165, 191, 227, 87, 225, 156, 15,
    98, 162, 116, 79, 169, 140, 190, 205, 168, 194, 41, 250, 27, 20, 14, 241,
    50, 214, 72, 192, 220, 233, 67, 148, 96, 185, 176, 181, 215, 207, 172, 85,
    89, 90, 209, 128, 124, 2, 55, 173, 66, 152, 47, 129, 59, 43, 159, 240,
    239, 12, 189, 212, 144, 28, 200, 77, 219, 198, 134, 228, 45, 92, 125, 151,
    5, 53, 255, 52, 68, 245, 160, 158, 61, 86, 58, 82, 117, 37, 242, 145,
    69, 188, 115, 76, 63, 100, 49, 111, 153, 80, 38, 57, 174, 224, 71, 231,
    23, 25, 48, 218, 120, 147, 208, 36, 226, 223, 193, 238, 157, 204, 146, 31
};

int     rndindex = 0, rndindex2 = 0;

byte M_Random(void)
{
    if(rndindex > 255)
    {
        rndindex = 0;
        rndindex2++;
    }
    return rndtable[(++rndindex) & 0xff] ^ rndtable[rndindex2 & 0xff];
}

float M_FRandom(void)
{
    return (M_Random() | (M_Random() << 8)) / 65535.0f;
}

/*
 * Returns the value mod length (length > 0).
 */
float M_CycleIntoRange(float value, float length)
{
    if(value < 0)
    {
        return value - ((int) (value / length) - 1) * length;
    }
    if(value > length)
    {
        return value - ((int) (value / length)) * length;
    }
    return value;
}

/*
 * Normalize a vector. Returns the former length.
 */
float M_Normalize(float *a)
{
    float   len = sqrt(a[VX] * a[VX] + a[VY] * a[VY] + a[VZ] * a[VZ]);

    if(len)
    {
        a[VX] /= len;
        a[VY] /= len;
        a[VZ] /= len;
    }
    return len;
}

/*
 * For convenience.
 */
float M_Distance(float *a, float *b)
{
    float delta[3];
    int i;

    for(i = 0; i < 3; ++i)
        delta[i] = b[i] - a[i];

    return M_Normalize(delta);
}

float M_DotProduct(float *a, float *b)
{
    return a[VX] * b[VX] + a[VY] * b[VY] + a[VZ] * b[VZ];
}

void M_Scale(float *dest, const float *a, float scale)
{
    dest[VX] = a[VX] * scale;
    dest[VY] = a[VY] * scale;
    dest[VZ] = a[VZ] * scale;
}

/*
 * Cross product of two vectors.
 */
void M_CrossProduct(float *a, float *b, float *out)
{
    out[VX] = a[VY] * b[VZ] - a[VZ] * b[VY];
    out[VY] = a[VZ] * b[VX] - a[VX] * b[VZ];
    out[VZ] = a[VX] * b[VY] - a[VY] * b[VX];
}

/*
 * Cross product of two vectors composed of three points.
 */
void M_PointCrossProduct(float *v1, float *v2, float *v3, float *out)
{
    float   a[3], b[3];
    int     i;

    for(i = 0; i < 3; i++)
    {
        a[i] = v2[i] - v1[i];
        b[i] = v3[i] - v1[i];
    }
    M_CrossProduct(a, b, out);
}

/*
 * First yaw, then pitch. Two consecutive 2D rotations.
 * Probably could be done a bit more efficiently.
 */
void M_RotateVector(float vec[3], float degYaw, float degPitch)
{
    float   radYaw = degYaw / 180 * PI, radPitch = degPitch / 180 * PI;
    float   Cos, Sin, res[3];

    // Yaw.
    if(radYaw != 0)
    {
        Cos = cos(radYaw);
        Sin = sin(radYaw);
        res[VX] = vec[VX] * Cos + vec[VY] * Sin;
        res[VY] = vec[VX] * -Sin + vec[VY] * Cos;
        vec[VX] = res[VX];
        vec[VY] = res[VY];
    }
    // Pitch.
    if(radPitch != 0)
    {
        Cos = cos(radPitch);
        Sin = sin(radPitch);
        res[VZ] = vec[VZ] * Cos + vec[VX] * Sin;
        res[VX] = vec[VZ] * -Sin + vec[VX] * Cos;
        vec[VZ] = res[VZ];
        vec[VX] = res[VX];
    }
}

/*
 * Line a -> b, point c. The line must be exactly one unit long!
 */
float M_PointUnitLineDistance(float *a, float *b, float *c)
{
    return
        fabs(((a[VY] - c[VY]) * (b[VX] - a[VX]) -
              (a[VX] - c[VX]) * (b[VY] - a[VY])));
}

/*
 * Line a -> b, point c.
 */
float M_PointLineDistance(float *a, float *b, float *c)
{
    float   d[2], len;

    d[VX] = b[VX] - a[VX];
    d[VY] = b[VY] - a[VY];
    len = sqrt(d[VX] * d[VX] + d[VY] * d[VY]);  // Accurate.
    if(!len)
        return 0;
    return
        fabs(((a[VY] - c[VY]) * (b[VX] - a[VX]) -
              (a[VX] - c[VX]) * (b[VY] - a[VY])) / len);
}

/*
 * Input is fixed, output is floating point. Gap is the distance left
 * between the line and the projected point.
 */
void M_ProjectPointOnLinef(fixed_t *point, fixed_t *linepoint, fixed_t *delta,
                           float gap, float *result)
{
#define DOTPROD(a,b)    (a[VX]*b[VX] + a[VY]*b[VY])
    float   pointvec[2] = {
        FIX2FLT(point[VX] - linepoint[VX]),
        FIX2FLT(point[VY] - linepoint[VY])
    };
    float   line[2] = { FIX2FLT(delta[VX]), FIX2FLT(delta[VY]) };
    float   div = DOTPROD(line, line);
    float   diff[2], dist;

    if(!div)
        return;
    div = DOTPROD(pointvec, line) / div;
    result[VX] = FIX2FLT(linepoint[VX]) + line[VX] * div;
    result[VY] = FIX2FLT(linepoint[VY]) + line[VY] * div;

    // If a gap should be left, there is some extra math to do.
    if(gap)
    {
        diff[VX] = result[VX] - FIX2FLT(point[VX]);
        diff[VY] = result[VY] - FIX2FLT(point[VY]);
        if((dist = M_ApproxDistancef(diff[VX], diff[VY])) != 0)
        {
            int     i;

            for(i = 0; i < 2; i++)
                result[i] -= diff[i] / dist * gap;
        }
    }
}

float M_BoundingBoxDiff(float in[4], float out[4])
{
    return in[BLEFT] - out[BLEFT] + in[BTOP] - out[BTOP] + out[BRIGHT] -
        in[BRIGHT] + out[BBOTTOM] - in[BBOTTOM];
}

void M_ClearBox(fixed_t *box)
{
    box[BOXTOP] = box[BOXRIGHT] = DDMININT;
    box[BOXBOTTOM] = box[BOXLEFT] = DDMAXINT;
}

void M_AddToBox(fixed_t *box, fixed_t x, fixed_t y)
{
    if(x < box[BOXLEFT])
        box[BOXLEFT] = x;
    else if(x > box[BOXRIGHT])
        box[BOXRIGHT] = x;
    if(y < box[BOXBOTTOM])
        box[BOXBOTTOM] = y;
    else if(y > box[BOXTOP])
        box[BOXTOP] = y;
}

void M_JoinBoxes(float bbox[4], const float other[4])
{
    if(other[BLEFT] < bbox[BLEFT])
        bbox[BLEFT] = other[BLEFT];

    if(other[BRIGHT] > bbox[BRIGHT])
        bbox[BRIGHT] = other[BRIGHT];

    if(other[BTOP] < bbox[BTOP])
        bbox[BTOP] = other[BTOP];

    if(other[BBOTTOM] > bbox[BBOTTOM])
        bbox[BBOTTOM] = other[BBOTTOM];

}

#ifndef O_BINARY
#  define O_BINARY 0
#endif

boolean M_WriteFile(char const *name, void *source, int length)
{
    int     handle, count;

    handle = open(name, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0666);
    if(handle == -1)
        return false;
    count = write(handle, source, length);
    close(handle);

    if(count < length)
        return false;

    return true;
}

/*
 * Read a file into a buffer allocated using Z_Malloc().
 */
int M_ReadFile(char const *name, byte **buffer)
{
    return FileReader(name, buffer, MALLOC_ZONE);
}

/*
 * Read a file into a buffer allocated using malloc().
 */
int M_ReadFileCLib(char const *name, byte **buffer)
{
    return FileReader(name, buffer, MALLOC_CLIB);
}

static int FileReader(char const *name, byte **buffer, int mallocType)
{
    int     handle, count, length;
    struct stat fileinfo;
    byte   *buf;
    LZFILE *file;

    // First try with LZSS.
    if((file = lzOpen((char *) name, "rp")) != NULL)
    {
#define BSIZE 1024
        byte    rbuf[BSIZE];

        // Read 1kb pieces until file ends.
        length = 0;
        buf = 0;
        while(!lzEOF(file))
        {
            count = lzRead(rbuf, BSIZE, file);
            // Allocate more memory.
            if(mallocType == MALLOC_ZONE)
            {
                byte   *newbuf = Z_Malloc(length + count, PU_STATIC, 0);

                if(buf)
                {
                    memcpy(newbuf, buf, length);
                    Z_Free(buf);
                }
                buf = newbuf;
            }
            else
            {
                byte   *newbuf = realloc(buf, length + count);

                if(newbuf == NULL)
                    Con_Error("FileReader: realloc failed.");

                buf = newbuf;
            }
            // Copy new data to buffer.
            memcpy(buf + length, rbuf, count);
            length += count;
        }
        lzClose(file);
        *buffer = buf;
        return length;
    }

    handle = open(name, O_RDONLY | O_BINARY, 0666);
    if(handle == -1)
    {
        Con_Error("Couldn't read file %s\n", name);
    }
    if(fstat(handle, &fileinfo) == -1)
    {
        Con_Error("Couldn't read file %s\n", name);
    }
    length = fileinfo.st_size;
    if(mallocType == MALLOC_ZONE)
    {                           // Use zone memory allocation
        buf = Z_Malloc(length, PU_STATIC, NULL);
    }
    else
    {                           // Use c library memory allocation
        buf = malloc(length);
        if(buf == NULL)
        {
            Con_Error("Couldn't malloc buffer %d for file %s.\n", length,
                      name);
        }
    }
    count = read(handle, buf, length);
    close(handle);
    if(count < length)
    {
        Con_Error("Couldn't read file %s\n", name);
    }
    *buffer = buf;
    return length;
}

/*
 * Change string to uppercase.
 */
void M_ForceUppercase(char *text)
{
    char    c;

    while((c = *text) != 0)
    {
        if(c >= 'a' && c <= 'z')
        {
            *text++ = c - ('a' - 'A');
        }
        else
        {
            text++;
        }
    }
}

void M_WriteCommented(FILE * file, const char *text)
{
    char   *buff = malloc(strlen(text) + 1), *line;

    strcpy(buff, text);
    line = strtok(buff, "\n");
    while(line)
    {
        fprintf(file, "# %s\n", line);
        line = strtok(NULL, "\n");
    }
    free(buff);
}

/*
 * The caller must provide the opening and closing quotes.
 */
void M_WriteTextEsc(FILE * file, char *text)
{
    int     i;

    for(i = 0; text[i]; i++)
    {
        if(text[i] == '"' || text[i] == '\\')
            fprintf(file, "\\");
        fprintf(file, "%c", text[i]);
    }
}

/*
 * Gives an estimation of distance (not exact)
 */
#if 0
fixed_t M_AproxDistance(fixed_t dx, fixed_t dy)
{
    dx = abs(dx);
    dy = abs(dy);
    if(dx < dy)
        return dx + dy - (dx >> 1);
    return dx + dy - (dy >> 1);
}
#endif

float M_ApproxDistancef(float dx, float dy)
{
    dx = fabs(dx);
    dy = fabs(dy);
    if(dx < dy)
        return dx + dy - dx / 2;
    return dx + dy - dy / 2;
}

float M_ApproxDistance3(float delta[3])
{
    return M_ApproxDistancef(
        M_ApproxDistancef(delta[0], delta[1]), delta[2]);
}

float M_ApproxDistance3f(float dx, float dy, float dz)
{
    return M_ApproxDistancef(M_ApproxDistancef(dx, dy), dz);
}

/*
 * Writes a Targa file of the specified depth.
 */
int M_ScreenShot(char *filename, int bits)
{
    byte   *screen;

    if(bits != 16 && bits != 24)
        return false;

    // Grab that screen!
    screen = GL_GrabScreen();

    if(bits == 16)
        TGA_Save16_rgb888(filename, screenWidth, screenHeight, screen);
    else
        TGA_Save24_rgb888(filename, screenWidth, screenHeight, screen);

    free(screen);
    return true;
}

void M_PrependBasePath(const char *path, char *newpath)
{
    char    buf[300];

    if(Dir_IsAbsolute(path))
    {
        // Can't prepend to absolute paths.
        strcpy(newpath, path);
    }
    else
    {
        sprintf(buf, "%s%s", ddBasePath, path);
        strcpy(newpath, buf);
    }
}

/*
 * If the base path is found in the beginning of the path, it is removed.
 */
void M_RemoveBasePath(const char *absPath, char *newPath)
{
    if(!strnicmp(absPath, ddBasePath, strlen(ddBasePath)))
    {
        // This makes the new path relative to the base path.
        strcpy(newPath, absPath + strlen(ddBasePath));
    }
    else
    {
        // This doesn't appear to be the base path.
        strcpy(newPath, absPath);
    }
}

/*
 * Expands >.
 */
void M_TranslatePath(const char *path, char *translated)
{
    char    buf[300];

    if(path[0] == '>' || path[0] == '}')
    {
        path++;
        if(!Dir_IsAbsolute(path))
            M_PrependBasePath(path, buf);
        else
            strcpy(buf, path);
        strcpy(translated, buf);
    }
    else if(translated != path)
    {
        strcpy(translated, path);
    }
    Dir_FixSlashes(translated);
}

/*
 * Also checks for '>'.
 * The file must be a *real* file!
 */
int M_FileExists(const char *file)
{
    char    buf[256];

    M_TranslatePath(file, buf);
    return !access(buf, 4);     // Read permission?
}

/*
 * Check that the given directory exists. If it doesn't, create it.
 * The path must be relative!
 * Return true if the directory already exists.
 */
boolean M_CheckPath(char *path)
{
    char    full[256];
    char    buf[256], *ptr, *endptr;

    // Convert all backslashes to normal slashes.
    strcpy(full, path);
    Dir_FixSlashes(full);

    if(!access(full, 0))
        return true;            // Quick test.

    // Check and create the path in segments.
    ptr = full;
    memset(buf, 0, sizeof(buf));
    for(;;)
    {
        endptr = strchr(ptr, DIR_SEP_CHAR);
        if(!endptr)
            strcat(buf, ptr);
        else
            strncat(buf, ptr, endptr - ptr);
        if(access(buf, 0))
        {
            // Path doesn't exist, create it.
#if defined(WIN32)
            mkdir(buf);
#elif defined(UNIX)
            mkdir(buf, 0775);
#endif
        }
        strcat(buf, DIR_SEP_STR);
        ptr = endptr + 1;
        if(!endptr)
            break;
    }
    return false;
}

/*
 * The dot is not included in the returned extension.
 * The extension can be at most 10 characters long.
 */
void M_GetFileExt(const char *path, char *ext)
{
    char   *ptr = strrchr(path, '.');

    *ext = 0;
    if(!ptr)
        return;
    strncpy(ext, ptr + 1, 10);
    strlwr(ext);
}

/*
 * The new extension must not include a dot.
 */
void M_ReplaceFileExt(char *path, char *newext)
{
    char   *ptr = strrchr(path, '.');

    if(!ptr)
    {
        strcat(path, ".");
        strcat(path, newext);
    }
    else
    {
        strcpy(ptr + 1, newext);
    }
}

/*
 * Return a prettier copy of the original path.
 */
const char *M_Pretty(const char *path)
{
#define MAX_BUFS 8
    static char buffers[MAX_BUFS][256];
    static uint index = 0;
    char   *str;

    if(!strnicmp(path, ddBasePath, strlen(ddBasePath)))
    {
        str = buffers[index++ % MAX_BUFS];
        M_RemoveBasePath(path, str);
        Dir_FixSlashes(str);
        return str;
    }

    // We don't know how to make this prettier.
    return path;
}

/*
 * Concatenates src to dest as a quoted string.  " is escaped to \".
 * Returns dest.
 */
char* M_StrCatQuoted(char *dest, char *src)
{
    int     k = strlen(dest) + 1, i;

    strcat(dest, "\"");
    for(i = 0; src[i]; i++)
    {
        if(src[i] == '"')
        {
            strcat(dest, "\\\"");
            k += 2;
        }
        else
        {
            dest[k++] = src[i];
            dest[k] = 0;
        }
    }
    strcat(dest, "\"");
    return dest;
}

/*
 * Advances time and return true if the trigger is triggered.
 */
boolean M_CheckTrigger(trigger_t * trigger, timespan_t advanceTime)
{
    trigger->accum += advanceTime;

    if(trigger->accum >= trigger->duration)
    {
        trigger->accum -= trigger->duration;
        return true;
    }

    // It wasn't triggered.
    return false;
}

/*
 * Calculate CRC-32 for an arbitrary data buffer.
 */
uint M_CRC32(byte *data, uint length)
{
/* ====================================================================== */
/*  COPYRIGHT (C) 1986 Gary S. Brown.  You may use this program, or       */
/*  code or tables extracted from it, as desired without restriction.     */
/*                                                                        */
/*  First, the polynomial itself and its table of feedback terms.  The    */
/*  polynomial is                                                         */
/*  X^32+X^26+X^23+X^22+X^16+X^12+X^11+X^10+X^8+X^7+X^5+X^4+X^2+X^1+X^0   */
/*                                                                        */
/*  Note that we take it "backwards" and put the highest-order term in    */
/*  the lowest-order bit.  The X^32 term is "implied"; the LSB is the     */
/*  X^31 term, etc.  The X^0 term (usually shown as "+1") results in      */
/*  the MSB being 1.                                                      */
/*                                                                        */
/*  Note that the usual hardware shift register implementation, which     */
/*  is what we're using (we're merely optimizing it by doing eight-bit    */
/*  chunks at a time) shifts bits into the lowest-order term.  In our     */
/*  implementation, that means shifting towards the right.  Why do we     */
/*  do it this way?  Because the calculated CRC must be transmitted in    */
/*  order from highest-order term to lowest-order term.  UARTs transmit   */
/*  characters in order from LSB to MSB.  By storing the CRC this way,    */
/*  we hand it to the UART in the order low-byte to high-byte; the UART   */
/*  sends each low-bit to hight-bit; and the result is transmission bit   */
/*  by bit from highest- to lowest-order term without requiring any bit   */
/*  shuffling on our part.  Reception works similarly.                    */
/*                                                                        */
/*  The feedback terms table consists of 256, 32-bit entries.  Notes:     */
/*                                                                        */
/*      The table can be generated at runtime if desired; code to do so   */
/*      is shown later.  It might not be obvious, but the feedback        */
/*      terms simply represent the results of eight shift/xor opera-      */
/*      tions for all combinations of data and CRC register values.       */
/*                                                                        */
/*      The values must be right-shifted by eight bits by the "updcrc"    */
/*      logic; the shift must be unsigned (bring in zeroes).  On some     */
/*      hardware you could probably optimize the shift in assembler by    */
/*      using byte-swap instructions.                                     */
/*      polynomial $edb88320                                              */
/*                                                                        */
/*  --------------------------------------------------------------------  */

    static unsigned long crc32_tab[] = {
        0x00000000L, 0x77073096L, 0xee0e612cL, 0x990951baL, 0x076dc419L,
        0x706af48fL, 0xe963a535L, 0x9e6495a3L, 0x0edb8832L, 0x79dcb8a4L,
        0xe0d5e91eL, 0x97d2d988L, 0x09b64c2bL, 0x7eb17cbdL, 0xe7b82d07L,
        0x90bf1d91L, 0x1db71064L, 0x6ab020f2L, 0xf3b97148L, 0x84be41deL,
        0x1adad47dL, 0x6ddde4ebL, 0xf4d4b551L, 0x83d385c7L, 0x136c9856L,
        0x646ba8c0L, 0xfd62f97aL, 0x8a65c9ecL, 0x14015c4fL, 0x63066cd9L,
        0xfa0f3d63L, 0x8d080df5L, 0x3b6e20c8L, 0x4c69105eL, 0xd56041e4L,
        0xa2677172L, 0x3c03e4d1L, 0x4b04d447L, 0xd20d85fdL, 0xa50ab56bL,
        0x35b5a8faL, 0x42b2986cL, 0xdbbbc9d6L, 0xacbcf940L, 0x32d86ce3L,
        0x45df5c75L, 0xdcd60dcfL, 0xabd13d59L, 0x26d930acL, 0x51de003aL,
        0xc8d75180L, 0xbfd06116L, 0x21b4f4b5L, 0x56b3c423L, 0xcfba9599L,
        0xb8bda50fL, 0x2802b89eL, 0x5f058808L, 0xc60cd9b2L, 0xb10be924L,
        0x2f6f7c87L, 0x58684c11L, 0xc1611dabL, 0xb6662d3dL, 0x76dc4190L,
        0x01db7106L, 0x98d220bcL, 0xefd5102aL, 0x71b18589L, 0x06b6b51fL,
        0x9fbfe4a5L, 0xe8b8d433L, 0x7807c9a2L, 0x0f00f934L, 0x9609a88eL,
        0xe10e9818L, 0x7f6a0dbbL, 0x086d3d2dL, 0x91646c97L, 0xe6635c01L,
        0x6b6b51f4L, 0x1c6c6162L, 0x856530d8L, 0xf262004eL, 0x6c0695edL,
        0x1b01a57bL, 0x8208f4c1L, 0xf50fc457L, 0x65b0d9c6L, 0x12b7e950L,
        0x8bbeb8eaL, 0xfcb9887cL, 0x62dd1ddfL, 0x15da2d49L, 0x8cd37cf3L,
        0xfbd44c65L, 0x4db26158L, 0x3ab551ceL, 0xa3bc0074L, 0xd4bb30e2L,
        0x4adfa541L, 0x3dd895d7L, 0xa4d1c46dL, 0xd3d6f4fbL, 0x4369e96aL,
        0x346ed9fcL, 0xad678846L, 0xda60b8d0L, 0x44042d73L, 0x33031de5L,
        0xaa0a4c5fL, 0xdd0d7cc9L, 0x5005713cL, 0x270241aaL, 0xbe0b1010L,
        0xc90c2086L, 0x5768b525L, 0x206f85b3L, 0xb966d409L, 0xce61e49fL,
        0x5edef90eL, 0x29d9c998L, 0xb0d09822L, 0xc7d7a8b4L, 0x59b33d17L,
        0x2eb40d81L, 0xb7bd5c3bL, 0xc0ba6cadL, 0xedb88320L, 0x9abfb3b6L,
        0x03b6e20cL, 0x74b1d29aL, 0xead54739L, 0x9dd277afL, 0x04db2615L,
        0x73dc1683L, 0xe3630b12L, 0x94643b84L, 0x0d6d6a3eL, 0x7a6a5aa8L,
        0xe40ecf0bL, 0x9309ff9dL, 0x0a00ae27L, 0x7d079eb1L, 0xf00f9344L,
        0x8708a3d2L, 0x1e01f268L, 0x6906c2feL, 0xf762575dL, 0x806567cbL,
        0x196c3671L, 0x6e6b06e7L, 0xfed41b76L, 0x89d32be0L, 0x10da7a5aL,
        0x67dd4accL, 0xf9b9df6fL, 0x8ebeeff9L, 0x17b7be43L, 0x60b08ed5L,
        0xd6d6a3e8L, 0xa1d1937eL, 0x38d8c2c4L, 0x4fdff252L, 0xd1bb67f1L,
        0xa6bc5767L, 0x3fb506ddL, 0x48b2364bL, 0xd80d2bdaL, 0xaf0a1b4cL,
        0x36034af6L, 0x41047a60L, 0xdf60efc3L, 0xa867df55L, 0x316e8eefL,
        0x4669be79L, 0xcb61b38cL, 0xbc66831aL, 0x256fd2a0L, 0x5268e236L,
        0xcc0c7795L, 0xbb0b4703L, 0x220216b9L, 0x5505262fL, 0xc5ba3bbeL,
        0xb2bd0b28L, 0x2bb45a92L, 0x5cb36a04L, 0xc2d7ffa7L, 0xb5d0cf31L,
        0x2cd99e8bL, 0x5bdeae1dL, 0x9b64c2b0L, 0xec63f226L, 0x756aa39cL,
        0x026d930aL, 0x9c0906a9L, 0xeb0e363fL, 0x72076785L, 0x05005713L,
        0x95bf4a82L, 0xe2b87a14L, 0x7bb12baeL, 0x0cb61b38L, 0x92d28e9bL,
        0xe5d5be0dL, 0x7cdcefb7L, 0x0bdbdf21L, 0x86d3d2d4L, 0xf1d4e242L,
        0x68ddb3f8L, 0x1fda836eL, 0x81be16cdL, 0xf6b9265bL, 0x6fb077e1L,
        0x18b74777L, 0x88085ae6L, 0xff0f6a70L, 0x66063bcaL, 0x11010b5cL,
        0x8f659effL, 0xf862ae69L, 0x616bffd3L, 0x166ccf45L, 0xa00ae278L,
        0xd70dd2eeL, 0x4e048354L, 0x3903b3c2L, 0xa7672661L, 0xd06016f7L,
        0x4969474dL, 0x3e6e77dbL, 0xaed16a4aL, 0xd9d65adcL, 0x40df0b66L,
        0x37d83bf0L, 0xa9bcae53L, 0xdebb9ec5L, 0x47b2cf7fL, 0x30b5ffe9L,
        0xbdbdf21cL, 0xcabac28aL, 0x53b39330L, 0x24b4a3a6L, 0xbad03605L,
        0xcdd70693L, 0x54de5729L, 0x23d967bfL, 0xb3667a2eL, 0xc4614ab8L,
        0x5d681b02L, 0x2a6f2b94L, 0xb40bbe37L, 0xc30c8ea1L, 0x5a05df1bL,
        0x2d02ef8dL
    };

    unsigned int i;
    unsigned long crc32 = 0;

    for(i = 0; i < length; i++)
    {
        crc32 = crc32_tab[(crc32 ^ data[i]) & 0xff] ^ (crc32 >> 8);
    }
    return crc32;
}
