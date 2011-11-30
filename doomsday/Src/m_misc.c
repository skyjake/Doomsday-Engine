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

#if defined(WIN32)
#  include <direct.h>
#  include <io.h>
#  include <conio.h>
#endif

#include "de_platform.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

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

//===========================================================================
// M_Malloc
//===========================================================================
void   *M_Malloc(size_t size)
{
	return malloc(size);
}

//===========================================================================
// M_Calloc
//===========================================================================
void   *M_Calloc(size_t size)
{
	return calloc(size, 1);
}

//===========================================================================
// M_Realloc
//===========================================================================
void   *M_Realloc(void *ptr, size_t size)
{
	return realloc(ptr, size);
}

//===========================================================================
// M_Free
//===========================================================================
void M_Free(void *ptr)
{
	free(ptr);
}

//==========================================================================
// M_CheckFileID
//  Returns true if the given file can be read, or false if it has already
//  been read.
//==========================================================================
boolean M_CheckFileID(const char *path)
{
	int     id = Dir_FileID(path);
	int     i;

	if(read_count == MAX_READ)
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

//===========================================================================
// M_SkipWhite
//===========================================================================
char   *M_SkipWhite(char *str)
{
	while(*str && ISSPACE(*str))
		str++;
	return str;
}

//===========================================================================
// M_FindWhite
//===========================================================================
char   *M_FindWhite(char *str)
{
	while(*str && !ISSPACE(*str))
		str++;
	return str;
}

//===========================================================================
// M_SkipLine
//===========================================================================
char   *M_SkipLine(char *str)
{
	while(*str && *str != '\n')
		str++;
	// If the newline was found, skip it, too.
	if(*str == '\n')
		str++;
	return str;
}

//===========================================================================
// M_LimitedStrCat
//===========================================================================
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

//===========================================================================
// M_ReadLine
//===========================================================================
void M_ReadLine(char *buffer, int len, DFILE * file)
{
	int     p;
	char    ch;

	memset(buffer, 0, len);
	for(p = 0; p < len - 1;)	// Make the last null stay there.
	{
		ch = F_GetC(file);
		if(ch == '\r')
			continue;
		if(deof(file) || ch == '\n')
			break;
		buffer[p++] = ch;
	}
}

//===========================================================================
// M_IsComment
//===========================================================================
boolean M_IsComment(char *buffer)
{
	int     i = 0;

	while(isspace(buffer[i]) && buffer[i])
		i++;
	if(buffer[i] == '#')
		return true;
	return false;
}

/*
   ===============
   =
   = M_Random
   =
   = Returns a 0-255 number
   =
   ===============
 */

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

/*unsigned char *usedRndtable = NULL;

   int backup_rndindex, backup_prndindex;

   int prndindex = 0;

   void SetRandomTable(unsigned char *table)
   {
   usedRndtable = table;
   }

   unsigned char P_Random (void)
   {
   return usedRndtable[(++prndindex)&0xff];
   }

   int M_Random (void)
   {
   rndindex = (rndindex+1)&0xff;
   return usedRndtable[rndindex];
   }

   void M_ClearRandom (void)
   {
   rndindex = prndindex = 0;
   }

   void M_SaveRandom (void)
   {
   backup_rndindex = rndindex;
   backup_prndindex = prndindex;
   }

   void M_RestoreRandom (void)
   {
   rndindex = backup_rndindex;
   prndindex = backup_prndindex;
   } */

//===========================================================================
// M_CycleIntoRange
//  Returns the value mod length (length > 0).
//===========================================================================
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

//===========================================================================
// M_Normalize
//  Normalize a vector. Returns the former length.
//===========================================================================
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

//===========================================================================
// M_DotProduct
//===========================================================================
float M_DotProduct(float *a, float *b)
{
	return a[VX] * b[VX] + a[VY] * b[VY] + a[VZ] * b[VZ];
}

//===========================================================================
// M_CrossProduct
//  Cross product of two vectors.
//===========================================================================
void M_CrossProduct(float *a, float *b, float *out)
{
	out[VX] = a[VY] * b[VZ] - a[VZ] * b[VY];
	out[VY] = a[VZ] * b[VX] - a[VX] * b[VZ];
	out[VZ] = a[VX] * b[VY] - a[VY] * b[VX];
}

//===========================================================================
// M_PointCrossProduct
//  Cross product of two vectors composed of three points.
//===========================================================================
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

//===========================================================================
// M_RotateVector
//  First yaw, then pitch. Two consecutive 2D rotations.
//  Probably could be done a bit more efficiently.
//===========================================================================
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

//===========================================================================
// M_PointUnitLineDistance
//  Line a -> b, point c. The line must be exactly one unit long!
//===========================================================================
float M_PointUnitLineDistance(float *a, float *b, float *c)
{
	return
		fabs(((a[VY] - c[VY]) * (b[VX] - a[VX]) -
			  (a[VX] - c[VX]) * (b[VY] - a[VY])));
}

//===========================================================================
// M_PointLineDistance
//  Line a -> b, point c.
//===========================================================================
float M_PointLineDistance(float *a, float *b, float *c)
{
	float   d[2], len;

	d[VX] = b[VX] - a[VX];
	d[VY] = b[VY] - a[VY];
	len = sqrt(d[VX] * d[VX] + d[VY] * d[VY]);	// Accurate.
	if(!len)
		return 0;
	return
		fabs(((a[VY] - c[VY]) * (b[VX] - a[VX]) -
			  (a[VX] - c[VX]) * (b[VY] - a[VY])) / len);
}

//===========================================================================
// M_ProjectPointOnLinef
//  Input is fixed, output is floating point. Gap is the distance left 
//  between the line and the projected point.
//===========================================================================
void M_ProjectPointOnLinef(fixed_t *point, fixed_t *linepoint, fixed_t *delta,
						   float gap, float *result)
{
#define DOTPROD(a,b)	(a[VX]*b[VX] + a[VY]*b[VY])
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

//===========================================================================
// M_BoundingBoxDiff
//===========================================================================
float M_BoundingBoxDiff(float in[4], float out[4])
{
	return in[BLEFT] - out[BLEFT] + in[BTOP] - out[BTOP] + out[BRIGHT] -
		in[BRIGHT] + out[BBOTTOM] - in[BBOTTOM];
}

//===========================================================================
// M_ClearBox
//===========================================================================
void M_ClearBox(fixed_t *box)
{
	box[BOXTOP] = box[BOXRIGHT] = DDMININT;
	box[BOXBOTTOM] = box[BOXLEFT] = DDMAXINT;
}

//===========================================================================
// M_AddToBox
//===========================================================================
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

/*
   ==================
   =
   = M_WriteFile
   =
   ==================
 */

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

//==========================================================================
//
// M_ReadFile
//
// Read a file into a buffer allocated using Z_Malloc().
//
//==========================================================================

int M_ReadFile(char const *name, byte **buffer)
{
	return FileReader(name, buffer, MALLOC_ZONE);
}

//==========================================================================
//
// M_ReadFileCLib
//
// Read a file into a buffer allocated using malloc().
//
//==========================================================================

int M_ReadFileCLib(char const *name, byte **buffer)
{
	return FileReader(name, buffer, MALLOC_CLIB);
}

//==========================================================================
//
// ReadFile
//
//==========================================================================

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
				buf = realloc(buf, length + count);
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
	{							// Use zone memory allocation
		buf = Z_Malloc(length, PU_STATIC, NULL);
	}
	else
	{							// Use c library memory allocation
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

//---------------------------------------------------------------------------
//
// PROC M_ForceUppercase
//
// Change string to uppercase.
//
//---------------------------------------------------------------------------

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

//===========================================================================
// M_WriteTextEsc
//  The caller must provide the opening and closing quotes.
//===========================================================================
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
   ===================
   =
   = M_AproxDistance
   =
   = Gives an estimation of distance (not exact)
   =
   ===================
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

//===========================================================================
// M_ApproxDistancef
//===========================================================================
float M_ApproxDistancef(float dx, float dy)
{
	dx = fabs(dx);
	dy = fabs(dy);
	if(dx < dy)
		return dx + dy - dx / 2;
	return dx + dy - dy / 2;
}

//===========================================================================
// M_ApproxDistance3f
//===========================================================================
float M_ApproxDistance3f(float dx, float dy, float dz)
{
	return M_ApproxDistancef(M_ApproxDistancef(dx, dy), dz);
}

//===========================================================================
// M_ScreenShot
//  Writes a Targa file of the specified depth.
//===========================================================================
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

//===========================================================================
// M_PrependBasePath
//===========================================================================
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

//===========================================================================
// M_RemoveBasePath
//  If the base path is found in the beginning of the path, it is removed.
//===========================================================================
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

//===========================================================================
// M_TranslatePath
//  Expands >.
//===========================================================================
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
	else
	{
		strcpy(translated, path);
	}
	Dir_FixSlashes(translated);
}

//===========================================================================
// M_FileExists
//  Also checks for '>'.
//  The file must be a *real* file!
//===========================================================================
int M_FileExists(const char *file)
{
	char    buf[256];

	M_TranslatePath(file, buf);
	return !access(buf, 4);		// Read permission?
}

//===========================================================================
// M_CheckPath
//  Check that the given directory exists. If it doesn't, create it.
//  The path must be relative!
//  Return true if the directory already exists.
//===========================================================================
boolean M_CheckPath(char *path)
{
	char    full[256];
	char    buf[256], *ptr, *endptr;

	// Convert all backslashes to normal slashes.
	strcpy(full, path);
	Dir_FixSlashes(full);

	if(!access(full, 0))
		return true;			// Quick test.

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

//===========================================================================
// M_GetFileExt
//  The dot is not included in the returned extension.
//  The extension can be at most 10 characters long.
//===========================================================================
void M_GetFileExt(const char *path, char *ext)
{
	char   *ptr = strrchr(path, '.');

	*ext = 0;
	if(!ptr)
		return;
	strncpy(ext, ptr + 1, 10);
	strlwr(ext);
}

//===========================================================================
// M_ReplaceFileExt
//  The new extension must not include a dot.
//===========================================================================
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
