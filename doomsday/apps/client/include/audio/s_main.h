/** @file s_main.h  Audio Subsystem
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2014 Daniel Swanson <danij@dengine.net>
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

#ifndef DENG_AUDIO_S_MAIN_H
#define DENG_AUDIO_S_MAIN_H

#ifndef __cplusplus
#  error "s_main.h requires C++"
#endif

extern int showSoundInfo;

/**
 * Main sound system initialization. Inits both the Sfx and Mus modules.
 *
 * @return  @c true, if there were no errors.
 */
dd_bool S_Init(void);

/**
 * Shutdown the whole sound system (Sfx + Mus).
 */
void S_Shutdown(void);

/**
 * Must be called before the map is changed.
 */
void S_MapChange(void);

/**
 * Must be called after the map has been changed.
 */
void S_SetupForChangedMap(void);

/**
 * Draws debug information on-screen.
 */
void S_Drawer(void);

#endif // DENG_AUDIO_S_MAIN_H
