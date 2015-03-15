/** @file st_stuff.cpp  DOOM 64, player head-up display (HUD) management.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2003-2005 Samuel Villarreal <svkaiser@gmail.com>
 * @authors Copyright © 1993-1996 by id Software, Inc.
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

#include "jdoom64.h"
#include "st_stuff.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "d_net.h"
#include "dmu_lib.h"
#include "hu_lib.h"
#include "hu_stuff.h"
#include "hud/automapstyle.h"
#include "p_inventory.h"
#include "p_mapsetup.h"
#include "p_tick.h"       // for Pause_IsPaused
#include "player.h"
#include "r_common.h"

#include "hud/widgets/automapwidget.h"
#include "hud/widgets/chatwidget.h"
#include "hud/widgets/groupwidget.h"
#include "hud/widgets/playerlogwidget.h"

using namespace de;

enum {
    UWG_MAPNAME = 0,
    UWG_BOTTOMLEFT,
    UWG_BOTTOMRIGHT,
    UWG_BOTTOMCENTER,
    UWG_TOP,
    UWG_COUNTERS,
    UWG_AUTOMAP,
    NUM_UIWIDGET_GROUPS
};

struct hudstate_t
{
    dd_bool inited;
    dd_bool stopped;
    int hideTics;
    float hideAmount;
    float alpha;              ///< Fullscreen hud alpha value.
    dd_bool statusbarActive;  ///< Whether the HUD is on.
    int automapCheatLevel;    ///< @todo Belongs in player state?

    uiwidgetid_t groupIds[NUM_UIWIDGET_GROUPS];
    uiwidgetid_t automapId;
    uiwidgetid_t chatId;
    uiwidgetid_t logId;

    dd_bool firstTime;        ///< ST_Start() has just been called.
    int currentFragsCount;    ///< Number of frags so far in deathmatch.
};

static hudstate_t hudStates[MAXPLAYERS];

int ST_ActiveHud(int /*player*/)
{
    return (cfg.common.screenBlocks < 10? 0 : cfg.common.screenBlocks - 10);
}

void ST_HUDUnHide(int localPlayer, hueevent_t ev)
{
    DENG2_ASSERT(ev >= HUE_FORCE && ev < NUMHUDUNHIDEEVENTS);

    if(localPlayer < 0 || localPlayer >= MAXPLAYERS)
        return;

    player_t *plr = &players[localPlayer];
    if(!plr->plr->inGame) return;

    if(ev == HUE_FORCE || cfg.hudUnHide[ev])
    {
        hudStates[localPlayer].hideTics = (cfg.common.hudTimer * TICSPERSEC);
        hudStates[localPlayer].hideAmount = 0;
    }
}

static void updateWidgets(int localPlayer)
{
    player_t *plr   = &players[localPlayer];
    hudstate_t *hud = &hudStates[localPlayer];

    // Used by wFrags widget.
    hud->currentFragsCount = 0;

    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        if(!players[i].plr->inGame)
            continue;

        hud->currentFragsCount += plr->frags[i] * (i != localPlayer ? 1 : -1);
    }
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
    bool const isSharpTic = DD_IsSharpTick();
    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        player_t *plr   = &players[i];
        hudstate_t *hud = &hudStates[i];

        if(!plr->plr->inGame)
            continue;

        // Fade in/out the fullscreen HUD.
        if(hud->statusbarActive)
        {
            if(hud->alpha > 0.0f)
            {
                hud->statusbarActive = 0;
                hud->alpha-=0.1f;
            }
        }
        else
        {
            if(cfg.common.screenBlocks == 13)
            {
                if(hud->alpha > 0.0f)
                {
                    hud->alpha-=0.1f;
                }
            }
            else
            {
                if(hud->alpha < 1.0f)
                    hud->alpha += 0.1f;
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

            /// @todo Refactor away.
            updateWidgets(i);
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

static void drawWidgets(hudstate_t *hud)
{
#define X_OFFSET            ( 138 )
#define Y_OFFSET            ( 171 )
#define MAXDIGITS           ( 2 )

    if(G_Ruleset_Deathmatch())
    {
        if(hud->currentFragsCount == 1994)
            return;

        char buf[20]; dd_snprintf(buf, 20, "%i", hud->currentFragsCount);

        DGL_Enable(DGL_TEXTURE_2D);

        FR_SetFont(FID(GF_STATUS));
        FR_LoadDefaultAttrib();
        FR_SetColorAndAlpha(1, 1, 1, hud->alpha);

        FR_DrawTextXY3(buf, X_OFFSET, Y_OFFSET, ALIGN_TOPRIGHT, DTF_NO_EFFECTS);

        DGL_Disable(DGL_TEXTURE_2D);
    }

#undef MAXDIGITS
#undef Y_OFFSET
#undef X_OFFSET
}

void ST_doRefresh(int localPlayer)
{
    if(localPlayer < 0 || localPlayer > MAXPLAYERS) return;

    hudstate_t *hud = &hudStates[localPlayer];
    hud->firstTime = false;

    drawWidgets(hud);
}

void ST_doFullscreenStuff(int localPlayer)
{
    /*static int const ammo_sprite[NUM_AMMO_TYPES] = {
        SPR_AMMO,
        SPR_SBOX,
        SPR_CELL,
        SPR_RCKT
    };*/

    hudstate_t *hud = &hudStates[localPlayer];
    player_t *plr = &players[localPlayer];
    char buf[20];
    int w, h, pos = 0, oldPos = 0, spr,i;
    int h_width = 320 / cfg.common.hudScale;
    int h_height = 200 / cfg.common.hudScale;
    float textalpha = hud->alpha - hud->hideAmount - ( 1 - cfg.common.hudColor[3]);
    float iconalpha = hud->alpha - hud->hideAmount - ( 1 - cfg.common.hudIconAlpha);

    textalpha = MINMAX_OF(0.f, textalpha, 1.f);
    iconalpha = MINMAX_OF(0.f, iconalpha, 1.f);

    FR_LoadDefaultAttrib();

    if(IS_NETGAME && G_Ruleset_Deathmatch() && cfg.hudShown[HUD_FRAGS])
    {
        // Display the frag counter.
        i = 199 - HUDBORDERY;
        if(cfg.hudShown[HUD_HEALTH])
        {
            i -= 18 * cfg.common.hudScale;
        }

        sprintf(buf, "FRAGS:%i", hud->currentFragsCount);

        DGL_Enable(DGL_TEXTURE_2D);
        FR_SetFont(FID(GF_FONTA));
        FR_SetColorAndAlpha(cfg.common.hudColor[0], cfg.common.hudColor[1], cfg.common.hudColor[2], textalpha);

        FR_DrawTextXY3(buf, HUDBORDERX, i, ALIGN_TOPLEFT, DTF_NO_EFFECTS);

        DGL_Disable(DGL_TEXTURE_2D);
    }

    // Setup the scaling matrix.
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Scalef(cfg.common.hudScale, cfg.common.hudScale, 1);

    // Draw the visible HUD data, first health.
    if(cfg.hudShown[HUD_HEALTH])
    {

        sprintf(buf, "HEALTH");

        DGL_Enable(DGL_TEXTURE_2D);

        FR_SetFont(FID(GF_FONTA));
        FR_SetColorAndAlpha(1, 1, 1, iconalpha);

        pos = FR_TextWidth(buf)/2;
        FR_DrawTextXY3(buf, HUDBORDERX, h_height - HUDBORDERY - 4, ALIGN_BOTTOM, DTF_NO_EFFECTS);

        sprintf(buf, "%i", plr->health);

        FR_SetFont(FID(GF_FONTB));
        FR_SetColorAndAlpha(cfg.common.hudColor[0], cfg.common.hudColor[1], cfg.common.hudColor[2], textalpha);
        FR_DrawTextXY3(buf, HUDBORDERX + pos, h_height - HUDBORDERY, ALIGN_BOTTOM, DTF_NO_EFFECTS);

        DGL_Disable(DGL_TEXTURE_2D);

        oldPos = pos;
        pos = HUDBORDERX * 2 + FR_TextWidth(buf);
    }

    // Keys  | use a bit of extra scale.
    if(cfg.hudShown[HUD_KEYS])
    {
Draw_BeginZoom(0.75f, pos , h_height - HUDBORDERY);
        for(i = 0; i < 3; ++i)
        {
            spr = 0;
            if(plr->
               keys[i == 0 ? KT_REDCARD : i ==
                     1 ? KT_YELLOWCARD : KT_BLUECARD])
                spr = i == 0 ? SPR_RKEY : i == 1 ? SPR_YKEY : SPR_BKEY;
            if(plr->
               keys[i == 0 ? KT_REDSKULL : i ==
                     1 ? KT_YELLOWSKULL : KT_BLUESKULL])
                spr = i == 0 ? SPR_RSKU : i == 1 ? SPR_YSKU : SPR_BSKU;
            if(spr)
            {
                GUI_DrawSprite(spr, pos, h_height - 2, HOT_BLEFT, 1,
                    iconalpha, false, &w, &h);
                pos += w + 2;
            }
        }
Draw_EndZoom();
    }
    pos = oldPos;

    // Inventory
    if(cfg.hudShown[HUD_INVENTORY])
    {
        if(P_InventoryCount(localPlayer, IIT_DEMONKEY1))
        {
            spr = SPR_ART1;
            GUI_DrawSprite(spr, HUDBORDERX + pos -w/2, h_height - 44,
                HOT_BLEFT, 1, iconalpha, false, &w, &h);
        }

        if(P_InventoryCount(localPlayer, IIT_DEMONKEY2))
        {
            spr = SPR_ART2;
            GUI_DrawSprite(spr, HUDBORDERX + pos -w/2, h_height - 84,
                HOT_BLEFT, 1, iconalpha, false, &w, &h);
        }

        if(P_InventoryCount(localPlayer, IIT_DEMONKEY3))
        {
            spr = SPR_ART3;
            GUI_DrawSprite(spr, HUDBORDERX + pos -w/2, h_height - 124,
                HOT_BLEFT, 1, iconalpha, false, &w, &h);
        }
    }

    if(cfg.hudShown[HUD_AMMO])
    {
        /// @todo Only supports one type of ammo per weapon.
        for(int ammotype = 0; ammotype < NUM_AMMO_TYPES; ++ammotype)
        {
            if(!weaponInfo[plr->readyWeapon][plr->class_].mode[0].ammoType[ammotype])
                continue;

            sprintf(buf, "%i", plr->ammo[ammotype].owned);
            pos = h_width/2;

            DGL_Enable(DGL_TEXTURE_2D);

            FR_SetFont(FID(GF_FONTB));
            FR_SetColorAndAlpha(cfg.common.hudColor[0], cfg.common.hudColor[1], cfg.common.hudColor[2], textalpha);
            FR_DrawTextXY3(buf, pos, h_height - HUDBORDERY, ALIGN_TOP, DTF_NO_EFFECTS);

            DGL_Disable(DGL_TEXTURE_2D);
            break;
        }
    }

    pos = h_width - 1;
    if(cfg.hudShown[HUD_ARMOR])
    {
        sprintf(buf, "ARMOR");

        DGL_Enable(DGL_TEXTURE_2D);
        FR_SetFont(FID(GF_FONTA));
        FR_SetColorAndAlpha(1, 1, 1, iconalpha);
        w = FR_TextWidth(buf);
        FR_DrawTextXY3(buf, h_width - HUDBORDERX, h_height - HUDBORDERY - 4, ALIGN_BOTTOMRIGHT, DTF_NO_EFFECTS);

        sprintf(buf, "%i", plr->armorPoints);
        FR_SetFont(FID(GF_FONTB));
        FR_SetColorAndAlpha(cfg.common.hudColor[0], cfg.common.hudColor[1], cfg.common.hudColor[2], textalpha);
        FR_DrawTextXY3(buf, h_width - (w/2) - HUDBORDERX, h_height - HUDBORDERY, ALIGN_BOTTOMRIGHT, DTF_NO_EFFECTS);

        DGL_Disable(DGL_TEXTURE_2D);
    }

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

struct uiwidgetdef_t
{
    HudElementName type;
    int group;
    gamefontid_t fontIdx;
    void (*updateGeometry) (HudWidget* obj);
    void (*drawer) (HudWidget* obj, int x, int y);
    void (*ticker) (HudWidget* obj, timespan_t ticLength);
    uiwidgetid_t *id;
};

struct uiwidgetgroupdef_t
{
    int group;
    int alignFlags;
    int groupFlags;
    int padding; // In fixed 320x200 pixels.
};

static void drawUIWidgetsForPlayer(player_t *plr)
{
    DENG2_ASSERT(plr);
    int const playerNum = plr - players;
    ST_doFullscreenStuff(playerNum);
}

void ST_Drawer(int localPlayer)
{
    if(localPlayer < 0 || localPlayer >= MAXPLAYERS) return;

    if(!players[localPlayer].plr->inGame) return;

    R_UpdateViewFilter(localPlayer);

    hudstate_t *hud = &hudStates[localPlayer];
    hud->firstTime       = hud->firstTime;
    hud->statusbarActive = (ST_ActiveHud(localPlayer) < 2) || (ST_AutomapIsOpen(localPlayer) && (cfg.common.automapHudDisplay == 0 || cfg.common.automapHudDisplay == 2));

    drawUIWidgetsForPlayer(&players[localPlayer]);
}

dd_bool ST_StatusBarIsActive(int localPlayer)
{
    DENG2_ASSERT(localPlayer >= 0 && localPlayer < MAXPLAYERS);
    return false;
}

float ST_StatusBarShown(int localPlayer)
{
    DENG2_ASSERT(localPlayer >= 0 && localPlayer < MAXPLAYERS);
    return 0;
}

void ST_loadData()
{
    // Nothing to do.
}

static void initData(hudstate_t *hud)
{
    DENG2_ASSERT(hud);
    int const player = hud - hudStates;

    hud->firstTime = true;
    hud->statusbarActive = true;
    hud->stopped = true;
    hud->alpha = 0.f;

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

    automap.setMapBounds(*((coord_t *) DD_GetVariable(DD_MAP_MIN_X)),
                         *((coord_t *) DD_GetVariable(DD_MAP_MAX_X)),
                         *((coord_t *) DD_GetVariable(DD_MAP_MIN_Y)),
                         *((coord_t *) DD_GetVariable(DD_MAP_MAX_Y)));

#ifdef __JDOOM__
    automapcfg_t *style = automap.style();
#endif

    // Determine the obj view scale factors.
    if(automap.cameraZoomMode())
        automap.setScale(0);

    automap.clearAllPoints(true/*silent*/);

#if !__JHEXEN__
    if(G_Ruleset_Skill() == SM_BABY && cfg.common.automapBabyKeys)
    {
        automap.setFlags(automap.flags() | AWF_SHOW_KEYS);
    }
#endif

#if __JDOOM__
    if(!IS_NETGAME && hud->automapCheatLevel)
        AM_SetVectorGraphic(style, AMO_THINGPLAYER, VG_CHEATARROW);
#endif

    // Are we re-centering on a followed mobj?
    if(mobj_t *mob = automap.followMobj())
    {
        automap.setCameraOrigin(Vector2d(mob->origin));
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
    hudstate_t *hud;
    int flags;

    if(localPlayer < 0 || localPlayer >= MAXPLAYERS)
    {
        Con_Error("ST_Start: Invalid player #%i.", localPlayer);
        exit(1); // Unreachable.
    }
    hud = &hudStates[localPlayer];

    if(!hud->stopped)
    {
        ST_Stop(localPlayer);
    }

    initData(hud);

    //
    // Initialize widgets according to player preferences.
    //

    HudWidget &tGroup = GUI_FindWidgetById(hud->groupIds[UWG_TOP]);
    flags = tGroup.alignment();
    flags &= ~(ALIGN_LEFT|ALIGN_RIGHT);
    if(cfg.common.msgAlign == 0)
        flags |= ALIGN_LEFT;
    else if(cfg.common.msgAlign == 2)
        flags |= ALIGN_RIGHT;
    tGroup.setAlignment(flags);

    auto &automap = GUI_FindWidgetById(hud->automapId).as<AutomapWidget>();
    // If the automap was left open; close it.
    automap.open(false, true /*instantly*/);
    initAutomapForCurrentMap(automap);
    automap.setCameraRotationMode(CPP_BOOL(cfg.common.automapRotate));

    hud->stopped = false;
}

void ST_Stop(int localPlayer)
{
    if(localPlayer < 0 || localPlayer >= MAXPLAYERS) return;
    hudstate_t *hud = &hudStates[localPlayer];

    if(hud->stopped) return;

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
    if(localPlayer < 0 || localPlayer >= MAXPLAYERS) return;
    hudstate_t *hud = &hudStates[localPlayer];

    uiwidgetgroupdef_t const widgetGroupDefs[] = {
        { UWG_MAPNAME, ALIGN_BOTTOMLEFT },
        { UWG_AUTOMAP, ALIGN_TOPLEFT }
    };
    for(uiwidgetgroupdef_t const &def : widgetGroupDefs)
    {
        HudWidget *grp = makeGroupWidget(def.groupFlags, localPlayer, def.alignFlags, ORDER_NONE, def.padding);
        GUI_AddWidget(grp);
        hud->groupIds[def.group] = grp->id();
    }

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
    ST_AutomapOpen(localPlayer, false, fast);
#if __JHERETIC__ || __JHEXEN__
    Hu_InventoryOpen(localPlayer, false);
#endif
}

/// @note May be called prior to HUD init / outside game session.
AutomapWidget *ST_TryFindAutomapWidget(int localPlayer)
{
    if(localPlayer < 0 || localPlayer >= MAXPLAYERS) return nullptr;
    hudstate_t *hud = &hudStates[localPlayer];
    if(auto *wi = GUI_TryFindWidgetById(hud->automapId))
    {
        return wi->maybeAs<AutomapWidget>();
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
        return wi->maybeAs<ChatWidget>();
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
        return wi->maybeAs<PlayerLogWidget>();
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

void ST_LogPost(int localPlayer, byte flags, char const *msg)
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
    // Stub.
#if 0
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
#endif
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

dd_bool ST_AutomapObscures2(int localPlayer, RectRaw const * /*region*/)
{
    AutomapWidget *automap = ST_TryFindAutomapWidget(localPlayer);
    if(!automap) return false;

    if(automap->isOpen())
    {
        if(cfg.common.automapOpacity * ST_AutomapOpacity(localPlayer) >= ST_AUTOMAP_OBSCURE_TOLERANCE)
        {
            /*if(AutomapWidget_Fullscreen(obj))
            {*/
                return true;
            /*}
            else
            {
                // We'll have to compare the dimensions.
                const int scrwidth  = Get(DD_WINDOW_WIDTH);
                const int scrheight = Get(DD_WINDOW_HEIGHT);
                const Rect* rect = UIWidget_Geometry(obj);
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
        return automap->addPoint(Vector3d(x, y, z));
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
static int parseMacroId(String const &str) // static
{
    if(!str.isEmpty())
    {
        bool isNumber = false;
        int const id  = str.toInt(&isNumber);
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
static int parseTeamNumber(String const &str)
{
    if(!str.isEmpty())
    {
        bool isNumber = false;
        int const num = str.toInt(&isNumber);
        if(isNumber && num >= 0 && num <= NUMTEAMS)
        {
            return num;
        }
    }
    return -1;
}

D_CMD(ChatOpen)
{
    DENG2_UNUSED(src);

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
    DENG2_UNUSED2(src, argc);

    if(G_QuitInProgress()) return false;

    ChatWidget *chat = ST_TryFindChatWidget(CONSOLEPLAYER);
    if(!chat || !chat->isActive()) return false;

    auto const cmd = String(argv[0] + 4);
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
    DENG2_UNUSED(src);

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
    C_VAR_FLOAT2( "hud-color-r",                    &cfg.common.hudColor[0], 0, 0, 1, unhideHUD )
    C_VAR_FLOAT2( "hud-color-g",                    &cfg.common.hudColor[1], 0, 0, 1, unhideHUD )
    C_VAR_FLOAT2( "hud-color-b",                    &cfg.common.hudColor[2], 0, 0, 1, unhideHUD )
    C_VAR_FLOAT2( "hud-color-a",                    &cfg.common.hudColor[3], 0, 0, 1, unhideHUD )
    C_VAR_FLOAT2( "hud-icon-alpha",                 &cfg.common.hudIconAlpha, 0, 0, 1, unhideHUD )
    C_VAR_INT   ( "hud-patch-replacement",          &cfg.common.hudPatchReplaceMode, 0, 0, 1 )
    C_VAR_FLOAT2( "hud-scale",                      &cfg.common.hudScale, 0, 0.1f, 1, unhideHUD )
    C_VAR_FLOAT ( "hud-timer",                      &cfg.common.hudTimer, 0, 0, 60 )

    // Displays
    C_VAR_BYTE2 ( "hud-ammo",                       &cfg.hudShown[HUD_AMMO     ], 0, 0, 1, unhideHUD )
    C_VAR_BYTE2 ( "hud-armor",                      &cfg.hudShown[HUD_ARMOR    ], 0, 0, 1, unhideHUD )
    C_VAR_BYTE2 ( "hud-cheat-counter",              &cfg.common.hudShownCheatCounters,          0,   0, 63, unhideHUD )
    C_VAR_FLOAT2( "hud-cheat-counter-scale",        &cfg.common.hudCheatCounterScale,           0, .1f,  1, unhideHUD )
    C_VAR_BYTE2 ( "hud-cheat-counter-show-mapopen", &cfg.common.hudCheatCounterShowWithAutomap, 0,   0,  1, unhideHUD )
    C_VAR_BYTE2 ( "hud-frags",                      &cfg.hudShown[HUD_FRAGS    ], 0, 0, 1, unhideHUD )
    C_VAR_BYTE2 ( "hud-health",                     &cfg.hudShown[HUD_HEALTH   ], 0, 0, 1, unhideHUD )
    C_VAR_BYTE2 ( "hud-keys",                       &cfg.hudShown[HUD_KEYS     ], 0, 0, 1, unhideHUD )
    C_VAR_BYTE2 ( "hud-power",                      &cfg.hudShown[HUD_INVENTORY], 0, 0, 1, unhideHUD )

    // Events.
    C_VAR_BYTE  ( "hud-unhide-damage",              &cfg.hudUnHide[HUE_ON_DAMAGE       ], 0, 0, 1 )
    C_VAR_BYTE  ( "hud-unhide-pickup-ammo",         &cfg.hudUnHide[HUE_ON_PICKUP_AMMO  ], 0, 0, 1 )
    C_VAR_BYTE  ( "hud-unhide-pickup-armor",        &cfg.hudUnHide[HUE_ON_PICKUP_ARMOR ], 0, 0, 1 )
    C_VAR_BYTE  ( "hud-unhide-pickup-health",       &cfg.hudUnHide[HUE_ON_PICKUP_HEALTH], 0, 0, 1 )
    C_VAR_BYTE  ( "hud-unhide-pickup-key",          &cfg.hudUnHide[HUE_ON_PICKUP_KEY   ], 0, 0, 1 )
    C_VAR_BYTE  ( "hud-unhide-pickup-powerup",      &cfg.hudUnHide[HUE_ON_PICKUP_POWER ], 0, 0, 1 )
    C_VAR_BYTE  ( "hud-unhide-pickup-weapon",       &cfg.hudUnHide[HUE_ON_PICKUP_WEAPON], 0, 0, 1 )

    C_CMD("beginchat",       nullptr,   ChatOpen )
    C_CMD("chatcancel",      "",        ChatAction )
    C_CMD("chatcomplete",    "",        ChatAction )
    C_CMD("chatdelete",      "",        ChatAction )
    C_CMD("chatsendmacro",   nullptr,   ChatSendMacro )
}
