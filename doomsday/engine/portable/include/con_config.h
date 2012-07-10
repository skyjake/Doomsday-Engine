/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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
 * Config Files.
 */

#ifndef LIBDENG_CONSOLE_CONFIG_H
#define LIBDENG_CONSOLE_CONFIG_H

// Flags for Con_ParseCommands2.
#define CPCF_SET_DEFAULT            0x1
#define CPCF_ALLOW_SAVE_STATE       0x2
#define CPCF_ALLOW_SAVE_BINDINGS    0x4

boolean Con_ParseCommands(const char* fileName);

boolean Con_ParseCommands2(const char* fileName, int flags);

void Con_SaveDefaults(void);

boolean Con_WriteState(const char* fileName, const char* bindingsFileName);

D_CMD(WriteConsole);

#endif /* LIBDENG_CONSOLE_CONFIG_H */
