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

// Sv_Sound toPlr flags.
#define SVSF_TO_ALL		0x00000010
#define SVSF_MASK		0x7fffffff
#define SVSF_NOT		0x80000000	// Send to all except the specified client.

void	Sv_Sound(int sound_id, mobj_t *origin, int toPlr);
void	Sv_SoundAtVolume(int sound_id_and_flags, mobj_t *origin, int volume, int toPlr);

#endif 