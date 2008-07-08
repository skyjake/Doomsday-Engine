/**\file
 *\section License
 * License: Raven
 * Online License Link: http://www.dengine.net/raven_license/End_User_License_Hexen_Source_Code.html
 *
 *\author Copyright © 2003-2008 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2008 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999 Activision
 *
 * This program is covered by the HERETIC / HEXEN (LIMITED USE) source
 * code license; you can redistribute it and/or modify it under the terms
 * of the HERETIC / HEXEN source code license as published by Activision.
 *
 * THIS MATERIAL IS NOT MADE OR SUPPORTED BY ACTIVISION.
 *
 * WARRANTY INFORMATION.
 * This program is provided as is. Activision and it's affiliates make no
 * warranties of any kind, whether oral or written , express or implied,
 * including any warranty of merchantability, fitness for a particular
 * purpose or non-infringement, and no other representations or claims of
 * any kind shall be binding on or obligate Activision or it's affiliates.
 *
 * LICENSE CONDITIONS.
 * You shall not:
 *
 * 1) Exploit this Program or any of its parts commercially.
 * 2) Use this Program, or permit use of this Program, on more than one
 *    computer, computer terminal, or workstation at the same time.
 * 3) Make copies of this Program or any part thereof, or make copies of
 *    the materials accompanying this Program.
 * 4) Use the program, or permit use of this Program, in a network,
 *    multi-user arrangement or remote access arrangement, including any
 *    online use, except as otherwise explicitly provided by this Program.
 * 5) Sell, rent, lease or license any copies of this Program, without
 *    the express prior written consent of Activision.
 * 6) Remove, disable or circumvent any proprietary notices or labels
 *    contained on or within the Program.
 *
 * You should have received a copy of the HERETIC / HEXEN source code
 * license along with this program (Ravenlic.txt); if not:
 * http://www.ravensoft.com/
 */

/**
 * hrefresh.h: - jHexen specific.
 */

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jhexen.h"

#include "f_infine.h"
#include "r_common.h"
#include "p_mapsetup.h"
#include "g_controls.h"
#include "am_map.h"
#include "g_common.h"
#include "hu_menu.h"
#include "x_hair.h"

// MACROS ------------------------------------------------------------------

#define FMAKERGBA(r,g,b,a) ( (byte)(0xff*r) + ((byte)(0xff*g)<<8) + ((byte)(0xff*b)<<16) + ((byte)(0xff*a)<<24) )

#define WINDOWHEIGHT            (Get(DD_VIEWWINDOW_HEIGHT))

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

extern void MN_DrCenterTextA_CS(char *text, int center_x, int y);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

boolean setsizeneeded;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

void R_InitRefresh(void)
{
    // Nothing to do.
}

/**
 * Don't really change anything here, because i might be in the middle of
 * a refresh.  The change will take effect next refresh.
 */
void R_SetViewSize(int blocks, int detail)
{
    setsizeneeded = true;
    if(cfg.setBlocks != blocks && blocks > 10 && blocks < 13)
    {   // When going fullscreen, force a hud show event (to reset the timer).
        ST_HUDUnHide(HUE_FORCE);
    }
    cfg.setBlocks = blocks;
}

void R_DrawMapTitle(void)
{
    float               alpha = 1;
    int                 y = 12;
    char               *lname, *lauthor;

    if(!cfg.levelTitle || actualLevelTime > 6 * 35)
        return;

    // Make the text a bit smaller.
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(160, y, 0);
    DGL_Scalef(.75f, .75f, 1);   // Scale to 3/4
    DGL_Translatef(-160, -y, 0);

    if(actualLevelTime < 35)
        alpha = actualLevelTime / 35.0f;
    if(actualLevelTime > 5 * 35)
        alpha = 1 - (actualLevelTime - 5 * 35) / 35.0f;

    lname = P_GetMapNiceName();
    lauthor = (char *) DD_GetVariable(DD_MAP_AUTHOR);

    // Use stardard map name if DED didn't define it.
    if(!lname)
        lname = P_GetMapName(gameMap);

    Draw_BeginZoom((1 + cfg.hudScale)/2, 160, y);

    if(lname)
    {
        M_WriteText3(160 - M_StringWidth(lname, huFontB) / 2, y, lname,
                    huFontB, defFontRGB[0], defFontRGB[1], defFontRGB[2],
                    alpha, false, 0);
        y += 20;
    }

    if(lauthor)
    {
        M_WriteText3(160 - M_StringWidth(lauthor, huFontA) / 2, y, lauthor,
                    huFontA, .5f, .5f, .5f, alpha, false, 0);
    }

    Draw_EndZoom();

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

void G_Display(void)
{
    static boolean      viewactivestate = false;
    static boolean      menuactivestate = false;
    static int          fullscreenmode = 0;
    static gamestate_t  oldgamestate = -1;

    int                 py;
    player_t           *vplayer = &players[DISPLAYPLAYER];
    boolean             iscam =
        (vplayer->plr->flags & DDPF_CAMERA) != 0; // $democam
    float               x, y, w, h;
    boolean             mapHidesView;

    // $democam: can be set on every frame.
    if(cfg.setBlocks > 10 || iscam)
    {
        // Full screen.
        R_SetViewWindowTarget(0, 0, 320, 200);
    }
    else
    {
        int                 w, h;

        w = cfg.setBlocks * 32;
        h = cfg.setBlocks * (200 - SBARHEIGHT * cfg.statusbarScale / 20) / 10;
        R_SetViewWindowTarget(160 - (w >> 1),
                              (200 - SBARHEIGHT * cfg.statusbarScale / 20 - h) >> 1,
                              w, h);
    }

    R_GetViewWindow(&x, &y, &w, &h);
    R_ViewWindow((int) x, (int) y, (int) w, (int) h);

    switch(G_GetGameState())
    {
    case GS_LEVEL:
        // Clients should be a little careful about the first frames.
        if(IS_CLIENT && (!Get(DD_GAME_READY) || !Get(DD_GOTFRAME)))
            break;

        // Good luck trying to render the view without a viewpoint...
        if(!vplayer->plr->mo)
            break;

        if(!IS_CLIENT && levelTime < 2)
        {
            // Don't render too early; the first couple of frames
            // might be a bit unstable -- this should be considered
            // a bug, but since there's an easy fix...
            break;
        }

        mapHidesView =
            R_MapObscures(DISPLAYPLAYER, (int) x, (int) y, (int) w, (int) h);

        if(!(MN_CurrentMenuHasBackground() && Hu_MenuAlpha() >= 1) &&
           !mapHidesView)
        {
            boolean             special200 = false;
            float               viewOffset[2] = {0, 0};
            int                 viewAngleOffset =
                ANGLE_MAX * -G_GetLookOffset(DISPLAYPLAYER);

            // Set flags for the renderer.
            if(IS_CLIENT)
            {
                // Server updates mobj flags in NetSv_Ticker.
                R_SetAllDoomsdayFlags();
            }
            GL_SetFilter(vplayer->plr->filter); // $democam

            // Check for the sector special 200: use sky2.
            // I wonder where this is used?
            if(P_ToXSectorOfSubsector(vplayer->plr->mo->subsector)->special == 200)
            {
                special200 = true;
                Rend_SkyParams(0, DD_DISABLE, NULL);
                Rend_SkyParams(1, DD_ENABLE, NULL);
            }

            // How about a bit of quake?
            if(localQuakeHappening[DISPLAYPLAYER] && !paused)
            {
                int     intensity = localQuakeHappening[DISPLAYPLAYER];

                viewOffset[0] = (float) ((M_Random() % (intensity << 2)) -
                                    (intensity << 1));
                viewOffset[1] = (float) ((M_Random() % (intensity << 2)) -
                                    (intensity << 1));
            }
            DD_SetVariable(DD_VIEWX_OFFSET, &viewOffset[0]);
            DD_SetVariable(DD_VIEWY_OFFSET, &viewOffset[1]);

            // The view angle offset.
            DD_SetVariable(DD_VIEWANGLE_OFFSET, &viewAngleOffset);
            R_RenderPlayerView(DISPLAYPLAYER);

            if(special200)
            {
                Rend_SkyParams(0, DD_ENABLE, NULL);
                Rend_SkyParams(1, DD_DISABLE, NULL);
            }

            if(!iscam)
                X_Drawer(); // Draw the crosshair.
        }

        // Draw the automap.
        AM_Drawer(DISPLAYPLAYER);
        break;

    default:
        break;
    }

    menuactivestate = Hu_MenuIsActive();
    viewactivestate = viewActive;
    oldgamestate = G_GetGameState();

    if(paused && !fiActive)
    {
        //if(AM_IsMapActive(DISPLAYPLAYER))
            py = 4;
        //else
        //    py = 4; // in jDOOM this is viewwindowy + 4

        GL_DrawPatch(160, py, W_GetNumForName("PAUSED"));
    }
}

void G_Display2(void)
{
    switch(G_GetGameState())
    {
    case GS_LEVEL:
        if(IS_CLIENT && (!Get(DD_GAME_READY) || !Get(DD_GOTFRAME)))
            break;

        if(!IS_CLIENT && levelTime < 2)
        {
            // Don't render too early; the first couple of frames
            // might be a bit unstable -- this should be considered
            // a bug, but since there's an easy fix...
            break;
        }

        // These various HUD's will be drawn unless Doomsday advises not to.
        if(DD_GetInteger(DD_GAME_DRAW_HUD_HINT))
        {
            // Draw HUD displays only visible when the automap is open.
            if(AM_IsMapActive(DISPLAYPLAYER))
                HU_DrawMapCounters();

            // Level information is shown for a few seconds in the
            // beginning of a level.
            R_DrawMapTitle();

            // Do we need to render a full status bar at this point?
            if(!(AM_IsMapActive(DISPLAYPLAYER) && cfg.automapHudDisplay == 0 ))
            {
                player_t   *vplayer = &players[DISPLAYPLAYER];
                boolean     iscam = (vplayer->plr->flags & DDPF_CAMERA) != 0; // $democam

                if(!iscam)
                {
                    if(true == (WINDOWHEIGHT == 200))
                    {
                        // Fullscreen. Which mode?
                        ST_Drawer(cfg.setBlocks - 10, true); // $democam
                    }
                    else
                    {
                        ST_Drawer(0 , true); // $democam
                    }
                }
            }

            HU_Drawer();
        }
        break;

    case GS_INTERMISSION:
        IN_Drawer();
        break;

    case GS_WAITING:
        GL_DrawRawScreen(W_GetNumForName("TITLE"), 0, 0);
        DGL_Color3f(1, 1, 1);
        MN_DrCenterTextA_CS("WAITING... PRESS ESC FOR MENU", 160, 188);
        break;

    default:
        break;
    }

    // InFine is drawn whenever active.
    FI_Drawer();

    // The menu is drawn whenever active.
    Hu_MenuDrawer();
}

int R_GetFilterColor(int filter)
{
    // We have to choose the right color and alpha.
    if(filter >= STARTREDPALS && filter < STARTREDPALS + NUMREDPALS)
        // Red?
        return FMAKERGBA(1, 0, 0, filter / 8.0); // Full red with filter 8.
    else if(filter >= STARTBONUSPALS && filter < STARTBONUSPALS + NUMBONUSPALS)
        // Light Yellow?
        return FMAKERGBA(1, 1, .5, (filter - STARTBONUSPALS + 1) / 16.0);
    else if(filter >= STARTPOISONPALS &&
            filter < STARTPOISONPALS + NUMPOISONPALS)
        // Green?
        return FMAKERGBA(0, 1, 0, (filter - STARTPOISONPALS + 1) / 16.0);
    else if(filter >= STARTSCOURGEPAL)
        // Orange?
        return FMAKERGBA(1, .5, 0, (STARTSCOURGEPAL + 3 - filter) / 6.0);
    else if(filter >= STARTHOLYPAL)
        // White?
        return FMAKERGBA(1, 1, 1, (STARTHOLYPAL + 3 - filter) / 6.0);
    else if(filter == STARTICEPAL)
        // Light blue?
        return FMAKERGBA(.5f, .5f, 1, .4f);
    else if(filter)
        Con_Error("R_GetFilterColor: Strange filter number: %d.\n", filter);
    return 0;
}

void R_SetFilter(int filter)
{
    GL_SetFilter(R_GetFilterColor(filter));
}

void H2_EndFrame(void)
{
    SN_UpdateActiveSequences();
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
    uint                    i;
    int                     Class;
    mobj_t                 *mo;

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
            if(P_IsCamera(mo))
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

            mo->ddFlags |= mo->flags & MF_TRANSLATION;

            // Which translation table to use?
            if(mo->flags & MF_TRANSLATION)
            {
                if(mo->player)
                {
                    Class = mo->player->class;
                }
                else
                {
                    Class = mo->special1;
                }
                if(Class > 2)
                {
                    Class = 0;
                }
                // The last two bits.
                mo->ddFlags |= Class << DDMF_CLASSTRSHIFT;
            }

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
