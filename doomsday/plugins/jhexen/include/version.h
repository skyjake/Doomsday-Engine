/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2004-2008 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2007-2008 Daniel Swanson <danij@dengine.net>
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

#ifndef __JHEXEN_VERSION_H__
#define __JHEXEN_VERSION_H__

#ifndef __JHEXEN__
#  error "Using jHexen headers without __JHEXEN__"
#endif

#ifndef VER_ID
#  define VER_ID "Doomsday"
#endif

#define GAMENAMETEXT "jhexen"

// Version numbering changes: 200 means JHexen v1.0.
#define VERSION 200
#define VERSION_TEXT "1.3."DOOMSDAY_RELEASE_NAME

#ifdef RANGECHECK
#  define VERSIONTEXT "Version "VERSION_TEXT" +R "__DATE__" ("VER_ID")"
#else
#  define VERSIONTEXT "Version "VERSION_TEXT" "__DATE__" ("VER_ID")"
#endif

#endif
