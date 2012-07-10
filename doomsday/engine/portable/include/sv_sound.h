/**
 * @file sv_sound.h
 * Serverside Sound Management.
 *
 * @ingroup server
 *
 * @authors Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_SERVER_SOUND_H
#define LIBDENG_SERVER_SOUND_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup soundPacketFlags  Sound Packet Flags
 * Used with PSV_SOUND packets.
 */
///@{
#define SNDF_ORIGIN             0x01 ///< Sound has an origin.
#define SNDF_SECTOR             0x02 ///< Originates from a degenmobj.
#define SNDF_PLAYER             0x04 ///< Originates from a player.
#define SNDF_VOLUME             0x08 ///< Volume included.
#define SNDF_ID                 0x10 ///< Mobj ID of the origin.
#define SNDF_REPEATING          0x20 ///< Repeat sound indefinitely.
#define SNDF_SHORT_SOUND_ID     0x40 ///< Sound ID is a short.
///@}

/**
 * @defgroup stopSoundPacketFlags  Stop Sound Packet Flags
 * Used with PSV_STOP_SOUND packets.
 */
///@{
#define STOPSNDF_SOUND_ID       0x01
#define STOPSNDF_ID             0x02
#define STOPSNDF_SECTOR         0x04
///@}

/**
 * @defgroup svsoundFlags  Flags for Sv_Sound
 * Used to select the target audience for a sound delta.
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
void Sv_Sound(int soundId, struct mobj_s* origin, int toPlr);

/**
 * Tell clients to play a sound.
 */
void Sv_SoundAtVolume(int soundIdAndFlags, struct mobj_s* origin, float volume, int toPlr);

/**
 * To be called when the server wishes to instruct clients to stop a sound.
 */
void Sv_StopSound(int soundId, struct mobj_s* origin);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /// LIBDENG_SERVER_SOUND_H
