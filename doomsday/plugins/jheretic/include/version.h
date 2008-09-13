/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2008 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2008 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006 Jamie Jones <yagisan@dengine.net>
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

/**
 * version.h:
 */

#ifndef __JHERETIC_VERSION_H__
#define __JHERETIC_VERSION_H__

#ifndef __JHERETIC__
#  error "Using jHeretic headers without __JHERETIC__"
#endif

#define SAVE_VERSION        130 // Don't change this.

#ifndef JHERETIC_VER_ID
#  ifdef _DEBUG
#    define JHERETIC_VER_ID "+D Doomsday"
#  else
#    define JHERETIC_VER_ID "Doomsday"
#  endif
#endif

#define GAMENAMETEXT        "jheretic"

#define VERSION             210
#define VERSION_TEXT        "1.4."DOOMSDAY_RELEASE_NAME
#define VERSIONTEXT         "Version "VERSION_TEXT" "__DATE__" ("JHERETIC_VER_ID")"

#endif
