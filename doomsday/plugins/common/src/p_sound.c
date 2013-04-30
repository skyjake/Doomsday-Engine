/**
 * @file p_sound.h
 * id tech 1 sound playback functionality for the play simulation.
 *
 * @ingroup play
 *
 * @authors Copyright &copy; 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2005-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright &copy; 1999 Activision
 * @authors Copyright &copy; 1993-1996 by id Software, Inc.
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

#include "common.h"
#include "dmu_lib.h"
#include "p_sound.h"

void S_MapMusic(uint episode, uint map)
{
#ifdef __JHEXEN__
    int idx = Def_Get(DD_DEF_MUSIC, "currentmap", 0);
    int cdTrack = P_GetMapCDTrack(map);

    // Update the 'currentmap' music definition.

    VERBOSE( Con_Message("S_MapMusic: Ep %i, map %i, lump %s", episode, map, P_GetMapSongLump(map)) )

    Def_Set(DD_DEF_MUSIC, idx, DD_LUMP, P_GetMapSongLump(map));
    Def_Set(DD_DEF_MUSIC, idx, DD_CD_TRACK, &cdTrack);

    if(S_StartMusic("currentmap", true))
    {
        // Set the game status cvar for the map music
        gsvMapMusic = idx;
    }
#else
    Uri* mapUri = G_ComposeMapUri(episode, map);
    AutoStr* mapPath = Uri_Compose(mapUri);
    ddmapinfo_t mapInfo;
    if(Def_Get(DD_DEF_MAP_INFO, Str_Text(mapPath), &mapInfo))
    {
        if(S_StartMusicNum(mapInfo.music, true))
        {
            // Set the game status cvar for the map music.
            gsvMapMusic = mapInfo.music;
        }
    }
    Uri_Delete(mapUri);
#endif
}

void S_SectorSound(Sector* sec, int id)
{
    if(!sec) return;

    S_SectorStopSounds(sec);
    S_StartSound(id, (mobj_t*) P_GetPtrp(sec, DMU_EMITTER));
}

void S_SectorStopSounds(Sector* sec)
{
    if(!sec) return;

    // Stop other sounds playing from origins in this sector.
    /// @todo Add a compatibility option allowing origins to work independently?
    S_StopSound2(0, (mobj_t*) P_GetPtrp(sec, DMU_EMITTER), SSF_ALL_SECTOR);
}

void S_PlaneSound(Plane* pln, int id)
{
    if(!pln) return;

    S_SectorStopSounds((Sector*) P_GetPtrp(pln, DMU_SECTOR));
    S_StartSound(id, (mobj_t*) P_GetPtrp(pln, DMU_EMITTER));
}

#ifdef __JHEXEN__
int S_GetSoundID(const char* name)
{
    return Def_Get(DD_DEF_SOUND_BY_NAME, name, 0);
}

void S_ParseSndInfoLump(void)
{
    lumpnum_t lump = W_CheckLumpNumForName("SNDINFO");
    char buf[80];

    if(lump != -1)
    {
        SC_OpenLump(lump);

        while(SC_GetString())
        {
            if(*sc_String == '$')
            {
                if(!stricmp(sc_String, "$ARCHIVEPATH"))
                {
                    SC_MustGetString();
                    // Presently unused.
                }
                else if(!stricmp(sc_String, "$MAP"))
                {
                    SC_MustGetNumber();
                    SC_MustGetString();
                    if(sc_Number - 1 >= 0)
                    {
                        P_PutMapSongLump(sc_Number - 1, sc_String);
                    }
                }
                continue;
            }
            else
            {
                int soundId = Def_Get(DD_DEF_SOUND_BY_NAME, sc_String, 0);
                if(soundId)
                {
                    SC_MustGetString();
                    Def_Set(DD_DEF_SOUND, soundId, DD_LUMP,
                            *sc_String != '?' ? sc_String : "default");
                }
                else
                {
                    // Read the lumpname anyway.
                    SC_MustGetString();
                }
            }
        }
        SC_Close();
    }

    // All sounds left without a lumpname will use "DEFAULT".
    { int i;
    for(i = 0; i < Get(DD_NUMSOUNDS); ++i)
    {
        /// @kludge This uses a kludge to traverse the entire sound list.
        /// @todo Implement a mechanism for walking the Def databases.
        Def_Get(DD_DEF_SOUND_LUMPNAME, (char*) &i, buf);
        if(!strcmp(buf, ""))
            Def_Set(DD_DEF_SOUND, i, DD_LUMP, "default");
    }}

    if(gameMode == hexen_betademo)
    {
        // The WAD contains two lumps with the name CHAIN, one a sample and
        // the other a graphics lump.
        int soundId = Def_Get(DD_DEF_SOUND_BY_NAME, "AMBIENT12", 0);
        Def_Get(DD_DEF_SOUND_LUMPNAME, (char*) &soundId, buf);
        if(!strcasecmp(buf, "chain"))
        {
            Def_Set(DD_DEF_SOUND, soundId, DD_LUMP, "default");
        }
    }
}
#endif
