/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2005-2007 Daniel Swanson <danij@dengine.net>
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

/*
 * Heads-up displays, font handling, text drawing routines
 */

#ifdef MSVC
#  pragma warning(disable:4018)
#endif

// HEADER FILES ------------------------------------------------------------

#include <stdlib.h>
#include <ctype.h>
#include <math.h>

#if  __DOOM64TC__
#  include "doom64tc.h"
#elif __WOLFTC__
#  include "wolftc.h"
#elif __JDOOM__
#  include "jdoom.h"
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

#if __JDOOM__ || __JHERETIC__ || __DOOM64TC__ || __WOLFTC__
// Counter Cheat flags.
#define CCH_KILLS           0x1
#define CCH_ITEMS           0x2
#define CCH_SECRET          0x4
#define CCH_KILLS_PRCNT         0x8
#define CCH_ITEMS_PRCNT         0x10
#define CCH_SECRET_PRCNT        0x20
#endif

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern char   *borderLumps[];
extern boolean automapactive;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

dpatch_t hu_font[HU_FONTSIZE];
dpatch_t hu_font_a[HU_FONTSIZE], hu_font_b[HU_FONTSIZE];

int     typein_time = 0;

#ifdef __JDOOM__
 // Name graphics of each level (centered)
dpatch_t *lnames;
#endif

boolean hu_showallfrags = false;

cvar_t  hudCVars[] = {
#if __JDOOM__ || __JHERETIC__ || __DOOM64TC__ || __WOLFTC__
    {"map-cheat-counter", 0, CVT_BYTE, &cfg.counterCheat, 0, 63},
    {"map-cheat-counter-scale", 0, CVT_FLOAT, &cfg.counterCheatScale, .1f, 1},
#endif
    {NULL}
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static dpatch_t borderpatches[8];
static boolean headsupactive = false;

// Code -------------------------------------------------------------------

/**
 * Called during pre-init to register cvars and ccmds for the hud displays.
 */
void HU_Register(void)
{
    uint        i;

    for(i = 0; hudCVars[i].name; ++i)
        Con_AddVariable(&hudCVars[i]);
}

void R_CachePatch(dpatch_t *dp, char *name)
{
    lumppatch_t *patch;

    if(IS_DEDICATED)
        return;

    dp->lump = W_CheckNumForName(name);
    if(dp->lump == -1)
        return;
    patch = (lumppatch_t *) W_CacheLumpNum(dp->lump, PU_CACHE);
    dp->width = SHORT(patch->width);
    dp->height = SHORT(patch->height);
    dp->leftoffset = SHORT(patch->leftoffset);
    dp->topoffset = SHORT(patch->topoffset);

    // Precache the patch while we're at it.
    R_PrecachePatch(dp->lump);
}

/*
 * Loads the font patches and inits various strings
 * jHEXEN Note: Don't bother with the yellow font, we'll colour the white version
 */
void HU_Init(void)
{
    int     i;
    int     j;
    char    buffer[9];
#ifndef __JDOOM__
    dpatch_t tmp;
#endif

#ifdef __JDOOM__
    char    name[9];
#endif

    // Load the border patches
    for(i = 1; i < 9; ++i)
        R_CachePatch(&borderpatches[i-1], borderLumps[i]);

    // Setup strings.
#define INIT_STRINGS(x, x_idx) \
    for(i=0; i<sizeof(x_idx)/sizeof(int); i++) \
        x[i] = x_idx[i]==-1? "NEWLEVEL" : GET_TXT(x_idx[i]);

#ifdef __JDOOM__
    // load the heads-up fonts
    j = HU_FONTSTART;
    for(i = 0; i < HU_FONTSIZE; i++, j++)
    {
        // The original small red font.
        sprintf(buffer, "STCFN%.3d", j);
        R_CachePatch(&hu_font[i], buffer);

        // Small white font.
        sprintf(buffer, "FONTA%.3d", j);
        R_CachePatch(&hu_font_a[i], buffer);

        // Large (12) white font.
        sprintf(buffer, "FONTB%.3d", j);
        R_CachePatch(&hu_font_b[i], buffer);
        if(hu_font_b[i].lump == -1)
        {
            // This character is missing! (the first character
            // is supposedly always found)
            memcpy(&hu_font_b[0 + i], &hu_font_b[4], sizeof(dpatch_t));
        }
    }

    // load the map name patches
# if __DOOM64TC__
    lnames = Z_Malloc(sizeof(dpatch_t) * (39+7), PU_STATIC, 0);
    for(i = 0; i < 2; ++i) // number of episodes
    {
        for(j=0; j < (i==0? 39 : 7); ++j) // number of maps per episode
        {
            sprintf(name, "WILV%2.2d", (i * 39) + j);
            R_CachePatch(&lnames[(i * 39)+j], name);
        }
    }
# else
    if(gamemode == commercial)
    {
        int NUMCMAPS = 32;
        lnames = Z_Malloc(sizeof(dpatch_t) * NUMCMAPS, PU_STATIC, 0);
        for(i = 0; i < NUMCMAPS; i++)
        {
            sprintf(name, "CWILV%2.2d", i);
            R_CachePatch(&lnames[i], name);
        }
    }
    else
    {
        // Don't waste space - patches are loaded back to back
        // ie no space in the array is left for E1M10
        lnames = Z_Malloc(sizeof(dpatch_t) * (9*4), PU_STATIC, 0);
        for(j = 0; j < 4; j++) // number of episodes
        {
            for(i = 0; i < 9; i++) // number of maps per episode
            {
                sprintf(name, "WILV%2.2d", (j * 10) + i);
                R_CachePatch(&lnames[(j* 9)+i], name);
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
    for(i = 0; i < HU_FONTSIZE; i++, j++)
    {
        // The original small red font.
        sprintf(buffer, "STCFN%.3d", j);
        R_CachePatch(&hu_font[i], buffer);

        // Small white font.
        sprintf(buffer, "STCFN%.3d", j);
        R_CachePatch(&hu_font_a[i], buffer);

        // Large (12) white font.
        sprintf(buffer, "STBFN.3d", j);
        R_CachePatch(&hu_font_b[i], buffer);
        if(hu_font_b[i].lump == -1)
        {
            // This character is missing! (the first character
            // is supposedly always found)
            memcpy(&hu_font_b[0 + i], &hu_font_b[4], sizeof(dpatch_t));
        }
    }

    // deactivate monochrome mode
    DD_SetInteger(DD_MONOCHROME_PATCHES, 0);
#else
    // Tell Doomsday to load the following patches in monochrome mode
    // (2 = weighted average)
    DD_SetInteger(DD_MONOCHROME_PATCHES, 2);

    // Heretic/Hexen don't use ASCII numbered font patches
    // plus they don't even have a full set eg '!' = 1 '_'= 58 !!!
    j = 1;
    for(i = 0; i < HU_FONTSIZE; i++, j++)
    {
        // Small font.
        sprintf(buffer, "FONTA%.2d", j);
        R_CachePatch(&hu_font_a[i], buffer);

        // Large (12) font.
        sprintf(buffer, "FONTB%.2d", j);
        R_CachePatch(&hu_font_b[i], buffer);
        if(hu_font_b[i].lump == -1)
        {
            // This character is missing! (the first character
            // is supposedly always found)
            memcpy(&hu_font_b[0 + i], &hu_font_b[4], sizeof(dpatch_t));
        }
    }

    // deactivate monochrome mode
    DD_SetInteger(DD_MONOCHROME_PATCHES, 0);

    // Heretic and Hexen don't use ASCII numbering for all font patches.
    // As such we need to switch some pathches.

    tmp = hu_font_a[58];
    memcpy(&hu_font_a[58], &hu_font_a[62], sizeof(dpatch_t));
    memcpy(&hu_font_a[62], &tmp, sizeof(dpatch_t));

    tmp = hu_font_b[58];
    memcpy(&hu_font_b[58], &hu_font_b[62], sizeof(dpatch_t));
    memcpy(&hu_font_b[62], &tmp, sizeof(dpatch_t));

#endif

    HUMsg_Init();
}

void HU_UnloadData(void)
{
#if __JDOOM__
    if(lnames)
        Z_Free(lnames);
#endif
}

void HU_Stop(void)
{
    headsupactive = false;
}

void HU_Start(void)
{
    if(headsupactive)
        HU_Stop();

    HUMsg_Start();

    headsupactive = true;
}

void HU_Drawer(void)
{
    int     i, k, x, y;
    char    buf[80];
    player_t *plr;

    HUMsg_Drawer();

    if(hu_showallfrags)
    {
        for(y = 8, i = 0; i < MAXPLAYERS; i++)
        {
            plr = &players[i];
            if(!plr->plr || !plr->plr->ingame)
                continue;

            sprintf(buf, "%i%s", i, (i == consoleplayer ? "=" : ":"));

            M_WriteText(0, y, buf);

            x = 20;
            for(k = 0; k < MAXPLAYERS; k++, x += 18)
            {
                if(players[k].plr || !players[k].plr->ingame)
                    continue;

                sprintf(buf, "%i", plr->frags[k]);
                M_WriteText(x, y, buf);
            }

            y += 10;
        }
    }
}

#if __JDOOM__
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
        if(!players[i].plr->ingame)
            continue;

        inCount++;
        for(k = 0; k < MAXPLAYERS; ++k)
            totalFrags[i] += players[i].frags[k] * (k != i ? 1 : -1);
    }

    // Start drawing from the top.
# if __DOOM64TC__
    y = HU_TITLEY + 32 * (inCount - 1) * LINEHEIGHT_A;
# else
    y = HU_TITLEY + 32 * (20 - cfg.sbarscale) / 20 - (inCount - 1) * LINEHEIGHT_A;
#endif
    for(i = 0; i < inCount; ++i, y += LINEHEIGHT_A)
    {
        // Find the largest.
        for(max = FRAGS_DRAWN + 1, k = 0; k < MAXPLAYERS; ++k)
        {
            if(!players[k].plr->ingame || totalFrags[k] == FRAGS_DRAWN)
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
            gl.Color3f(0, .8f, 0);
            break;

        case 1:                // gray
            gl.Color3f(.45f, .45f, .45f);
            break;

        case 2:                // brown
            gl.Color3f(.7f, .5f, .4f);
            break;

        case 3:                // red
            gl.Color3f(1, 0, 0);
            break;
        }

        M_WriteText2(320 - w - M_StringWidth(name, hu_font_a) - 6, y, name,
                     hu_font_a, -1, -1, -1, -1);
        // A colon.
        M_WriteText2(320 - w - 5, y, ":", hu_font_a, -1, -1, -1, -1);
        // The frags count.
        sprintf(tmp, "%i", totalFrags[choose]);
        M_WriteText2(320 - w, y, tmp, hu_font_a, -1, -1, -1, -1);
        // Mark to ignore in the future.
        totalFrags[choose] = FRAGS_DRAWN;
    }
}
#endif

/**
 * Draws the deathmatch stats
 * TODO: Merge with drawFragsTable()
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
        if(!players[i].plr->ingame)
            continue;

        for(j = 0; j < MAXPLAYERS; ++j)
        {
            if(players[i].plr->ingame)
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
           !players[order[i]].plr->ingame)
        {
            continue;
        }
        else
        {
            GL_SetColor(their_colors[cfg.playerColor[order[i]]]);
            memset(textBuffer, 0, 80);
            strncpy(textBuffer, Net_GetPlayerName(order[i]), 78);
            strcat(textBuffer, ":");
            MN_TextFilter(textBuffer);

            M_WriteText2(4, yPosition, textBuffer, hu_font_a, -1, -1, -1, -1);
            j = M_StringWidth(textBuffer, hu_font_a);

            sprintf(textBuffer, "%d", fragCount[order[i]]);
            M_WriteText2(j + 8, yPosition, textBuffer, hu_font_a, -1, -1, -1, -1);
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
#ifndef __JDOOM__
#ifndef __JHERETIC__
    int     days, hours, minutes, seconds;
    int     worldTimer;
    char    timeBuffer[15];
    char    dayBuffer[20];

    worldTimer = players[displayplayer].worldTimer;

    worldTimer /= 35;
    days = worldTimer / 86400;
    worldTimer -= days * 86400;
    hours = worldTimer / 3600;
    worldTimer -= hours * 3600;
    minutes = worldTimer / 60;
    worldTimer -= minutes * 60;
    seconds = worldTimer;

    sprintf(timeBuffer, "%.2d : %.2d : %.2d", hours, minutes, seconds);
    M_WriteText2(240, 8, timeBuffer, hu_font_a, 1, 1, 1, 1);

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

        M_WriteText2(240, 20, dayBuffer, hu_font_a, 1, 1, 1, 1);
        if(days >= 5)
        {
            M_WriteText2(230, 35, "YOU FREAK!!!", hu_font_a, 1, 1, 1, 1);
        }
    }
#endif
#endif
}

/**
 * Handles what counters to draw eg title, timer, dm stats etc
 */
void HU_DrawMapCounters(void)
{
    player_t   *plr;
#if __JDOOM__ || __JHERETIC__
    char        buf[40], tmp[20];
    int         x = 5, y = LINEHEIGHT_A * 3;
#endif

    plr = &players[displayplayer];

#if __JDOOM__ || __JHERETIC__
    gl.Color3f(1, 1, 1);

    gl.MatrixMode(DGL_MODELVIEW);
    gl.PushMatrix();
#endif

    gl.Enable(DGL_TEXTURING);

    drawWorldTimer();

#if __JDOOM__ || __JHERETIC__ || __DOOM64TC__ || __WOLFTC__
    Draw_BeginZoom(cfg.counterCheatScale, x, y);

    if(cfg.counterCheat)
    {
        // Kills.
        if(cfg.counterCheat & (CCH_KILLS | CCH_KILLS_PRCNT))
        {
            strcpy(buf, "Kills: ");
            if(cfg.counterCheat & CCH_KILLS)
            {
                sprintf(tmp, "%i/%i ", plr->killcount, totalkills);
                strcat(buf, tmp);
            }
            if(cfg.counterCheat & CCH_KILLS_PRCNT)
            {
                sprintf(tmp, "%s%i%%%s", (cfg.counterCheat & CCH_KILLS ? "(" : ""),
                        totalkills ? plr->killcount * 100 / totalkills : 100,
                        (cfg.counterCheat & CCH_KILLS ? ")" : ""));
                strcat(buf, tmp);
            }

            M_WriteText2(x, y, buf, hu_font_a, 1, 1, 1, 1);

            y += LINEHEIGHT_A;
        }

        // Items.
        if(cfg.counterCheat & (CCH_ITEMS | CCH_ITEMS_PRCNT))
        {
            strcpy(buf, "Items: ");
            if(cfg.counterCheat & CCH_ITEMS)
            {
                sprintf(tmp, "%i/%i ", plr->itemcount, totalitems);
                strcat(buf, tmp);
            }
            if(cfg.counterCheat & CCH_ITEMS_PRCNT)
            {
                sprintf(tmp, "%s%i%%%s", (cfg.counterCheat & CCH_ITEMS ? "(" : ""),
                        totalitems ? plr->itemcount * 100 / totalitems : 100,
                        (cfg.counterCheat & CCH_ITEMS ? ")" : ""));
                strcat(buf, tmp);
            }

            M_WriteText2(x, y, buf, hu_font_a, 1, 1, 1, 1);

            y += LINEHEIGHT_A;
        }

        // Secrets.
        if(cfg.counterCheat & (CCH_SECRET | CCH_SECRET_PRCNT))
        {
            strcpy(buf, "Secret: ");
            if(cfg.counterCheat & CCH_SECRET)
            {
                sprintf(tmp, "%i/%i ", plr->secretcount, totalsecret);
                strcat(buf, tmp);
            }
            if(cfg.counterCheat & CCH_SECRET_PRCNT)
            {
                sprintf(tmp, "%s%i%%%s", (cfg.counterCheat & CCH_SECRET ? "(" : ""),
                        totalsecret ? plr->secretcount * 100 / totalsecret : 100,
                        (cfg.counterCheat & CCH_SECRET ? ")" : ""));
                strcat(buf, tmp);
            }

            M_WriteText2(x, y, buf, hu_font_a, 1, 1, 1, 1);

            y += LINEHEIGHT_A;
        }
    }

    Draw_EndZoom();

#if __JDOOM__
    if(deathmatch)
        drawFragsTable();
#endif

    gl.MatrixMode(DGL_MODELVIEW);
    gl.PopMatrix();

#endif

#if __JHEXEN__ || __JSTRIFE__
    if(IS_NETGAME)
    {
        // Always draw deathmatch stats in a netgame, even in coop
        drawDeathmatchStats();
    }
#endif
}

void HU_Ticker(void)
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
    int     k;

    for(k = 0; text[k]; k++)
    {
        text[k] = MN_FilterChar(text[k]);
    }
}

/*
 * Expected: <whitespace> = <whitespace> <float>
 */
float WI_ParseFloat(char **str)
{
    float   value;
    char   *end;

    *str = M_SkipWhite(*str);
    if(**str != '=')
        return 0;               // Now I'm confused!
    *str = M_SkipWhite(*str + 1);
    value = strtod(*str, &end);
    *str = end;
    return value;
}

/*
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
                        font = hu_font_a;
                        string += 5;
                    }
                    else if(!strnicmp(string, "fontb", 5))
                    {
                        font = hu_font_b;
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
                    font = hu_font_a;
                    string += 5;
                }
                else if(!strnicmp(string, "fontb", 5))
                {
                    font = hu_font_b;
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
            gl.MatrixMode(DGL_MODELVIEW);
            gl.PushMatrix();

            // Rotate.
            if(angle != 0)
            {
                // The origin is the specified (x,y) for the patch.
                // We'll undo the VGA aspect ratio (otherwise the result would
                // be skewed).
                gl.Translatef(x, y, 0);
                gl.Scalef(1, 200.0f / 240.0f, 1);
                gl.Rotatef(angle, 0, 0, 1);
                gl.Scalef(1, 240.0f / 200.0f, 1);
                gl.Translatef(-x, -y, 0);
            }

            gl.Translatef(cx + offX - alignx,
                          cy + offY +
                          (caseScale ? caseMod[curCase].offset : 0), 0);
            extraScale = (caseScale ? caseMod[curCase].scale : 1);
            gl.Scalef(scaleX, scaleY * extraScale, 1);

            // Draw it.
            M_WriteText3(0, 0, temp, font, r, g, b, a, typeIn,
                         typeIn ? charCount : 0);
            charCount += strlen(temp);

            // Advance the current position.
            cx += scaleX * M_StringWidth(temp, font);

            gl.MatrixMode(DGL_MODELVIEW);
            gl.PopMatrix();
        }
    }
}

/**
 * Find string width from hu_font chars
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
 * Find string height from hu_font chars
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
    gl.GetIntegerv(DGL_RGBA, origColor);

    gl.Bind(Get(DD_DYNLIGHT_TEXTURE));

    if(bright)
        gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_ONE);
    else
        gl.Func(DGL_BLENDING, DGL_ZERO, DGL_ONE_MINUS_SRC_ALPHA);

    GL_DrawRect(x + w / 2.0f - fw / 2, y + h / 2.0f - fh / 2, fw, fh, red,
                green, blue, alpha);

    gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_ONE_MINUS_SRC_ALPHA);

    // Restore original color.
    gl.Color4ub(origColor[0], origColor[1], origColor[2], origColor[3]);
}

/*
 * Write a string using the hu_font
 */
void M_WriteText(int x, int y, const char *string)
{
    M_WriteText2(x, y, string, hu_font_a, 1, 1, 1, 1);
}

void M_WriteText2(int x, int y, const char *string, dpatch_t *font, float red,
                  float green, float blue, float alpha)
{
    M_WriteText3(x, y, string, font, red, green, blue, alpha, true, 0);
}

/*
 * Write a string using a colored, custom font.
 * Also do a type-in effect.
 */
void M_WriteText3(int x, int y, const char *string, dpatch_t *font,
                  float red, float green, float blue, float alpha,
                  boolean doTypeIn, int initialCount)
{
    int     pass;
    int     w, h;
    const char *ch;
    int     c;
    int     cx;
    int     cy;
    int     count, maxCount, yoff;
    float   flash;

    float   fr = (1 + 2 * red) / 3;
    float   fb = (1 + 2 * blue) / 3;
    float   fg = (1 + 2 * green) / 3;
    float   fa = cfg.menuGlitter * alpha;

    for(pass = 0; pass < 2; ++pass)
    {
        count = initialCount;
        maxCount = typein_time * 2;

        // Disable type-in?
        if(!doTypeIn || cfg.menuEffects > 0)
            maxCount = 0xffff;

        if(red >= 0)
            gl.Color4f(red, green, blue, alpha);

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
                    gl.Color4f(1, 1, 1, 1);
            }
            else if(count + 1 == maxCount)
            {
                flash = 0.5f;
                if(red >= 0)
                    gl.Color4f((1 + red) / 2, (1 + green) / 2, (1 + blue) / 2,
                               alpha);
            }
            else if(count + 2 == maxCount)
            {
                flash = 0.25f;
                if(red >= 0)
                    gl.Color4f(red, green, blue, alpha);
            }
            else if(count + 3 == maxCount)
            {
                flash = 0.12f;
                if(red >= 0)
                    gl.Color4f(red, green, blue, alpha);
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
                               0 ? gl.GetInteger(DGL_A) /
                               255.0f : alpha) * cfg.menuShadow);
            }

            cx += w;
        }
    }
}

/*
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
            WI_DrawParamText(x, y, altstring, hu_font_b, r, g, b, a, false,
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
                WI_DrawParamText(x, y, string, hu_font_b, r, g, b, a, false,
                                 true, halign);
                return;
            }

            // A built-in replacement?
            if(cfg.usePatchReplacement == 2 && altstring)
            {
                WI_DrawParamText(x, y, altstring, hu_font_b, r, g, b, a, false,
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

    gl.Color4f(1, 1, 1, a);
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
 *  Draws a box using the border patches, a border is drawn outside.
 */
void M_DrawBackgroundBox(int x, int y, int w, int h, float red, float green,
                         float blue, float alpha, boolean background, int border)
{
    dpatch_t    *t = 0, *b = 0, *l = 0, *r = 0, *tl = 0, *tr = 0, *br = 0, *bl = 0;
    int         up = -1;

    switch(border)
    {
    case BORDERUP:
        t = &borderpatches[2];
        b = &borderpatches[0];
        l = &borderpatches[1];
        r = &borderpatches[3];
        tl = &borderpatches[6];
        tr = &borderpatches[7];
        br = &borderpatches[4];
        bl = &borderpatches[5];

        up = -1;
        break;

    case BORDERDOWN:
        t = &borderpatches[0];
        b = &borderpatches[2];
        l = &borderpatches[3];
        r = &borderpatches[1];
        tl = &borderpatches[4];
        tr = &borderpatches[5];
        br = &borderpatches[6];
        bl = &borderpatches[7];

        up = 1;
        break;

    default:
        break;
    }

    GL_SetColorAndAlpha(red, green, blue, alpha);

    if(background)
    {
        GL_SetFlat(R_FlatNumForName(borderLumps[0]));
        GL_DrawRectTiled(x, y, w, h, 64, 64);
    }

    if(border)
    {
        // Top
        GL_SetPatch(t->lump);
        GL_DrawRectTiled(x, y - t->height, w, t->height,
                         up * t->width, up * t->height);
        // Bottom
        GL_SetPatch(b->lump);
        GL_DrawRectTiled(x, y + h, w, b->height, up * b->width,
                         up * b->height);
        // Left
        GL_SetPatch(l->lump);
        GL_DrawRectTiled(x - l->width, y, l->width, h,
                         up * l->width, up * l->height);
        // Right
        GL_SetPatch(r->lump);
        GL_DrawRectTiled(x + w, y, r->width, h, up * r->width,
                         up * r->height);

        // Top Left
        GL_SetPatch(tl->lump);
        GL_DrawRectTiled(x - tl->width, y - tl->height,
                         tl->width, tl->height,
                         up * tl->width, up * tl->height);
        // Top Right
        GL_SetPatch(tr->lump);
        GL_DrawRectTiled(x + w, y - tr->height, tr->width,
                         tr->height, up * tr->width,
                         up * tr->height);
        // Bottom Right
        GL_SetPatch(br->lump);
        GL_DrawRectTiled(x + w, y + h, br->width, br->height,
                         up * br->width, up * br->height);
        // Bottom Left
        GL_SetPatch(bl->lump);
        GL_DrawRectTiled(x - bl->width, y + h, bl->width,
                         bl->height, up * bl->width,
                         up * bl->height);
    }
}

/**
 * Draws a menu slider control
 */
#ifndef __JDOOM__
void M_DrawSlider(int x, int y, int width, int slot, float alpha)
#else
void M_DrawSlider(int x, int y, int width, int height, int slot, float alpha)
#endif
{
#ifndef __JDOOM__
    gl.Color4f( 1, 1, 1, alpha);

    GL_DrawPatch_CS(x - 32, y, W_GetNumForName("M_SLDLT"));
    GL_DrawPatch_CS(x + width * 8, y, W_GetNumForName("M_SLDRT"));

    GL_SetPatch(W_GetNumForName("M_SLDMD1"));
    GL_DrawRectTiled(x - 1, y + 1, width * 8 + 2, 13, 8, 13);

    gl.Color4f( 1, 1, 1, alpha);
    GL_DrawPatch_CS(x + 4 + slot * 8, y + 7, W_GetNumForName("M_SLDKB"));
#else
    int         xx;
    float       scale = height / 13.0f;

    xx = x;
    GL_SetPatch(W_GetNumForName("M_THERML"));
    GL_DrawRect(xx, y, 6 * scale, height, 1, 1, 1, alpha);
    xx += 6 * scale;
    GL_SetPatch(W_GetNumForName("M_THERM2"));    // in jdoom.wad
    GL_DrawRectTiled(xx, y, 8 * width * scale, height, 8 * scale, height);
    xx += 8 * width * scale;
    GL_SetPatch(W_GetNumForName("M_THERMR"));
    GL_DrawRect(xx, y, 6 * scale, height, 1, 1, 1, alpha);
    GL_SetPatch(W_GetNumForName("M_THERMO"));
    GL_DrawRect(x + (6 + slot * 8) * scale, y, 6 * scale, height, 1, 1, 1,
                alpha);
#endif
}

void Draw_BeginZoom(float s, float originX, float originY)
{
    gl.MatrixMode(DGL_MODELVIEW);
    gl.PushMatrix();

    gl.Translatef(originX, originY, 0);
    gl.Scalef(s, s, 1);
    gl.Translatef(-originX, -originY, 0);
}

void Draw_EndZoom(void)
{
    gl.MatrixMode(DGL_MODELVIEW);
    gl.PopMatrix();
}
