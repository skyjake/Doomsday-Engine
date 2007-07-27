//------------------------------------------------------------------------
// SYSTEM : Bridging code
//------------------------------------------------------------------------
//
//  GL-Friendly Node Builder (C) 2000-2007 Andrew Apted
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

#ifndef __GLBSP_SYSTEM_H__
#define __GLBSP_SYSTEM_H__

#include "glbsp.h"


// use this for inlining.  Usually defined in the makefile.
#ifndef INLINE_G
#define INLINE_G  /* nothing */
#endif


// internal storage of node building parameters

extern const nodebuildinfo_t *cur_info;
extern const nodebuildfuncs_t *cur_funcs;
extern volatile nodebuildcomms_t *cur_comms;


/* ----- function prototypes ---------------------------- */

// fatal error messages (these don't return)
void FatalError(const char *str, ...) GCCATTR((format (printf, 1, 2)));
void InternalError(const char *str, ...) GCCATTR((format (printf, 1, 2)));

// display normal messages & warnings to the screen
void PrintMsg(const char *str, ...) GCCATTR((format (printf, 1, 2)));
void PrintVerbose(const char *str, ...) GCCATTR((format (printf, 1, 2)));
void PrintWarn(const char *str, ...) GCCATTR((format (printf, 1, 2)));
void PrintMiniWarn(const char *str, ...) GCCATTR((format (printf, 1, 2)));

// set message for certain errors
void SetErrorMsg(const char *str, ...) GCCATTR((format (printf, 1, 2)));

// endian handling
void InitEndian(void);
uint16_g Endian_U16(uint16_g);
uint32_g Endian_U32(uint32_g);

// these are only used for debugging
void InitDebug(void);
void TermDebug(void);
void PrintDebug(const char *str, ...) GCCATTR((format (printf, 1, 2)));

// macros for the display stuff
#define DisplayOpen        (* cur_funcs->display_open)
#define DisplaySetTitle    (* cur_funcs->display_setTitle)
#define DisplaySetBar      (* cur_funcs->display_setBar)
#define DisplaySetBarLimit (* cur_funcs->display_setBarLimit)
#define DisplaySetBarText  (* cur_funcs->display_setBarText)
#define DisplayClose       (* cur_funcs->display_close)

#define DisplayTicker      (* cur_funcs->ticker)


#endif /* __GLBSP_SYSTEM_H__ */
