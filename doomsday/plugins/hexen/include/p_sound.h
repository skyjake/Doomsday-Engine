/**
 * @file p_sound.h
 * Sound related utility routines.
 *
 * @authors Copyright &copy; 2004-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2007-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBHEXEN_SOUND_H
#define LIBHEXEN_SOUND_H

#ifndef __JHEXEN__
#  error "Using jHexen headers without __JHEXEN__"
#endif

#define MAX_SND_DIST        (2025)
#define MAX_CHANNELS        (16)

int S_GetSoundID(const char* name);

/**
 * Starts the song of the specified map, updating the currentmap definition
 * in the process.
 *
 * @param episode  Logical episode.
 * @param map      Logical map number.
 */
void S_MapMusic(uint episode, uint map);

void S_ParseSndInfoLump(void);

#endif // LIBHEXEN_SOUND_H
