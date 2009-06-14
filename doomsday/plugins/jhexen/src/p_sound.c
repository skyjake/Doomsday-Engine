/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
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
 * Boston, MA  02110-1301 USA
 */

/**
 * s_sound.c:
 *
 * \bug Not 64bit clean: In function 'S_ParseSndInfoLump': cast to pointer from integer of different size
 */

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "jhexen.h"

// MACROS ------------------------------------------------------------------

#define DEFAULT_ARCHIVEPATH     "o:\\sound\\archive\\"

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static char ArchivePath[128];

// CODE --------------------------------------------------------------------

int S_GetSoundID(const char* name)
{
    return Def_Get(DD_DEF_SOUND_BY_NAME, name, 0);
}

/**
 * Starts the song of the current map..
 */
void S_MapMusic(void)
{
    int                 idx = Def_Get(DD_DEF_MUSIC, "currentmap", 0);
    int                 cdTrack;

    // Update the 'currentmap' music definition.
    Def_Set(DD_DEF_MUSIC, idx, DD_LUMP, P_GetMapSongLump(gameMap));
    cdTrack = P_GetMapCDTrack(gameMap);
    Def_Set(DD_DEF_MUSIC, idx, DD_CD_TRACK, &cdTrack);
    if(S_StartMusic("currentmap", true))
    {
        // Set the game status cvar for the map music
        gsvMapMusic = idx;
    }
}

void S_ParseSndInfoLump(void)
{
    int                 i;
    char                buf[80];
    lumpnum_t           lump = W_CheckNumForName("SNDINFO");

    strcpy(ArchivePath, DEFAULT_ARCHIVEPATH);

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
                    strcpy(ArchivePath, sc_String);
                }
                else if(!stricmp(sc_String, "$MAP"))
                {
                    SC_MustGetNumber();
                    SC_MustGetString();
                    if(sc_Number)
                    {
                        P_PutMapSongLump(sc_Number, sc_String);
                    }
                }
                continue;
            }
            else
            {
                i = Def_Get(DD_DEF_SOUND_BY_NAME, sc_String, 0);
                if(i)
                {
                    SC_MustGetString();
                    Def_Set(DD_DEF_SOUND, i, DD_LUMP,
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
    for(i = 0; i < Get(DD_NUMSOUNDS); ++i)
    {
        //// \kludge DJS - This uses a kludge to traverse the entire sound list.
        //// \fixme Implement a mechanism for walking the Def databases.
        Def_Get(DD_DEF_SOUND_LUMPNAME, (char *) &i, buf);
        if(!strcmp(buf, ""))
            Def_Set(DD_DEF_SOUND, i, DD_LUMP, "default");
    }
}
