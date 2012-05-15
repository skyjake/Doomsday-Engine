/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2007-2012 Daniel Swanson <danij@dengine.net>
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
 * Version Information
 */

#ifndef LIBDENG_VERSION_H
#define LIBDENG_VERSION_H

#define DOOMSDAY_NICENAME       "Doomsday Engine"

#define DOOMSDAY_HOMEURL        "http://dengine.net"
#define DOOMSDAY_MASTERURL      "http://dengine.net/master.php"
#define DOOMSDAY_DOCSURL        "http://dengine.net/dew"

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

#define DOOMSDAY_VERSION_BASE   "1.9.9"
#define DOOMSDAY_VERSION_NUMBER 1,9,9,0 // For WIN32 version info.
//#define DOOMSDAY_RELEASE_NAME   "Example"

/**
 * DOOMSDAY_RELEASE_TYPE determines the classification of the release.
 * Possible values are "Unstable", "Candidate" and "Stable".
 */
#define DOOMSDAY_RELEASE_TYPE   "Unstable"
//#define DOOMSDAY_RELEASE_TYPE   "Candidate"
//#define DOOMSDAY_RELEASE_TYPE   "Stable"

/**
 * Version constants.  The Game module can use DOOMSDAY_VERSION to
 * verify that the engine is new enough.  Don't change
 * DOOMSDAY_VERSION unless you wish to break compatibility.
 */
#define DOOMSDAY_VERSION        10907 // Don't touch; see above.

#if !defined(DENG_STABLE) && defined(DOOMSDAY_BUILD_TEXT)
/** The build number is included only for non-Stable builds. */
#  define DOOMSDAY_VERSION_TEXT             DOOMSDAY_VERSION_BASE " [#" DOOMSDAY_BUILD_TEXT "]"
#  if defined(WIN32) && defined(UNICODE)
#    define DOOMSDAY_VERSION_TEXT_WSTR      TEXT(DOOMSDAY_VERSION_BASE) TEXT(" [#") TEXT(DOOMSDAY_BUILD_TEXT) TEXT("]")
#  endif

#elif defined(DOOMSDAY_RELEASE_NAME)
#  define DOOMSDAY_VERSION_TEXT             (DOOMSDAY_VERSION_BASE "-" DOOMSDAY_RELEASE_NAME)
#  if defined(WIN32) && defined(UNICODE)
#    define DOOMSDAY_VERSION_TEXT_WSTR      TEXT(DOOMSDAY_VERSION_BASE) TEXT("-") TEXT(DOOMSDAY_RELEASE_NAME)
#  endif

#else
#  define DOOMSDAY_VERSION_TEXT             DOOMSDAY_VERSION_BASE
#  if defined(WIN32) && defined(UNICODE)
#    define DOOMSDAY_VERSION_TEXT_WSTR      TEXT(DOOMSDAY_VERSION_BASE)
#  endif
#endif

#endif /* LIBDENG_VERSION_H */
