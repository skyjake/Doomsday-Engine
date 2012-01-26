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
 * dd_def.h: Internal Doomsday Macros and Constants
 */

#ifndef __DOOMSDAY_DEFS_H__
#define __DOOMSDAY_DEFS_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dd_types.h"
#include "dd_api.h"

#ifdef WIN32
#   define stricmp		_stricmp
#   define strnicmp		_strnicmp
#   define strlwr       _strlwr
#	define strupr       _strupr
// Disable annoying MSVC warnings.
// 4761: integral size mismatch in argument 
// 4244: conversion from 'type1' to 'type2', possible loss of data
#pragma warning (disable:4761 4244)
#endif

// Important definitions.
#define MAXPLAYERS			DDMAXPLAYERS
#define players				ddplayers

// if rangecheck is undefined, most parameter validation debugging code
// will not be compiled
#ifndef NORANGECHECKING
#	define RANGECHECK
#endif

#ifndef DOOMSDAY_VER_ID
#	ifdef _DEBUG
#		define DOOMSDAY_VER_ID "+D DGL"
#	else
#		define DOOMSDAY_VER_ID "DGL"
#	endif
#endif

#ifdef RANGECHECK
#	define DOOMSDAY_VERSIONTEXT "Version "DOOMSDAY_VERSION_TEXT" +R "__DATE__" ("DOOMSDAY_VER_ID")"
#else
#	define DOOMSDAY_VERSIONTEXT "Version "DOOMSDAY_VERSION_TEXT" "__DATE__" ("DOOMSDAY_VER_ID")"
#endif

#define SAFEDIV(x,y)	(!(y) || !((x)/(y))? 1 : (x)/(y))
#define ORDER(x,y,a,b)	( (x)<(y)? ((a)=(x),(b)=(y)) : ((b)=(x),(a)=(y)) )
#define LAST_CHAR(str)	(str[strlen(str) - 1])

#define MAXEVENTS		64
#define	SBARHEIGHT		39		   // status bar height at bottom of screen
#define PI				3.141592657

#define SECONDS_TO_TICKS(sec) ((int)((sec)*35))

// Heap relations.
#define HEAP_PARENT(i)	(((i) + 1)/2 - 1)
#define HEAP_LEFT(i)	(2*(i) + 1)
#define HEAP_RIGHT(i)	(2*(i) + 2)

enum { BLEFT, BTOP, BRIGHT, BBOTTOM, BFLOOR, BCEILING };
enum { VX, VY, VZ };			   // Vertex indices.
enum { CR, CG, CB, CA };		   // Color indices.

// dd_pinit.c
extern game_export_t __gx;
extern game_import_t __gi;

#define gx __gx
#define gi __gi

// tab_video.c
extern byte     gammatable[5][256];
extern int      usegamma;

// tab_tables.c
extern fixed_t  finesine[5 * FINEANGLES / 4];
extern fixed_t *finecosine;

#endif
