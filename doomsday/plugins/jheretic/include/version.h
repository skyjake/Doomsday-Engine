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

#ifndef JHERETIC_VERSION_H
#define JHERETIC_VERSION_H

#ifndef __JHERETIC__
#  error "Using jHeretic headers without __JHERETIC__"
#endif

#include "dengproject.h"
#include "dd_plugin.h"

#ifndef JHERETIC_VER_ID
#  ifdef _DEBUG
#    define JHERETIC_VER_ID "+D Doomsday"
#  else
#    define JHERETIC_VER_ID "Doomsday"
#  endif
#endif

// Used to derive filepaths.
#define PLUGIN_NAMETEXT     "jheretic"

// Presented to the user in dialogs, messages etc.
#define PLUGIN_NICENAME     "jHeretic"
#define PLUGIN_NICEAUTHOR   "deng team"
#define PLUGIN_DETAILS      "jHeretic is based on Heretic v1.3 by Raven Software."

#define PLUGIN_HOMEURL      DOOMSDAY_HOMEURL
#define PLUGIN_DOCSURL      DOOMSDAY_DOCSURL

#define PLUGIN_VERSION_TEXT "1.5.1"
#define PLUGIN_VERSION_TEXTLONG "Version " PLUGIN_VERSION_TEXT " " __DATE__ " (" JHERETIC_VER_ID ")"
#define PLUGIN_VERSION_NUMBER 1,5,1,0 // For WIN32 version info.

// For WIN32 version info:
#define PLUGIN_DESC         PLUGIN_NICENAME " " LIBDENG_PLUGINDESC
#define PLUGIN_COPYRIGHT    "2003-2012, " DENGPROJECT_NICEAUTHOR

#endif /* JHERETIC_VERSION_H */
