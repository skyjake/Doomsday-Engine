/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999 Activision
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
 */

/**
 * p_mapinfo.c: MAPINFO lump support.
 */

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <string.h>

#include "jhexen.h"

#include "r_common.h"

// MACROS ------------------------------------------------------------------

#define MAPINFO_SCRIPT_NAME "MAPINFO"

#define UNKNOWN_MAP_NAME "DEVELOPMENT MAP"
#define DEFAULT_SONG_LUMP "DEFSONG"
#define DEFAULT_FADE_TABLE "COLORMAP"

// TYPES -------------------------------------------------------------------

typedef struct mapinfo_s {
    boolean         fromMAPINFO;    ///< The data for this was read from the MAPINFO lump.
    short           cluster;
    uint            warpTrans;
    uint            nextMap;
    short           cdTrack;
    char            name[32];
    materialid_t    sky1Material;
    materialid_t    sky2Material;
    float           sky1ScrollDelta;
    float           sky2ScrollDelta;
    boolean         doubleSky;
    boolean         lightning;
    int             fadetable;
    char            songLump[10];
} mapinfo_t;

enum {
    MCMD_NONE,
    MCMD_SKY1,
    MCMD_SKY2,
    MCMD_LIGHTNING,
    MCMD_FADETABLE,
    MCMD_DOUBLESKY,
    MCMD_CLUSTER,
    MCMD_WARPTRANS,
    MCMD_NEXT,
    MCMD_CDTRACK,
    MCMD_CD_STARTTRACK,
    MCMD_CD_END1TRACK,
    MCMD_CD_END2TRACK,
    MCMD_CD_END3TRACK,
    MCMD_CD_INTERTRACK,
    MCMD_CD_TITLETRACK,
    NUM_MAP_CMDS
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void setSongCDTrack(int index, int track);
static uint qualifyMap(uint map);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static mapinfo_t MapInfo[99];
static uint mapCount;

static char *mapCmdNames[] = {
    "SKY1",
    "SKY2",
    "DOUBLESKY",
    "LIGHTNING",
    "FADETABLE",
    "CLUSTER",
    "WARPTRANS",
    "NEXT",
    "CDTRACK",
    "CD_START_TRACK",
    "CD_END1_TRACK",
    "CD_END2_TRACK",
    "CD_END3_TRACK",
    "CD_INTERMISSION_TRACK",
    "CD_TITLE_TRACK",
    NULL
};
static int mapCmdIDs[] = {
    MCMD_SKY1,
    MCMD_SKY2,
    MCMD_DOUBLESKY,
    MCMD_LIGHTNING,
    MCMD_FADETABLE,
    MCMD_CLUSTER,
    MCMD_WARPTRANS,
    MCMD_NEXT,
    MCMD_CDTRACK,
    MCMD_CD_STARTTRACK,
    MCMD_CD_END1TRACK,
    MCMD_CD_END2TRACK,
    MCMD_CD_END3TRACK,
    MCMD_CD_INTERTRACK,
    MCMD_CD_TITLETRACK
};

static int cdNonMapTracks[6]; // Non-map specific song cd track numbers

static char* cdSongDefIDs[] =  // Music defs that correspond the above.
{
    "startup",
    "hall",
    "orb",
    "chess",
    "hub",
    "hexen"
};

// CODE --------------------------------------------------------------------

/**
 * Initializes the MapInfo database.
 * All MAPINFO lumps are then parsed and stored into the database.
 *
 * Called by P_Init()
 */
void P_InitMapInfo(void)
{
    uint map, mapMax = 0;
    int mcmdValue;
    char songMulch[10];
    mapinfo_t defMapInfo;
    mapinfo_t* info;

    memset(&MapInfo, 0, sizeof(MapInfo));

    // Configure the defaults
    defMapInfo.fromMAPINFO = false; // just default values
    defMapInfo.cluster = 0;
    defMapInfo.warpTrans = 0;
    defMapInfo.nextMap = 0; // Always go to map 0 if not specified.
    defMapInfo.cdTrack = 1;
    defMapInfo.sky1Material = Materials_ResolveUriCString(gameMode == hexen_demo || gameMode == hexen_betademo? "Textures:SKY2" : "Textures:SKY1");
    defMapInfo.sky2Material = defMapInfo.sky1Material;
    defMapInfo.sky1ScrollDelta = 0;
    defMapInfo.sky2ScrollDelta = 0;
    defMapInfo.doubleSky = false;
    defMapInfo.lightning = false;
    defMapInfo.fadetable = W_GetLumpNumForName(DEFAULT_FADE_TABLE);
    strcpy(defMapInfo.name, UNKNOWN_MAP_NAME);

    for(map = 0; map < 99; ++map)
        MapInfo[map].warpTrans = 0;

    SC_Open(MAPINFO_SCRIPT_NAME);
    while(SC_GetString())
    {
        if(SC_Compare("MAP") == false)
        {
            SC_ScriptError(NULL);
        }
        SC_MustGetNumber();
        if(sc_Number < 1 || sc_Number > 99)
            SC_ScriptError(NULL);

        map = (unsigned) sc_Number - 1;
        info = &MapInfo[map];

        // Save song lump name
        strcpy(songMulch, info->songLump);

        // Copy defaults to current map definition
        memcpy(info, &defMapInfo, sizeof(*info));

        // Restore song lump name
        strcpy(info->songLump, songMulch);

        // This information has been parsed from MAPINFO.
        info->fromMAPINFO = true;

        // The warp translation defaults to the map number
        info->warpTrans = map;

        // Map name must follow the number
        SC_MustGetString();
        strcpy(info->name, sc_String);

        // Process optional tokens
        while(SC_GetString())
        {
            if(SC_Compare("MAP"))
            {   // Start next map definition.
                SC_UnGet();
                break;
            }

            mcmdValue = mapCmdIDs[SC_MustMatchString(mapCmdNames)];
            switch(mcmdValue)
            {
            case MCMD_CLUSTER:
                SC_MustGetNumber();
                if(sc_Number < 1)
                {
                    char buf[40];
                    dd_snprintf(buf, 40, "Invalid cluster %i", sc_Number);
                    SC_ScriptError(buf);
                }
                info->cluster = sc_Number;
                break;

            case MCMD_WARPTRANS:
                SC_MustGetNumber();
                if(sc_Number < 1 || sc_Number > 99)
                    SC_ScriptError(NULL);
                info->warpTrans = (unsigned) sc_Number - 1;
                break;

            case MCMD_NEXT:
                SC_MustGetNumber();
                if(sc_Number < 1 || sc_Number > 99)
                    SC_ScriptError(NULL);
                info->nextMap = (unsigned) sc_Number - 1;
                break;

            case MCMD_CDTRACK:
                SC_MustGetNumber();
                info->cdTrack = sc_Number;
                break;

            case MCMD_SKY1: {
                ddstring_t path;
                Uri* uri;

                SC_MustGetString();
                Str_Init(&path);
                Str_PercentEncode(Str_Set(&path, sc_String));

                uri = Uri_NewWithPath2("Textures:", RC_NULL);
                Uri_SetPath(uri, Str_Text(&path));
                info->sky1Material = Materials_ResolveUri(uri);
                Uri_Delete(uri);
                Str_Free(&path);

                SC_MustGetNumber();
                info->sky1ScrollDelta = (float) sc_Number / 256;
                break;
              }
            case MCMD_SKY2: {
                ddstring_t path;
                Uri* uri;

                SC_MustGetString();
                Str_Init(&path);
                Str_PercentEncode(Str_Set(&path, sc_String));

                uri = Uri_NewWithPath2("Textures:", RC_NULL);
                Uri_SetPath(uri, Str_Text(&path));
                info->sky2Material = Materials_ResolveUri(uri);
                Uri_Delete(uri);
                Str_Free(&path);

                SC_MustGetNumber();
                info->sky2ScrollDelta = (float) sc_Number / 256;
                break;
              }
            case MCMD_DOUBLESKY:
                info->doubleSky = true;
                break;

            case MCMD_LIGHTNING:
                info->lightning = true;
                break;

            case MCMD_FADETABLE:
                SC_MustGetString();
                info->fadetable = W_GetLumpNumForName(sc_String);
                break;

            case MCMD_CD_STARTTRACK:
            case MCMD_CD_END1TRACK:
            case MCMD_CD_END2TRACK:
            case MCMD_CD_END3TRACK:
            case MCMD_CD_INTERTRACK:
            case MCMD_CD_TITLETRACK:
                SC_MustGetNumber();
                setSongCDTrack(mcmdValue - MCMD_CD_STARTTRACK, sc_Number);
                break;

            default:
                break;
            }
        }

#ifdef _DEBUG
        Con_Message("MAPINFO: map%i \"%s\" warp:%i\n", map, info->name, info->warpTrans);
#endif

        mapMax = map > mapMax ? map : mapMax;
    }

    SC_Close();
    mapCount = mapMax+1;
}

/**
 * Special early initializer needed to start sound before R_InitRefresh()
 */
void P_InitMapMusicInfo(void)
{
    uint i;

    for(i = 0; i < 99; ++i)
    {
        strcpy(MapInfo[i].songLump, DEFAULT_SONG_LUMP);
    }

    mapCount = 98;
}

static void setSongCDTrack(int index, int track)
{
    int         cdTrack = track;

#ifdef _DEBUG
    Con_Message("setSongCDTrack: index=%i, track=%i\n", index, track);
#endif

    // Set the internal array.
    cdNonMapTracks[index] = cdTrack;

    // Update the corresponding Doomsday definition.
    Def_Set(DD_DEF_MUSIC, Def_Get(DD_DEF_MUSIC, cdSongDefIDs[index], 0),
            DD_CD_TRACK, &cdTrack);
}

static __inline uint qualifyMap(uint map)
{
    return (map >= mapCount) ? 0 : map;
}

uint P_TranslateMapIfExists(uint map)
{
    uint i;
    uint matchedWithoutCluster = P_INVALID_LOGICAL_MAP;

    for(i = 0; i < 99; ++i)
    {
        const mapinfo_t* info = &MapInfo[i];
        if(!info->fromMAPINFO) continue; // Ignoring, undefined values.
        if(info->warpTrans == map)
        {
            if(info->cluster)
            {
#ifdef _DEBUG
                Con_Message("P_TranslateMapIfExists: warp %i translated to logical %i, cluster %i\n", map, i, info->cluster);
#endif
                return i;
            }
            else
            {
#ifdef _DEBUG
                Con_Message("P_TranslateMapIfExists: warp %i matches logical %i, but it has no cluster\n", map, i);
#endif
                matchedWithoutCluster = i;
            }
        }
    }

#ifdef _DEBUG
    Con_Message("P_TranslateMapIfExists: could not find warp %i, translating to logical %i\n",
                map, matchedWithoutCluster);
#endif
    return matchedWithoutCluster;
}

uint P_TranslateMap(uint map)
{
    uint translated = P_TranslateMapIfExists(map);
    if(translated == P_INVALID_LOGICAL_MAP)
    {
        // This function always returns a valid logical map.
        return 0;
    }
    return translated;
}

/**
 * Set the song lump name of a map.
 *
 * NOTE: Cannot be used to alter the default map #0.
 *
 * @param map           The map (logical number) to be changed.
 * @param lumpName      The lumpName to be set.
 */
void P_PutMapSongLump(uint map, char *lumpName)
{
    if(map >= mapCount)
        return;

    strncpy(MapInfo[map].songLump, lumpName, sizeof(MapInfo[map].songLump));
}

/**
 * Retrieve the name of the given map.
 *
 * @param map           The map (logical number) to be queried.
 *
 * @return              Ptr to string containing the name of the map.
 */
char *P_GetMapName(uint map)
{
    return MapInfo[qualifyMap(map)].name;
}

/**
 * Retrieve the cluster number of the given map.
 *
 * @param map           The map (logical number) to be queried.
 *
 * @return              The cluster number of the map.
 */
int P_GetMapCluster(uint map)
{
    return (int) MapInfo[qualifyMap(map)].cluster;
}

/**
 * Retrieve the CD track number of the given map.
 *
 * @param map           The map (logical number) to be queried.
 *
 * @return              The CD track number for the map.
 */
int P_GetMapCDTrack(uint map)
{
    return (int) MapInfo[qualifyMap(map)].cdTrack;
}

/**
 * Retrieve the map warp number of the given map.
 *
 * @param map           The map (logical number) to be queried.
 *
 * @return              The warp number of the map.
 */
uint P_GetMapWarpTrans(uint map)
{
    return MapInfo[qualifyMap(map)].warpTrans;
}

/**
 * Retrieve the next map number of the given map.
 *
 * @param map           The map (logical number) to be queried.
 *
 * @return              The next map number of the map.
 */
uint P_GetMapNextMap(uint map)
{
    return MapInfo[qualifyMap(map)].nextMap;
}

/**
 * Retrieve the sky1 material of the given map.
 *
 * @param map           The map (logical number) to be queried.
 *
 * @return              The sky1 material num of the map.
 */
materialid_t P_GetMapSky1Material(uint map)
{
    return MapInfo[qualifyMap(map)].sky1Material;
}

/**
 * Retrieve the sky2 material of the given map.
 *
 * @param map           The map (logical number) to be queried.
 *
 * @return              The sky2 material num of the map.
 */
materialid_t P_GetMapSky2Material(uint map)
{
    return MapInfo[qualifyMap(map)].sky2Material;
}

/**
 * Retrieve the sky1 scroll delta of the given map.
 *
 * @param map           The map (logical number) to be queried.
 *
 * @return              The sky1 scroll delta of the map.
 */
float P_GetMapSky1ScrollDelta(uint map)
{
    return MapInfo[qualifyMap(map)].sky1ScrollDelta;
}

/**
 * Retrieve the sky2 scroll delta of the given map.
 *
 * @param map           The map (logical number) to be queried.
 *
 * @return              The sky2 scroll delta of the map.
 */
float P_GetMapSky2ScrollDelta(uint map)
{
    return MapInfo[qualifyMap(map)].sky2ScrollDelta;
}

/**
 * Retrieve the value of the double sky property of the given map.
 *
 * @param map           The map (logical number) to be queried.
 *
 * @return              @c true, if the map is set to doublesky.
 */
boolean P_GetMapDoubleSky(uint map)
{
    return MapInfo[qualifyMap(map)].doubleSky;
}

/**
 * Retrieve the value of the lighting property of the given map.
 *
 * @param map           The map (logical number) to be queried.
 *
 * @return              @c true, if the map is set to lightning.
 */
boolean P_GetMapLightning(uint map)
{
    return MapInfo[qualifyMap(map)].lightning;
}

/**
 * Retrieve the fadetable lump id of the given map.
 *
 * @param map           The map (logical number) to be queried.
 *
 * @return              The lump id used for the fadetable for the map.
 */
int P_GetMapFadeTable(uint map)
{
    return MapInfo[qualifyMap(map)].fadetable;
}

/**
 * Retrieve the song lump name for the given map.
 *
 * @param map           The map (logical number) to be queried.
 *
 * @return              @c NULL, if the map is set to use the
 *                      default song lump, else a ptr to a string
 *                      containing the name of the song lump.
 */
char *P_GetMapSongLump(uint map)
{
    if(!strcasecmp(MapInfo[qualifyMap(map)].songLump, DEFAULT_SONG_LUMP))
    {
        return NULL;
    }
    else
    {
        return MapInfo[qualifyMap(map)].songLump;
    }
}

/**
 * Retrieve the CD start track number of the given map.
 *
 * @param map           The map (logical number) to be queried.
 *
 * @return              The CD start track number of the map.
 */
int P_GetCDStartTrack(void)
{
    return cdNonMapTracks[MCMD_CD_STARTTRACK - MCMD_CD_STARTTRACK];
}

/**
 * Retrieve the CD end1 track number of the given map.
 *
 * @param map           The map (logical number) to be queried.
 *
 * @return              The CD end1 track number of the map.
 */
int P_GetCDEnd1Track(void)
{
    return cdNonMapTracks[MCMD_CD_END1TRACK - MCMD_CD_STARTTRACK];
}

/**
 * Retrieve the CD end2 track number of the given map.
 *
 * @param map           The map (logical number) to be queried.
 *
 * @return              The CD end2 track number of the map.
 */
int P_GetCDEnd2Track(void)
{
    return cdNonMapTracks[MCMD_CD_END2TRACK - MCMD_CD_STARTTRACK];
}

/**
 * Retrieve the CD end3 track number of the given map.
 *
 * @param map           The map (logical number) to be queried.
 *
 * @return              The CD end3 track number of the map.
 */
int P_GetCDEnd3Track(void)
{
    return cdNonMapTracks[MCMD_CD_END3TRACK - MCMD_CD_STARTTRACK];
}

/**
 * Retrieve the CD intermission track number of the given map.
 *
 * @param map           The map (logical number) to be queried.
 *
 * @return              The CD intermission number of the map.
 */
int P_GetCDIntermissionTrack(void)
{
    return cdNonMapTracks[MCMD_CD_INTERTRACK - MCMD_CD_STARTTRACK];
}

/**
 * Retrieve the CD title track number of the given map.
 *
 * @param map           The map (logical number) to be queried.
 *
 * @return              The CD title track number of the map.
 */
int P_GetCDTitleTrack(void)
{
    return cdNonMapTracks[MCMD_CD_TITLETRACK - MCMD_CD_STARTTRACK];
}
