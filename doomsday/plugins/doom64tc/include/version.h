/* DE1: $Id: version.h 3411 2006-07-08 15:36:14Z yagisan $
 * Copyright (C) 2005 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef __DOOM64TC_VERSION_H__
#define __DOOM64TC_VERSION_H__

#ifndef __DOOM64TC__
#  error "Using Doom64TC headers without __DOOM64TC__"
#endif

// DOOM64TC version

#ifndef DOOM64TC_VER_ID
#  ifdef _DEBUG
#    define DOOM64TC_VER_ID "+D Doomsday"
#  else
#    define DOOM64TC_VER_ID "Doomsday"
#  endif
#endif

#define GAMENAMETEXT "doom64tc"

// My my, the names of these #defines are really well chosen...
#define VERSION_TEXT "1.15."DOOMSDAY_RELEASE_NAME
#define VERSIONTEXT "Version "VERSION_TEXT" "__DATE__" ("DOOM64TC_VER_ID")"

// All the versions of Doom have different savegame IDs, but
// 500 will be the savegame base from now on.
#define SAVE_VERSION_BASE   500
#define SAVE_VERSION        (SAVE_VERSION_BASE + gamemode)

#endif
