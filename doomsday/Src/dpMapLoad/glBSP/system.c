//------------------------------------------------------------------------
// SYSTEM : System specific code
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


#define DEBUG_ENABLED   0

#define DEBUGGING_FILE  "gb_debug.txt"


int total_big_warn;
int total_small_warn;


static char message_buf[1024];

#if DEBUG_ENABLED
static FILE *debug_fp = NULL;
#endif


//
// FatalError
//
void FatalError(const char *str, ...)
{
  va_list args;

  va_start(args, str);
  vsprintf(message_buf, str, args);
  va_end(args);

  (* cur_funcs->fatal_error)("\nError: *** %s ***\n\n", message_buf);
}

//
// InternalError
//
void InternalError(const char *str, ...)
{
  va_list args;

  va_start(args, str);
  vsprintf(message_buf, str, args);
  va_end(args);

  (* cur_funcs->fatal_error)("\nINTERNAL ERROR: *** %s ***\n\n", message_buf);
}

//
// PrintMsg
//
void PrintMsg(const char *str, ...)
{
  va_list args;

  va_start(args, str);
  vsprintf(message_buf, str, args);
  va_end(args);

  (* cur_funcs->print_msg)("%s", message_buf);

#if DEBUG_ENABLED
  PrintDebug(">>> %s", message_buf);
#endif
}

//
// PrintWarn
//
void PrintWarn(const char *str, ...)
{
  va_list args;

  va_start(args, str);
  vsprintf(message_buf, str, args);
  va_end(args);

  (* cur_funcs->print_msg)("Warning: %s", message_buf);

  total_big_warn++;

#if DEBUG_ENABLED
  PrintDebug("Warning: %s", message_buf);
#endif
}

//
// PrintMiniWarn
//
void PrintMiniWarn(const char *str, ...)
{
  va_list args;

  if (cur_info->mini_warnings)
  {
    va_start(args, str);
    vsprintf(message_buf, str, args);
    va_end(args);

    (* cur_funcs->print_msg)("Warning: %s", message_buf);
  }

  total_small_warn++;

#if DEBUG_ENABLED
  va_start(args, str);
  vsprintf(message_buf, str, args);
  va_end(args);

  PrintDebug("MiniWarn: %s", message_buf);
#endif
}


/* -------- debugging code ----------------------------- */

//
// InitDebug
//
void InitDebug(void)
{
#if DEBUG_ENABLED
  debug_fp = fopen(DEBUGGING_FILE, "w");

  if (! debug_fp)
    PrintWarn("Unable to open DEBUG FILE: %s\n", DEBUGGING_FILE);

  PrintDebug("=== START OF DEBUG FILE ===\n");
#endif
}

//
// TermDebug
//
void TermDebug(void)
{
#if DEBUG_ENABLED
  if (debug_fp)
  {
    PrintDebug("=== END OF DEBUG FILE ===\n");

    fclose(debug_fp);
    debug_fp = NULL;
  }
#endif
}

//
// PrintDebug
//
void PrintDebug(const char *str, ...)
{
#if DEBUG_ENABLED
  if (debug_fp)
  {
    va_list args;

    va_start(args, str);
    vfprintf(debug_fp, str, args);
    va_end(args);

    fflush(debug_fp);
  }
#else
  (void) str;
#endif
}

