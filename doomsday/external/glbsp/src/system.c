//------------------------------------------------------------------------
// SYSTEM : System specific code
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


#define DEBUG_ENABLED   0

#define DEBUGGING_FILE  "gb_debug.txt"

#define DEBUG_ENDIAN  0

static int cpu_big_endian = 0;


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
// PrintVerbose
//
void PrintVerbose(const char *str, ...)
{
  va_list args;

  if (! cur_info->quiet)
  {
    va_start(args, str);
    vsprintf(message_buf, str, args);
    va_end(args);

    (* cur_funcs->print_msg)("%s", message_buf);
  }

#if DEBUG_ENABLED
  {
    va_start(args, str);
    vsprintf(message_buf, str, args);
    va_end(args);

    PrintDebug(">>> %s", message_buf);
  }
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

  cur_comms->total_big_warn++;

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

  cur_comms->total_small_warn++;

#if DEBUG_ENABLED
  va_start(args, str);
  vsprintf(message_buf, str, args);
  va_end(args);

  PrintDebug("MiniWarn: %s", message_buf);
#endif
}

//
// SetErrorMsg
//
void SetErrorMsg(const char *str)
{
  GlbspFree(cur_comms->message);

  cur_comms->message = GlbspStrDup(str);
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


/* -------- endian code ----------------------------- */

//
// InitEndian
//
// Parts inspired by the Yadex endian.cc code.
//
void InitEndian(void)
{
  volatile union
  {
    uint8_g mem[32];
    uint32_g val;
  }
  u;
 
  /* sanity-check type sizes */

  if (sizeof(uint8_g) != 1)
    FatalError("Sanity check failed: sizeof(uint8_g) = %d", 
        sizeof(uint8_g));

  if (sizeof(uint16_g) != 2)
    FatalError("Sanity check failed: sizeof(uint16_g) = %d", 
        sizeof(uint16_g));

  if (sizeof(uint32_g) != 4)
    FatalError("Sanity check failed: sizeof(uint32_g) = %d", 
        sizeof(uint32_g));

  /* check endianness */

  memset((uint32_g *) u.mem, 0, sizeof(u.mem));

  u.mem[0] = 0x70;  u.mem[1] = 0x71;
  u.mem[2] = 0x72;  u.mem[3] = 0x73;

# if DEBUG_ENDIAN
  PrintDebug("Endianness magic value: 0x%08x\n", u.val);
# endif

  if (u.val == 0x70717273)
    cpu_big_endian = 1;
  else if (u.val == 0x73727170)
    cpu_big_endian = 0;
  else
    FatalError("Sanity check failed: weird endianness (0x%08x)", u.val);

# if DEBUG_ENDIAN
  PrintDebug("Endianness = %s\n", cpu_big_endian ? "BIG" : "LITTLE");

  PrintDebug("Endianness check: 0x1234 --> 0x%04x\n", 
      (int) Endian_U16(0x1234));
  
  PrintDebug("Endianness check: 0x11223344 --> 0x%08x\n", 
      Endian_U32(0x11223344));
# endif
}

//
// Endian_U16
//
uint16_g Endian_U16(uint16_g x)
{
  if (cpu_big_endian)
    return (x >> 8) | (x << 8);
  else
    return x;
}

//
// Endian_U32
//
uint32_g Endian_U32(uint32_g x)
{
  if (cpu_big_endian)
    return (x >> 24) | ((x >> 8) & 0xff00) |
           ((x << 8) & 0xff0000) | (x << 24);
  else
    return x;
}

