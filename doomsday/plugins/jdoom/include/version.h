/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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
 * Version numbering, naming etc.
 */

#ifndef JDOOM_VERSION_H
#define JDOOM_VERSION_H

#ifndef __JDOOM__
#  error "Using jDoom headers without __JDOOM__"
#endif

#include "dengproject.h"
#include "dd_plugin.h"

#ifndef JDOOM_VER_ID
#  ifdef _DEBUG
#    define JDOOM_VER_ID "+D Doomsday"
#  else
#    define JDOOM_VER_ID "Doomsday"
#  endif
#endif

// Used to derive filepaths.
#define PLUGIN_NAMETEXT     "jdoom"

// Presented to the user in dialogs, messages etc.
#define PLUGIN_NICENAME     "jDoom"
#define PLUGIN_NICEAUTHOR   "deng team"
#define PLUGIN_DETAILS      "jDoom is based on linuxdoom-1.10."

#define PLUGIN_HOMEURL      DOOMSDAY_HOMEURL
#define PLUGIN_DOCSURL      DOOMSDAY_DOCSURL

#define PLUGIN_VERSION_TEXT "1.16.2"
#define PLUGIN_VERSION_TEXTLONG "Version " PLUGIN_VERSION_TEXT " " __DATE__ " (" JDOOM_VER_ID ")"
#define PLUGIN_VERSION_NUMBER 1,16,2,0 // For WIN32 version info.

// For WIN32 version info:
#define PLUGIN_DESC         PLUGIN_NICENAME " " LIBDENG_PLUGINDESC
#define PLUGIN_COPYRIGHT    "2003-2012, " DENGPROJECT_NICEAUTHOR

#endif /* JDOOM_VERSION_H */
