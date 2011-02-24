/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2007-2009 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
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
 * dd_version.h: Version Information
 */

#ifndef __DOOMSDAY_VERSION_H__
#define __DOOMSDAY_VERSION_H__

/**
 * Version number rules: (major).(minor).(revision)-(release name)
 *
 * Major version will be 1 for now (few things short of a complete
 * rewrite will increase the major version).
 *
 * Minor version increases with important feature releases.
 * NOTE: No extra zeros. Numbering goes from 1 to 9 and continues from
 * 10 like 'normal' numbers.
 *
 * Revision number increases with each small (maintenance) release.
 */

#define DOOMSDAY_VERSION_BASE   "1.9.7"
#define DOOMSDAY_VERSION_NUMBER 1,9,7,0 // For WIN32 version info.
//#define DOOMSDAY_RELEASE_NAME   "Example"

/**
 * Version constants.  The Game module can use DOOMSDAY_VERSION to
 * verify that the engine is new enough.  Don't change
 * DOOMSDAY_VERSION unless you wish to break compatibility.
 */
#define DOOMSDAY_VERSION        10900 // Don't touch; see above.
#if defined(DOOMSDAY_RELEASE_NAME)
#  define DOOMSDAY_VERSION_TEXT DOOMSDAY_VERSION_BASE "-" DOOMSDAY_RELEASE_NAME
#else
#  define DOOMSDAY_VERSION_TEXT DOOMSDAY_VERSION_BASE
#endif

#define DOOMSDAY_PROJECTURL     "http://sourceforge.net/projects/deng/"

/**
 * If DOOMSDAY_RELEASE_FULL is NOT defined, the Win32 version info
 * will be marked accordingly, indicating that this is a special build,
 * for example, an alpha/beta/release candidate/etc and the SpecialBuild
 * string will be populated with the content of DOOMSDAY_RELEASE_NAME.
 */
#define DOOMSDAY_RELEASE_FULL   1

#endif
