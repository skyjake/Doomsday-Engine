//------------------------------------------------------------------------
// UTILITY : general purpose functions
//------------------------------------------------------------------------
//
//  GL-Friendly Node Builder (C) 2000-2002 Andrew Apted
//
//  Based on `BSP 2.3' by Colin Reed, Lee Killough and others.
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
  int len = strlen(str);

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

int StrCaseCmp(const char *A, const char *B)
{
  for (; *A || *B; A++, B++)
  {
    if (toupper(*A) != toupper(*B))
      return (toupper(*A) - toupper(*B));
  }

  // strings are equal
  return 0;
}


//
// RoundPOW2
//
// Rounds the value _up_ to the nearest power of two.
//
int RoundPOW2(int x)
{
  int tmp;

  if (x <= 2)
    return x;

  x--;
  
  for (tmp=x / 2; tmp; tmp /= 2)
    x |= tmp;
  
  return (x + 1);
}

