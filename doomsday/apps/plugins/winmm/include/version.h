/**@file version.h  Version numbering, naming etc.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

#ifndef WINMM_VERSION_H
#define WINMM_VERSION_H

#ifndef DSWINMM_VER_ID
#  ifdef _DEBUG
#    define DSWINMM_VER_ID "+D Doomsday"
#  else
#    define DSWINMM_VER_ID "Doomsday"
#  endif
#endif

// Used to derive filepaths.
#define PLUGIN_NAME         "dswinmm"

// Presented to the user in dialogs, messages etc.
#define PLUGIN_NICENAME     "Windows Multimedia Audio Driver"
#define PLUGIN_DETAILS      "Doomsday plugin for audio playback via Windows Multimedia."

#define PLUGIN_VERSION_TEXT "1.0.5"
#define PLUGIN_VERSION_TEXTLONG "Version " PLUGIN_VERSION_TEXT " " __DATE__ " (" DSWINMM_VER_ID ")"
#define PLUGIN_VERSION_NUMBER 1,0,5,0 // For WIN32 version info.

#endif  // WINMM_VERSION_H
