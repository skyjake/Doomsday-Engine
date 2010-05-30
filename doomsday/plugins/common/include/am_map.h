/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2005-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * Boston, MA  02110-1301  USA
 */

/**
 * am_map.h : Automap, automap menu and related code.
 */

#ifndef __COMMON_AUTOMAP_MANAGER__
#define __COMMON_AUTOMAP_MANAGER__

#define AUTOMAP_OPEN_SECONDS    (.3f) // Num of seconds to open/close the map.

#if __JDOOM__ || __JDOOM64__
// For use if I do walls with outsides/insides
#define BLUES       (256-4*16+8)
#define YELLOWRANGE 1
#define BLACK       0
#define REDS        (256-5*16)
#define REDRANGE    16
#define BLUERANGE   8
#define GREENS      (7*16)
#define GREENRANGE  16
#define GRAYS       (6*16)
#define GRAYSRANGE  16
#define BROWNS      (4*16)
#define BROWNRANGE  16
#define YELLOWS     (256-32+7)
#define WHITE       (256-47)
#define YOURCOLORS      WHITE
#define YOURRANGE       0

#define WALLCOLORS      REDS
#define WALLRANGE       REDRANGE
#define TSWALLCOLORS    GRAYS
#define TSWALLRANGE     GRAYSRANGE
#define CDWALLCOLORS    YELLOWS
#define CDWALLRANGE     YELLOWRANGE
#define THINGCOLORS     GREENS
#define THINGRANGE      GREENRANGE

#define SECRETWALLCOLORS WALLCOLORS
#define SECRETWALLRANGE WALLRANGE
#define GRIDCOLORS      (GRAYS + GRAYSRANGE/2)
#define GRIDRANGE       0
#define XHAIRCOLORS     GRAYS
#define FDWALLCOLORS        BROWNS
#define FDWALLRANGE     BROWNRANGE

#define BORDEROFFSET 3

// Automap colors
#define BACKGROUND      BLACK

// Keys for Baby Mode
#define KEY1_COLOR   197      //  Blue Key
#define KEY2_COLOR   (256-5*16)   //  Red Key
#define KEY3_COLOR   (256-32+7)   //  Yellow Key
#define KEY4_COLOR   (256-32+7)   //  Yellow Skull
#define KEY5_COLOR   (256-5*16)   //  Red Skull
#define KEY6_COLOR   197      //  Blue Skull

#define BORDERGRAPHIC "brdr_b"

#define MARKERPATCHES (sprintf(namebuf, "AMMNUM%d", i))     // DJS - Patches used for marking the automap, a bit of a hack I suppose

#endif


#ifdef __JHERETIC__

// For use if I do walls with outsides/insides
#define REDS        12*8
#define REDRANGE    1              //16
#define BLUES       (256-4*16+8)
#define BLUERANGE   1              //8
#define GREENS      (33*8)
#define GREENRANGE  1              //16
#define GRAYS       (5*8)
#define GRAYSRANGE  1              //16
#define BROWNS      (14*8)
#define BROWNRANGE  1              //16
#define YELLOWS     10*8
#define YELLOWRANGE 1
#define BLACK       0
#define WHITE       4*8
#define PARCH       13*8-1
#define WALLCOLORS      REDS
#define FDWALLCOLORS        BROWNS
#define CDWALLCOLORS        YELLOWS
#define YOURCOLORS      WHITE
#define YOURRANGE       0
#define WALLRANGE       REDRANGE
#define TSWALLCOLORS        GRAYS
#define TSWALLRANGE     GRAYSRANGE
#define FDWALLRANGE     BROWNRANGE
#define CDWALLRANGE     YELLOWRANGE
#define THINGCOLORS     GREENS
#define THINGRANGE      GREENRANGE
#define SECRETWALLCOLORS WALLCOLORS
#define SECRETWALLRANGE WALLRANGE
#define GRIDCOLORS      (GRAYS + GRAYSRANGE/2)
#define GRIDRANGE       0
#define XHAIRCOLORS     GRAYS

#define BLOODRED  150

// Automap colors
#define BACKGROUND  PARCH
#define YOURCOLORS  WHITE
#define YOURRANGE   0

#define WALLRANGE   REDRANGE
#define TSWALLCOLORS    GRAYS
#define TSWALLRANGE GRAYSRANGE

#define FDWALLRANGE BROWNRANGE

#define CDWALLRANGE YELLOWRANGE
#define THINGCOLORS GREENS
#define THINGRANGE  GREENRANGE
#define SECRETWALLCOLORS WALLCOLORS
#define SECRETWALLRANGE WALLRANGE
#define GRIDCOLORS  (GRAYS + GRAYSRANGE/2)
#define GRIDRANGE   0
#define XHAIRCOLORS GRAYS

#define BORDEROFFSET 4

// Keys for Baby Mode
#define KEY1_COLOR   144  // HERETIC - Green Key
#define KEY2_COLOR   197  // HERETIC - Yellow Key
#define KEY3_COLOR   220  // HERETIC - Blue Key

#define BORDERGRAPHIC "bordb"

#define MARKERPATCHES (sprintf(namebuf, "FONTA%d", (16 +i) ))       // DJS - Patches used for marking the automap, a bit of a hack I suppose

#endif


#ifdef __JHEXEN__
// For use if I do walls with outsides/insides
#define REDS        12*8
#define REDRANGE    1              //16
#define BLUES       (256-4*16+8)
#define BLUERANGE   1              //8
#define GREENS      (33*8)
#define GREENRANGE  1              //16
#define GRAYS       (5*8)
#define GRAYSRANGE  1              //16
#define BROWNS      (14*8)
#define BROWNRANGE  1              //16
#define YELLOWS     10*8
#define YELLOWRANGE 1
#define BLACK       0
#define WHITE       4*8
#define PARCH       13*8-1
#define BLOODRED    177

// Automap colors
#define BACKGROUND  PARCH
#define YOURCOLORS  WHITE
#define YOURRANGE   0
#define WALLCOLORS  83      // REDS
#define WALLRANGE   REDRANGE
#define TSWALLCOLORS    GRAYS
#define TSWALLRANGE GRAYSRANGE
#define FDWALLCOLORS    96      // BROWNS
#define FDWALLRANGE BROWNRANGE
#define CDWALLCOLORS    107     // YELLOWS
#define CDWALLRANGE YELLOWRANGE
#define THINGCOLORS GREENS
#define THINGRANGE  GREENRANGE
#define SECRETWALLCOLORS WALLCOLORS
#define SECRETWALLRANGE WALLRANGE
#define GRIDCOLORS  (GRAYS + GRAYSRANGE/2)
#define GRIDRANGE   0
#define XHAIRCOLORS GRAYS

#define BORDEROFFSET 4

// Automap colors
#define AM_PLR1_COLOR 157       // Blue
#define AM_PLR2_COLOR 177       // Red
#define AM_PLR3_COLOR 137       // Yellow
#define AM_PLR4_COLOR 198       // Green
#define AM_PLR5_COLOR 215       // Jade
#define AM_PLR6_COLOR 32        // White
#define AM_PLR7_COLOR 106       // Hazel
#define AM_PLR8_COLOR 234       // Purple

#define KEY1   197  // HEXEN -
#define KEY2   144  // HEXEN -
#define KEY3   220  // HEXEN -

#define BORDERGRAPHIC "bordb"

#define MARKERPATCHES (sprintf(namebuf, "FONTA%d", (16 +i) ))       // DJS - Patches used for marking the automap, a bit of a hack I suppose

#endif

#ifdef __JSTRIFE__
// For use if I do walls with outsides/insides
#define REDS        12*8
#define REDRANGE    1              //16
#define BLUES       (256-4*16+8)
#define BLUERANGE   1              //8
#define GREENS      (33*8)
#define GREENRANGE  1              //16
#define GRAYS       (5*8)
#define GRAYSRANGE  1              //16
#define BROWNS      (14*8)
#define BROWNRANGE  1              //16
#define YELLOWS     10*8
#define YELLOWRANGE 1
#define BLACK       0
#define WHITE       4*8
#define PARCH       13*8-1
#define BLOODRED    177

// Automap colors
#define BACKGROUND  PARCH
#define YOURCOLORS  WHITE
#define YOURRANGE   0
#define WALLCOLORS  83      // REDS
#define WALLRANGE   REDRANGE
#define TSWALLCOLORS    GRAYS
#define TSWALLRANGE GRAYSRANGE
#define FDWALLCOLORS    96      // BROWNS
#define FDWALLRANGE BROWNRANGE
#define CDWALLCOLORS    107     // YELLOWS
#define CDWALLRANGE YELLOWRANGE
#define THINGCOLORS GREENS
#define THINGRANGE  GREENRANGE
#define SECRETWALLCOLORS WALLCOLORS
#define SECRETWALLRANGE WALLRANGE
#define GRIDCOLORS  (GRAYS + GRAYSRANGE/2)
#define GRIDRANGE   0
#define XHAIRCOLORS GRAYS

#define BORDEROFFSET 4

// Automap colors
#define KEY1   197  // HEXEN -
#define KEY2   144  // HEXEN -
#define KEY3   220  // HEXEN -

#define BORDERGRAPHIC "bordb"

#define MARKERPATCHES (sprintf(namebuf, "FONTA%d", (16 +i) ))       // DJS - Patches used for marking the automap, a bit of a hack I suppose

#endif

typedef unsigned int automapid_t;

typedef enum {
    AMO_NONE = -1,
    AMO_THING = 0,
    AMO_THINGPLAYER,
    AMO_BACKGROUND,
    AMO_UNSEENLINE,
    AMO_SINGLESIDEDLINE,
    AMO_TWOSIDEDLINE,
    AMO_FLOORCHANGELINE,
    AMO_CEILINGCHANGELINE,
    AMO_NUMOBJECTS
} automapobjectname_t;

typedef struct mapobjectinfo_s {
    float           rgba[4];
    int             blendMode;
    float           glowAlpha, glowWidth;
    boolean         glow;
    boolean         scaleWithView;
} mapobjectinfo_t;

enum {
    MOL_LINEDEF = 0,
    MOL_LINEDEF_TWOSIDED,
    MOL_LINEDEF_FLOOR,
    MOL_LINEDEF_CEILING,
    MOL_LINEDEF_UNSEEN,
    NUM_MAP_OBJECTLISTS
};

#define AM_MAXSPECIALLINES      32

typedef struct automapspecialline_s {
    int             special;
    int             sided;
    int             cheatLevel; // minimum cheat level for this special.
    mapobjectinfo_t info;
} automapspecialline_t;

typedef struct automapcfg_s {
    float           lineGlowScale;
    boolean         glowingLineSpecials;
    float           backgroundRGBA[4];
    float           panSpeed;
    boolean         panResetOnOpen;
    float           zoomSpeed;
    float           openSeconds; // Num seconds it takes for the map to open/close.
    automapspecialline_t specialLines[AM_MAXSPECIALLINES];
    uint            numSpecialLines;
    vectorgraphicid_t vectorGraphicForPlayer;
    vectorgraphicid_t vectorGraphicForThing;
    int             cheating;
    boolean         revealed;
    uint            followPlayer; // Player id of that to follow.

    mapobjectinfo_t mapObjectInfo[NUM_MAP_OBJECTLISTS];
} automapcfg_t;

typedef enum glowtype_e {
    NO_GLOW,
    TWOSIDED_GLOW,
    BACK_GLOW,
    FRONT_GLOW
} glowtype_t;

void            AM_Register(void);
void            AM_Init(void);
void            AM_Shutdown(void);

void            AM_InitForMap(void);
void            AM_Ticker(timespan_t ticLength);
void            AM_Drawer(int player);

automapid_t     AM_MapForPlayer(int plrnum);

void            AM_Open(automapid_t id, boolean yes, boolean fast);
boolean         AM_IsActive(automapid_t id);

void            AM_UpdateLinedef(automapid_t id, uint lineIdx, boolean visible);
int             AM_AddMark(automapid_t id, float x, float y, float z);
void            AM_ClearMarks(automapid_t id);

void            AM_IncMapCheatLevel(automapid_t id);
void            AM_SetCheatLevel(automapid_t id, int level);
void            AM_SetViewRotate(automapid_t id, int offOnToggle);

void            AM_RevealMap(automapid_t id, boolean on);
void            AM_ToggleZoomMax(automapid_t id);
void            AM_ToggleFollow(automapid_t id);

float           AM_GlobalAlpha(automapid_t id);
void            AM_GetWindow(automapid_t id, float* x, float* y, float* w, float* h);
boolean         AM_IsMapWindowInFullScreenMode(automapid_t id);
int             AM_GetFlags(automapid_t id);
boolean         AM_IsRevealed(automapid_t id);
float           AM_MapToFrame(automapid_t id, float val);
void            AM_GetViewPosition(automapid_t id, float* x, float* y);
void            M_DrawMAP(void);

const automapcfg_t* AM_GetMapConfig(automapid_t id);
const mapobjectinfo_t* AM_GetMapObjectInfo(automapid_t id, int objectname);
const mapobjectinfo_t* AM_GetInfoForSpecialLine(automapid_t id, int special,
                                                const sector_t* frontsector,
                                                const sector_t* backsector);
void AM_GetColorAndAlpha(automapid_t id, int objectname, float* r, float* g,
                         float* b, float* a);
void AM_SetColorAndAlpha(automapid_t id, int objectname, float r, float g,
                         float b, float a);

void AM_GetMapColor(float* rgb, const float* uColor, int palidx,
                    boolean customPal);
vectorgraphicid_t AM_GetVectorGraphic(const automapcfg_t* cfg, int objectname);
void AM_SetVectorGraphic(automapcfg_t* cfg, int objectname, int vgname);
#endif
