/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
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

#include "jhexen.h"

#include "r_common.h"

// MACROS ------------------------------------------------------------------

#define MAPINFO_SCRIPT_NAME "MAPINFO"

#define UNKNOWN_MAP_NAME "UNKNOWN MAP"
#define DEFAULT_SKY_NAME "SKY1"
#define DEFAULT_SONG_LUMP "DEFSONG"
#define DEFAULT_FADE_TABLE "COLORMAP"

// TYPES -------------------------------------------------------------------

typedef struct mapinfo_s {
    short           cluster;
    short           warpTrans;
    short           nextMap;
    short           cdTrack;
    char            name[32];
    materialnum_t   sky1Material;
    materialnum_t   sky2Material;
    float           sky1ScrollDelta;
    float           sky2ScrollDelta;
    boolean         doubleSky;
    boolean         lightning;
    int             fadetable;
    char            songLump[9];
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
static int qualifyMap(int map);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static mapinfo_t MapInfo[99];
static int mapCount;

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
 * Initializes the MapInfo database. Default settings are stored in map 0.
 * All MAPINFO lumps are then parsed and stored into the database.
 *
 * Called by P_Init()
 */
void P_InitMapInfo(void)
{
    int         map, mapMax = 1, mcmdValue;
    char        songMulch[10];
    mapinfo_t  *info;

    // Put defaults into MapInfo[0]
    info = MapInfo;
    info->cluster = 0;
    info->warpTrans = 0;
    info->nextMap = 1;          // Always go to map 1 if not specified
    info->cdTrack = 1;
    info->sky1Material =
        P_MaterialNumForName(shareware ? "SKY2" : DEFAULT_SKY_NAME, MN_TEXTURES);
    info->sky2Material = info->sky1Material;
    info->sky1ScrollDelta = 0;
    info->sky2ScrollDelta = 0;
    info->doubleSky = false;
    info->lightning = false;
    info->fadetable = W_GetNumForName(DEFAULT_FADE_TABLE);
    strcpy(info->name, UNKNOWN_MAP_NAME);

    for(map = 0; map < 99; ++map)
        MapInfo[map].warpTrans = 0;

    if(!useScriptsDir)
    {
        SC_OpenLump(W_GetNumForName(MAPINFO_SCRIPT_NAME));
    }
    else
    {
        filename_t          path;

        snprintf(path, FILENAME_T_MAXLEN, "%s%s.txt", scriptsDir, MAPINFO_SCRIPT_NAME);
        SC_OpenFile(path);
    }

    while(SC_GetString())
    {
        if(SC_Compare("MAP") == false)
        {
            SC_ScriptError(NULL);
        }
        SC_MustGetNumber();
        if(SC_LastReadInteger() < 1 || SC_LastReadInteger() > 99)
        {
            SC_ScriptError(NULL);
        }

        map = SC_LastReadInteger();
        info = &MapInfo[map];

        // Save song lump name
        strcpy(songMulch, info->songLump);

        // Copy defaults to current map definition
        memcpy(info, &MapInfo[0], sizeof(*info));

        // Restore song lump name
        strcpy(info->songLump, songMulch);

        // The warp translation defaults to the map number
        info->warpTrans = map;

        // Map name must follow the number
        SC_MustGetString();
        strcpy(info->name, SC_LastReadString());

        // Process optional tokens
        while(SC_GetString())
        {
            if(SC_Compare("MAP"))
            {                   // Start next map definition
                SC_UnGet();
                break;
            }

            mcmdValue = mapCmdIDs[SC_MustMatchString(mapCmdNames)];
            switch(mcmdValue)
            {
            case MCMD_CLUSTER:
                SC_MustGetNumber();
                info->cluster = SC_LastReadInteger();
                break;

            case MCMD_WARPTRANS:
                SC_MustGetNumber();
                info->warpTrans = SC_LastReadInteger();
                break;

            case MCMD_NEXT:
                SC_MustGetNumber();
                info->nextMap = SC_LastReadInteger();
                break;

            case MCMD_CDTRACK:
                SC_MustGetNumber();
                info->cdTrack = SC_LastReadInteger();
                break;

            case MCMD_SKY1:
                SC_MustGetString();
                info->sky1Material =
                    P_MaterialNumForName(SC_LastReadString(), MN_TEXTURES);
                SC_MustGetNumber();
                info->sky1ScrollDelta = (float) SC_LastReadInteger() / 256;
                break;

            case MCMD_SKY2:
                SC_MustGetString();
                info->sky2Material =
                    P_MaterialNumForName(SC_LastReadString(), MN_TEXTURES);
                SC_MustGetNumber();
                info->sky2ScrollDelta = (float) SC_LastReadInteger() / 256;
                break;

            case MCMD_DOUBLESKY:
                info->doubleSky = true;
                break;

            case MCMD_LIGHTNING:
                info->lightning = true;
                break;

            case MCMD_FADETABLE:
                SC_MustGetString();
                info->fadetable = W_GetNumForName(SC_LastReadString());
                break;

            case MCMD_CD_STARTTRACK:
            case MCMD_CD_END1TRACK:
            case MCMD_CD_END2TRACK:
            case MCMD_CD_END3TRACK:
            case MCMD_CD_INTERTRACK:
            case MCMD_CD_TITLETRACK:
                SC_MustGetNumber();
                setSongCDTrack(mcmdValue - MCMD_CD_STARTTRACK, SC_LastReadInteger());
                break;

            default:
                break;
            }
        }
        mapMax = map > mapMax ? map : mapMax;
    }

    SC_Close();
    mapCount = mapMax;
}

/**
 * Special early initializer needed to start sound before R_InitRefresh()
 */
void P_InitMapMusicInfo(void)
{
    int         i;

    for(i = 0; i < 99; ++i)
    {
        strcpy(MapInfo[i].songLump, DEFAULT_SONG_LUMP);
    }

    mapCount = 98;
}

static void setSongCDTrack(int index, int track)
{
    int         cdTrack = track;

    // Set the internal array.
    cdNonMapTracks[index] = SC_LastReadInteger();

    // Update the corresponding Doomsday definition.
    Def_Set(DD_DEF_MUSIC, Def_Get(DD_DEF_MUSIC, cdSongDefIDs[index], 0),
            DD_CD_TRACK, &cdTrack);
}

static __inline int qualifyMap(int map)
{
    return (map < 1 || map > mapCount) ? 0 : map;
}

/**
 * Translates a warp map number to logical map number.
 *
 * @param map           The warp map number to translate.
 *
 * @return              The logical map number given a warp map number.
 */
int P_TranslateMap(int map)
{
    int         i;

    for(i = 1; i < 99; ++i)
    {
        if((int) MapInfo[i].warpTrans == map)
            return i;
    }

    return -1; // Not found
}

/**
 * Set the song lump name of a map.
 *
 * NOTE: Cannot be used to alter the default map #0.
 *
 * @param map           The map (logical number) to be changed.
 * @param lumpName      The lumpName to be set.
 */
void P_PutMapSongLump(int map, const char* lumpName)
{
    if(map < 1 || map > mapCount)
        return;

    if(lumpName && lumpName[0])
    {
    strncpy(MapInfo[map].songLump, lumpName, sizeof(MapInfo[map].songLump));
    }
}

/**
 * Retrieve the name of the given map.
 *
 * @param map           The map (logical number) to be queried.
 *
 * @return              Ptr to string containing the name of the map.
 */
char *P_GetMapName(int map)
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
int P_GetMapCluster(int map)
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
int P_GetMapCDTrack(int map)
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
int P_GetMapWarpTrans(int map)
{
    return (int) MapInfo[qualifyMap(map)].warpTrans;
}

/**
 * Retrieve the next map number of the given map.
 *
 * @param map           The map (logical number) to be queried.
 *
 * @return              The next map number of the map.
 */
int P_GetMapNextMap(int map)
{
    return (int) MapInfo[qualifyMap(map)].nextMap;
}

/**
 * Retrieve the sky1 material num of the given map.
 *
 * @param map           The map (logical number) to be queried.
 *
 * @return              The sky1 material num of the map.
 */
materialnum_t P_GetMapSky1Material(int map)
{
    return MapInfo[qualifyMap(map)].sky1Material;
}

/**
 * Retrieve the sky2 material num of the given map.
 *
 * @param map           The map (logical number) to be queried.
 *
 * @return              The sky2 material num of the map.
 */
materialnum_t P_GetMapSky2Material(int map)
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
float P_GetMapSky1ScrollDelta(int map)
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
float P_GetMapSky2ScrollDelta(int map)
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
boolean P_GetMapDoubleSky(int map)
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
boolean P_GetMapLightning(int map)
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
int P_GetMapFadeTable(int map)
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
char *P_GetMapSongLump(int map)
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
