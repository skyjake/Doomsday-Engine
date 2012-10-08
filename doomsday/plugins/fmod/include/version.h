/**
 * @file version.h
 * Version information for the FMOD Ex audio plugin. @ingroup dsfmod
 *
 * @authors Copyright © 2011-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef __DSFMOD_VERSION_H__
#define __DSFMOD_VERSION_H__

#ifndef DSFMOD_VER_ID
#  ifdef _DEBUG
#    define DSFMOD_VER_ID "+D Doomsday"
#  else
#    define DSFMOD_VER_ID "Doomsday"
#  endif
#endif

// Used to derive filepaths.
#define PLUGIN_NAME         "dsfmod"

// Presented to the user in dialogs, messages etc.
#define PLUGIN_NICENAME     "FMOD Ex Audio Driver"
#define PLUGIN_DETAILS      "Doomsday plugin for audio playback via Firelight Technologies' FMOD Ex "

#define PLUGIN_VERSION_TEXT "1.0.2"
#define PLUGIN_VERSION_TEXTLONG "Version " PLUGIN_VERSION_TEXT " " __DATE__ " (" DSFMOD_VER_ID ")"
#define PLUGIN_VERSION_NUMBER 1,0,2,0 // For WIN32 version info.

#endif
