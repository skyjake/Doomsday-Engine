/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999 Activision
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
 * st_stuff.c:
 */

// HEADER FILES ------------------------------------------------------------

#include "jhexen.h"
#include "st_lib.h"
#include "d_net.h"

#include "p_tick.h" // for P_IsPaused
#include "am_map.h"
#include "g_common.h"
#include "p_inventory.h"
#include "hu_inventory.h"

// MACROS ------------------------------------------------------------------

// Inventory
#define ST_INVENTORYX       (50)
#define ST_INVENTORYY       (163)

// Current inventory item.
#define ST_INVITEMX         (143)
#define ST_INVITEMY         (163)

// Current inventory item count.
#define ST_INVITEMCWIDTH    (2) // Num digits
#define ST_INVITEMCX        (174)
#define ST_INVITEMCY        (184)

// HEALTH number pos.
#define ST_HEALTHWIDTH          3
#define ST_HEALTHX              64
#define ST_HEALTHY              176

// MANA A
#define ST_MANAAWIDTH           3
#define ST_MANAAX               91
#define ST_MANAAY               181

// MANA A ICON
#define ST_MANAAICONX           77
#define ST_MANAAICONY           164

// MANA A VIAL
#define ST_MANAAVIALX           94
#define ST_MANAAVIALY           164

// MANA B
#define ST_MANABWIDTH           3
#define ST_MANABX               123
#define ST_MANABY               181

// MANA B ICON
#define ST_MANABICONX           110
#define ST_MANABICONY           164

// MANA B VIAL
#define ST_MANABVIALX           102
#define ST_MANABVIALY           164

// ARMOR number pos.
#define ST_ARMORWIDTH           2
#define ST_ARMORX               274
#define ST_ARMORY               176

// Frags pos.
#define ST_FRAGSWIDTH           3
#define ST_FRAGSX               64
#define ST_FRAGSY               176

// TYPES -------------------------------------------------------------------

typedef struct {
    boolean         stopped;
    int             hideTics;
    float           hideAmount;

    float           showBar; // Slide statusbar amount 1.0 is fully open.
    float           alpha; // Fullscreen hud alpha value.

    float           statusbarCounterAlpha;
    boolean         firstTime; // ST_Start() has just been called.
    boolean         statusbarActive; // Whether left-side main status bar is active.

    boolean         hitCenterFrame;
    int             currentInvItemFlash;
    int             armorLevel; // Current armor level.
    int             manaAIcon; // Current mana A icon index. 0 = none.
    int             manaBIcon; // Current mana B icon index. 0 = none.
    int             manaAVial; // Current mana A vial index. 0 = none.
    int             manaBVial; // Current mana B vial index. 0 = none.
    int             manaACount;
    int             manaBCount;
    int             fragsCount; // Number of frags so far in deathmatch.
    boolean         blended; // Whether to use alpha blending.

    int             healthMarker;

    int             oldHealth;

    // Widgets:
    st_multiicon_t  wManaA; // Current mana A icon.
    st_multiicon_t  wManaB; // Current mana B icon.
    st_number_t     wManaACount; // Current mana A count.
    st_number_t     wManaBCount; // Current mana B count.
    st_multiicon_t  wManaAVial; // Current mana A vial.
    st_multiicon_t  wManaBVial; // Current mana B vial.
    st_number_t     wFrags; // In deathmatch only, summary of frags stats.
    st_number_t     wHealth; // Health.
    st_number_t     wArmor; // Armor.
} hudstate_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// Console commands for the HUD/Statusbar
DEFCC(CCmdStatusBarSize);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void DrINumber(signed int val, int x, int y, float r, float g,
                      float b, float a);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DECLARATIONS ------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static hudstate_t hudStates[MAXPLAYERS];

static dpatch_t dpStatusBar;
static dpatch_t dpStatusBarTop;
static dpatch_t dpKills;
static dpatch_t dpStatBar;
static dpatch_t dpKeyBar;
static dpatch_t dpKeySlot[NUM_KEY_TYPES];
static dpatch_t dpArmorSlot[NUMARMOR];
static dpatch_t dpINumbers[10];
static dpatch_t dpNegative;
static dpatch_t dpManaAVials[2];
static dpatch_t dpManaBVials[2];
static dpatch_t dpManaAIcons[2];
static dpatch_t dpManaBIcons[2];
static dpatch_t dpInventoryBar;
static dpatch_t dpWeaponSlot[3]; // [Fighter, Cleric, Mage]
static dpatch_t dpWeaponFull[3]; // [Fighter, Cleric, Mage]
static dpatch_t dpLifeGem[3][8]; // [Fighter, Cleric, Mage][color]
static dpatch_t dpWeaponPiece1[3]; // [Fighter, Cleric, Mage]
static dpatch_t dpWeaponPiece2[3]; // [Fighter, Cleric, Mage]
static dpatch_t dpWeaponPiece3[3]; // [Fighter, Cleric, Mage]
static dpatch_t dpChain[3]; // [Fighter, Cleric, Mage]
static dpatch_t dpInvItemFlash[5];
static dpatch_t dpSpinFly[16];
static dpatch_t dpSpinMinotaur[16];
static dpatch_t dpSpinSpeed[16];
static dpatch_t dpSpinDefense[16];
static dpatch_t dpTeleIcon;

// CVARs for the HUD/Statusbar
cvar_t sthudCVars[] = {
    // HUD scale
    {"hud-scale", 0, CVT_FLOAT, &cfg.hudScale, 0.1f, 10},

    {"hud-status-size", CVF_PROTECTED, CVT_INT, &cfg.statusbarScale, 1, 20},

    // HUD colour + alpha
    {"hud-color-r", 0, CVT_FLOAT, &cfg.hudColor[0], 0, 1},
    {"hud-color-g", 0, CVT_FLOAT, &cfg.hudColor[1], 0, 1},
    {"hud-color-b", 0, CVT_FLOAT, &cfg.hudColor[2], 0, 1},
    {"hud-color-a", 0, CVT_FLOAT, &cfg.hudColor[3], 0, 1},
    {"hud-icon-alpha", 0, CVT_FLOAT, &cfg.hudIconAlpha, 0, 1},

    {"hud-status-alpha", 0, CVT_FLOAT, &cfg.statusbarOpacity, 0, 1},
    {"hud-status-icon-a", 0, CVT_FLOAT, &cfg.statusbarCounterAlpha, 0, 1},

    // HUD icons
    {"hud-mana", 0, CVT_BYTE, &cfg.hudShown[HUD_MANA], 0, 2},
    {"hud-health", 0, CVT_BYTE, &cfg.hudShown[HUD_HEALTH], 0, 1},
    {"hud-currentitem", 0, CVT_BYTE, &cfg.hudShown[HUD_CURRENTITEM], 0, 1},

    // HUD displays
    {"hud-timer", 0, CVT_FLOAT, &cfg.hudTimer, 0, 60},

    {"hud-unhide-damage", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_DAMAGE], 0, 1},
    {"hud-unhide-pickup-health", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_HEALTH], 0, 1},
    {"hud-unhide-pickup-armor", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_ARMOR], 0, 1},
    {"hud-unhide-pickup-powerup", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_POWER], 0, 1},
    {"hud-unhide-pickup-weapon", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_WEAPON], 0, 1},
    {"hud-unhide-pickup-ammo", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_AMMO], 0, 1},
    {"hud-unhide-pickup-key", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_KEY], 0, 1},
    {"hud-unhide-pickup-invitem", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_INVITEM], 0, 1},
    {NULL}
};

// Console commands for the HUD/Status bar
ccmd_t sthudCCmds[] = {
    {"sbsize",      "s",    CCmdStatusBarSize},
    {NULL}
};

// CODE --------------------------------------------------------------------

/**
 * Register CVARs and CCmds for the HUD/Status bar.
 */
void ST_Register(void)
{
    int                 i;

    for(i = 0; sthudCVars[i].name; ++i)
        Con_AddVariable(sthudCVars + i);
    for(i = 0; sthudCCmds[i].name; ++i)
        Con_AddCommand(sthudCCmds + i);

    Hu_InventoryRegister();
}

static void drawAnimatedIcons(hudstate_t* hud)
{
    int             leftoff = 0;
    int             frame;
    float           iconalpha = (hud->statusbarActive? 1: hud->alpha) - (1 - cfg.hudIconAlpha);
    int             player = hud - hudStates;
    player_t*       plr = &players[player];

    // If the fullscreen mana is drawn, we need to move the icons on the left
    // a bit to the right.
    if(cfg.hudShown[HUD_MANA] == 1 && cfg.screenBlocks > 11)
        leftoff = 42;

    Draw_BeginZoom(cfg.hudScale, 2, 2);

    // Wings of wrath
    if(plr->powers[PT_FLIGHT])
    {
        if(plr->powers[PT_FLIGHT] > BLINKTHRESHOLD ||
           !(plr->powers[PT_FLIGHT] & 16))
        {
            frame = (mapTime / 3) & 15;
            if(plr->plr->mo->flags2 & MF2_FLY)
            {
                if(hud->hitCenterFrame && (frame != 15 && frame != 0))
                {
                    GL_DrawPatchLitAlpha(20 + leftoff, 19, 1, iconalpha,
                                         dpSpinFly[15].lump);
                }
                else
                {
                    GL_DrawPatchLitAlpha(20 + leftoff, 19, 1, iconalpha,
                                         dpSpinFly[frame].lump);
                    hud->hitCenterFrame = false;
                }
            }
            else
            {
                if(!hud->hitCenterFrame && (frame != 15 && frame != 0))
                {
                    GL_DrawPatchLitAlpha(20 + leftoff, 19, 1, iconalpha,
                                         dpSpinFly[frame].lump);
                    hud->hitCenterFrame = false;
                }
                else
                {
                    GL_DrawPatchLitAlpha(20 + leftoff, 19, 1, iconalpha,
                                         dpSpinFly[15].lump);
                    hud->hitCenterFrame = true;
                }
            }
        }
    }

    // Speed Boots
    if(plr->powers[PT_SPEED])
    {
        if(plr->powers[PT_SPEED] > BLINKTHRESHOLD ||
           !(plr->powers[PT_SPEED] & 16))
        {
            frame = (mapTime / 3) & 15;
            GL_DrawPatchLitAlpha(60 + leftoff, 19, 1, iconalpha,
                                 dpSpinSpeed[frame].lump);
        }
    }

    Draw_EndZoom();

    Draw_BeginZoom(cfg.hudScale, 318, 2);

    // Defensive power
    if(plr->powers[PT_INVULNERABILITY])
    {
        if(plr->powers[PT_INVULNERABILITY] > BLINKTHRESHOLD ||
           !(plr->powers[PT_INVULNERABILITY] & 16))
        {
            frame = (mapTime / 3) & 15;
            GL_DrawPatchLitAlpha(260, 19, 1, iconalpha,
                                 dpSpinDefense[frame].lump);
        }
    }

    // Minotaur Active
    if(plr->powers[PT_MINOTAUR])
    {
        if(plr->powers[PT_MINOTAUR] > BLINKTHRESHOLD ||
           !(plr->powers[PT_MINOTAUR] & 16))
        {
            frame = (mapTime / 3) & 15;
            GL_DrawPatchLitAlpha(300, 19, 1, iconalpha,
                                 dpSpinMinotaur[frame].lump);
        }
    }

    Draw_EndZoom();
}

static void drawKeyBar(hudstate_t* hud)
{
    int                 i, xPosition, temp, pClass;
    int                 player = hud - hudStates;
    player_t*           plr = &players[player];

    // Original player class (i.e. not pig).
    pClass = cfg.playerClass[player];

    xPosition = 46;
    for(i = 0; i < NUM_KEY_TYPES && xPosition <= 126; ++i)
    {
        if(plr->keys & (1 << i))
        {
            GL_DrawPatchLitAlpha(xPosition, 163, 1, hud->statusbarCounterAlpha,
                                 dpKeySlot[i].lump);
            xPosition += 20;
        }
    }

    temp =
        PCLASS_INFO(pClass)->autoArmorSave + plr->armorPoints[ARMOR_ARMOR] +
        plr->armorPoints[ARMOR_SHIELD] +
        plr->armorPoints[ARMOR_HELMET] +
        plr->armorPoints[ARMOR_AMULET];

    for(i = 0; i < NUMARMOR; ++i)
    {
        if(!plr->armorPoints[i])
            continue;

        if(plr->armorPoints[i] <= (PCLASS_INFO(pClass)->armorIncrement[i] >> 2))
        {
            GL_DrawPatchLitAlpha(150 + 31 * i, 164, 1, hud->statusbarCounterAlpha * 0.3,
                                 dpArmorSlot[i].lump);
        }
        else if(plr->armorPoints[i] <=
                (PCLASS_INFO(pClass)->armorIncrement[i] >> 1))
        {
            GL_DrawPatchLitAlpha(150 + 31 * i, 164, 1, hud->statusbarCounterAlpha * 0.6,
                                 dpArmorSlot[i].lump);
        }
        else
        {
            GL_DrawPatchLitAlpha(150 + 31 * i, 164, 1, hud->statusbarCounterAlpha,
                                 dpArmorSlot[i].lump);
        }
    }
}

static void drawWeaponPieces(hudstate_t* hud)
{
    int                 player = hud - hudStates, pClass;
    player_t*           plr = &players[player];
    float               alpha;

    // Original player class (i.e. not pig).
    pClass = cfg.playerClass[player];

    alpha = MINMAX_OF(0.f, cfg.statusbarOpacity - hud->hideAmount, 1.f);

    if(plr->pieces == 7)
    {
        GL_DrawPatchLitAlpha(190, 162, 1, hud->statusbarCounterAlpha,
                             dpWeaponFull[pClass].lump);
    }
    else
    {
        if(plr->pieces & WPIECE1)
        {
            GL_DrawPatchLitAlpha(PCLASS_INFO(pClass)->pieceX[0],
                                 162, 1, hud->statusbarCounterAlpha,
                                 dpWeaponPiece1[pClass].lump);
        }

        if(plr->pieces & WPIECE2)
        {
            GL_DrawPatchLitAlpha(PCLASS_INFO(pClass)->pieceX[1],
                                 162, 1, hud->statusbarCounterAlpha,
                                 dpWeaponPiece2[pClass].lump);
        }

        if(plr->pieces & WPIECE3)
        {
            GL_DrawPatchLitAlpha(PCLASS_INFO(pClass)->pieceX[2],
                                 162, 1, hud->statusbarCounterAlpha,
                                 dpWeaponPiece3[pClass].lump);
        }
    }
}

static void drawChain(hudstate_t* hud)
{
    static int theirColors[] = {
        157, // Blue
        177, // Red
        137, // Yellow
        198, // Green
        215, // Jade
        32,  // White
        106, // Hazel
        234  // Purple
    };

    int                 x, y, w, h, cw;
    int                 gemoffset = 36, pClass, pColor;
    float               healthPos, gemXOffset;
    float               gemglow, rgb[3];
    int                 player = hud - hudStates;
    player_t*           plr = &players[player];

    hud->oldHealth = hud->healthMarker;

    // Original player class (i.e. not pig).
    pClass = cfg.playerClass[player];

    healthPos = MINMAX_OF(0, hud->healthMarker / 100.f, 100);

    if(!IS_NETGAME)
    {
        pColor = 1; // Always use the red life gem (the second gem).
    }
    else
    {
        pColor = cfg.playerColor[player];

        if(pClass == PCLASS_FIGHTER)
        {
            if(pColor == 0)
                pColor = 2;
            else if(pColor == 2)
                pColor = 0;
        }
    }

    gemglow = healthPos;

    // Draw the chain.
    x = 44;
    y = 193;
    w = ST_WIDTH - 44 - 44;
    h = 7;
    cw = (float) w / dpChain[pClass].width;

    DGL_SetPatch(dpChain[pClass].lump, DGL_REPEAT, DGL_CLAMP);

    DGL_Color4f(1, 1, 1, hud->statusbarCounterAlpha);

    gemXOffset = (w - dpLifeGem[pClass][pColor].width) * healthPos;

    if(gemXOffset > 0)
    {   // Left chain section.
        float               cw = gemXOffset / dpChain[pClass].width;

        DGL_Begin(DGL_QUADS);
            DGL_TexCoord2f(0, 1 - cw, 0);
            DGL_Vertex2f(x, y);

            DGL_TexCoord2f(0, 1, 0);
            DGL_Vertex2f(x + gemXOffset, y);

            DGL_TexCoord2f(0, 1, 1);
            DGL_Vertex2f(x + gemXOffset, y + h);

            DGL_TexCoord2f(0, 1 - cw, 1);
            DGL_Vertex2f(x, y + h);
        DGL_End();
    }

    if(gemXOffset + dpLifeGem[pClass][pColor].width < w)
    {   // Right chain section.
        float               cw =
            (w - gemXOffset - dpLifeGem[pClass][pColor].width) /
                dpChain[pClass].width;

        DGL_Begin(DGL_QUADS);
            DGL_TexCoord2f(0, 0, 0);
            DGL_Vertex2f(x + gemXOffset + dpLifeGem[pClass][pColor].width, y);

            DGL_TexCoord2f(0, cw, 0);
            DGL_Vertex2f(x + w, y);

            DGL_TexCoord2f(0, cw, 1);
            DGL_Vertex2f(x + w, y + h);

            DGL_TexCoord2f(0, 0, 1);
            DGL_Vertex2f(x + gemXOffset + dpLifeGem[pClass][pColor].width, y + h);
        DGL_End();
    }

    // Draw the life gem.
    GL_DrawPatchLitAlpha(x + gemXOffset, 193,
                         1, hud->statusbarCounterAlpha,
                         dpLifeGem[pClass][pColor].lump);

    // How about a glowing gem?
    DGL_BlendMode(BM_ADD);
    DGL_Bind(Get(DD_DYNLIGHT_TEXTURE));

    R_GetColorPaletteRGBf(0, rgb, theirColors[pColor], false);
    DGL_DrawRect(x + gemXOffset + 23, 193 - 6, 41, 24, rgb[0], rgb[1],
                 rgb[2], gemglow - (1 - hud->statusbarCounterAlpha));

    DGL_BlendMode(BM_NORMAL);
    DGL_Color4f(1, 1, 1, 1);
}

static void drawStatusBarBackground(int player)
{
    int                 x, y, w, h;
    int                 pClass;
    float               cw, cw2, ch;
    hudstate_t*         hud = &hudStates[player];
    player_t*           plr = &players[player];
    float               alpha;

    // Original class (i.e. not pig).
    pClass = cfg.playerClass[player];

    if(hud->blended)
    {
        alpha = cfg.statusbarOpacity - hud->hideAmount;
        if(!(alpha > 0))
            return;
        alpha = MINMAX_OF(0.f, alpha, 1.f);
    }
    else
        alpha = 1.0f;

    if(!(alpha < 1))
    {
        GL_DrawPatch(0, 134, dpStatusBar.lump);
        /**
         * \kludge The Hexen statusbar graphic has a chain already in the
         * image, which shows through the modified chain patches.
         * Mask out the chain on the statusbar by drawing a solid black
         * rectangle over it.
         */
        DGL_SetNoMaterial();
        DGL_DrawRect(44, 193, 232, 7, .1f, .1f, .1f, 1);
        //// \kludge end
        GL_DrawPatch(0, 134, dpStatusBarTop.lump);

        if(!Hu_InventoryIsOpen(player))
        {
            // Main interface
            if(!AM_IsActive(AM_MapForPlayer(player)))
            {
                GL_DrawPatch(38, 162, dpStatBar.lump);

                if(deathmatch)
                {
                    GL_DrawPatch_CS(38, 162, dpKills.lump);
                }

                GL_DrawPatch(190, 162, dpWeaponSlot[pClass].lump);
            }
            else
            {
                GL_DrawPatch(38, 162, dpKeyBar.lump);
                drawKeyBar(hud);
            }
        }
        else
        {
            GL_DrawPatch(38, 162, dpInventoryBar.lump);
        }
    }
    else
    {
        DGL_Color4f(1, 1, 1, alpha);
        DGL_SetPatch(dpStatusBar.lump, DGL_CLAMP, DGL_CLAMP);

        DGL_Begin(DGL_QUADS);

        // top
        x = 0;
        y = 135;
        w = ST_WIDTH;
        h = 27;
        ch = 0.41538461538461538461538461538462;

        DGL_TexCoord2f(0, 0, 0);
        DGL_Vertex2f(x, y);
        DGL_TexCoord2f(0, 1, 0);
        DGL_Vertex2f(x + w, y);
        DGL_TexCoord2f(0, 1, ch);
        DGL_Vertex2f(x + w, y + h);
        DGL_TexCoord2f(0, 0, ch);
        DGL_Vertex2f(x, y + h);

        // left statue
        x = 0;
        y = 162;
        w = 38;
        h = 38;
        cw = (float) 38 / ST_WIDTH;
        ch = 0.41538461538461538461538461538462;

        DGL_TexCoord2f(0, 0, ch);
        DGL_Vertex2f(x, y);
        DGL_TexCoord2f(0, cw, ch);
        DGL_Vertex2f(x + w, y);
        DGL_TexCoord2f(0, cw, 1);
        DGL_Vertex2f(x + w, y + h);
        DGL_TexCoord2f(0, 0, 1);
        DGL_Vertex2f(x, y + h);

        // right statue
        x = 282;
        y = 162;
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

        /**
         * \kludge The Hexen statusbar graphic has a chain already in the
         * image, which shows through the modified chain patches.
         * Mask out the chain on the statusbar by cutting a window out and
         * drawing a solid near-black rectangle to fill the hole.
         */
        DGL_DrawCutRectTiled(38, 192, 244, 8, 320, 65, 38, 192-135,
                             44, 193, 232, 7);
        DGL_SetNoMaterial();
        DGL_DrawRect(44, 193, 232, 7, .1f, .1f, .1f, alpha);
        DGL_Color4f(1, 1, 1, alpha);
        //// \kludge end

        if(!Hu_InventoryIsOpen(player))
        {
            // Main interface
            if(!AM_IsActive(AM_MapForPlayer(player)))
            {
                if(deathmatch)
                {
                    GL_DrawPatch_CS(38, 162, dpKills.lump);
                }

                // left of statbar (upto weapon puzzle display)
                DGL_SetPatch(dpStatBar.lump, DGL_CLAMP, DGL_CLAMP);
                DGL_Begin(DGL_QUADS);

                x = deathmatch ? 68 : 38;
                y = 162;
                w = deathmatch ? 122 : 152;
                h = 30;
                cw = deathmatch ? (float) 15 / 122 : 0;
                cw2 = 0.62295081967213114754098360655738;
                ch = 0.96774193548387096774193548387097;

                DGL_TexCoord2f(0, cw, 0);
                DGL_Vertex2f(x, y);
                DGL_TexCoord2f(0, cw2, 0);
                DGL_Vertex2f(x + w, y);
                DGL_TexCoord2f(0, cw2, ch);
                DGL_Vertex2f(x + w, y + h);
                DGL_TexCoord2f(0, cw, ch);
                DGL_Vertex2f(x, y + h);

                // right of statbar (after weapon puzzle display)
                x = 247;
                y = 162;
                w = 35;
                h = 30;
                cw = 0.85655737704918032786885245901639;
                ch = 0.96774193548387096774193548387097;

                DGL_TexCoord2f(0, cw, 0);
                DGL_Vertex2f(x, y);
                DGL_TexCoord2f(0, 1, 0);
                DGL_Vertex2f(x + w, y);
                DGL_TexCoord2f(0, 1, ch);
                DGL_Vertex2f(x + w, y + h);
                DGL_TexCoord2f(0, cw, ch);
                DGL_Vertex2f(x, y + h);

                DGL_End();

                GL_DrawPatch_CS(190, 162, dpWeaponSlot[pClass].lump);
            }
            else
            {
                GL_DrawPatch_CS(38, 162, dpKeyBar.lump);
            }
        }
        else
        {
            // INVBAR
            DGL_SetPatch(dpInventoryBar.lump, DGL_CLAMP, DGL_CLAMP);
            DGL_Begin(DGL_QUADS);

            x = 38;
            y = 162;
            w = 244;
            h = 30;
            ch = 0.96774193548387096774193548387097;

            DGL_TexCoord2f(0, 0, 0);
            DGL_Vertex2f(x, y);
            DGL_TexCoord2f(0, 1, 0);
            DGL_Vertex2f(x + w, y);
            DGL_TexCoord2f(0, 1, ch);
            DGL_Vertex2f(x + w, y + h);
            DGL_TexCoord2f(0, 0, ch);
            DGL_Vertex2f(x, y + h);

            DGL_End();
        }
    }
}

void ST_loadGraphics(void)
{
    int                 i;
    char                namebuf[9];


    R_CachePatch(&dpStatusBar, "H2BAR");
    R_CachePatch(&dpStatusBarTop, "H2TOP");
    R_CachePatch(&dpInventoryBar, "INVBAR");
    R_CachePatch(&dpStatBar, "STATBAR");
    R_CachePatch(&dpKeyBar, "KEYBAR");

    R_CachePatch(&dpManaAVials[0], "MANAVL1D");
    R_CachePatch(&dpManaBVials[0], "MANAVL2D");
    R_CachePatch(&dpManaAVials[1], "MANAVL1");
    R_CachePatch(&dpManaBVials[1], "MANAVL2");

    R_CachePatch(&dpManaAIcons[0], "MANADIM1");
    R_CachePatch(&dpManaBIcons[0], "MANADIM2");
    R_CachePatch(&dpManaAIcons[1], "MANABRT1");
    R_CachePatch(&dpManaBIcons[1], "MANABRT2");

    R_CachePatch(&dpNegative, "NEGNUM");
    R_CachePatch(&dpKills, "KILLS");

    for(i = 0; i < NUM_KEY_TYPES; ++i)
    {
        sprintf(namebuf, "KEYSLOT%X", i + 1);
        R_CachePatch(&dpKeySlot[i], namebuf);
    }

    for(i = 0; i < NUMARMOR; ++i)
    {
        sprintf(namebuf, "ARMSLOT%d", i + 1);
        R_CachePatch(&dpArmorSlot[i], namebuf);
    }

    for(i = 0; i < 16; ++i)
    {
        sprintf(namebuf, "SPFLY%d", i);
        R_CachePatch(&dpSpinFly[i], namebuf);

        sprintf(namebuf, "SPMINO%d", i);
        R_CachePatch(&dpSpinMinotaur[i], namebuf);

        sprintf(namebuf, "SPBOOT%d", i);
        R_CachePatch(&dpSpinSpeed[i], namebuf);

        sprintf(namebuf, "SPSHLD%d", i);
        R_CachePatch(&dpSpinDefense[i], namebuf);
    }

    // Fighter:
    R_CachePatch(&dpWeaponPiece1[PCLASS_FIGHTER], "WPIECEF1");
    R_CachePatch(&dpWeaponPiece2[PCLASS_FIGHTER], "WPIECEF2");
    R_CachePatch(&dpWeaponPiece3[PCLASS_FIGHTER], "WPIECEF3");
    R_CachePatch(&dpChain[PCLASS_FIGHTER], "CHAIN");
    R_CachePatch(&dpWeaponSlot[PCLASS_FIGHTER], "WPSLOT0");
    R_CachePatch(&dpWeaponFull[PCLASS_FIGHTER], "WPFULL0");
    R_CachePatch(&dpLifeGem[PCLASS_FIGHTER][0], "LIFEGEM");
    for(i = 1; i < 8; ++i)
    {
        sprintf(namebuf, "LIFEGMF%d", i + 1);
        R_CachePatch(&dpLifeGem[PCLASS_FIGHTER][i], namebuf);
    }

    // Cleric:
    R_CachePatch(&dpWeaponPiece1[PCLASS_CLERIC], "WPIECEC1");
    R_CachePatch(&dpWeaponPiece2[PCLASS_CLERIC], "WPIECEC2");
    R_CachePatch(&dpWeaponPiece3[PCLASS_CLERIC], "WPIECEC3");
    R_CachePatch(&dpChain[PCLASS_CLERIC], "CHAIN2");
    R_CachePatch(&dpWeaponSlot[PCLASS_CLERIC], "WPSLOT1");
    R_CachePatch(&dpWeaponFull[PCLASS_CLERIC], "WPFULL1");
    for(i = 0; i < 8; ++i)
    {
        sprintf(namebuf, "LIFEGMC%d", i + 1);
        R_CachePatch(&dpLifeGem[PCLASS_CLERIC][i], namebuf);
    }

    // Mage:
    R_CachePatch(&dpWeaponPiece1[PCLASS_MAGE], "WPIECEM1");
    R_CachePatch(&dpWeaponPiece2[PCLASS_MAGE], "WPIECEM2");
    R_CachePatch(&dpWeaponPiece3[PCLASS_MAGE], "WPIECEM3");
    R_CachePatch(&dpChain[PCLASS_MAGE], "CHAIN3");
    R_CachePatch(&dpWeaponSlot[PCLASS_MAGE], "WPSLOT2");
    R_CachePatch(&dpWeaponFull[PCLASS_MAGE], "WPFULL2");
    for(i = 0; i < 8; ++i)
    {
        sprintf(namebuf, "LIFEGMM%d", i + 1);
        R_CachePatch(&dpLifeGem[PCLASS_MAGE][i], namebuf);
    }

    for(i = 0; i < 10; ++i)
    {
        sprintf(namebuf, "IN%d", i);
        R_CachePatch(&dpINumbers[i], namebuf);
    }

    // Inventory item flash anim.
    {
    const char          invItemFlashAnim[5][9] = {
        {"USEARTIA"},
        {"USEARTIB"},
        {"USEARTIC"},
        {"USEARTID"},
        {"USEARTIE"}
    };

    for(i = 0; i < 5; ++i)
    {
        R_CachePatch(&dpInvItemFlash[i], invItemFlashAnim[i]);
    }
    }

    R_CachePatch(&dpTeleIcon, "TELEICON");
}

void ST_loadData(void)
{
    ST_loadGraphics();
}

static void initData(hudstate_t* hud)
{
    int                 player = hud - hudStates;
    player_t*           plr = &players[player];

    hud->alpha = 0.0f;
    hud->stopped = true;
    hud->showBar = 0.0f;
    hud->statusbarCounterAlpha = 0.0f;
    hud->blended = false;
    hud->oldHealth = -1;

    hud->armorLevel = 0;
    hud->manaAIcon = 0;
    hud->manaBIcon = 0;
    hud->manaAVial = 0;
    hud->manaBVial = 0;
    hud->manaACount = 0;
    hud->manaBCount = 0;
    hud->firstTime = true;
    hud->statusbarActive = true;


    ST_HUDUnHide(player, HUE_FORCE);
}

void ST_createWidgets(int player)
{
    player_t*           plr = &players[player];
    hudstate_t*         hud = &hudStates[player];

    // Health num.
    STlib_InitNum(&hud->wHealth, ST_HEALTHX, ST_HEALTHY, dpINumbers,
                  &plr->health, ST_HEALTHWIDTH, 1);

    // Frags sum.
    STlib_InitNum(&hud->wFrags, ST_FRAGSX, ST_FRAGSY, dpINumbers,
                  &hud->fragsCount, ST_FRAGSWIDTH, 1);

    // Armor num - should be colored later.
    STlib_InitNum(&hud->wArmor, ST_ARMORX, ST_ARMORY, dpINumbers,
                  &hud->armorLevel, ST_ARMORWIDTH, 1);

    // ManaA count.
    STlib_InitNum(&hud->wManaACount, ST_MANAAX, ST_MANAAY,
                  dpSmallNumbers, &hud->manaACount, ST_MANAAWIDTH, 1);

    // ManaB count.
    STlib_InitNum(&hud->wManaBCount, ST_MANABX, ST_MANABY,
                  dpSmallNumbers, &hud->manaBCount, ST_MANABWIDTH, 1);

    // Current mana A icon.
    STlib_InitMultiIcon(&hud->wManaA, ST_MANAAICONX, ST_MANAAICONY,
                        dpManaAIcons, 1);

    // Current mana B icon.
    STlib_InitMultiIcon(&hud->wManaB, ST_MANABICONX, ST_MANABICONY,
                        dpManaBIcons, 1);

    // Current mana A vial.
    STlib_InitMultiIcon(&hud->wManaAVial, ST_MANAAVIALX, ST_MANAAVIALY,
                        dpManaAVials, 1);

    // Current mana B vial.
    STlib_InitMultiIcon(&hud->wManaBVial, ST_MANABVIALX, ST_MANABVIALY,
                        dpManaBVials, 1);
}

void ST_Start(int player)
{
    hudstate_t*         hud;

    if(player < 0 || player >= MAXPLAYERS)
        return;

    hud = &hudStates[player];

    if(!hud->stopped)
        ST_Stop(player);

    initData(hud);
    ST_createWidgets(player);

    hud->stopped = false;
}

void ST_Stop(int player)
{
    hudstate_t*         hud;

    if(player < 0 || player >= MAXPLAYERS)
        return;

    hud = &hudStates[player];
    if(hud->stopped)
        return;

    hud->stopped = true;
}

void ST_Init(void)
{
    ST_loadData();
}

void ST_FlashCurrentItem(int player)
{
    player_t*           plr;
    hudstate_t*         hud;

    if(player < 0 || player >= MAXPLAYERS)
        return;

    plr = &players[player];
    if(!((plr->plr->flags & DDPF_LOCAL) && plr->plr->inGame))
        return;

    hud = &hudStates[player];

    hud->currentInvItemFlash = 4;
}

void ST_updateWidgets(int player)
{
    int                 i, pClass;
    hudstate_t*         hud = &hudStates[player];
    player_t*           plr = &players[player];

    // Original player class (i.e. not pig).
    pClass = cfg.playerClass[player];

    if(hud->blended)
    {
        hud->statusbarCounterAlpha =
            MINMAX_OF(0.f, cfg.statusbarCounterAlpha - hud->hideAmount, 1.f);
    }
    else
        hud->statusbarCounterAlpha = 1.0f;

    // Used by w_frags widget.
    hud->fragsCount = 0;

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        if(!players[i].plr->inGame)
            continue;

        hud->fragsCount += plr->frags[i] * (i != player ? 1 : -1);
    }

    // Armor
    hud->armorLevel = FixedDiv(
        PCLASS_INFO(pClass)->autoArmorSave + plr->armorPoints[ARMOR_ARMOR] +
        plr->armorPoints[ARMOR_SHIELD] +
        plr->armorPoints[ARMOR_HELMET] +
        plr->armorPoints[ARMOR_AMULET], 5 * FRACUNIT) >> FRACBITS;

    // mana A
    hud->manaACount = plr->ammo[AT_BLUEMANA].owned;

    // mana B
    hud->manaBCount = plr->ammo[AT_GREENMANA].owned;

    hud->manaAIcon = hud->manaBIcon = hud->manaAVial = hud->manaBVial = -1;

    // Mana
    if(!(plr->ammo[AT_BLUEMANA].owned > 0))
        hud->manaAIcon = 0; // Draw dim Mana icon.

    if(!(plr->ammo[AT_GREENMANA].owned > 0))
        hud->manaBIcon = 0; // Draw dim Mana icon.

    // Update mana graphics based upon mana count weapon type
    if(plr->readyWeapon == WT_FIRST)
    {
        hud->manaAIcon = 0;
        hud->manaBIcon = 0;

        hud->manaAVial = 0;
        hud->manaBVial = 0;
    }
    else if(plr->readyWeapon == WT_SECOND)
    {
        // If there is mana for this weapon, make it bright!
        if(hud->manaAIcon == -1)
        {
            hud->manaAIcon = 1;
        }

        hud->manaAVial = 1;

        hud->manaBIcon = 0;
        hud->manaBVial = 0;
    }
    else if(plr->readyWeapon == WT_THIRD)
    {
        hud->manaAIcon = 0;
        hud->manaAVial = 0;

        // If there is mana for this weapon, make it bright!
        if(hud->manaBIcon == -1)
        {
            hud->manaBIcon = 1;
        }

        hud->manaBVial = 1;
    }
    else
    {
        hud->manaAVial = 1;
        hud->manaBVial = 1;

        // If there is mana for this weapon, make it bright!
        if(hud->manaAIcon == -1)
        {
            hud->manaAIcon = 1;
        }
        if(hud->manaBIcon == -1)
        {
            hud->manaBIcon = 1;
        }
    }
}

void ST_Ticker(void)
{
    int                 i;

    Hu_InventoryTicker();

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        player_t*           plr = &players[i];
        hudstate_t*         hud = &hudStates[i];

        if(!(plr->plr->inGame && (plr->plr->flags & DDPF_LOCAL)))
            continue;

        ST_updateWidgets(i);

        if(!P_IsPaused())
        {
            int                 delta;
            int                 curHealth;

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

            if(hud->currentInvItemFlash > 0)
                hud->currentInvItemFlash--;

            curHealth = MAX_OF(plr->plr->mo->health, 0);
            if(curHealth < hud->healthMarker)
            {
                delta = MINMAX_OF(1, (hud->healthMarker - curHealth) >> 2, 6);
                hud->healthMarker -= delta;
            }
            else if(curHealth > hud->healthMarker)
            {
                delta = MINMAX_OF(1, (curHealth - hud->healthMarker) >> 2, 6);
                hud->healthMarker += delta;
            }
        }
    }
}

/**
 * Sets the new palette based upon the current values of
 * player_t->damageCount and player_t->bonusCount.
 */
void ST_doPaletteStuff(int player, boolean forceChange)
{
    int                 palette = 0;
    player_t*           plr;

    if(player < 0 || player >= MAXPLAYERS)
        return;

    plr = &players[player];

    if(G_GetGameState() == GS_MAP)
    {
        plr = &players[CONSOLEPLAYER];
        if(plr->poisonCount)
        {
            palette = 0;
            palette = (plr->poisonCount + 7) >> 3;
            if(palette >= NUMPOISONPALS)
            {
                palette = NUMPOISONPALS - 1;
            }
            palette += STARTPOISONPALS;
        }
        else if(plr->damageCount)
        {
            palette = (plr->damageCount + 7) >> 3;
            if(palette >= NUMREDPALS)
            {
                palette = NUMREDPALS - 1;
            }
            palette += STARTREDPALS;
        }
        else if(plr->bonusCount)
        {
            palette = (plr->bonusCount + 7) >> 3;
            if(palette >= NUMBONUSPALS)
            {
                palette = NUMBONUSPALS - 1;
            }
            palette += STARTBONUSPALS;
        }
        else if(plr->plr->mo->flags2 & MF2_ICEDAMAGE)
        {   // Frozen player
            palette = STARTICEPAL;
        }
    }

    // $democam
    if(palette)
    {
        plr->plr->flags |= DDPF_VIEW_FILTER;
        R_GetFilterColor(plr->plr->filterColor, palette);
    }
    else
        plr->plr->flags &= ~DDPF_VIEW_FILTER;
}

static void drawWidgets(hudstate_t* hud)
{
    int                 player = hud - hudStates;
    player_t*           plr = &players[player];
    float               alpha = hud->statusbarCounterAlpha;

    hud->oldHealth = -1;
    if(!Hu_InventoryIsOpen(player))
    {
        if(!AM_IsActive(AM_MapForPlayer(player)))
        {
            inventoryitemtype_t readyItem;

            // Frags
            if(deathmatch)
                STlib_DrawNum(&hud->wFrags, alpha);
            else
                STlib_DrawNum(&hud->wHealth, alpha);

            STlib_DrawNum(&hud->wArmor, alpha);

            if(hud->manaACount > 0)
                STlib_DrawNum(&hud->wManaACount, alpha);

            if(hud->manaBCount > 0)
                STlib_DrawNum(&hud->wManaBCount, alpha);

            STlib_DrawMultiIcon(&hud->wManaA, hud->manaAIcon, alpha);
            STlib_DrawMultiIcon(&hud->wManaB, hud->manaBIcon, alpha);
            STlib_DrawMultiIcon(&hud->wManaAVial, hud->manaAVial, alpha);
            STlib_DrawMultiIcon(&hud->wManaBVial, hud->manaBVial, alpha);

            // Draw the mana bars
            DGL_SetNoMaterial();
            DGL_DrawRect(95, 165, 3,
                         22 - (22 * plr->ammo[AT_BLUEMANA].owned) / MAX_MANA,
                         0, 0, 0, alpha);
            DGL_DrawRect(103, 165, 3,
                         22 - (22 * plr->ammo[AT_GREENMANA].owned) / MAX_MANA,
                         0, 0, 0, alpha);

            // Current inventory item.
            if((readyItem = P_InventoryReadyItem(player)) != IIT_NONE)
            {
                lumpnum_t           patch;

                if(hud->currentInvItemFlash > 0)
                {
                    patch = dpInvItemFlash[hud->currentInvItemFlash % 5].lump;
                }
                else
                {
                    patch = P_GetInvItem(readyItem-1)->patchLump;
                }

                DGL_Color4f(1, 1, 1, alpha);
                GL_DrawPatch_CS(ST_INVITEMX, ST_INVITEMY, patch);

                if(!(hud->currentInvItemFlash > 0))
                {
                    uint                count =
                        P_InventoryCount(player, readyItem);

                    if(count > 1)
                        Hu_DrawSmallNum(count, ST_INVITEMCWIDTH, ST_INVITEMCX,
                                        ST_INVITEMCY, alpha);
                }
            }
        }
        else
        {
            drawKeyBar(hud);
        }
    }
    else
    {   // Draw Inventory
        Hu_InventoryDraw2(player, ST_INVENTORYX, ST_INVENTORYY, alpha);
    }
}

/**
 * Draws a three digit number.
 */
static void DrINumber(signed int val, int x, int y, float r, float g,
                      float b, float a)
{
    int         oldval;

    DGL_Color4f(r,g,b,a);

    // Make sure it's a three digit number.
    if(val < -999)
        val = -999;
    if(val > 999)
        val = 999;

    oldval = val;
    if(val < 0)
    {
        val = -val;
        if(val > 99)
        {
            val = 99;
        }
        if(val > 9)
        {
            GL_DrawPatch_CS(x + 8, y, dpINumbers[val / 10].lump);
            GL_DrawPatch_CS(x, y, dpNegative.lump);
        }
        else
        {
            GL_DrawPatch_CS(x + 8, y, dpNegative.lump);
        }
        val = val % 10;
        GL_DrawPatch_CS(x + 16, y, dpINumbers[val].lump);
        return;
    }
    if(val > 99)
    {
        GL_DrawPatch_CS(x, y, dpINumbers[val / 100].lump);
    }
    val = val % 100;
    if(val > 9 || oldval > 99)
    {
        GL_DrawPatch_CS(x + 8, y, dpINumbers[val / 10].lump);
    }
    val = val % 10;
    GL_DrawPatch_CS(x + 16, y, dpINumbers[val].lump);

}

#if 0 // Unused atm
/**
 * Displays sound debugging information.
 */
static void DrawSoundInfo(void)
{
    int         i;
    SoundInfo_t s;
    ChanInfo_t *c;
    char        text[32];
    int         x;
    int         y;
    int         xPos[7] = { 1, 75, 112, 156, 200, 230, 260 };

    if(mapTime & 16)
    {
        MN_DrTextA("*** SOUND DEBUG INFO ***", xPos[0], 20);
    }
    S_GetChannelInfo(&s);
    if(s.channelCount == 0)
    {
        return;
    }
    x = 0;
    MN_DrTextA("NAME", xPos[x++], 30);
    MN_DrTextA("MO.T", xPos[x++], 30);
    MN_DrTextA("MO.X", xPos[x++], 30);
    MN_DrTextA("MO.Y", xPos[x++], 30);
    MN_DrTextA("ID", xPos[x++], 30);
    MN_DrTextA("PRI", xPos[x++], 30);
    MN_DrTextA("DIST", xPos[x++], 30);
    for(i = 0; i < s.channelCount; ++i)
    {
        c = &s.chan[i];
        x = 0;
        y = 40 + i * 10;
        if(c->mo == NULL)
        {                       // Channel is unused
            MN_DrTextA("------", xPos[0], y);
            continue;
        }
        sprintf(text, "%s", c->name);
        strupr(text);
        MN_DrTextA(text, xPos[x++], y);
        sprintf(text, "%d", c->mo->type);
        MN_DrTextA(text, xPos[x++], y);
        sprintf(text, "%d", c->mo->x >> FRACBITS);
        MN_DrTextA(text, xPos[x++], y);
        sprintf(text, "%d", c->mo->y >> FRACBITS);
        MN_DrTextA(text, xPos[x++], y);
        sprintf(text, "%d", c->id);
        MN_DrTextA(text, xPos[x++], y);
        sprintf(text, "%d", S_sfx[c->id].usefulness);
        MN_DrTextA(text, xPos[x++], y);
        sprintf(text, "%d", c->distance);
    }
}
#endif

/**
 * Unhides the current HUD display if hidden.
 *
 * @param player        The player whoose HUD to (maybe) unhide.
 * @param event         The HUD Update Event type to check for triggering.
 */
void ST_HUDUnHide(int player, hueevent_t ev)
{
    player_t*           plr;

    if(ev < HUE_FORCE || ev > NUMHUDUNHIDEEVENTS)
        return;

    plr = &players[player];
    if(!(plr->plr->inGame && (plr->plr->flags & DDPF_LOCAL)))
        return;

    if(ev == HUE_FORCE || cfg.hudUnHide[ev])
    {
        hudStates[player].hideTics = (cfg.hudTimer * TICSPERSEC);
        hudStates[player].hideAmount = 0;
    }
}

/**
 * All drawing for the status bar starts and ends here
 */
void ST_doRefresh(int player)
{
    hudstate_t*         hud;
    boolean             statusbarVisible;

    if(player < 0 || player > MAXPLAYERS)
        return;

    hud = &hudStates[player];

    statusbarVisible = (cfg.statusbarScale < 20 ||
        (cfg.statusbarScale == 20 && hud->showBar < 1.0f));

    hud->firstTime = false;

    if(statusbarVisible)
    {
        float               fscale = cfg.statusbarScale / 20.0f;
        float               h = 200 * (1 - fscale);

        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PushMatrix();
        DGL_Translatef(160 - 320 * fscale / 2, h / hud->showBar, 0);
        DGL_Scalef(fscale, fscale, 1);
    }

    drawStatusBarBackground(player);
    if(!Hu_InventoryIsOpen(player) && !AM_IsActive(AM_MapForPlayer(player)))
        drawWeaponPieces(hud);
    drawChain(hud);
    drawWidgets(hud);

    if(statusbarVisible)
    {
        // Restore the normal modelview matrix.
        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PopMatrix();
    }
}

void ST_doFullscreenStuff(int player)
{
    int                 i, temp;
    hudstate_t*         hud = &hudStates[player];
    player_t*           plr = &players[player];
    float               textAlpha =
        MINMAX_OF(0.f, hud->alpha - hud->hideAmount - ( 1 - cfg.hudColor[3]), 1.f);
    float               iconAlpha =
        MINMAX_OF(0.f, hud->alpha - hud->hideAmount - ( 1 - cfg.hudIconAlpha), 1.f);

    if(cfg.hudShown[HUD_HEALTH])
    {
        Draw_BeginZoom(cfg.hudScale, 5, 198);
        if(plr->plr->mo->health > 0)
        {
            DrBNumber(plr->plr->mo->health, 5, 180,
                      cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2],
                      textAlpha);
        }
        else
        {
            DrBNumber(0, 5, 180, cfg.hudColor[0], cfg.hudColor[1],
                      cfg.hudColor[2], textAlpha);
        }
        Draw_EndZoom();
    }

    if(cfg.hudShown[HUD_MANA])
    {
        int     dim[2] = { dpManaAIcons[0].lump, dpManaBIcons[0].lump };
        int     bright[2] = { dpManaAIcons[0].lump, dpManaBIcons[0].lump };
        int     patches[2] = { 0, 0 };
        int     ypos = cfg.hudShown[HUD_MANA] == 2 ? 152 : 2;

        for(i = 0; i < 2; i++)
            if(!(plr->ammo[i].owned > 0))
                patches[i] = dim[i];
        if(plr->readyWeapon == WT_FIRST)
        {
            for(i = 0; i < 2; i++)
                patches[i] = dim[i];
        }
        if(plr->readyWeapon == WT_SECOND)
        {
            if(!patches[0])
                patches[0] = bright[0];
            patches[1] = dim[1];
        }
        if(plr->readyWeapon == WT_THIRD)
        {
            patches[0] = dim[0];
            if(!patches[1])
                patches[1] = bright[1];
        }
        if(plr->readyWeapon == WT_FOURTH)
        {
            for(i = 0; i < 2; ++i)
                if(!patches[i])
                    patches[i] = bright[i];
        }
        Draw_BeginZoom(cfg.hudScale, 2, ypos);
        for(i = 0; i < 2; ++i)
        {
            GL_DrawPatchLitAlpha(2, ypos + i * 13, 1, iconAlpha, patches[i]);
            DrINumber(plr->ammo[i].owned, 18, ypos + i * 13,
                      1, 1, 1, textAlpha);
        }
        Draw_EndZoom();
    }

    if(deathmatch)
    {
        temp = 0;
        for(i = 0; i < MAXPLAYERS; ++i)
        {
            if(players[i].plr->inGame)
            {
                temp += plr->frags[i];
            }
        }
        Draw_BeginZoom(cfg.hudScale, 2, 198);
        DrINumber(temp, 45, 185, 1, 1, 1, textAlpha);
        Draw_EndZoom();
    }

    if(!Hu_InventoryIsOpen(player))
    {
        if(cfg.hudShown[HUD_CURRENTITEM])
        {
            if(hud->currentInvItemFlash > 0)
            {
Draw_BeginZoom(cfg.hudScale, 318, 198);
                GL_DrawPatchLitAlpha(286, 170, 1, iconAlpha / 2,
                                     dpInvItemBox.lump);
                GL_DrawPatchLitAlpha(286, 169, 1, iconAlpha,
                                     dpInvItemFlash[hud->currentInvItemFlash % 5].lump);
Draw_EndZoom();
            }
            else
            {
                inventoryitemtype_t readyItem = P_InventoryReadyItem(player);

                if(readyItem != IIT_NONE)
                {
                    uint                count;
                    lumpnum_t           patch =
                        P_GetInvItem(readyItem-1)->patchLump;

Draw_BeginZoom(cfg.hudScale, 318, 198);

                    GL_DrawPatchLitAlpha(290, 170, 1, iconAlpha / 2,
                                         dpInvItemBox.lump);
                    GL_DrawPatchLitAlpha(289, 169, 1, iconAlpha, patch);
                    if((count = P_InventoryCount(player, readyItem)) > 1)
                        Hu_DrawSmallNum(count, ST_INVITEMCWIDTH, 317, 192,
                                        textAlpha);
Draw_EndZoom();
                }
            }
        }
    }
    else
    {
#define BORDER              2
#define INVENTORY_HEIGHT    30
#define INVENTORY_X         SCREENWIDTH / 2 - 2
#define INVENTORY_Y         SCREENHEIGHT - INVENTORY_HEIGHT - BORDER

        float               textAlpha =
            MINMAX_OF(0.f, hud->alpha - hud->hideAmount - ( 1 - cfg.hudColor[3]), 1.f);
        float               iconAlpha =
            MINMAX_OF(0.f, hud->alpha - hud->hideAmount - ( 1 - cfg.hudIconAlpha), 1.f);

        Hu_InventoryDraw(player, INVENTORY_X, INVENTORY_Y, hud->alpha,
                         textAlpha, iconAlpha);

#undef BORDER
#undef INVENTORY_HEIGHT
#undef INVENTORY_X
#undef INVENTORY_Y
    }
}

void ST_Drawer(int player, int fullscreenmode, boolean refresh)
{
    hudstate_t*         hud;
    player_t*           plr;

    if(player < 0 || player >= MAXPLAYERS)
        return;

    plr = &players[player];
    if(!((plr->plr->flags & DDPF_LOCAL) && plr->plr->inGame))
        return;

    hud = &hudStates[player];

    hud->firstTime = hud->firstTime || refresh;
    hud->statusbarActive = (fullscreenmode < 2) ||
        (AM_IsActive(AM_MapForPlayer(player)) &&
         (cfg.automapHudDisplay == 0 || cfg.automapHudDisplay == 2));

    // Do palette shifts
    ST_doPaletteStuff(player, false);

    // Either slide the status bar in or fade out the fullscreen hud
    if(hud->statusbarActive)
    {
        if(hud->alpha > 0.0f)
        {
            hud->statusbarActive = 0;
            hud->alpha-=0.1f;
        }
        else if(hud->showBar < 1.0f)
            hud->showBar += 0.1f;
    }
    else
    {
        if(fullscreenmode == 3)
        {
            if(hud->alpha > 0.0f)
            {
                hud->alpha-=0.1f;
                fullscreenmode = 2;
            }
        }
        else
        {
            if(hud->showBar > 0.0f)
            {
                hud->showBar -= 0.1f;
                hud->statusbarActive = 1;
            }
            else if(hud->alpha < 1.0f)
                hud->alpha += 0.1f;
        }
    }

    // Always try to render statusbar with alpha in fullscreen modes
    if(fullscreenmode)
        hud->blended = 1;
    else
        hud->blended = 0;

    if(hud->statusbarActive)
    {
        ST_doRefresh(player);
    }
    else if(fullscreenmode != 3)
    {
        ST_doFullscreenStuff(player);
    }

    drawAnimatedIcons(hud);
}

/**
 * Draw teleport icon and show it on screen.
 */
void Draw_TeleportIcon(void)
{
    // Dedicated servers don't draw anything.
    if(IS_DEDICATED)
        return;

    GL_DrawRawScreen(W_CheckNumForName("TRAVLPIC"), 0, 0);
    GL_DrawPatch(100, 68, dpTeleIcon.lump);
}

/**
 * Console command to change the size of the status bar.
 */
DEFCC(CCmdStatusBarSize)
{
    int                 min = 1, max = 20, *val = &cfg.statusbarScale;

    if(!stricmp(argv[1], "+"))
        (*val)++;
    else if(!stricmp(argv[1], "-"))
        (*val)--;
    else
        *val = strtol(argv[1], NULL, 0);

    *val = MINMAX_OF(min, *val, max);

    // Update the view size if necessary.
    R_SetViewSize(cfg.screenBlocks);
    ST_HUDUnHide(CONSOLEPLAYER, HUE_FORCE); // So the user can see the change.

    return true;
}
