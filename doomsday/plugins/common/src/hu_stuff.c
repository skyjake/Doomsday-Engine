/**\file
 *\section License
 * License: GPL + jHeretic/jHexen Exception
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2008 Daniel Swanson <danij@dengine.net>
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
 *
 * In addition, as a special exception, we, the authors of deng
 * give permission to link the code of our release of deng with
 * the libjhexen and/or the libjheretic libraries (or with modified
 * versions of it that use the same license as the libjhexen or
 * libjheretic libraries), and distribute the linked executables.
 * You must obey the GNU General Public License in all respects for
 * all of the code used other than “libjhexen or libjheretic”. If
 * you modify this file, you may extend this exception to your
 * version of the file, but you are not obligated to do so. If you
 * do not wish to do so, delete this exception statement from your version.
 */

/**
 * hu_stuff.c: Heads-up displays, font handling, text drawing routines
 */

#ifdef MSVC
#  pragma warning(disable:4018)
#endif

// HEADER FILES ------------------------------------------------------------

#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#if __WOLFTC__
#  include "wolftc.h"
#elif __JDOOM__
#  include "jdoom.h"
#elif __JDOOM64__
#  include "jdoom64.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#elif __JSTRIFE__
#  include "jstrife.h"
#endif

#include "hu_stuff.h"
#include "hu_msg.h"

// MACROS ------------------------------------------------------------------

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__ || __WOLFTC__
// Counter Cheat flags.
#define CCH_KILLS               0x1
#define CCH_ITEMS               0x2
#define CCH_SECRET              0x4
#define CCH_KILLS_PRCNT         0x8
#define CCH_ITEMS_PRCNT         0x10
#define CCH_SECRET_PRCNT        0x20
#endif

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

dpatch_t huFont[HU_FONTSIZE];
dpatch_t huFontA[HU_FONTSIZE], huFontB[HU_FONTSIZE];

int typeInTime = 0;

#if __JDOOM__ || __JDOOM64__
 // Name graphics of each level (centered)
dpatch_t *levelNamePatches;
#endif

boolean huShowAllFrags = false;

cvar_t hudCVars[] = {
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__ || __WOLFTC__
    {"map-cheat-counter", 0, CVT_BYTE, &cfg.counterCheat, 0, 63},
    {"map-cheat-counter-scale", 0, CVT_FLOAT, &cfg.counterCheatScale, .1f, 1},
#endif
    {NULL}
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static dpatch_t borderPatches[8];
static boolean hudActive = false;

// Code -------------------------------------------------------------------

/**
 * Called during pre-init to register cvars and ccmds for the hud displays.
 */
void HU_Register(void)
{
    uint                i;

    for(i = 0; hudCVars[i].name; ++i)
        Con_AddVariable(&hudCVars[i]);
}

/**
 * Loads the font patches and inits various strings
 * JHEXEN Note: Don't bother with the yellow font, we'll colour the white version
 */
void Hu_LoadData(void)
{
    int                 i, j;
    char                buffer[9];
#if __JHERETIC__ || __JHEXEN__
    dpatch_t            tmp;
#endif

#if __JDOOM__ || __JDOOM64__
    char                name[9];
#endif

    // Load the border patches
    for(i = 1; i < 9; ++i)
        R_CachePatch(&borderPatches[i-1], borderLumps[i]);

    // Setup strings.
#define INIT_STRINGS(x, x_idx) \
    for(i = 0; i < sizeof(x_idx) / sizeof(int); ++i) \
        x[i] = x_idx[i] == -1? "NEWLEVEL" : GET_TXT(x_idx[i]);

#if __JDOOM__ || __JDOOM64__
    // load the heads-up fonts
    j = HU_FONTSTART;
    DD_SetInteger(DD_UPSCALE_AND_SHARPEN_PATCHES, true);
    for(i = 0; i < HU_FONTSIZE; ++i, ++j)
    {
        // The original small red font.
        sprintf(buffer, "STCFN%.3d", j);
        R_CachePatch(&huFont[i], buffer);

        // Small white font.
        sprintf(buffer, "FONTA%.3d", j);
        R_CachePatch(&huFontA[i], buffer);

        // Large (12) white font.
        sprintf(buffer, "FONTB%.3d", j);
        R_CachePatch(&huFontB[i], buffer);
        if(huFontB[i].lump == -1)
        {
            // This character is missing! (the first character
            // is supposedly always found)
            memcpy(&huFontB[0 + i], &huFontB[4], sizeof(dpatch_t));
        }
    }
    DD_SetInteger(DD_UPSCALE_AND_SHARPEN_PATCHES, false);

    // load the map name patches
# if __JDOOM64__
    {
        int NUMCMAPS = 32;
        levelNamePatches = Z_Malloc(sizeof(dpatch_t) * NUMCMAPS, PU_STATIC, 0);
        for(i = 0; i < NUMCMAPS; ++i)
        {
            sprintf(name, "WILV%2.2d", i);
            R_CachePatch(&levelNamePatches[i], name);
        }
    }
# else
    if(gameMode == commercial)
    {
        int NUMCMAPS = 32;
        levelNamePatches = Z_Malloc(sizeof(dpatch_t) * NUMCMAPS, PU_STATIC, 0);
        for(i = 0; i < NUMCMAPS; ++i)
        {
            sprintf(name, "CWILV%2.2d", i);
            R_CachePatch(&levelNamePatches[i], name);
        }
    }
    else
    {
        // Don't waste space - patches are loaded back to back
        // ie no space in the array is left for E1M10
        levelNamePatches = Z_Malloc(sizeof(dpatch_t) * (9*4), PU_STATIC, 0);
        for(j = 0; j < 4; ++j) // Number of episodes.
        {
            for(i = 0; i < 9; ++i) // Number of maps per episode.
            {
                sprintf(name, "WILV%2.2d", (j * 10) + i);
                R_CachePatch(&levelNamePatches[(j* 9)+i], name);
            }
        }
    }
# endif
#elif __JSTRIFE__
    // load the heads-up fonts

    // Tell Doomsday to load the following patches in monochrome mode
    // (2 = weighted average)
    DD_SetInteger(DD_MONOCHROME_PATCHES, 2);

    j = HU_FONTSTART;
    for(i = 0; i < HU_FONTSIZE; ++i, ++j)
    {
        // The original small red font.
        sprintf(buffer, "STCFN%.3d", j);
        R_CachePatch(&huFont[i], buffer);

        // Small white font.
        sprintf(buffer, "STCFN%.3d", j);
        R_CachePatch(&huFontA[i], buffer);

        // Large (12) white font.
        sprintf(buffer, "STBFN.3d", j);
        R_CachePatch(&huFontB[i], buffer);
        if(huFontB[i].lump == -1)
        {
            // This character is missing! (the first character
            // is supposedly always found)
            memcpy(&huFontB[0 + i], &huFontB[4], sizeof(dpatch_t));
        }
    }

    // deactivate monochrome mode
    DD_SetInteger(DD_MONOCHROME_PATCHES, 0);
#else
    // Tell Doomsday to load the following patches in monochrome mode
    // (2 = weighted average)
    DD_SetInteger(DD_MONOCHROME_PATCHES, 2);
    DD_SetInteger(DD_UPSCALE_AND_SHARPEN_PATCHES, true);

    // Heretic/Hexen don't use ASCII numbered font patches
    // plus they don't even have a full set eg '!' = 1 '_'= 58 !!!
    j = 1;
    for(i = 0; i < HU_FONTSIZE; ++i, ++j)
    {
        // Small font.
        sprintf(buffer, "FONTA%.2d", j);
        R_CachePatch(&huFontA[i], buffer);

        // Large (12) font.
        sprintf(buffer, "FONTB%.2d", j);
        R_CachePatch(&huFontB[i], buffer);
        if(huFontB[i].lump == -1)
        {
            // This character is missing! (the first character
            // is supposedly always found)
            memcpy(&huFontB[0 + i], &huFontB[4], sizeof(dpatch_t));
        }
    }

    // deactivate monochrome mode
    DD_SetInteger(DD_MONOCHROME_PATCHES, 0);
    DD_SetInteger(DD_UPSCALE_AND_SHARPEN_PATCHES, false);

    // Heretic and Hexen don't use ASCII numbering for all font patches.
    // As such we need to switch some patches.

    tmp = huFontA[58];
    memcpy(&huFontA[58], &huFontA[62], sizeof(dpatch_t));
    memcpy(&huFontA[62], &tmp, sizeof(dpatch_t));

    tmp = huFontB[58];
    memcpy(&huFontB[58], &huFontB[62], sizeof(dpatch_t));
    memcpy(&huFontB[62], &tmp, sizeof(dpatch_t));

#endif

    HUMsg_Init();
}

void HU_UnloadData(void)
{
#if __JDOOM__ || __JDOOM64__
    if(levelNamePatches)
        Z_Free(levelNamePatches);
#endif
}

void HU_Stop(void)
{
    hudActive = false;
}

void HU_Start(void)
{
    if(hudActive)
        HU_Stop();

    HUMsg_Start();

    hudActive = true;
}

void HU_Drawer(void)
{
    int         i, k, x, y;
    char        buf[80];
    player_t   *plr;

    HUMsg_Drawer();

    if(huShowAllFrags)
    {
        for(y = 8, i = 0; i < MAXPLAYERS; ++i)
        {
            plr = &players[i];
            if(!plr->plr || !plr->plr->inGame)
                continue;

            sprintf(buf, "%i%s", i, (i == CONSOLEPLAYER ? "=" : ":"));

            M_WriteText(0, y, buf);

            x = 20;
            for(k = 0; k < MAXPLAYERS; ++k, x += 18)
            {
                if(players[k].plr || !players[k].plr->inGame)
                    continue;

                sprintf(buf, "%i", plr->frags[k]);
                M_WriteText(x, y, buf);
            }

            y += 10;
        }
    }
}

#if __JDOOM__ || __JDOOM64__
/**
 * Draws a sorted frags list in the lower right corner of the screen.
 */
static void drawFragsTable(void)
{
#define FRAGS_DRAWN    -99999
    int         i, k, y, inCount = 0;    // How many players in the game?
    int         totalFrags[MAXPLAYERS];
    int         max, choose = 0;
    int         w = 30;
    char       *name, tmp[40];

    memset(totalFrags, 0, sizeof(totalFrags));
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        if(!players[i].plr->inGame)
            continue;

        inCount++;
        for(k = 0; k < MAXPLAYERS; ++k)
            totalFrags[i] += players[i].frags[k] * (k != i ? 1 : -1);
    }

    // Start drawing from the top.
# if __JDOOM64__
    y = HU_TITLEY + 32 * (inCount - 1) * LINEHEIGHT_A;
# else
    y = HU_TITLEY + 32 * (20 - cfg.statusbarScale) / 20 - (inCount - 1) * LINEHEIGHT_A;
#endif
    for(i = 0; i < inCount; ++i, y += LINEHEIGHT_A)
    {
        // Find the largest.
        for(max = FRAGS_DRAWN + 1, k = 0; k < MAXPLAYERS; ++k)
        {
            if(!players[k].plr->inGame || totalFrags[k] == FRAGS_DRAWN)
                continue;

            if(totalFrags[k] > max)
            {
                choose = k;
                max = totalFrags[k];
            }
        }

        // Draw the choice.
        name = Net_GetPlayerName(choose);
        switch(cfg.playerColor[choose])
        {
        case 0:                // green
            DGL_Color3f(0, .8f, 0);
            break;

        case 1:                // gray
            DGL_Color3f(.45f, .45f, .45f);
            break;

        case 2:                // brown
            DGL_Color3f(.7f, .5f, .4f);
            break;

        case 3:                // red
            DGL_Color3f(1, 0, 0);
            break;
        }

        M_WriteText2(320 - w - M_StringWidth(name, huFontA) - 6, y, name,
                     huFontA, -1, -1, -1, -1);
        // A colon.
        M_WriteText2(320 - w - 5, y, ":", huFontA, -1, -1, -1, -1);
        // The frags count.
        sprintf(tmp, "%i", totalFrags[choose]);
        M_WriteText2(320 - w, y, tmp, huFontA, -1, -1, -1, -1);
        // Mark to ignore in the future.
        totalFrags[choose] = FRAGS_DRAWN;
    }
}
#endif

/**
 * Draws the deathmatch stats
 * \todo Merge with drawFragsTable()
 */
#if __JHEXEN__
static void drawDeathmatchStats(void)
{
static const int their_colors[] = {
    AM_PLR1_COLOR,
    AM_PLR2_COLOR,
    AM_PLR3_COLOR,
    AM_PLR4_COLOR,
    AM_PLR5_COLOR,
    AM_PLR6_COLOR,
    AM_PLR7_COLOR,
    AM_PLR8_COLOR
};

    int         i, j, k, m;
    int         fragCount[MAXPLAYERS];
    int         order[MAXPLAYERS];
    char        textBuffer[80];
    int         yPosition;

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        fragCount[i] = 0;
        order[i] = -1;
    }

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        if(!players[i].plr->inGame)
            continue;

        for(j = 0; j < MAXPLAYERS; ++j)
        {
            if(players[i].plr->inGame)
            {
                fragCount[i] += players[i].frags[j];
            }
        }

        for(k = 0; k < MAXPLAYERS; ++k)
        {
            if(order[k] == -1)
            {
                order[k] = i;
                break;
            }
            else if(fragCount[i] > fragCount[order[k]])
            {
                for(m = MAXPLAYERS - 1; m > k; m--)
                {
                    order[m] = order[m - 1];
                }
                order[k] = i;
                break;
            }
        }
    }

    yPosition = 15;
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        if(order[i] < 0 || !players[order[i]].plr ||
           !players[order[i]].plr->inGame)
        {
            continue;
        }
        else
        {
            float               rgb[3];

            GL_PalIdxToRGB(their_colors[cfg.playerColor[order[i]]], rgb);
            DGL_Color3fv(rgb);

            memset(textBuffer, 0, 80);
            strncpy(textBuffer, Net_GetPlayerName(order[i]), 78);
            strcat(textBuffer, ":");
            MN_TextFilter(textBuffer);

            M_WriteText2(4, yPosition, textBuffer, huFontA, -1, -1, -1, -1);
            j = M_StringWidth(textBuffer, huFontA);

            sprintf(textBuffer, "%d", fragCount[order[i]]);
            M_WriteText2(j + 8, yPosition, textBuffer, huFontA, -1, -1, -1, -1);
            yPosition += 10;
        }
    }
}
#endif

/**
 * Draws the world time in the top right corner of the screen
 */
static void drawWorldTimer(void)
{
#if __JHEXEN__
    int     days, hours, minutes, seconds;
    int     worldTimer;
    char    timeBuffer[15];
    char    dayBuffer[20];

    worldTimer = players[DISPLAYPLAYER].worldTimer;

    worldTimer /= 35;
    days = worldTimer / 86400;
    worldTimer -= days * 86400;
    hours = worldTimer / 3600;
    worldTimer -= hours * 3600;
    minutes = worldTimer / 60;
    worldTimer -= minutes * 60;
    seconds = worldTimer;

    sprintf(timeBuffer, "%.2d : %.2d : %.2d", hours, minutes, seconds);
    M_WriteText2(240, 8, timeBuffer, huFontA, 1, 1, 1, 1);

    if(days)
    {
        if(days == 1)
        {
            sprintf(dayBuffer, "%.2d DAY", days);
        }
        else
        {
            sprintf(dayBuffer, "%.2d DAYS", days);
        }

        M_WriteText2(240, 20, dayBuffer, huFontA, 1, 1, 1, 1);
        if(days >= 5)
        {
            M_WriteText2(230, 35, "YOU FREAK!!!", huFontA, 1, 1, 1, 1);
        }
    }
#endif
}

/**
 * Handles what counters to draw eg title, timer, dm stats etc
 */
void HU_DrawMapCounters(void)
{
    player_t           *plr;
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    char                buf[40], tmp[20];
    int                 x = 5, y = LINEHEIGHT_A * 3;
#endif

    plr = &players[DISPLAYPLAYER];

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    DGL_Color3f(1, 1, 1);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
#endif

    DGL_Enable(DGL_TEXTURING);

    drawWorldTimer();

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__ || __WOLFTC__
    Draw_BeginZoom(cfg.counterCheatScale, x, y);

    if(cfg.counterCheat)
    {
        // Kills.
        if(cfg.counterCheat & (CCH_KILLS | CCH_KILLS_PRCNT))
        {
            strcpy(buf, "Kills: ");
            if(cfg.counterCheat & CCH_KILLS)
            {
                sprintf(tmp, "%i/%i ", plr->killCount, totalKills);
                strcat(buf, tmp);
            }
            if(cfg.counterCheat & CCH_KILLS_PRCNT)
            {
                sprintf(tmp, "%s%i%%%s", (cfg.counterCheat & CCH_KILLS ? "(" : ""),
                        totalKills ? plr->killCount * 100 / totalKills : 100,
                        (cfg.counterCheat & CCH_KILLS ? ")" : ""));
                strcat(buf, tmp);
            }

            M_WriteText2(x, y, buf, huFontA, 1, 1, 1, 1);

            y += LINEHEIGHT_A;
        }

        // Items.
        if(cfg.counterCheat & (CCH_ITEMS | CCH_ITEMS_PRCNT))
        {
            strcpy(buf, "Items: ");
            if(cfg.counterCheat & CCH_ITEMS)
            {
                sprintf(tmp, "%i/%i ", plr->itemCount, totalItems);
                strcat(buf, tmp);
            }
            if(cfg.counterCheat & CCH_ITEMS_PRCNT)
            {
                sprintf(tmp, "%s%i%%%s", (cfg.counterCheat & CCH_ITEMS ? "(" : ""),
                        totalItems ? plr->itemCount * 100 / totalItems : 100,
                        (cfg.counterCheat & CCH_ITEMS ? ")" : ""));
                strcat(buf, tmp);
            }

            M_WriteText2(x, y, buf, huFontA, 1, 1, 1, 1);

            y += LINEHEIGHT_A;
        }

        // Secrets.
        if(cfg.counterCheat & (CCH_SECRET | CCH_SECRET_PRCNT))
        {
            strcpy(buf, "Secret: ");
            if(cfg.counterCheat & CCH_SECRET)
            {
                sprintf(tmp, "%i/%i ", plr->secretCount, totalSecret);
                strcat(buf, tmp);
            }
            if(cfg.counterCheat & CCH_SECRET_PRCNT)
            {
                sprintf(tmp, "%s%i%%%s", (cfg.counterCheat & CCH_SECRET ? "(" : ""),
                        totalSecret ? plr->secretCount * 100 / totalSecret : 100,
                        (cfg.counterCheat & CCH_SECRET ? ")" : ""));
                strcat(buf, tmp);
            }

            M_WriteText2(x, y, buf, huFontA, 1, 1, 1, 1);

            y += LINEHEIGHT_A;
        }
    }

    Draw_EndZoom();

#if __JDOOM__ || __JDOOM64__
    if(deathmatch)
        drawFragsTable();
#endif

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#endif

#if __JHEXEN__ || __JSTRIFE__
    if(IS_NETGAME)
    {
        // Always draw deathmatch stats in a netgame, even in coop
        drawDeathmatchStats();
    }
#endif
}

void Hu_Ticker(void)
{
    HUMsg_Ticker();
}

int MN_FilterChar(int ch)
{
    ch = toupper(ch);
    if(ch == '_')
        ch = '[';
    else if(ch == '\\')
        ch = '/';
    else if(ch < 32 || ch > 'Z')
        ch = 32;                // We don't have this char.
    return ch;
}

void MN_TextFilter(char *text)
{
    int         k;

    for(k = 0; text[k]; ++k)
    {
        text[k] = MN_FilterChar(text[k]);
    }
}

/**
 * Expected: <whitespace> = <whitespace> <float>
 */
float WI_ParseFloat(char **str)
{
    float       value;
    char       *end;

    *str = M_SkipWhite(*str);
    if(**str != '=')
        return 0; // Now I'm confused!

    *str = M_SkipWhite(*str + 1);
    value = strtod(*str, &end);
    *str = end;
    return value;
}

/**
 * Draw a string of text controlled by parameter blocks.
 */
void WI_DrawParamText(int x, int y, char *str, dpatch_t *defFont,
                      float defRed, float defGreen, float defBlue, float defAlpha,
                      boolean defCase, boolean defTypeIn, int halign)
{
    char    temp[256], *end, *string;
    dpatch_t  *font = defFont;
    float   r = defRed, g = defGreen, b = defBlue, a = defAlpha;
    float   offX = 0, offY = 0, width = 0;
    float   scaleX = 1, scaleY = 1, angle = 0, extraScale;
    float   cx = x, cy = y;
    int     charCount = 0;
    boolean typeIn = defTypeIn;
    boolean caseScale = defCase;
    struct {
        float   scale, offset;
    } caseMod[2];               // 1=upper, 0=lower
    int     curCase = -1;

    int alignx = 0;

    caseMod[0].scale = 1;
    caseMod[0].offset = 3;
    caseMod[1].scale = 1.25f;
    caseMod[1].offset = 0;

    // With centrally aligned strings we need to calculate the width
    // of the whole visible string before we can draw any characters.
    // So we'll need to make two passes on the string.
    if(halign == ALIGN_CENTER)
    {
        string = str;
        while(*string)
        {
            if(*string == '{')      // Parameters included?
            {
                string++;
                while(*string && *string != '}')
                {
                    string = M_SkipWhite(string);

                    // We are only interested in font changes
                    // at this stage.
                    if(!strnicmp(string, "fonta", 5))
                    {
                        font = huFontA;
                        string += 5;
                    }
                    else if(!strnicmp(string, "fontb", 5))
                    {
                        font = huFontB;
                        string += 5;
                    }
                    else
                    {
                        // Unknown, skip it.
                        if(*string != '}')
                            string++;
                    }
                }

                // Skip over the closing brace.
                if(*string)
                    string++;
            }

            for(end = string; *end && *end != '{';)
            {
                // Find the end of the visible part of the string.
                for(; *end && *end != '{'; end++);

                strncpy(temp, string, end - string);
                temp[end - string] = 0;

                width += M_StringWidth(temp, font);

                string = end;       // Continue from here.
            }
        }
        width /= 2;
    }

    string = str;
    while(*string)
    {
        // Parse and draw the replacement string.
        if(*string == '{')      // Parameters included?
        {
            string++;
            while(*string && *string != '}')
            {
                string = M_SkipWhite(string);

                // What do we have here?
                if(!strnicmp(string, "fonta", 5))
                {
                    font = huFontA;
                    string += 5;
                }
                else if(!strnicmp(string, "fontb", 5))
                {
                    font = huFontB;
                    string += 5;
                }
                else if(!strnicmp(string, "flash", 5))
                {
                    string += 5;
                    typeIn = true;
                }
                else if(!strnicmp(string, "noflash", 7))
                {
                    string += 7;
                    typeIn = false;
                }
                else if(!strnicmp(string, "case", 4))
                {
                    string += 4;
                    caseScale = true;
                }
                else if(!strnicmp(string, "nocase", 6))
                {
                    string += 6;
                    caseScale = false;
                }
                else if(!strnicmp(string, "ups", 3))
                {
                    string += 3;
                    caseMod[1].scale = WI_ParseFloat(&string);
                }
                else if(!strnicmp(string, "upo", 3))
                {
                    string += 3;
                    caseMod[1].offset = WI_ParseFloat(&string);
                }
                else if(!strnicmp(string, "los", 3))
                {
                    string += 3;
                    caseMod[0].scale = WI_ParseFloat(&string);
                }
                else if(!strnicmp(string, "loo", 3))
                {
                    string += 3;
                    caseMod[0].offset = WI_ParseFloat(&string);
                }
                else if(!strnicmp(string, "break", 5))
                {
                    string += 5;
                    cx = x;
                    cy += scaleY * font[0].height;
                }
                else if(!strnicmp(string, "r", 1))
                {
                    string++;
                    r = WI_ParseFloat(&string);
                }
                else if(!strnicmp(string, "g", 1))
                {
                    string++;
                    g = WI_ParseFloat(&string);
                }
                else if(!strnicmp(string, "b", 1))
                {
                    string++;
                    b = WI_ParseFloat(&string);
                }
                else if(!strnicmp(string, "x", 1))
                {
                    string++;
                    offX = WI_ParseFloat(&string);
                }
                else if(!strnicmp(string, "y", 1))
                {
                    string++;
                    offY = WI_ParseFloat(&string);
                }
                else if(!strnicmp(string, "scalex", 6))
                {
                    string += 6;
                    scaleX = WI_ParseFloat(&string);
                }
                else if(!strnicmp(string, "scaley", 6))
                {
                    string += 6;
                    scaleY = WI_ParseFloat(&string);
                }
                else if(!strnicmp(string, "scale", 5))
                {
                    string += 5;
                    scaleX = scaleY = WI_ParseFloat(&string);
                }
                else if(!strnicmp(string, "angle", 5))
                {
                    string += 5;
                    angle = WI_ParseFloat(&string);
                }
                else
                {
                    // Unknown, skip it.
                    if(*string != '}')
                        string++;
                }
            }

            // Skip over the closing brace.
            if(*string)
                string++;
        }

        for(end = string; *end && *end != '{';)
        {
            if(caseScale)
            {
                curCase = -1;
                // Select a substring with characters of the same case
                // (or whitespace).
                for(; *end && *end != '{'; end++)
                {
                    // We can skip whitespace.
                    if(isspace(*end))
                        continue;

                    if(curCase < 0)
                        curCase = (isupper(*end) != 0);
                    else if(curCase != (isupper(*end) != 0))
                        break;
                }
            }
            else
            {
                // Find the end of the visible part of the string.
                for(; *end && *end != '{'; end++);
            }

            strncpy(temp, string, end - string);
            temp[end - string] = 0;
            string = end;       // Continue from here.

            if(halign == ALIGN_CENTER){
                alignx = width;
            } else if (halign == ALIGN_RIGHT){
                alignx = scaleX * M_StringWidth(temp, font);
            } else {
                alignx = 0;
            }

            // Setup the scaling.
            DGL_MatrixMode(DGL_MODELVIEW);
            DGL_PushMatrix();

            // Rotate.
            if(angle != 0)
            {
                // The origin is the specified (x,y) for the patch.
                // We'll undo the VGA aspect ratio (otherwise the result would
                // be skewed).
                DGL_Translatef(x, y, 0);
                DGL_Scalef(1, 200.0f / 240.0f, 1);
                DGL_Rotatef(angle, 0, 0, 1);
                DGL_Scalef(1, 240.0f / 200.0f, 1);
                DGL_Translatef(-x, -y, 0);
            }

            DGL_Translatef(cx + offX - alignx,
                          cy + offY +
                          (caseScale ? caseMod[curCase].offset : 0), 0);
            extraScale = (caseScale ? caseMod[curCase].scale : 1);
            DGL_Scalef(scaleX, scaleY * extraScale, 1);

            // Draw it.
            M_WriteText3(0, 0, temp, font, r, g, b, a, typeIn,
                         typeIn ? charCount : 0);
            charCount += strlen(temp);

            // Advance the current position.
            cx += scaleX * M_StringWidth(temp, font);

            DGL_MatrixMode(DGL_MODELVIEW);
            DGL_PopMatrix();
        }
    }
}

/**
 * Find string width from huFont chars
 * Skips parameter blocks eg "{param}Text" = 4 chars
 */
int M_StringWidth(const char *string, dpatch_t * font)
{
    uint        i;
    int         w = 0;
    int         c;
    boolean     skip;

    for(i = 0, skip = false; i < strlen(string); ++i)
    {
        c = toupper(string[i]) - HU_FONTSTART;

        if(string[i] == '{')
            skip = true;

        if(skip == false)
        {
            if(c < 0 || c >= HU_FONTSIZE)
                w += 4;
            else
                w += font[c].width;
        }

        if(string[i] == '}')
            skip = false;
    }
    return w;
}

/**
 * Find string height from huFont chars
 */
int M_StringHeight(const char *string, dpatch_t *font)
{
    uint        i;
    int         h;
    int         height = font[17].height;

    h = height;
    for(i = 0; i < strlen(string); ++i)
        if(string[i] == '\n')
            h += height;
    return h;
}

void M_LetterFlash(int x, int y, int w, int h, int bright, float red,
                          float green, float blue, float alpha)
{
    float   fsize = 4 + bright;
    float   fw = fsize * w / 2.0f, fh = fsize * h / 2.0f;
    int     origColor[4];

    // Don't draw anything for very small letters.
    if(h <= 4)
        return;

    CLAMP(alpha, 0.0f, 1.0f);

    // Don't bother with hidden letters.
    if(alpha == 0.0f)
        return;

    CLAMP(red, 0.0f, 1.0f);
    CLAMP(green, 0.0f, 1.0f);
    CLAMP(blue, 0.0f, 1.0f);

    // Store original color.
    DGL_GetIntegerv(DGL_CURRENT_COLOR_RGBA, origColor);

    DGL_Bind(Get(DD_DYNLIGHT_TEXTURE));

    if(bright)
        GL_BlendMode(BM_ADD);
    else
        DGL_BlendFunc(DGL_ZERO, DGL_ONE_MINUS_SRC_ALPHA);

    GL_DrawRect(x + w / 2.0f - fw / 2, y + h / 2.0f - fh / 2, fw, fh, red,
                green, blue, alpha);

    GL_BlendMode(BM_NORMAL);

    // Restore original color.
    DGL_Color4ub(origColor[0], origColor[1], origColor[2], origColor[3]);
}

/**
 * Write a string using the huFont.
 */
void M_WriteText(int x, int y, const char *string)
{
    M_WriteText2(x, y, string, huFontA, 1, 1, 1, 1);
}

void M_WriteText2(int x, int y, const char *string, dpatch_t *font, float red,
                  float green, float blue, float alpha)
{
    M_WriteText3(x, y, string, font, red, green, blue, alpha, false, 0);
}

/**
 * Write a string using a colored, custom font.
 * Also do a type-in effect.
 */
void M_WriteText3(int x, int y, const char *string, dpatch_t *font,
                  float red, float green, float blue, float alpha,
                  boolean doTypeIn, int initialCount)
{
    int                 pass;
    int                 w, h;
    const char         *ch;
    int                 c;
    int                 cx;
    int                 cy;
    int                 count, maxCount, yoff;
    float               flash;
    float               fr = (1 + 2 * red) / 3;
    float               fb = (1 + 2 * blue) / 3;
    float               fg = (1 + 2 * green) / 3;
    float               fa = cfg.menuGlitter * alpha;

    for(pass = 0; pass < 2; ++pass)
    {
        count = initialCount;
        maxCount = typeInTime * 2;

        // Disable type-in?
        if(!doTypeIn || cfg.menuEffects > 0)
            maxCount = 0xffff;

        if(red >= 0)
            DGL_Color4f(red, green, blue, alpha);

        ch = string;
        cx = x;
        cy = y;

        for(;;)
        {
            c = *ch++;
            count++;
            yoff = 0;
            flash = 0;
            if(count == maxCount)
            {
                flash = 1;
                if(red >= 0)
                    DGL_Color4f(1, 1, 1, 1);
            }
            else if(count + 1 == maxCount)
            {
                flash = 0.5f;
                if(red >= 0)
                    DGL_Color4f((1 + red) / 2, (1 + green) / 2, (1 + blue) / 2,
                               alpha);
            }
            else if(count + 2 == maxCount)
            {
                flash = 0.25f;
                if(red >= 0)
                    DGL_Color4f(red, green, blue, alpha);
            }
            else if(count + 3 == maxCount)
            {
                flash = 0.12f;
                if(red >= 0)
                    DGL_Color4f(red, green, blue, alpha);
            }
            else if(count > maxCount)
            {
                break;
            }
            if(!c)
                break;
            if(c == '\n')
            {
                cx = x;
                cy += 12;
                continue;
            }

            c = toupper(c) - HU_FONTSTART;
            if(c < 0 || c >= HU_FONTSIZE)
            {
                cx += 4;
                continue;
            }

            w = font[c].width;
            h = font[c].height;

            if(!font[c].lump){
                // crap. a character we don't have a patch for...?!
                continue;
            }

            if(pass)
            {
                // The character itself.
                GL_DrawPatch_CS(cx, cy + yoff, font[c].lump);

                // Do something flashy!
                if(flash > 0)
                {
                    M_LetterFlash(cx, cy + yoff, w, h, true,
                                  fr, fg, fb, flash * fa);
                }
            }
            else if(cfg.menuShadow > 0)
            {
                // Shadow.
                M_LetterFlash(cx, cy + yoff, w, h, false, 1, 1, 1,
                              (red <
                               0 ? DGL_GetInteger(DGL_CURRENT_COLOR_A) /
                               255.0f : alpha) * cfg.menuShadow);
            }

            cx += w;
        }
    }
}

/**
 * This routine tests for a string-replacement for the patch.
 * If one is found, it's used instead of the original graphic.
 *
 * "{fontb; r=0.5; g=1; b=0; x=2; y=-2}This is good!"
 *
 * If the patch is not in an IWAD, it won't be replaced!
 *
 * @param altstring     String to use instead of the patch if appropriate.
 * @param built-in      (True) if the altstring is a built-in replacement
 *                      (ie it does not originate from a DED definition).
 */
void WI_DrawPatch(int x, int y, float r, float g, float b, float a,
                  int lump, char *altstring, boolean builtin, int halign)
{
    char    def[80], *string;
    int patchString = 0;
    int posx = x;
    lumppatch_t *patch;

    if(IS_DEDICATED)
        return;

    if(altstring && !builtin)
    {   // We have already determined a string to replace this with.
        if(W_IsFromIWAD(lump))
        {
            WI_DrawParamText(x, y, altstring, huFontB, r, g, b, a, false,
                             true, halign);
            return;
        }
    }
    else if(cfg.usePatchReplacement)
    {   // We might be able to replace the patch with a string
        if(lump <= 0)
            return;

        strcpy(def, "Patch Replacement|");
        strcat(def, W_LumpName(lump));

        patchString = Def_Get(DD_DEF_VALUE, def, &string);

        if(W_IsFromIWAD(lump))
        {
            // A user replacement?
            if(patchString)
            {
                WI_DrawParamText(x, y, string, huFontB, r, g, b, a, false,
                                 true, halign);
                return;
            }

            // A built-in replacement?
            if(cfg.usePatchReplacement == 2 && altstring)
            {
                WI_DrawParamText(x, y, altstring, huFontB, r, g, b, a, false,
                                 true, halign);
                return;
            }
        }
    }

    if(lump <= 0)
        return;

    patch = (lumppatch_t *) W_CacheLumpNum(lump, PU_CACHE);

    // No replacement possible/wanted - use the original patch.
    if(halign == ALIGN_CENTER)
        posx -= SHORT(patch->width) /2;
    else if(halign == ALIGN_RIGHT)
        posx -= SHORT(patch->width);

    DGL_Color4f(1, 1, 1, a);
    GL_DrawPatch_CS(posx, y, lump);
}

/**
 * Draws a little colour box using the background box for a border
 */
void M_DrawColorBox(int x, int y, float r, float g, float b, float a)
{
    if(a < 0)
        a = 1;

    M_DrawBackgroundBox(x, y, 2, 1, 1, 1, 1, a, false, 1);
    GL_SetNoTexture();
    GL_DrawRect(x-1,y-1, 4, 3, r, g, b, a);
}

/**
 * Draws a box using the border patches, a border is drawn outside.
 */
void M_DrawBackgroundBox(int x, int y, int w, int h, float red, float green,
                         float blue, float alpha, boolean background, int border)
{
    dpatch_t    *t = 0, *b = 0, *l = 0, *r = 0, *tl = 0, *tr = 0, *br = 0, *bl = 0;
    int         up = -1;

    switch(border)
    {
    case BORDERUP:
        t = &borderPatches[2];
        b = &borderPatches[0];
        l = &borderPatches[1];
        r = &borderPatches[3];
        tl = &borderPatches[6];
        tr = &borderPatches[7];
        br = &borderPatches[4];
        bl = &borderPatches[5];

        up = -1;
        break;

    case BORDERDOWN:
        t = &borderPatches[0];
        b = &borderPatches[2];
        l = &borderPatches[3];
        r = &borderPatches[1];
        tl = &borderPatches[4];
        tr = &borderPatches[5];
        br = &borderPatches[6];
        bl = &borderPatches[7];

        up = 1;
        break;

    default:
        break;
    }

    DGL_Color4f(red, green, blue, alpha);

    if(background)
    {
        GL_SetMaterial(R_MaterialNumForName(borderLumps[0], MAT_FLAT), MAT_FLAT);
        GL_DrawRectTiled(x, y, w, h, 64, 64);
    }

    if(border)
    {
        // Top
        GL_SetPatch(t->lump, DGL_REPEAT, DGL_REPEAT);
        GL_DrawRectTiled(x, y - t->height, w, t->height,
                         up * t->width, up * t->height);
        // Bottom
        GL_SetPatch(b->lump, DGL_REPEAT, DGL_REPEAT);
        GL_DrawRectTiled(x, y + h, w, b->height, up * b->width,
                         up * b->height);
        // Left
        GL_SetPatch(l->lump, DGL_REPEAT, DGL_REPEAT);
        GL_DrawRectTiled(x - l->width, y, l->width, h,
                         up * l->width, up * l->height);
        // Right
        GL_SetPatch(r->lump, DGL_REPEAT, DGL_REPEAT);
        GL_DrawRectTiled(x + w, y, r->width, h, up * r->width,
                         up * r->height);

        // Top Left
        GL_SetPatch(tl->lump, DGL_CLAMP, DGL_CLAMP);
        GL_DrawRect(x - tl->width, y - tl->height,
                    tl->width, tl->height,
                    red, green, blue, alpha);
        // Top Right
        GL_SetPatch(tr->lump, DGL_CLAMP, DGL_CLAMP);
        GL_DrawRect(x + w, y - tr->height, tr->width, tr->height,
                    red, green, blue, alpha);
        // Bottom Right
        GL_SetPatch(br->lump, DGL_CLAMP, DGL_CLAMP);
        GL_DrawRect(x + w, y + h, br->width, br->height,
                    red, green, blue, alpha);
        // Bottom Left
        GL_SetPatch(bl->lump, DGL_CLAMP, DGL_CLAMP);
        GL_DrawRect(x - bl->width, y + h, bl->width, bl->height,
                    red, green, blue, alpha);
    }
}

/**
 * Draws a menu slider control
 */
#if __JHERETIC__ || __JHEXEN__ || __JSTRIFE__
void M_DrawSlider(int x, int y, int width, int slot, float alpha)
#else
void M_DrawSlider(int x, int y, int width, int height, int slot, float alpha)
#endif
{
#if __JHERETIC__ || __JHEXEN__ || __JSTRIFE__
    DGL_Color4f( 1, 1, 1, alpha);

    GL_DrawPatch_CS(x - 32, y, W_GetNumForName("M_SLDLT"));
    GL_DrawPatch_CS(x + width * 8, y, W_GetNumForName("M_SLDRT"));

    GL_SetPatch(W_GetNumForName("M_SLDMD1"), DGL_REPEAT, DGL_REPEAT);
    GL_DrawRectTiled(x - 1, y + 1, width * 8 + 2, 13, 8, 13);

    DGL_Color4f( 1, 1, 1, alpha);
    GL_DrawPatch_CS(x + 4 + slot * 8, y + 7, W_GetNumForName("M_SLDKB"));
#else
    int         xx;
    float       scale = height / 13.0f;

    xx = x;
    GL_SetPatch(W_GetNumForName("M_THERML"), DGL_CLAMP, DGL_CLAMP);
    GL_DrawRect(xx, y, 6 * scale, height, 1, 1, 1, alpha);
    xx += 6 * scale;
    GL_SetPatch(W_GetNumForName("M_THERM2"), DGL_REPEAT, DGL_CLAMP);
    GL_DrawRectTiled(xx, y, 8 * width * scale, height, 8 * scale, height);
    xx += 8 * width * scale;
    GL_SetPatch(W_GetNumForName("M_THERMR"), DGL_CLAMP, DGL_CLAMP);
    GL_DrawRect(xx, y, 6 * scale, height, 1, 1, 1, alpha);

    GL_SetPatch(W_GetNumForName("M_THERMO"), DGL_CLAMP, DGL_CLAMP);
    GL_DrawRect(x + (6 + slot * 8) * scale, y, 6 * scale, height, 1, 1, 1,
                alpha);
#endif
}

void Draw_BeginZoom(float s, float originX, float originY)
{
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();

    DGL_Translatef(originX, originY, 0);
    DGL_Scalef(s, s, 1);
    DGL_Translatef(-originX, -originY, 0);
}

void Draw_EndZoom(void)
{
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

/**
 * Draws a 'fancy' fullscreen fog effect. Used by the menu.
 *
 * \fixme A bit of a mess really...
 */
void Hu_DrawFogEffect(int effectID, DGLuint tex, float texOffset[2],
                      float texAngle, float alpha, float arg1)
{
    const float     xscale = 2.0f;
    const float     yscale = 1.0f;

    if(alpha <= 0)
        return;

    if(cfg.menuEffects > 1)
        return;

    if(effectID == 4)
    {
        GL_SetNoTexture();
        GL_DrawRect(0, 0, 320, 200, 0.0f, 0.0f, 0.0f, alpha/2.5f);
        return;
    }

    if(effectID == 2)
    {
        DGL_Disable(DGL_TEXTURING);
        DGL_Color4f(alpha, alpha / 2, 0, alpha / 3);
        GL_BlendMode(BM_INVERSE_MUL);
        GL_DrawRectTiled(0, 0, 320, 200, 1, 1);
        DGL_Enable(DGL_TEXTURING);
    }

    DGL_Bind(tex);
    DGL_Color3f(alpha, alpha, alpha);
    DGL_MatrixMode(DGL_TEXTURE);
    DGL_PushMatrix();

    if(effectID == 1)
    {
        DGL_Color3f(alpha / 3, alpha / 2, alpha / 2);
        GL_BlendMode(BM_INVERSE_MUL);
    }
    else if(effectID == 2)
    {
        DGL_Color3f(alpha / 5, alpha / 3, alpha / 2);
        DGL_BlendFunc(DGL_SRC_ALPHA, DGL_SRC_ALPHA);
    }
    else if(effectID == 0)
    {
        DGL_Color3f(alpha * 0.15, alpha * 0.2, alpha * 0.3);
        DGL_BlendFunc(DGL_SRC_ALPHA, DGL_SRC_ALPHA);
    }

    if(effectID == 3)
    {   // The fancy one.
        DGL_BlendFunc(DGL_SRC_ALPHA, DGL_SRC_ALPHA);

        DGL_LoadIdentity();

        DGL_Translatef(texOffset[VX] / 320, texOffset[VY] / 200, 0);
        DGL_Rotatef(texAngle * 1, 0, 0, 1);
        DGL_Translatef(-texOffset[VX] / 320, -texOffset[VY] / 200, 0);

        DGL_Begin(DGL_QUADS);
            // Top Half
            DGL_Color4f(alpha * 0.25, alpha * 0.3, alpha * 0.4, 1 - (alpha * 0.8) );
            DGL_TexCoord2f( 0, 0);
            DGL_Vertex2f(0, 0);

            DGL_Color4f(alpha * 0.25, alpha * 0.3, alpha * 0.4, 1 - (alpha * 0.8) );
            DGL_TexCoord2f(xscale, 0);
            DGL_Vertex2f(320, 0);

            DGL_Color4f(alpha * 0.7, alpha * 0.7, alpha * 0.8, 1 - (0-(alpha * 0.9)));
            DGL_TexCoord2f(xscale, yscale * arg1);
            DGL_Vertex2f(320, 200 * arg1);

            DGL_Color4f(alpha * 0.7, alpha * 0.7, alpha * 0.8, 1 - (0-(alpha * 0.9)));
            DGL_TexCoord2f(0, yscale * arg1);
            DGL_Vertex2f(0, 200 * arg1);

            // Bottom Half
            DGL_Color4f(alpha * 0.7, alpha * 0.7, alpha * 0.8, 1 - (0-(alpha * 0.9)));
            DGL_TexCoord2f(0, yscale * arg1);
            DGL_Vertex2f(0, 200 * arg1);

            DGL_Color4f(alpha * 0.7, alpha * 0.7, alpha * 0.8, 1 - (0-(alpha * 0.9)));
            DGL_TexCoord2f( xscale, yscale * arg1);
            DGL_Vertex2f(320, 200 * arg1);

            DGL_Color4f(alpha * 0.25, alpha * 0.3, alpha * 0.4, 1 - (alpha * 0.8) );
            DGL_TexCoord2f(xscale, yscale);
            DGL_Vertex2f(320, 200);

            DGL_Color4f(alpha * 0.25, alpha * 0.3, alpha * 0.4, 1 - (alpha * 0.8) );
            DGL_TexCoord2f(0, yscale);
            DGL_Vertex2f(0, 200);
        DGL_End();
    }
    else
    {
        DGL_LoadIdentity();

        DGL_Translatef(texOffset[VX] / 320, texOffset[VY] / 200, 0);
        DGL_Rotatef(texAngle * (effectID == 0 ? 0.5 : 1), 0, 0, 1);
        DGL_Translatef(-texOffset[VX] / 320, -texOffset[VY] / 200, 0);
        if(effectID == 2)
            GL_DrawRectTiled(0, 0, 320, 200, 270 / 8, 4 * 225);
        else if(effectID == 0)
            GL_DrawRectTiled(0, 0, 320, 200, 270 / 4, 8 * 225);
        else
            GL_DrawRectTiled(0, 0, 320, 200, 270, 225);
    }

    DGL_MatrixMode(DGL_TEXTURE);
    DGL_PopMatrix();

    GL_BlendMode(BM_NORMAL);
}
