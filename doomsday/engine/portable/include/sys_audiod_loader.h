/**\file sys_audiod_loader.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2009-2011 Daniel Swanson <danij@dengine.net>
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
 * Audio Driver Loader.
 */

#ifndef LIBDENG_SYSTEM_AUDIO_LOADER_H
#define LIBDENG_SYSTEM_AUDIO_LOADER_H

#include "sys_audiod.h"

/**
 * Attempt to load the specified audio driver, import the entry points and
 * add to the available audio drivers.
 *
 * @param name  Name of the driver to be loaded e.g., "openal".
 * @return  Ptr to the audio driver interface if successful, else @c NULL.
 */
audiodriver_t* Sys_LoadAudioDriver(const char* name);

void Sys_ShutdownAudioDriver(void);
#endif /* LIBDENG_SYSTEM_AUDIO_LOADER_H */
