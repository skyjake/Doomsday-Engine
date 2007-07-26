/**\file
 *\section License
 * License: GPL + jHeretic/jHexen Exception
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2004-2007 Jaakko Keränen <skyjake@dengine.net>
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
 *
 * In addition, as a special exception, we, the authors of deng
 * give permission to link the code of our release of deng with
 * the libjhexen and/or the libjheretic libraries (or with modified
 * versions of it that use the same license as the libjhexen or
 * libjheretic libraries), and distribute the linked executables.
 * You must obey the GNU General Public License in all respects for
 * all of the code used other than “libjhexen or libjheretic”. If
 * you modify this file, you may extend this exception to your
 * version of the file, but you are not obligated to do so. If you
 * do not wish to do so, delete this exception statement from your version.
 */

#ifndef __SOUNDSTH__
#define __SOUNDSTH__

#ifndef __JHEXEN__
#  error "Using jHexen headers without __JHEXEN__"
#endif

//#define S_sfx (*gi.sounds)

#define MAX_SND_DIST    2025
#define MAX_CHANNELS    16

/*typedef struct
   {
   char name[8];
   int p1;
   } musicinfo_t; */

/*typedef struct sfxinfo_s
   {
   char tagName[32];
   char lumpname[12]; // Only need 9 bytes, but padded out to be dword aligned
   //struct sfxinfo_s *link; // Make alias for another sound
   int priority; // Higher priority takes precendence
   int usefulness; // Determines when a sound should be cached out
   void *snd_ptr;
   int lumpnum;
   int numchannels; // total number of channels a sound type may occupy
   boolean  changePitch;
   } sfxinfo_t; */

/*typedef struct
   {
   mobj_t *mo;
   int sound_id;
   int handle;
   int volume;
   int pitch;
   int priority;
   char veryloud;
   } channel_t;

   typedef struct
   {
   long id;
   unsigned short priority;
   char *name;
   mobj_t *mo;
   int distance;
   } ChanInfo_t;

   typedef struct
   {
   int channelCount;
   int musicVolume;
   int soundVolume;
   ChanInfo_t chan[16];
   } SoundInfo_t;
 */
//extern int snd_MaxVolume;
//extern int snd_MusicVolume;

int             S_GetSoundID(char *name);
void            S_LevelMusic(void);

/*void S_ShutDown(void);
   void S_Start(void);
   void S_StartSound(mobj_t *origin, int sound_id);
   void S_StartSoundAtVolume(mobj_t *origin, int sound_id, int volume);
   void S_StopSound(mobj_t *origin);
   void S_StopAllSound(void);
   void S_PauseSound(void);
   void S_ResumeSound(void);
   void S_UpdateSounds(mobj_t *listener);
   void S_StartSong(int song, boolean loop);
   void S_StartSongName(char *songLump, boolean loop);
   void S_StopSong(void);
   void S_Init(void);
   void S_Reset(void);
   void S_GetChannelInfo(SoundInfo_t *s);
   void S_SetMusicVolume(void);
   boolean S_GetSoundPlayingInfo(mobj_t *mobj, int sound_id); */

#endif
