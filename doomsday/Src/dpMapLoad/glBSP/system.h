//------------------------------------------------------------------------
// SYSTEM : Bridging code
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

#ifndef __GLBSP_SYSTEM_H__
#define __GLBSP_SYSTEM_H__

#include "glbsp.h"


// consistency check
#if (!defined(GLBSP_TEXT) && !defined(GLBSP_GUI) && !defined(GLBSP_PLUGIN))
#error Must define one of GLBSP_TEXT, GLBSP_GUI or GLBSP_PLUGIN 
#endif


// use this for inlining.  Usually defined in the makefile.
#ifndef INLINE_G
#define INLINE_G  /* nothing */
#endif


// internal storage of node building parameters

extern const nodebuildinfo_t *cur_info;
extern const nodebuildfuncs_t *cur_funcs;
extern volatile nodebuildcomms_t *cur_comms;

extern int cur_build_pos;
extern int cur_file_pos;

extern int total_big_warn;
extern int total_small_warn;


/* ----- function prototypes ---------------------------- */

// fatal error messages (these don't return)
void FatalError(const char *str, ...);
void InternalError(const char *str, ...);

// display normal messages & warnings to the screen
void PrintMsg(const char *str, ...);
void PrintWarn(const char *str, ...);
void PrintMiniWarn(const char *str, ...);

// these are only used for debugging
void InitDebug(void);
void TermDebug(void);
void PrintDebug(const char *str, ...);

// macros for the display stuff
#define DisplayOpen        (* cur_funcs->display_open)
#define DisplaySetTitle    (* cur_funcs->display_setTitle)
#define DisplaySetBar      (* cur_funcs->display_setBar)
#define DisplaySetBarLimit (* cur_funcs->display_setBarLimit)
#define DisplaySetBarText  (* cur_funcs->display_setBarText)
#define DisplayClose       (* cur_funcs->display_close)

#define DisplayTicker      (* cur_funcs->ticker)


#endif /* __GLBSP_SYSTEM_H__ */
