#ifndef __S_SOUND__
#define __S_SOUND__

#ifndef __JDOOM__
#  error "Using jDoom headers without __JDOOM__"
#endif

//#include "d_player.h"
#include "r_defs.h"

#include "SndIdx.h"				   // Sfx and music indices.

#define sfx_chat (gamemode == commercial? sfx_radio : sfx_tink)

void            S_LevelMusic(void);
void            S_SectorSound(sector_t *sector, int sound_id);

/*
   typedef struct
   {
   mobj_t *mo;
   long sound_id;
   long handle;
   long pitch;
   int priority;
   int volume;
   char veryloud;
   } channel_t;

   #define MAX_SND_DIST 1600

   #include "s_common.h"

   //
   // Initializes sound stuff, including volume
   // Sets channels, SFX and music volume,
   //  allocates channel buffer, sets S_sfx lookup.
   //
   void S_Init();

   //
   // Per level startup code.
   // Kills playing sounds at start of level,
   //  determines music if any, changes music.
   //
   void S_Start(void);
   void S_LevelMusic(void);

   // Stop sound for thing at <origin>
   void S_StopSound(void* origin);
   void S_StopSoundNum(mobj_t *origin, int sfxnum);

   // Start music using <music_id> from sounds.h
   void S_StartMusic(int music_id);

   // Start music using <music_id> from sounds.h,
   //  and set whether looping
   void
   S_ChangeMusic
   ( int        music_id,
   int      looping );

   int S_GetMusicNum(int episode, int map);

   // Stops the music fer sure.
   void S_StopMusic(void);

   // Stop and resume music, during game PAUSE.
   void S_PauseSound(void);
   void S_ResumeSound(void);

   //
   // Updates music & sounds
   //
   void S_SetMusicVolume(int volume);
   void S_SetSfxVolume(int volume);
 */

#endif
