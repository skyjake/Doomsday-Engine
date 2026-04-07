/** @file automapstyle.cpp  Style configuration for AutomapWidget.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "common.h"
#include "hud/automapstyle.h"

#include <cstdio>
#include <cmath>
#include <cctype>
#include <cstring>
#include <de/logbuffer.h>
#include "p_mapsetup.h"
#include "hu_stuff.h"
#include "player.h"
#include "hud/widgets/automapwidget.h"  /// @ref automapWidgetFlags

using namespace de;

DE_PIMPL_NOREF(AutomapStyle)
{
    automapcfg_lineinfo_t lineInfo[AUTOMAPCFG_MAX_LINEINFO];
    uint lineInfoCount = 0;

    svgid_t playerSvg = 0;
    svgid_t thingSvg = 0;

    automapcfg_lineinfo_t mapObjectInfo[NUM_MAP_OBJECTLISTS];

    Impl() { de::zap(mapObjectInfo); }

    void reset()
    {
        de::zap(lineInfo);
        lineInfoCount = 0;

        playerSvg = 0;
        thingSvg  = 0;

        de::zap(mapObjectInfo);
    }

    automapcfg_lineinfo_t *findLineInfo(int reqAutomapFlags, int reqSpecial, int reqSided,
        int reqNotFlagged)
    {
        for(uint i = 0; i < lineInfoCount; ++i)
        {
            automapcfg_lineinfo_t *info = &lineInfo[i];

            if(info->reqSpecial      != reqSpecial) continue;
            if(info->reqAutomapFlags != reqAutomapFlags) continue;
            if(info->reqSided        != reqSided) continue;
            if(info->reqNotFlagged   != reqNotFlagged) continue;

            return info;  // This is the one.
        }
        return nullptr;  // Not found.
    }

    void newLineInfo(int reqAutomapFlags, int reqSpecial, int reqSided, int reqNotFlagged,
        float r, float g, float b, float a, blendmode_t /*blendmode*/, glowtype_t glowType,
        float glowStrength, float glowSize, dd_bool scaleGlowWithView)
    {
        DE_ASSERT(reqSpecial >= 0)
        DE_ASSERT(reqSided >= 0 && reqSided <= 2);

        // Later re-registrations override earlier ones.
        automapcfg_lineinfo_t *info = findLineInfo(reqAutomapFlags, reqSpecial, reqSided, reqNotFlagged);
        if(!info)
        {
            // Any room for a new special line?
            if(lineInfoCount >= AUTOMAPCFG_MAX_LINEINFO)
                throw Error("AutomapStyle::d->newLineInfo", "No available slot.");

            info = &lineInfo[lineInfoCount++];
        }

        info->reqAutomapFlags   = reqAutomapFlags;
        info->reqSpecial        = reqSpecial;
        info->reqSided          = reqSided;
        info->reqNotFlagged     = reqNotFlagged;

        info->rgba[0]       = de::clamp(0.f, r, 1.f);
        info->rgba[1]       = de::clamp(0.f, g, 1.f);
        info->rgba[2]       = de::clamp(0.f, b, 1.f);
        info->rgba[3]       = de::clamp(0.f, a, 1.f);
        info->glow          = glowType;
        info->glowStrength  = de::clamp(0.f, glowStrength, 1.f);
        info->glowSize      = glowSize;
        info->scaleWithView = scaleGlowWithView;
        //info->blendMode     = blendmode;
    }
};

AutomapStyle::AutomapStyle() : d(new Impl)
{}

AutomapStyle::~AutomapStyle()
{}

const automapcfg_lineinfo_t &AutomapStyle::lineInfo(int lineType)
{
    DE_ASSERT(lineType >= 0 && lineType < NUM_MAP_OBJECTLISTS);
    return d->mapObjectInfo[lineType];
}

const automapcfg_lineinfo_t *AutomapStyle::tryFindLineInfo(automapcfg_objectname_t name) const
{
    if(name == AMO_NONE) return nullptr;  // Ignore

    if(name < 0 || name >= AMO_NUMOBJECTS)
        throw Error("AutomapStyle::tryFindLineInfo", "Unknown object #" + String::asText((int) name));

    switch(name)
    {
    case AMO_UNSEENLINE:        return &d->mapObjectInfo[MOL_LINEDEF_UNSEEN  ];
    case AMO_SINGLESIDEDLINE:   return &d->mapObjectInfo[MOL_LINEDEF         ];
    case AMO_TWOSIDEDLINE:      return &d->mapObjectInfo[MOL_LINEDEF_TWOSIDED];
    case AMO_FLOORCHANGELINE:   return &d->mapObjectInfo[MOL_LINEDEF_FLOOR   ];
    case AMO_CEILINGCHANGELINE: return &d->mapObjectInfo[MOL_LINEDEF_CEILING ];

    default: break;
    }

    return nullptr;
}

const automapcfg_lineinfo_t *AutomapStyle::tryFindLineInfo_special(int special,
    int flags, const Sector *frontsector, const Sector *backsector, int automapFlags) const
{
    if(special > 0)
    {
        for(uint i = 0; i < d->lineInfoCount; ++i)
        {
            const automapcfg_lineinfo_t *info = &d->lineInfo[i];

            // Special restriction?
            if(info->reqSpecial != special) continue;

            // Sided restriction?
            if((info->reqSided == 1 && backsector && frontsector) ||
               (info->reqSided == 2 && (!backsector || !frontsector)))
                continue;

            // Line flags restriction?
            if(info->reqNotFlagged && (flags & info->reqNotFlagged)) continue;

            // Automap flags restriction?
            if(info->reqAutomapFlags != 0 && !(automapFlags & info->reqAutomapFlags)) continue;

            // This is the one!
            return info;
        }
    }
    return nullptr;  // Not found.
}

void AutomapStyle::applyDefaults()
{
    d->reset();

    d->mapObjectInfo[MOL_LINEDEF_UNSEEN].glow = GLOW_NONE;
    d->mapObjectInfo[MOL_LINEDEF_UNSEEN].glowStrength = 1;
    d->mapObjectInfo[MOL_LINEDEF_UNSEEN].glowSize = 10;
    //d->mapObjectInfo[MOL_LINEDEF_UNSEEN].blendMode = BM_NORMAL;
    d->mapObjectInfo[MOL_LINEDEF_UNSEEN].scaleWithView = false;
    d->mapObjectInfo[MOL_LINEDEF_UNSEEN].rgba[0] = 1;
    d->mapObjectInfo[MOL_LINEDEF_UNSEEN].rgba[1] = 1;
    d->mapObjectInfo[MOL_LINEDEF_UNSEEN].rgba[2] = 1;
    d->mapObjectInfo[MOL_LINEDEF_UNSEEN].rgba[3] = 1;
    d->mapObjectInfo[MOL_LINEDEF].glow = GLOW_NONE;
    d->mapObjectInfo[MOL_LINEDEF].glowStrength = 1;
    d->mapObjectInfo[MOL_LINEDEF].glowSize = 10;
    //d->mapObjectInfo[MOL_LINEDEF].blendMode = BM_NORMAL;
    d->mapObjectInfo[MOL_LINEDEF].scaleWithView = false;
    d->mapObjectInfo[MOL_LINEDEF].rgba[0] = 1;
    d->mapObjectInfo[MOL_LINEDEF].rgba[1] = 1;
    d->mapObjectInfo[MOL_LINEDEF].rgba[2] = 1;
    d->mapObjectInfo[MOL_LINEDEF].rgba[3] = 1;
    d->mapObjectInfo[MOL_LINEDEF_TWOSIDED].glow = GLOW_NONE;
    d->mapObjectInfo[MOL_LINEDEF_TWOSIDED].glowStrength = 1;
    d->mapObjectInfo[MOL_LINEDEF_TWOSIDED].glowSize = 10;
    //d->mapObjectInfo[MOL_LINEDEF_TWOSIDED].blendMode = BM_NORMAL;
    d->mapObjectInfo[MOL_LINEDEF_TWOSIDED].scaleWithView = false;
    d->mapObjectInfo[MOL_LINEDEF_TWOSIDED].rgba[0] = 1;
    d->mapObjectInfo[MOL_LINEDEF_TWOSIDED].rgba[1] = 1;
    d->mapObjectInfo[MOL_LINEDEF_TWOSIDED].rgba[2] = 1;
    d->mapObjectInfo[MOL_LINEDEF_TWOSIDED].rgba[3] = 1;
    d->mapObjectInfo[MOL_LINEDEF_FLOOR].glow = GLOW_NONE;
    d->mapObjectInfo[MOL_LINEDEF_FLOOR].glowStrength = 1;
    d->mapObjectInfo[MOL_LINEDEF_FLOOR].glowSize = 10;
    //d->mapObjectInfo[MOL_LINEDEF_FLOOR].blendMode = BM_NORMAL;
    d->mapObjectInfo[MOL_LINEDEF_FLOOR].scaleWithView = false;
    d->mapObjectInfo[MOL_LINEDEF_FLOOR].rgba[0] = 1;
    d->mapObjectInfo[MOL_LINEDEF_FLOOR].rgba[1] = 1;
    d->mapObjectInfo[MOL_LINEDEF_FLOOR].rgba[2] = 1;
    d->mapObjectInfo[MOL_LINEDEF_FLOOR].rgba[3] = 1;
    d->mapObjectInfo[MOL_LINEDEF_CEILING].glow = GLOW_NONE;
    d->mapObjectInfo[MOL_LINEDEF_CEILING].glowStrength = 1;
    d->mapObjectInfo[MOL_LINEDEF_CEILING].glowSize = 10;
    //d->mapObjectInfo[MOL_LINEDEF_CEILING].blendMode = BM_NORMAL;
    d->mapObjectInfo[MOL_LINEDEF_CEILING].scaleWithView = false;
    d->mapObjectInfo[MOL_LINEDEF_CEILING].rgba[0] = 1;
    d->mapObjectInfo[MOL_LINEDEF_CEILING].rgba[1] = 1;
    d->mapObjectInfo[MOL_LINEDEF_CEILING].rgba[2] = 1;
    d->mapObjectInfo[MOL_LINEDEF_CEILING].rgba[3] = 1;

    // Register lines we want to display in a special way.
#if __JDOOM__ || __JDOOM64__
    // Blue locked door, open.
    d->newLineInfo(0,  32, 2, ML_SECRET, 0, 0, .776f, 1, BM_NORMAL, GLOW_BOTH, .75f, 5, true);
    // Blue locked door, locked.
    d->newLineInfo(0,  26, 2, ML_SECRET, 0, 0, .776f, 1, BM_NORMAL, GLOW_BOTH, .75f, 5, true);
    d->newLineInfo(0,  99, 0, ML_SECRET, 0, 0, .776f, 1, BM_NORMAL, GLOW_BOTH, .75f, 5, true);
    d->newLineInfo(0, 133, 0, ML_SECRET, 0, 0, .776f, 1, BM_NORMAL, GLOW_BOTH, .75f, 5, true);
    // Red locked door, open.
    d->newLineInfo(0,  33, 2, ML_SECRET, .682f, 0, 0, 1, BM_NORMAL, GLOW_BOTH, .75f, 5, true);
    // Red locked door, locked.
    d->newLineInfo(0,  28, 2, ML_SECRET, .682f, 0, 0, 1, BM_NORMAL, GLOW_BOTH, .75f, 5, true);
    d->newLineInfo(0, 134, 0, ML_SECRET, .682f, 0, 0, 1, BM_NORMAL, GLOW_BOTH, .75f, 5, true);
    d->newLineInfo(0, 135, 0, ML_SECRET, .682f, 0, 0, 1, BM_NORMAL, GLOW_BOTH, .75f, 5, true);
    // Yellow locked door, open.
    d->newLineInfo(0,  34, 2, ML_SECRET, .905f, .9f, 0, 1, BM_NORMAL, GLOW_BOTH, .75f, 5, true);
    // Yellow locked door, locked.
    d->newLineInfo(0,  27, 2, ML_SECRET, .905f, .9f, 0, 1, BM_NORMAL, GLOW_BOTH, .75f, 5, true);
    d->newLineInfo(0, 136, 0, ML_SECRET, .905f, .9f, 0, 1, BM_NORMAL, GLOW_BOTH, .75f, 5, true);
    d->newLineInfo(0, 137, 0, ML_SECRET, .905f, .9f, 0, 1, BM_NORMAL, GLOW_BOTH, .75f, 5, true);
    // Exit switch.
    d->newLineInfo(AWF_SHOW_SPECIALLINES,  11, 0, ML_SECRET, 0, 1, 0, 1, BM_NORMAL, GLOW_BOTH, .75f, 5, true);
    // Exit cross line.
    d->newLineInfo(AWF_SHOW_SPECIALLINES,  52, 2, ML_SECRET, 0, 1, 0, 1, BM_NORMAL, GLOW_BOTH, .75f, 5, true);
    // Secret Exit switch.
    d->newLineInfo(AWF_SHOW_SPECIALLINES,  51, 0, ML_SECRET, 0, 1, 1, 1, BM_NORMAL, GLOW_BOTH, .75f, 5, true);
    // Secret Exit cross line.
    d->newLineInfo(AWF_SHOW_SPECIALLINES, 124, 2, ML_SECRET, 0, 1, 1, 1, BM_NORMAL, GLOW_BOTH, .75f, 5, true);
#elif __JHERETIC__
    // Blue locked door.
    d->newLineInfo(0, 26, 2, ML_SECRET, 0, 0, .776f, 1, BM_NORMAL, GLOW_BOTH, .75f, 5, true);
    // Blue switch?
    d->newLineInfo(0, 32, 0, ML_SECRET, 0, 0, .776f, 1, BM_NORMAL, GLOW_BOTH, .75f, 5, true);
    // Yellow locked door.
    d->newLineInfo(0, 27, 2, ML_SECRET, .905f, .9f, 0, 1, BM_NORMAL, GLOW_BOTH, .75f, 5, true);
    // Yellow switch?
    d->newLineInfo(0, 34, 0, ML_SECRET, .905f, .9f, 0, 1, BM_NORMAL, GLOW_BOTH, .75f, 5, true);
    // Green locked door.
    d->newLineInfo(0, 28, 2, ML_SECRET, 0, .9f, 0, 1, BM_NORMAL, GLOW_BOTH, .75f, 5, true);
    // Green switch?
    d->newLineInfo(0, 33, 0, ML_SECRET, 0, .9f, 0, 1, BM_NORMAL, GLOW_BOTH, .75f, 5, true);
#elif __JHEXEN__
    // A locked door (all are green).
    d->newLineInfo(0, 13, 0, ML_SECRET, 0, .9f, 0, 1, BM_NORMAL, GLOW_BOTH, .75f, 5, true);
    d->newLineInfo(0, 83, 0, ML_SECRET, 0, .9f, 0, 1, BM_NORMAL, GLOW_BOTH, .75f, 5, true);
    // Intra-map teleporters (all are blue).
    d->newLineInfo(0, 70, 2, ML_SECRET, 0, 0, .776f, 1, BM_NORMAL, GLOW_BOTH, .75f, 5, true);
    d->newLineInfo(0, 71, 2, ML_SECRET, 0, 0, .776f, 1, BM_NORMAL, GLOW_BOTH, .75f, 5, true);
    // Inter-map teleport.
    d->newLineInfo(0, 74, 2, ML_SECRET, .682f, 0, 0, 1, BM_NORMAL, GLOW_BOTH, .75f, 5, true);
    // Game-winning exit.
    d->newLineInfo(0, 75, 2, ML_SECRET, .682f, 0, 0, 1, BM_NORMAL, GLOW_BOTH, .75f, 5, true);
#endif

    setObjectSvg(AMO_THING, VG_TRIANGLE);
    setObjectSvg(AMO_THINGPLAYER, VG_ARROW);

    float rgb[3];
    AM_GetMapColor(rgb, cfg.common.automapL0, UNWALLCOLORS, customPal);
    setObjectColorAndOpacity(AMO_UNSEENLINE, rgb[0], rgb[1], rgb[2], 1);

    AM_GetMapColor(rgb, cfg.common.automapL1, WALLCOLORS, customPal);
    setObjectColorAndOpacity(AMO_SINGLESIDEDLINE, rgb[0], rgb[1], rgb[2], 1);

    AM_GetMapColor(rgb, cfg.common.automapL0, TSWALLCOLORS, customPal);
    setObjectColorAndOpacity(AMO_TWOSIDEDLINE, rgb[0], rgb[1], rgb[2], 1);

    AM_GetMapColor(rgb, cfg.common.automapL2, FDWALLCOLORS, customPal);
    setObjectColorAndOpacity(AMO_FLOORCHANGELINE, rgb[0], rgb[1], rgb[2], 1);

    AM_GetMapColor(rgb, cfg.common.automapL3, CDWALLCOLORS, customPal);
    setObjectColorAndOpacity(AMO_CEILINGCHANGELINE, rgb[0], rgb[1], rgb[2], 1);
}

void AutomapStyle::objectColor(automapcfg_objectname_t name, float *r, float *g, float *b, float *a) const
{
    if(name < 0 || name >= AMO_NUMOBJECTS)
        throw Error("AutomapStyle::objectColor", "Unknown object #" + String::asText((int) name));

    // It must be an object with an info.
    const automapcfg_lineinfo_t *info = nullptr;
    switch(name)
    {
    case AMO_UNSEENLINE:        info = &d->mapObjectInfo[MOL_LINEDEF_UNSEEN  ]; break;
    case AMO_SINGLESIDEDLINE:   info = &d->mapObjectInfo[MOL_LINEDEF         ]; break;
    case AMO_TWOSIDEDLINE:      info = &d->mapObjectInfo[MOL_LINEDEF_TWOSIDED]; break;
    case AMO_FLOORCHANGELINE:   info = &d->mapObjectInfo[MOL_LINEDEF_FLOOR   ]; break;
    case AMO_CEILINGCHANGELINE: info = &d->mapObjectInfo[MOL_LINEDEF_CEILING ]; break;

    default: DE_ASSERT_FAIL("Object has no color property");
    }

    if(r) *r = info->rgba[0];
    if(g) *g = info->rgba[1];
    if(b) *b = info->rgba[2];
    if(a) *a = info->rgba[3];
}

void AutomapStyle::setObjectColor(automapcfg_objectname_t name, float r, float g, float b)
{
    if(name == AMO_NONE) return;  // Ignore.

    if(name < 0 || name >= AMO_NUMOBJECTS)
        throw Error("AutomapStyle::setObjectColor", "Unknown object #" + String::asText((int) name));

    // It must be an object with a name.
    automapcfg_lineinfo_t *info = nullptr;
    switch(name)
    {
    case AMO_UNSEENLINE:        info = &d->mapObjectInfo[MOL_LINEDEF_UNSEEN  ]; break;
    case AMO_SINGLESIDEDLINE:   info = &d->mapObjectInfo[MOL_LINEDEF         ]; break;
    case AMO_TWOSIDEDLINE:      info = &d->mapObjectInfo[MOL_LINEDEF_TWOSIDED]; break;
    case AMO_FLOORCHANGELINE:   info = &d->mapObjectInfo[MOL_LINEDEF_FLOOR   ]; break;
    case AMO_CEILINGCHANGELINE: info = &d->mapObjectInfo[MOL_LINEDEF_CEILING ]; break;

    default: DE_ASSERT_FAIL("Object has no color property");
    }

    info->rgba[0] = de::clamp(0.f, r, 1.f);
    info->rgba[1] = de::clamp(0.f, g, 1.f);
    info->rgba[2] = de::clamp(0.f, b, 1.f);
}

void AutomapStyle::setObjectColorAndOpacity(automapcfg_objectname_t name, float r, float g, float b, float a)
{
    if(name < 0 || name >= AMO_NUMOBJECTS)
        throw Error("AutomapStyle::setObjectColorAndOpacity", "Unknown object #" + String::asText((int) name));

    // It must be an object with an info.
    automapcfg_lineinfo_t *info = nullptr;
    switch(name)
    {
    case AMO_UNSEENLINE:        info = &d->mapObjectInfo[MOL_LINEDEF_UNSEEN  ]; break;
    case AMO_SINGLESIDEDLINE:   info = &d->mapObjectInfo[MOL_LINEDEF         ]; break;
    case AMO_TWOSIDEDLINE:      info = &d->mapObjectInfo[MOL_LINEDEF_TWOSIDED]; break;
    case AMO_FLOORCHANGELINE:   info = &d->mapObjectInfo[MOL_LINEDEF_FLOOR   ]; break;
    case AMO_CEILINGCHANGELINE: info = &d->mapObjectInfo[MOL_LINEDEF_CEILING ]; break;

    default: DE_ASSERT_FAIL("Object has no color property");
    }

    info->rgba[0] = de::clamp(0.f, r, 1.f);
    info->rgba[1] = de::clamp(0.f, g, 1.f);
    info->rgba[2] = de::clamp(0.f, b, 1.f);
    info->rgba[3] = de::clamp(0.f, a, 1.f);
}

void AutomapStyle::setObjectGlow(automapcfg_objectname_t name, glowtype_t type, float size,
    float alpha, dd_bool canScale)
{
    if(name < 0 || name >= AMO_NUMOBJECTS)
        throw Error("AutomapStyle::setObjectGlow", "Unknown object #" + String::asText((int) name));

    automapcfg_lineinfo_t *info = nullptr;
    switch(name)
    {
    case AMO_UNSEENLINE:        info = &d->mapObjectInfo[MOL_LINEDEF_UNSEEN  ]; break;
    case AMO_SINGLESIDEDLINE:   info = &d->mapObjectInfo[MOL_LINEDEF         ]; break;
    case AMO_TWOSIDEDLINE:      info = &d->mapObjectInfo[MOL_LINEDEF_TWOSIDED]; break;
    case AMO_FLOORCHANGELINE:   info = &d->mapObjectInfo[MOL_LINEDEF_FLOOR   ]; break;
    case AMO_CEILINGCHANGELINE: info = &d->mapObjectInfo[MOL_LINEDEF_CEILING ]; break;

    default: DE_ASSERT_FAIL("Object has no glow property");
    }

    info->glow          = type;
    info->glowStrength  = de::clamp(0.f, alpha, 1.f);
    info->glowSize      = de::clamp(0.f, size, 100.f);
    info->scaleWithView = canScale;
}

svgid_t AutomapStyle::objectSvg(automapcfg_objectname_t name) const
{
    if(name < 0 || name >= AMO_NUMOBJECTS)
        throw Error("AutomapStyle::objectSvg", "Unknown object #" + String::asText((int) name));

    switch(name)
    {
    case AMO_THING:       return d->thingSvg;
    case AMO_THINGPLAYER: return d->playerSvg;

    default: DE_ASSERT_FAIL("Object has no SVG property");
    }

    return 0;  // None.
}

void AutomapStyle::setObjectSvg(automapcfg_objectname_t name, svgid_t svg)
{
    if(name < 0 || name >= AMO_NUMOBJECTS)
        throw Error("AutomapStyle::setObjectSvg", "Unknown object #" + String::asText((int) name));

    switch(name)
    {
    case AMO_THING:         d->thingSvg  = svg; break;
    case AMO_THINGPLAYER:   d->playerSvg = svg; break;

    default: DE_ASSERT_FAIL("Object has no SVG property");
    }
}

static AutomapStyle style;

AutomapStyle *ST_AutomapStyle()
{
    return &style;
}

void ST_InitAutomapStyle()
{
    LOG_XVERBOSE("Initializing automap...", "");
    style.applyDefaults();
}

void AM_GetMapColor(float *rgb, const float *uColor, int palidx, dd_bool customPal)
{
    if ((!customPal && !cfg.common.automapCustomColors) ||
        (customPal && cfg.common.automapCustomColors != 2))
    {
        R_GetColorPaletteRGBf(0, palidx, rgb, false);
        return;
    }

    rgb[0] = uColor[0];
    rgb[1] = uColor[1];
    rgb[2] = uColor[2];
}
