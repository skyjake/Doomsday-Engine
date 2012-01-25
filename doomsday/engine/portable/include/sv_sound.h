/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

/*
 * sv_sound.h: Serverside Sound Management
 */

#ifndef __DOOMSDAY_SERVER_SOUND_H__
#define __DOOMSDAY_SERVER_SOUND_H__

// Sound packet (PSV_SOUND) flags.
#define SNDF_ORIGIN			0x01   // Sound has an origin.
#define SNDF_SECTOR			0x02   // Originates from a degenmobj.
#define SNDF_PLAYER			0x04   // Originates from a player.
#define SNDF_VOLUME			0x08   // Volume included.
#define SNDF_ID				0x10   // Mobj ID of the origin.
#define SNDF_REPEATING		0x20   // Repeat sound indefinitely.
#define SNDF_SHORT_SOUND_ID	0x40   // Sound ID is a short.

// Stop Sound packet (PSV_STOP_SOUND) flags.
#define STOPSNDF_SOUND_ID	0x01
#define STOPSNDF_ID			0x02
#define STOPSNDF_SECTOR		0x04

// Sv_Sound toPlr flags.
#define SVSF_TO_ALL         0x01000000
#define SVSF_EXCLUDE_ORIGIN 0x02000000 // Sound is not sent to origin player.
#define SVSF_MASK           0x7fffffff

void            Sv_Sound(int sound_id, mobj_t *origin, int toPlr);
void            Sv_SoundAtVolume(int sound_id_and_flags, mobj_t *origin,
								 float volume, int toPlr);
void            Sv_StopSound(int sound_id, mobj_t *origin);

#endif
