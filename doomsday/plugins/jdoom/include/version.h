/**\file
 * Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, 
 * Boston, MA  02110-1301  USA
 */

#ifndef __JDOOM_VERSION_H__
#define __JDOOM_VERSION_H__

#ifndef __JDOOM__
#  error "Using jDoom headers without __JDOOM__"
#endif

// DOOM version

#ifndef JDOOM_VER_ID
#  ifdef _DEBUG
#    define JDOOM_VER_ID "+D Doomsday"
#  else
#    define JDOOM_VER_ID "Doomsday"
#  endif
#endif

#define GAMENAMETEXT "jdoom"

// My my, the names of these #defines are really well chosen...
#define VERSION_TEXT "1.15."DOOMSDAY_RELEASE_NAME
#define VERSIONTEXT "Version "VERSION_TEXT" "__DATE__" ("JDOOM_VER_ID")"

// All the versions of Doom have different savegame IDs, but
// 500 will be the savegame base from now on.
#define SAVE_VERSION_BASE   500
#define SAVE_VERSION        (SAVE_VERSION_BASE + gamemode)

#endif
