
//**************************************************************************
//**
//** P_SOUND.C
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include "jHeretic/Doomdef.h"
#include "jHeretic/h_stat.h"
#include "jHeretic/Soundst.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int gsvMapMusic;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/*
 * Start the song for the current map.
 */
void S_LevelMusic(void)
{
    ddmapinfo_t info;
    char    id[10];
    int    songid = 0;

    if(gamestate != GS_LEVEL)
        return;

    sprintf(id, "E%iM%i", gameepisode, gamemap);
    if(Def_Get(DD_DEF_MAP_INFO, id, &info) && info.music >= 0)
    {
        songid = info.music;
        S_StartMusicNum(songid, true);
    }
    else
    {
        songid = (gameepisode - 1) * 9 + gamemap - 1;
        S_StartMusicNum(songid, true);
    }

    // set the map music game status cvar
    gsvMapMusic = songid;
}

/*
 * Doom-like sector sounds: when a new sound starts, stop any old ones
 * from the same origin.
 *
 * @param sec           Sector in which the sound should be played.
 * @param origin        Origin of the sound (center/floor/ceiling).
 * @param id            ID number of the sound to be played.
 */
void S_SectorSound(sector_t *sec, int origin, int id)
{
    mobj_t *centerorigin = (mobj_t *) P_GetPtrp(sec, DMU_SOUND_ORIGIN);
    mobj_t *floororigin = (mobj_t *) P_GetPtrp(sec, DMU_FLOOR_SOUND_ORIGIN);
    mobj_t *ceilingorigin = (mobj_t *) P_GetPtrp(sec, DMU_CEILING_SOUND_ORIGIN);

    S_StopSound(0, centerorigin);
    S_StopSound(0, floororigin);
    S_StopSound(0, ceilingorigin);

    switch(origin)
    {
    case SORG_FLOOR:
        S_StartSound(id, floororigin);
        break;

    case SORG_CEILING:
        S_StartSound(id, ceilingorigin);
        break;

    case SORG_CENTER:
    default:
        S_StartSound(id, centerorigin);
        break;
    }
}
