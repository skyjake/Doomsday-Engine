/** @file p_sound.cpp  id Tech 1 sound playback functionality.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2015 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 1999 Activision
 * @authors Copyright © 1993-1996 id Software, Inc.
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
#include "p_sound.h"

#include <de/log.h>
#include "dmu_lib.h"
#include "g_defs.h"
#ifdef __JHEXEN__
#  include "g_common.h"
#  include "hexlex.h"
#endif

using namespace de;

bool S_MapMusic(const res::Uri &mapUri)
{
    String musicId = G_MapInfoForMapUri(mapUri).gets("music");
    if (musicId.isEmpty())
    {
        musicId = mapUri.path();
    }
    if (S_StartMusic(musicId, true))
    {
        // Set the game status cvar for the map music.
        Con_SetInteger2("map-music", Defs().getMusicNum(musicId), SVF_WRITE_OVERRIDE);
        return true;
    }
    else
    {
        // We don't have anything to play.
        S_StopMusic();
        Con_SetInteger2("map-music", -1, SVF_WRITE_OVERRIDE);
    }
    return false;
}

void S_SectorSound(Sector *sec, int id)
{
    if(!sec) return;

    S_SectorStopSounds(sec);
    S_StartSound(id, (mobj_t *) P_GetPtrp(sec, DMU_EMITTER));
}

void S_SectorStopSounds(Sector *sec)
{
    if(!sec) return;

    // Stop other sounds playing from origins in this sector.
    /// @todo Add a compatibility option allowing emitters to work independently?
    S_StopSound2(0, (mobj_t *) P_GetPtrp(sec, DMU_EMITTER), SSF_ALL_SECTOR);
}

void S_PlaneSound(Plane *pln, int id)
{
    if(!pln) return;

    S_SectorStopSounds((Sector *) P_GetPtrp(pln, DMU_SECTOR));
    S_StartSound(id, (mobj_t *) P_GetPtrp(pln, DMU_EMITTER));
}

#ifdef __JHEXEN__

int S_GetSoundID(const char *name)
{
    return Defs().getSoundNumForName(name);
}

void SndInfoParser(const ddstring_s *path)
{
    AutoStr *script = M_ReadFileIntoString(path, 0);

    if(script && !Str_IsEmpty(script))
    {
        App_Log(DE2_RES_VERBOSE, "Parsing \"%s\"...", F_PrettyPath(Str_Text(path)));

        HexLex lexer(script, path);

        while(lexer.readToken())
        {
            if(!Str_CompareIgnoreCase(lexer.token(), "$archivepath")) // Unused.
            {
                // $archivepath string(local-directory)
                // Used in combination with the -devsnd command line argument to
                // redirect the loading of sounds to a directory in the local file
                // system.
                lexer.readString();
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "$map"))
            {
                // $map int(map-number) string(lump-name)
                // Associate a music lump to a map.
                const int mapNumber        = (int)lexer.readNumber();
                const ddstring_t *lumpName = lexer.readString();

                if(mapNumber > 0)
                {
                    Record &mapInfo = G_MapInfoForMapUri(G_ComposeMapUri(0, mapNumber - 1));
                    if (const Record *music = Defs().musics.tryFind("id", Str_Text(lumpName)))
                    {
                        // There is a music definition with this ID, let's use that.
                        mapInfo.set("music", Str_Text(lumpName));
                    }
                    else
                    {
                        // Modify the map's currently used music to override the music lump.
                        if (Record *music = Defs().musics.tryFind("id", mapInfo.gets("music")))
                        {
                            music->set("lumpName", Str_Text(lumpName));
                        }
                    }
                }
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "$registered")) // Unused.
            {
                continue;
            }
            if(Str_At(lexer.token(), 0) == '$')
            {
                // Found an unknown command.
                Con_Error("SndInfoParser: Unknown command '%s' in \"%s\" on line #%i",
                          lexer.token(), F_PrettyPath(Str_Text(path)), lexer.lineNumber());
            }

            lexer.unreadToken();

            // string(sound-id) string(lump-name | '?')
            // A sound definition.
            const int soundIndex       = Defs().getSoundNumForName(Str_Text(lexer.readString()));
            const ddstring_t *lumpName = lexer.readString();

            if(soundIndex)
            {
                Def_Set(DD_DEF_SOUND, soundIndex,
                        DD_LUMP, Str_At(lumpName, 0) == '?' ? "default" : Str_Text(lumpName));
            }
        }
    }

    // All sounds left without a lumpname will use "DEFAULT".
    char buf[80];
    for(int i = 0; i < Get(DD_NUMSOUNDS); ++i)
    {
        /// @kludge This uses a kludge to traverse the entire sound list.
        /// @todo Implement a mechanism for walking the Def databases.
        Def_Get(DD_DEF_SOUND_LUMPNAME, (char *) &i, buf);
        if(!strcmp(buf, ""))
        {
            Def_Set(DD_DEF_SOUND, i, DD_LUMP, "default");
        }
    }

    if(gameMode == hexen_betademo)
    {
        // The WAD contains two lumps with the name CHAIN, one a sample and the
        // other a graphics lump.
        int soundId = Defs().getSoundNumForName("AMBIENT12");
        Def_Get(DD_DEF_SOUND_LUMPNAME, (char *) &soundId, buf);
        if(!strcasecmp(buf, "chain"))
        {
            Def_Set(DD_DEF_SOUND, soundId, DD_LUMP, "default");
        }
    }
}

#endif // __JHEXEN__
