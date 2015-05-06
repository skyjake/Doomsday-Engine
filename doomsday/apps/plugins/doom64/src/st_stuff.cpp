/**\file st_stuff.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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
#include "p_mapsetup.h"
#include "p_tick.h" // for Pause_IsPaused
#include "p_inventory.h"
#include "player.h"
#include "r_common.h"

// Widgets
// ============================================================================

#include "hu_stuff.h"

#include "hud/widgets/automapwidget.h"
#include "hud/widgets/playerlogwidget.h"
#include "hud/widgets/chatwidget.h"

#include "hud/widgets/readyammoiconwidget.h"
#include "hud/widgets/readyammowidget.h"
#include "hud/widgets/keyswidget.h"
#include "hud/widgets/itemswidget.h"

#include "hud/widgets/healthwidget.h"
#include "hud/widgets/armorwidget.h"

#include "hud/widgets/killswidget.h"
#include "hud/widgets/secretswidget.h"

// Types / Constants
// ============================================================================

const int STARTREDPALS                  = 1;
const int NUMREDPALS                    = 8;
const int STARTBONUSPALS                = 9;
const int NUMBONUSPALS                  = 4;

enum {
    UWG_MAPNAME,
    UWG_BOTTOM,
    UWG_BOTTOMLEFT,
    UWG_BOTTOMLEFT2,
    UWG_BOTTOMRIGHT,
    UWG_BOTTOMCENTER,
    UWG_TOPCENTER,
    UWG_TOP = UWG_TOPCENTER,
    UWG_COUNTERS,
    UWG_AUTOMAP,
    NUM_UIWIDGET_GROUPS
};

struct hudstate_t {
    dd_bool inited;
    dd_bool stopped;
    int     hideTics;
    float   hideAmount;
    // Fullscreen HUD alpha
    float   alpha; 
    int     automapCheatLevel; 

    /*
     * UI Widgets
     */

    uiwidgetid_t groupIds[NUM_UIWIDGET_GROUPS];

    // No statusbar, just fullscreen, for maximum d64 experience
    uiwidgetid_t healthIconId;
    uiwidgetid_t healthId;

    uiwidgetid_t armorIconId;
    uiwidgetid_t armorId;

    uiwidgetid_t readyAmmoIconId;
    uiwidgetid_t readyAmmoId;

    uiwidgetid_t fragsId;

    // Keys should be able to hold our demon key
    uiwidgetid_t keysId;

    // Secrets, Items, Kills status panel
    uiwidgetid_t secretsId;
    uiwidgetid_t itemsId;
    uiwidgetid_t killsId;

    // Other things
    uiwidgetid_t automapId;
    uiwidgetid_t chatId;
    uiwidgetid_t logId;
};

static hudstate_t hudStates[MAXPLAYERS];

// Private Logic
// ============================================================================

/**
 * Unhide all players' HUDs'
 * Used exclusively by ST_Register (as a pointer)
 */

static void unhideHUD(void)
{
    int i;
    for(i = 0; i < MAXPLAYERS; ++i)
        ST_HUDUnHide(i, HUE_FORCE);
}

static void ST_drawHUDSprite(int sprite, float x, float y, hotloc_t hotspot,
    float scale, float alpha, dd_bool flip, int* drawnWidth, int* drawnHeight)
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

/**
 * Draw the ingame heads-up display and the automap.
 * This is called for each render pass.
 */
static void drawUIWidgetsForPlayer(player_t* plr)
{
    // UI Widgets are drawn N units from the edge of the screen on all sides in order to look pleasent
    static const int    INSET       = 2;

    // Magic (not really -- standard 1.2:1 anamporphic) aspect ratio used to adjust render height
    static const float  ASPECT_TRIM = 1.2F;
    
    DENG2_ASSERT(plr);

    int const playerId    = (plr - players);
    int const hudMode     = ST_ActiveHud(playerId);
    hudstate_t* hud       = &hudStates[player];

    Size2Raw portSize; R_ViewPortSize(playerId, &portSize);

    // TODO float const automapOpacity
    // Automap Group
    {
        HudWidget& amGroup = GUI_FindWidgetById(hud->groupIds[UWG_AUTOMAP]);
        amGroup.setOpacity(ST_AutomapOpacity(playerId));
        amGroup.setMaximumSize(portSize);
        
        GUI_DrawWidgetXY(amGroup, 0, 0);
    }

    // Ingame UI
    // displayMode >= 3 presumeable refers to `No-Hud` 
    // There ought to be some constants for this
    if (hud->alpha > 0 || displayMode < 3) {
        float uiScale;
        R_ChooseAlignModeAndScaleFactor(&scale, SCREENWIDTH, SCREENHEIGHT, 
                                        portSize.width, portSize.height, SCALEMODE_SMART_STRETCH);

        float opacity = de::min(1.0F, hud->alpha) * (1 - hud->hideAmount);

        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PushMatrix();
        DGL_Scalef(uiScale, uiScale * ASPECT_TRIM, 1);
       
        RectRaw displayRegion;
        {
            displayRegion.origin.x    = INSET;
            displayRegion.origin.y    = INSET;
            displayRegion.size.width  = (0.5F + portSize.width / uiScale) - (2 * INSET);
            displayRegion.size.height = (0.5F + portSize.height / (uiScale * ASPECT_TRIM)) - (2 * INSET);
        }

        // This is used to calculate a suitable offset for the map name group
        Size2Raw regionRendered;

        // Bottom widget group
        {
            HudWidget& bottomGroup  = GUI_FindWidgetById(hud->groupIds[UWG_BOTTOM]);
            bottomGroup.setOpacity(opacity);
            bottomGroup.setMaximumSize(displayRegion.size);

            GUI_DrawWidget(&bottomGroup, &displayRegion.origin);
            
            Size2_Raw(Rect_Size(&bottomGroup.geometry()), &regionRendered)
        }

        // Map name widget group
        {
            HudWidget& mapNameGroup = GUI_FindWidgetById(hud->groupIds[UWG_MAPNAME]);
            mapNameGroup.setOpacity(ST_AutomapOpacity(playerId));

            Size2Raw remainingVertical(displayRegion.size.width,
                                       displayRegion.size.height - (drawnSize.height > 0 ? drawnSize.height : 0));
            
            mapNameGroup.setMaximumSize(remainingVertical);

            GUI_DrawWidget(&mapNameGroup, &displayRegion.origin);
        }


        // Remaining widgets: Top Center, Counters (Kills, Secrets, Items)
        {
            // Kills widget, etc, are always visible unless no-hud
            if (displayMode < 3)
            {
                opacity = 1F;
            }

            // Top Center 
            {
                HudWidget& topCenter = GUI_FindWidgetById(hud->groupIds[UWG_TOPCENTER]);
                topCenter.setOpacity(opacity);
                topCenter.setMaximumSize(displayRegion.size);

                GUI_DrawWidget(&topCenter, &displayRegion.origin);
            }

            // Counters
            {
                HudWidget& counters = GUI_FindWidgetById(hud->groupIds[UWG_COUNTERS]);
                counters.setOpacity(opacity);
                counters.setMaximumSize(displayRegion.size);

                GUI_DrawWidget(&counter, &displayRegion.origin);
            }
        }

        // Clean up GL context
        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PopMatrix();
    }
}

/**
 * This initializes widgets used by the provided heads-up display to zero-values
 * or the equivalent thereof
 */
static void initData(hudstate_t* hud)
{
    DENG2_ASSERT(hud);

    hud->statusbarActive = true;
    hud->stopped         = true;

    // Reset/Initialize Elements
    {
        GUI_FindWidgetById(hud->healthId).as<guidata_health_t>().reset();
        GUI_FindWidgetById(hud->armorIconId).as<guidata_armoricon_t>().reset();
        GUI_FindWidgetById(hud->armorId).as<guidata_armor_t>().reset();
        GUI_FindWidgetById(hud->keysId).as<guidata_keys_t>().reset();
        GUI_FindWidgetById(hud->fragsId).as<guidata_frags_t>().reset();

        GUI_FindWidgetById(hud->secretsId).as<guidata_secrets_t>().reset();
        GUI_FindWidgetById(hud->itemsId).as<guidata_items_t>().reset();
        GUI_FindWidgetById(hud->killsId).as<guidata_kills_t>().reset();

        GUI_FindWidgetById(hud->logId).as<PlayerLogWidget>().clear();
    }

    ST_HUDUnHide(hud - hudStates, HUE_FORCE);
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
#ifdef __JDOOM__
    hudstate_t *hud = &hudStates[UIWidget_Player(obj)];
    automapcfg_t *mcfg;
#endif
    mobj_t *followMobj;
    int i;

    UIAutomap_Reset(obj);

    UIAutomap_SetMinScale(obj, 2 * PLAYERRADIUS);
    UIAutomap_SetWorldBounds(obj, *((coord_t*) DD_GetVariable(DD_MAP_MIN_X)),
                                  *((coord_t*) DD_GetVariable(DD_MAP_MAX_X)),
                                  *((coord_t*) DD_GetVariable(DD_MAP_MIN_Y)),
                                  *((coord_t*) DD_GetVariable(DD_MAP_MAX_Y)));

#ifdef __JDOOM__
    mcfg = UIAutomap_Config(obj);
#endif

    // Determine the obj view scale factors.
    if(UIAutomap_ZoomMax(obj))
        UIAutomap_SetScale(obj, 0);

    UIAutomap_ClearPoints(obj);

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
        xline_t *xline = &xlines[i];
        if(!(xline->flags & ML_MAPPED)) continue;

        P_SetLineAutomapVisibility(UIWidget_Player(obj), i, true);
    }
}

static void ST_BuildWidgets(int player)
{
    struct uiwidgetgroupdef_t {
        int     group;
        int     alignFlags;
        order_t order;
        int     groupFlags;
        int     padding; // In fixed 320x200 pixels.
    };

    struct uiwidgetdef_t {
        guiwidgettype_t type;
        int             alignFlags;
        int             group;
        gamefontid_t    fontIdx;

        void (*updateGeometry) (uiwidget_t* obj);
        void (*drawer) (uiwidget_t* obj, int x, int y);
        void (*ticker) (uiwidget_t* obj, timespan_t ticLength);
        void* typedata;
    };

    hudstate_t* hud = &hudStates[player];

    // Create a table of positioning constraints for widgets and add them to the HUD
    {
        static const int PADDING = 2;
        uiwidgetgroupdef_t const widgetGroupDefs[] = {
            { UWG_MAPNAME,      ALIGN_BOTTOMLEFT,   ORDER_NONE,         0,              0       },
            { UWG_BOTTOMLEFT,   ALIGN_BOTTOMLEFT,   ORDER_RIGHTTOLEFT,  UWGF_VERTICAL,  PADDING },
            { UWG_BOTTOMLEFT2,  ALIGN_BOTTOMLEFT,   ORDER_LEFTTORIGHT,  0,              PADDING },
            { UWG_BOTTOMRIGHT,  ALIGN_BOTTOMRIGHT,  ORDER_RIGHTTOLEFT,  0,              PADDING },
            { UWG_BOTTOMCENTER, ALIGN_BOTTOM,       ORDER_RIGHTTOLEFT,  UWGF_VERTICAL,  PADDING },
            { UWG_BOTTOM,       ALIGN_BOTTOMLEFT,   ORDER_LEFTTORIGHT,  0,              0       },
            { UWG_TOPCENTER,    ALIGN_TOPLEFT,      ORDER_LEFTTORIGHT,  UWGF_VERTICAL,  PADDING },
            { UWG_COUNTERS,     ALIGN_LEFT,         ORDER_RIGHTTOLEFT,  UWGF_VERTICAL,  PADDING },
            { UWG_AUTOMAP,      ALIGN_TOPLEFT,      ORDER_NONE,         0,              0       }
        };

        for (uiwidgetgroupdef_t const &def : widgetGroupDefs)
        {
            HudWidget* grp           = makeGroupWidget(def.groupFlags, player, def.alignFlags, def.order, def.padding);
            hud->groupIds[def.group] = grp->id();
            GUI_AddWidget(grp);
        }
    }

    // Configure the bottom row of groups by adding UWG_BOTTOM{LEFT, CENTER, RIGHT} to UWG_BOTTOM in that order
    {
        GroupWidget& bottom = GUI_FindWidgetById(hud->groupIds[UWG_BOTTOM]).as<GroupWidget>();

        bottom.addChild(&GUI_FindWidgetById(hud->groupIds[UWG_BOTTOMLEFT]));
        bottom.addChild(&GUI_FindWidgetById(hud->groupIds[UWG_BOTTOMCENTER]));
        bottom.addChild(&GUI_FindWidgetById(hud->groupIds[UWG_BOTTOMRIGHT]));


        // Add BOTTOMLEFT2 to BOTTOMLEFT
        GUI_FindWidgetById(hud->groupIds[UWG_BOTTOMLEFT]).as<GroupWidget>().addChild(&GUI_FindWidgetById(hud->groupIds[UWG_BOTTOMLEFT2]));
    }

    DENG2_ASSERT(player >= 0 && player < MAXPLAYERS);

    // Create a table of needed widgets and initialize them
    {
        uiwidgetdef_t const widgetDefs[] = {
            { GUI_HEALTHICON,       ALIGN_BOTTOMLEFT,   UWG_BOTTOMLEFT2,    GF_NONE,    nullptr,                                                                nullptr,                                                &hud->healthIconId      },
            { GUI_HEALTH,           ALIGN_BOTTOMLEFT,   UWG_BOTTOMLEFT2,    GF_FONTB,   function_cast<UpdateGeometryFunc>(HealthWidget_UpdateGeometry),         function_cast<DrawFunc>(HealthWidget_Draw),             &hud->healthId          },
            { GUI_READYAMMOICON,    ALIGN_BOTTOMLEFT,   UWG_BOTTOMLEFT2,    GF_NONE,    function_cast<UpdateGeometryFunc>(ReadyAmmoIconWidget_UpdateGeometry),  function_cast<DrawFunc>(ReadyAmmoIconWidget_Drawer),    &hud->readyAmmoIconId   },        
            { GUI_READYAMMO,        ALIGN_BOTTOMLEFT,   UWG_BOTTOMLEFT2,    GF_FONTB,   function_cast<UpdateGeometryFunc>(ReadyAmmo_UpdateGeometry),            function_cast<DrawFunc>(ReadyAmmo_Drawer),              &hud->readyAmmoId       },

            // { GUI_FRAGS,            ALIGN_BOTTOMCENTER, UWG_BOTTOMCENTER,   GF_FONTA,   function_cast<UpdateGeometryFunc>(FragsWidget_UpdateGeometry),          function_cast<DrawFunc>(FragsWidget_Draw),              &hud->fragsId           },
            
            { GUI_KEYS,             ALIGN_BOTTOMRIGHT,  UWG_BOTTOMRIGHT,    GF_NONE,    nullptr,                                                                nullptr,                                                &hud->keysId            },
            { GUI_ARMOR,            ALIGN_BOTTOMRIGHT,  UWG_BOTTOMRIGHT,    GF_FONTB,   function_cast<UpdateGeometryFunc>(Armor_UpdateGeometry),                function_cast<DrawFunc>(ArmorWidget_Draw),              &hud->armorId           },
            { GUI_ARMORICON,        ALIGN_BOTTOMRIGHT,  UWG_BOTTOMRIGHT,    GF_NONE,    nullptr,                                                                nullptr,                                                &hud->armorIconId       },

            { GUI_SECRETS,          ALIGN_TOPLEFT,      UWG_COUNTERS,       GF_FONTA,   nullptr,                                                                nullptr,                                                &hud->secretsId         },
            { GUI_ITEMS,            ALIGN_TOPLEFT,      UWG_COUNTERS,       GF_FONTA,   nullptr,                                                                nullptr,                                                &hud->itemsId           },
            { GUI_KILLS,            ALIGN_TOPLEFT,      UWG_COUNTERS,       GF_FONTA,   nullptr,                                                                nullptr,                                                &hud->killsId           }
        };

        // Initialize widgets
        for (uiwidgetdef_t const &def : widgetDefs)
        {
            HudWidget* widget = nullptr;
            switch (def.type)
            {
                case GUI_HEALTHICON:    widget = new guidata_healthicon_t                                   (player); break;
                case GUI_HEALTH:        widget = new guidata_health_t       (def.updateGeometry, def.drawer, player); break;
                case GUI_ARMORICON:     widget = new guidata_armoricon_t                                    (player); break;
                case GUI_ARMOR:         widget = new guidata_armor_t        (def.updateGeometry, def.drawer, player); break;
                case GUI_KEYS:          widget = new guidata_keys_t                                         (player); break;
                case GUI_HEALTHICON:    widget = new guidata_healthicon_t   (def.updateGeometry, def.drawer, player); break;    
                case GUI_READYAMMOICON: widget = new guidata_readyammoicon_t(def.updateGeometry, def.drawer, player); break;
                case GUI_READYAMMO:     widget = new guidata_readyammo_t    (def.updateGeometry, def.drawer, player); break;
                case GUI_FRAGS:         widget = new guidata_frags_t        (def.updateGeometry, def.drawer, player); break;
                case GUI_SECRETS:       widget = new guidata_secrets_t                                      (player); break;
                case GUI_ITEMS:         widget = new guidata_items_t                                        (player); break;
                case GUI_KILLS:         widget = new guidata_kills_t                                        (player); break; 

                // Handled specially
                // case GUI_AUTOMAP:       widget = new AutomapWidget          (def.updateGeometry, def.drawer, player); break;
                // case GUI_LOG:           widget = new PlayerLogWidget        (def.updateGeometry, def.drawer, player); break;
                // case GUI_CHAT:          widget = new ChatWidget             (def.updateGeometry, def.drawer, player); break;
                
                default:
                    LOG_SCR_ERROR ("Unknown widget type: %i. Skipping")
                        << def.type;
                    continue;
            }

            widget->setAlignment(def.alignFlags).setFont(FID(def.fontIdx));
            GUI_AddWidget(wi);
            GUI_FindWidgetById(hud->groupIds[def.group]).as<GroupWidget>().addChild();

            if (def.id) 
            {
                *def.id = wi->id();
            }
        }
    }

    // Configure special widgets (Log, Chat, Map)
    {
        {
            PlayerLogWidget* log = new PlayerLogWidget(player);
            log->setFont(FID(GF_FONTA));
            GUI_AddWidget(log);
            hud->logId = log->id();
            GUI_FindWidgetById(hud->groupIds[UWG_TOPCENTER]).as<GroupWidget>().addChild(chat);
        }

        {
            ChatWidget* chat = new ChatWidget(player);
            chat->setFont(FID(GF_FONTA));
            GUI_AddWidget(chat);
            hud->chatId = chat->id();
            GUI_FindWidgetById(hud->groupIds[UWG_TOPCENTER]).as<GroupWidget>().addChild(chat);
        }

        {
            AutomapWidget* map = new AutomapWidget(player);
            map->setFont(FID(GF_FONTA));
            map->setCameraFollowPlayer(player);

            // TODO Possibly unneeded
            Rect_SetWidthHeight(&map->geometry(), SCREENWIDTH, SCREENHEIGHT);
            GUI_AddWidget(map);
            hud->automapId = map->id();
            GUI_FindWidgetById(hud->groupIds[UWG_AUTOMAP]).as<GroupWidget>().addChild(map);
        }
    }
}

static uiwidget_t* ST_UIChatForPlayer(int player)
{
    if(player >= 0 && player < MAXPLAYERS)
    {
        hudstate_t* hud = &hudStates[player];
        return GUI_FindObjectById(hud->chatWidgetId);
    }
    Con_Error("ST_UIChatForPlayer: Invalid player #%i.", player);
    exit(1); // Unreachable.
}

static uiwidget_t* ST_UILogForPlayer(int player)
{
    if(player >= 0 && player < MAXPLAYERS)
    {
        hudstate_t* hud = &hudStates[player];
        uiwidget_t* uiLog =  GUI_FindObjectById(hud->logWidgetId);

        DENG_ASSERT(uiLog != 0);

        return uiLog;
    }
    Con_Error("ST_UILogForPlayer: Invalid player #%i.", player);

    // TODO This is quite bad error handling
    exit(1); // Unreachable.
}

static uiwidget_t* ST_UIAutomapForPlayer(int player)
{
    if(player >= 0 && player < MAXPLAYERS)
    {
        hudstate_t* hud = &hudStates[player];
        return GUI_FindObjectById(hud->automapWidgetId);
    }
    Con_Error("ST_UIAutomapForPlayer: Invalid player #%i.", player);
    exit(1); // Unreachable.
}

static int ST_ChatResponder(int player, event_t* ev)
{
    uiwidget_t* obj = ST_UIChatForPlayer(player);
    if(NULL != obj)
    {
        return UIChat_Responder(obj, ev);
    }
    return false;
}

// }}}

// Public Logic
// ============================================================================

/*
 * HUD Lifecycle
 */

void ST_Register(void)
{
    C_VAR_FLOAT2( "hud-color-r", &cfg.common.hudColor[0], 0, 0, 1, unhideHUD )
    C_VAR_FLOAT2( "hud-color-g", &cfg.common.hudColor[1], 0, 0, 1, unhideHUD )
    C_VAR_FLOAT2( "hud-color-b", &cfg.common.hudColor[2], 0, 0, 1, unhideHUD )
    C_VAR_FLOAT2( "hud-color-a", &cfg.common.hudColor[3], 0, 0, 1, unhideHUD )
    C_VAR_FLOAT2( "hud-icon-alpha", &cfg.common.hudIconAlpha, 0, 0, 1, unhideHUD )
    C_VAR_INT(    "hud-patch-replacement", &cfg.common.hudPatchReplaceMode, 0, 0, 1 )
    C_VAR_FLOAT2( "hud-scale", &cfg.common.hudScale, 0, 0.1f, 1, unhideHUD )
    C_VAR_FLOAT(  "hud-timer", &cfg.common.hudTimer, 0, 0, 60 )

    // Displays
    C_VAR_BYTE2(  "hud-ammo", &cfg.hudShown[HUD_AMMO], 0, 0, 1, unhideHUD )
    C_VAR_BYTE2(  "hud-armor", &cfg.hudShown[HUD_ARMOR], 0, 0, 1, unhideHUD )
    C_VAR_BYTE2(  "hud-cheat-counter", &cfg.common.hudShownCheatCounters, 0, 0, 63, unhideHUD )
    C_VAR_FLOAT2( "hud-cheat-counter-scale", &cfg.common.hudCheatCounterScale, 0, .1f, 1, unhideHUD )
    C_VAR_BYTE2(  "hud-cheat-counter-show-mapopen", &cfg.common.hudCheatCounterShowWithAutomap, 0, 0, 1, unhideHUD )
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

/*
 * HUD Runtime Callbacks
 */

int ST_Responder(event_t* ev)
{
    int i, eaten = false; // WTF
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        eaten = ST_ChatResponder(i, ev);
        if(eaten) break;
    }
    return eaten;
}

void ST_Ticker(timespan_t ticLength)
{
    dd_bool const isSharpTic = DD_IsSharpTick();
    
    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        player_t* plr   = &players[i];
        hudstate_t* hud = &hudStates[i];

        if(!plr->plr->inGame)
            continue;

        // Fade in/out the fullscreen HUD.
        // TODO slide in / fade out?
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
        }

        if(hud->inited)
        {
            for(int j = 0; j < NUM_UIWIDGET_GROUPS; ++j)
            {
                GUI_FindWidgetById(hud->groupIds[k]).tick(ticLength);
            }
        }
        else
        {
            if(hud->hideTics > 0)
            {
                --hud->hideTics;
            }
            if(hud->hideTics == 0 && cfg.common.hudTimer > 0 && hud->hideAmount < 1)
            {
                hud->hideAmount += 0.1F;
            }
        }
    }
}

// referenced by d_refresh.c
void ST_Drawer(int player)
{
    if(player < 0 || player >= MAXPLAYERS) return;

    if(!players[player].plr->inGame) return;

    R_UpdateViewFilter(player);

    hudStates[player].statusbarActive = 
        (ST_ActiveHud(player) < 2) 
        || (ST_AutomapIsActive(player) && (cfg.common.automapHudDisplay == 0 || cfg.common.automapHudDisplay == 2));

    drawUIWidgetsForPlayer(players + player);
}

/*
 * HUD Control
 */

int ST_ActiveHud(int /* player */)
{
    return (cfg.common.screenBlocks < 10? 0 : cfg.common.screenBlocks - 10);
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
    if(cfg.common.msgAlign == 0)
        flags |= ALIGN_LEFT;
    else if(cfg.common.msgAlign == 2)
        flags |= ALIGN_RIGHT;
    UIWidget_SetAlignment(obj, flags);

    obj = GUI_MustFindObjectById(hud->automapWidgetId);
    // If the automap was left open; close it.
    UIAutomap_Open(obj, false, true);
    initAutomapForCurrentMap(obj);
    UIAutomap_SetCameraRotation(obj, cfg.common.automapRotate);

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

void ST_CloseAll(int player, dd_bool fast)
{
    ST_AutomapOpen(player, false, fast);
}

dd_bool ST_ChatIsActive(int player)
{
    uiwidget_t* obj = ST_UIChatForPlayer(player);
    if(NULL != obj)
    {
        return UIChat_IsActive(obj);
    }
    return false;
}

// referenced in p_inter.c
void ST_HUDUnHide(int player, hueevent_t ev)
{
    player_t* plr;

    if(player < 0 || player >= MAXPLAYERS)
        return;

    if(ev < HUE_FORCE || ev > NUMHUDUNHIDEEVENTS)
    {
        DENG_ASSERT(!"ST_HUDUnHide: Invalid event type");
        return;
    }

    plr = &players[player];
    if(!plr->plr->inGame) return;

    if(ev == HUE_FORCE || cfg.hudUnHide[ev])
    {
        hudStates[player].hideTics = (cfg.common.hudTimer * TICSPERSEC);
        hudStates[player].hideAmount = 0;
    }
}

/*
 * HUD Log
 */

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
        if(cfg.common.msgAlign == 0)
            flags |= UWGF_ALIGN_LEFT;
        else if(cfg.common.msgAlign == 2)
            flags |= UWGF_ALIGN_RIGHT;
        UIGroup_SetFlags(GUI_MustFindObjectById(hud->widgetGroupNames[UWG_TOP]), flags);
    }
#endif
}

/*
 * HUD Map
 */

/////////////////
// Map Control //
/////////////////

// referenced in p_inter.c
void ST_AutomapOpen(int player, dd_bool yes, dd_bool fast)
{
    uiwidget_t* obj = ST_UIAutomapForPlayer(player);
    if(!obj) return;
    UIAutomap_Open(obj, yes, fast);
}

dd_bool ST_AutomapIsOpen(int player)
{
    uiwidget_t* obj = ST_UIAutomapForPlayer(player);
    if(!obj) return false;
    return UIAutomap_Active(obj);
}


float ST_AutomapOpacity(int player)
{
    uiwidget_t* obj = ST_UIAutomapForPlayer(player);
    if(!obj) return 0;
    return UIAutomap_Opacity(obj);
}


static void ST_ToggleAutomapMaxZoom(int player)
{
    uiwidget_t* obj = ST_UIAutomapForPlayer(player);
    if(!obj) return;
    if(UIAutomap_SetZoomMax(obj, !UIAutomap_ZoomMax(obj)))
    {
        App_Log(0, "Maximum zoom %s in automap", UIAutomap_ZoomMax(obj)? "ON":"OFF");
    }
}

static void ST_ToggleAutomapPanMode(int player)
{
    uiwidget_t* ob = ST_UIAutomapForPlayer(player);
    if(!ob) return;
    if(UIAutomap_SetPanMode(ob, !UIAutomap_PanMode(ob)))
    {
        P_SetMessage(&players[player], LMF_NO_HIDE, (UIAutomap_PanMode(ob)? AMSTR_FOLLOWOFF : AMSTR_FOLLOWON));
    }
}


dd_bool ST_AutomapObscures(int player, int x, int y, int width, int height)
{
    RectRaw rect;
    rect.origin.x = x;
    rect.origin.y = y;
    rect.size.width  = width;
    rect.size.height = height;
    return ST_AutomapObscures2(player, &rect);
}

// referenced in d_refresh.cpp
dd_bool ST_AutomapObscures2(int player, const RectRaw* region)
{
    static const float AM_OBSCURE_TOLERANCE = 0.9999F;

    uiwidget_t* obj = ST_UIAutomapForPlayer(player);
    if(!obj) return false;
    if(UIAutomap_Active(obj))
    {
        if(cfg.common.automapOpacity * ST_AutomapOpacity(player) >= AM_OBSCURE_TOLERANCE)
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

/////////
// POI //
/////////

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

void ST_AutomapClearPoints(int player)
{
    uiwidget_t* ob = ST_UIAutomapForPlayer(player);
    if(!ob) return;

    UIAutomap_ClearPoints(ob);
    P_SetMessage(&players[player], LMF_NO_HIDE, AMSTR_MARKSCLEARED);
}

////////////////
// Appearance //
////////////////

void ST_SetAutomapCameraRotation(int player, dd_bool on)
{
    uiwidget_t* obj = ST_UIAutomapForPlayer(player);
    if(!obj) return;
    UIAutomap_SetCameraRotation(obj, on);
}

int ST_AutomapCheatLevel(int player)
{
    if(player >=0 && player < MAXPLAYERS)
        return hudStates[player].automapCheatLevel;
    return 0;
}


// referenced in m_cheat.cpp
void ST_SetAutomapCheatLevel(int player, int level)
{
    uiwidget_t* obj = ST_UIAutomapForPlayer(player);
    if(!obj) return;
    setAutomapCheatLevel(obj, level);
}

void ST_CycleAutomapCheatLevel(int player)
{
    if(player >= 0 && player < MAXPLAYERS)
    {
        hudstate_t* hud = &hudStates[player];
        ST_SetAutomapCheatLevel(player, (hud->automapCheatLevel + 1) % 3);
    }
}


// referenced in m_cheat.cpp, p_inter.c
void ST_RevealAutomap(int player, dd_bool on)
{
    uiwidget_t* obj = ST_UIAutomapForPlayer(player);
    if(!obj) return;
    UIAutomap_SetReveal(obj, on);
}

dd_bool ST_AutomapIsRevealed(int player)
{
    uiwidget_t* obj = ST_UIAutomapForPlayer(player);
    if(!obj) return false;
    return UIAutomap_Reveal(obj);
}

D_CMD(ChatOpen)
{
    int player = CONSOLEPLAYER, destination = 0;
    uiwidget_t* obj;

    if(G_QuitInProgress())
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
            App_Log(DE2_SCR_ERROR, "Invalid team number #%i (valid range: 0...%i)", destination, NUMTEAMS);
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

    if(G_QuitInProgress())
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

    if(G_QuitInProgress())
        return false;

    if(argc < 2 || argc > 3)
    {
        App_Log(DE2_SCR_NOTE, "Usage: %s (team) (macro number)", argv[0]);
        App_Log(DE2_SCR_MSG, "Send a chat macro to other player(s). "
                "If (team) is omitted, the message will be sent to all players.");
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
            App_Log(DE2_SCR_ERROR, "Invalid team number #%i (valid range: 0...%i)", destination, NUMTEAMS);
            return false;
        }
    }

    macroId = UIChat_ParseMacroId(argc == 3? argv[2] : argv[1]);
    if(-1 == macroId)
    {
        App_Log(DE2_SCR_ERROR, "Invalid macro id");
        return false;
    }

    UIChat_Activate(obj, true);
    UIChat_SetDestination(obj, destination);
    UIChat_LoadMacro(obj, macroId);
    UIChat_CommandResponder(obj, MCMD_SELECT);
    UIChat_Activate(obj, false);
    return true;
}
