/**\file am_map.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2005-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBCOMMON_AUTOMAP_CONFIG
#define LIBCOMMON_AUTOMAP_CONFIG

#define AUTOMAP_OPEN_SECONDS    (.3f)

#if __JDOOM__ || __JDOOM64__
#define BLACK                   (0)
#define WHITE                   (256-47)
#define REDS                    (256-5*16)
#define GREENS                  (7*16)
#define YELLOWS                 (256-32+7)
#define GRAYS                   (6*16)
#define BROWNS                  (4*16)

#define WALLCOLORS              REDS
#define TSWALLCOLORS            GRAYS
#define CDWALLCOLORS            YELLOWS
#define FDWALLCOLORS            BROWNS
#define THINGCOLORS             GREENS
#define BACKGROUND              BLACK

// Keys for Baby Mode
#define KEY1_COLOR              (197) // Blue Key
#define KEY2_COLOR              (256-5*16) // Red Key
#define KEY3_COLOR              (256-32+7) // Yellow Key
#define KEY4_COLOR              (256-32+7) // Yellow Skull
#define KEY5_COLOR              (256-5*16) // Red Skull
#define KEY6_COLOR              (197) // Blue Skull

#define AM_PLR1_COLOR           GREENS
#define AM_PLR2_COLOR           GRAYS
#define AM_PLR3_COLOR           BROWNS
#define AM_PLR4_COLOR           REDS
#endif

#ifdef __JHERETIC__
#define BLACK                   (0)
#define WHITE                   (4*8)
#define REDS                    (12*8)
#define GREENS                  (33*8)
#define YELLOWS                 (10*8)
#define GRAYS                   (5*8)
#define BROWNS                  (14*8-2)
#define PARCH                   (13*8-1)

#define WALLCOLORS              REDS
#define TSWALLCOLORS            GRAYS
#define CDWALLCOLORS            YELLOWS
#define FDWALLCOLORS            BROWNS
#define THINGCOLORS             (4)
#define BACKGROUND              PARCH

// Keys for Baby Mode
#define KEY1_COLOR              (144) // Green Key
#define KEY2_COLOR              (197) // Yellow Key
#define KEY3_COLOR              (220) // Blue Key

#define AM_PLR1_COLOR           (220)
#define AM_PLR2_COLOR           (197)
#define AM_PLR3_COLOR           (150)
#define AM_PLR4_COLOR           (144)
#endif


#ifdef __JHEXEN__
// For use if I do walls with outsides/insides
#define REDS                    (12*8)
#define BLUES                   (256-4*16+8)
#define GREENS                  (33*8)
#define GRAYS                   (5*8)
#define BROWNS                  (14*8)
#define YELLOWS                 (10*8)
#define BLACK                   (0)
#define WHITE                   (4*8)
#define PARCH                   (13*8-1)
#define BLOODRED                (177)

// Automap colors
#define BACKGROUND              PARCH
#define WALLCOLORS              (83) // REDS
#define TSWALLCOLORS            GRAYS
#define FDWALLCOLORS            (96) // BROWNS
#define CDWALLCOLORS            (107) // YELLOWS
#define THINGCOLORS             (255)
#define SECRETWALLCOLORS        WALLCOLORS

#define BORDEROFFSET            (4)

// Automap colors
#define AM_PLR1_COLOR           (157) // Blue
#define AM_PLR2_COLOR           (177) // Red
#define AM_PLR3_COLOR           (137) // Yellow
#define AM_PLR4_COLOR           (198) // Green
#define AM_PLR5_COLOR           (215) // Jade
#define AM_PLR6_COLOR           (32)  // White
#define AM_PLR7_COLOR           (106) // Hazel
#define AM_PLR8_COLOR           (234) // Purple

#define KEY1                    (197) // HEXEN -
#define KEY2                    (144) // HEXEN -
#define KEY3                    (220) // HEXEN -

#endif

typedef enum {
    AMO_NONE = -1,
    AMO_THING = 0,
    AMO_THINGPLAYER,
    AMO_UNSEENLINE,
    AMO_SINGLESIDEDLINE,
    AMO_TWOSIDEDLINE,
    AMO_FLOORCHANGELINE,
    AMO_CEILINGCHANGELINE,
    AMO_NUMOBJECTS
} automapcfg_objectname_t;

typedef enum glowtype_e {
    GLOW_NONE,
    GLOW_BOTH,
    GLOW_BACK,
    GLOW_FRONT
} glowtype_t;

enum {
    MOL_LINEDEF = 0,
    MOL_LINEDEF_TWOSIDED,
    MOL_LINEDEF_FLOOR,
    MOL_LINEDEF_CEILING,
    MOL_LINEDEF_UNSEEN,
    NUM_MAP_OBJECTLISTS
};

#define AUTOMAPCFG_MAX_LINEINFO         32

typedef struct {
    int reqSpecial;
    int reqSided;
    int reqAutomapFlags;
    float rgba[4];
    blendmode_t blendMode;
    float glowStrength, glowSize;
    glowtype_t glow;
    boolean scaleWithView;
} automapcfg_lineinfo_t;

typedef struct automapcfg_s {
    automapcfg_lineinfo_t lineInfo[AUTOMAPCFG_MAX_LINEINFO];
    uint lineInfoCount;

    svgid_t vectorGraphicForPlayer;
    svgid_t vectorGraphicForThing;

    automapcfg_lineinfo_t mapObjectInfo[NUM_MAP_OBJECTLISTS];
} automapcfg_t;

void ST_InitAutomapConfig(void);
automapcfg_t* ST_AutomapConfig(void);

void AM_GetMapColor(float* rgb, const float* uColor, int palidx, boolean customPal);

const automapcfg_lineinfo_t* AM_GetInfoForLine(automapcfg_t* mcfg, automapcfg_objectname_t name);
const automapcfg_lineinfo_t* AM_GetInfoForSpecialLine(automapcfg_t* mcfg, int special, const Sector* frontsector, const Sector* backsector, int automapFlags);

void AM_GetColorAndOpacity(automapcfg_t* mcfg, automapcfg_objectname_t name, float* r, float* g, float* b, float* a);
void AM_SetColorAndOpacity(automapcfg_t* mcfg, automapcfg_objectname_t name, float r, float g, float b, float a);

svgid_t AM_GetVectorGraphic(automapcfg_t* mcfg, automapcfg_objectname_t name);
void AM_SetVectorGraphic(automapcfg_t* mcfg, automapcfg_objectname_t name, svgid_t svg);

#endif /* LIBCOMMON_AUTOMAP_CONFIG */
