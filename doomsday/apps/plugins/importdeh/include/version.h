/**
 * @file version.h
 * Version information for the DeHackEd patch plugin. @ingroup dehread
 *
 * @authors Copyright &copy; 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDEHREAD_VERSION_H
#define LIBDEHREAD_VERSION_H

#ifndef DEHREAD_VER_ID
#  ifdef _DEBUG
#    define DEHREAD_VER_ID "+D Doomsday"
#  else
#    define DEHREAD_VER_ID "Doomsday"
#  endif
#endif

// Used to derive filepaths.
#define PLUGIN_NAME         "importdeh"

// Presented to the user in dialogs, messages etc.
#define PLUGIN_NICENAME     "Dehacked Importer"
#define PLUGIN_DETAILS      "Doomsday plugin for importing DeHackEd patches."

#define PLUGIN_VERSION_TEXT "2.0.0"
#define PLUGIN_VERSION_TEXTLONG "Version " PLUGIN_VERSION_TEXT " " __DATE__ " (" DEHREAD_VER_ID ")"
#define PLUGIN_VERSION_NUMBER 2,0,0,0 // For WIN32 version info.

#endif // LIBDEHREAD_VERSION_H
