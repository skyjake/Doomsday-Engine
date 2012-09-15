/**\file m_misc.c
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
#include "de_refresh.h"
#include "de_misc.h"
#include "de_play.h"

#include "lzss.h"

// MACROS ------------------------------------------------------------------

#define SLOPERANGE      2048
#define SLOPEBITS       11
#define DBITS           (FRACBITS-SLOPEBITS)

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

char* M_SkipWhite(char* str)
{
    while(*str && ISSPACE(*str))
        str++;
    return str;
}

char* M_FindWhite(char* str)
{
    while(*str && !ISSPACE(*str))
        str++;
    return str;
}

void M_StripLeft(char* str)
{
    size_t len, num;
    if(NULL == str || !str[0]) return;

    len = strlen(str);
    // Count leading whitespace characters.
    num = 0;
    while(num < len && isspace(str[num]))
        ++num;
    if(0 == num) return;

    // Remove 'num' characters.
    memmove(str, str + num, len - num);
    str[len] = 0;
}

void M_StripRight(char* str, size_t len)
{
    char* end;
    int numZeroed = 0;
    if(NULL == str || 0 == len) return;

    end = str + strlen(str) - 1;
    while(end >= str && isspace(*end))
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
    while(*str && *str != '\n')
        str++;
    // If the newline was found, skip it, too.
    if(*str == '\n')
        str++;
    return str;
}

char* M_StrCat(char* buf, const char* str, size_t bufSize)
{
    return M_StrnCat(buf, str, strlen(str), bufSize);
}

char* M_StrnCat(char* buf, const char* str, size_t nChars, size_t bufSize)
{
    int n = nChars;
    int destLen = strlen(buf);
    if((int)bufSize - destLen - 1 < n)
    {
        // Cannot copy more than fits in the buffer.
        // The 1 is for the null character.
        n = bufSize - destLen - 1;
    }
    if(n <= 0) return buf; // No space left.
    return strncat(buf, str, n);
}

char* M_LimitedStrCat(char* buf, const char* str, size_t maxWidth,
                      char separator, size_t bufLength)
{
    boolean         isEmpty = !buf[0];
    size_t          length;

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
            char            sepBuf[2];

            sepBuf[0] = separator;
            sepBuf[1] = 0;

            strcat(buf, sepBuf);
        }
        strncat(buf, str, length);
    }

    return buf;
}

void M_ReadLine(char* buffer, size_t len, DFile* file)
{
    size_t p;
    char ch;
    boolean isDone;

    memset(buffer, 0, len);
    p = 0;
    isDone = false;
    while(p < len - 1 && !isDone)    // Make the last null stay there.
    {
        ch = DFile_GetC(file);
        if(ch != '\r')
        {
            if(DFile_AtEnd(file) || ch == '\n')
                isDone = true;
            else
                buffer[p++] = ch;
        }
    }
}

boolean M_IsComment(const char* buffer)
{
    int i = 0;

    while(isspace((unsigned char) buffer[i]) && buffer[i])
        i++;
    if(buffer[i] == '#')
        return true;
    return false;
}

/// \note Part of the Doomsday public API
boolean M_IsStringValidInt(const char* str)
{
    size_t i, len;
    const char* c;
    boolean isBad;

    if(!str)
        return false;

    len = strlen(str);
    if(len == 0)
        return false;

    for(i = 0, c = str, isBad = false; i < len && !isBad; ++i, c++)
    {
        if(i != 0 && *c == '-')
            isBad = true; // sign is in the wrong place.
        else if(*c < '0' || *c > '9')
            isBad = true; // non-numeric character.
    }

    return !isBad;
}

/// \note Part of the Doomsday public API
boolean M_IsStringValidByte(const char* str)
{
    if(M_IsStringValidInt(str))
    {
        int val = atoi(str);
        if(!(val < 0 || val > 255))
            return true;
    }
    return false;
}

/// \note Part of the Doomsday public API
boolean M_IsStringValidFloat(const char* str)
{
    size_t i, len;
    const char* c;
    boolean isBad, foundDP = false;

    if(!str)
        return false;

    len = strlen(str);
    if(len == 0)
        return false;

    for(i = 0, c = str, isBad = false; i < len && !isBad; ++i, c++)
    {
        if(i != 0 && *c == '-')
            isBad = true; // sign is in the wrong place.
        else if(*c == '.')
        {
            if(foundDP)
                isBad = true; // multiple decimal places??
            else
                foundDP = true;
        }
        else if(*c < '0' || *c > '9')
            isBad = true; // other non-numeric character.
    }

    return !isBad;
}

// This is the new flat distribution table
static unsigned char rngTable[256] = {
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

static int rngIndex = 0, rngIndex2 = 0;

byte RNG_RandByte(void)
{
    if(rngIndex > 255)
    {
        rngIndex = 0;
        rngIndex2++;
    }
    return rngTable[(++rngIndex) & 0xff] ^ rngTable[rngIndex2 & 0xff];
}

float RNG_RandFloat(void)
{
    return (RNG_RandByte() | (RNG_RandByte() << 8)) / 65535.0f;
}

void RNG_Reset(void)
{
    rngIndex = 0, rngIndex2 = 0;
}

int M_RatioReduce(int* numerator, int* denominator)
{
    int n, d, temp;

    if(!numerator || !denominator)
    {
#if _DEBUG
        Con_Message("Warning: M_RatioReduce: Invalid arguments, returning 1:1.\n");
#endif
        return 1;
    }

    if(*numerator == *denominator) return 1; // 1:1

    n = abs(*numerator);
    d = abs(*denominator);
    // Ensure numerator is larger.
    if(n < d)
    {
        temp = n;
        n = d;
        d = temp;
    }

    // Reduce to the common divisor.
    while(d != 0)
    {
        temp = n;
        n = d;
        d = temp % d;
    }

    // Apply divisor.
    *numerator   /= n;
    *denominator /= n;
    return n;
}

/**
 * Finds the power of 2 that is equal to or greater than the specified
 * number.
 */
int M_CeilPow2(int num)
{
    int         cumul;

    for(cumul = 1; num > cumul; cumul <<= 1);

    return cumul;
}

/**
 * Finds the power of 2 that is less than or equal to the specified number.
 */
int M_FloorPow2(int num)
{
    int         fl = M_CeilPow2(num);

    if(fl > num)
        fl >>= 1;
    return fl;
}

/**
 * Finds the power of 2 that is nearest the specified number. In ambiguous
 * cases, the smaller number is returned.
 */
int M_RoundPow2(int num)
{
    int         cp2 = M_CeilPow2(num), fp2 = M_FloorPow2(num);

    return ((cp2 - num >= num - fp2) ? fp2 : cp2);
}

/**
 * Weighted rounding. Weight determines the point where the number is still
 * rounded down (0..1).
 */
int M_WeightPow2(int num, float weight)
{
    int         fp2 = M_FloorPow2(num);
    float       frac = (num - fp2) / (float) fp2;

    if(frac <= weight)
        return fp2;
    else
        return (fp2 << 1);
}

/**
 * @return          value mod length (length > 0).
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

double M_DirectionToAngleXY(double dx, double dy)
{
    double angle;

    if(dx == 0)
        return (dy > 0? 90.0 : 270.0);

    angle = atan2((double) dy, (double) dx) * 180.0 / PI_D;

    if(angle < 0)
        angle += 360.0;

    return angle;
}

double M_DirectionToAngle(double const direction[])
{
    return M_DirectionToAngleXY(direction[VX], direction[VY]);
}

double M_InverseAngle(double angle)
{
    if(angle < 180.0)
    {
        return (180.0 - -angle);
    }
    return angle - 180.0;
}

slopetype_t M_SlopeTypeXY(double dx, double dy)
{
    if(FEQUAL(dx, 0))
    {
        return ST_VERTICAL;
    }
    else if(FEQUAL(dy, 0))
    {
        return ST_HORIZONTAL;
    }
    else if(dy / dx > 0)
    {
        return ST_POSITIVE;
    }
    else
    {
        return ST_NEGATIVE;
    }
}

slopetype_t M_SlopeType(double const direction[])
{
    return M_SlopeTypeXY(direction[VX], direction[VY]);
}

int M_NumDigits(int value)
{
    return floor(log10(abs(value))) + 1;
}

double M_TriangleArea(double const v1[], double const v2[], double const v3[])
{
    double a[2], b[2];
    double area;

    V2d_Subtract(a, v2, v1);
    V2d_Subtract(b, v3, v1);

    area = (a[VX] * b[VY] - b[VX] * a[VY]) / 2;
    if(area < 0)
        return -area;
    return area;
}

/**
 * First yaw, then pitch. Two consecutive 2D rotations.
 * Probably could be done a bit more efficiently.
 */
void M_RotateVector(float vec[3], float degYaw, float degPitch)
{
    float radYaw = degYaw / 180 * PI, radPitch = degPitch / 180 * PI;
    float Cos, Sin, res[3];

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

float M_BoundingBoxDiff(const float in[4], const float out[4])
{
    return in[BOXLEFT]    - out[BOXLEFT] +
           in[BOXBOTTOM]  - out[BOXBOTTOM] +
           out[BOXRIGHT]  - in[BOXRIGHT] +
           out[BOXTOP]    - in[BOXTOP];
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
    if(other[BOXLEFT] < bbox[BOXLEFT])
        bbox[BOXLEFT] = other[BOXLEFT];

    if(other[BOXRIGHT] > bbox[BOXRIGHT])
        bbox[BOXRIGHT] = other[BOXRIGHT];

    if(other[BOXTOP] > bbox[BOXTOP])
        bbox[BOXTOP] = other[BOXTOP];

    if(other[BOXBOTTOM] < bbox[BOXBOTTOM])
        bbox[BOXBOTTOM] = other[BOXBOTTOM];
}

void M_CopyBox(fixed_t dest[4], const fixed_t src[4])
{
    dest[BOXLEFT]   = src[BOXLEFT];
    dest[BOXRIGHT]  = src[BOXRIGHT];
    dest[BOXBOTTOM] = src[BOXBOTTOM];
    dest[BOXTOP]    = src[BOXTOP];
}

#ifndef O_BINARY
#  define O_BINARY 0
#endif

boolean M_WriteFile(const char* name, const char* source, size_t length)
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
size_t M_ReadFile(const char* name, char** buffer)
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
    { LZFILE* file = lzOpen((char*) name, "rp");
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
    }}

    handle = open(name, O_RDONLY | O_BINARY, 0666);
    if(handle == -1)
    {
#if _DEBUG
        Con_Message("Warning:FileReader: Failed opening \"%s\" for reading.\n", name);
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

    buf = Z_Malloc(length, PU_APPSTATIC, 0);
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

/**
 * Change string to uppercase.
 */
void M_ForceUppercase(char *text)
{
    char                c;

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

void M_WriteCommented(FILE *file, const char* text)
{
    char*               buff = M_Malloc(strlen(text) + 1), *line;

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

static int slopeDiv(unsigned num, unsigned den)
{
    uint ans;

    if(den < 512)
        return SLOPERANGE;
    ans = (num << 3) / (den >> 8);
    return ans <= SLOPERANGE ? ans : SLOPERANGE;
}

angle_t M_PointToAngle(double const point[])
{
    fixed_t pos[2];

    pos[VX] = FLT2FIX(point[VX]);
    pos[VY] = FLT2FIX(point[VY]);

    if(pos[VX] == 0 && pos[VY] == 0)
        return 0;

    if(pos[VX] >= 0)
    {
        // x >=0
        if(pos[VY] >= 0)
        {
            // y>= 0
            if(pos[VX] > pos[VY])
                return tantoangle[slopeDiv(pos[VY], pos[VX])]; // octant 0

            return ANG90 - 1 - tantoangle[slopeDiv(pos[VX], pos[VY])]; // octant 1
        }

        // y<0
        pos[VY] = -pos[VY];
        if(pos[VX] > pos[VY])
            return -tantoangle[slopeDiv(pos[VY], pos[VX])]; // octant 8

        return ANG270 + tantoangle[slopeDiv(pos[VX], pos[VY])]; // octant 7
    }

    // x<0
    pos[VX] = -pos[VX];
    if(pos[VY] >= 0)
    {
        // y>= 0
        if(pos[VX] > pos[VY])
            return ANG180 - 1 - tantoangle[slopeDiv(pos[VY], pos[VX])]; // octant 3

        return ANG90 + tantoangle[slopeDiv(pos[VX], pos[VY])]; // octant 2
    }

    // y<0
    pos[VY] = -pos[VY];
    if(pos[VX] > pos[VY])
        return ANG180 + tantoangle[slopeDiv(pos[VY], pos[VX])]; // octant 4

    return ANG270 - 1 - tantoangle[slopeDiv(pos[VX], pos[VY])]; // octant 5
}

angle_t M_PointXYToAngle(double x, double y)
{
    double point[2] = { x, y };
    return M_PointToAngle(point);
}

angle_t M_PointToAngle2(double const a[], double const b[])
{
    double delta[2] = { b[VX] - a[VX], b[VY] - a[VY] };
    return M_PointToAngle(delta);
}

angle_t M_PointXYToAngle2(double aX, double aY, double bX, double bY)
{
    double a[2] = { aX, aY };
    double b[2] = { bX, bY };
    return M_PointToAngle2(a, b);
}

double M_PointDistance(double const a[], double const b[])
{
    double delta[2];
    uint angle;

    delta[VX] = fabs(b[VX] - a[VX]);
    delta[VY] = fabs(b[VY] - a[VY]);

    if(delta[VY] > delta[VX])
    {
        double temp = delta[VX];
        delta[VX] = delta[VY];
        delta[VY] = temp;
    }

    angle = (tantoangle[FLT2FIX(delta[VY] / delta[VX]) >> DBITS] + ANG90) >> ANGLETOFINESHIFT;
    return delta[VX] / FIX2FLT(finesine[angle]); // Use as cosine
}

double M_PointXYDistance(double aX, double aY, double bX, double bY)
{
    double a[2] = { aX, aY };
    double b[2] = { bX, bY };
    return M_PointDistance(a, b);
}

double M_ApproxDistance(double dx, double dy)
{
    dx = fabs(dx);
    dy = fabs(dy);
    if(dx < dy)
        return dx + dy - dx / 2;
    return dx + dy - dy / 2;
}

float M_ApproxDistancef(float dx, float dy)
{
    dx = fabs(dx);
    dy = fabs(dy);
    if(dx < dy)
        return dx + dy - dx / 2;
    return dx + dy - dy / 2;
}

double M_ApproxDistance3(double dx, double dy, double dz)
{
    return M_ApproxDistance(M_ApproxDistance(dx, dy), dz);
}

float M_ApproxDistance3f(float dx, float dy, float dz)
{
    return M_ApproxDistancef(M_ApproxDistancef(dx, dy), dz);
}

int M_ScreenShot(const char* name, int bits)
{
    ddstring_t fullName;
    boolean result;

    Str_Init(&fullName);
    Str_Set(&fullName, name);
    if(!F_FindFileExtension(name))
    {
        Str_Append(&fullName, ".png"); // Default format.
    }
    F_ToNativeSlashes(&fullName, &fullName);
    result = Window_GrabToFile(Window_Main(), Str_Text(&fullName));
    Str_Free(&fullName);
    return result;

    /*
    byte* screen = (byte*) GL_GrabScreen();
    Str fullName;
    FILE* file;

    if(!screen)
    {
        Con_Message("Warning:M_ScreenShot: Failed acquiring frame buffer content.\n");
        return false;
    }

    // Compose the final file name.
    Str_InitStd(&fullName); Str_Set(&fullName, name);
    if(!F_FindFileExtension(Str_Text(&fullName)))
        Str_Append(&fullName, ".tga");
    F_ToNativeSlashes(&fullName, &fullName);

    file = fopen(Str_Text(&fullName), "wb");
    if(!file)
    {
        Con_Message("Warning: M_Screenshot: Failed opening \"%s\" for write.\n", F_PrettyPath(Str_Text(&fullName)));
        Str_Free(&fullName);
        return false;
    }

    bits = (bits == 24? 24 : 16);
    if(bits == 16)
        TGA_Save16_rgb888(file, Window_Width(theWindow), Window_Height(theWindow), screen);
    else
        TGA_Save24_rgb888(file, Window_Width(theWindow), Window_Height(theWindow), screen);

    fclose(file);
    Str_Free(&fullName);
    free(screen);
    return true;
    */
}

/**
 * Concatenates src to dest as a quoted string. " is escaped to \".
 * Returns dest.
 */
char* M_StrCatQuoted(char* dest, const char* src, size_t len)
{
    size_t              k = strlen(dest) + 1, i;

    strncat(dest, "\"", len);
    for(i = 0; src[i]; i++)
    {
        if(src[i] == '"')
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

/**
 * Somewhat similar to strtok().
 */
char* M_StrTok(char** cursor, char* delimiters)
{
    char*               begin = *cursor;

    while(**cursor && !strchr(delimiters, **cursor))
        (*cursor)++;

    if(**cursor)
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
    for(ptr += strlen(ptr) - 1; ptr >= trimmedFloatBuffer; ptr--)
    {
        if(*ptr == '0')
            *ptr = 0;
        else if(*ptr == '.')
        {
            *ptr = 0;
            break;
        }
        else
            break;
    }
    return trimmedFloatBuffer;
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

    for(i = 0; i < length; ++i)
    {
        crc32 = crc32_tab[(crc32 ^ data[i]) & 0xff] ^ (crc32 >> 8);
    }
    return crc32;
}
