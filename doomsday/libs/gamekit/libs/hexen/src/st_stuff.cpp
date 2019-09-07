/** @file st_stuff.cpp  Hexen specific HUD and statusbar widgets.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2014 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 1999 Activision
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

#if defined(WIN32) && defined(_MSC_VER)
// Something in here is incompatible with MSVC 2010 optimization.
// Symptom: automap not visible.
#  pragma optimize("", off)
#  pragma warning(disable : 4748)
#endif

#include "jhexen.h"
#include "st_stuff.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "d_net.h"
#include "d_netsv.h"
#include "dmu_lib.h"
#include "g_common.h"
#include "hu_lib.h"
#include "hu_inventory.h"
#include "hu_stuff.h"
#include "hud/automapstyle.h"
#include "gl_drawpatch.h"
#include "p_inventory.h"
#include "p_mapsetup.h"
#include "p_tick.h" // for Pause_IsPaused
#include "player.h"
#include "r_common.h"

#include "hud/widgets/armorwidget.h"
#include "hud/widgets/automapwidget.h"
#include "hud/widgets/chainwidget.h"
#include "hud/widgets/chatwidget.h"
#include "hud/widgets/flightwidget.h"
#include "hud/widgets/fragswidget.h"
#include "hud/widgets/groupwidget.h"
#include "hud/widgets/healthwidget.h"
#include "hud/widgets/keyswidget.h"
#include "hud/widgets/playerlogwidget.h"
#include "hud/widgets/readyammowidget.h"
#include "hud/widgets/readyitemwidget.h"

#include "hud/widgets/armoriconswidget.h"
#include "hud/widgets/bluemanaiconwidget.h"
#include "hud/widgets/bluemanavialwidget.h"
#include "hud/widgets/bluemanawidget.h"
#include "hud/widgets/bootswidget.h"
#include "hud/widgets/defensewidget.h"
#include "hud/widgets/greenmanaiconwidget.h"
#include "hud/widgets/greenmanavialwidget.h"
#include "hud/widgets/greenmanawidget.h"
#include "hud/widgets/servantwidget.h"
#include "hud/widgets/weaponpieceswidget.h"
#include "hud/widgets/worldtimewidget.h"

using namespace de;

enum
{
    UWG_STATUSBAR,
    UWG_MAPNAME,
    UWG_BOTTOMLEFT,
    UWG_BOTTOMRIGHT,
    UWG_BOTTOMCENTER,
    UWG_BOTTOM,
    UWG_TOP,
    UWG_TOPCENTER,
    UWG_TOPLEFT,
    UWG_TOPLEFT2,
    UWG_TOPLEFT3,
    UWG_TOPRIGHT,
    UWG_AUTOMAP,
    NUM_UIWIDGET_GROUPS
};

struct hudstate_t
{
    dd_bool inited;
    dd_bool stopped;
    int hideTics;
    float hideAmount;
    float alpha;                ///< Fullscreen hud alpha value.
    float showBar;              ///< Slide statusbar amount 1.0 is fully open.
    dd_bool statusbarActive;    ///< Whether the statusbar is active.
    int automapCheatLevel;      ///< @todo Belongs in player state?
    int readyItemFlashCounter;

    uiwidgetid_t groupIds[NUM_UIWIDGET_GROUPS];

    // Statusbar:
    uiwidgetid_t sbarHealthId;
    uiwidgetid_t sbarWeaponpiecesId;
    uiwidgetid_t sbarBluemanaiconId;
    uiwidgetid_t sbarBluemanaId;
    uiwidgetid_t sbarBluemanavialId;
    uiwidgetid_t sbarGreenmanaiconId;
    uiwidgetid_t sbarGreenmanaId;
    uiwidgetid_t sbarGreenmanavialId;
    uiwidgetid_t sbarKeysId;
    uiwidgetid_t sbarArmoriconsId;
    uiwidgetid_t sbarChainId;
    uiwidgetid_t sbarArmorId;
    uiwidgetid_t sbarFragsId;
    uiwidgetid_t sbarReadyitemId;

    // Fullscreen:
    uiwidgetid_t healthId;
    uiwidgetid_t fragsId;
    uiwidgetid_t bluemanaiconId;
    uiwidgetid_t bluemanaId;
    uiwidgetid_t greenmanaiconId;
    uiwidgetid_t greenmanaId;
    uiwidgetid_t readyitemId;

    // Other:
    uiwidgetid_t automapId;
    uiwidgetid_t chatId;
    uiwidgetid_t logId;
    uiwidgetid_t flightId;
    uiwidgetid_t bootsId;
    uiwidgetid_t servantId;
    uiwidgetid_t defenseId;
    uiwidgetid_t worldtimeId;
};

static hudstate_t hudStates[MAXPLAYERS];

static patchid_t pStatusBar;
static patchid_t pStatusBarTop;
static patchid_t pKills;
static patchid_t pStatBar;
static patchid_t pKeyBar;
static patchid_t pInventoryBar;
static patchid_t pWeaponSlot[3]; // [Fighter, Cleric, Mage]

/**
 * Draws the whole statusbar backgound.
 * @todo There is a whole lot of constants in here. What if someone wants to
 * replace the statusbar with new patches?
 */
void SBarBackground_Drawer(HudWidget *wi, const Point2Raw *offset)
{
#define WIDTH       (ST_WIDTH)
#define HEIGHT      (ST_HEIGHT)
#define ORIGINX     int(-WIDTH / 2)
#define ORIGINY     int(-HEIGHT * hud->showBar)

    DE_ASSERT(wi);
    const hudstate_t *hud   = &hudStates[wi->player()];
    const int pClass        = cfg.playerClass[wi->player()]; // Original class (i.e. not pig).
    const int activeHud     = ST_ActiveHud(wi->player());
    const float iconOpacity = (activeHud == 0? 1 : uiRendState->pageAlpha * cfg.common.statusbarOpacity);

    if(ST_AutomapIsOpen(wi->player()) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player()].plr->mo) && Get(DD_PLAYBACK)) return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.common.statusbarScale, cfg.common.statusbarScale, 1);

    if(!(iconOpacity < 1))
    {
        DGL_Enable(DGL_TEXTURE_2D);

        DGL_Color4f(1, 1, 1, 1);
        GL_DrawPatch(pStatusBar, Vec2i(ORIGINX, ORIGINY - 28));

        DGL_Disable(DGL_TEXTURE_2D);

        /// @todo Kludge: The Hexen statusbar graphic has a chain already in the
        /// image, which shows through the modified chain patches.
        /// Mask out the chain on the statusbar by drawing a solid black
        /// rectangle over it.
        DGL_SetNoMaterial();
        DGL_DrawRectf2Color(ORIGINX+44, ORIGINY+31, 232, 7, .1f, .1f, .1f, 1);
        //// @todo Kludge: end

        DGL_Enable(DGL_TEXTURE_2D);

        DGL_Color4f(1, 1, 1, 1);
        GL_DrawPatch(pStatusBarTop, Vec2i(ORIGINX, ORIGINY - 28));

        if(!Hu_InventoryIsOpen(wi->player()))
        {
            // Main interface
            if(!ST_AutomapIsOpen(wi->player()))
            {
                GL_DrawPatch(pStatBar, Vec2i(ORIGINX + 38, ORIGINY));

                if(gfw_Rule(deathmatch))
                {
                    GL_DrawPatch(pKills, Vec2i(ORIGINX + 38, ORIGINY));
                }

                GL_DrawPatch(pWeaponSlot[pClass], Vec2i(ORIGINX + 190, ORIGINY));
            }
            else
            {
                GL_DrawPatch(pKeyBar, Vec2i(ORIGINX + 38, ORIGINY));
            }
        }
        else
        {
            GL_DrawPatch(pInventoryBar, Vec2i(ORIGINX + 38, ORIGINY));
        }

        DGL_Disable(DGL_TEXTURE_2D);
    }
    else
    {
        DGL_Enable(DGL_TEXTURE_2D);

        DGL_Color4f(1, 1, 1, iconOpacity);
        DGL_SetPatch(pStatusBar, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);

        DGL_Begin(DGL_QUADS);

        // top
        int x    = ORIGINX;
        int y    = ORIGINY-27;
        int w    = ST_WIDTH;
        int h    = 27;
        float ch = 0.41538461538461538461538461538462f;

        DGL_TexCoord2f(0, 0, 0);
        DGL_Vertex2f(x, y);
        DGL_TexCoord2f(0, 1, 0);
        DGL_Vertex2f(x + w, y);
        DGL_TexCoord2f(0, 1, ch);
        DGL_Vertex2f(x + w, y + h);
        DGL_TexCoord2f(0, 0, ch);
        DGL_Vertex2f(x, y + h);

        // left statue
        x = ORIGINX;
        y = ORIGINY;
        w = 38;
        h = 38;
        float cw = (float) 38 / ST_WIDTH;
        ch = 0.41538461538461538461538461538462f;

        DGL_TexCoord2f(0, 0, ch);
        DGL_Vertex2f(x, y);
        DGL_TexCoord2f(0, cw, ch);
        DGL_Vertex2f(x + w, y);
        DGL_TexCoord2f(0, cw, 1);
        DGL_Vertex2f(x + w, y + h);
        DGL_TexCoord2f(0, 0, 1);
        DGL_Vertex2f(x, y + h);

        // right statue
        x = ORIGINX+282;
        y = ORIGINY;
        w = 38;
        h = 38;
        cw = (float) (ST_WIDTH - 38) / ST_WIDTH;
        ch = 0.41538461538461538461538461538462f;

        DGL_TexCoord2f(0, cw, ch);
        DGL_Vertex2f(x, y);
        DGL_TexCoord2f(0, 1, ch);
        DGL_Vertex2f(x + w, y);
        DGL_TexCoord2f(0, 1, 1);
        DGL_Vertex2f(x + w, y + h);
        DGL_TexCoord2f(0, cw, 1);
        DGL_Vertex2f(x, y + h);
        DGL_End();

        /// @todo Kludge: The Hexen statusbar graphic has a chain already in the
        /// image, which shows through the modified chain patches.
        /// Mask out the chain on the statusbar by cutting a window out and
        /// drawing a solid near-black rectangle to fill the hole.
        DGL_DrawCutRectf2Tiled(ORIGINX+38, ORIGINY+31, 244, 8, 320, 65, 38, 192-134, ORIGINX+44, ORIGINY+31, 232, 7);
        DGL_Disable(DGL_TEXTURE_2D);
        DGL_SetNoMaterial();
        DGL_DrawRectf2Color(ORIGINX+44, ORIGINY+31, 232, 7, .1f, .1f, .1f, iconOpacity);
        DGL_Color4f(1, 1, 1, iconOpacity);
        //// @todo Kludge: end

        if(!Hu_InventoryIsOpen(wi->player()))
        {
            DGL_Enable(DGL_TEXTURE_2D);

            // Main interface
            if(!ST_AutomapIsOpen(wi->player()))
            {
                patchinfo_t pStatBarInfo;
                if(R_GetPatchInfo(pStatBar, &pStatBarInfo))
                {
                    x = ORIGINX + (gfw_Rule(deathmatch) ? 68 : 38);
                    y = ORIGINY;
                    w = gfw_Rule(deathmatch)?214:244;
                    h = 31;
                    DGL_SetPatch(pStatBar, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);
                    DGL_DrawCutRectf2Tiled(x, y, w, h, pStatBarInfo.geometry.size.width, pStatBarInfo.geometry.size.height, gfw_Rule(deathmatch)?30:0, 0, ORIGINX+190, ORIGINY, 57, 30);
                }

                GL_DrawPatch(pWeaponSlot[pClass], Vec2i(ORIGINX + 190, ORIGINY));
                if(gfw_Rule(deathmatch))
                    GL_DrawPatch(pKills, Vec2i(ORIGINX + 38, ORIGINY));
            }
            else
            {
                GL_DrawPatch(pKeyBar, Vec2i(ORIGINX + 38, ORIGINY));
            }

            DGL_Disable(DGL_TEXTURE_2D);
        }
        else
        {   // INVBAR

            DGL_SetPatch(pInventoryBar, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);
            DGL_Enable(DGL_TEXTURE_2D);

            x = ORIGINX+38;
            y = ORIGINY;
            w = 244;
            h = 30;
            ch = 0.96774193548387096774193548387097f;

            DGL_Begin(DGL_QUADS);
                DGL_TexCoord2f(0, 0, 0);
                DGL_Vertex2f(x, y);
                DGL_TexCoord2f(0, 1, 0);
                DGL_Vertex2f(x + w, y);
                DGL_TexCoord2f(0, 1, ch);
                DGL_Vertex2f(x + w, y + h);
                DGL_TexCoord2f(0, 0, ch);
                DGL_Vertex2f(x, y + h);
            DGL_End();

            DGL_Disable(DGL_TEXTURE_2D);
        }
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef WIDTH
#undef HEIGHT
#undef ORIGINX
#undef ORIGINY
}

void SBarBackground_UpdateGeometry(HudWidget *wi)
{
    DE_ASSERT(wi);

    Rect_SetWidthHeight(&wi->geometry(), 0, 0);

    if(ST_AutomapIsOpen(wi->player()) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player()].plr->mo) && Get(DD_PLAYBACK)) return;

    Rect_SetWidthHeight(&wi->geometry(), ST_WIDTH  * cfg.common.statusbarScale,
                                         ST_HEIGHT * cfg.common.statusbarScale);
}

void SBarInventory_Drawer(HudWidget *wi, const Point2Raw *offset)
{
#define X_OFFSET            ( 50 )
#define Y_OFFSET            (  1 )

    DE_ASSERT(wi);
    const hudstate_t *hud   = &hudStates[wi->player()];
    const int activeHud     = ST_ActiveHud(wi->player());
    const float yOffset     = ST_HEIGHT*(1-hud->showBar);
    //const float textOpacity = (activeHud == 0? 1 : uiRendState->pageAlpha * cfg.common.statusbarCounterAlpha);
    const float iconOpacity = (activeHud == 0? 1 : uiRendState->pageAlpha * cfg.common.statusbarCounterAlpha);

    if(!Hu_InventoryIsOpen(wi->player())) return;
    if(ST_AutomapIsOpen(wi->player()) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player()].plr->mo) && Get(DD_PLAYBACK)) return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.common.statusbarScale, cfg.common.statusbarScale, 1);

    Hu_InventoryDraw2(wi->player(), -ST_WIDTH/2 + X_OFFSET, -ST_HEIGHT + yOffset + Y_OFFSET, iconOpacity);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef Y_OFFSET
#undef X_OFFSET
}

void SBarInventory_UpdateGeometry(HudWidget *wi)
{
    DE_ASSERT(wi);

    Rect_SetWidthHeight(&wi->geometry(), 0, 0);

    if(!Hu_InventoryIsOpen(wi->player())) return;
    if(ST_AutomapIsOpen(wi->player()) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player()].plr->mo) && Get(DD_PLAYBACK)) return;

    // @todo calculate dimensions properly!
    Rect_SetWidthHeight(&wi->geometry(), (ST_WIDTH-(43*2)) * cfg.common.statusbarScale,
                                         41 * cfg.common.statusbarScale);
}

void Inventory_Drawer(HudWidget *wi, const Point2Raw *offset)
{
#define INVENTORY_HEIGHT    ( 29 )
#define EXTRA_SCALE         ( 0.75 )

    DE_ASSERT(wi);
    const float textOpacity = ::uiRendState->pageAlpha * ::cfg.common.hudColor[3];
    const float iconOpacity = ::uiRendState->pageAlpha * ::cfg.common.hudIconAlpha;

    if(!Hu_InventoryIsOpen(wi->player())) return;
    if(ST_AutomapIsOpen(wi->player()) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player()].plr->mo) && Get(DD_PLAYBACK)) return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(EXTRA_SCALE * cfg.common.hudScale, EXTRA_SCALE * cfg.common.hudScale, 1);

    Hu_InventoryDraw(wi->player(), 0, -INVENTORY_HEIGHT, textOpacity, iconOpacity);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef EXTRA_SCALE
#undef INVENTORY_HEIGHT
}

void Inventory_UpdateGeometry(HudWidget *wi)
{
#define INVENTORY_HEIGHT    29
#define EXTRA_SCALE         .75f

    DE_ASSERT(wi);

    Rect_SetWidthHeight(&wi->geometry(), 0, 0);

    if(!Hu_InventoryIsOpen(wi->player())) return;
    if(ST_AutomapIsOpen(wi->player()) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player()].plr->mo) && Get(DD_PLAYBACK)) return;

    Rect_SetWidthHeight(&wi->geometry(), (31 * 7 + 16 * 2) * EXTRA_SCALE * cfg.common.hudScale,
                                         INVENTORY_HEIGHT  * EXTRA_SCALE * cfg.common.hudScale);

#undef EXTRA_SCALE
#undef INVENTORY_HEIGHT
}

int ST_ActiveHud(int /*player*/)
{
    return (cfg.common.screenBlocks < 10? 0 : cfg.common.screenBlocks - 10);
}

void ST_HUDUnHide(int localPlayer, hueevent_t ev)
{
    if(localPlayer < 0 || localPlayer >= MAXPLAYERS)
        return;

    if(ev < HUE_FORCE || ev > NUMHUDUNHIDEEVENTS)
    {
        DE_ASSERT_FAIL("ST_HUDUnHide: Invalid event type");
        return;
    }

    player_t *plr = &players[localPlayer];
    if(!plr->plr->inGame) return;

    if(ev == HUE_FORCE || cfg.hudUnHide[ev])
    {
        hudStates[localPlayer].hideTics = (cfg.common.hudTimer * TICSPERSEC);
        hudStates[localPlayer].hideAmount = 0;
    }
}

void ST_loadGraphics()
{
    pStatusBar    = R_DeclarePatch("H2BAR");
    pStatusBarTop = R_DeclarePatch("H2TOP");
    pInventoryBar = R_DeclarePatch("INVBAR");
    pStatBar      = R_DeclarePatch("STATBAR");
    pKeyBar       = R_DeclarePatch("KEYBAR");
    pKills        = R_DeclarePatch("KILLS");
    pWeaponSlot[PCLASS_FIGHTER] = R_DeclarePatch("WPSLOT0");
    pWeaponSlot[PCLASS_CLERIC ] = R_DeclarePatch("WPSLOT1");
    pWeaponSlot[PCLASS_MAGE   ] = R_DeclarePatch("WPSLOT2");

    guidata_chain_t::prepareAssets();
    guidata_flight_t::prepareAssets();
    guidata_keys_t::prepareAssets();
    guidata_readyitem_t::prepareAssets();
    guidata_weaponpieces_t::prepareAssets();

    guidata_armoricons_t::prepareAssets();
    guidata_boots_t::prepareAssets();
    guidata_bluemanaicon_t::prepareAssets();
    guidata_bluemanavial_t::prepareAssets();
    guidata_defense_t::prepareAssets();
    guidata_greenmanaicon_t::prepareAssets();
    guidata_greenmanavial_t::prepareAssets();
    guidata_servant_t::prepareAssets();
}

void ST_loadData()
{
    ST_loadGraphics();
}

static void initData(hudstate_t *hud)
{
    DE_ASSERT(hud);
    const int player = hud - hudStates;

    hud->statusbarActive = true;
    hud->stopped = true;
    hud->showBar = 1;
    hud->readyItemFlashCounter = 0;

    // Statusbar:
    GUI_FindWidgetById(hud->sbarHealthId).as<guidata_health_t>().reset();
    GUI_FindWidgetById(hud->sbarWeaponpiecesId).as<guidata_weaponpieces_t>().reset();
    GUI_FindWidgetById(hud->sbarFragsId).as<guidata_frags_t>().reset();
    GUI_FindWidgetById(hud->sbarArmorId).as<guidata_armor_t>().reset();
    GUI_FindWidgetById(hud->sbarChainId).as<guidata_chain_t>().reset();
    GUI_FindWidgetById(hud->sbarBluemanaiconId).as<guidata_bluemanaicon_t>().reset();
    GUI_FindWidgetById(hud->sbarBluemanaId).as<guidata_bluemana_t>().reset();
    GUI_FindWidgetById(hud->sbarBluemanavialId).as<guidata_bluemanavial_t>().reset();
    GUI_FindWidgetById(hud->sbarGreenmanaiconId).as<guidata_greenmanaicon_t>().reset();
    GUI_FindWidgetById(hud->sbarGreenmanaId).as<guidata_greenmana_t>().reset();
    GUI_FindWidgetById(hud->sbarGreenmanavialId).as<guidata_greenmanavial_t>().reset();
    GUI_FindWidgetById(hud->sbarReadyitemId).as<guidata_readyitem_t>().reset();
    GUI_FindWidgetById(hud->sbarKeysId).as<guidata_keys_t>().reset();
    GUI_FindWidgetById(hud->sbarArmoriconsId).as<guidata_armoricons_t>().reset();

    // Fullscreen:
    GUI_FindWidgetById(hud->healthId).as<guidata_health_t>().reset();
    GUI_FindWidgetById(hud->fragsId).as<guidata_frags_t>().reset();
    GUI_FindWidgetById(hud->bluemanaiconId).as<guidata_bluemanaicon_t>().reset();
    GUI_FindWidgetById(hud->bluemanaId).as<guidata_bluemana_t>().reset();
    GUI_FindWidgetById(hud->greenmanaiconId).as<guidata_greenmanaicon_t>().reset();
    GUI_FindWidgetById(hud->greenmanaId).as<guidata_greenmana_t>().reset();
    GUI_FindWidgetById(hud->readyitemId).as<guidata_readyitem_t>().reset();

    // Other:
    GUI_FindWidgetById(hud->flightId).as<guidata_flight_t>().reset();
    GUI_FindWidgetById(hud->bootsId).as<guidata_boots_t>().reset();
    GUI_FindWidgetById(hud->servantId).as<guidata_servant_t>().reset();
    GUI_FindWidgetById(hud->defenseId).as<guidata_defense_t>().reset();
    GUI_FindWidgetById(hud->worldtimeId).as<guidata_worldtime_t>().reset();

    GUI_FindWidgetById(hud->logId).as<PlayerLogWidget>().clear();

    ST_HUDUnHide(player, HUE_FORCE);
}

static void setAutomapCheatLevel(AutomapWidget &automap, int level)
{
    hudstate_t *hud = &hudStates[automap.player()];
    hud->automapCheatLevel = level;

    int flags = automap.flags() & ~(AWF_SHOW_ALLLINES|AWF_SHOW_THINGS|AWF_SHOW_SPECIALLINES|AWF_SHOW_VERTEXES|AWF_SHOW_LINE_NORMALS);
    if(hud->automapCheatLevel >= 1)
        flags |= AWF_SHOW_ALLLINES;
    if(hud->automapCheatLevel == 2)
        flags |= AWF_SHOW_THINGS | AWF_SHOW_SPECIALLINES;
    if(hud->automapCheatLevel > 2)
        flags |= (AWF_SHOW_VERTEXES | AWF_SHOW_LINE_NORMALS);
    automap.setFlags(flags);
}

static void initAutomapForCurrentMap(AutomapWidget &automap)
{
#ifdef __JDOOM__
    hudstate_t *hud = &hudStates[automap.player()];
#endif

    automap.reset();

    const AABoxd *mapBounds = reinterpret_cast<AABoxd *>(DD_GetVariable(DD_MAP_BOUNDING_BOX));
    automap.setMapBounds(mapBounds->minX, mapBounds->maxX, mapBounds->minY, mapBounds->maxY);

#if __JDOOM__
    automapcfg_t *style = automap.style();
#endif

    // Determine the wi view scale factors.
    if(automap.cameraZoomMode())
        automap.setScale(0);

    automap.clearAllPoints(true/*silent*/);

#if !__JHEXEN__
    if(gameRules.skill == SM_BABY && cfg.common.automapBabyKeys)
    {
        automap.setFlags(automap.flags() | AWF_SHOW_KEYS);
    }
#endif

#if __JDOOM__
    if(!IS_NETGAME && hud->automapCheatLevel)
        AM_SetVectorGraphic(style, AMO_THINGPLAYER, VG_CHEATARROW);
#endif

    // Are we re-centering on a followed mobj?
    if (mobj_t *mob = automap.followMobj())
    {
        automap.setCameraOrigin(Vec2d(mob->origin), true);
    }

    if(IS_NETGAME)
    {
        setAutomapCheatLevel(automap, 0);
    }

    automap.reveal(false);

    // Add all immediately visible lines.
    for(int i = 0; i < numlines; ++i)
    {
        xline_t *xline = &xlines[i];
        if(!(xline->flags & ML_MAPPED)) continue;

        P_SetLineAutomapVisibility(automap.player(), i, true);
    }
}

void ST_Start(int localPlayer)
{
    if(localPlayer < 0 || localPlayer >= MAXPLAYERS)
    {
        Con_Error("ST_Start: Invalid player #%i.", localPlayer);
        exit(1); // Unreachable.
    }
    hudstate_t *hud = &hudStates[localPlayer];

    if(!hud->stopped)
    {
        ST_Stop(localPlayer);
    }

    initData(hud);

    // Initialize widgets according to player preferences.

    HudWidget &tcGroup = GUI_FindWidgetById(hud->groupIds[UWG_TOPCENTER]);
    int flags = tcGroup.alignment();
    flags &= ~(ALIGN_LEFT|ALIGN_RIGHT);
    if(cfg.common.msgAlign == 0)
        flags |= ALIGN_LEFT;
    else if(cfg.common.msgAlign == 2)
        flags |= ALIGN_RIGHT;
    tcGroup.setAlignment(flags);

    AutomapWidget &automap = GUI_FindWidgetById(hud->automapId).as<AutomapWidget>();
    // If the automap was left open; close it.
    automap.open(false, true /*instantly*/);
    initAutomapForCurrentMap(automap);

    hud->stopped = false;
}

void ST_Stop(int localPlayer)
{
    if(localPlayer < 0 || localPlayer >= MAXPLAYERS)
        return;

    hudstate_t *hud = &hudStates[localPlayer];
    hud->stopped = true;
}

static HudWidget *makeGroupWidget(int groupFlags, int localPlayer, int alignFlags, order_t order, int padding)
{
    auto *grp = new GroupWidget(localPlayer);
    grp->setAlignment(alignFlags)
        .setFont(1);

    grp->setFlags(groupFlags);
    grp->setOrder(order);
    grp->setPadding(padding);

    return grp;
}

void ST_BuildWidgets(int localPlayer)
{
#define PADDING             (2) /// Units in fixed 320x200 screen space.

struct uiwidgetgroupdef_t
{
    int group;
    int alignFlags;
    order_t order;
    int groupFlags;
    int padding; // In fixed 320x200 pixels.
};

struct uiwidgetdef_t
{
    HudElementName type;
    int alignFlags;
    int group;
    gamefontid_t fontIdx;
    void (*updateGeometry) (HudWidget *wi);
    void (*drawer) (HudWidget *wi, const Point2Raw *offset);
    uiwidgetid_t *id;
};

    hudstate_t *hud = &hudStates[localPlayer];
    uiwidgetgroupdef_t const widgetGroupDefs[] = {
        { UWG_STATUSBAR,    ALIGN_BOTTOM },
        { UWG_MAPNAME,      ALIGN_BOTTOMLEFT },
        { UWG_BOTTOMLEFT,   ALIGN_BOTTOMLEFT,  ORDER_LEFTTORIGHT, 0, PADDING },
        { UWG_BOTTOMRIGHT,  ALIGN_BOTTOMRIGHT, ORDER_RIGHTTOLEFT, 0, PADDING },
        { UWG_BOTTOMCENTER, ALIGN_BOTTOM,      ORDER_RIGHTTOLEFT, UWGF_VERTICAL, PADDING },
        { UWG_BOTTOM,       ALIGN_BOTTOM,      ORDER_LEFTTORIGHT },
        { UWG_TOP,          ALIGN_TOPLEFT,     ORDER_LEFTTORIGHT },
        { UWG_TOPCENTER,    ALIGN_TOP,         ORDER_LEFTTORIGHT, UWGF_VERTICAL, PADDING },
        { UWG_TOPLEFT,      ALIGN_TOPLEFT,     ORDER_LEFTTORIGHT, 0, PADDING },
        { UWG_TOPLEFT2,     ALIGN_TOPLEFT,     ORDER_LEFTTORIGHT, 0, PADDING },
        { UWG_TOPLEFT3,     ALIGN_TOPLEFT,     ORDER_LEFTTORIGHT, 0, PADDING },
        { UWG_TOPRIGHT,     ALIGN_TOPRIGHT,    ORDER_RIGHTTOLEFT, 0, PADDING },
        { UWG_AUTOMAP,      ALIGN_TOPLEFT }
    };
    uiwidgetdef_t const widgetDefs[] = {
        { GUI_BOX,          ALIGN_TOPLEFT,    UWG_STATUSBAR,    GF_NONE,      function_cast<UpdateGeometryFunc>(SBarBackground_UpdateGeometry), function_cast<DrawFunc>(SBarBackground_Drawer) },
        { GUI_WEAPONPIECES, ALIGN_TOPLEFT,    UWG_STATUSBAR,    GF_NONE,      nullptr, nullptr, &hud->sbarWeaponpiecesId },
        { GUI_CHAIN,        ALIGN_TOPLEFT,    UWG_STATUSBAR,    GF_NONE,      nullptr, nullptr, &hud->sbarChainId },
        { GUI_INVENTORY,    ALIGN_TOPLEFT,    UWG_STATUSBAR,    GF_SMALLIN,   function_cast<UpdateGeometryFunc>(SBarInventory_UpdateGeometry), function_cast<DrawFunc>(SBarInventory_Drawer) },
        { GUI_KEYS,         ALIGN_TOPLEFT,    UWG_STATUSBAR,    GF_NONE,      nullptr, nullptr, &hud->sbarKeysId },
        { GUI_ARMORICONS,   ALIGN_TOPLEFT,    UWG_STATUSBAR,    GF_NONE,      nullptr, nullptr, &hud->sbarArmoriconsId },
        { GUI_FRAGS,        ALIGN_TOPLEFT,    UWG_STATUSBAR,    GF_STATUS,    function_cast<UpdateGeometryFunc>(SBarFragsWidget_UpdateGeometry), function_cast<DrawFunc>(SBarFragsWidget_Draw), &hud->sbarFragsId },
        { GUI_HEALTH,       ALIGN_TOPLEFT,    UWG_STATUSBAR,    GF_STATUS,    function_cast<UpdateGeometryFunc>(SBarHealthWidget_UpdateGeometry), function_cast<DrawFunc>(SBarHealthWidget_Draw), &hud->sbarHealthId },
        { GUI_ARMOR,        ALIGN_TOPLEFT,    UWG_STATUSBAR,    GF_STATUS,    function_cast<UpdateGeometryFunc>(SBarArmor_UpdateGeometry), function_cast<DrawFunc>(SBarArmorWidget_Draw), &hud->sbarArmorId },
        { GUI_READYITEM,    ALIGN_TOPLEFT,    UWG_STATUSBAR,    GF_SMALLIN,   function_cast<UpdateGeometryFunc>(SBarReadyItem_UpdateGeometry), function_cast<DrawFunc>(SBarReadyItem_Drawer), &hud->sbarReadyitemId },
        { GUI_BLUEMANAICON, ALIGN_TOPLEFT,    UWG_STATUSBAR,    GF_NONE,      function_cast<UpdateGeometryFunc>(SBarBlueManaIconWidget_UpdateGeometry), function_cast<DrawFunc>(SBarBlueManaIconWidget_Draw), &hud->sbarBluemanaiconId },
        { GUI_BLUEMANA,     ALIGN_TOPLEFT,    UWG_STATUSBAR,    GF_SMALLIN,   function_cast<UpdateGeometryFunc>(SBarBlueManaWidget_UpdateGeometry), function_cast<DrawFunc>(SBarBlueManaWidget_Draw), &hud->sbarBluemanaId },
        { GUI_BLUEMANAVIAL, ALIGN_TOPLEFT,    UWG_STATUSBAR,    GF_NONE,      nullptr, nullptr, &hud->sbarBluemanavialId },
        { GUI_GREENMANAICON, ALIGN_TOPLEFT,   UWG_STATUSBAR,    GF_NONE,      function_cast<UpdateGeometryFunc>(SBarGreenManaIconWidget_UpdateGeometry), function_cast<DrawFunc>(SBarGreenManaIconWidget_Draw), &hud->sbarGreenmanaiconId },
        { GUI_GREENMANA,    ALIGN_TOPLEFT,    UWG_STATUSBAR,    GF_SMALLIN,   function_cast<UpdateGeometryFunc>(SBarGreenManaWidget_UpdateGeometry), function_cast<DrawFunc>(SBarGreenManaWidget_Draw), &hud->sbarGreenmanaId },
        { GUI_GREENMANAVIAL, ALIGN_TOPLEFT,   UWG_STATUSBAR,    GF_NONE,      nullptr, nullptr, &hud->sbarGreenmanavialId },
        { GUI_BLUEMANAICON, ALIGN_TOPLEFT,    UWG_TOPLEFT,      GF_NONE,      function_cast<UpdateGeometryFunc>(BlueManaIconWidget_UpdateGeometry), function_cast<DrawFunc>(BlueManaIconWidget_Draw), &hud->bluemanaiconId },
        { GUI_BLUEMANA,     ALIGN_TOPLEFT,    UWG_TOPLEFT,      GF_STATUS,    function_cast<UpdateGeometryFunc>(BlueMana_UpdateGeometry), function_cast<DrawFunc>(BlueManaWidget_Draw), &hud->bluemanaId },
        { GUI_GREENMANAICON, ALIGN_TOPLEFT,   UWG_TOPLEFT2,     GF_NONE,      function_cast<UpdateGeometryFunc>(GreenManaIconWidget_UpdateGeometry), function_cast<DrawFunc>(GreenManaIconWidget_Draw), &hud->greenmanaiconId },
        { GUI_GREENMANA,    ALIGN_TOPLEFT,    UWG_TOPLEFT2,     GF_STATUS,    function_cast<UpdateGeometryFunc>(GreenManaWidget_UpdateGeometry), function_cast<DrawFunc>(GreenManaWidget_Draw), &hud->greenmanaId },
        { GUI_FLIGHT,       ALIGN_TOPLEFT,    UWG_TOPLEFT3,     GF_NONE,      nullptr, nullptr, &hud->flightId },
        { GUI_BOOTS,        ALIGN_TOPLEFT,    UWG_TOPLEFT3,     GF_NONE,      nullptr, nullptr, &hud->bootsId },
        { GUI_SERVANT,      ALIGN_TOPRIGHT,   UWG_TOPRIGHT,     GF_NONE,      nullptr, nullptr, &hud->servantId },
        { GUI_DEFENSE,      ALIGN_TOPRIGHT,   UWG_TOPRIGHT,     GF_NONE,      nullptr, nullptr, &hud->defenseId },
        { GUI_WORLDTIME,    ALIGN_TOPRIGHT,   UWG_TOPRIGHT,     GF_FONTA,     nullptr, nullptr, &hud->worldtimeId },
        { GUI_HEALTH,       ALIGN_BOTTOMLEFT, UWG_BOTTOMLEFT,   GF_FONTB,     function_cast<UpdateGeometryFunc>(HealthWidget_UpdateGeometry), function_cast<DrawFunc>(HealthWidget_Draw), &hud->healthId },
        { GUI_FRAGS,        ALIGN_BOTTOMLEFT, UWG_BOTTOMLEFT,   GF_STATUS,    function_cast<UpdateGeometryFunc>(FragsWidget_UpdateGeometry), function_cast<DrawFunc>(FragsWidget_Draw), &hud->fragsId },
        { GUI_READYITEM,    ALIGN_BOTTOMRIGHT, UWG_BOTTOMRIGHT, GF_SMALLIN,   function_cast<UpdateGeometryFunc>(ReadyItem_UpdateGeometry), function_cast<DrawFunc>(ReadyItem_Drawer), &hud->readyitemId },
        { GUI_INVENTORY,    ALIGN_TOPLEFT,    UWG_BOTTOMCENTER, GF_SMALLIN,   function_cast<UpdateGeometryFunc>(Inventory_UpdateGeometry), function_cast<DrawFunc>(Inventory_Drawer) },
    };

    if(localPlayer < 0 || localPlayer >= MAXPLAYERS)
    {
        Con_Error("ST_BuildWidgets: Invalid localPlayer #%i.", localPlayer);
        exit(1); // Unreachable.
    }

    for(const uiwidgetgroupdef_t &def : widgetGroupDefs)
    {
        HudWidget *grp = makeGroupWidget(def.groupFlags, localPlayer, def.alignFlags, def.order, def.padding);
        GUI_AddWidget(grp);
        hud->groupIds[def.group] = grp->id();
    }

    for(const uiwidgetdef_t &def : widgetDefs)
    {
        HudWidget *wi = nullptr;
        switch(def.type)
        {
        case GUI_BOX:           wi = new HudWidget(def.updateGeometry, def.drawer, localPlayer); break;
        case GUI_HEALTH:        wi = new guidata_health_t(def.updateGeometry, def.drawer, localPlayer); break;
        case GUI_ARMOR:         wi = new guidata_armor_t(def.updateGeometry, def.drawer, localPlayer); break;
        case GUI_KEYS:          wi = new guidata_keys_t(localPlayer); break;
        case GUI_READYAMMO:     wi = new guidata_readyammo_t(def.updateGeometry, def.drawer, localPlayer); break;
        case GUI_FRAGS:         wi = new guidata_frags_t(def.updateGeometry, def.drawer, localPlayer); break;
        case GUI_ARMORICONS:    wi = new guidata_armoricons_t(localPlayer); break;
        case GUI_WEAPONPIECES:  wi = new guidata_weaponpieces_t(localPlayer); break;
        case GUI_BLUEMANAICON:  wi = new guidata_bluemanaicon_t(def.updateGeometry, def.drawer, localPlayer); break;
        case GUI_BLUEMANA:      wi = new guidata_bluemana_t(def.updateGeometry, def.drawer, localPlayer); break;
        case GUI_BLUEMANAVIAL:  wi = new guidata_bluemanavial_t(localPlayer); break;
        case GUI_GREENMANAICON: wi = new guidata_greenmanaicon_t(def.updateGeometry, def.drawer, localPlayer); break;
        case GUI_GREENMANA:     wi = new guidata_greenmana_t(def.updateGeometry, def.drawer, localPlayer); break;
        case GUI_GREENMANAVIAL: wi = new guidata_greenmanavial_t(localPlayer); break;
        case GUI_BOOTS:         wi = new guidata_boots_t(localPlayer); break;
        case GUI_SERVANT:       wi = new guidata_servant_t(localPlayer); break;
        case GUI_DEFENSE:       wi = new guidata_defense_t(localPlayer); break;
        case GUI_WORLDTIME:     wi = new guidata_worldtime_t(localPlayer); break;
        case GUI_INVENTORY:     wi = new HudWidget(def.updateGeometry, def.drawer, localPlayer); break;
        case GUI_CHAIN:         wi = new guidata_chain_t(localPlayer); break;
        case GUI_READYITEM:     wi = new guidata_readyitem_t(def.updateGeometry, def.drawer, localPlayer); break;
        case GUI_FLIGHT:        wi = new guidata_flight_t(localPlayer); break;
        //case GUI_AUTOMAP:       wi = new AutomapWidget(def.updateGeometry, def.drawer, localPlayer); break;

        default: DE_ASSERT_FAIL("Unknown widget type"); break;
        }

        wi->setAlignment(def.alignFlags)
           .setFont(FID(def.fontIdx));
        GUI_AddWidget(wi);
        GUI_FindWidgetById(hud->groupIds[def.group]).as<GroupWidget>()
                .addChild(wi);

        if(def.id) *def.id = wi->id();
    }

    GUI_FindWidgetById(hud->groupIds[UWG_BOTTOM]).as<GroupWidget>()
            .addChild(&GUI_FindWidgetById(hud->groupIds[UWG_BOTTOMLEFT]));
    GUI_FindWidgetById(hud->groupIds[UWG_BOTTOM]).as<GroupWidget>()
            .addChild(&GUI_FindWidgetById(hud->groupIds[UWG_BOTTOMCENTER]));
    GUI_FindWidgetById(hud->groupIds[UWG_BOTTOM]).as<GroupWidget>()
            .addChild(&GUI_FindWidgetById(hud->groupIds[UWG_BOTTOMRIGHT]));

    GUI_FindWidgetById(hud->groupIds[UWG_TOP]).as<GroupWidget>()
            .addChild(&GUI_FindWidgetById(hud->groupIds[UWG_TOPCENTER]));
    GUI_FindWidgetById(hud->groupIds[UWG_TOP]).as<GroupWidget>()
            .addChild(&GUI_FindWidgetById(hud->groupIds[UWG_TOPRIGHT]));

    auto *log = new PlayerLogWidget(localPlayer);
    log->setFont(FID(GF_FONTA));
    GUI_AddWidget(log);
    hud->logId = log->id();
    GUI_FindWidgetById(hud->groupIds[UWG_TOPCENTER]).as<GroupWidget>()
            .addChild(log);

    auto *chat = new ChatWidget(localPlayer);
    chat->setFont(FID(GF_FONTA));
    GUI_AddWidget(chat);
    hud->chatId = chat->id();
    GUI_FindWidgetById(hud->groupIds[UWG_TOPCENTER]).as<GroupWidget>()
            .addChild(chat);

    auto *automap = new AutomapWidget(localPlayer);
    automap->setFont(FID(GF_FONTA));
    automap->setCameraFollowPlayer(localPlayer);
    /// Set initial geometry size.
    /// @todo Should not be necessary...
    Rect_SetWidthHeight(&automap->geometry(), SCREENWIDTH, SCREENHEIGHT);
    GUI_AddWidget(automap);
    hud->automapId = automap->id();
    GUI_FindWidgetById(hud->groupIds[UWG_AUTOMAP]).as<GroupWidget>()
            .addChild(automap);

#undef PADDING
}

void ST_Init()
{
    ST_InitAutomapStyle();
    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        hudstate_t *hud = &hudStates[i];
        ST_BuildWidgets(i);
        hud->inited = true;
    }
    ST_loadData();
}

void ST_Shutdown()
{
    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        hudstate_t *hud = &hudStates[i];
        hud->inited = false;
    }
}

void HU_WakeWidgets(int localPlayer)
{
    if(localPlayer < 0)
    {
        // Wake the widgets of all players.
        for(uint i = 0; i < MAXPLAYERS; ++i)
        {
            if(!players[i].plr->inGame) continue;
            HU_WakeWidgets(i);
        }
        return;
    }
    if(localPlayer < MAXPLAYERS)
    {
        if(!players[localPlayer].plr->inGame) return;
        ST_Start(localPlayer);
    }
}

void ST_CloseAll(int localPlayer, dd_bool fast)
{
    NetSv_DismissHUDs(localPlayer, fast);

    ST_AutomapOpen(localPlayer, false, fast);
    Hu_InventoryOpen(localPlayer, false);
}

/// @note May be called prior to HUD init / outside game session.
AutomapWidget *ST_TryFindAutomapWidget(int localPlayer)
{
    if(localPlayer < 0 || localPlayer >= MAXPLAYERS) return nullptr;
    hudstate_t *hud = &hudStates[localPlayer];
    if(auto *wi = GUI_TryFindWidgetById(hud->automapId))
    {
        return maybeAs<AutomapWidget>(wi);
    }
    return nullptr;
}

/// @note May be called prior to HUD init / outside game session.
ChatWidget *ST_TryFindChatWidget(int localPlayer)
{
    if(localPlayer < 0 || localPlayer >= MAXPLAYERS) return nullptr;
    hudstate_t *hud = &hudStates[localPlayer];
    if(auto *wi = GUI_TryFindWidgetById(hud->chatId))
    {
        return maybeAs<ChatWidget>(wi);
    }
    return nullptr;
}

/// @note May be called prior to HUD init / outside game session.
PlayerLogWidget *ST_TryFindPlayerLogWidget(int localPlayer)
{
    if(localPlayer < 0 || localPlayer >= MAXPLAYERS) return nullptr;
    hudstate_t *hud = &hudStates[localPlayer];
    if(auto *wi = GUI_TryFindWidgetById(hud->logId))
    {
        return maybeAs<PlayerLogWidget>(wi);
    }
    return nullptr;
}

dd_bool ST_ChatIsActive(int localPlayer)
{
    if(auto *chat = ST_TryFindChatWidget(localPlayer))
    {
        return chat->isActive();
    }
    return false;
}

void ST_LogPost(int localPlayer, byte flags, const char *msg)
{
    if(auto *log = ST_TryFindPlayerLogWidget(localPlayer))
    {
        log->post(flags, msg);
    }
}

void ST_LogRefresh(int localPlayer)
{
    if(auto *log = ST_TryFindPlayerLogWidget(localPlayer))
    {
        log->refresh();
    }
}

void ST_LogEmpty(int localPlayer)
{
    if(auto *log = ST_TryFindPlayerLogWidget(localPlayer))
    {
        log->clear();
    }
}

void ST_LogUpdateAlignment()
{
    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        hudstate_t *hud = &hudStates[i];
        if(!hud->inited) continue;

        HudWidget &tcGroup = GUI_FindWidgetById(hud->groupIds[UWG_TOPCENTER]);
        int flags = tcGroup.alignment();
        flags &= ~(ALIGN_LEFT|ALIGN_RIGHT);
        if(cfg.common.msgAlign == 0)
            flags |= ALIGN_LEFT;
        else if(cfg.common.msgAlign == 2)
            flags |= ALIGN_RIGHT;
        tcGroup.setAlignment(flags);
    }
}

void ST_AutomapOpen(int localPlayer, dd_bool yes, dd_bool instantly)
{
    if(auto *automap = ST_TryFindAutomapWidget(localPlayer))
    {
        automap->open(CPP_BOOL(yes), CPP_BOOL(instantly));
    }
}

dd_bool ST_AutomapIsOpen(int localPlayer)
{
    if(auto *automap = ST_TryFindAutomapWidget(localPlayer))
    {
        return automap->isOpen();
    }
    return false;
}

dd_bool ST_AutomapObscures2(int localPlayer, const RectRaw * /*region*/)
{
    AutomapWidget *automap = ST_TryFindAutomapWidget(localPlayer);
    if(!automap) return false;

    if(automap->isOpen())
    {
        if(cfg.common.automapOpacity * ST_AutomapOpacity(localPlayer) >= ST_AUTOMAP_OBSCURE_TOLERANCE)
        {
            /*if(AutomapWidget_Fullscreen(wi))
            {*/
                return true;
            /*}
            else
            {
                // We'll have to compare the dimensions.
                const int scrwidth  = Get(DD_WINDOW_WIDTH);
                const int scrheight = Get(DD_WINDOW_HEIGHT);

                const Rect *rect = UIWidget_Geometry(automap);
                float fx = FIXXTOSCREENX(region->origin.x);
                float fy = FIXYTOSCREENY(region->origin.y);
                float fw = FIXXTOSCREENX(region->size.width);
                float fh = FIXYTOSCREENY(region->size.height);

                if(dims->origin.x >= fx && dims->origin.y >= fy && dims->size.width >= fw && dims->size.height >= fh)
                    return true;
            }*/
        }
    }
    return false;
}

dd_bool ST_AutomapObscures(int localPlayer, int x, int y, int width, int height)
{
    RectRaw rect;
    rect.origin.x = x;
    rect.origin.y = y;
    rect.size.width  = width;
    rect.size.height = height;
    return ST_AutomapObscures2(localPlayer, &rect);
}

void ST_AutomapClearPoints(int localPlayer)
{
    if(auto *automap = ST_TryFindAutomapWidget(localPlayer))
    {
        automap->clearAllPoints();
    }
}

int ST_AutomapAddPoint(int localPlayer, coord_t x, coord_t y, coord_t z)
{
    if(auto *automap = ST_TryFindAutomapWidget(localPlayer))
    {
        return automap->addPoint(Vec3d(x, y, z));
    }
    return -1;
}

void ST_AutomapZoomMode(int localPlayer)
{
    if(auto *automap = ST_TryFindAutomapWidget(localPlayer))
    {
        automap->setCameraZoomMode(!automap->cameraZoomMode());
    }
}

float ST_AutomapOpacity(int localPlayer)
{
    if(auto *automap = ST_TryFindAutomapWidget(localPlayer))
    {
        return automap->opacityEX();
    }
    return 0;
}

void ST_SetAutomapCameraRotation(int localPlayer, dd_bool yes)
{
    if(auto *autmap = ST_TryFindAutomapWidget(localPlayer))
    {
        autmap->setCameraRotationMode(CPP_BOOL(yes));
    }
}

void ST_AutomapFollowMode(int localPlayer)
{
    if(auto *automap = ST_TryFindAutomapWidget(localPlayer))
    {
        automap->setCameraFollowMode(!automap->cameraFollowMode());
    }
}

void ST_CycleAutomapCheatLevel(int localPlayer)
{
    if(localPlayer >= 0 && localPlayer < MAXPLAYERS)
    {
        hudstate_t *hud = &hudStates[localPlayer];
        ST_SetAutomapCheatLevel(localPlayer, (hud->automapCheatLevel + 1) % 3);
    }
}

void ST_SetAutomapCheatLevel(int localPlayer, int level)
{
    if(auto *automap = ST_TryFindAutomapWidget(localPlayer))
    {
        setAutomapCheatLevel(*automap, level);
    }
}

void ST_RevealAutomap(int localPlayer, dd_bool on)
{
    if(auto *automap = ST_TryFindAutomapWidget(localPlayer))
    {
        automap->reveal(on);
    }
}

dd_bool ST_AutomapIsRevealed(int localPlayer)
{
    if(auto *automap = ST_TryFindAutomapWidget(localPlayer))
    {
        return automap->isRevealed();
    }
    return false;
}

int ST_AutomapCheatLevel(int localPlayer)
{
    if(localPlayer >= 0 && localPlayer < MAXPLAYERS)
    {
        return hudStates[localPlayer].automapCheatLevel;
    }
    return 0;
}

void ST_FlashCurrentItem(int localPlayer)
{
    if(localPlayer < 0 || localPlayer >= MAXPLAYERS) return;

    player_t *plr = &players[localPlayer];
    if(!(/*(plr->plr->flags & DDPF_LOCAL) &&*/ plr->plr->inGame)) return;

    hudstate_t *hud = &hudStates[localPlayer];
    hud->readyItemFlashCounter = HUD_ITEM_FLASH_TICS;
}

int ST_ReadyItemFlashCounter(int localPlayer)
{
    if(localPlayer < 0 || localPlayer >= MAXPLAYERS) return 0;
    const hudstate_t *hud = &hudStates[localPlayer];
    return hud->readyItemFlashCounter;
}

int ST_Responder(event_t *ev)
{
    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        if(auto *chat = ST_TryFindChatWidget(i))
        {
            if(int eaten = chat->handleEvent(*ev))
                return eaten;
        }
    }
    return false;
}

void ST_Ticker(timespan_t ticLength)
{
    const dd_bool isSharpTic = DD_IsSharpTick();

    if(isSharpTic)
        Hu_InventoryTicker();

    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        player_t *plr   = &players[i];
        hudstate_t *hud = &hudStates[i];

        if(!plr->plr->inGame)
            continue;

        // Either slide the statusbar in or fade out the fullscreen HUD.
        if(hud->statusbarActive)
        {
            if(hud->alpha > 0.0f)
            {
                hud->alpha -= 0.1f;
            }
            else if(hud->showBar < 1.0f)
            {
                hud->showBar += 0.1f;
            }
        }
        else
        {
            if(cfg.common.screenBlocks == 13)
            {
                if(hud->alpha > 0.0f)
                {
                    hud->alpha -= 0.1f;
                }
            }
            else
            {
                if(hud->showBar > 0.0f)
                {
                    hud->showBar -= 0.1f;
                }
                else if(hud->alpha < 1.0f)
                {
                    hud->alpha += 0.1f;
                }
            }
        }

        // The following is restricted to fixed 35 Hz ticks.
        if(isSharpTic && !Pause_IsPaused())
        {
            if(cfg.common.hudTimer == 0)
            {
                hud->hideTics = hud->hideAmount = 0;
            }
            else
            {
                if(hud->hideTics > 0)
                    hud->hideTics--;
                if(hud->hideTics == 0 && cfg.common.hudTimer > 0 && hud->hideAmount < 1)
                    hud->hideAmount += 0.1f;
            }

            if(hud->readyItemFlashCounter > 0)
                --hud->readyItemFlashCounter;
        }

        if(hud->inited)
        {
            for(int k = 0; k < NUM_UIWIDGET_GROUPS; ++k)
            {
                GUI_FindWidgetById(hud->groupIds[k]).tick(ticLength);
            }
        }
    }
}

static void drawUIWidgetsForPlayer(player_t *plr)
{
    DE_ASSERT(plr);

/// Units in fixed 320x200 screen space.
#define DISPLAY_BORDER      (2)
#define PADDING             (2)

    const int playerNum   = plr - players;
    const int displayMode = ST_ActiveHud(playerNum);
    hudstate_t *hud       = &hudStates[playerNum];

    Size2Raw portSize;    R_ViewPortSize(playerNum, &portSize);
    Point2Raw portOrigin; R_ViewPortOrigin(playerNum, &portOrigin);

    // The automap is drawn in a viewport scaled coordinate space (of viewwindow dimensions).
    HudWidget &amGroup = GUI_FindWidgetById(hud->groupIds[UWG_AUTOMAP]);
    amGroup.setOpacity(ST_AutomapOpacity(playerNum));
    amGroup.setMaximumSize(portSize);
    GUI_DrawWidgetXY(&amGroup, 0, 0);

    // The rest of the UI is drawn in a fixed 320x200 coordinate space.
    // Determine scale factors.
    float scale;
    R_ChooseAlignModeAndScaleFactor(&scale, SCREENWIDTH, SCREENHEIGHT,
        portSize.width, portSize.height, SCALEMODE_SMART_STRETCH);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(portOrigin.x, portOrigin.y, 0);
    DGL_Scalef(scale, scale, 1);

    if(hud->statusbarActive || (displayMode < 3 || hud->alpha > 0))
    {
        float opacity = /**@todo Kludge: clamp*/MIN_OF(1.0f, hud->alpha)/**kludge end*/ * (1-hud->hideAmount);
        Size2Raw drawnSize = { 0, 0 };
        RectRaw displayRegion;
        int posX, posY, availWidth, availHeight;

        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_Scalef(1, 1.2f/*aspect correct*/, 1);

        displayRegion.origin.x = displayRegion.origin.y = 0;
        displayRegion.size.width  = .5f + portSize.width  / scale;
        displayRegion.size.height = .5f + portSize.height / (scale * 1.2f /*aspect correct*/);

        if(hud->statusbarActive)
        {
            const float statusbarOpacity = (1 - hud->hideAmount) * hud->showBar;

            HudWidget &sbGroup = GUI_FindWidgetById(hud->groupIds[UWG_STATUSBAR]);
            sbGroup.setOpacity(statusbarOpacity);
            sbGroup.setMaximumSize(displayRegion.size);

            GUI_DrawWidget(&sbGroup, &displayRegion.origin);

            Size2_Raw(Rect_Size(&sbGroup.geometry()), &drawnSize);
        }

        displayRegion.origin.x += DISPLAY_BORDER;
        displayRegion.origin.y += DISPLAY_BORDER;
        displayRegion.size.width  -= DISPLAY_BORDER*2;
        displayRegion.size.height -= DISPLAY_BORDER*2;

        if(!hud->statusbarActive)
        {
            HudWidget &bGroup = GUI_FindWidgetById(hud->groupIds[UWG_BOTTOM]);
            bGroup.setOpacity(opacity);
            bGroup.setMaximumSize(displayRegion.size);

            GUI_DrawWidget(&bGroup, &displayRegion.origin);

            Size2_Raw(Rect_Size(&bGroup.geometry()), &drawnSize);
        }

        availHeight = displayRegion.size.height - (drawnSize.height > 0 ? drawnSize.height : 0);
        HudWidget &mnGroup = GUI_FindWidgetById(hud->groupIds[UWG_MAPNAME]);
        mnGroup.setOpacity(ST_AutomapOpacity(playerNum));
        Size2Raw size = {{{displayRegion.size.width, availHeight}}};
        mnGroup.setMaximumSize(size);

        GUI_DrawWidget(&mnGroup, &displayRegion.origin);

        // The other displays are always visible except when using the "no-hud" mode.
        if(hud->statusbarActive || displayMode < 3)
            opacity = 1.0f;

        HudWidget &tGroup = GUI_FindWidgetById(hud->groupIds[UWG_TOP]);
        tGroup.setOpacity(opacity);
        tGroup.setMaximumSize(displayRegion.size);

        GUI_DrawWidget(&tGroup, &displayRegion.origin);

        Size2_Raw(Rect_Size(&tGroup.geometry()), &drawnSize);

        if(!hud->statusbarActive)
        {
            Size2Raw tlDrawnSize;

            HudWidget &tlGroup = GUI_FindWidgetById(hud->groupIds[UWG_TOPLEFT]);
            tlGroup.setOpacity(opacity);
            tlGroup.setMaximumSize(displayRegion.size);

            GUI_DrawWidget(&tlGroup, &displayRegion.origin);

            Size2_Raw(Rect_Size(&tlGroup.geometry()), &drawnSize);
            posY = displayRegion.origin.y + (drawnSize.height > 0 ? drawnSize.height + PADDING : 0);

            HudWidget &tl2Group = GUI_FindWidgetById(hud->groupIds[UWG_TOPLEFT2]);
            tl2Group.setOpacity(opacity);
            tl2Group.setMaximumSize(displayRegion.size);

            GUI_DrawWidgetXY(&tl2Group, displayRegion.origin.x, posY);

            Size2_Raw(Rect_Size(&tl2Group.geometry()), &tlDrawnSize);
            if(tlDrawnSize.width > drawnSize.width)
                drawnSize.width = tlDrawnSize.width;
        }
        else
        {
            drawnSize.width = 0;
        }

        posX = displayRegion.origin.x + (drawnSize.width > 0 ? drawnSize.width + PADDING : 0);
        availWidth = displayRegion.size.width - (drawnSize.width > 0 ? drawnSize.width + PADDING : 0);
        HudWidget &tl3Group = GUI_FindWidgetById(hud->groupIds[UWG_TOPLEFT3]);
        tl3Group.setOpacity(opacity);
        size.width = availWidth; size.height = displayRegion.size.height;
        tl3Group.setMaximumSize(size);

        GUI_DrawWidgetXY(&tl3Group, posX, displayRegion.origin.y);
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef PADDING
#undef DISPLAY_BORDER
}

void ST_Drawer(int localPlayer)
{
    if(localPlayer < 0 || localPlayer >= MAXPLAYERS)
        return;

    if(!players[localPlayer].plr->inGame) return;

    R_UpdateViewFilter(localPlayer);

    hudstate_t *hud = &hudStates[localPlayer];
    hud->statusbarActive = (ST_ActiveHud(localPlayer) < 2) || (ST_AutomapIsOpen(localPlayer) && (cfg.common.automapHudDisplay == 0 || cfg.common.automapHudDisplay == 2));

    drawUIWidgetsForPlayer(&players[localPlayer]);
}

dd_bool ST_StatusBarIsActive(int localPlayer)
{
    DE_ASSERT(localPlayer >= 0 && localPlayer < MAXPLAYERS);

    if(!players[localPlayer].plr->inGame) return false;

    return hudStates[localPlayer].statusbarActive;
}

float ST_StatusBarShown(int localPlayer)
{
    DE_ASSERT(localPlayer >= 0 && localPlayer < MAXPLAYERS);
    return hudStates[localPlayer].showBar;
}

/**
 * Called when the statusbar scale cvar changes.
 */
static void updateViewWindow()
{
    R_ResizeViewWindow(RWF_FORCE);
    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        ST_HUDUnHide(i, HUE_FORCE); // So the user can see the change.
    }
}

/**
 * Called when a cvar changes that affects the look/behavior of the HUD in order to unhide it.
 */
static void unhideHUD()
{
    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        ST_HUDUnHide(i, HUE_FORCE);
    }
}

/**
 * @return  Parsed chat macro identifier or @c -1 if invalid.
 */
static int parseMacroId(const String &str) // static
{
    if(!str.isEmpty())
    {
        bool isNumber = false;
        const int id  = str.toInt(&isNumber);
        if(isNumber && id >= 0 && id <= 9)
        {
            return id;
        }
    }
    return -1;
}

/**
 * @return  Parsed chat destination number from or @c -1 if invalid.
 */
static int parseTeamNumber(const String &str)
{
    if(!str.isEmpty())
    {
        bool isNumber = false;
        const int num = str.toInt(&isNumber);
        if(isNumber && num >= 0 && num <= NUMTEAMS)
        {
            return num;
        }
    }
    return -1;
}

D_CMD(ChatOpen)
{
    DE_UNUSED(src);

    if(G_QuitInProgress()) return false;

    ChatWidget *chat = ST_TryFindChatWidget(CONSOLEPLAYER);
    if(!chat) return false;

    int destination = 0;
    if(argc == 2)
    {
        destination = parseTeamNumber(argv[1]);
        if(destination < 0)
        {
            LOG_SCR_ERROR("Invalid team number #%i (valid range: 0..%i)") << destination << NUMTEAMS;
            return false;
        }
    }
    chat->setDestination(destination);
    chat->activate();
    return true;
}

D_CMD(ChatAction)
{
    DE_UNUSED(src, argc);

    if(G_QuitInProgress()) return false;

    ChatWidget *chat = ST_TryFindChatWidget(CONSOLEPLAYER);
    if(!chat || !chat->isActive()) return false;

    const auto cmd = String(argv[0] + 4);
    if(!cmd.compareWithoutCase("complete")) // Send the message.
    {
        return chat->handleMenuCommand(MCMD_SELECT);
    }
    if(!cmd.compareWithoutCase("cancel")) // Close chat.
    {
        return chat->handleMenuCommand(MCMD_CLOSE);
    }
    if(!cmd.compareWithoutCase("delete"))
    {
        return chat->handleMenuCommand(MCMD_DELETE);
    }
    return true;
}

D_CMD(ChatSendMacro)
{
    DE_UNUSED(src);

    if(G_QuitInProgress()) return false;

    if(argc < 2 || argc > 3)
    {
        LOG_SCR_NOTE("Usage: %s (team) (macro number)") << argv[0];
        LOG_SCR_MSG("Send a chat macro to other player(s). "
                    "If (team) is omitted, the message will be sent to all players.");
        return true;
    }

    ChatWidget *chat = ST_TryFindChatWidget(CONSOLEPLAYER);
    if(!chat) return false;

    int destination = 0;
    if(argc == 3)
    {
        destination = parseTeamNumber(argv[1]);
        if(destination < 0)
        {
            LOG_SCR_ERROR("Invalid team number #%i (valid range: 0..%i)") << destination << NUMTEAMS;
            return false;
        }
    }

    int macroId = parseMacroId(argc == 3? argv[2] : argv[1]);
    if(macroId < 0)
    {
        LOG_SCR_ERROR("Invalid macro id");
        return false;
    }

    chat->activate();
    chat->setDestination(destination);
    chat->messageAppendMacro(macroId);
    chat->handleMenuCommand(MCMD_SELECT);
    chat->activate(false);

    return true;
}

void ST_Register()
{
    C_VAR_FLOAT2( "hud-color-r",                &cfg.common.hudColor[0], 0, 0, 1, unhideHUD )
    C_VAR_FLOAT2( "hud-color-g",                &cfg.common.hudColor[1], 0, 0, 1, unhideHUD )
    C_VAR_FLOAT2( "hud-color-b",                &cfg.common.hudColor[2], 0, 0, 1, unhideHUD )
    C_VAR_FLOAT2( "hud-color-a",                &cfg.common.hudColor[3], 0, 0, 1, unhideHUD )
    C_VAR_FLOAT2( "hud-icon-alpha",             &cfg.common.hudIconAlpha, 0, 0, 1, unhideHUD )
    C_VAR_INT   ( "hud-patch-replacement",      &cfg.common.hudPatchReplaceMode, 0, 0, 1 )
    C_VAR_FLOAT2( "hud-scale",                  &cfg.common.hudScale, 0, 0.1f, 1, unhideHUD )
    C_VAR_FLOAT ( "hud-timer",                  &cfg.common.hudTimer, 0, 0, 60 )

    // Displays:
    C_VAR_BYTE2 ( "hud-currentitem",            &cfg.hudShown[HUD_READYITEM], 0, 0, 1, unhideHUD )
    C_VAR_BYTE2 ( "hud-health",                 &cfg.hudShown[HUD_HEALTH], 0, 0, 1, unhideHUD )
    C_VAR_BYTE2 ( "hud-mana",                   &cfg.hudShown[HUD_MANA], 0, 0, 1, unhideHUD )

    C_VAR_FLOAT2( "hud-status-alpha",           &cfg.common.statusbarOpacity, 0, 0, 1, unhideHUD )
    C_VAR_FLOAT2( "hud-status-icon-a",          &cfg.common.statusbarCounterAlpha, 0, 0, 1, unhideHUD )
    C_VAR_FLOAT2( "hud-status-size",            &cfg.common.statusbarScale, 0, 0.1f, 1, updateViewWindow )

    // Events:
    C_VAR_BYTE  ( "hud-unhide-damage",          &cfg.hudUnHide[HUE_ON_DAMAGE], 0, 0, 1 )
    C_VAR_BYTE  ( "hud-unhide-pickup-ammo",     &cfg.hudUnHide[HUE_ON_PICKUP_AMMO], 0, 0, 1 )
    C_VAR_BYTE  ( "hud-unhide-pickup-armor",    &cfg.hudUnHide[HUE_ON_PICKUP_ARMOR], 0, 0, 1 )
    C_VAR_BYTE  ( "hud-unhide-pickup-health",   &cfg.hudUnHide[HUE_ON_PICKUP_HEALTH], 0, 0, 1 )
    C_VAR_BYTE  ( "hud-unhide-pickup-invitem",  &cfg.hudUnHide[HUE_ON_PICKUP_INVITEM], 0, 0, 1 )
    C_VAR_BYTE  ( "hud-unhide-pickup-key",      &cfg.hudUnHide[HUE_ON_PICKUP_KEY], 0, 0, 1 )
    C_VAR_BYTE  ( "hud-unhide-pickup-powerup",  &cfg.hudUnHide[HUE_ON_PICKUP_POWER], 0, 0, 1 )
    C_VAR_BYTE  ( "hud-unhide-pickup-weapon",   &cfg.hudUnHide[HUE_ON_PICKUP_WEAPON], 0, 0, 1 )

    C_CMD("beginchat",       nullptr,   ChatOpen )
    C_CMD("chatcancel",      "",        ChatAction )
    C_CMD("chatcomplete",    "",        ChatAction )
    C_CMD("chatdelete",      "",        ChatAction )
    C_CMD("chatsendmacro",   nullptr,   ChatSendMacro )

    Hu_InventoryRegister();
}
