/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2005-2006 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006 Martin Eyre <martineyre@btinternet.com>
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
 * Intermission/stat screens
 *
 * Different between registered DOOM (1994) and
 * Ultimate DOOM - Final edition (retail, 1995?).
 * This is supposedly ignored for commercial release (aka DOOM II),
 * which had 34 maps in one episode. So there.
 */

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <ctype.h>

#include "wolftc.h"

#include "hu_stuff.h"
#include "d_net.h"

// MACROS ------------------------------------------------------------------

#define NUM_TEAMS       4       // Color = team.

#define NUMEPISODES     4
#define NUMMAPS         9

// GLOBAL LOCATIONS
#define WI_TITLEY       2
#define WI_SPACINGY     33

// SINGPLE-PLAYER STUFF
#define SP_STATSX       50
#define SP_STATSY       50
#define SP_TIMEX        16
#define SP_TIMEY        (SCREENHEIGHT-32)

// NET GAME STUFF
#define NG_STATSY       50
#define NG_STATSX       (32 + star.width/2 + 32*!dofrags)
#define NG_SPACINGX     64

// DEATHMATCH STUFF
#define DM_MATRIXX      42
#define DM_MATRIXY      68
#define DM_SPACINGX     40
#define DM_TOTALSX      269
#define DM_KILLERSX     10
#define DM_KILLERSY     100
#define DM_VICTIMSX     5
#define DM_VICTIMSY     50

// States for single-player
#define SP_KILLS        0
#define SP_ITEMS        2
#define SP_SECRET       4
#define SP_FRAGS        6
#define SP_TIME         8
#define SP_PAR          ST_TIME
#define SP_PAUSE        1

// in seconds
#define SHOWNEXTLOCDELAY    4

// TYPES -------------------------------------------------------------------

typedef enum {
    ANIM_ALWAYS,
    ANIM_RANDOM,
    ANIM_LEVEL
} animenum_t;

typedef struct {
    int     x;
    int     y;
} point_t;

typedef struct {
    animenum_t type;

    // period in tics between animations
    int     period;

    // number of animation frames
    int     nanims;

    // location of animation
    point_t loc;

    // ALWAYS: n/a,
    // RANDOM: period deviation (<256),
    // LEVEL: level
    int     data1;

    // ALWAYS: n/a,
    // RANDOM: random base period,
    // LEVEL: n/a
    int     data2;

    // actual graphics for frames of animations
    dpatch_t p[3];

    // following must be initialized to zero before use!

    // next value of bcnt (used in conjunction with period)
    int     nexttic;

    // last drawn animation frame
    int     lastdrawn;

    // next frame number to animate
    int     ctr;

    // used by RANDOM and LEVEL when animating
    int     state;

} wianim_t;

typedef struct {
    int     members;            // 0 if team not present.
    int     frags[NUM_TEAMS];
    int     totalfrags;         // Kills minus suicides.
    int     items;
    int     kills;
    int     secret;
} teaminfo_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern dpatch_t hu_font[HU_FONTSIZE];
extern dpatch_t hu_font_a[HU_FONTSIZE], hu_font_b[HU_FONTSIZE];

 // Name graphics of each level (centered)
extern dpatch_t *lnames;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static teaminfo_t teaminfo[NUM_TEAMS];

static point_t lnodes[NUMEPISODES][NUMMAPS] = {
    // Episode 0 World Map
    {
     {185, 164},                // location of level 0 (CJ)
     {148, 143},                // location of level 1 (CJ)
     {69, 122},                 // location of level 2 (CJ)
     {209, 102},                // location of level 3 (CJ)
     {116, 89},                 // location of level 4 (CJ)
     {166, 55},                 // location of level 5 (CJ)
     {71, 56},                  // location of level 6 (CJ)
     {135, 29},                 // location of level 7 (CJ)
     {71, 24}                   // location of level 8 (CJ)
     },

    // Episode 1 World Map should go here
    {
     {254, 25},                 // location of level 0 (CJ)
     {97, 50},                  // location of level 1 (CJ)
     {188, 64},                 // location of level 2 (CJ)
     {128, 78},                 // location of level 3 (CJ)
     {214, 92},                 // location of level 4 (CJ)
     {133, 130},                // location of level 5 (CJ)
     {208, 136},                // location of level 6 (CJ)
     {148, 140},                // location of level 7 (CJ)
     {235, 158}                 // location of level 8 (CJ)
     },

    // Episode 2 World Map should go here
    {
     {156, 168},                // location of level 0 (CJ)
     {48, 154},                 // location of level 1 (CJ)
     {174, 95},                 // location of level 2 (CJ)
     {265, 75},                 // location of level 3 (CJ)
     {130, 48},                 // location of level 4 (CJ)
     {279, 23},                 // location of level 5 (CJ)
     {198, 48},                 // location of level 6 (CJ)
     {140, 25},                 // location of level 7 (CJ)
     {281, 136}                 // location of level 8 (CJ)
     }
};

//
// Animation locations for episode 0 (1).
// Using patches saves a lot of space,
//  as they replace 320x200 full screen frames.
//
static wianim_t epsd0animinfo[] = {
    {ANIM_ALWAYS, TICRATE / 3, 3, {224, 104}},
    {ANIM_ALWAYS, TICRATE / 3, 3, {184, 160}},
    {ANIM_ALWAYS, TICRATE / 3, 3, {112, 136}},
    {ANIM_ALWAYS, TICRATE / 3, 3, {72, 112}},
    {ANIM_ALWAYS, TICRATE / 3, 3, {88, 96}},
    {ANIM_ALWAYS, TICRATE / 3, 3, {64, 48}},
    {ANIM_ALWAYS, TICRATE / 3, 3, {192, 40}},
    {ANIM_ALWAYS, TICRATE / 3, 3, {136, 16}},
    {ANIM_ALWAYS, TICRATE / 3, 3, {80, 16}},
    {ANIM_ALWAYS, TICRATE / 3, 3, {64, 24}}
};

static wianim_t epsd1animinfo[] = {
    {ANIM_LEVEL, TICRATE / 3, 1, {128, 136}, 1},
    {ANIM_LEVEL, TICRATE / 3, 1, {128, 136}, 2},
    {ANIM_LEVEL, TICRATE / 3, 1, {128, 136}, 3},
    {ANIM_LEVEL, TICRATE / 3, 1, {128, 136}, 4},
    {ANIM_LEVEL, TICRATE / 3, 1, {128, 136}, 5},
    {ANIM_LEVEL, TICRATE / 3, 1, {128, 136}, 6},
    {ANIM_LEVEL, TICRATE / 3, 1, {128, 136}, 7},
    {ANIM_LEVEL, TICRATE / 3, 3, {192, 144}, 8},
    {ANIM_LEVEL, TICRATE / 3, 1, {128, 136}, 8}
};

static wianim_t epsd2animinfo[] = {
    {ANIM_ALWAYS, TICRATE / 3, 3, {104, 168}},
    {ANIM_ALWAYS, TICRATE / 3, 3, {40, 136}},
    {ANIM_ALWAYS, TICRATE / 3, 3, {160, 96}},
    {ANIM_ALWAYS, TICRATE / 3, 3, {104, 80}},
    {ANIM_ALWAYS, TICRATE / 3, 3, {120, 32}},
    {ANIM_ALWAYS, TICRATE / 4, 3, {40, 0}}
};

static int NUMANIMS[NUMEPISODES] = {
    sizeof(epsd0animinfo) / sizeof(wianim_t),
    sizeof(epsd1animinfo) / sizeof(wianim_t),
    sizeof(epsd2animinfo) / sizeof(wianim_t)
};

static wianim_t *anims[NUMEPISODES] = {
    epsd0animinfo,
    epsd1animinfo,
    epsd2animinfo
};

// used to accelerate or skip a stage
static int acceleratestage;

static boolean snl_pointeron = false;

static int sp_state;

static int dm_state;
static int dm_frags[NUM_TEAMS][NUM_TEAMS];
static int dm_totals[NUM_TEAMS];

static int cnt_frags[NUM_TEAMS];
static int dofrags;
static int ng_state;

// wbs->pnum
static int me;
static int myteam;

 // specifies current state
static stateenum_t state;

// contains information passed into intermission
static wbstartstruct_t *wbs;

static wbplayerstruct_t *plrs;  // wbs->plyr[]

// used for general timing
static int cnt;

// used for timing of background animation
static int bcnt;

// signals to refresh everything for one frame
static int firstrefresh;

static int cnt_kills[NUM_TEAMS];
static int cnt_items[NUM_TEAMS];
static int cnt_secret[NUM_TEAMS];
static int cnt_time;
static int cnt_par;
static int cnt_pause;

// # of commercial levels
//static int NUMCMAPS;

//
//  GRAPHICS
//

// background (map of levels).
static dpatch_t bg;

// You Are Here graphic
static dpatch_t yah[2];

// splat
static dpatch_t splat;

// %, : graphics
static dpatch_t percent;
static dpatch_t colon;

// 0-9 graphic
static dpatch_t num[10];

// minus sign
static dpatch_t wiminus;

// "Finished!" graphics
static dpatch_t finished;

// "Entering" graphic
static dpatch_t entering;

// "secret"
static dpatch_t sp_secret;

 // "Kills", "Scrt", "Items", "Frags"
static dpatch_t kills;
static dpatch_t secret;
static dpatch_t items;
static dpatch_t frags;

// Time sucks.
static dpatch_t time;
static dpatch_t par;
static dpatch_t sucks;

// "killers", "victims"
static dpatch_t killers;
static dpatch_t victims;

// "Total", your face, your dead face
static dpatch_t total;
static dpatch_t star;
static dpatch_t bstar;

// "red P[1..MAXPLAYERS]"
static dpatch_t p[MAXPLAYERS];

// "gray P[1..MAXPLAYERS]"
static dpatch_t bp[MAXPLAYERS];

// CODE --------------------------------------------------------------------

void WI_slamBackground(void)
{
    GL_DrawPatch(0, 0, bg.lump);
}

/*
 * The ticker is used to detect keys
 *  because of timing issues in netgames.
 */
boolean WI_Responder(event_t *ev)
{
    return false;
}

/*
 * Draws "<Levelname> Finished!"
 */
void WI_drawLF(void)
{
    int     y = WI_TITLEY;
    int     mapnum;
    char   *lname, *ptr;

    lname = (char *) DD_GetVariable(DD_MAP_NAME);

    if(gamemode == commercial)
        mapnum = wbs->last;
    else
        mapnum = ((gameepisode -1) * 9) + wbs->last;

    ptr = strchr(lname, ':');   // Skip the E#M# or Level #.
    if(ptr)
    {
        lname = ptr + 1;
        while(*lname && isspace(*lname))
            lname++;
    }

    // draw <LevelName>
    WI_DrawPatch(SCREENWIDTH / 2, y, 1, 1, 1, 1,
                 lnames[mapnum].lump, lname, false, ALIGN_CENTER);

    // draw "Finished!"
    y += (5 * lnames[mapnum].height) / 4;

    WI_DrawPatch(SCREENWIDTH / 2, y, 1, 1, 1, 1,
                 finished.lump, NULL, false, ALIGN_CENTER);
}

/*
 * Draws "Entering <LevelName>"
 */
void WI_drawEL(void)
{
    int     y = WI_TITLEY;
    int     mapnum;
    char   *lname = "", *ptr;
    ddmapinfo_t minfo;
    char    levid[10];

    if(gamemode == commercial)
    {
        sprintf(levid, "MAP%02i", wbs->next+1);
        mapnum = wbs->next;
    }
    else
    {
        sprintf(levid, "E%iM%i", gameepisode, wbs->next+1);
        mapnum = ((gameepisode -1) * 9) + wbs->next;
    }

    // See if there is a level name
    if(Def_Get(DD_DEF_MAP_INFO, levid, &minfo) && minfo.name)
        lname = minfo.name;

    ptr = strchr(lname, ':');   // Skip the E#M# or Level #.
    if(ptr)
    {
        lname = ptr + 1;
        while(*lname && isspace(*lname))
            lname++;
    }

    // draw "Entering"
    WI_DrawPatch(SCREENWIDTH / 2, y, 1, 1, 1, 1, entering.lump,
                 NULL, false, ALIGN_CENTER);

    // draw level
    y += (5 * lnames[wbs->next].height) / 4;

    WI_DrawPatch(SCREENWIDTH / 2, y, 1, 1, 1, 1,
                 lnames[((gameepisode -1) * 9) + wbs->next].lump,
                 lname, false, ALIGN_CENTER);
}

void WI_drawOnLnode(int n, dpatch_t * c)
{
    int     i;
    int     left;
    int     top;
    int     right;
    int     bottom;
    boolean fits = false;

    i = 0;
    do
    {
        left = lnodes[wbs->epsd][n].x - c[i].leftoffset;
        top = lnodes[wbs->epsd][n].y - c[i].topoffset;
        right = left + c[i].width;
        bottom = top + c[i].height;
        if(left >= 0 && right < SCREENWIDTH && top >= 0 &&
           bottom < SCREENHEIGHT)
            fits = true;
        else
            i++;
    } while(!fits && i != 2);

    if(fits && i < 2)
    {
        WI_DrawPatch(lnodes[wbs->epsd][n].x, lnodes[wbs->epsd][n].y, 1, 1, 1, 1,
                     c[i].lump, NULL, false, ALIGN_LEFT);
    }
    else
    {
        // DEBUG
        Con_Message("Could not place patch on level %d", n + 1);
    }
}

void WI_initAnimatedBack(void)
{
    int     i;
    wianim_t *a;

    if(gamemode == commercial)
        return;
    if(wbs->epsd > 2)
        return;

    for(i = 0; i < NUMANIMS[wbs->epsd]; i++)
    {
        a = &anims[wbs->epsd][i];

        // init variables
        a->ctr = -1;

        // specify the next time to draw it
        if(a->type == ANIM_ALWAYS)
            a->nexttic = bcnt + 1 + (M_Random() % a->period);
        else if(a->type == ANIM_RANDOM)
            a->nexttic = bcnt + 1 + a->data2 + (M_Random() % a->data1);
        else if(a->type == ANIM_LEVEL)
            a->nexttic = bcnt + 1;
    }
}

void WI_updateAnimatedBack(void)
{
    int     i;
    wianim_t *a;

    if(gamemode == commercial)
        return;
    if(wbs->epsd > 2)
        return;

    for(i = 0; i < NUMANIMS[wbs->epsd]; i++)
    {
        a = &anims[wbs->epsd][i];

        if(bcnt == a->nexttic)
        {
            switch (a->type)
            {
            case ANIM_ALWAYS:
                if(++a->ctr >= a->nanims)
                    a->ctr = 0;
                a->nexttic = bcnt + a->period;
                break;

            case ANIM_RANDOM:
                a->ctr++;
                if(a->ctr == a->nanims)
                {
                    a->ctr = -1;
                    a->nexttic = bcnt + a->data2 + (M_Random() % a->data1);
                }
                else
                    a->nexttic = bcnt + a->period;
                break;

            case ANIM_LEVEL:
                // gawd-awful hack for level anims
                if(!(state == StatCount && i == 7) && wbs->next == a->data1)
                {
                    a->ctr++;
                    if(a->ctr == a->nanims)
                        a->ctr--;
                    a->nexttic = bcnt + a->period;
                }
                break;
            }
        }
    }
}

void WI_drawAnimatedBack(void)
{
    int     i;
    wianim_t *a;

    if(gamemode == commercial)
        return;
    if(wbs->epsd > 2)
        return;

    for(i = 0; i < NUMANIMS[wbs->epsd]; i++)
    {
        a = &anims[wbs->epsd][i];
        if(a->ctr >= 0)
            WI_DrawPatch(a->loc.x, a->loc.y, 1, 1, 1, 1, a->p[a->ctr].lump,
                         NULL, false, ALIGN_LEFT);
    }

}

/*
 * Draws a number.
 * If digits > 0, then use that many digits minimum,
 * otherwise only use as many as necessary.
 * Returns new x position.
 */
int WI_drawNum(int x, int y, int n, int digits)
{
    int     fontwidth = num[0].width;
    int     neg;
    int     temp;

    if(digits < 0)
    {
        if(!n)
        {
            // make variable-length zeros 1 digit long
            digits = 1;
        }
        else
        {
            // figure out # of digits in #
            digits = 0;
            temp = n;
            while(temp)
            {
                temp /= 10;
                digits++;
            }
        }
    }
    neg = n < 0;
    if(neg)
        n = -n;

    // if non-number, do not draw it
    if(n == 1994)
        return 0;

    // draw the new number
    while(digits--)
    {
        x -= fontwidth;
        WI_DrawPatch(x, y, 1, 1, 1, 1, num[n % 10].lump, NULL, false, ALIGN_LEFT);
        n /= 10;
    }

    // draw a minus sign if necessary
    if(neg)
        WI_DrawPatch(x -= 8, y, 1, 1, 1, 1, wiminus.lump, NULL, false, ALIGN_LEFT);

    return x;
}

void WI_drawPercent(int x, int y, int p)
{
    if(p < 0)
        return;

    WI_DrawPatch(x, y, 1, 1, 1, 1, percent.lump, NULL, false, ALIGN_LEFT);
    WI_drawNum(x, y, p, -1);
}

/*
 * Display level completion time and par, or "sucks" message if overflow.
 */
void WI_drawTime(int x, int y, int t)
{
    int     div;
    int     n;

    if(t < 0)
        return;

    if(t <= 61 * 59)
    {
        div = 1;
        do
        {
            n = (t / div) % 60;
            x = WI_drawNum(x, y, n, 2) - colon.width;
            div *= 60;

            // draw
            if(div == 60 || t / div)
                WI_DrawPatch(x, y, 1, 1, 1, 1, colon.lump, NULL, false,
                             ALIGN_LEFT);

        } while(t / div);
    }
    else
    {
        // "sucks"
        WI_DrawPatch(x - sucks.width, y, 1, 1, 1, 1, sucks.lump,
                     NULL, false, ALIGN_LEFT);
    }
}

void WI_End(void)
{
    void    WI_unloadData(void);

    NetSv_Intermission(IMF_END, 0, 0);
    WI_unloadData();
}

void WI_initNoState(void)
{
    state = NoState;
    acceleratestage = 0;
    cnt = 10;

    NetSv_Intermission(IMF_STATE, state, 0);
}

void WI_updateNoState(void)
{

    WI_updateAnimatedBack();

    if(!--cnt)
    {
        if(IS_CLIENT)
            return;
        WI_End();
        G_WorldDone();
    }

}

void WI_initShowNextLoc(void)
{
    state = ShowNextLoc;
    acceleratestage = 0;
    cnt = SHOWNEXTLOCDELAY * TICRATE;

    WI_initAnimatedBack();

    NetSv_Intermission(IMF_STATE, state, 0);
}

void WI_updateShowNextLoc(void)
{
    WI_updateAnimatedBack();

    if(!--cnt || acceleratestage)
        WI_initNoState();
    else
        snl_pointeron = (cnt & 31) < 20;
}

void WI_drawShowNextLoc(void)
{
    int     i;
    int     last;

    WI_slamBackground();

    // draw animated background
    WI_drawAnimatedBack();

    if(gamemode != commercial)
    {
        if(wbs->epsd > 2)
        {
            WI_drawEL();
            return;
        }

        last = (wbs->last == 8) ? wbs->next - 1 : wbs->last;

        // draw a splat on taken cities.
        for(i = 0; i <= last; i++)
            WI_drawOnLnode(i, &splat);

        // splat the secret level?
        if(wbs->didsecret)
            WI_drawOnLnode(8, &splat);

        // draw flashing ptr
        if(snl_pointeron)
            WI_drawOnLnode(wbs->next, yah);
    }

    // draws which level you are entering..
    if((gamemode != commercial) || wbs->next != 30)
        WI_drawEL();
}

void WI_drawNoState(void)
{
    snl_pointeron = true;
    WI_drawShowNextLoc();
}

int WI_fragSum(int teamnum)
{
    /*    int       i;
       int      frags = 0;

       for(i=0; i<NUM_TEAMS; i++)
       {
       if(players[i].plr->ingame && i!=playernum)
       {
       frags += plrs[playernum].frags[i];
       }
       }

       // JDC hack - negative frags.
       frags -= plrs[playernum].frags[playernum];
       // UNUSED if (frags < 0)
       //   frags = 0;

       return frags;
     */
    return teaminfo[teamnum].totalfrags;
}

void WI_initDeathmatchStats(void)
{
    int     i;

    state = StatCount;
    acceleratestage = 0;
    dm_state = 1;

    cnt_pause = TICRATE;

    // Clear the on-screen counters.
    memset(dm_totals, 0, sizeof(dm_totals));
    for(i = 0; i < NUM_TEAMS; i++)
        memset(dm_frags[i], 0, sizeof(dm_frags[i]));

    WI_initAnimatedBack();
}

void WI_updateDeathmatchStats(void)
{

    int     i;
    int     j;
    boolean stillticking;

    WI_updateAnimatedBack();

    if(acceleratestage && dm_state != 4)
    {
        acceleratestage = 0;
        for(i = 0; i < NUM_TEAMS; i++)
        {
            //          if(teaminfo[i].members)
            //          {
            for(j = 0; j < NUM_TEAMS; j++)
            {
                //              if (players[i].plr->ingame)
                dm_frags[i][j] = teaminfo[i].frags[j];
            }
            dm_totals[i] = WI_fragSum(i);
            //          }
        }
        S_LocalSound(sfx_intcmp, 0);
        dm_state = 4;
    }

    if(dm_state == 2)
    {
        if(!(bcnt & 3))
            S_LocalSound(sfx_intcnt, 0);
        stillticking = false;
        for(i = 0; i < NUM_TEAMS; i++)
        {
            //          if (players[i].plr->ingame)
            {
                for(j = 0; j < NUM_TEAMS; j++)
                {
                    if( /*players[i].plr->ingame && */ dm_frags[i][j] !=
                       teaminfo[i].frags[j])
                    {
                        if(teaminfo[i].frags[j] < 0)
                            dm_frags[i][j]--;
                        else
                            dm_frags[i][j]++;

                        if(dm_frags[i][j] > 99)
                            dm_frags[i][j] = 99;

                        if(dm_frags[i][j] < -99)
                            dm_frags[i][j] = -99;

                        stillticking = true;
                    }
                }
                dm_totals[i] = WI_fragSum(i);

                if(dm_totals[i] > 99)
                    dm_totals[i] = 99;

                if(dm_totals[i] < -99)
                    dm_totals[i] = -99;
            }
        }
        if(!stillticking)
        {
            S_LocalSound(sfx_intcmp, 0);
            dm_state++;
        }
    }
    else if(dm_state == 4)
    {
        if(acceleratestage)
        {
            S_LocalSound(sfx_hitslp, 0);
            if(gamemode == commercial)
                WI_initNoState();
            else
                WI_initShowNextLoc();
        }
    }
    else if(dm_state & 1)
    {
        if(!--cnt_pause)
        {
            dm_state++;
            cnt_pause = TICRATE;
        }
    }
}

void WI_drawDeathmatchStats(void)
{
    int     i;
    int     j;
    int     x;
    int     y;
    int     w;

    int     lh;                 // line height

    lh = WI_SPACINGY;

    WI_slamBackground();

    // draw animated background
    WI_drawAnimatedBack();
    WI_drawLF();

    // draw stat titles (top line)
    WI_DrawPatch(DM_TOTALSX - total.width / 2,
                 DM_MATRIXY - WI_SPACINGY + 10, 1, 1, 1, 1, total.lump, NULL,
                 false, ALIGN_LEFT);

    WI_DrawPatch(DM_KILLERSX, DM_KILLERSY, 1, 1, 1, 1, killers.lump, NULL,
                 false, ALIGN_LEFT);
    WI_DrawPatch(DM_VICTIMSX, DM_VICTIMSY, 1, 1, 1, 1, victims.lump, NULL,
                 false, ALIGN_LEFT);

    // draw P?
    x = DM_MATRIXX + DM_SPACINGX;
    y = DM_MATRIXY;

    for(i = 0; i < NUM_TEAMS; i++)
    {
        if(teaminfo[i].members)
        {
            WI_DrawPatch(x - p[i].width / 2, DM_MATRIXY - WI_SPACINGY,
                         1, 1, 1, 1, p[i].lump, NULL, false, ALIGN_LEFT);

            WI_DrawPatch(DM_MATRIXX - p[i].width / 2, y, 1, 1, 1, 1,
                         p[i].lump, NULL, false, ALIGN_LEFT);

            if(i == myteam)
            {
                WI_DrawPatch(x - p[i].width / 2,
                             DM_MATRIXY - WI_SPACINGY, 1, 1, 1, 1, bstar.lump,
                             NULL, false, ALIGN_LEFT);

                WI_DrawPatch(DM_MATRIXX - p[i].width / 2, y, 1, 1, 1, 1,
                             star.lump, NULL, false, ALIGN_LEFT);
            }

            // If more than 1 member, show the member count.
            if(teaminfo[i].members > 1)
            {
                char    tmp[20];

                sprintf(tmp, "%i", teaminfo[i].members);
                M_WriteText2(x - p[i].width / 2 + 1,
                             DM_MATRIXY - WI_SPACINGY + p[i].height - 8, tmp,
                             hu_font_a, 1, 1, 1, 1);
                M_WriteText2(DM_MATRIXX - p[i].width / 2 + 1,
                             y + p[i].height - 8, tmp, hu_font_a, 1, 1, 1, 1);
            }
        }
        else
        {
            WI_DrawPatch(x - bp[i].width / 2, DM_MATRIXY - WI_SPACINGY, 1, 1, 1, 1,
                         bp[i].lump, NULL, false, ALIGN_LEFT);

            WI_DrawPatch(DM_MATRIXX - bp[i].width / 2, y, 1, 1, 1, 1,
                         bp[i].lump, NULL, false, ALIGN_LEFT);
        }
        x += DM_SPACINGX;
        y += WI_SPACINGY;
    }

    // draw stats
    y = DM_MATRIXY + 10;
    w = num[0].width;

    for(i = 0; i < NUM_TEAMS; i++)
    {
        x = DM_MATRIXX + DM_SPACINGX;
        if(teaminfo[i].members)
        {
            for(j = 0; j < NUM_TEAMS; j++)
            {
                if(teaminfo[j].members)
                    WI_drawNum(x + w, y, dm_frags[i][j], 2);
                x += DM_SPACINGX;
            }
            WI_drawNum(DM_TOTALSX + w, y, dm_totals[i], 2);
        }
        y += WI_SPACINGY;
    }
}

void WI_initNetgameStats(void)
{
    int     i;

    state = StatCount;
    acceleratestage = 0;
    ng_state = 1;
    cnt_pause = TICRATE;

    memset(cnt_kills, 0, sizeof(cnt_kills));
    memset(cnt_items, 0, sizeof(cnt_items));
    memset(cnt_secret, 0, sizeof(cnt_secret));
    memset(cnt_frags, 0, sizeof(cnt_frags));
    dofrags = 0;

    for(i = 0; i < NUM_TEAMS; i++)
    {
        /*if (!players[i].plr->ingame)
           continue;

           cnt_kills[i] = cnt_items[i] = cnt_secret[i] = cnt_frags[i] = 0; */
        dofrags += teaminfo[i].totalfrags;  //WI_fragSum(i);
    }
    dofrags = !!dofrags;

    WI_initAnimatedBack();
}

void WI_updateNetgameStats(void)
{
    int     i;
    int     fsum;
    boolean stillticking;

    WI_updateAnimatedBack();

    if(acceleratestage && ng_state != 10)
    {
        acceleratestage = 0;
        for(i = 0; i < NUM_TEAMS; i++)
        {
            //if (!players[i].plr->ingame) continue;
            cnt_kills[i] = (teaminfo[i].kills * 100) / wbs->maxkills;
            cnt_items[i] = (teaminfo[i].items * 100) / wbs->maxitems;
            cnt_secret[i] = (teaminfo[i].secret * 100) / wbs->maxsecret;

            if(dofrags)
                cnt_frags[i] = teaminfo[i].totalfrags;  //WI_fragSum(i);
        }
        S_LocalSound(sfx_intcmp, 0);
        ng_state = 10;
    }

    if(ng_state == 2)
    {
        if(!(bcnt & 3))
            S_LocalSound(sfx_intcnt, 0);
        stillticking = false;

        for(i = 0; i < NUM_TEAMS; i++)
        {
            //if (!players[i].plr->ingame) continue;

            cnt_kills[i] += 2;

            if(cnt_kills[i] >= (teaminfo[i].kills * 100) / wbs->maxkills)
                cnt_kills[i] = (teaminfo[i].kills * 100) / wbs->maxkills;
            else
                stillticking = true;
        }
        if(!stillticking)
        {
            S_LocalSound(sfx_intcmp, 0);
            ng_state++;
        }
    }
    else if(ng_state == 4)
    {
        if(!(bcnt & 3))
            S_LocalSound(sfx_intcnt, 0);
        stillticking = false;

        for(i = 0; i < NUM_TEAMS; i++)
        {
            //if (!players[i].plr->ingame) continue;

            cnt_items[i] += 2;
            if(cnt_items[i] >= (teaminfo[i].items * 100) / wbs->maxitems)
                cnt_items[i] = (teaminfo[i].items * 100) / wbs->maxitems;
            else
                stillticking = true;
        }
        if(!stillticking)
        {
            S_LocalSound(sfx_intcmp, 0);
            ng_state++;
        }
    }
    else if(ng_state == 6)
    {
        if(!(bcnt & 3))
            S_LocalSound(sfx_intcnt, 0);

        stillticking = false;

        for(i = 0; i < NUM_TEAMS; i++)
        {
            //if (!players[i].plr->ingame) continue;

            cnt_secret[i] += 2;

            if(cnt_secret[i] >= (teaminfo[i].secret * 100) / wbs->maxsecret)
                cnt_secret[i] = (teaminfo[i].secret * 100) / wbs->maxsecret;
            else
                stillticking = true;
        }
        if(!stillticking)
        {
            S_LocalSound(sfx_intcmp, 0);
            ng_state += 1 + 2 * !dofrags;
        }
    }
    else if(ng_state == 8)
    {
        if(!(bcnt & 3))
            S_LocalSound(sfx_intcnt, 0);

        stillticking = false;

        for(i = 0; i < NUM_TEAMS; i++)
        {
            //if (!players[i].plr->ingame) continue;

            cnt_frags[i] += 1;

            if(cnt_frags[i] >= (fsum = WI_fragSum(i)))
                cnt_frags[i] = fsum;
            else
                stillticking = true;
        }
        if(!stillticking)
        {
            S_LocalSound(sfx_plydth, 0);
            ng_state++;
        }
    }
    else if(ng_state == 10)
    {
        if(acceleratestage)
        {
            S_LocalSound(sfx_intyea, 0);
            if(gamemode == commercial)
                WI_initNoState();
            else
                WI_initShowNextLoc();
        }
    }
    else if(ng_state & 1)
    {
        if(!--cnt_pause)
        {
            ng_state++;
            cnt_pause = TICRATE;
        }
    }
}

void WI_drawNetgameStats(void)
{
    int     i;
    int     x;
    int     y;
    int     pwidth = percent.width;

    WI_slamBackground();

    // draw animated background
    WI_drawAnimatedBack();

    WI_drawLF();

    // draw stat titles (top line)
    WI_DrawPatch(NG_STATSX + NG_SPACINGX - kills.width, NG_STATSY,
                 1, 1, 1, 1, kills.lump, NULL, false, ALIGN_LEFT);

    WI_DrawPatch(NG_STATSX + 2 * NG_SPACINGX - items.width, NG_STATSY,
                 1, 1, 1, 1, items.lump, NULL, false, ALIGN_LEFT);

    WI_DrawPatch(NG_STATSX + 3 * NG_SPACINGX - secret.width, NG_STATSY,
                 1, 1, 1, 1, secret.lump, NULL, false, ALIGN_LEFT);

    if(dofrags)
        WI_DrawPatch(NG_STATSX + 4 * NG_SPACINGX - frags.width,
                     1, 1, 1, 1, NG_STATSY, frags.lump, NULL, false, ALIGN_LEFT);

    // draw stats
    y = NG_STATSY + kills.height;

    for(i = 0; i < NUM_TEAMS; i++)
    {
        if(!teaminfo[i].members)
            continue;

        x = NG_STATSX;
        WI_DrawPatch(x - p[i].width, y, 1, 1, 1, 1, p[i].lump, NULL,
                     false, ALIGN_LEFT);
        // If more than 1 member, show the member count.
        if(teaminfo[i].members > 1)
        {
            char    tmp[40];

            sprintf(tmp, "%i", teaminfo[i].members);
            M_WriteText2(x - p[i].width + 1, y + p[i].height - 8, tmp,
                         hu_font_a, 1, 1, 1, 1);
        }

        if(i == myteam)
            WI_DrawPatch(x - p[i].width, y, 1, 1, 1, 1, star.lump, NULL,
                         false, ALIGN_LEFT);

        x += NG_SPACINGX;
        WI_drawPercent(x - pwidth, y + 10, cnt_kills[i]);
        x += NG_SPACINGX;
        WI_drawPercent(x - pwidth, y + 10, cnt_items[i]);
        x += NG_SPACINGX;
        WI_drawPercent(x - pwidth, y + 10, cnt_secret[i]);
        x += NG_SPACINGX;

        if(dofrags)
            WI_drawNum(x, y + 10, cnt_frags[i], -1);

        y += WI_SPACINGY;
    }
}

void WI_initStats(void)
{
    state = StatCount;
    acceleratestage = 0;
    sp_state = 1;
    cnt_kills[0] = cnt_items[0] = cnt_secret[0] = -1;
    cnt_time = cnt_par = -1;
    cnt_pause = TICRATE;
    WI_initAnimatedBack();

    //NetSv_Intermission(IMF_STATE, state, 0);
}

void WI_updateStats(void)
{
    WI_updateAnimatedBack();

    if(acceleratestage && sp_state != 10)
    {
        acceleratestage = 0;
        cnt_kills[0] = (plrs[me].skills * 100) / wbs->maxkills;
        cnt_items[0] = (plrs[me].sitems * 100) / wbs->maxitems;
        cnt_secret[0] = (plrs[me].ssecret * 100) / wbs->maxsecret;
        cnt_time = plrs[me].stime / TICRATE;
        if(wbs->partime != -1)
            cnt_par = wbs->partime / TICRATE;
        S_LocalSound(sfx_intcmp, 0);
        sp_state = 10;
    }

    if(sp_state == 2)
    {
        cnt_kills[0] += 2;

        if(!(bcnt & 3))
            S_LocalSound(sfx_intcnt, 0);

        if(cnt_kills[0] >= (plrs[me].skills * 100) / wbs->maxkills)
        {
            cnt_kills[0] = (plrs[me].skills * 100) / wbs->maxkills;
            S_LocalSound(sfx_intcmp, 0);
            sp_state++;
        }
    }
    else if(sp_state == 4)
    {
        cnt_items[0] += 2;

        if(!(bcnt & 3))
            S_LocalSound(sfx_intcnt, 0);

        if(cnt_items[0] >= (plrs[me].sitems * 100) / wbs->maxitems)
        {
            cnt_items[0] = (plrs[me].sitems * 100) / wbs->maxitems;
            S_LocalSound(sfx_intcmp, 0);
            sp_state++;
        }
    }
    else if(sp_state == 6)
    {
        cnt_secret[0] += 2;

        if(!(bcnt & 3))
            S_LocalSound(sfx_intcnt, 0);

        if(cnt_secret[0] >= (plrs[me].ssecret * 100) / wbs->maxsecret)
        {
            cnt_secret[0] = (plrs[me].ssecret * 100) / wbs->maxsecret;
            S_LocalSound(sfx_intcmp, 0);
            sp_state++;
        }
    }

    else if(sp_state == 8)
    {
        if(!(bcnt & 3))
            S_LocalSound(sfx_intcnt, 0);

        cnt_time += 3;

        if(cnt_time >= plrs[me].stime / TICRATE)
            cnt_time = plrs[me].stime / TICRATE;

        if(cnt_par != -1)
        {
            cnt_par += 3;

            if(cnt_par >= wbs->partime / TICRATE)
            {
                cnt_par = wbs->partime / TICRATE;

                if(cnt_time >= plrs[me].stime / TICRATE)
                {
                    S_LocalSound(sfx_intcmp, 0);
                    sp_state++;
                }
            }
        }
        else
            sp_state++;
    }
    else if(sp_state == 10)
    {
        if(acceleratestage)
        {
            S_LocalSound(sfx_intyea, 0);

            if(gamemode == commercial)
                WI_initNoState();
            else
                WI_initShowNextLoc();
        }
    }
    else if(sp_state & 1)
    {
        if(!--cnt_pause)
        {
            sp_state++;
            cnt_pause = TICRATE;
        }
    }
}

void WI_drawStats(void)
{
    // line height
    int     lh;

    lh = (3 * num[0].height) / 2;

    WI_slamBackground();

    // draw animated background
    WI_drawAnimatedBack();

    WI_drawLF();

    WI_DrawPatch(SP_STATSX, SP_STATSY, 1, 1, 1, 1, kills.lump, NULL,
                 false, ALIGN_LEFT);
    WI_drawPercent(SCREENWIDTH - SP_STATSX, SP_STATSY, cnt_kills[0]);

    WI_DrawPatch(SP_STATSX, SP_STATSY + lh, 1, 1, 1, 1, items.lump, NULL,
                 false, ALIGN_LEFT);
    WI_drawPercent(SCREENWIDTH - SP_STATSX, SP_STATSY + lh, cnt_items[0]);

    WI_DrawPatch(SP_STATSX, SP_STATSY + 2 * lh, 1, 1, 1, 1, sp_secret.lump,
                 NULL, false, ALIGN_LEFT);
    WI_drawPercent(SCREENWIDTH - SP_STATSX, SP_STATSY + 2 * lh, cnt_secret[0]);

    WI_DrawPatch(SP_TIMEX, SP_TIMEY, 1, 1, 1, 1, time.lump, NULL, false,
                 ALIGN_LEFT);
    WI_drawTime(SCREENWIDTH / 2 - SP_TIMEX, SP_TIMEY, cnt_time);

    if(wbs->epsd < 3 && wbs->partime != -1)
    {
        WI_DrawPatch(SCREENWIDTH / 2 + SP_TIMEX, SP_TIMEY, 1, 1, 1, 1, par.lump,
                     NULL, false, ALIGN_LEFT);
        WI_drawTime(SCREENWIDTH - SP_TIMEX, SP_TIMEY, cnt_par);
    }

}

void WI_checkForAccelerate(void)
{
    int     i;
    player_t *player;

    // check for button presses to skip delays
    for(i = 0, player = players; i < MAXPLAYERS; i++, player++)
    {
        if(players[i].plr->ingame)
        {
            if(player->plr->cmd.actions & BT_ATTACK)
            {
                if(!player->attackdown)
                    acceleratestage = 1;
                player->attackdown = true;
            }
            else
                player->attackdown = false;
            if(player->plr->cmd.actions & BT_USE)
            {
                if(!player->usedown)
                    acceleratestage = 1;
                player->usedown = true;
            }
            else
                player->usedown = false;
        }
    }
}

// Updates stuff each tick
void WI_Ticker(void)
{
    // counter for general background animation
    bcnt++;

    if(bcnt == 1)
    {
        // intermission music
        if(gamemode == commercial)
            S_StartMusicNum(mus_wlfint, true);
        else
            S_StartMusicNum(mus_inter, true);
    }

    WI_checkForAccelerate();

    switch (state)
    {
    case StatCount:
        if(deathmatch)
            WI_updateDeathmatchStats();
        else if(IS_NETGAME)
            WI_updateNetgameStats();
        else
            WI_updateStats();
        break;

    case ShowNextLoc:
        WI_updateShowNextLoc();
        break;

    case NoState:
        WI_updateNoState();
        break;
    }

}

void WI_loadData(void)
{
    int     i;
    int     j;
    char    name[9];
    wianim_t *a;

    if(gamemode == commercial)
        strcpy(name, "INTERPIC");
    else
        sprintf(name, "WIMAP%d", wbs->epsd);

    if(gamemode == retail)
    {
        if(wbs->epsd == 3)
            strcpy(name, "INTERPIC");
    }

    if(!Get(DD_NOVIDEO))
    {
        // background
        R_CachePatch(&bg, name);
        GL_DrawPatch(0, 0, bg.lump);
    }

    if(gamemode != commercial)
    {
        // you are here
        R_CachePatch(&yah[0], "WIURH0");

        // you are here (alt.)
        R_CachePatch(&yah[1], "WIURH1");

        // splat
        R_CachePatch(&splat, "WISPLAT");

        if(wbs->epsd < 3)
        {
            for(j = 0; j < NUMANIMS[wbs->epsd]; j++)
            {
                a = &anims[wbs->epsd][j];
                for(i = 0; i < a->nanims; i++)
                {
                    // MONDO HACK!
                    if(wbs->epsd != 1 || j != 8)
                    {
                        // animations
                        sprintf(name, "WIA%d%.2d%.2d", wbs->epsd, j, i);
                        R_CachePatch(&a->p[i], name);
                    }
                    else
                    {
                        // HACK ALERT!
                        memcpy(&a->p[i], &anims[1][4].p[i], sizeof(dpatch_t));
                    }
                }
            }
        }
    }

    // More hacks on minus sign.
    R_CachePatch(&wiminus, "WIMINUS");

    for(i = 0; i < 10; i++)
    {
        // numbers 0-9
        sprintf(name, "WINUM%d", i);
        //num[i] = W_CacheLumpName(name, PU_STATIC);
        R_CachePatch(&num[i], name);
    }

    // percent sign
    R_CachePatch(&percent, "WIPCNT");

    // "finished"
    R_CachePatch(&finished, "WIF");

    // "entering"
    R_CachePatch(&entering, "WIENTER");

    // "kills"
    R_CachePatch(&kills, "WIOSTK");

    // "scrt"
    R_CachePatch(&secret, "WIOSTS");

    // "secret"
    R_CachePatch(&sp_secret, "WISCRT2");

    //items
    R_CachePatch(&items, "WIOSTI");

    // "frgs"
    R_CachePatch(&frags, "WIFRGS");

    // ":"
    R_CachePatch(&colon, "WICOLON");

    // "time"
    R_CachePatch(&time, "WITIME");

    // "sucks"
    R_CachePatch(&sucks, "WISUCKS");

    // "par"
    R_CachePatch(&par, "WIPAR");

    // "killers" (vertical)
    R_CachePatch(&killers, "WIKILRS");

    // "victims" (horiz)
    R_CachePatch(&victims, "WIVCTMS");

    // "total"
    R_CachePatch(&total, "WIMSTT");

    // your face
    R_CachePatch(&star, "STFST01");

    // dead face
    R_CachePatch(&bstar, "STFDEAD0");

    for(i = 0; i < MAXPLAYERS; i++)
    {
        // "1,2,3,4"
        sprintf(name, "STPB%d", i);
        R_CachePatch(&p[i], name);

        // "1,2,3,4"
        sprintf(name, "WIBP%d", i + 1);
        R_CachePatch(&bp[i], name);
    }

}

void WI_unloadData(void)
{
#if 0
    int     i;
    int     j;

    Z_ChangeTag(wiminus.patch, PU_CACHE);

    for(i = 0; i < 10; i++)
        Z_ChangeTag(num[i].patch, PU_CACHE);

    if(gamemode == commercial)
    {
        for(i = 0; i < NUMCMAPS; i++)
            Z_ChangeTag(lnames[i].patch, PU_CACHE);
    }
    else
    {
        Z_ChangeTag(yah[0].patch, PU_CACHE);
        Z_ChangeTag(yah[1].patch, PU_CACHE);

        Z_ChangeTag(splat.patch, PU_CACHE);

        for(i = 0; i < NUMMAPS; i++)
            Z_ChangeTag(lnames[i].patch, PU_CACHE);

        if(wbs->epsd < 3)
        {
            for(j = 0; j < NUMANIMS[wbs->epsd]; j++)
            {
                if(wbs->epsd != 1 || j != 8)
                    for(i = 0; i < anims[wbs->epsd][j].nanims; i++)
                        Z_ChangeTag(anims[wbs->epsd][j].p[i].patch, PU_CACHE);
            }
        }
    }
#endif

#if 0
    Z_ChangeTag(percent.patch, PU_CACHE);
    Z_ChangeTag(colon.patch, PU_CACHE);
    Z_ChangeTag(finished.patch, PU_CACHE);
    Z_ChangeTag(entering.patch, PU_CACHE);
    Z_ChangeTag(kills.patch, PU_CACHE);
    Z_ChangeTag(secret.patch, PU_CACHE);
    Z_ChangeTag(sp_secret.patch, PU_CACHE);
    Z_ChangeTag(items.patch, PU_CACHE);
    Z_ChangeTag(frags.patch, PU_CACHE);
    Z_ChangeTag(time.patch, PU_CACHE);
    Z_ChangeTag(sucks.patch, PU_CACHE);
    Z_ChangeTag(par.patch, PU_CACHE);

    Z_ChangeTag(victims.patch, PU_CACHE);
    Z_ChangeTag(killers.patch, PU_CACHE);
    Z_ChangeTag(total.patch, PU_CACHE);
    //  Z_ChangeTag(star, PU_CACHE);
    //  Z_ChangeTag(bstar, PU_CACHE);

    for(i = 0; i < MAXPLAYERS; i++)
        Z_ChangeTag(p[i].patch, PU_CACHE);

    for(i = 0; i < MAXPLAYERS; i++)
        Z_ChangeTag(bp[i].patch, PU_CACHE);
#endif
}

void WI_Drawer(void)
{
    switch (state)
    {
    case StatCount:
        if(deathmatch)
            WI_drawDeathmatchStats();
        else if(IS_NETGAME)
            WI_drawNetgameStats();
        else
            WI_drawStats();
        break;

    case ShowNextLoc:
        WI_drawShowNextLoc();
        break;

    case NoState:
        WI_drawNoState();
        break;
    }
}

void WI_initVariables(wbstartstruct_t * wbstartstruct)
{
    wbs = wbstartstruct;

#ifdef RANGECHECK
    if(gamemode != commercial)
    {
        if(gamemode == retail)
            RNGCHECK(wbs->epsd, 0, 3);
        else
            RNGCHECK(wbs->epsd, 0, 2);
    }
    else
    {
        RNGCHECK(wbs->last, 0, 8);
        RNGCHECK(wbs->next, 0, 8);
    }
    RNGCHECK(wbs->pnum, 0, MAXPLAYERS);
    RNGCHECK(wbs->pnum, 0, MAXPLAYERS);
#endif

    acceleratestage = 0;
    cnt = bcnt = 0;
    firstrefresh = 1;
    me = wbs->pnum;
    myteam = cfg.playerColor[wbs->pnum];
    plrs = wbs->plyr;

    if(!wbs->maxkills)
        wbs->maxkills = 1;
    if(!wbs->maxitems)
        wbs->maxitems = 1;
    if(!wbs->maxsecret)
        wbs->maxsecret = 1;

    if(gamemode != retail)
        if(wbs->epsd > 2)
            wbs->epsd -= 3;
}

void WI_Start(wbstartstruct_t * wbstartstruct)
{
    teaminfo_t *tin;
    int     i, j, k;

    GL_SetFilter(0);
    WI_initVariables(wbstartstruct);
    WI_loadData();

    // Calculate team stats.
    memset(teaminfo, 0, sizeof(teaminfo));
    for(i = 0, tin = teaminfo; i < NUM_TEAMS; i++, tin++)
    {
        for(j = 0; j < MAXPLAYERS; j++)
        {
            // Is the player in this team?
            if(!plrs[j].in || cfg.playerColor[j] != i)
                continue;
            tin->members++;
            // Check the frags.
            for(k = 0; k < MAXPLAYERS; k++)
                tin->frags[cfg.playerColor[k]] += plrs[j].frags[k];
            // Counters.
            if(plrs[j].sitems > tin->items)
                tin->items = plrs[j].sitems;
            if(plrs[j].skills > tin->kills)
                tin->kills = plrs[j].skills;
            if(plrs[j].ssecret > tin->secret)
                tin->secret = plrs[j].ssecret;
        }
        // Calculate team's total frags.
        for(j = 0; j < NUM_TEAMS; j++)
        {
            if(j == i)          // Suicides are negative frags.
                tin->totalfrags -= tin->frags[j];
            else
                tin->totalfrags += tin->frags[j];
        }
    }

    if(deathmatch)
        WI_initDeathmatchStats();
    else if(IS_NETGAME)
        WI_initNetgameStats();
    else
        WI_initStats();
}

void WI_SetState(stateenum_t st)
{
    if(st == StatCount)
        WI_initStats();
    if(st == ShowNextLoc)
        WI_initShowNextLoc();
    if(st == NoState)
        WI_initNoState();
}
