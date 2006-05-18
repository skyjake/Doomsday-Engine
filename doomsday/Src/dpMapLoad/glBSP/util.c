//------------------------------------------------------------------------
// UTILITY : general purpose functions
//------------------------------------------------------------------------
//
//  GL-Friendly Node Builder (C) 2000-2005 Andrew Apted
//
//  Based on 'BSP 2.3' by Colin Reed, Lee Killough and others.
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//------------------------------------------------------------------------

#include "system.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <math.h>
#include <limits.h>
#include <assert.h>

#include "util.h"

#ifdef WIN32
#include <Windows.h>
#else
#include <time.h>
#endif


//
// UtilCalloc
//
// Allocate memory with error checking.  Zeros the memory.
//
void *UtilCalloc(int size)
{
  void *ret = calloc(1, size);
  
  if (!ret)
    FatalError("Out of memory (cannot allocate %d bytes)", size);

  return ret;
}

//
// UtilRealloc
//
// Reallocate memory with error checking.
//
void *UtilRealloc(void *old, int size)
{
  void *ret = realloc(old, size);

  if (!ret)
    FatalError("Out of memory (cannot reallocate %d bytes)", size);

  return ret;
}

//
// UtilFree
//
// Free the memory with error checking.
//
void UtilFree(void *data)
{
  if (data == NULL)
    InternalError("Trying to free a NULL pointer");
  
  free(data);
}

//
// UtilStrDup
//
// Duplicate a string with error checking.
//
char *UtilStrDup(const char *str)
{
  char *result;
  int len = (int)strlen(str);

  result = UtilCalloc(len+1);

  if (len > 0)
    memcpy(result, str, len);
  
  result[len] = 0;

  return result;
}

//
// UtilStrNDup
//
// Duplicate a limited length string.
//
char *UtilStrNDup(const char *str, int size)
{
  char *result;
  int len;

  for (len=0; len < size && str[len]; len++)
  { }

  result = UtilCalloc(len+1);

  if (len > 0)
    memcpy(result, str, len);
  
  result[len] = 0;

  return result;
}

int UtilStrCaseCmp(const char *A, const char *B)
{
  for (; *A || *B; A++, B++)
  {
    // this test also catches end-of-string conditions
    if (toupper(*A) != toupper(*B))
      return (toupper(*A) - toupper(*B));
  }

  // strings are equal
  return 0;
}


//
// UtilRoundPOW2
//
// Rounds the value _up_ to the nearest power of two.
//
int UtilRoundPOW2(int x)
{
  int tmp;

  if (x <= 2)
    return x;

  x--;
  
  for (tmp=x / 2; tmp; tmp /= 2)
    x |= tmp;
  
  return (x + 1);
}


//
// UtilComputeAngle
//
// Translate (dx, dy) into an angle value (degrees)
//
angle_g UtilComputeAngle(float_g dx, float_g dy)
{
  double angle;

  if (dx == 0)
    return (dy > 0) ? 90.0 : 270.0;

  angle = atan2((double) dy, (double) dx) * 180.0 / M_PI;

  if (angle < 0) 
    angle += 360.0;

  return angle;
}


//
// UtilFileExists
//
int UtilFileExists(const char *filename)
{
  FILE *fp = fopen(filename, "rb");

  if (fp)
  {
    fclose(fp);
    return TRUE;
  }

  return FALSE;
}

//
// UtilTimeString
//
const char *UtilTimeString(void)
{
#ifdef WIN32

  SYSTEMTIME sys_time;

  static char str_buf[200];

  GetSystemTime(&sys_time);

  sprintf(str_buf, "%04d-%02d-%02d %02d:%02d:%02d.%04d",
      sys_time.wYear, sys_time.wMonth, sys_time.wDay,
      sys_time.wHour, sys_time.wMinute, sys_time.wSecond,
      sys_time.wMilliseconds * 10);

  return str_buf;

#else // LINUX or MACOSX

  time_t epoch_time;
  struct tm *calend_time;

  static char str_buf[200];
 
  if (time(&epoch_time) == (time_t)-1)
    return NULL;

  calend_time = localtime(&epoch_time);
  if (! calend_time)
    return NULL;

  sprintf(str_buf, "%04d-%02d-%02d %02d:%02d:%02d.%04d",
      calend_time->tm_year + 1900, calend_time->tm_mon + 1,
      calend_time->tm_mday,
      calend_time->tm_hour, calend_time->tm_min,
      calend_time->tm_sec,  0);

  return str_buf;

#endif  
}

//------------------------------------------------------------------------
//  Adler-32 CHECKSUM Code
//------------------------------------------------------------------------

void Adler32_Begin(uint32_g *crc)
{
  *crc = 1;
}

void Adler32_AddBlock(uint32_g *crc, const uint8_g *data, int length)
{
    uint32_g s1 = (*crc) & 0xFFFF;
    uint32_g s2 = ((*crc) >> 16) & 0xFFFF;

    for (; length > 0; data++, length--)
    {
        s1 = (s1 + *data) % 65521;
        s2 = (s2 + s1)    % 65521;
    }

    *crc = (s2 << 16) | s1;
}

void Adler32_Finish(uint32_g *crc)
{
  /* nothing to do */
}

