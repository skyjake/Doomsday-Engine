//===========================================================================
// SV_SOUND.H
//===========================================================================
#ifndef __DOOMSDAY_SERVER_SOUND_H__
#define __DOOMSDAY_SERVER_SOUND_H__

// Sound packet (psv_sound) flags.
#define SNDF_ORIGIN			0x01		// Sound has an origin.
#define SNDF_SECTOR			0x02		// Originates from a degenmobj.
#define SNDF_PLAYER			0x04		// Originates from a player.
#define SNDF_VOLUME			0x08		// Volume included.
#define SNDF_ID				0x10		// Mobj ID of the origin.
#define SNDF_REPEATING		0x20		// Repeat sound indefinitely.
#define SNDF_SHORT_SOUND_ID	0x40		// Sound ID is a short.

// Stop Sound packet (psv_stop_sound) flags.
#define STOPSNDF_SOUND_ID	0x01
#define STOPSNDF_ID			0x02
#define STOPSNDF_SECTOR		0x04

// Sv_Sound toPlr flags.
#define SVSF_TO_ALL		0x00000010
#define SVSF_MASK		0x7fffffff

void	Sv_Sound(int sound_id, mobj_t *origin, int toPlr);
void	Sv_SoundAtVolume(int sound_id_and_flags, mobj_t *origin, float volume, int toPlr);
void	Sv_StopSound(int sound_id, mobj_t *origin);

#endif 