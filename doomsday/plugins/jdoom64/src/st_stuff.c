/**\file st_stuff.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2003-2005 Samuel Villarreal <svkaiser@gmail.com>
 *\author Copyright © 1993-1996 by id Software, Inc.
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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jdoom64.h"

#include "dmu_lib.h"
#include "d_net.h"
#include "hu_stuff.h"
#include "hu_lib.h"
#include "hu_chat.h"
#include "hu_log.h"
#include "hu_automap.h"
#include "p_mapsetup.h"
#include "p_tick.h" // for P_IsPaused
#include "p_inventory.h"
#include "p_player.h"
#include "am_map.h"
#include "r_common.h"

// Frags pos.
#define ST_FRAGSX           138
#define ST_FRAGSY           171
#define ST_FRAGSWIDTH       2

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

typedef struct {
    boolean inited;
    boolean stopped;
    int hideTics;
    float hideAmount;
    float alpha; // Fullscreen hud alpha value.
    boolean statusbarActive; // Whether the HUD is on.
    int automapCheatLevel; /// \todo Belongs in player state?

    int widgetGroupIds[NUM_UIWIDGET_GROUPS];
    int automapWidgetId;
    int chatWidgetId;
    int logWidgetId;

    boolean firstTime;  // ST_Start() has just been called.
    int currentFragsCount; // Number of frags so far in deathmatch.

    // Other:
    guidata_automap_t automap;
    guidata_chat_t chat;
    guidata_log_t log;
} hudstate_t;

typedef enum hotloc_e {
    HOT_TLEFT,
    HOT_TRIGHT,
    HOT_BRIGHT,
    HOT_BLEFT
} hotloc_t;

int ST_ChatResponder(int player, event_t* ev);
void unhideHUD(void);

static hudstate_t hudStates[MAXPLAYERS];

void ST_Register(void)
{
    C_VAR_FLOAT2( "hud-color-r", &cfg.hudColor[0], 0, 0, 1, unhideHUD )
    C_VAR_FLOAT2( "hud-color-g", &cfg.hudColor[1], 0, 0, 1, unhideHUD )
    C_VAR_FLOAT2( "hud-color-b", &cfg.hudColor[2], 0, 0, 1, unhideHUD )
    C_VAR_FLOAT2( "hud-color-a", &cfg.hudColor[3], 0, 0, 1, unhideHUD )
    C_VAR_FLOAT2( "hud-icon-alpha", &cfg.hudIconAlpha, 0, 0, 1, unhideHUD )
    C_VAR_INT(    "hud-patch-replacement", &cfg.hudPatchReplaceMode, 0, PRM_FIRST, PRM_LAST )
    C_VAR_FLOAT2( "hud-scale", &cfg.hudScale, 0, 0.1f, 1, unhideHUD )
    C_VAR_FLOAT(  "hud-timer", &cfg.hudTimer, 0, 0, 60 )

    // Displays
    C_VAR_BYTE2(  "hud-ammo", &cfg.hudShown[HUD_AMMO], 0, 0, 1, unhideHUD )
    C_VAR_BYTE2(  "hud-armor", &cfg.hudShown[HUD_ARMOR], 0, 0, 1, unhideHUD )
    C_VAR_BYTE2(  "hud-cheat-counter", &cfg.hudShownCheatCounters, 0, 0, 63, unhideHUD )
    C_VAR_FLOAT2( "hud-cheat-counter-scale", &cfg.hudCheatCounterScale, 0, .1f, 1, unhideHUD )
    C_VAR_BYTE2(  "hud-cheat-counter-show-mapopen", &cfg.hudCheatCounterShowWithAutomap, 0, 0, 1, unhideHUD )
    C_VAR_BYTE2(  "hud-frags", &cfg.hudShown[HUD_FRAGS], 0, 0, 1, unhideHUD )
    C_VAR_BYTE2(  "hud-health", &cfg.hudShown[HUD_HEALTH], 0, 0, 1, unhideHUD )
    C_VAR_BYTE2(  "hud-keys", &cfg.hudShown[HUD_KEYS], 0, 0, 1, unhideHUD )
    C_VAR_BYTE2(  "hud-power", &cfg.hudShown[HUD_INVENTORY], 0, 0, 1, unhideHUD )

    // Events.
    C_VAR_BYTE(   "hud-unhide-damage", &cfg.hudUnHide[HUE_ON_DAMAGE], 0, 0, 1 )
    C_VAR_BYTE(   "hud-unhide-pickup-ammo", &cfg.hudUnHide[HUE_ON_PICKUP_AMMO], 0, 0, 1 )
    C_VAR_BYTE(   "hud-unhide-pickup-armor", &cfg.hudUnHide[HUE_ON_PICKUP_ARMOR], 0, 0, 1 )
    C_VAR_BYTE(   "hud-unhide-pickup-health", &cfg.hudUnHide[HUE_ON_PICKUP_HEALTH], 0, 0, 1 )
    C_VAR_BYTE(   "hud-unhide-pickup-key", &cfg.hudUnHide[HUE_ON_PICKUP_KEY], 0, 0, 1 )
    C_VAR_BYTE(   "hud-unhide-pickup-powerup", &cfg.hudUnHide[HUE_ON_PICKUP_POWER], 0, 0, 1 )
    C_VAR_BYTE(   "hud-unhide-pickup-weapon", &cfg.hudUnHide[HUE_ON_PICKUP_WEAPON], 0, 0, 1 )

    C_CMD("beginchat",       NULL,   ChatOpen )
    C_CMD("chatcancel",      "",     ChatAction )
    C_CMD("chatcomplete",    "",     ChatAction )
    C_CMD("chatdelete",      "",     ChatAction )
    C_CMD("chatsendmacro",   NULL,   ChatSendMacro )
}

static int fullscreenMode(int player)
{
    return (cfg.screenBlocks < 10? 0 : cfg.screenBlocks - 10);
}

void ST_HUDUnHide(int player, hueevent_t ev)
{
    player_t* plr;

    if(player < 0 || player >= MAXPLAYERS)
    {
#if _DEBUG
        Con_Message("Warning:ST_HUDUnHide: Invalid player #%i, ignoring.", player);
#endif
        return;
    }
    if(ev < HUE_FORCE || ev > NUMHUDUNHIDEEVENTS)
    {
#if _DEBUG
        Con_Message("Warning:ST_HUDUnHide: Invalid event type %i, ignoring.", (int) ev);
#endif
        return;
    }

    plr = &players[player];
    if(!plr->plr->inGame) return;

    if(ev == HUE_FORCE || cfg.hudUnHide[ev])
    {
        hudStates[player].hideTics = (cfg.hudTimer * TICSPERSEC);
        hudStates[player].hideAmount = 0;
    }
}

void ST_updateWidgets(int player)
{
    int                 i;
    player_t*           plr = &players[player];
    hudstate_t*         hud = &hudStates[player];

    // Used by wFrags widget.
    hud->currentFragsCount = 0;

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        if(!players[i].plr->inGame)
            continue;

        hud->currentFragsCount += plr->frags[i] * (i != player ? 1 : -1);
    }
}

int ST_Responder(event_t* ev)
{
    int i, eaten = false;
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        eaten = ST_ChatResponder(i, ev);
        if(eaten) break;
    }
    return eaten;
}

void ST_Ticker(timespan_t ticLength)
{
    const boolean isSharpTic = DD_IsSharpTick();
    int i;
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        player_t* plr = &players[i];
        hudstate_t* hud = &hudStates[i];

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
            if(cfg.screenBlocks == 13)
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
        if(isSharpTic && !P_IsPaused())
        {
            if(cfg.hudTimer == 0)
            {
                hud->hideTics = hud->hideAmount = 0;
            }
            else
            {
                if(hud->hideTics > 0)
                    hud->hideTics--;
                if(hud->hideTics == 0 && cfg.hudTimer > 0 && hud->hideAmount < 1)
                    hud->hideAmount += 0.1f;
            }

            /// \todo Refactor me away.
            ST_updateWidgets(i);
        }

        if(hud->inited)
        {
            int j;
            for(j = 0; j < NUM_UIWIDGET_GROUPS; ++j)
            {
                UIWidget_RunTic(GUI_MustFindObjectById(hud->widgetGroupIds[j]), ticLength);
            }
        }
    }
}

static void drawWidgets(hudstate_t* hud)
{
#define MAXDIGITS           ST_FRAGSWIDTH

    if(deathmatch)
    {
        char buf[20];
        if(hud->currentFragsCount == 1994)
            return;
        dd_snprintf(buf, 20, "%i", hud->currentFragsCount);

        DGL_Enable(DGL_TEXTURE_2D);

        FR_SetFont(FID(GF_STATUS));
        FR_LoadDefaultAttrib();
        FR_SetColorAndAlpha(1, 1, 1, hud->alpha);

        FR_DrawTextXY3(buf, ST_FRAGSX, ST_FRAGSY, ALIGN_TOPRIGHT, DTF_NO_EFFECTS);

        DGL_Disable(DGL_TEXTURE_2D);
    }

#undef MAXDIGITS
}

void ST_doRefresh(int player)
{
    hudstate_t* hud;

    if(player < 0 || player > MAXPLAYERS)
        return;

    hud = &hudStates[player];
    hud->firstTime = false;

    drawWidgets(hud);
}

void ST_drawHUDSprite(int sprite, float x, float y, hotloc_t hotspot,
    float scale, float alpha, boolean flip, int* drawnWidth, int* drawnHeight)
{
    spriteinfo_t info;

    if(!(alpha > 0))
        return;

    alpha = MINMAX_OF(0.f, alpha, 1.f);
    R_GetSpriteInfo(sprite, 0, &info);

    switch(hotspot)
    {
    case HOT_BRIGHT:
        y -= info.geometry.size.height * scale;

    case HOT_TRIGHT:
        x -= info.geometry.size.width * scale;
        break;

    case HOT_BLEFT:
        y -= info.geometry.size.height * scale;
        break;
    default:
        break;
    }

    DGL_SetPSprite(info.material);
    DGL_Enable(DGL_TEXTURE_2D);

    DGL_Color4f(1, 1, 1, alpha);
    DGL_Begin(DGL_QUADS);
        DGL_TexCoord2f(0, flip * info.texCoord[0], 0);
        DGL_Vertex2f(x, y);

        DGL_TexCoord2f(0, !flip * info.texCoord[0], 0);
        DGL_Vertex2f(x + info.geometry.size.width * scale, y);

        DGL_TexCoord2f(0, !flip * info.texCoord[0], info.texCoord[1]);
        DGL_Vertex2f(x + info.geometry.size.width * scale, y + info.geometry.size.height * scale);

        DGL_TexCoord2f(0, flip * info.texCoord[0], info.texCoord[1]);
        DGL_Vertex2f(x, y + info.geometry.size.height * scale);
    DGL_End();

    DGL_Disable(DGL_TEXTURE_2D);

    if(drawnWidth)  *drawnWidth  = info.geometry.size.width  * scale;
    if(drawnHeight) *drawnHeight = info.geometry.size.height * scale;
}

void ST_doFullscreenStuff(int player)
{
    static const int    ammo_sprite[NUM_AMMO_TYPES] = {
        SPR_AMMO,
        SPR_SBOX,
        SPR_CELL,
        SPR_RCKT
    };

    hudstate_t*         hud = &hudStates[player];
    player_t*           plr = &players[player];
    char                buf[20];
    int                 w, h, pos = 0, oldPos = 0, spr,i;
    int                 h_width = 320 / cfg.hudScale;
    int                 h_height = 200 / cfg.hudScale;
    float               textalpha =
        hud->alpha - hud->hideAmount - ( 1 - cfg.hudColor[3]);
    float               iconalpha =
        hud->alpha - hud->hideAmount - ( 1 - cfg.hudIconAlpha);

    textalpha = MINMAX_OF(0.f, textalpha, 1.f);
    iconalpha = MINMAX_OF(0.f, iconalpha, 1.f);

    FR_LoadDefaultAttrib();

    if(IS_NETGAME && deathmatch && cfg.hudShown[HUD_FRAGS])
    {
        // Display the frag counter.
        i = 199 - HUDBORDERY;
        if(cfg.hudShown[HUD_HEALTH])
        {
            i -= 18 * cfg.hudScale;
        }

        sprintf(buf, "FRAGS:%i", hud->currentFragsCount);

        DGL_Enable(DGL_TEXTURE_2D);
        FR_SetFont(FID(GF_FONTA));
        FR_SetColorAndAlpha(cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textalpha);

        FR_DrawTextXY3(buf, HUDBORDERX, i, ALIGN_TOPLEFT, DTF_NO_EFFECTS);

        DGL_Disable(DGL_TEXTURE_2D);
    }

    // Setup the scaling matrix.
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Scalef(cfg.hudScale, cfg.hudScale, 1);

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
        FR_SetColorAndAlpha(cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textalpha);
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
                ST_drawHUDSprite(spr, pos, h_height - 2, HOT_BLEFT, 1,
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
        if(P_InventoryCount(player, IIT_DEMONKEY1))
        {
            spr = SPR_ART1;
            ST_drawHUDSprite(spr, HUDBORDERX + pos -w/2, h_height - 44,
                HOT_BLEFT, 1, iconalpha, false, &w, &h);
        }

        if(P_InventoryCount(player, IIT_DEMONKEY2))
        {
            spr = SPR_ART2;
            ST_drawHUDSprite(spr, HUDBORDERX + pos -w/2, h_height - 84,
                HOT_BLEFT, 1, iconalpha, false, &w, &h);
        }

        if(P_InventoryCount(player, IIT_DEMONKEY3))
        {
            spr = SPR_ART3;
            ST_drawHUDSprite(spr, HUDBORDERX + pos -w/2, h_height - 124,
                HOT_BLEFT, 1, iconalpha, false, &w, &h);
        }
    }

    if(cfg.hudShown[HUD_AMMO])
    {
        ammotype_t ammotype;

        //// \todo Only supports one type of ammo per weapon.
        //// for each type of ammo this weapon takes.
        for(ammotype=0; ammotype < NUM_AMMO_TYPES; ++ammotype)
        {
            if(!weaponInfo[plr->readyWeapon][plr->class_].mode[0].ammoType[ammotype])
                continue;

            sprintf(buf, "%i", plr->ammo[ammotype].owned);
            pos = h_width/2;

            DGL_Enable(DGL_TEXTURE_2D);

            FR_SetFont(FID(GF_FONTB));
            FR_SetColorAndAlpha(cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textalpha);
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
        FR_SetColorAndAlpha(cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textalpha);
        FR_DrawTextXY3(buf, h_width - (w/2) - HUDBORDERX, h_height - HUDBORDERY, ALIGN_BOTTOMRIGHT, DTF_NO_EFFECTS);

        DGL_Disable(DGL_TEXTURE_2D);
    }

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

void MapName_Drawer(uiwidget_t* obj, int x, int y)
{
    assert(obj && obj->type == GUI_MAPNAME);
    {
    const float scale = .75f;
    const float textAlpha = uiRendState->pageAlpha;
    const patchid_t patch = P_FindMapTitlePatch(gameEpisode, gameMap);
    const char* text = Hu_ChoosePatchReplacement2(PRM_ALLOW_TEXT, patch, P_GetMapNiceName());

    if(!text && 0 == patch) return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(x, y, 0);
    DGL_Scalef(scale, scale, 1);

    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, textAlpha);
    FR_SetFont(obj->font);
    FR_SetColorAndAlpha(cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textAlpha);

    WI_DrawPatchXY3(patch, text, 0, 0, ALIGN_BOTTOMLEFT, 0, DTF_NO_EFFECTS);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
}

void MapName_UpdateGeometry(uiwidget_t* obj)
{
    const patchid_t patch = P_FindMapTitlePatch(gameEpisode, gameMap);
    const char* text = Hu_ChoosePatchReplacement2(PRM_ALLOW_TEXT, patch, P_GetMapNiceName());
    const float scale = .75f;
    patchinfo_t info;
    assert(obj && obj->type == GUI_MAPNAME);

    Rect_SetWidthHeight(obj->geometry, 0, 0);

    if(!text && 0 == patch) return;

    if(text)
    {
        Size2Raw textSize;
        FR_SetFont(obj->font);
        FR_TextSize(&textSize, text);
        textSize.width  *= scale;
        textSize.height *= scale;
        Rect_SetWidthHeight(obj->geometry, textSize.width, textSize.height);
        return;
    }

    R_GetPatchInfo(patch, &info);
    Rect_SetWidthHeight(obj->geometry, info.geometry.size.width  * scale,
                                       info.geometry.size.height * scale);
}

typedef struct {
    guiwidgettype_t type;
    int group;
    gamefontid_t fontIdx;
    void (*updateGeometry) (uiwidget_t* obj);
    void (*drawer) (uiwidget_t* obj, int x, int y);
    void (*ticker) (uiwidget_t* obj, timespan_t ticLength);
    void* typedata;
} uiwidgetdef_t;

typedef struct {
    int group;
    int alignFlags;
    int groupFlags;
    int padding; // In fixed 320x200 pixels.
} uiwidgetgroupdef_t;

static void drawUIWidgetsForPlayer(player_t* plr)
{
    const int playerNum = plr - players;
    assert(plr);
    ST_doFullscreenStuff(playerNum);
}

void ST_Drawer(int player)
{
    hudstate_t* hud;

    if(player < 0 || player >= MAXPLAYERS)
    {
#if _DEBUG
        Con_Message("Warning:ST_Drawer: Invalid player #%i, ignoring.\n", player);
#endif
        return;
    }
    if(!players[player].plr->inGame) return;

    R_UpdateViewFilter(player);

    hud = &hudStates[player];
    hud->firstTime = hud->firstTime;
    hud->statusbarActive = (fullscreenMode(player) < 2) || (ST_AutomapIsActive(player) && (cfg.automapHudDisplay == 0 || cfg.automapHudDisplay == 2));

    drawUIWidgetsForPlayer(players + player);
}

void ST_loadData(void)
{
    // Nothing to do.
}

static void initData(hudstate_t* hud)
{
    int player = hud - hudStates;

    hud->firstTime = true;
    hud->statusbarActive = true;
    hud->stopped = true;
    hud->alpha = 0.f;

    hud->log._msgCount = 0;
    hud->log._nextUsedMsg = 0;
    hud->log._pvisMsgCount = 0;
    memset(hud->log._msgs, 0, sizeof(hud->log._msgs));

    ST_HUDUnHide(player, HUE_FORCE);
}

static void setAutomapCheatLevel(uiwidget_t* obj, int level)
{
    hudstate_t* hud = &hudStates[UIWidget_Player(obj)];
    int flags;

    hud->automapCheatLevel = level;

    flags = UIAutomap_Flags(obj) & ~(AMF_REND_ALLLINES|AMF_REND_THINGS|AMF_REND_SPECIALLINES|AMF_REND_VERTEXES|AMF_REND_LINE_NORMALS);
    if(hud->automapCheatLevel >= 1)
        flags |= AMF_REND_ALLLINES;
    if(hud->automapCheatLevel == 2)
        flags |= AMF_REND_THINGS | AMF_REND_SPECIALLINES;
    if(hud->automapCheatLevel > 2)
        flags |= (AMF_REND_VERTEXES | AMF_REND_LINE_NORMALS);
    UIAutomap_SetFlags(obj, flags);
}

static void initAutomapForCurrentMap(uiwidget_t* obj)
{
    hudstate_t* hud = &hudStates[UIWidget_Player(obj)];
    automapcfg_t* mcfg;
    mobj_t* followMobj;
    uint i;

    UIAutomap_Reset(obj);

    UIAutomap_SetMinScale(obj, 2 * PLAYERRADIUS);
    UIAutomap_SetWorldBounds(obj, *((coord_t*) DD_GetVariable(DD_MAP_MIN_X)),
                                  *((coord_t*) DD_GetVariable(DD_MAP_MAX_X)),
                                  *((coord_t*) DD_GetVariable(DD_MAP_MIN_Y)),
                                  *((coord_t*) DD_GetVariable(DD_MAP_MAX_Y)));

    mcfg = UIAutomap_Config(obj);

    // Determine the obj view scale factors.
    if(UIAutomap_ZoomMax(obj))
        UIAutomap_SetScale(obj, 0);

    UIAutomap_ClearPoints(obj);

#if !__JHEXEN__
    if(gameSkill == SM_BABY && cfg.automapBabyKeys)
    {
        int flags = UIAutomap_Flags(obj);
        UIAutomap_SetFlags(obj, flags|AMF_REND_KEYS);
    }
#endif

#if __JDOOM__
    if(!IS_NETGAME && hud->automapCheatLevel)
        AM_SetVectorGraphic(mcfg, AMO_THINGPLAYER, VG_CHEATARROW);
#endif

    // Are we re-centering on a followed mobj?
    followMobj = UIAutomap_FollowMobj(obj);
    if(followMobj)
    {
        UIAutomap_SetCameraOrigin(obj, followMobj->origin[VX], followMobj->origin[VY]);
    }

    if(IS_NETGAME)
    {
        setAutomapCheatLevel(obj, 0);
    }

    UIAutomap_SetReveal(obj, false);

    // Add all immediately visible lines.
    for(i = 0; i < numlines; ++i)
    {
        xline_t* xline = &xlines[i];
        if(!(xline->flags & ML_MAPPED)) continue;

        P_SetLinedefAutomapVisibility(UIWidget_Player(obj), i, true);
    }
}

void ST_Start(int player)
{
    uiwidget_t* obj;
    hudstate_t* hud;
    int flags;

    if(player < 0 || player >= MAXPLAYERS)
    {
        Con_Error("ST_Start: Invalid player #%i.", player);
        exit(1); // Unreachable.
    }
    hud = &hudStates[player];

    if(!hud->stopped)
    {
        ST_Stop(player);
    }

    initData(hud);

    /**
     * Initialize widgets according to player preferences.
     */

    obj = GUI_MustFindObjectById(hud->widgetGroupIds[UWG_TOP]);
    flags = UIWidget_Alignment(obj);
    flags &= ~(ALIGN_LEFT|ALIGN_RIGHT);
    if(cfg.msgAlign == 0)
        flags |= ALIGN_LEFT;
    else if(cfg.msgAlign == 2)
        flags |= ALIGN_RIGHT;
    UIWidget_SetAlignment(obj, flags);

    obj = GUI_MustFindObjectById(hud->automapWidgetId);
    // If the automap was left open; close it.
    UIAutomap_Open(obj, false, true);
    initAutomapForCurrentMap(obj);
    UIAutomap_SetCameraRotation(obj, cfg.automapRotate);

    hud->stopped = false;
}

void ST_Stop(int player)
{
    hudstate_t* hud;

    if(player < 0 || player >= MAXPLAYERS)
        return;

    hud = &hudStates[player];
    if(hud->stopped)
        return;

    hud->stopped = true;
}

void ST_BuildWidgets(int player)
{
    assert(player >= 0 && player < MAXPLAYERS);
    {
#define PADDING 2 // In fixed 320x200 units.

    hudstate_t* hud = &hudStates[player];
    const uiwidgetgroupdef_t widgetGroupDefs[] = {
        { UWG_MAPNAME,      ALIGN_BOTTOMLEFT },
        { UWG_AUTOMAP,      ALIGN_TOPLEFT }
    };
    size_t i;

    for(i = 0; i < sizeof(widgetGroupDefs)/sizeof(widgetGroupDefs[0]); ++i)
    {
        const uiwidgetgroupdef_t* def = &widgetGroupDefs[i];
        hud->widgetGroupIds[def->group] = GUI_CreateGroup(def->groupFlags, player, def->alignFlags, 0, def->padding);
    }

    hud->automapWidgetId = GUI_CreateWidget(GUI_AUTOMAP, player, 0, FID(GF_FONTB), 1, UIAutomap_UpdateGeometry, UIAutomap_Drawer, UIAutomap_Ticker, &hud->automap);
    UIGroup_AddWidget(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_AUTOMAP]), GUI_FindObjectById(hud->automapWidgetId));

#undef PADDING
    }
}

void ST_Init(void)
{
    int i;
    ST_InitAutomapConfig();
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        hudstate_t* hud = &hudStates[i];
        ST_BuildWidgets(i);
        hud->inited = true;
    }
    ST_loadData();
}

void ST_Shutdown(void)
{
    int i;
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        hudstate_t* hud = &hudStates[i];
        hud->inited = false;
    }
}

uiwidget_t* ST_UIChatForPlayer(int player)
{
    if(player >= 0 && player < MAXPLAYERS)
    {
        hudstate_t* hud = &hudStates[player];
        return GUI_FindObjectById(hud->chatWidgetId);
    }
    Con_Error("ST_UIChatForPlayer: Invalid player #%i.", player);
    exit(1); // Unreachable.
}

uiwidget_t* ST_UILogForPlayer(int player)
{
    if(player >= 0 && player < MAXPLAYERS)
    {
        hudstate_t* hud = &hudStates[player];
        return GUI_FindObjectById(hud->logWidgetId);
    }
    Con_Error("ST_UILogForPlayer: Invalid player #%i.", player);
    exit(1); // Unreachable.
}

uiwidget_t* ST_UIAutomapForPlayer(int player)
{
    if(player >= 0 && player < MAXPLAYERS)
    {
        hudstate_t* hud = &hudStates[player];
        return GUI_FindObjectById(hud->automapWidgetId);
    }
    Con_Error("ST_UIAutomapForPlayer: Invalid player #%i.", player);
    exit(1); // Unreachable.
}

int ST_ChatResponder(int player, event_t* ev)
{
    uiwidget_t* obj = ST_UIChatForPlayer(player);
    if(NULL != obj)
    {
        return UIChat_Responder(obj, ev);
    }
    return false;
}

boolean ST_ChatIsActive(int player)
{
    uiwidget_t* obj = ST_UIChatForPlayer(player);
    if(NULL != obj)
    {
        return UIChat_IsActive(obj);
    }
    return false;
}

void ST_LogPost(int player, byte flags, const char* msg)
{
    uiwidget_t* obj = ST_UILogForPlayer(player);
    if(!obj) return;

    UILog_Post(obj, flags, msg);
}

void ST_LogRefresh(int player)
{
    uiwidget_t* obj = ST_UILogForPlayer(player);
    if(!obj) return;
    UILog_Refresh(obj);
}

void ST_LogEmpty(int player)
{
    uiwidget_t* obj = ST_UILogForPlayer(player);
    if(!obj) return;
    UILog_Empty(obj);
}

void ST_LogPostVisibilityChangeNotification(void)
{
    int i;
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        ST_LogPost(i, LMF_NO_HIDE, !cfg.hudShown[HUD_LOG] ? MSGOFF : MSGON);
    }
}

void ST_LogUpdateAlignment(void)
{
    // Stub.
#if 0
    short flags;
    int i;
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        hudstate_t* hud = &hudStates[i];
        if(!hud->inited) continue;

        flags = UIGroup_Flags(GUI_MustFindObjectById(hud->widgetGroupNames[UWG_TOP]));
        flags &= ~(UWGF_ALIGN_LEFT|UWGF_ALIGN_RIGHT);
        if(cfg.msgAlign == 0)
            flags |= UWGF_ALIGN_LEFT;
        else if(cfg.msgAlign == 2)
            flags |= UWGF_ALIGN_RIGHT;
        UIGroup_SetFlags(GUI_MustFindObjectById(hud->widgetGroupNames[UWG_TOP]), flags);
    }
#endif
}

void ST_AutomapOpen(int player, boolean yes, boolean fast)
{
    uiwidget_t* obj = ST_UIAutomapForPlayer(player);
    if(!obj) return;
    UIAutomap_Open(obj, yes, fast);
}

boolean ST_AutomapIsActive(int player)
{
    uiwidget_t* obj = ST_UIAutomapForPlayer(player);
    if(!obj) return false;
    return UIAutomap_Active(obj);
}

boolean ST_AutomapObscures2(int player, const RectRaw* region)
{
    uiwidget_t* obj = ST_UIAutomapForPlayer(player);
    if(!obj) return false;
    if(UIAutomap_Active(obj))
    {
        if(cfg.automapOpacity * ST_AutomapOpacity(player) >= ST_AUTOMAP_OBSCURE_TOLERANCE)
        {
            /*if(UIAutomap_Fullscreen(obj))
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

boolean ST_AutomapObscures(int player, int x, int y, int width, int height)
{
    RectRaw rect;
    rect.origin.x = x;
    rect.origin.y = y;
    rect.size.width  = width;
    rect.size.height = height;
    return ST_AutomapObscures2(player, &rect);
}

void ST_AutomapClearPoints(int player)
{
    uiwidget_t* ob = ST_UIAutomapForPlayer(player);
    if(!ob) return;

    UIAutomap_ClearPoints(ob);
    P_SetMessage(&players[player], LMF_NO_HIDE, AMSTR_MARKSCLEARED);
}

/**
 * Adds a marker at the specified X/Y location.
 */
int ST_AutomapAddPoint(int player, coord_t x, coord_t y, coord_t z)
{
    static char buffer[20];
    uiwidget_t* obj = ST_UIAutomapForPlayer(player);
    int newPoint;
    if(!obj) return - 1;

    if(UIAutomap_PointCount(obj) == MAX_MAP_POINTS) return -1;

    newPoint = UIAutomap_AddPoint(obj, x, y, z);
    sprintf(buffer, "%s %d", AMSTR_MARKEDSPOT, newPoint);
    P_SetMessage(&players[player], LMF_NO_HIDE, buffer);

    return newPoint;
}

boolean ST_AutomapPointOrigin(int player, int point, coord_t* x, coord_t* y, coord_t* z)
{
    uiwidget_t* obj = ST_UIAutomapForPlayer(player);
    if(!obj) return false;
    return UIAutomap_PointOrigin(obj, point, x, y, z);
}

void ST_ToggleAutomapMaxZoom(int player)
{
    uiwidget_t* obj = ST_UIAutomapForPlayer(player);
    if(!obj) return;
    if(UIAutomap_SetZoomMax(obj, !UIAutomap_ZoomMax(obj)))
    {
        Con_Printf("Maximum zoom %s in automap.\n", UIAutomap_ZoomMax(obj)? "ON":"OFF");
    }
}

float ST_AutomapOpacity(int player)
{
    uiwidget_t* obj = ST_UIAutomapForPlayer(player);
    if(!obj) return 0;
    return UIAutomap_Opacity(obj);
}

void ST_SetAutomapCameraRotation(int player, boolean on)
{
    uiwidget_t* obj = ST_UIAutomapForPlayer(player);
    if(!obj) return;
    UIAutomap_SetCameraRotation(obj, on);
}

void ST_ToggleAutomapPanMode(int player)
{
    uiwidget_t* ob = ST_UIAutomapForPlayer(player);
    if(!ob) return;
    if(UIAutomap_SetPanMode(ob, !UIAutomap_PanMode(ob)))
    {
        P_SetMessage(&players[player], LMF_NO_HIDE, (UIAutomap_PanMode(ob)? AMSTR_FOLLOWOFF : AMSTR_FOLLOWON));
    }
}

void ST_CycleAutomapCheatLevel(int player)
{
    if(player >= 0 && player < MAXPLAYERS)
    {
        hudstate_t* hud = &hudStates[player];
        ST_SetAutomapCheatLevel(player, (hud->automapCheatLevel + 1) % 3);
    }
}

void ST_SetAutomapCheatLevel(int player, int level)
{
    uiwidget_t* obj = ST_UIAutomapForPlayer(player);
    if(!obj) return;
    setAutomapCheatLevel(obj, level);
}

void ST_RevealAutomap(int player, boolean on)
{
    uiwidget_t* obj = ST_UIAutomapForPlayer(player);
    if(!obj) return;
    UIAutomap_SetReveal(obj, on);
}

boolean ST_AutomapHasReveal(int player)
{
    uiwidget_t* obj = ST_UIAutomapForPlayer(player);
    if(!obj) return false;
    return UIAutomap_Reveal(obj);
}

void ST_RebuildAutomap(int player)
{
    uiwidget_t* obj = ST_UIAutomapForPlayer(player);
    if(!obj) return;
    UIAutomap_Rebuild(obj);
}

int ST_AutomapCheatLevel(int player)
{
    if(player >=0 && player < MAXPLAYERS)
        return hudStates[player].automapCheatLevel;
    return 0;
}

/**
 * Called when a cvar changes that affects the look/behavior of the HUD in order to unhide it.
 */
void unhideHUD(void)
{
    int i;
    for(i = 0; i < MAXPLAYERS; ++i)
        ST_HUDUnHide(i, HUE_FORCE);
}

D_CMD(ChatOpen)
{
    int player = CONSOLEPLAYER, destination = 0;
    uiwidget_t* obj;

    if(G_GameAction() == GA_QUIT)
    {
        return false;
    }

    obj = ST_UIChatForPlayer(player);
    if(!obj)
    {
        return false;
    }

    if(argc == 2)
    {
        destination = UIChat_ParseDestination(argv[1]);
        if(destination < 0)
        {
            Con_Message("Invalid team number #%i (valid range: 0..%i).\n", destination, NUMTEAMS);
            return false;
        }
    }
    UIChat_SetDestination(obj, destination);
    UIChat_Activate(obj, true);
    return true;
}

D_CMD(ChatAction)
{
    int player = CONSOLEPLAYER;
    const char* cmd = argv[0] + 4;
    uiwidget_t* obj;

    if(G_GameAction() == GA_QUIT)
    {
        return false;
    }

    obj = ST_UIChatForPlayer(player);
    if(NULL == obj || !UIChat_IsActive(obj))
    {
        return false;
    }
    if(!stricmp(cmd, "complete")) // Send the message.
    {
        return UIChat_CommandResponder(obj, MCMD_SELECT);
    }
    else if(!stricmp(cmd, "cancel")) // Close chat.
    {
        return UIChat_CommandResponder(obj, MCMD_CLOSE);
    }
    else if(!stricmp(cmd, "delete"))
    {
        return UIChat_CommandResponder(obj, MCMD_DELETE);
    }
    return true;
}

D_CMD(ChatSendMacro)
{
    int player = CONSOLEPLAYER, macroId, destination = 0;
    uiwidget_t* obj;

    if(G_GameAction() == GA_QUIT)
        return false;

    if(argc < 2 || argc > 3)
    {
        Con_Message("Usage: %s (team) (macro number)\n", argv[0]);
        Con_Message("Send a chat macro to other player(s).\n"
                    "If (team) is omitted, the message will be sent to all players.\n");
        return true;
    }

    obj = ST_UIChatForPlayer(player);
    if(!obj)
    {
        return false;
    }

    if(argc == 3)
    {
        destination = UIChat_ParseDestination(argv[1]);
        if(destination < 0)
        {
            Con_Message("Invalid team number #%i (valid range: 0..%i).\n", destination, NUMTEAMS);
            return false;
        }
    }

    macroId = UIChat_ParseMacroId(argc == 3? argv[2] : argv[1]);
    if(-1 == macroId)
    {
        Con_Message("Invalid macro id.\n");
        return false;
    }

    UIChat_Activate(obj, true);
    UIChat_SetDestination(obj, destination);
    UIChat_LoadMacro(obj, macroId);
    UIChat_CommandResponder(obj, MCMD_SELECT);
    UIChat_Activate(obj, false);
    return true;
}
