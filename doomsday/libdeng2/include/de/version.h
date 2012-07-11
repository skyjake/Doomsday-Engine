/**
 * @file version.h
 * Version numbering, naming etc for libdeng 2.0
 *
 * @authors Copyright &copy; 2011-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2011-2012 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef LIBDENG2_VERSION_H
#define LIBDENG2_VERSION_H

// We'll take the values for most of these from deng 1.0 to avoid duplication
// (and hopefully avoid any oversights which might occur as a consequence).
#include "../../engine/portable/include/dd_version.h"

// Presented to the user in dialogs, messages etc.
#define LIBDENG2_NICENAME       "deng2"
#define LIBDENG2_NICEAUTHOR     DENGPROJECT_NICEAUTHOR
#define LIBDENG2_DETAILS        "Doomsday 2.0 core engine library."

#define LIBDENG2_HOMEURL        DOOMSDAY_HOMEURL
#define LIBDENG2_MASTERURL      DOOMSDAY_MASTERURL
#define LIBDENG2_DOCSURL        DOOMSDAY_DOCSURL

#define LIBDENG2_VERSION_BASE   DOOMSDAY_VERSION_BASE
#define LIBDENG2_VERSION_NUMBER DOOMSDAY_VERSION_NUMBER // For WIN32 version info.
#ifdef DOOMSDAY_RELEASE_FULL
#  define LIBDENG2_RELEASE_FULL   DOOMSDAY_RELEASE_FULL
#endif
#ifdef DOOMSDAY_RELEASE_NAME
#  define LIBDENG2_RELEASE_NAME   DOOMSDAY_RELEASE_NAME
#endif

/**
 * LIBDENG2_RELEASE_TYPE determines the classification of the release.
 * Possible values are "Unstable", "Candidate" and "Stable".
 */
#define LIBDENG2_RELEASE_TYPE   DOOMSDAY_RELEASE_TYPE

#define LIBDENG2_VERSION_TEXT   DOOMSDAY_VERSION_TEXT
#if defined(WIN32) && defined(UNICODE)
#  define LIBDENG2_VERSION_TEXT_WSTR DOOMSDAY_VERSION_TEXT_WSTR
#endif

// For WIN32 version info:
#if defined(WIN32)
#  define LIBDENG2_FILENAME       "deng2.dll"
#  define LIBDENG2_COPYRIGHT      DOOMSDAY_COPYRIGHT
#  if defined(UNICODE)
#    define LIBDENG2_DESC_WSTR      TEXT(LIBDENG2_DETAILS)
#  else
#    define LIBDENG2_DESC           LIBDENG2_DETAILS
#  endif
#endif

#endif // LIBDENG2_VERSION_H
