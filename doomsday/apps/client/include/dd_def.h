/**
 * @file dd_def.h
 * Internal macros and constants for Doomsday. @ingroup base
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef __DOOMSDAY_DEFS_H__
#define __DOOMSDAY_DEFS_H__

/**
 * @defgroup flags Flags (Internal)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dd_types.h"
#include <doomsday/doomsdayapp.h>
#include <doomsday/gameapi.h>
#include "api_internaldata.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef WIN32_MSVC
// Disable annoying MSVC warnings.
// 4761: integral size mismatch in argument
// 4244: conversion from 'type1' to 'type2', possible loss of data
#pragma warning (disable:4761 4244)
#endif

// if rangecheck is undefined, most parameter validation debugging code
// will not be compiled
#ifndef DE_NO_RANGECHECKING
#   define RANGECHECK
#endif

#ifdef RANGECHECK
#   define DOOMSDAY_VER_ID_RANGECHECK " +R"
#else
#   define DOOMSDAY_VER_ID_RANGECHECK ""
#endif

#ifdef _DEBUG
#   define DOOMSDAY_VER_ID_DEBUG " +D"
#else
#   define DOOMSDAY_VER_ID_DEBUG ""
#endif

#ifdef __64BIT__
#   define DOOMSDAY_VER_ID_64BIT " 64-bit"
#else
#   define DOOMSDAY_VER_ID_64BIT " 32-bit"
#endif

#if defined(DE_STABLE) && defined(DOOMSDAY_BUILD_TEXT)
#  define DOOMSDAY_VER_ID_BUILD " #" DOOMSDAY_BUILD_TEXT
#else
#  define DOOMSDAY_VER_ID_BUILD ""
#endif

#define DOOMSDAY_VER_ID     DOOMSDAY_RELEASE_TYPE DOOMSDAY_VER_ID_64BIT DOOMSDAY_VER_ID_DEBUG DOOMSDAY_VER_ID_RANGECHECK DOOMSDAY_VER_ID_BUILD

#define DOOMSDAY_VERSION_FULLTEXT     DOOMSDAY_VERSION_TEXT " (" DOOMSDAY_VER_ID ") " __DATE__ " " __TIME__

#define SAFEDIV(x,y)    (!(y) || !((x)/(y))? 1 : (x)/(y))
#define ORDER(x,y,a,b)  ( (x)<(y)? ((a)=(x),(b)=(y)) : ((b)=(x),(a)=(y)) )

#ifdef _DEBUG
#  define ASSERT_64BIT(p) {if(sizeof(p) != 8) App_Error(#p " is not 64-bit in " __FILE__ " at line %i.\n", __LINE__);}
#  define ASSERT_NOT_64BIT(p) {if(sizeof(p) == 8) App_Error(#p " is 64-bit in " __FILE__ " at line %i.\n", __LINE__);}
#  define ASSERT_32BIT(p) {if(sizeof(p) != 4) App_Error(#p " is not 32-bit in " __FILE__ " at line %i.\n", __LINE__);}
#  define ASSERT_16BIT(p) {if(sizeof(p) != 2) App_Error(#p " is not 16-bit in " __FILE__ " at line %i.\n", __LINE__);}
#else
#  define ASSERT_64BIT(p)
#  define ASSERT_NOT_64BIT(p)
#  define ASSERT_32BIT(p)
#  define ASSERT_16BIT(p)
#endif

#define MAXEVENTS       256
#define SBARHEIGHT      39         // status bar height at bottom of screen

#define SECONDS_TO_TICKS(sec) ((int)((sec)*35))

// Heap relations.
#define HEAP_PARENT(i)  (((i) + 1)/2 - 1)
#define HEAP_LEFT(i)    (2*(i) + 1)
#define HEAP_RIGHT(i)   (2*(i) + 2)

//enum { VX, VY, VZ };               // Vertex indices.
enum { CR, CG, CB, CA };           // Color indices.

extern float    texGamma;

#ifdef __cplusplus
} // extern "C"
#endif

#define gx (DoomsdayApp::plugins().gameExports())

#endif
