/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2005-2008 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef __COMMON_AUTOMAP__
#define __COMMON_AUTOMAP__

#define NUMMARKPOINTS       (10)

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

// Automap flags:
#define AMF_REND_THINGS         0x01
#define AMF_REND_KEYS           0x02
#define AMF_REND_ALLLINES       0x04
#define AMF_REND_XGLINES        0x08
#define AMF_REND_VERTEXES       0x10
#define AMF_REND_LINE_NORMALS   0x20

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

typedef struct automapcfg_s {
    float           lineGlowScale;
    boolean         glowingLineSpecials;
    float           backgroundRGBA[4];
    float           panSpeed;
    boolean         panResetOnOpen;
    float           zoomSpeed;

    mapobjectinfo_t mapObjectInfo[NUM_MAP_OBJECTLISTS];
} automapcfg_t;

typedef enum glowtype_e {
    NO_GLOW,
    TWOSIDED_GLOW,
    BACK_GLOW,
    FRONT_GLOW
} glowtype_t;

typedef enum vectorgraphname_e {
    VG_NONE = -1,
    VG_KEYSQUARE,
    VG_TRIANGLE,
    VG_ARROW,
    VG_CHEATARROW,
    NUM_VECTOR_GRAPHS
} vectorgrapname_t;

typedef struct mpoint_s {
    float               pos[3];
} mpoint_t;

typedef struct mline_s {
    mpoint_t            a, b;
} vgline_t;

typedef struct vectorgrap_s {
    DGLuint         dlist;
    uint            count;
    vgline_t*       lines;
} vectorgrap_t;

vectorgrap_t* AM_GetVectorGraph(vectorgrapname_t id);

extern int mapviewplayer;

void    AM_Register(void); // Called during init to register automap cvars and ccmds.
void    AM_Init(void); // Called during init to initialize the automap.
void    AM_Shutdown(void); // Called on exit to free any allocated memory.

void    AM_InitForMap(void); // Called at the end of a map load.
void    AM_Ticker(void); // Called by main loop.

automapid_t AM_MapForPlayer(int plrnum);

void    AM_Open(automapid_t id, boolean yes, boolean fast);

void    AM_SetWindowTarget(automapid_t id, int x, int y, int w, int h);
void    AM_SetWindowFullScreenMode(automapid_t id, int value);
void    AM_SetViewTarget(automapid_t id, float x, float y);
void    AM_SetViewScaleTarget(automapid_t id, float scale);
void    AM_SetViewAngleTarget(automapid_t id, float angle);
void    AM_SetViewRotate(automapid_t id, int offOnToggle);
void    AM_SetGlobalAlphaTarget(automapid_t id, float alpha);
void    AM_SetColor(automapid_t id, int objectname, float r, float g, float b);
void    AM_SetColorAndAlpha(automapid_t id, int objectname, float r, float g,
                            float b, float a);
void    AM_SetBlendmode(automapid_t id, int objectname, blendmode_t blendmode);
void    AM_SetGlow(automapid_t id, int objectname, glowtype_t type, float size,
                   float alpha, boolean canScale);
void    AM_SetVectorGraphic(automapid_t id, int objectname, int vgname);
vectorgrapname_t AM_GetVectorGraphic(automapid_t id, int objectname);
void    AM_RegisterSpecialLine(automapid_t id, int cheatLevel, int lineSpecial,
                               int sided,
                               float r, float g, float b, float a,
                               blendmode_t blendmode,
                               glowtype_t glowType, float glowAlpha,
                               float glowWidth, boolean scaleGlowWithView);
int     AM_AddMark(automapid_t id, float x, float y);
boolean AM_GetMark(automapid_t id, uint mark, float* x, float* y);
void    AM_ClearMarks(automapid_t id);
void    AM_ToggleFollow(automapid_t id);
void    AM_ToggleZoomMax(automapid_t id);
void    AM_UpdateLinedef(automapid_t id, uint lineIdx, boolean visible);
void    AM_RevealMap(automapid_t id, boolean on);
boolean AM_IsRevealed(automapid_t id);

// \todo Split this functionality down into logical seperate settings.
void    AM_SetCheatLevel(automapid_t id, int level);
void    AM_IncMapCheatLevel(automapid_t id); // Called to increase map cheat level.

boolean AM_IsActive(automapid_t id);
float   AM_FrameToMap(automapid_t id, float val);
float   AM_MapToFrame(automapid_t id, float val);
void    AM_GetWindow(automapid_t id, float* x, float* y, float* w, float* h);
boolean AM_IsMapWindowInFullScreenMode(automapid_t id);
float   AM_GlobalAlpha(automapid_t id);
void    AM_GetColor(automapid_t id, int objectname, float* r, float* g, float* b);
void    AM_GetColorAndAlpha(automapid_t id, int objectname, float* r, float* g,
                            float* b, float* a);
void    AM_GetViewPosition(automapid_t id, float* x, float* y);
void    AM_GetViewParallaxPosition(automapid_t id, float* x, float* y);
float   AM_ViewAngle(automapid_t id);
float   AM_MapToFrameMultiplier(automapid_t id);
int     AM_GetFlags(automapid_t id);
void    AM_GetMapBBox(automapid_t id, float vbbox[4]);

void    M_DrawMAP(void); // Called to render the map menu.

const automapcfg_t* AM_GetMapConfig(automapid_t id);
const mapobjectinfo_t* AM_GetMapObjectInfo(automapid_t id, int objectname);
const mapobjectinfo_t* AM_GetInfoForSpecialLine(automapid_t id, int special,
                                                const sector_t* frontsector,
                                                const sector_t* backsector);

void AM_GetMapColor(float* rgb, const float* uColor, int palidx,
                    boolean customPal);
#endif
