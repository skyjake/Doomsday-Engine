/**\file st_stuff.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

using namespace de;

// Widgets
// ============================================================================

#include "hu_stuff.h"

#include "hud/widgets/groupwidget.h"

#include "hud/automapstyle.h"
#include "hud/widgets/automapwidget.h"

#include "hud/widgets/playerlogwidget.h"
#include "hud/widgets/chatwidget.h"

#include "hud/widgets/readyammoiconwidget.h"
#include "hud/widgets/readyammowidget.h"
#include "hud/widgets/keyswidget.h"

#include "hud/widgets/healthiconwidget.h"
#include "hud/widgets/healthwidget.h"

#include "hud/widgets/armoriconwidget.h"
#include "hud/widgets/armorwidget.h"

#include "hud/widgets/itemswidget.h"
#include "hud/widgets/killswidget.h"
#include "hud/widgets/secretswidget.h"

#include "hud/widgets/fragswidget.h"

// Types / Constants
// ============================================================================

const int STARTREDPALS   = 1;
const int NUMREDPALS     = 8;
const int STARTBONUSPALS = 9;
const int NUMBONUSPALS   = 4;
const int ST_WIDTH       = SCREENWIDTH;
const int ST_HEIGHT      = SCREENHEIGHT;

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

    DE_ASSERT(plr);

    const int playerId    = (plr - players);
    const int hudMode     = ST_ActiveHud(playerId);
    hudstate_t* hud       = &hudStates[playerId];

    Size2Raw portSize;    R_ViewPortSize(playerId, &portSize);
    Point2Raw portOrigin; R_ViewPortOrigin(playerId, &portOrigin);

    // TODO float const automapOpacity
    // Automap Group
    {
        HudWidget& amGroup = GUI_FindWidgetById(hud->groupIds[UWG_AUTOMAP]);
        amGroup.setOpacity(ST_AutomapOpacity(playerId));
        amGroup.setMaximumSize(portSize);

        GUI_DrawWidgetXY(&amGroup, 0, 0);
    }

    // Ingame UI
    // hudMode >= 3 presumeable refers to `No-Hud`
    // There ought to be some constants for this
    if (hud->alpha > 0 || hudMode < 3) {
        float uiScale;
        R_ChooseAlignModeAndScaleFactor(&uiScale, SCREENWIDTH, SCREENHEIGHT,
                                        portSize.width, portSize.height, SCALEMODE_SMART_STRETCH);

        float opacity = de::min(1.0F, hud->alpha) * (1 - hud->hideAmount);

        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PushMatrix();
        DGL_Translatef(portOrigin.x, portOrigin.y, 0);
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

            Size2_Raw(Rect_Size(&bottomGroup.geometry()), &regionRendered);
        }

        // Map name widget group
        {
            HudWidget& mapNameGroup = GUI_FindWidgetById(hud->groupIds[UWG_MAPNAME]);
            mapNameGroup.setOpacity(ST_AutomapOpacity(playerId));

            Size2Raw remainingVertical = {{{displayRegion.size.width,
                                            displayRegion.size.height - (regionRendered.height > 0 ? regionRendered.height : 0)}}};

            mapNameGroup.setMaximumSize(remainingVertical);

            GUI_DrawWidget(&mapNameGroup, &displayRegion.origin);
        }


        // Remaining widgets: Top Center, Counters (Kills, Secrets, Items)
        {
            // Kills widget, etc, are always visible unless no-hud
            if (hudMode < 3)
            {
                opacity = 1.0F;
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

                GUI_DrawWidget(&counters, &displayRegion.origin);
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
    DE_ASSERT(hud);

    hud->stopped         = true;

    // Reset/Initialize Elements
    {
        GUI_FindWidgetById(hud->healthId).as<guidata_health_t>().reset();
        GUI_FindWidgetById(hud->armorIconId).as<guidata_armoricon_t>().reset();
        GUI_FindWidgetById(hud->armorId).as<guidata_armor_t>().reset();
        GUI_FindWidgetById(hud->keysId).as<guidata_keys_t>().reset();
        // GUI_FindWidgetById(hud->fragsId).as<guidata_frags_t>().reset();

        GUI_FindWidgetById(hud->secretsId).as<guidata_secrets_t>().reset();
        GUI_FindWidgetById(hud->itemsId).as<guidata_items_t>().reset();
        GUI_FindWidgetById(hud->killsId).as<guidata_kills_t>().reset();

        GUI_FindWidgetById(hud->logId).as<PlayerLogWidget>().clear();
    }

    ST_HUDUnHide(hud - hudStates, HUE_FORCE);
}

static void setAutomapCheatLevel(AutomapWidget& map, int level)
{
    hudstate_t* hud = &hudStates[map.player()];

    hud->automapCheatLevel = level;

    dint flags = map.flags()
        & ~ (AWF_SHOW_ALLLINES
            |AWF_SHOW_THINGS
            |AWF_SHOW_SPECIALLINES
            |AWF_SHOW_VERTEXES
            |AWF_SHOW_LINE_NORMALS);

    if (hud->automapCheatLevel >= 1)
    {
        flags |= AWF_SHOW_ALLLINES;
    }

    if (hud->automapCheatLevel == 2)
    {
        flags |= AWF_SHOW_THINGS
              |  AWF_SHOW_SPECIALLINES;
    }
    else if (hud->automapCheatLevel > 2)
    {
        flags |= AWF_SHOW_VERTEXES
              |  AWF_SHOW_LINE_NORMALS;
    }

    map.setFlags(flags);
}

static void initAutomapForCurrentMap(AutomapWidget& map)
{
    hudstate_t* hud = &hudStates[map.player()];

    map.reset();

    const AABoxd *mapBounds = reinterpret_cast<AABoxd *>(DD_GetVariable(DD_MAP_BOUNDING_BOX));
    map.setMapBounds(mapBounds->minX, mapBounds->maxX, mapBounds->minY, mapBounds->maxY);

    // Disable cheats for network games
    if (IS_NETGAME)
    {
        setAutomapCheatLevel(map, 0);
    }

    // Silent clear POI's
    map.clearAllPoints(true);

    // Reset map scale
    if (map.cameraZoomMode())
    {
        map.setScale(0);
    }

    // Enable keyboard guide for "baby" mode
    if (gfw_Rule(skill) == SM_BABY && cfg.common.automapBabyKeys)
    {
        map.setFlags(map.flags() | AWF_SHOW_KEYS);
    }

    // Show player arrow in cheat/dev mode
    if (hud->automapCheatLevel > 0)
    {
        AutomapStyle* mapStyle = map.style();
        mapStyle->setObjectSvg(AMO_THINGPLAYER, VG_CHEATARROW);
    }

    // Focus camera on currently followed map object, if applicable
    {
        mobj_t* followTarget = map.followMobj();

        if (followTarget)
        {
            map.setCameraOrigin(Vec2d(followTarget->origin), true);
        }
    }

    // Hide things that need not be seen
    map.reveal(false);

    // Add initially visible lines (i.e. those immediately present when player has spawned
    for(int lineNumber = 0; lineNumber < numlines; ++lineNumber)
    {
        xline_t *xline = &xlines[lineNumber];

        if(xline->flags & ML_MAPPED)
        {
            P_SetLineAutomapVisibility(map.player(), lineNumber, true);
        }
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
        HudElementName  type;
        int             alignFlags;
        int             group;
        gamefontid_t    fontIdx;

        void (*updateGeometry) (HudWidget* obj);
        void (*drawer) (HudWidget* obj, Point2Raw const* origin);

        uiwidgetid_t* id;
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

        for (const uiwidgetgroupdef_t &def : widgetGroupDefs)
        {
            HudWidget* grp;
            {
                GroupWidget* groupWidget = new GroupWidget(player);
                groupWidget->setAlignment(def.alignFlags).setFont(1);
                groupWidget->setFlags(def.groupFlags);
                groupWidget->setOrder(def.order);
                groupWidget->setPadding(def.padding);

                grp = groupWidget;
            }

            GUI_AddWidget(grp);
            hud->groupIds[def.group] = grp->id();
        }

        // Add BOTTOMLEFT2 to BOTTOMLEFT
        GUI_FindWidgetById(hud->groupIds[UWG_BOTTOMLEFT]).as<GroupWidget>()
            .addChild(&GUI_FindWidgetById(hud->groupIds[UWG_BOTTOMLEFT2]));
    }

    DE_ASSERT(player >= 0 && player < MAXPLAYERS);

    // Create a table of needed widgets and initialize them
    {
        uiwidgetdef_t const widgetDefs[] = {
            { GUI_HEALTHICON,       ALIGN_BOTTOMLEFT,   UWG_BOTTOMLEFT2,    GF_NONE,    nullptr,                                                                nullptr,                                                &hud->healthIconId      },
            { GUI_HEALTH,           ALIGN_BOTTOMLEFT,   UWG_BOTTOMLEFT2,    GF_FONTB,   function_cast<UpdateGeometryFunc>(HealthWidget_UpdateGeometry),         function_cast<DrawFunc>(HealthWidget_Draw),             &hud->healthId          },
            { GUI_READYAMMOICON,    ALIGN_BOTTOMLEFT,   UWG_BOTTOMLEFT2,    GF_NONE,    function_cast<UpdateGeometryFunc>(ReadyAmmoIconWidget_UpdateGeometry),  function_cast<DrawFunc>(ReadyAmmoIconWidget_Drawer),    &hud->readyAmmoIconId   },
            { GUI_READYAMMO,        ALIGN_BOTTOM,       UWG_BOTTOMCENTER,   GF_FONTB,   function_cast<UpdateGeometryFunc>(ReadyAmmo_UpdateGeometry),            function_cast<DrawFunc>(ReadyAmmo_Drawer),              &hud->readyAmmoId       },

            // { GUI_FRAGS,            ALIGN_BOTTOMCENTER, UWG_BOTTOMCENTER,   GF_FONTA,   function_cast<UpdateGeometryFunc>(FragsWidget_UpdateGeometry),          function_cast<DrawFunc>(FragsWidget_Draw),              &hud->fragsId           },

            { GUI_KEYS,             ALIGN_BOTTOMRIGHT,  UWG_BOTTOMRIGHT,    GF_NONE,    nullptr,                                                                nullptr,                                                &hud->keysId            },
            { GUI_ARMOR,            ALIGN_BOTTOMRIGHT,  UWG_BOTTOMRIGHT,    GF_FONTB,   function_cast<UpdateGeometryFunc>(Armor_UpdateGeometry),                function_cast<DrawFunc>(ArmorWidget_Draw),              &hud->armorId           },
            { GUI_ARMORICON,        ALIGN_BOTTOMRIGHT,  UWG_BOTTOMRIGHT,    GF_NONE,    nullptr,                                                                nullptr,                                                &hud->armorIconId       },

            { GUI_SECRETS,          ALIGN_TOPLEFT,      UWG_COUNTERS,       GF_FONTA,   nullptr,                                                                nullptr,                                                &hud->secretsId         },
            { GUI_ITEMS,            ALIGN_TOPLEFT,      UWG_COUNTERS,       GF_FONTA,   nullptr,                                                                nullptr,                                                &hud->itemsId           },
            { GUI_KILLS,            ALIGN_TOPLEFT,      UWG_COUNTERS,       GF_FONTA,   nullptr,                                                                nullptr,                                                &hud->killsId           }
        };

        // Initialize widgets
        for (const uiwidgetdef_t &def : widgetDefs)
        {
            HudWidget* widget = nullptr;
            switch (def.type)
            {
                case GUI_HEALTHICON:    widget = new guidata_healthicon_t                         (player, SPR_STIM); break;
                case GUI_HEALTH:        widget = new guidata_health_t       (def.updateGeometry, def.drawer, player); break;
                case GUI_ARMORICON:     widget = new guidata_armoricon_t                (player, SPR_ARM1, SPR_ARM2); break;
                case GUI_ARMOR:         widget = new guidata_armor_t        (def.updateGeometry, def.drawer, player); break;
                case GUI_KEYS:          widget = new guidata_keys_t                                         (player); break;
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
            GUI_AddWidget(widget);
            GUI_FindWidgetById(hud->groupIds[def.group]).as<GroupWidget>().addChild(widget);

            if (def.id)
            {
                *def.id = widget->id();
            }
        }
    }

    // Configure the bottom row of groups by adding UWG_BOTTOM{LEFT, CENTER, RIGHT} to UWG_BOTTOM in that order
    {
        GroupWidget& bottom = GUI_FindWidgetById(hud->groupIds[UWG_BOTTOM]).as<GroupWidget>();

        bottom.addChild(&GUI_FindWidgetById(hud->groupIds[UWG_BOTTOMLEFT]));
        bottom.addChild(&GUI_FindWidgetById(hud->groupIds[UWG_BOTTOMCENTER]));
        bottom.addChild(&GUI_FindWidgetById(hud->groupIds[UWG_BOTTOMRIGHT]));
    }

    // Configure special widgets (Log, Chat, Map)
    {
        {
            PlayerLogWidget* log = new PlayerLogWidget(player);
            log->setFont(FID(GF_FONTA));
            GUI_AddWidget(log);
            hud->logId = log->id();
            GUI_FindWidgetById(hud->groupIds[UWG_TOPCENTER]).as<GroupWidget>().addChild(log);
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

// TODO This is used everywhere, and could probably be added to libcommon
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

// }}}

// Commands (must be declared before ST_Register)
// ============================================================================

D_CMD(ChatOpen)
{
    DE_UNUSED(src);

    if (G_QuitInProgress())
    {
        return false;
    }

    ChatWidget* chat = ST_TryFindChatWidget(CONSOLEPLAYER);

    if (chat)
    {
        int destination = 0;
        {
            if (argc == 2)
            {
                destination = parseTeamNumber(argv[1]);
                if (destination < 0)
                {
                    LOG_SCR_ERROR("Invalid team number: %i (valid numbers in range 0 through %i")
                            << destination
                            << NUMTEAMS;
                    return false;
                }
            }
        }

        chat->setDestination(destination);
        chat->activate();
        return true;
    }
    else
    {
        return false;
    }
}

D_CMD(ChatAction)
{
    DE_UNUSED(src, argc);

    if (G_QuitInProgress())
    {
        return false;
    }

    ChatWidget* chat = ST_TryFindChatWidget(CONSOLEPLAYER);

    // TODO Design a more extensible and modular means of doing this
    if (chat && !chat->isActive())
    {
        const String command = String(argv[0] + 4 /* ghetto substring */);

        if (command.compareWithoutCase("complete") == 0)
        {
            return chat->handleMenuCommand(MCMD_SELECT);
        }
        else if (command.compareWithoutCase("cancel") == 0)
        {
            return chat->handleMenuCommand(MCMD_CLOSE);
        }
        else if (command.compareWithoutCase("delete") == 0)
        {
            return chat->handleMenuCommand(MCMD_DELETE);
        }
        else
        {
            return true;
        }
    }
    else
    {
        return false;
    }
}

D_CMD(ChatSendMacro)
{
    DE_UNUSED(src);

    if (G_QuitInProgress())
    {
        return false;
    }

    if (argc < 2 || argc > 3)
    {
        LOG_SCR_NOTE("Usage: %s (team) (macro number)")
                << argv[0];

        LOG_SCR_MSG("Send a chat macro to other player(s). "
                    "If (team) is omitted, the message with be broadcast to all players.");

        return true;
    }
    else
    {
        ChatWidget* chat = ST_TryFindChatWidget(CONSOLEPLAYER);

        if (chat)
        {
            int destination = 0;
            {
                if (argc == 3)
                {
                    destination = parseTeamNumber(argv[1]);

                    if (destination < 0)
                    {
                        LOG_SCR_ERROR("Invalid team number: %i. Valid numbers are within the range 0 through %i")
                            << destination
                            << NUMTEAMS;

                        return false;
                    }
                }
            }

            int macroId = parseMacroId(argc == 3 ? argv[2] : argv[1]);

            if (macroId >= 0)
            {
                chat->activate();
                chat->setDestination(destination);
                chat->messageAppendMacro(macroId);
                chat->handleMenuCommand(MCMD_SELECT);
                chat->activate(false);
            }

            return true;
        }
        else
        {
            return false;
        }
    }
}

// Public Logic
// ============================================================================

/*
 * HUD Lifecycle
 */

void ST_Register(void)
{
    /*
     * Convars
     */

    C_VAR_FLOAT2( "hud-color-r",                    &cfg.common.hudColor[0],                    0, 0,       1,     unhideHUD    );
    C_VAR_FLOAT2( "hud-color-g",                    &cfg.common.hudColor[1],                    0, 0,       1,     unhideHUD    );
    C_VAR_FLOAT2( "hud-color-b",                    &cfg.common.hudColor[2],                    0, 0,       1,     unhideHUD    );
    C_VAR_FLOAT2( "hud-color-a",                    &cfg.common.hudColor[3],                    0, 0,       1,     unhideHUD    );
    C_VAR_FLOAT2( "hud-icon-alpha",                 &cfg.common.hudIconAlpha,                   0, 0,       1,     unhideHUD    );
    C_VAR_INT(    "hud-patch-replacement",          &cfg.common.hudPatchReplaceMode,            0, 0,       1                   );
    C_VAR_FLOAT2( "hud-scale",                      &cfg.common.hudScale,                       0, 0.1F,    1,     unhideHUD    );
    C_VAR_FLOAT(  "hud-timer",                      &cfg.common.hudTimer,                       0, 0,       60                  );
    C_VAR_BYTE2(  "hud-ammo",                       &cfg.hudShown[HUD_AMMO],                    0, 0,       1,     unhideHUD    );
    C_VAR_BYTE2(  "hud-armor",                      &cfg.hudShown[HUD_ARMOR],                   0, 0,       1,     unhideHUD    );
    C_VAR_BYTE2(  "hud-cheat-counter",              &cfg.common.hudShownCheatCounters,          0, 0,       63,    unhideHUD    );
    C_VAR_FLOAT2( "hud-cheat-counter-scale",        &cfg.common.hudCheatCounterScale,           0, 0.1F,    1,     unhideHUD    );
    C_VAR_BYTE2(  "hud-cheat-counter-show-mapopen", &cfg.common.hudCheatCounterShowWithAutomap, 0, 0,       1,     unhideHUD    );
    C_VAR_BYTE2(  "hud-frags",                      &cfg.hudShown[HUD_FRAGS],                   0, 0,       1,     unhideHUD    );
    C_VAR_BYTE2(  "hud-health",                     &cfg.hudShown[HUD_HEALTH],                  0, 0,       1,     unhideHUD    );
    C_VAR_BYTE2(  "hud-keys",                       &cfg.hudShown[HUD_KEYS],                    0, 0,       1,     unhideHUD    );
    C_VAR_BYTE2(  "hud-power",                      &cfg.hudShown[HUD_INVENTORY],               0, 0,       1,     unhideHUD    );
    C_VAR_BYTE(   "hud-unhide-damage",              &cfg.hudUnHide[HUE_ON_DAMAGE],              0, 0,       1                   );
    C_VAR_BYTE(   "hud-unhide-pickup-ammo",         &cfg.hudUnHide[HUE_ON_PICKUP_AMMO],         0, 0,       1                   );
    C_VAR_BYTE(   "hud-unhide-pickup-armor",        &cfg.hudUnHide[HUE_ON_PICKUP_ARMOR],        0, 0,       1                   );
    C_VAR_BYTE(   "hud-unhide-pickup-health",       &cfg.hudUnHide[HUE_ON_PICKUP_HEALTH],       0, 0,       1                   );
    C_VAR_BYTE(   "hud-unhide-pickup-key",          &cfg.hudUnHide[HUE_ON_PICKUP_KEY],          0, 0,       1                   );
    C_VAR_BYTE(   "hud-unhide-pickup-powerup",      &cfg.hudUnHide[HUE_ON_PICKUP_POWER],        0, 0,       1                   );
    C_VAR_BYTE(   "hud-unhide-pickup-weapon",       &cfg.hudUnHide[HUE_ON_PICKUP_WEAPON],       0, 0,       1                   );

    /*
     * Commands
     */

    C_CMD("beginchat",       nullptr,    ChatOpen       )
    C_CMD("chatcancel",      "",         ChatAction     )
    C_CMD("chatcomplete",    "",         ChatAction     )
    C_CMD("chatdelete",      "",         ChatAction     )
    C_CMD("chatsendmacro",   nullptr,    ChatSendMacro  )
}

void ST_Init(void)
{
    ST_InitAutomapStyle();

    for(int i = 0; i < MAXPLAYERS; ++i)
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

// Handles chat events
int ST_Responder(event_t* ev)
{
    for (int playerId = 0; playerId < MAXPLAYERS; ++playerId)
    {
        ChatWidget* chat = ST_TryFindChatWidget(playerId);
        if (chat)
        {
            int nEaten = chat->handleEvent(*ev);
            if (nEaten > 0)
            {
                return nEaten;
            }
        }
    }

    return 0;
}

void ST_Ticker(timespan_t ticLength)
{
    const dd_bool isSharpTic = DD_IsSharpTick();

    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        player_t* plr   = &players[i];
        hudstate_t* hud = &hudStates[i];

        if(!plr->plr->inGame)
            continue;

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
                GUI_FindWidgetById(hud->groupIds[j]).tick(ticLength);
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
    if (player > 0 && player < MAXPLAYERS)
    {
        hudstate_t* hud = &hudStates[player];

        if (!hud->stopped)
        {
            ST_Stop(player);
        }

        // Initialize all widgets to default values
        initData(hud);

        /*
         * Set user preferences for layout, etc...
         */

        // Top Center widget group
        {
            HudWidget& topCenter = GUI_FindWidgetById(hud->groupIds[UWG_TOPCENTER]);

            int alignFlags = topCenter.alignment() & ~(ALIGN_LEFT | ALIGN_RIGHT);

            if (cfg.common.msgAlign == 0)
            {
                alignFlags |= ALIGN_LEFT;
            }
            else if (cfg.common.msgAlign == 2)
            {
                alignFlags |= ALIGN_RIGHT;
            }

            topCenter.setAlignment(alignFlags);
        }

        // Automap
        {
            AutomapWidget& map = GUI_FindWidgetById(hud->automapId).as<AutomapWidget>();

            // Reset automap open state to closed
            map.open(false, true /* close instantly */);

            initAutomapForCurrentMap(map);
            map.setCameraRotationMode(CPP_BOOL(cfg.common.automapRotate));
        }

        hud->stopped = false;
    }
}

void ST_Stop(int player)
{
    if (player > 0 && player < MAXPLAYERS)
    {
        hudStates[player].stopped = true;
    }
}

void ST_CloseAll(int player, dd_bool fast)
{
    ST_AutomapOpen(player, false, fast);
}

void HU_WakeWidgets(int player)
{
    if (player < 0)
    {
        for (uint i = 0; i < MAXPLAYERS; ++i)
        {
            if (players[i].plr->inGame)
            {
                HU_WakeWidgets(i);
            }
        }
    }
    else
    {
        if (players[player].plr->inGame)
        {
            ST_Start(player);
        }
    }
}

/**
 * No
 */
dd_bool ST_StatusBarIsActive(int)
{
    // No.
    return false;
}

/**
 * No
 */
float ST_StatusBarShown(int)
{
    // No.
    return 0;
}

dd_bool ST_ChatIsActive(int player)
{
    ChatWidget* chat = ST_TryFindChatWidget(player);

    if (chat)
    {
        return chat->isActive();
    }
    else
    {
        return false;
    }
}

// referenced in p_inter.c
void ST_HUDUnHide(int player, hueevent_t ev)
{
    player_t* plr;

    if(player < 0 || player >= MAXPLAYERS)
        return;

    if(ev < HUE_FORCE || ev > NUMHUDUNHIDEEVENTS)
    {
        DE_ASSERT_FAIL("ST_HUDUnHide: Invalid event type");
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
    PlayerLogWidget* log = ST_TryFindLogWidget(player);

    if (log)
    {
        log->post(flags, msg);
    }
}

void ST_LogRefresh(int player)
{
    PlayerLogWidget* log = ST_TryFindLogWidget(player);

    if (log)
    {
        log->refresh();
    }
}

void ST_LogEmpty(int player)
{
    PlayerLogWidget* log = ST_TryFindLogWidget(player);

    if (log)
    {
        log->clear();
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
void ST_AutomapOpen(int player, dd_bool yes, dd_bool instant)
{
    AutomapWidget* map = ST_TryFindAutomapWidget(player);

    if (map)
    {
        map->open(CPP_BOOL(yes), CPP_BOOL(instant));
    }
}

dd_bool ST_AutomapIsOpen(int player)
{
    AutomapWidget* map = ST_TryFindAutomapWidget(player);

    if (map)
    {
        return map->isOpen();
    }
    else
    {
        return false;
    }
}


float ST_AutomapOpacity(int player)
{
    AutomapWidget* map = ST_TryFindAutomapWidget(player);

    if (map)
    {
        return map->opacityEX();
    }
    else
    {
        return 0;
    }
}


void ST_AutomapZoomMode(int player)
{
    AutomapWidget* map = ST_TryFindAutomapWidget(player);

    if (map)
    {
        map->setCameraZoomMode(!map->cameraZoomMode());
    }
}

void ST_AutomapFollowMode(int player)
{
    AutomapWidget* map = ST_TryFindAutomapWidget(player);

    if (map)
    {
        map->setCameraFollowMode(!map->cameraFollowMode());
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

    AutomapWidget* map = ST_TryFindAutomapWidget(player);

    if (map && map->isOpen())
    {
        return (cfg.common.automapOpacity * ST_AutomapOpacity(player) >= AM_OBSCURE_TOLERANCE);
    }
    else
    {
        return false;
    }
}

/////////
// POI //
/////////

int ST_AutomapAddPoint(int player, coord_t x, coord_t y, coord_t z)
{
    AutomapWidget* map = ST_TryFindAutomapWidget(player);

    if (map)
    {
        return map->addPoint(Vec3d(x, y, z));
    }
    else
    {
        return -1;
    }
}

void ST_AutomapClearPoints(int player)
{
    AutomapWidget* map = ST_TryFindAutomapWidget(player);

    if (map)
    {
        map->clearAllPoints();
    }
}

////////////////
// Appearance //
////////////////

void ST_SetAutomapCameraRotation(int player, dd_bool on)
{
    AutomapWidget* map = ST_TryFindAutomapWidget(player);

    if (map)
    {
        map->setCameraRotationMode(CPP_BOOL(on));
    }
}

int ST_AutomapCheatLevel(int player)
{
    if (player >= 0 && player < MAXPLAYERS)
    {
        return hudStates[player].automapCheatLevel;
    }
    else
    {
        return 0;
    }
}


// referenced in m_cheat.cpp
void ST_SetAutomapCheatLevel(int player, int level)
{
    AutomapWidget* map = ST_TryFindAutomapWidget(player);

    if (map)
    {
        setAutomapCheatLevel(*map, level);
    }
}

void ST_CycleAutomapCheatLevel(int player)
{
    if (player >= 0 && player < MAXPLAYERS)
    {
        ST_SetAutomapCheatLevel(player, (hudStates[player].automapCheatLevel + 1) % 3);
    }
}


// referenced in m_cheat.cpp, p_inter.c
void ST_RevealAutomap(int player, dd_bool on)
{
    AutomapWidget* map = ST_TryFindAutomapWidget(player);

    if (map)
    {
        map->reveal(on);
    }
}

dd_bool ST_AutomapIsRevealed(int player)
{
    AutomapWidget* map = ST_TryFindAutomapWidget(player);

    if (map)
    {
        return map->isRevealed();
    }
    else
    {
        return false;
    }
}

/*
 * HUD Widget Access
 */

ChatWidget* ST_TryFindChatWidget(int player)
{
    if(player >= 0 && player < MAXPLAYERS)
    {
        HudWidget* widgetPointer = GUI_TryFindWidgetById(hudStates[player].chatId);

        if (widgetPointer)
        {
            return maybeAs<ChatWidget>(widgetPointer);
        }
    }

    return nullptr;
}

PlayerLogWidget* ST_TryFindLogWidget(int player)
{
    if(player >= 0 && player < MAXPLAYERS)
    {
        HudWidget* widgetPointer = GUI_TryFindWidgetById(hudStates[player].logId);

        if (widgetPointer)
        {
            return maybeAs<PlayerLogWidget>(widgetPointer);
        }
    }

    return nullptr;
}

AutomapWidget* ST_TryFindAutomapWidget(int player)
{
    if(player >= 0 && player < MAXPLAYERS)
    {
        HudWidget* widgetPointer = GUI_TryFindWidgetById(hudStates[player].automapId);

        if (widgetPointer)
        {
            return maybeAs<AutomapWidget>(widgetPointer);
        }
    }

    return nullptr;
}
