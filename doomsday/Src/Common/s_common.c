
//**************************************************************************
//**
//** S_COMMON.C
//**
//** Sound routines shared by jDoom, jHeretic and jHexen.
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#if __JHEXEN__
#include "h2def.h"
#include "d_net.h"

#elif __JHERETIC__
#include "doomdef.h"
#include "soundst.h"

#else // __JDOOM__
#include "doomdef.h"
#include "doomstat.h"
#include "s_sound.h"
#include "d_config.h"
#include "m_random.h"
#include "m_cheat.h"
#include "d_net.h"
#endif

#include "p_local.h"
//#include "settings.h"
#include "s_common.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

#if __JDOOM__
void S_StopSoundNum(mobj_t *origin, int sfxid);
#endif

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void S_SoundAtVolume(mobj_t *origin, int sound_id, int volume);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

sector_t		*listenerSector = NULL;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int		NextCleanup;
static int		snd_MaxChannels = 20;

// CODE --------------------------------------------------------------------

//===========================================================================
// S_FillSound3D
//	Fills in the position and velocity.
//	Note that mo can only be a real mobj or a degenmobj (sector sound 
//	origin).
//===========================================================================
void S_FillSound3D(mobj_t *mo, sound3d_t *desc)
{
	desc->flags |= DDSOUNDF_POS;
	desc->pos[VX] = mo->x;
	desc->pos[VY] = mo->z;
	desc->pos[VZ] = mo->y;

	if(mo->thinker.function) // Not a degenmobj?
		desc->pos[VY] += mo->height/2;

	if(mo == players[displayplayer].plr->mo)
	{
		int tableAngle = mo->angle >> ANGLETOFINESHIFT;
		int xoff = finecosine[tableAngle] * 64, yoff = finesine[tableAngle] * 64;
		desc->pos[VX] += xoff;
		desc->pos[VZ] += yoff;
	}

	if(mo->thinker.function) // Not a degenmobj?
	{
		desc->flags |= DDSOUNDF_MOV;
		desc->mov[VX] = mo->momx * 35;
		desc->mov[VY] = mo->momz * 35;
		desc->mov[VZ] = mo->momy * 35;
	}
}

//=========================================================================
// S_CalcSep
//	Return value is in range 0-255. 0 means the sound is coming from 
//	the left. 128 is the center.
//=========================================================================
int S_CalcSep(mobj_t *listener, mobj_t *sound, unsigned int dirangle)
{
	unsigned int angle = R_PointToAngle2(listener->x, listener->y, 
		sound->x, sound->y);
	int sep;
	
	sep = (int)(angle>>24) - (int)(dirangle>>24);
	if(sep > 128) 
		sep -= 256;
	else if(sep < -128)
		sep += 256;
	sep = 128 - sep*2;
	if(sep > 256) sep = 512-sep;
	else if(sep < 0) sep = -sep;
	return sep;
}

int S_GetSfxLumpNum(sfxinfo_t *sound)
{
#ifndef __JHEXEN__
	if(sound->name == 0) return 0;
	if(sound->link) sound = sound->link;
#endif
	return W_GetNumForName(sound->lumpname);
}

//=========================================================================
// S_GetFreeChannel
//	Get a free channel, but also get rid of stopped channels.
//	If for_mobj is NULL, any free channel will do. Otherwise, an earlier
//	channel owned by for_mobj is used.
//=========================================================================
channel_t *S_GetFreeChannel(mobj_t *for_mobj)
{
	int			i;
	channel_t	*chan = NULL;

	if(for_mobj && !for_mobj->thinker.function) for_mobj = NULL;
	// Check through all the channels.
	for(i=0; i<numChannels; i++)
	{
		// Is this a free channel?
		if(!chan && !gi.SoundIsPlaying(Channel[i].handle))
		{
			// This will be used if a clear channel is needed.
			chan = Channel + i;
			if(!for_mobj) break; // This'll do.
		}
		if(for_mobj && Channel[i].mo == for_mobj)
		{
			// Mobjs are allowed only one channel.
			chan = Channel + i;
			break;
		}
	}
	// No suitable channel found, and there already are as much 
	// channels as there can be?
	if(!chan && numChannels >= snd_MaxChannels)
	{
		// Pick one at random, then.
		chan = Channel + M_Random() % numChannels;
	}
	// Was a suitable channel found?
	if(chan)
	{
		S_StopChannel(chan);
		return chan;
	}	
	// We need to allocate a new channel.
	Channel = realloc(Channel, sizeof(channel_t) * (++numChannels));
	chan = Channel + numChannels-1;
	memset(chan, 0, sizeof(channel_t));
	return chan;
}

//=========================================================================
// S_StartSoundAtVolume
//	Start playing the given sound, and broadcast it to everybody.
//=========================================================================
void S_StartSoundAtVolume(mobj_t *origin, int sound_id, int volume)
{
	// The sound is audible to everybody.
	NetSv_SoundAtVolume(origin, sound_id, volume, NSSF_TO_ALL);
	S_SoundAtVolume(origin, sound_id, volume);
}

//=========================================================================
// S_SoundAtVolume
//	Start playing the given sound.
//=========================================================================
void S_SoundAtVolume(mobj_t *origin, int sound_id, int volume)
{
	int dist;
	int absx;
	int absy;
	int vol;
	int sep;
	mobj_t *plrmo = players[displayplayer].plr->mo;
	static int sndcount = 0;
	channel_t *channel;

	if(sound_id == 0 || Get(DD_SFX_VOLUME) == 0)
		return;

	if(volume == 0)	return;

#if __JDOOM__
	// Doom's chainsaw sounds need some special handling.
	if(sound_id == sfx_sawup
		|| sound_id == sfx_sawidl
		|| sound_id == sfx_sawful
		|| sound_id == sfx_sawhit)
	{
		// Stop any previous instances from the same origin.
		S_StopSoundNum(origin, sfx_sawup);
		S_StopSoundNum(origin, sfx_sawidl);
		S_StopSoundNum(origin, sfx_sawful);
		S_StopSoundNum(origin, sfx_sawhit);
	}
#endif

	/*Con_Message( "Play %s, origin %p, vol %i, use %i.\n", 
		S_sfx[sound_id].name, origin, volume,
		S_sfx[sound_id].usefulness);*/

	// calculate the distance before other stuff so that we can throw out
	// sounds that are beyond the hearing range.
	if(volume >= 255) // A very loud sound?
	{
		dist = 0;	
		// Might as well be; the sound can be heard from a great distance.
	}
	else if(plrmo && origin)
	{
		absx = abs(origin->x - plrmo->x);
		absy = abs(origin->y - plrmo->y);
		dist = absx+absy-(absx > absy ? absy>>1 : absx>>1);
		dist >>= FRACBITS;
		if(dist >= MAX_SND_DIST)
		{
			return; // sound is beyond the hearing range...
		}
		if(dist < 0) dist = 0;
	}
	else
	{
		// The player has no mo! We're probably in the start screen.
		origin = NULL;	
		dist = 0;		// The sound is very close.
	}

	// We won't play the same sound too many times.
	if(S_sfx[sound_id].usefulness >= 5 && origin && origin != plrmo)
	{
		// We'll replace the most distant of the existing sounds.
		int	i, maxdist = 0;
		for(i=0; i<numChannels; i++)
		{
			if(Channel[i].sound_id == sound_id && Channel[i].priority >= maxdist)
			{
				maxdist = Channel[i].priority;
				channel = Channel + i;
			}
		}
		if(dist > maxdist) return; // Don't replace a close sound with a far one.
		gi.StopSound(channel->handle);
	}
	else
	{
		if(S_sfx[sound_id].lumpnum == 0)
		{
			S_sfx[sound_id].lumpnum = S_GetSfxLumpNum(&S_sfx[sound_id]);
		}
		if(S_sfx[sound_id].data == NULL)
		{
#if __JHEXEN__
			if(UseSndScript)
			{
				char name[128];
				sprintf(name, "%s%s.lmp", ArchivePath, 
					S_sfx[sound_id].lumpname);
				M_ReadFile(name, (byte **)&S_sfx[sound_id].data);
			}
			else
#endif
				S_sfx[sound_id].data = W_CacheLumpNum
					(S_sfx[sound_id].lumpnum,PU_SOUND);
		}
		if(S_sfx[sound_id].usefulness < 0)
		{
			S_sfx[sound_id].usefulness = 1;
		}
		else
		{
			S_sfx[sound_id].usefulness++;
		}		
		// The player can have any number of sounds, but other mobjs only one.
		channel = S_GetFreeChannel(origin == plrmo? NULL : origin);
	}
	channel->mo = origin;
	channel->volume = volume;
	if(channel->volume > 127) channel->volume = 127;
	channel->veryloud = (volume >= 255);

#if __JHEXEN__
	if(S_sfx[sound_id].flags & 1)
		channel->pitch = (byte) (127+(M_Random()&7)-(M_Random()&7));
	else
		channel->pitch = 127;

#elif __JDOOM__
	channel->pitch = 127;
	if(sound_id >= sfx_sawup && sound_id <= sfx_sawhit)
		channel->pitch += 8 - (M_Random()&15);
	else if(sound_id != sfx_itemup && sound_id != sfx_tink)
		channel->pitch += 16 - (M_Random()&31);
	// Check that pitch is in range.
	if(channel->pitch < 0) channel->pitch = 0;
	else if(channel->pitch > 255) channel->pitch = 255;

#else
	channel->pitch = (byte) (127+(M_Random()&7)-(M_Random()&7));
#endif

	channel->sound_id = sound_id;
	channel->priority = dist;

	if(cfg.snd_3D && origin) // Play the sound in 3D?
	{
		sound3d_t desc;
		desc.flags = DDSOUNDF_VOLUME | DDSOUNDF_PITCH;
		// Very loud sounds have virtually no rolloff.
		if(channel->veryloud) desc.flags |= DDSOUNDF_VERY_LOUD;
		desc.volume = (channel->volume*1000)/127;
		desc.pitch = (channel->pitch*1000)/128;
		//if(origin == plrmo) desc.flags |= DDSOUNDF_LOCAL;
		S_FillSound3D(origin, &desc);

		if(!channel->handle)
			channel->handle = gi.Play3DSound(S_sfx[sound_id].data, &desc);
		else
			gi.Update3DSound(channel->handle, &desc);
	}
	else // Play the sound in 2D.
	{
		if(!origin || !plrmo || origin == plrmo)
		{
			sep = 128;
			vol = channel->volume;
		}
		else
		{
#if __JHEXEN__
			vol = (SoundCurve[dist]*(15*8)*channel->volume)>>14;
#else
			vol = SOUNDCURVE(dist, channel->volume); 
#endif
			sep = S_CalcSep(plrmo, origin, Get(DD_VIEWANGLE));
		}
		if(!channel->handle)
		{
			channel->handle = gi.PlaySound(S_sfx[sound_id].data, 
				DDVOL(vol), DDPAN(sep), DDPITCH(channel->pitch));
		}
		else
		{
			gi.UpdateSound(channel->handle, 
				DDVOL(vol), DDPAN(sep), DDPITCH(channel->pitch));
		}
	}

#ifdef PRINT_DEBUG_INFO
	printf( "PLAY:   i:%d handle:%d volume:%3d vol:%3d lump:%8s mo:%p\n", i, Channel[i].handle, 
		Channel[i].volume, vol, S_sfx[Channel[i].sound_id].lumpname, Channel[i].mo);
#endif
}

//=========================================================================
// S_StartSound
//	Play a world sound. It will be broadcasted to all players in the game.
//=========================================================================
void S_StartSound(mobj_t *origin, int sound_id)
{
	// The sound is audible to everybody.
	NetSv_Sound(origin, sound_id, NSSF_TO_ALL);
	S_SoundAtVolume(origin, sound_id, 127);
}

//=========================================================================
// S_PlayerSound
//	Play a player sound. Only the specified player will hear it.
//=========================================================================
void S_PlayerSound(mobj_t *origin, int sound_id, player_t *player)
{
	if(player == &players[consoleplayer])
	{
		// It's for us!
		S_SoundAtVolume(origin, sound_id, 127);
	}
	/*else*/ 
	NetSv_Sound(origin, sound_id, player - players);
}

//=========================================================================
// S_LocalSound
//	Play a local sound.
//=========================================================================
void S_LocalSound(mobj_t *origin, int sound_id)
{
	S_LocalSoundAtVolume(origin, sound_id, 127);
}

//=========================================================================
// S_LocalSoundAtVolume
//	Play a local sound.
//=========================================================================
void S_LocalSoundAtVolume(mobj_t *origin, int sound_id, int volume)
{
	S_SoundAtVolume(origin, sound_id, volume);
}

//=========================================================================
// S_UpdateSounds
//=========================================================================
void S_UpdateSounds(mobj_t *listener)
{
	int i;
	int dist, vol;
	int sep;
	int absx;
	int absy;
	
	if(!listener || Get(DD_SFX_VOLUME) == 0) return;

	// Update the listener first.
	if(cfg.snd_3D)
	{
		listener3d_t lis;
		lis.flags = DDLISTENERF_POS | DDLISTENERF_MOV | DDLISTENERF_YAW | DDLISTENERF_PITCH;
		lis.pos[VX] = listener->x;
		lis.pos[VY] = listener->z + listener->height - (5<<FRACBITS);
		lis.pos[VZ] = listener->y;
		lis.mov[VX] = listener->momx * 35;
		lis.mov[VY] = listener->momz * 35;
		lis.mov[VZ] = listener->momy * 35;
		lis.yaw = -(listener->angle / (float) ANGLE_MAX * 360 - 90);
		lis.pitch = listener->player? LOOKDIR2DEG(listener->player->plr->lookdir) : 0;
		// If the sector changes, so does the reverb.
		if(listener->subsector->sector != listenerSector 
			&& cfg.snd_ReverbFactor > 0)
		{
			listenerSector = listener->subsector->sector;
			lis.flags |= DDLISTENERF_SET_REVERB;
			lis.reverb.space = listenerSector->reverb[SRD_SPACE];
			lis.reverb.decay = listenerSector->reverb[SRD_DECAY];
			lis.reverb.volume = listenerSector->reverb[SRD_VOLUME] 
				* cfg.snd_ReverbFactor;
			lis.reverb.damping = listenerSector->reverb[SRD_DAMPING];
			if(cfg.reverbDebug)
			{
				Con_Message( "Sec %i: s:%.2f dc:%.2f v:%.2f dm:%.2f\n", 
					listenerSector-sectors, lis.reverb.space, 
					lis.reverb.decay, lis.reverb.volume, lis.reverb.damping);
			}
		}
		if(cfg.snd_ReverbFactor == 0 && listenerSector)
		{
			listenerSector = NULL;
			lis.flags |= DDLISTENERF_DISABLE_REVERB;
		}
		gi.UpdateListener(&lis);
	}
	
#if __JHEXEN__
	// Update any Sequences
	SN_UpdateActiveSequences();
#endif

	if(NextCleanup < gametic)
	{
#if __JHEXEN__
		if(UseSndScript)
		{
			for(i = 0; i < MAXSFX; i++)
			{
				if(S_sfx[i].usefulness == 0 && S_sfx[i].data)
				{
					S_sfx[i].usefulness = -1;
				}
			}
		}
		else
#endif
		{
			for(i = 0; i < MAXSFX; i++)
			{
				if(S_sfx[i].usefulness == 0 && S_sfx[i].data)
				{
					W_ChangeCacheTag(S_sfx[i].lumpnum, PU_CACHE);
					S_sfx[i].usefulness = -1;
					S_sfx[i].data = NULL;
				}
			}
		}
		NextCleanup = gametic + 35*30; // every 30 seconds
	}
	for(i=0; i<numChannels; i++)
	{
		if(!Channel[i].handle || !Channel[i].sound_id) continue; // Empty channel.

		// Take care of stopped sounds.
		if(!gi.SoundIsPlaying(Channel[i].handle))
		{
			S_StopChannel(Channel + i);
			continue;
		}

		// Does this sound need updating?
		if(Channel[i].mo == NULL)
		{
#ifdef PRINT_DEBUG_INFO
			printf( "UPD/CN: i:%d handle:%d volume:%3d vol:--- lump:%8s mo:%p\n", i, Channel[i].handle, 
				Channel[i].volume, S_sfx[Channel[i].sound_id].lumpname, Channel[i].mo);
#endif
			// No, apparently not.
			continue;
		}
		else
		{
			// The sound has a source.
			if(cfg.snd_3D)
			{
				sound3d_t desc;
				desc.flags = 0;
				// Fill in position and velocity.
				S_FillSound3D(Channel[i].mo, &desc);
				gi.Update3DSound(Channel[i].handle, &desc);
			}
			else // 2D mode.
			{
				absx = abs(Channel[i].mo->x - listener->x);
				absy = abs(Channel[i].mo->y - listener->y);
				dist = absx+absy-(absx > absy ? absy>>1 : absx>>1);
				dist >>= FRACBITS;
				if(dist >= MAX_SND_DIST && !Channel[i].veryloud)
				{
					S_StopSound(Channel[i].mo);
					continue;
				}
				if(dist < 0) dist = 0;
				if(Channel[i].veryloud)
					vol = 127;
				else
				{
#if __JHEXEN__
					vol = (SoundCurve[dist]*(15*8)*Channel[i].volume)>>14;
#else
					vol = SOUNDCURVE(dist, Channel[i].volume);
#endif
				}
				if(Channel[i].mo == listener)
				{
					sep = 128;
				}
				else
				{
					sep = S_CalcSep(listener, Channel[i].mo, 
						Get(DD_VIEWANGLE));
				}
				gi.UpdateSound(Channel[i].handle, 
					DDVOL(vol), DDPAN(sep), DDPITCH(Channel[i].pitch));
			}

#ifdef PRINT_DEBUG_INFO
			printf( "UPDATE: i:%d handle:%d volume:%3d vol:%3d lump:%8s mo:%p\n", i, Channel[i].handle, 
				Channel[i].volume, vol, S_sfx[Channel[i].sound_id].lumpname, Channel[i].mo);
#endif
		}
	}
}

#if __JDOOM__
void S_SetMusicVolume(int volume)
{
	gi.SetMIDIVolume((volume*255)/15);
}
#endif

#if __JHERETIC__
void S_SetMusicVolume(void)
{
	gi.SetMIDIVolume(snd_MusicVolume);
	if(snd_MusicVolume == 0)
	{
		gi.PauseSong();
		MusicPaused = true;
	}
	else if(MusicPaused)
	{
		MusicPaused = false;
		gi.ResumeSong();
	}
}
#endif

// Broadcasts the sound to everybody.
/*void S_NetStartSound(mobj_t *origin, int sound_id)
{
	S_StartSound(origin, sound_id);
	NetSv_Sound(origin, sound_id, NSSF_TO_ALL);
}

void S_NetStartSoundAtVolume(mobj_t *origin, int sound_id, int vol)
{
	S_StartSoundAtVolume(origin, sound_id, vol);
	NetSv_SoundAtVolume(origin, sound_id, vol, NSSF_TO_ALL);
}

void S_NetStartPlrSound(player_t *player, int sound_id)
{
	if(player == &players[consoleplayer])
		S_StartSound(NULL, sound_id);
	NetSv_Sound(NULL, sound_id, player-players);
}

// Broadcasts the sound to everybody except 'not_to_player'.
void S_NetAvoidStartSound(mobj_t *origin, int sound_id, player_t *not_to_player)
{
	S_StartSound(origin, sound_id);
	NetSv_Sound(origin, sound_id, NSSF_NOT | (not_to_player-players));
}*/

//=========================================================================
// S_SectorSound
//	Play a sector sound. Everybody will hear it.
//=========================================================================
void S_SectorSound(sector_t *sector, int sound_id)
{
	S_StartSound( (mobj_t*) &sector->soundorg, sound_id);
}

//==========================================================================
//
// CONSOLE COMMANDS
//
//==========================================================================

int CCmdCD(int argc, char **argv)
{
	if(argc > 1)
	{
		if(!strcmpi(argv[1], "init"))
		{
			if(!gi.CD(DD_INIT,0))
				Con_Printf( "CD init successful.\n");
			else
				Con_Printf( "CD init failed.\n");
		}
		else if(!strcmpi(argv[1], "info") && argc == 2)
		{
			int secs = gi.CD(DD_GET_TIME_LEFT, 0);
			Con_Printf( "CD available: %s\n", gi.CD(DD_AVAILABLE,0)? "yes" : "no");
			Con_Printf( "First track: %d\n", gi.CD(DD_GET_FIRST_TRACK,0));
			Con_Printf( "Last track: %d\n", gi.CD(DD_GET_LAST_TRACK,0));
			Con_Printf( "Current track: %d\n", gi.CD(DD_GET_CURRENT_TRACK,0));
			Con_Printf( "Time left: %d:%02d\n", secs/60, secs%60);
			Con_Printf( "Play mode: ");
			if(MusicPaused)
				Con_Printf( "paused\n");
			else if(s_CDTrack)
				Con_Printf( "looping track %d\n", s_CDTrack);
			else
				Con_Printf( "map track\n");
			return true;
		}
		else if(!strcmpi(argv[1], "play") && argc == 3)
		{
			s_CDTrack = atoi(argv[2]);
			if(!gi.CD(DD_PLAY_LOOP, s_CDTrack)) 
			{
				Con_Printf( "Playing track %d.\n", s_CDTrack);
			}
			else
			{
				Con_Printf( "Error playing track %d.\n", s_CDTrack);
				return false;
			}
		}
#if __JHEXEN__
		else if(!strcmpi(argv[1], "map"))
		{
			int track;
			int mapnum = gamemap;
			if(argc == 3) mapnum = atoi(argv[2]);
			s_CDTrack = false;	// Clear the user selection.
			track = P_GetMapCDTrack(mapnum);			
			if(!gi.CD(DD_PLAY_LOOP, track)) // Uses s_CDTrack.
			{
				Con_Printf( "Playing track %d.\n", track);
			}
			else
			{
				Con_Printf( "Error playing track %d.\n", track);
				return false;
			}
		}
#endif
		else if(!strcmpi(argv[1], "stop") && argc == 2)
		{
			gi.CD(DD_STOP,0);
			Con_Printf( "CD stopped.\n");
		}
		else if(!strcmpi(argv[1], "resume") && argc == 2)
		{
			gi.CD(DD_RESUME,0);
			Con_Printf( "CD resumed.\n");
		}
		else
			Con_Printf( "Bad command. Try 'cd'.\n");
	}
	else
	{
		Con_Printf( "CD player control. Usage: CD (cmd)\n");
		Con_Printf( "Commands are: init, info, play (track#), map, map (#), stop, resume.\n");
	}
	return true;
}

int CCmdMidi(int argc, char **argv)
{
	if(argc == 1)
	{
		Con_Printf( "Usage: midi (cmd)\n");
#if __JDOOM__
		Con_Printf( "Commands are: reset, play (num).\n");
#else
		Con_Printf( "Commands are: reset, play (name), map, map (num).\n");
#endif
		return true;
	}
	if(argc == 2)
	{
		if(!strcmpi(argv[1], "reset"))
		{
#if __JDOOM__
			if(!strcmpi(argv[1], "reset"))
			{
				S_StopMusic();
				Con_Printf( "MIDI has been reset.\n");
			}
			else
				return false;
#else
			if(RegisteredSong)
			{
				gi.StopSong();
#if __JHEXEN__
				if(UseSndScript)
					Z_Free(Mus_SndPtr);
				else
#endif
					gi.W_ChangeCacheTag(Mus_LumpNum, PU_CACHE);
				RegisteredSong = 0;
			}
			Mus_Song = -1;
			Con_Printf( "MIDI has been reset.\n");
#endif
		}
		else if(!strcmpi(argv[1], "map")) 
		{
			Con_Printf( "Playing the song of the current map (%d).\n", gamemap);
#if __JDOOM__
			S_ChangeMusic(S_GetMusicNum(gameepisode, gamemap), true);
#else
			S_StartSong(gamemap, true);
#endif
		}
		else
			return false;
	}
	else if(argc == 3)
	{
#if __JDOOM__
		if(!strcmpi(argv[1], "play"))
		{
			char buf[10];
			sprintf(buf, "%.2i", atoi(argv[2]));
			Con_Printf( "Playing song %s.\n", buf);
			cht_MusicFunc(&players[consoleplayer], buf);
		}
#else
		if(i_CDMusic)
		{	
			Con_Printf( "MIDI is not the current music device.\n");
			return true;
		}
#if __JHEXEN__
		if(!strcmpi(argv[1], "play"))
		{
			Con_Printf( "Playing song '%s'.\n", argv[2]);
			S_StartSongName(argv[2], true);
		}
		else
#endif
		if(!strcmpi(argv[1], "map"))
		{
			Con_Printf( "Playing song for map %d.\n", atoi(argv[2]));
			S_StartSong(atoi(argv[2]), true);
		}
#endif
		else
			return false;
	}
	else 
		return false;
	// Oh, we're done.
	return true;
}



