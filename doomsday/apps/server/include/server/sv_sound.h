/** @file sv_sound.h Serverside Sound Management.
 * @ingroup server
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DE_SERVER_SOUND_H
#define DE_SERVER_SOUND_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup svsoundFlags  Flags for Sv_Sound
 * Used to select the target audience for a sound delta.
 * @ingroup flags
 */
///@{
#define SVSF_TO_ALL             0x01000000
#define SVSF_EXCLUDE_ORIGIN     0x02000000 ///< Sound is not sent to origin player.
#define SVSF_MASK               0x7fffffff
///@}

struct mobj_s;

/**
 * Tell clients to play a sound with full volume.
 */
void Sv_Sound(int soundId, const struct mobj_s *origin, int toPlr);

/**
 * Tell clients to play a sound.
 */
void Sv_SoundAtVolume(int soundIdAndFlags, const struct mobj_s *origin, float volume, int toPlr);

/**
 * To be called when the server wishes to instruct clients to stop a sound.
 */
void Sv_StopSound(int soundId, const struct mobj_s *origin);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // DE_SERVER_SOUND_H
