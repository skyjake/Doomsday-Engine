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

// MACROS ------------------------------------------------------------------

// inventory
#define ST_INVENTORYX           50
#define ST_INVENTORYY           163

// how many inventory slots are visible
#define NUMVISINVSLOTS 7

// invSlot artifact count (relative to each slot)
#define ST_INVCOUNTOFFX         30
#define ST_INVCOUNTOFFY         22

// current artifact (sbbar)
#define ST_ARTIFACTWIDTH        24
#define ST_ARTIFACTX            143
#define ST_ARTIFACTY            163

// current artifact count (sbar)
#define ST_ARTIFACTCWIDTH       2
#define ST_ARTIFACTCX           174
#define ST_ARTIFACTCY           184

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
    float           alpha; // Fullscreen hud alpha value.

    float           showBar; // Slide statusbar amount 1.0 is fully open.
    float           statusbarCounterAlpha;

    boolean         firstTime; // ST_Start() has just been called.
    boolean         blended; // Whether to use alpha blending.
    boolean         statusbarOn; // Whether left-side main status bar is active.
    boolean         fragsOn; // !deathmatch.
    int             fragsCount; // Number of frags so far in deathmatch.

    int             healthMarker;

    int             oldHealth;
    int             oldArti;
    int             oldArtiCount;

    int             artifactFlash;
    int             invSlot[NUMVISINVSLOTS]; // Current inventory slot indices. 0 = none.
    int             invSlotCount[NUMVISINVSLOTS]; // Current inventory slot count indices. 0 = none.
    int             armorLevel; // Current armor level.
    int             artiCI; // Current artifact index. 0 = none.
    int             manaAIcon; // Current mana A icon index. 0 = none.
    int             manaBIcon; // Current mana B icon index. 0 = none.
    int             manaAVial; // Current mana A vial index. 0 = none.
    int             manaBVial; // Current mana B vial index. 0 = none.
    int             manaACount;
    int             manaBCount;

    int             inventoryTics;
    boolean         inventory;
    boolean         hitCenterFrame;

    // Widgets:
    st_multicon_t   wArti; // Current artifact icon.
    st_number_t     wArtiCount; // Current artifact count.
    st_multicon_t   wInvSlot[NUMVISINVSLOTS]; // Inventory slot icons.
    st_number_t     wInvSlotCount[NUMVISINVSLOTS]; // Inventory slot counts.
    st_multicon_t   wManaA; // Current mana A icon.
    st_multicon_t   wManaB; // Current mana B icon.
    st_number_t     wManaACount; // Current mana A count.
    st_number_t     wManaBCount; // Current mana B count.
    st_multicon_t   wManaAVial; // Current mana A vial.
    st_multicon_t   wManaBVial; // Current mana B vial.
    st_number_t     wHealth; // Health.
    st_number_t     wFrags; // In deathmatch only, summary of frags stats.
    st_number_t     wArmor; // Armor.
} hudstate_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// Console commands for the HUD/Statusbar
DEFCC(CCmdStatusBarSize);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void DrINumber(signed int val, int x, int y, float r, float g,
                      float b, float a);
static void DrBNumber(signed int val, int x, int y, float Red, float Green,
                      float Blue, float Alpha);

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
static dpatch_t dpSelectBox;
static dpatch_t dpINumbers[10];
static dpatch_t dpNegative;
static dpatch_t dpSmallNumbers[10];
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
static dpatch_t dpIndicatorLeft1;
static dpatch_t dpIndicatorLeft2;
static dpatch_t dpIndicatorRight1;
static dpatch_t dpIndicatorRight2;
static dpatch_t dpArtifacts[38];
static dpatch_t dpSpinFly[16];
static dpatch_t dpSpinMinotaur[16];
static dpatch_t dpSpinSpeed[16];
static dpatch_t dpSpinDefense[16];
static dpatch_t dpTeleIcon;

// CVARs for the HUD/Statusbar
cvar_t sthudCVars[] = {
    // HUD scale
    {"hud-scale", 0, CVT_FLOAT, &PLRPROFILE.hud.scale, 0.1f, 10},

    {"hud-status-size", CVF_PROTECTED, CVT_INT, &PLRPROFILE.statusbar.scale, 1, 20},

    // HUD colour + alpha
    {"hud-color-r", 0, CVT_FLOAT, &PLRPROFILE.hud.color[0], 0, 1},
    {"hud-color-g", 0, CVT_FLOAT, &PLRPROFILE.hud.color[1], 0, 1},
    {"hud-color-b", 0, CVT_FLOAT, &PLRPROFILE.hud.color[2], 0, 1},
    {"hud-color-a", 0, CVT_FLOAT, &PLRPROFILE.hud.color[3], 0, 1},
    {"hud-icon-alpha", 0, CVT_FLOAT, &PLRPROFILE.hud.iconAlpha, 0, 1},

    {"hud-status-alpha", 0, CVT_FLOAT, &PLRPROFILE.statusbar.opacity, 0, 1},
    {"hud-status-icon-a", 0, CVT_FLOAT, &PLRPROFILE.statusbar.counterAlpha, 0, 1},

    // HUD icons
    {"hud-mana", 0, CVT_BYTE, &PLRPROFILE.hud.shown[HUD_MANA], 0, 2},
    {"hud-health", 0, CVT_BYTE, &PLRPROFILE.hud.shown[HUD_HEALTH], 0, 1},
    {"hud-artifact", 0, CVT_BYTE, &PLRPROFILE.hud.shown[HUD_ARTI], 0, 1},

    // HUD displays
    {"hud-inventory-timer", 0, CVT_FLOAT, &PLRPROFILE.inventory.timer, 0, 30},

    {"hud-timer", 0, CVT_FLOAT, &PLRPROFILE.hud.timer, 0, 60},

    {"hud-unhide-damage", 0, CVT_BYTE, &PLRPROFILE.hud.unHide[HUE_ON_DAMAGE], 0, 1},
    {"hud-unhide-pickup-health", 0, CVT_BYTE, &PLRPROFILE.hud.unHide[HUE_ON_PICKUP_HEALTH], 0, 1},
    {"hud-unhide-pickup-armor", 0, CVT_BYTE, &PLRPROFILE.hud.unHide[HUE_ON_PICKUP_ARMOR], 0, 1},
    {"hud-unhide-pickup-powerup", 0, CVT_BYTE, &PLRPROFILE.hud.unHide[HUE_ON_PICKUP_POWER], 0, 1},
    {"hud-unhide-pickup-weapon", 0, CVT_BYTE, &PLRPROFILE.hud.unHide[HUE_ON_PICKUP_WEAPON], 0, 1},
    {"hud-unhide-pickup-ammo", 0, CVT_BYTE, &PLRPROFILE.hud.unHide[HUE_ON_PICKUP_AMMO], 0, 1},
    {"hud-unhide-pickup-key", 0, CVT_BYTE, &PLRPROFILE.hud.unHide[HUE_ON_PICKUP_KEY], 0, 1},
    {"hud-unhide-pickup-invitem", 0, CVT_BYTE, &PLRPROFILE.hud.unHide[HUE_ON_PICKUP_INVITEM], 0, 1},
    {NULL}
};

// Console commands for the HUD/Status bar
ccmd_t  sthudCCmds[] = {
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
}

static void drawAnimatedIcons(hudstate_t* hud)
{
    int             leftoff = 0;
    int             frame;
    float           iconalpha = (hud->statusbarOn? 1: hud->alpha) - (1 - PLRPROFILE.hud.iconAlpha);
    int             player = hud - hudStates;
    player_t*       plr = &players[player];

    // If the fullscreen mana is drawn, we need to move the icons on the left
    // a bit to the right.
    if(PLRPROFILE.hud.shown[HUD_MANA] == 1 && PLRPROFILE.screen.blocks > 11)
        leftoff = 42;

    Draw_BeginZoom(PLRPROFILE.hud.scale, 2, 2);

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

    Draw_BeginZoom(PLRPROFILE.hud.scale, 318, 2);

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
    pClass = gs.players[player].pClass;

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
    pClass = gs.players[player].pClass;

    alpha = MINMAX_OF(0.f, PLRPROFILE.statusbar.opacity - hud->hideAmount, 1.f);

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
    pClass = gs.players[player].pClass;

    healthPos = MINMAX_OF(0, hud->healthMarker / 100.f, 100);

    if(!IS_NETGAME)
        pColor = 1; // Always use the red life gem (the second gem).
    else
        pColor = gs.players[player].color;

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

    R_PalIdxToRGB(rgb, theirColors[pColor], false);
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
    pClass = gs.players[player].pClass;

    if(hud->blended)
    {
        alpha = PLRPROFILE.statusbar.opacity - hud->hideAmount;
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

        if(!hud->inventory)
        {
            // Main interface
            if(!AM_IsActive(AM_MapForPlayer(player)))
            {
                GL_DrawPatch(38, 162, dpStatBar.lump);

                if(GAMERULES.deathmatch)
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

        if(!hud->inventory)
        {
            // Main interface
            if(!AM_IsActive(AM_MapForPlayer(player)))
            {
                if(GAMERULES.deathmatch)
                {
                    GL_DrawPatch_CS(38, 162, dpKills.lump);
                }

                // left of statbar (upto weapon puzzle display)
                DGL_SetPatch(dpStatBar.lump, DGL_CLAMP, DGL_CLAMP);
                DGL_Begin(DGL_QUADS);

                x = GAMERULES.deathmatch ? 68 : 38;
                y = 162;
                w = GAMERULES.deathmatch ? 122 : 152;
                h = 30;
                cw = GAMERULES.deathmatch ? (float) 15 / 122 : 0;
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
    char                artifactlist[][9] = {
        {"USEARTIA"}, // use artifact flash
        {"USEARTIB"},
        {"USEARTIC"},
        {"USEARTID"},
        {"USEARTIE"},
        {"ARTIBOX"}, // none
        {"ARTIINVU"}, // invulnerability
        {"ARTIPTN2"}, // health
        {"ARTISPHL"}, // superhealth
        {"ARTIHRAD"}, // healing radius
        {"ARTISUMN"}, // summon maulator
        {"ARTITRCH"}, // torch
        {"ARTIPORK"}, // egg
        {"ARTISOAR"}, // fly
        {"ARTIBLST"}, // blast radius
        {"ARTIPSBG"}, // poison bag
        {"ARTITELO"}, // teleport other
        {"ARTISPED"}, // speed
        {"ARTIBMAN"}, // boost mana
        {"ARTIBRAC"}, // boost armor
        {"ARTIATLP"}, // teleport
        {"ARTISKLL"},
        {"ARTIBGEM"},
        {"ARTIGEMR"},
        {"ARTIGEMG"},
        {"ARTIGMG2"},
        {"ARTIGEMB"},
        {"ARTIGMB2"},
        {"ARTIBOK1"},
        {"ARTIBOK2"},
        {"ARTISKL2"},
        {"ARTIFWEP"},
        {"ARTICWEP"},
        {"ARTIMWEP"},
        {"ARTIGEAR"},
        {"ARTIGER2"},
        {"ARTIGER3"},
        {"ARTIGER4"}
    };

    R_CachePatch(&dpStatusBar, "H2BAR");
    R_CachePatch(&dpStatusBarTop, "H2TOP");
    R_CachePatch(&dpInventoryBar, "INVBAR");
    R_CachePatch(&dpStatBar, "STATBAR");
    R_CachePatch(&dpKeyBar, "KEYBAR");
    R_CachePatch(&dpSelectBox, "SELECTBOX");

    R_CachePatch(&dpManaAVials[0], "MANAVL1D");
    R_CachePatch(&dpManaBVials[0], "MANAVL2D");
    R_CachePatch(&dpManaAVials[1], "MANAVL1");
    R_CachePatch(&dpManaBVials[1], "MANAVL2");

    R_CachePatch(&dpManaAIcons[0], "MANADIM1");
    R_CachePatch(&dpManaBIcons[0], "MANADIM2");
    R_CachePatch(&dpManaAIcons[1], "MANABRT1");
    R_CachePatch(&dpManaBIcons[1], "MANABRT2");

    R_CachePatch(&dpIndicatorLeft1, "invgeml1");
    R_CachePatch(&dpIndicatorLeft2, "invgeml2");
    R_CachePatch(&dpIndicatorRight1, "invgemr1");
    R_CachePatch(&dpIndicatorRight2, "invgemr2");
    R_CachePatch(&dpNegative, "NEGNUM");
    R_CachePatch(&dpKills, "KILLS");

    for(i = 0; i < NUM_KEY_TYPES; ++i)
    {
        sprintf(namebuf, "KEYSLOT%X", i + 1);
        R_CachePatch(&dpKeySlot[i], namebuf);
    }

    for(i = 0; i < NUMARMOR; ++i)
    {
        sprintf(namebuf, "ARMSLOT", i + 1);
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

    for(i = 0; i < 10; ++i)
    {
        sprintf(namebuf, "SMALLIN%d", i);
        R_CachePatch(&dpSmallNumbers[i], namebuf);
    }

    // Artifact icons (+5 for the use-artifact flash patches)
    for(i = 0; i < (NUM_ARTIFACT_TYPES + 5); ++i)
    {
        sprintf(namebuf, "%s", artifactlist[i]);
        R_CachePatch(&dpArtifacts[i], namebuf);
    }

    R_CachePatch(&dpTeleIcon, "TELEICON");
}

void ST_loadData(void)
{
    ST_loadGraphics();
}

static void initData(hudstate_t* hud)
{
    int                 i;
    int                 player = hud - hudStates;
    player_t*           plr = &players[player];

    // Ensure the HUD widget lib has been inited.
    STlib_init();

    hud->alpha = 0.0f;
    hud->stopped = true;
    hud->showBar = 0.0f;
    hud->statusbarCounterAlpha = 0.0f;
    hud->blended = false;
    hud->oldHealth = -1;
    hud->oldArti = 0;
    hud->oldArtiCount = 0;

    hud->armorLevel = 0;
    hud->artiCI = 0;
    hud->manaAIcon = 0;
    hud->manaBIcon = 0;
    hud->manaAVial = 0;
    hud->manaBVial = 0;
    hud->manaACount = 0;
    hud->manaBCount = 0;
    hud->inventory = false;

    hud->firstTime = true;

    hud->statusbarOn = true;

    for(i = 0; i < NUMVISINVSLOTS; ++i)
    {
        hud->invSlot[i] = 0;
        hud->invSlotCount[i] = 0;
    }

    ST_HUDUnHide(player, HUE_FORCE);
}

void ST_createWidgets(int player)
{
    int                 i, width, temp;
    player_t*           plr = &players[player];
    hudstate_t*         hud = &hudStates[player];

    // Health num.
    STlib_initNum(&hud->wHealth, ST_HEALTHX, ST_HEALTHY, dpINumbers,
                  &plr->health, &hud->statusbarOn, ST_HEALTHWIDTH,
                  &hud->statusbarCounterAlpha);

    // Frags sum.
    STlib_initNum(&hud->wFrags, ST_FRAGSX, ST_FRAGSY, dpINumbers,
                  &hud->fragsCount, &hud->fragsOn, ST_FRAGSWIDTH,
                  &hud->statusbarCounterAlpha);

    // Armor num - should be colored later.
    STlib_initNum(&hud->wArmor, ST_ARMORX, ST_ARMORY, dpINumbers,
                  &hud->armorLevel, &hud->statusbarOn, ST_ARMORWIDTH,
                  &hud->statusbarCounterAlpha);

    // ManaA count.
    STlib_initNum(&hud->wManaACount, ST_MANAAX, ST_MANAAY,
                  dpSmallNumbers, &hud->manaACount, &hud->statusbarOn,
                  ST_MANAAWIDTH, &hud->statusbarCounterAlpha);

    // ManaB count.
    STlib_initNum(&hud->wManaBCount, ST_MANABX, ST_MANABY,
                  dpSmallNumbers, &hud->manaBCount, &hud->statusbarOn,
                  ST_MANABWIDTH, &hud->statusbarCounterAlpha);

    // Current mana A icon.
    STlib_initMultIcon(&hud->wManaA, ST_MANAAICONX, ST_MANAAICONY, dpManaAIcons,
                       &hud->manaAIcon, &hud->statusbarOn, &hud->statusbarCounterAlpha);

    // Current mana B icon.
    STlib_initMultIcon(&hud->wManaB, ST_MANABICONX, ST_MANABICONY, dpManaBIcons,
                       &hud->manaBIcon, &hud->statusbarOn, &hud->statusbarCounterAlpha);

    // Current mana A vial.
    STlib_initMultIcon(&hud->wManaAVial, ST_MANAAVIALX, ST_MANAAVIALY, dpManaAVials,
                       &hud->manaAVial, &hud->statusbarOn, &hud->statusbarCounterAlpha);

    // Current mana B vial.
    STlib_initMultIcon(&hud->wManaBVial, ST_MANABVIALX, ST_MANABVIALY, dpManaBVials,
                       &hud->manaBVial, &hud->statusbarOn, &hud->statusbarCounterAlpha);

    // Current artifact (stbar not inventory).
    STlib_initMultIcon(&hud->wArti, ST_ARTIFACTX, ST_ARTIFACTY, dpArtifacts,
                       &hud->artiCI, &hud->statusbarOn, &hud->statusbarCounterAlpha);

    // Current artifact count.
    STlib_initNum(&hud->wArtiCount, ST_ARTIFACTCX, ST_ARTIFACTCY, dpSmallNumbers,
                  &hud->oldArtiCount, &hud->statusbarOn, ST_ARTIFACTCWIDTH,
                  &hud->statusbarCounterAlpha);

    // Inventory slots
    width = dpArtifacts[5].width + 1;
    temp = 0;

    for(i = 0; i < NUMVISINVSLOTS; ++i)
    {
        // Inventory slot icon.
        STlib_initMultIcon(&hud->wInvSlot[i], ST_INVENTORYX + temp , ST_INVENTORYY,
                           dpArtifacts, &hud->invSlot[i],
                           &hud->statusbarOn, &hud->statusbarCounterAlpha);

        // Inventory slot counter.
        STlib_initNum(&hud->wInvSlotCount[i], ST_INVENTORYX + temp + ST_INVCOUNTOFFX,
                      ST_INVENTORYY + ST_INVCOUNTOFFY, dpSmallNumbers,
                      &hud->invSlotCount[i], &hud->statusbarOn, ST_ARTIFACTCWIDTH,
                      &hud->statusbarCounterAlpha);

        temp += width;
    }
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

void ST_Inventory(int player, boolean show)
{
    player_t*           plr;
    hudstate_t*         hud;

    if(player < 0 || player >= MAXPLAYERS)
        return;

    plr = &players[player];
    if(!((plr->plr->flags & DDPF_LOCAL) && plr->plr->inGame))
        return;

    hud = &hudStates[player];

    if(show)
    {
        hud->inventory = true;

        hud->inventoryTics = (int) (PLRPROFILE.inventory.timer * TICSPERSEC);
        if(hud->inventoryTics < 1)
            hud->inventoryTics = 1;

        ST_HUDUnHide(player, HUE_FORCE);
    }
    else
    {
        hud->inventory = false;
    }
}

boolean ST_IsInventoryVisible(int player)
{
    player_t*           plr;
    hudstate_t*         hud;

    if(player < 0 || player >= MAXPLAYERS)
        return false;

    plr = &players[player];
    if(!((plr->plr->flags & DDPF_LOCAL) && plr->plr->inGame))
        return false;

    hud = &hudStates[player];

    return hud->inventory;
}

void ST_InventoryFlashCurrent(int player)
{
    player_t*           plr;
    hudstate_t*         hud;

    if(player < 0 || player >= MAXPLAYERS)
        return;

    plr = &players[player];
    if(!((plr->plr->flags & DDPF_LOCAL) && plr->plr->inGame))
        return;

    hud = &hudStates[player];

    hud->artifactFlash = 4;
}

void ST_updateWidgets(int player)
{
    int                 i, x, pClass;
    hudstate_t*         hud = &hudStates[player];
    player_t*           plr = &players[player];

    // Original player class (i.e. not pig).
    pClass = gs.players[player].pClass;

    if(hud->blended)
    {
        hud->statusbarCounterAlpha =
            MINMAX_OF(0.f, PLRPROFILE.statusbar.counterAlpha - hud->hideAmount, 1.f);
    }
    else
        hud->statusbarCounterAlpha = 1.0f;

    // Used by w_frags widget.
    hud->fragsOn = GAMERULES.deathmatch && hud->statusbarOn;

    hud->fragsCount = 0;

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        if(!players[i].plr->inGame)
            continue;

        hud->fragsCount += plr->frags[i] * (i != player ? 1 : -1);
    }

    // Current artifact.
    if(hud->artifactFlash)
    {
        hud->artiCI = 5 - hud->artifactFlash;
        hud->artifactFlash--;
        hud->oldArti = -1; // So that the correct artifact fills in after the flash
    }
    else if(hud->oldArti != plr->readyArtifact ||
            hud->oldArtiCount != plr->inventory[plr->invPtr].count)
    {
        if(plr->readyArtifact > 0)
        {
            hud->artiCI = plr->readyArtifact + 5;
        }
        hud->oldArti = plr->readyArtifact;
        hud->oldArtiCount = plr->inventory[plr->invPtr].count;
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

    // update the inventory

    x = plr->invPtr - plr->curPos;

    for(i = 0; i < NUMVISINVSLOTS; ++i)
    {
        hud->invSlot[i] = plr->inventory[x + i].type +5;  // plus 5 for useartifact patches
        hud->invSlotCount[i] = plr->inventory[x + i].count;
    }
}

void ST_Ticker(void)
{
    int                 i;

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        player_t*           plr = &players[i];
        hudstate_t*         hud = &hudStates[i];

        if(!(plr->plr->inGame && (plr->plr->flags & DDPF_LOCAL)))
            continue;

        if(!P_IsPaused())
        {
            int                 delta;
            int                 curHealth;

            if(PLRPROFILE.hud.timer == 0)
            {
                hud->hideTics = hud->hideAmount = 0;
            }
            else
            {
                if(hud->hideTics > 0)
                    hud->hideTics--;
                if(hud->hideTics == 0 && PLRPROFILE.hud.timer > 0 && hud->hideAmount < 1)
                    hud->hideAmount += 0.1f;
            }

            ST_updateWidgets(i);

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

            // Turn inventory off after a certain amount of time.
            if(hud->inventory && !(--hud->inventoryTics))
            {
                plr->readyArtifact = plr->inventory[plr->invPtr].type;
                hud->inventory = false;
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
    int                 i, x;
    int                 player = hud - hudStates;
    player_t*           plr = &players[player];
    boolean             refresh = true;

    hud->oldHealth = -1;
    if(!hud->inventory)
    {
        if(!AM_IsActive(AM_MapForPlayer(player)))
        {
            // Frags
            if(GAMERULES.deathmatch)
                STlib_updateNum(&hud->wFrags, refresh);
            else
                STlib_updateNum(&hud->wHealth, refresh);

            STlib_updateNum(&hud->wArmor, refresh);

            if(plr->readyArtifact != AFT_NONE)
            {
                STlib_updateMultIcon(&hud->wArti, refresh);
                if(!hud->artifactFlash && plr->inventory[plr->invPtr].count > 1)
                    STlib_updateNum(&hud->wArtiCount, refresh);
            }

            if(hud->manaACount > 0)
                STlib_updateNum(&hud->wManaACount, refresh);

            if(hud->manaBCount > 0)
                STlib_updateNum(&hud->wManaBCount, refresh);

            STlib_updateMultIcon(&hud->wManaA, refresh);
            STlib_updateMultIcon(&hud->wManaB, refresh);
            STlib_updateMultIcon(&hud->wManaAVial, refresh);
            STlib_updateMultIcon(&hud->wManaBVial, refresh);

            // Draw the mana bars
            DGL_SetNoMaterial();
            DGL_DrawRect(95, 165, 3,
                         22 - (22 * plr->ammo[AT_BLUEMANA].owned) / MAX_MANA,
                         0, 0, 0, hud->statusbarCounterAlpha);
            DGL_DrawRect(103, 165, 3,
                         22 - (22 * plr->ammo[AT_GREENMANA].owned) / MAX_MANA,
                         0, 0, 0, hud->statusbarCounterAlpha);
        }
        else
        {
            drawKeyBar(hud);
        }
    }
    else
    {   // Draw Inventory

        x = plr->invPtr - plr->curPos;

        for(i = 0; i < NUMVISINVSLOTS; ++i)
        {
            if(plr->inventory[x + i].type != AFT_NONE)
            {
                STlib_updateMultIcon(&hud->wInvSlot[i], refresh);

                if(plr->inventory[x + i].count > 1)
                    STlib_updateNum(&hud->wInvSlotCount[i], refresh);
            }
        }

        // Draw selector box
        GL_DrawPatch(ST_INVENTORYX + plr->curPos * 31, 163, dpSelectBox.lump);

        // Draw more left indicator
        if(x != 0)
            GL_DrawPatchLitAlpha(42, 163, 1, hud->statusbarCounterAlpha,
                                 !(mapTime & 4) ? dpIndicatorLeft1.lump : dpIndicatorLeft2.lump);

        // Draw more right indicator
        if(plr->inventorySlotNum - x > 7)
            GL_DrawPatchLitAlpha(269, 163, 1, hud->statusbarCounterAlpha,
                                 !(mapTime & 4) ? dpIndicatorRight1.lump : dpIndicatorRight2.lump);
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

void GL_DrawShadowedPatch2(float x, float y, float r, float g, float b,
                           float a, lumpnum_t lump)
{
    GL_DrawPatchLitAlpha(x + 2, y + 2, 0, a * .4f, lump);
    DGL_Color4f(r, g, b, a);
    GL_DrawPatch_CS(x, y, lump);
    DGL_Color4f(1, 1, 1, 1);
}

/**
 * Draws a three digit number using FontB
 */
static void DrBNumber(int val, int x, int y, float red, float green,
                      float blue, float alpha)
{
    dpatch_t*           patch;
    int                 xpos;
    int                 oldval;

    // Limit to three digits.
    if(val > 999)
        val = 999;
    if(val < -999)
        val = -999;

    oldval = val;
    xpos = x;
    if(val < 0)
    {
        val = 0;
    }

    if(val > 99)
    {
        patch = &huFontB['0' - HU_FONTSTART + val / 100];
        GL_DrawShadowedPatch2(xpos + 6 - patch->width / 2, y, red, green,
                              blue, alpha, patch->lump);
    }

    val = val % 100;
    xpos += 12;
    if(val > 9 || oldval > 99)
    {
        patch = &huFontB['0' - HU_FONTSTART + val / 10];
        GL_DrawShadowedPatch2(xpos + 6 - patch->width / 2, y, red, green,
                              blue, alpha, patch->lump);
    }

    val = val % 10;
    xpos += 12;
    patch = &huFontB['0' - HU_FONTSTART + val];
    GL_DrawShadowedPatch2(xpos + 6 - patch->width / 2, y, red, green, blue,
                          alpha, patch->lump);
}

/**
 * Draws a small two digit number.
 */
static void DrSmallNumber(int val, int x, int y, float r, float g, float b, float a)
{
    DGL_Color4f(r,g,b,a);

    if(val <= 0)
    {
        return;
    }
    if(val > 999)
    {
        val %= 1000;
    }
    if(val > 99)
    {
        GL_DrawPatch_CS(x, y, dpSmallNumbers[val / 100].lump);
        GL_DrawPatch_CS(x + 4, y, dpSmallNumbers[(val % 100) / 10].lump);
    }
    else if(val > 9)
    {
        GL_DrawPatch_CS(x + 4, y, dpSmallNumbers[val / 10].lump);
    }
    val %= 10;
    GL_DrawPatch_CS(x + 8, y, dpSmallNumbers[val].lump);
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

    if(ev == HUE_FORCE || PLRPROFILE.hud.unHide[ev])
    {
        hudStates[player].hideTics = (PLRPROFILE.hud.timer * TICSPERSEC);
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

    statusbarVisible = (PLRPROFILE.statusbar.scale < 20 ||
        (PLRPROFILE.statusbar.scale == 20 && hud->showBar < 1.0f));

    hud->firstTime = false;

    if(statusbarVisible)
    {
        float               fscale = PLRPROFILE.statusbar.scale / 20.0f;
        float               h = 200 * (1 - fscale);

        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PushMatrix();
        DGL_Translatef(160 - 320 * fscale / 2, h / hud->showBar, 0);
        DGL_Scalef(fscale, fscale, 1);
    }

    drawStatusBarBackground(player);
    if(!hud->inventory && !AM_IsActive(AM_MapForPlayer(player)))
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
    int                 i, x, temp;
    hudstate_t*         hud = &hudStates[player];
    player_t*           plr = &players[player];
    float               textalpha =
        hud->alpha - hud->hideAmount - (1 - PLRPROFILE.hud.color[3]);
    float               iconalpha =
        hud->alpha - hud->hideAmount - (1 - PLRPROFILE.hud.iconAlpha);

    if(PLRPROFILE.hud.shown[HUD_HEALTH])
    {
        Draw_BeginZoom(PLRPROFILE.hud.scale, 5, 198);
        if(plr->plr->mo->health > 0)
        {
            DrBNumber(plr->plr->mo->health, 5, 180,
                      PLRPROFILE.hud.color[0], PLRPROFILE.hud.color[1], PLRPROFILE.hud.color[2], textalpha);
        }
        else
        {
            DrBNumber(0, 5, 180, PLRPROFILE.hud.color[0], PLRPROFILE.hud.color[1], PLRPROFILE.hud.color[2],
                      textalpha);
        }
        Draw_EndZoom();
    }

    if(PLRPROFILE.hud.shown[HUD_MANA])
    {
        int     dim[2] = { dpManaAIcons[0].lump, dpManaBIcons[0].lump };
        int     bright[2] = { dpManaAIcons[0].lump, dpManaBIcons[0].lump };
        int     patches[2] = { 0, 0 };
        int     ypos = PLRPROFILE.hud.shown[HUD_MANA] == 2 ? 152 : 2;

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
            for(i = 0; i < 2; i++)
                if(!patches[i])
                    patches[i] = bright[i];
        }
        Draw_BeginZoom(PLRPROFILE.hud.scale, 2, ypos);
        for(i = 0; i < 2; i++)
        {
            GL_DrawPatchLitAlpha(2, ypos + i * 13, 1, iconalpha, patches[i]);
            DrINumber(plr->ammo[i].owned, 18, ypos + i * 13,
                      1, 1, 1, textalpha);
        }
        Draw_EndZoom();
    }

    if(GAMERULES.deathmatch)
    {
        temp = 0;
        for(i = 0; i < MAXPLAYERS; i++)
        {
            if(players[i].plr->inGame)
            {
                temp += plr->frags[i];
            }
        }
        Draw_BeginZoom(PLRPROFILE.hud.scale, 2, 198);
        DrINumber(temp, 45, 185, 1, 1, 1, textalpha);
        Draw_EndZoom();
    }

    if(!hud->inventory)
    {
        if(PLRPROFILE.hud.shown[HUD_ARTI])
        {
            if(plr->readyArtifact > 0)
            {
                Draw_BeginZoom(PLRPROFILE.hud.scale, 318, 198);

                GL_DrawPatchLitAlpha(286, 170, 1, iconalpha/2,
                                     dpArtifacts[5 + AFT_NONE].lump);
                GL_DrawPatchLitAlpha(284, 169, 1, iconalpha,
                                     dpArtifacts[5 + plr->readyArtifact].lump);
                if(plr->inventory[plr->invPtr].count > 1)
                {
                    DrSmallNumber(plr->inventory[plr->invPtr].count,
                                  302, 192, 1, 1, 1, textalpha);
                }

                Draw_EndZoom();
            }
        }
    }
    else
    {
        float               invScale;

        invScale = MINMAX_OF(0.25f, PLRPROFILE.hud.scale - 0.25f, 0.8f);

        Draw_BeginZoom(invScale, 160, 198);
        x = plr->invPtr - plr->curPos;
        for(i = 0; i < 7; i++)
        {
            GL_DrawPatchLitAlpha(50 + i * 31, 168, 1, iconalpha/2, dpArtifacts[5 + AFT_NONE].lump);
            if(plr->inventorySlotNum > x + i &&
               plr->inventory[x + i].type != AFT_NONE)
            {
                GL_DrawPatchLitAlpha(49 + i * 31, 167, 1, i==plr->curPos? hud->alpha : iconalpha,
                                     dpArtifacts[5 + plr->inventory[x + i].type].lump);

                if(plr->inventory[x + i].count > 1)
                {
                    DrSmallNumber(plr->inventory[x + i].count, 66 + i * 31,
                                  188,1, 1, 1, i==plr->curPos? hud->alpha : textalpha/2);
                }
            }
        }
        GL_DrawPatchLitAlpha(50 + plr->curPos * 31, 167, 1, hud->alpha,dpSelectBox.lump);
        if(x != 0)
        {
            GL_DrawPatchLitAlpha(40, 167, 1, iconalpha,
                         !(mapTime & 4) ? dpIndicatorLeft1.lump :
                         dpIndicatorLeft2.lump);
        }
        if(plr->inventorySlotNum - x > 7)
        {
            GL_DrawPatchLitAlpha(268, 167, 1, iconalpha,
                         !(mapTime & 4) ? dpIndicatorRight1.lump :
                         dpIndicatorRight2.lump);
        }
        Draw_EndZoom();
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
    hud->statusbarOn = (fullscreenmode < 2) ||
        (AM_IsActive(AM_MapForPlayer(player)) &&
         (PLRPROFILE.automap.hudDisplay == 0 || PLRPROFILE.automap.hudDisplay == 2));

    // Do palette shifts
    ST_doPaletteStuff(player, false);

    // Either slide the status bar in or fade out the fullscreen hud
    if(hud->statusbarOn)
    {
        if(hud->alpha > 0.0f)
        {
            hud->statusbarOn = 0;
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
                hud->statusbarOn = 1;
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

    if(hud->statusbarOn)
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
    int                 min = 1, max = 20, *val = &PLRPROFILE.statusbar.scale;

    if(!stricmp(argv[1], "+"))
        (*val)++;
    else if(!stricmp(argv[1], "-"))
        (*val)--;
    else
        *val = strtol(argv[1], NULL, 0);

    if(*val < min)
        *val = min;
    if(*val > max)
        *val = max;

    // Update the view size if necessary.
    R_SetViewSize(CONSOLEPLAYER, PLRPROFILE.screen.blocks);
    ST_HUDUnHide(CONSOLEPLAYER, HUE_FORCE); // So the user can see the change.

    return true;
}
