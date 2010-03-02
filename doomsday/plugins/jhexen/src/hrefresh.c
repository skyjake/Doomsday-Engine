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
 * hrefresh.h: - jHexen specific.
 */

// HEADER FILES ------------------------------------------------------------

#include <string.h>

#include "jhexen.h"

#include "f_infine.h"
#include "r_common.h"
#include "p_mapsetup.h"
#include "g_controls.h"
#include "g_common.h"
#include "hu_menu.h"
#include "hu_msg.h"
#include "am_map.h"
#include "x_hair.h"
#include "p_tick.h"
#include "rend_automap.h"

// MACROS ------------------------------------------------------------------

#define WINDOWHEIGHT            (Get(DD_VIEWWINDOW_HEIGHT))

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

extern void MN_DrCenterTextA_CS(char *text, int center_x, int y);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

boolean setsizeneeded;
float quitDarkenOpacity = 0;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * Don't really change anything here, because i might be in the middle of
 * a refresh.  The change will take effect next refresh.
 */
void R_SetViewSize(int blocks)
{
    setsizeneeded = true;

    if(cfg.setBlocks != blocks && blocks > 10 && blocks < 13)
    {   // When going fullscreen, force a hud show event (to reset the timer).
        int                 i;

        for(i = 0; i < MAXPLAYERS; ++i)
            ST_HUDUnHide(i, HUE_FORCE);
    }

    cfg.setBlocks = blocks;
}

void R_DrawMapTitle(void)
{
    float alpha;
    int y = 12;
    const char* lname, *lauthor;

    if(!cfg.mapTitle || actualMapTime > 6 * 35)
        return;

    // Make the text a bit smaller.
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(160, y, 0);
    DGL_Scalef(.75f, .75f, 1);   // Scale to 3/4
    DGL_Translatef(-160, -y, 0);

    alpha = 1;
    if(actualMapTime < 35)
        alpha = actualMapTime / 35.0f;
    if(actualMapTime > 5 * 35)
        alpha = 1 - (actualMapTime - 5 * 35) / 35.0f;

    lname = P_GetMapNiceName();
    lauthor = P_GetMapAuthor(cfg.hideIWADAuthor);

    // Use stardard map name if DED didn't define it.
    if(!lname)
        lname = P_GetMapName(gameMap);

    Draw_BeginZoom((1 + cfg.hudScale)/2, 160, y);

    if(lname)
    {
        M_WriteText3(160 - M_StringWidth(lname, GF_FONTB) / 2, y, lname,
                    GF_FONTB, defFontRGB[0], defFontRGB[1], defFontRGB[2],
                    alpha, false, true, 0);
        y += 20;
    }

    if(lauthor)
    {
        M_WriteText3(160 - M_StringWidth(lauthor, GF_FONTA) / 2, y,
                     lauthor, GF_FONTA, .5f, .5f, .5f, alpha, false,
                     true, 0);
    }

    Draw_EndZoom();

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

static void rendPlayerView(int player)
{
    player_t* plr = &players[player];
    boolean special200 = false;
    float viewPos[3], viewPitch, pspriteOffsetY;
    angle_t viewAngle;

    if(IS_CLIENT)
    {
        // Server updates mobj flags in NetSv_Ticker.
        R_SetAllDoomsdayFlags();
    }

    // Check for the sector special 200: use sky2.
    // I wonder where this is used?
    if(P_ToXSectorOfSubsector(plr->plr->mo->subsector)->special == 200)
    {
        special200 = true;
        Rend_SkyParams(0, DD_DISABLE, NULL);
        Rend_SkyParams(1, DD_ENABLE, NULL);
    }

    // How about a bit of quake?
    if(localQuakeHappening[player] && !P_IsPaused())
    {
        int intensity = localQuakeHappening[player];

        plr->viewOffset[VX] =
            (float) ((M_Random() % (intensity << 2)) - (intensity << 1));
        plr->viewOffset[VY] =
            (float) ((M_Random() % (intensity << 2)) - (intensity << 1));
    }
    else
    {
        plr->viewOffset[VX] = plr->viewOffset[VY] = 0;
    }

    viewPos[VX] = plr->plr->mo->pos[VX] + plr->viewOffset[VX];
    viewPos[VY] = plr->plr->mo->pos[VY] + plr->viewOffset[VY];
    viewPos[VZ] = plr->viewZ + plr->viewOffset[VZ];
    viewAngle = plr->plr->mo->angle + (int) (ANGLE_MAX * -G_GetLookOffset(player));
    viewPitch = plr->plr->lookDir;

    DD_SetVariable(DD_VIEW_X, &viewPos[VX]);
    DD_SetVariable(DD_VIEW_Y, &viewPos[VY]);
    DD_SetVariable(DD_VIEW_Z, &viewPos[VZ]);
    DD_SetVariable(DD_VIEW_ANGLE, &viewAngle);
    DD_SetVariable(DD_VIEW_PITCH, &viewPitch);

    pspriteOffsetY = HU_PSpriteYOffset(plr);
    DD_SetVariable(DD_PSPRITE_OFFSET_Y, &pspriteOffsetY);

    // $democam
    GL_SetFilter((plr->plr->flags & DDPF_VIEW_FILTER)? true : false);
    if(plr->plr->flags & DDPF_VIEW_FILTER)
    {
        const float* color = plr->plr->filterColor;
        GL_SetFilterColor(color[CR], color[CG], color[CB], color[CA]);
    }

    // Render the view with possible custom filters.
    R_RenderPlayerView(player);

    if(special200)
    {
        Rend_SkyParams(0, DD_ENABLE, NULL);
        Rend_SkyParams(1, DD_DISABLE, NULL);
    }
}

static void rendHUD(int player)
{
    player_t*           plr;

    if(player < 0 || player >= MAXPLAYERS)
        return;

    if(G_GetGameState() != GS_MAP)
        return;

    if(IS_CLIENT && (!Get(DD_GAME_READY) || !Get(DD_GOTFRAME)))
        return;

    plr = &players[player];

    // These various HUD's will be drawn unless Doomsday advises not to
    if(DD_GetInteger(DD_GAME_DRAW_HUD_HINT))
    {
        automapid_t         map = AM_MapForPlayer(player);

        // Draw HUD displays only visible when the automap is open.
        if(AM_IsActive(map))
            HU_DrawMapCounters();

        // Do we need to render a full status bar at this point?
        if(!(AM_IsActive(map) && cfg.automapHudDisplay == 0) &&
           !(P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK)))
        {
            if(true == (WINDOWHEIGHT == 200))
            {
                // Fullscreen. Which mode?
                ST_Drawer(player, cfg.setBlocks - 10, true);
            }
            else
            {
                ST_Drawer(player, 0, true);
            }
        }

        HU_Drawer(player);
    }
}

/**
 * Draws the in-viewport display.
 *
 * @param layer         @c 0 = bottom layer (before the viewport border).
 *                      @c 1 = top layer (after the viewport border).
 */
void G_Display(int layer)
{
    int player = DISPLAYPLAYER;
    player_t* plr = &players[player];
    float x, y, w, h;

    if(layer != 0)
    {
        rendHUD(player);
        return;
    }

    if(G_GetGameState() == GS_MAP)
    {
        // $democam: can be set on every frame.
        if(cfg.setBlocks > 10 || (P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK)))
        {
            // Full screen.
            R_SetViewWindowTarget(0, 0, SCREENWIDTH, SCREENHEIGHT);
        }
        else
        {
            int w = cfg.setBlocks * 32;
            int h = cfg.setBlocks * (SCREENHEIGHT - SBARHEIGHT * cfg.statusbarScale / 20) / 10;

            R_SetViewWindowTarget(SCREENWIDTH/2 - w/2, (SCREENHEIGHT - SBARHEIGHT * cfg.statusbarScale / 20 - h) / 2, w, h);
        }

        R_GetViewWindow(&x, &y, &w, &h);
    }
    else
    {
        x = 0;
        y = 0;
        w = SCREENWIDTH;
        h = SCREENHEIGHT;
    }

    R_SetViewWindow((int) x, (int) y, (int) w, (int) h);

    switch(G_GetGameState())
    {
    case GS_MAP:
        if(!(MN_CurrentMenuHasBackground() && Hu_MenuAlpha() >= 1) &&
           !R_MapObscures(player, (int) x, (int) y, (int) w, (int) h))
        {
            if(IS_CLIENT && (!Get(DD_GAME_READY) || !Get(DD_GOTFRAME)))
                return;

            rendPlayerView(player);

            // Crosshair.
            if(!(P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK))) // $democam
                X_Drawer(player);
        }

        // Draw the automap.
        AM_Drawer(player);
        break;
    case GS_STARTUP:
        DGL_Disable(DGL_TEXTURING);
        DGL_DrawRect(x, y, w, h, 0, 0, 0, 1);
        DGL_Enable(DGL_TEXTURING);
        break;
    default:
        break;
    }
}

void G_Display2(void)
{
    switch(G_GetGameState())
    {
    case GS_MAP:
        if(IS_CLIENT && (!Get(DD_GAME_READY) || !Get(DD_GOTFRAME)))
            break;

        if(DD_GetInteger(DD_GAME_DRAW_HUD_HINT))
        {
            // Map information is shown for a few seconds in the
            // beginning of a map.
            R_DrawMapTitle();
        }
        break;

    case GS_INTERMISSION:
        IN_Drawer();
        break;

    case GS_WAITING:
        //GL_DrawRawScreen(W_GetNumForName("TITLE"), 0, 0);
        //DGL_Color3f(1, 1, 1);
        //MN_DrCenterTextA_CS("WAITING... PRESS ESC FOR MENU", 160, 188);
        break;

    case GS_INFINE:
        if(!fiCmdExecuted)
        {   // A (de)briefing is in process but the script hasn't started yet.
            // Just clear the screen, then.
            DGL_Disable(DGL_TEXTURING);
            DGL_DrawRect(0, 0, 320, 200, 0, 0, 0, 1);
            DGL_Enable(DGL_TEXTURING);
        }
        break;

    default:
        break;
    }

    // Draw pause pic (but not if InFine active).
    if(paused && !fiActive)
    {
        GL_DrawPatch(SCREENWIDTH/2, 4, W_GetNumForName("PAUSED"));
    }

    // InFine is drawn whenever active.
    FI_Drawer();

    // Draw HUD displays; menu, messages.
    Hu_Drawer();

    if(G_GetGameAction() == GA_QUIT)
    {
        DGL_Disable(DGL_TEXTURING);
        DGL_DrawRect(0, 0, 320, 200, 0, 0, 0, quitDarkenOpacity);
        DGL_Enable(DGL_TEXTURING);
    }
}

boolean R_GetFilterColor(float rgba[4], int filter)
{
    if(!rgba)
        return false;

    // We have to choose the right color and alpha.
    if(filter >= STARTREDPALS && filter < STARTREDPALS + NUMREDPALS)
    {   // Red.
        rgba[CR] = 1;
        rgba[CG] = 0;
        rgba[CB] = 0;
        rgba[CA] = (deathmatch? 1.0f : cfg.filterStrength) * filter / 8.f; // Full red with filter 8.
        return true;
    }
    else if(filter >= STARTBONUSPALS && filter < STARTBONUSPALS + NUMBONUSPALS)
    {   // Light Yellow.
        rgba[CR] = 1;
        rgba[CG] = 1;
        rgba[CB] = .5f;
        rgba[CA] = cfg.filterStrength * (filter - STARTBONUSPALS + 1) / 16.f;
        return true;
    }
    else if(filter >= STARTPOISONPALS && filter < STARTPOISONPALS + NUMPOISONPALS)
    {   // Green.
        rgba[CR] = 0;
        rgba[CG] = 1;
        rgba[CB] = 0;
        rgba[CA] = cfg.filterStrength * (filter - STARTPOISONPALS + 1) / 16.f;
        return true;
    }
    else if(filter >= STARTSCOURGEPAL)
    {   // Orange.
        rgba[CR] = 1;
        rgba[CG] = .5f;
        rgba[CB] = 0;
        rgba[CA] = cfg.filterStrength * (STARTSCOURGEPAL + 3 - filter) / 6.f;
        return true;
    }
    else if(filter >= STARTHOLYPAL)
    {   // White.
        rgba[CR] = 1;
        rgba[CG] = 1;
        rgba[CB] = 1;
        rgba[CA] = cfg.filterStrength * (STARTHOLYPAL + 3 - filter) / 6.f;
        return true;
    }
    else if(filter == STARTICEPAL)
    {   // Light blue.
        rgba[CR] = .5f;
        rgba[CG] = .5f;
        rgba[CB] = 1;
        rgba[CA] = cfg.filterStrength * .4f;
        return true;
    }

    if(filter)
        Con_Error("R_GetFilterColor: Strange filter number: %d.\n", filter);
    return false;
}

/**
 * Updates ddflags of all visible mobjs (in sectorlinks).
 * Not strictly necessary (in single player games at least) but here
 * we tell the engine about light-emitting objects, special effects,
 * object properties (solid, local, low/nograv, etc.), color translation
 * and other interesting little details.
 */
void R_SetAllDoomsdayFlags(void)
{
    uint                i;
    mobj_t*             mo;

    // Only visible things are in the sector thinglists, so this is good.
    for(i = 0; i < numsectors; ++i)
        for(mo = P_GetPtr(DMU_SECTOR, i, DMT_MOBJS); mo; mo = mo->sNext)
        {
            if(IS_CLIENT && mo->ddFlags & DDMF_REMOTE)
                continue;

            // Reset the flags for a new frame.
            mo->ddFlags &= DDMF_CLEAR_MASK;

            if(mo->flags & MF_LOCAL)
                mo->ddFlags |= DDMF_LOCAL;
            if(mo->flags & MF_SOLID)
                mo->ddFlags |= DDMF_SOLID;
            if(mo->flags & MF_MISSILE)
                mo->ddFlags |= DDMF_MISSILE;
            if(mo->flags2 & MF2_FLY)
                mo->ddFlags |= DDMF_FLY | DDMF_NOGRAVITY;
            if(mo->flags2 & MF2_FLOATBOB)
                mo->ddFlags |= DDMF_BOB | DDMF_NOGRAVITY;
            if(mo->flags2 & MF2_LOGRAV)
                mo->ddFlags |= DDMF_LOWGRAVITY;
            if(mo->flags & MF_NOGRAVITY /* || mo->flags2 & MF2_FLY */ )
                mo->ddFlags |= DDMF_NOGRAVITY;

            // $democam: cameramen are invisible.
            if(P_MobjIsCamera(mo))
                mo->ddFlags |= DDMF_DONTDRAW;

            // Choose which ddflags to set.
            if(mo->flags2 & MF2_DONTDRAW)
            {
                mo->ddFlags |= DDMF_DONTDRAW;
                continue; // No point in checking the other flags.
            }

            if((mo->flags & MF_BRIGHTSHADOW) == MF_BRIGHTSHADOW)
                mo->ddFlags |= DDMF_BRIGHTSHADOW;
            else
            {
                if(mo->flags & MF_SHADOW)
                    mo->ddFlags |= DDMF_SHADOW;
                if(mo->flags & MF_ALTSHADOW ||
                   (cfg.translucentIceCorpse && mo->flags & MF_ICECORPSE))
                    mo->ddFlags |= DDMF_ALTSHADOW;
            }

            if((mo->flags & MF_VIEWALIGN && !(mo->flags & MF_MISSILE)) ||
               mo->flags & MF_FLOAT || (mo->flags & MF_MISSILE &&
                                        !(mo->flags & MF_VIEWALIGN)))
                mo->ddFlags |= DDMF_VIEWALIGN;

            R_SetTranslation(mo);

            // An offset for the light emitted by this object.
            /*          Class = MobjLightOffsets[mo->type];
               if(Class < 0) Class = 8-Class;
               // Class must now be in range 0-15.
               mo->ddFlags |= Class << DDMF_LIGHTOFFSETSHIFT; */

            // The Mage's ice shards need to be a bit smaller.
            // This'll make them half the normal size.
            if(mo->type == MT_SHARDFX1)
                mo->ddFlags |= 2 << DDMF_LIGHTSCALESHIFT;
        }
}
