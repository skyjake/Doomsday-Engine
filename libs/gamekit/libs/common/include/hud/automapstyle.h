/** @file automapstyle.h  Style configuration for AutomapWidget.
 *
 * @authors Copyright © 2005-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2015 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBCOMMON_UI_AUTOMAPSTYLE_H
#define LIBCOMMON_UI_AUTOMAPSTYLE_H

#include "doomsday.h"

#if defined (__JDOOM__) || defined(__JDOOM64__)
#   define BLACK                (0)
#   define WHITE                (256-47)
#   define REDS                 (256-5*16)
#   define GREENS               (7*16)
#   define YELLOWS              (256-32+7)
#   define GRAYS                (6*16)
#   define BROWNS               (4*16)

#   define WALLCOLORS           REDS
#   define TSWALLCOLORS         GRAYS
#   define UNWALLCOLORS         (GRAYS + 3)
#   define CDWALLCOLORS         YELLOWS
#   define FDWALLCOLORS         BROWNS
#   define THINGCOLORS          GREENS
#   define BACKGROUND           BLACK

// Keys for Baby Mode
#   define KEY1_COLOR           (197) // Blue Key
#   define KEY2_COLOR           (256-5*16) // Red Key
#   define KEY3_COLOR           (256-32+7) // Yellow Key
#   define KEY4_COLOR           (256-32+7) // Yellow Skull
#   define KEY5_COLOR           (256-5*16) // Red Skull
#   define KEY6_COLOR           (197) // Blue Skull

#   define AM_PLR1_COLOR        GREENS
#   define AM_PLR2_COLOR        GRAYS
#   define AM_PLR3_COLOR        BROWNS
#   define AM_PLR4_COLOR        REDS
#endif

#if defined (__JHERETIC__)
#   define BLACK                (0)
#   define WHITE                (35)
#   define REDS                 (145)
#   define GREENS               (209)
#   define YELLOWS              (111)
#   define GRAYS                (16)
#   define BROWNS               (66)
#   define PARCH                (103)

#   define WALLCOLORS           (72)
#   define TSWALLCOLORS         (1)
#   define UNWALLCOLORS         (40)
#   define CDWALLCOLORS         (77)
#   define FDWALLCOLORS         (110)
#   define THINGCOLORS          (4)
#   define BACKGROUND           PARCH

// Keys for Baby Mode
#   define KEY1_COLOR           (144) // Green Key
#   define KEY2_COLOR           (197) // Yellow Key
#   define KEY3_COLOR           (220) // Blue Key

#   define AM_PLR1_COLOR        (220)
#   define AM_PLR2_COLOR        (197)
#   define AM_PLR3_COLOR        (150)
#   define AM_PLR4_COLOR        (144)
#endif

#if defined (__JHEXEN__)
// For use if I do walls with outsides/insides
#   define REDS                 (12*8)
#   define BLUES                (256-4*16+8)
#   define GREENS               (33*8)
#   define GRAYS                (5*8)
#   define BROWNS               (14*8)
#   define YELLOWS              (10*8)
#   define BLACK                (0)
#   define WHITE                (4*8)
#   define PARCH                (13*8-1)
#   define BLOODRED             (177)

// Automap colors
#   define BACKGROUND           PARCH
#   define WALLCOLORS           (83) // REDS
#   define TSWALLCOLORS         GRAYS
#   define UNWALLCOLORS         (GRAYS + 3)
#   define FDWALLCOLORS         (96) // BROWNS
#   define CDWALLCOLORS         (107) // YELLOWS
#   define THINGCOLORS          (255)
#   define SECRETWALLCOLORS     WALLCOLORS

#   define BORDEROFFSET            (4)

// Automap colors
#   define AM_PLR1_COLOR        (157) // Blue
#   define AM_PLR2_COLOR        (177) // Red
#   define AM_PLR3_COLOR        (137) // Yellow
#   define AM_PLR4_COLOR        (198) // Green
#   define AM_PLR5_COLOR        (215) // Jade
#   define AM_PLR6_COLOR        (32)  // White
#   define AM_PLR7_COLOR        (106) // Hazel
#   define AM_PLR8_COLOR        (234) // Purple

#   define KEY1                 (197) // HEXEN -
#   define KEY2                 (144) // HEXEN -
#   define KEY3                 (220) // HEXEN -
#endif

enum automapcfg_objectname_t
{
    AMO_NONE = -1,
    AMO_THING = 0,
    AMO_THINGPLAYER,
    AMO_UNSEENLINE,
    AMO_SINGLESIDEDLINE,
    AMO_TWOSIDEDLINE,
    AMO_FLOORCHANGELINE,
    AMO_CEILINGCHANGELINE,
    AMO_NUMOBJECTS
};

enum glowtype_t
{
    GLOW_NONE,
    GLOW_BOTH,
    GLOW_BACK,
    GLOW_FRONT
};

enum
{
    MOL_LINEDEF = 0,
    MOL_LINEDEF_TWOSIDED,
    MOL_LINEDEF_FLOOR,
    MOL_LINEDEF_CEILING,
    MOL_LINEDEF_UNSEEN,
    NUM_MAP_OBJECTLISTS
};

#define AUTOMAPCFG_MAX_LINEINFO         32

struct automapcfg_lineinfo_t
{
    int reqSpecial;
    int reqSided;
    int reqNotFlagged;
    int reqAutomapFlags;
    float rgba[4];
    //blendmode_t blendMode;
    float glowStrength, glowSize;
    glowtype_t glow;
    dd_bool scaleWithView;
};

/**
 * @ingroup ui
 */
class AutomapStyle
{
    DE_NO_ASSIGN(AutomapStyle)
    DE_NO_COPY(AutomapStyle)

public:
    AutomapStyle();
    ~AutomapStyle();

    void applyDefaults();

    const automapcfg_lineinfo_t &lineInfo(int lineType);

    const automapcfg_lineinfo_t *tryFindLineInfo(automapcfg_objectname_t name) const;
    const automapcfg_lineinfo_t *tryFindLineInfo_special(int special, int flags, const Sector *frontsector, const Sector *backsector, int automapFlags) const;

    void objectColor(automapcfg_objectname_t name, float *r, float *g, float *b, float *a) const;
    void setObjectColor(automapcfg_objectname_t name, float r, float g, float b);
    void setObjectColorAndOpacity(automapcfg_objectname_t name, float r, float g, float b, float a);

    void setObjectGlow(automapcfg_objectname_t name, glowtype_t type, float size, float alpha, dd_bool canScale);

    svgid_t objectSvg(automapcfg_objectname_t name) const;
    void setObjectSvg(automapcfg_objectname_t name, svgid_t svg);

private:
    DE_PRIVATE(d)
};

void ST_InitAutomapStyle();

AutomapStyle *ST_AutomapStyle();

void AM_GetMapColor(float *rgb, const float *uColor, int palidx, dd_bool customPal);

#endif  // LIBCOMMON_UI_AUTOMAPSTYLE_H
