/** @file con_config.h  Config file IO.
 * @ingroup console
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
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

#ifndef DE_CONSOLE_CONFIG_H
#define DE_CONSOLE_CONFIG_H

#include "dd_share.h"

#include <de/file.h>
#include <de/nativepath.h>

// Flags for Con_ParseCommands:
#define CPCF_SILENT                 0x1
#define CPCF_ALLOW_SAVE_STATE       0x2
#define CPCF_ALLOW_SAVE_BINDINGS    0x4

void Con_SetAllowed(int flags);

bool Con_ParseCommands(const de::NativePath &nativePath, int flags = 0);

bool Con_ParseCommands(const de::File &file, int flags = 0);

void Con_SetDefaultPath(const de::Path &path);

/**
 * Saves all bindings, aliases and archiveable console variables.
 * The output file is a collection of console commands.
 */
void Con_SaveDefaults();

void Con_SaveDefaultsIfChanged();

D_CMD(WriteConsole);

#endif // DE_CONSOLE_CONFIG_H
