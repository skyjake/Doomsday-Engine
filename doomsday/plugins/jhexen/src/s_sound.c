/**\file
 *\section Copyright and License Summary
 * License: GPL + jHeretic/jHexen Exception
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2005-2006 Daniel Swanson <danij@dengine.net>
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

void    S_StopSoundID(int sound_id);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int gsvMapMusic;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static char ArchivePath[128];

// CODE --------------------------------------------------------------------

int S_GetSoundID(char *name)
{
    return Def_Get(DD_DEF_SOUND_BY_NAME, name, 0);
}

/**
 * Starts the song of the current level.
 */
void S_LevelMusic(void)
{
    int         idx = Def_Get(DD_DEF_MUSIC, "currentmap", 0);
    int         cdTrack;

    // Update the 'currentmap' music definition.
    Def_Set(DD_DEF_MUSIC, idx, DD_LUMP, P_GetMapSongLump(gamemap));
    cdTrack = P_GetMapCDTrack(gamemap);
    Def_Set(DD_DEF_MUSIC, idx, DD_CD_TRACK, &cdTrack);
    S_StartMusic("currentmap", true);

    // set the game status cvar for the map music
    gsvMapMusic = idx;
}

void S_InitScript(void)
{
    int         i;
    char        buf[80];

    strcpy(ArchivePath, DEFAULT_ARCHIVEPATH);
    SC_OpenLump("SNDINFO");
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

    // All sounds left without a lumpname will use "DEFAULT".
    for(i = 0; i < Get(DD_NUMSOUNDS); ++i)
    {
        // DJS - This uses a kludge to traverse the entire sound list.
        // FIXME: Implement a mechanism for walking the Def databases.
        Def_Get(DD_DEF_SOUND_LUMPNAME, (char *) i, buf);
        if(!strcmp(buf, ""))
            Def_Set(DD_DEF_SOUND, i, DD_LUMP, "default");
    }
}
