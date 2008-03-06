/**\file
 *\section License
 * License: Raven
 * Online License Link: http://www.dengine.net/raven_license/End_User_License_Hexen_Source_Code.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
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

// invslot artifact count (relative to each slot)
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

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void ST_drawWidgets(boolean refresh);

// Console commands for the HUD/Statusbar
DEFCC(CCmdHUDShow);
DEFCC(CCmdStatusBarSize);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void DrINumber(signed int val, int x, int y, float r, float g,
                      float b, float a);
static void DrBNumber(signed int val, int x, int y, float Red, float Green,
                      float Blue, float Alpha);
static void DrawChain(void);
static void DrawKeyBar(void);
static void DrawWeaponPieces(void);
static void ST_doFullscreenStuff(void);
static void DrawAnimatedIcons(void);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern boolean huShowAllFrags; // in hu_stuff.c currently

// PUBLIC DATA DECLARATIONS ------------------------------------------------

int inventoryTics;
boolean inventory = false;

int SB_state = -1;

int lu_palette;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int hudHideTics;
static float hudHideAmount;

// whether to use alpha blending
static boolean st_blended = false;

static int HealthMarker;

static int FontBNumBase;

static int oldarti = 0;
static int oldartiCount = 0;

static int oldhealth = -1;

static int ArtifactFlash;

// ST_Start() has just been called
static boolean st_firsttime;

// fullscreen hud alpha value
static float hudalpha = 0.0f;

static float statusbarCounterAlpha = 0.0f;

// slide statusbar amount 1.0 is fully open
static float showbar = 0.0f;

// whether left-side main status bar is active
static boolean st_statusbaron;

// used for timing
static unsigned int st_clock;

// used when in chat
static st_chatstateenum_t st_chatstate;

// whether in automap or first-person
static st_stateenum_t st_gamestate;

// whether status bar chat is active
static boolean st_chat;

// value of st_chat before message popped up
static boolean st_oldchat;

// whether chat window has the cursor on
static boolean st_cursoron;

// current inventory slot indices. 0 = none
static int st_invslot[NUMVISINVSLOTS];

// current inventory slot count indices. 0 = none
static int st_invslotcount[NUMVISINVSLOTS];

// current armor level
static int armorlevel = 0;

// current artifact index. 0 = none
static int st_artici = 0;

// current artifact widget
static st_multicon_t w_artici;

// current artifact count widget
static st_number_t w_articount;

// inventory slot widgets
static st_multicon_t w_invslot[NUMVISINVSLOTS];

// inventory slot count widgets
static st_number_t w_invslotcount[NUMVISINVSLOTS];

// current mana A icon index. 0 = none
static int st_manaAicon = 0;

// current mana B icon index. 0 = none
static int st_manaBicon = 0;

// current mana A vial index. 0 = none
static int st_manaAvial = 0;

// current mana B vial index. 0 = none
static int st_manaBvial = 0;

// current mana A icon widget
static st_multicon_t w_manaAicon;

// current mana B icon widget
static st_multicon_t w_manaBicon;

// current mana A vial widget
static st_multicon_t w_manaAvial;

// current mana B vial widget
static st_multicon_t w_manaBvial;

// health widget
static st_number_t w_health;

// in deathmatch only, summary of frags stats
static st_number_t w_frags;

// armor widget
static st_number_t w_armor;

// current mana level
static int manaACount = 0;
static int manaBCount = 0;

// mana A counter widget
static st_number_t w_manaACount;

// mana B widget
static st_number_t w_manaBCount;

// number of frags so far in deathmatch
static int st_fragscount;

// !deathmatch
static boolean st_fragson;

static dpatch_t PatchNumH2BAR;
static dpatch_t PatchNumH2TOP;
static dpatch_t PatchNumLFEDGE;
static dpatch_t PatchNumRTEDGE;
static dpatch_t PatchNumKILLS;
static dpatch_t PatchNumCHAIN;
static dpatch_t PatchNumSTATBAR;
static dpatch_t PatchNumKEYBAR;
static dpatch_t PatchNumSELECTBOX;
static dpatch_t PatchNumINumbers[10];
static dpatch_t PatchNumNEGATIVE;
static dpatch_t PatchNumSmNumbers[10];
static dpatch_t PatchMANAAVIALS[2];
static dpatch_t PatchMANABVIALS[2];
static dpatch_t PatchMANAAICONS[2];
static dpatch_t PatchMANABICONS[2];
static dpatch_t PatchNumINVBAR;
static dpatch_t PatchNumWEAPONSLOT;
static dpatch_t PatchNumWEAPONFULL;
static dpatch_t PatchNumPIECE1;
static dpatch_t PatchNumPIECE2;
static dpatch_t PatchNumPIECE3;
static dpatch_t PatchNumINVLFGEM1;
static dpatch_t PatchNumINVLFGEM2;
static dpatch_t PatchNumINVRTGEM1;
static dpatch_t PatchNumINVRTGEM2;

static dpatch_t     PatchARTIFACTS[38];

static dpatch_t SpinFlylump;
static dpatch_t SpinMinotaurLump;
static dpatch_t SpinSpeedLump;
static dpatch_t SpinDefenseLump;

static int PatchNumLIFEGEM;

char    artifactlist[][10] = {
    {"USEARTIA"},               // use artifact flash
    {"USEARTIB"},
    {"USEARTIC"},
    {"USEARTID"},
    {"USEARTIE"},
    {"ARTIBOX"},                // none
    {"ARTIINVU"},               // invulnerability
    {"ARTIPTN2"},               // health
    {"ARTISPHL"},               // superhealth
    {"ARTIHRAD"},               // healing radius
    {"ARTISUMN"},               // summon maulator
    {"ARTITRCH"},               // torch
    {"ARTIPORK"},               // egg
    {"ARTISOAR"},               // fly
    {"ARTIBLST"},               // blast radius
    {"ARTIPSBG"},               // poison bag
    {"ARTITELO"},               // teleport other
    {"ARTISPED"},               // speed
    {"ARTIBMAN"},               // boost mana
    {"ARTIBRAC"},               // boost armor
    {"ARTIATLP"},               // teleport
    {"ARTISKLL"},               // arti_puzzskull
    {"ARTIBGEM"},               // arti_puzzgembig
    {"ARTIGEMR"},               // arti_puzzgemred
    {"ARTIGEMG"},               // arti_puzzgemgreen1
    {"ARTIGMG2"},               // arti_puzzgemgreen2
    {"ARTIGEMB"},               // arti_puzzgemblue1
    {"ARTIGMB2"},               // arti_puzzgemblue2
    {"ARTIBOK1"},               // arti_puzzbook1
    {"ARTIBOK2"},               // arti_puzzbook2
    {"ARTISKL2"},               // arti_puzzskull2
    {"ARTIFWEP"},               // arti_puzzfweapon
    {"ARTICWEP"},               // arti_puzzcweapon
    {"ARTIMWEP"},               // arti_puzzmweapon
    {"ARTIGEAR"},               // arti_puzzgear1
    {"ARTIGER2"},               // arti_puzzgear2
    {"ARTIGER3"},               // arti_puzzgear3
    {"ARTIGER4"},               // arti_puzzgear4
};

// CVARs for the HUD/Statusbar
cvar_t sthudCVars[] =
{
    // HUD scale
    {"hud-scale", 0, CVT_FLOAT, &cfg.hudScale, 0.1f, 10},

    {"hud-status-size", CVF_PROTECTED, CVT_INT, &cfg.statusbarScale, 1, 20},

    // HUD colour + alpha
    {"hud-color-r", 0, CVT_FLOAT, &cfg.hudColor[0], 0, 1},
    {"hud-color-g", 0, CVT_FLOAT, &cfg.hudColor[1], 0, 1},
    {"hud-color-b", 0, CVT_FLOAT, &cfg.hudColor[2], 0, 1},
    {"hud-color-a", 0, CVT_FLOAT, &cfg.hudColor[3], 0, 1},
    {"hud-icon-alpha", 0, CVT_FLOAT, &cfg.hudIconAlpha, 0, 1},

    {"hud-status-alpha", 0, CVT_FLOAT, &cfg.statusbarAlpha, 0, 1},
    {"hud-status-icon-a", 0, CVT_FLOAT, &cfg.statusbarCounterAlpha, 0, 1},

    // HUD icons
    {"hud-mana", 0, CVT_BYTE, &cfg.hudShown[HUD_MANA], 0, 2},
    {"hud-health", 0, CVT_BYTE, &cfg.hudShown[HUD_HEALTH], 0, 1},
    {"hud-artifact", 0, CVT_BYTE, &cfg.hudShown[HUD_ARTI], 0, 1},

    // HUD displays
    {"hud-inventory-timer", 0, CVT_FLOAT, &cfg.inventoryTimer, 0, 30},

    {"hud-frags-all", 0, CVT_BYTE, &huShowAllFrags, 0, 1},

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
ccmd_t  sthudCCmds[] = {
    {"sbsize",      "s",    CCmdStatusBarSize},
    {"showhud",     "",     CCmdHUDShow},
    {NULL}
};

// CODE --------------------------------------------------------------------

/**
 * Register CVARs and CCmds for the HUD/Status bar
 */
void ST_Register(void)
{
    int         i;

    for(i = 0; sthudCVars[i].name; ++i)
        Con_AddVariable(sthudCVars + i);
    for(i = 0; sthudCCmds[i].name; ++i)
        Con_AddCommand(sthudCCmds + i);
}

void ST_loadGraphics(void)
{
    int         i;
    char        namebuf[9];

    FontBNumBase = W_GetNumForName("FONTB16");  // to be removed

    R_CachePatch(&PatchNumH2BAR, "H2BAR");
    R_CachePatch(&PatchNumH2TOP, "H2TOP");
    R_CachePatch(&PatchNumINVBAR, "INVBAR");
    R_CachePatch(&PatchNumLFEDGE, "LFEDGE");
    R_CachePatch(&PatchNumRTEDGE, "RTEDGE");
    R_CachePatch(&PatchNumSTATBAR, "STATBAR");
    R_CachePatch(&PatchNumKEYBAR, "KEYBAR");
    R_CachePatch(&PatchNumSELECTBOX, "SELECTBOX");

    R_CachePatch(&PatchMANAAVIALS[0], "MANAVL1D");
    R_CachePatch(&PatchMANABVIALS[0], "MANAVL2D");
    R_CachePatch(&PatchMANAAVIALS[1], "MANAVL1");
    R_CachePatch(&PatchMANABVIALS[1], "MANAVL2");

    R_CachePatch(&PatchMANAAICONS[0], "MANADIM1");
    R_CachePatch(&PatchMANABICONS[0], "MANADIM2");
    R_CachePatch(&PatchMANAAICONS[1], "MANABRT1");
    R_CachePatch(&PatchMANABICONS[1], "MANABRT2");

    R_CachePatch(&PatchNumINVLFGEM1, "invgeml1");
    R_CachePatch(&PatchNumINVLFGEM2, "invgeml2");
    R_CachePatch(&PatchNumINVRTGEM1, "invgemr1");
    R_CachePatch(&PatchNumINVRTGEM2, "invgemr2");
    R_CachePatch(&PatchNumNEGATIVE, "NEGNUM");
    R_CachePatch(&PatchNumKILLS, "KILLS");
    R_CachePatch(&SpinFlylump, "SPFLY0");
    R_CachePatch(&SpinMinotaurLump, "SPMINO0");
    R_CachePatch(&SpinSpeedLump, "SPBOOT0");
    R_CachePatch(&SpinDefenseLump, "SPSHLD0");

    for(i = 0; i < 10; ++i)
    {
        sprintf(namebuf, "IN%d", i);
        R_CachePatch(&PatchNumINumbers[i], namebuf);
    }

    for(i = 0; i < 10; ++i)
    {
        sprintf(namebuf, "SMALLIN%d", i);
        R_CachePatch(&PatchNumSmNumbers[i], namebuf);
    }

    // artifact icons (+5 for the use-artifact flash patches)
    for(i = 0; i < (NUMARTIFACTS + 5); ++i)
    {
        sprintf(namebuf, "%s", artifactlist[i]);
        R_CachePatch(&PatchARTIFACTS[i], namebuf);
    }

}

void ST_loadData(void)
{
    lu_palette = W_GetNumForName("PLAYPAL");

    SB_SetClassData();

    ST_loadGraphics();
}

void ST_initData(void)
{
    int         i;

    st_firsttime = true;

    st_clock = 0;
    st_chatstate = StartChatState;
    st_gamestate = FirstPersonState;

    st_statusbaron = true;
    st_oldchat = st_chat = false;
    st_cursoron = false;

    for(i = 0; i < NUMVISINVSLOTS; ++i)
    {
        st_invslot[i] = 0;
        st_invslotcount[i] = 0;
    }

    STlib_init();
    ST_HUDUnHide(HUE_FORCE);
}

void ST_createWidgets(void)
{
    int         i, width, temp;
    player_t   *plyr = &players[CONSOLEPLAYER];

    // health num
    STlib_initNum(&w_health, ST_HEALTHX, ST_HEALTHY, PatchNumINumbers,
                  &plyr->health, &st_statusbaron, ST_HEALTHWIDTH,
                  &statusbarCounterAlpha);

    // frags sum
    STlib_initNum(&w_frags, ST_FRAGSX, ST_FRAGSY, PatchNumINumbers, &st_fragscount,
                  &st_fragson, ST_FRAGSWIDTH, &statusbarCounterAlpha);

    // armor num - should be colored later
    STlib_initNum(&w_armor, ST_ARMORX, ST_ARMORY, PatchNumINumbers,
                  &armorlevel, &st_statusbaron, ST_ARMORWIDTH,
                  &statusbarCounterAlpha );

    // manaA count
    STlib_initNum(&w_manaACount, ST_MANAAX, ST_MANAAY, PatchNumSmNumbers,
                  &manaACount, &st_statusbaron, ST_MANAAWIDTH,
                  &statusbarCounterAlpha);

    // manaB count
    STlib_initNum(&w_manaBCount, ST_MANABX, ST_MANABY, PatchNumSmNumbers,
                  &manaBCount, &st_statusbaron, ST_MANABWIDTH,
                  &statusbarCounterAlpha);

    // current mana A icon
    STlib_initMultIcon(&w_manaAicon, ST_MANAAICONX, ST_MANAAICONY, PatchMANAAICONS,
                       &st_manaAicon, &st_statusbaron, &statusbarCounterAlpha);

    // current mana B icon
    STlib_initMultIcon(&w_manaBicon, ST_MANABICONX, ST_MANABICONY, PatchMANABICONS,
                       &st_manaBicon, &st_statusbaron, &statusbarCounterAlpha);

    // current mana A vial
    STlib_initMultIcon(&w_manaAvial, ST_MANAAVIALX, ST_MANAAVIALY, PatchMANAAVIALS,
                       &st_manaAvial, &st_statusbaron, &statusbarCounterAlpha);

    // current mana B vial
    STlib_initMultIcon(&w_manaBvial, ST_MANABVIALX, ST_MANABVIALY, PatchMANABVIALS,
                       &st_manaBvial, &st_statusbaron, &statusbarCounterAlpha);

    // current artifact (stbar not inventory)
    STlib_initMultIcon(&w_artici, ST_ARTIFACTX, ST_ARTIFACTY, PatchARTIFACTS,
                       &st_artici, &st_statusbaron, &statusbarCounterAlpha);

    // current artifact count
    STlib_initNum(&w_articount, ST_ARTIFACTCX, ST_ARTIFACTCY, PatchNumSmNumbers,
                  &oldartiCount, &st_statusbaron, ST_ARTIFACTCWIDTH,
                  &statusbarCounterAlpha);

    // inventory slots
    width = PatchARTIFACTS[5].width + 1;
    temp = 0;

    for(i = 0; i < NUMVISINVSLOTS; ++i)
    {
        // inventory slot icon
        STlib_initMultIcon(&w_invslot[i], ST_INVENTORYX + temp , ST_INVENTORYY,
                           PatchARTIFACTS, &st_invslot[i],
                           &st_statusbaron, &statusbarCounterAlpha);

        // inventory slot counter
        STlib_initNum(&w_invslotcount[i], ST_INVENTORYX + temp + ST_INVCOUNTOFFX,
                      ST_INVENTORYY + ST_INVCOUNTOFFY, PatchNumSmNumbers,
                      &st_invslotcount[i], &st_statusbaron, ST_ARTIFACTCWIDTH,
                      &statusbarCounterAlpha);

        temp += width;
    }
}

static boolean st_stopped = true;

void ST_Start(void)
{

    if(!st_stopped)
        ST_Stop();

    ST_initData();
    ST_createWidgets();
    st_stopped = false;
}

void ST_Stop(void)
{
    if(st_stopped)
        return;
    st_stopped = true;
}

void ST_Init(void)
{
    ST_loadData();
}

void ST_Inventory(boolean show)
{
    if(show)
    {
        inventory = true;

        inventoryTics = (int) (cfg.inventoryTimer * TICSPERSEC);
        if(inventoryTics < 1)
            inventoryTics = 1;

        ST_HUDUnHide(HUE_FORCE);
    }
    else
        inventory = false;
}

boolean ST_IsInventoryVisible(void)
{
    return inventory;
}

void ST_InventoryFlashCurrent(player_t *player)
{
    if(player == &players[CONSOLEPLAYER])
        ArtifactFlash = 4;
}

void SB_SetClassData(void)
{
    int         class;
    char        namebuf[9];

    class = cfg.playerClass[CONSOLEPLAYER]; // original player class (not pig)

    sprintf(namebuf, "wpslot%d", 0 + class);
    R_CachePatch(&PatchNumWEAPONSLOT, namebuf);

    sprintf(namebuf, "wpfull%d", 0 + class);
    R_CachePatch(&PatchNumWEAPONFULL, namebuf);

    switch(class)
    {
    case 0: // fighter
        R_CachePatch(&PatchNumPIECE1, "wpiecef1");
        R_CachePatch(&PatchNumPIECE2, "wpiecef2");
        R_CachePatch(&PatchNumPIECE3, "wpiecef3");
        R_CachePatch(&PatchNumCHAIN, "chain");
        break;

    case 1: // cleric
        R_CachePatch(&PatchNumPIECE1, "wpiecec1");
        R_CachePatch(&PatchNumPIECE2, "wpiecec2");
        R_CachePatch(&PatchNumPIECE3, "wpiecec3");
        R_CachePatch(&PatchNumCHAIN, "chain2");
        break;

    case 2: // mage
        R_CachePatch(&PatchNumPIECE1, "wpiecem1");
        R_CachePatch(&PatchNumPIECE2, "wpiecem2");
        R_CachePatch(&PatchNumPIECE3, "wpiecem3");
        R_CachePatch(&PatchNumCHAIN, "chain3");
        break;

    default:
        break;
    }

    if(!IS_NETGAME)
    {   // single player game uses red life gem (the second gem)
        PatchNumLIFEGEM = W_GetNumForName("lifegem") + MAXPLAYERS * class + 1;
    }
    else
    {
        PatchNumLIFEGEM =
            W_GetNumForName("lifegem") + MAXPLAYERS * class + CONSOLEPLAYER;
    }

    SB_state = -1;
}

void ST_updateWidgets(void)
{
    int         i, x;
    player_t   *plr = &players[CONSOLEPLAYER];

    if(st_blended)
    {
        statusbarCounterAlpha = cfg.statusbarCounterAlpha - hudHideAmount;
        CLAMP(statusbarCounterAlpha, 0.0f, 1.0f);
    }
    else
        statusbarCounterAlpha = 1.0f;

    // used by w_frags widget
    st_fragson = deathmatch && st_statusbaron;

    st_fragscount = 0;

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        if(!players[i].plr->inGame)
            continue;

        st_fragscount += plr->frags[i] * (i != CONSOLEPLAYER ? 1 : -1);
    }

    // current artifact
    if(ArtifactFlash)
    {
        st_artici = 5 - ArtifactFlash;
        ArtifactFlash--;
        oldarti = -1; // so that the correct artifact fills in after the flash
    }
    else if(oldarti != plr->readyArtifact ||
            oldartiCount != plr->inventory[plr->invPtr].count)
    {
        if(plr->readyArtifact > 0)
        {
            st_artici = plr->readyArtifact + 5;
        }
        oldarti = plr->readyArtifact;
        oldartiCount = plr->inventory[plr->invPtr].count;
    }

    // Armor
    armorlevel = FixedDiv(
        PCLASS_INFO(plr->class)->autoArmorSave + plr->armorPoints[ARMOR_ARMOR] +
        plr->armorPoints[ARMOR_SHIELD] +
        plr->armorPoints[ARMOR_HELMET] +
        plr->armorPoints[ARMOR_AMULET], 5 * FRACUNIT) >> FRACBITS;

    // mana A
    manaACount = plr->ammo[0];

    // mana B
    manaBCount = plr->ammo[1];

    st_manaAicon = st_manaBicon = st_manaAvial = st_manaBvial = -1;

    // Mana
    if(plr->ammo[0] == 0)              // Draw Dim Mana icon
        st_manaAicon = 0;

    if(plr->ammo[1] == 0)              // Draw Dim Mana icon
        st_manaBicon = 0;


    // Update mana graphics based upon mana count weapon type
    if(plr->readyWeapon == WT_FIRST)
    {
        st_manaAicon = 0;
        st_manaBicon = 0;

        st_manaAvial = 0;
        st_manaBvial = 0;
    }
    else if(plr->readyWeapon == WT_SECOND)
    {
        // If there is mana for this weapon, make it bright!
        if(st_manaAicon == -1)
        {
            st_manaAicon = 1;
        }

        st_manaAvial = 1;

        st_manaBicon = 0;
        st_manaBvial = 0;
    }
    else if(plr->readyWeapon == WT_THIRD)
    {
        st_manaAicon = 0;
        st_manaAvial = 0;

        // If there is mana for this weapon, make it bright!
        if(st_manaBicon == -1)
        {
            st_manaBicon = 1;
        }

        st_manaBvial = 1;
    }
    else
    {
        st_manaAvial = 1;
        st_manaBvial = 1;

        // If there is mana for this weapon, make it bright!
        if(st_manaAicon == -1)
        {
            st_manaAicon = 1;
        }
        if(st_manaBicon == -1)
        {
            st_manaBicon = 1;
        }
    }

    // update the inventory

    x = plr->invPtr - plr->curPos;

    for(i = 0; i < NUMVISINVSLOTS; ++i)
    {
        st_invslot[i] = plr->inventory[x + i].type +5;  // plus 5 for useartifact patches
        st_invslotcount[i] = plr->inventory[x + i].count;
    }
}

void ST_Ticker(void)
{
    int     delta;
    int     curHealth;
    player_t *plr = &players[CONSOLEPLAYER];

    if(!plr->plr->mo)
        return;

    if(!P_IsPaused())
    {
        if(cfg.hudTimer == 0)
        {
            hudHideTics = hudHideAmount = 0;
        }
        else
        {
            if(hudHideTics > 0)
                hudHideTics--;
            if(hudHideTics == 0 && cfg.hudTimer > 0 && hudHideAmount < 1)
                hudHideAmount += 0.1f;
        }
    }

    ST_updateWidgets();

    curHealth = plr->plr->mo->health;
    if(curHealth < 0)
    {
        curHealth = 0;
    }
    if(curHealth < HealthMarker)
    {
        delta = (HealthMarker - curHealth) >> 2;
        if(delta < 1)
        {
            delta = 1;
        }
        else if(delta > 6)
        {
            delta = 6;
        }
        HealthMarker -= delta;
    }
    else if(curHealth > HealthMarker)
    {
        delta = (curHealth - HealthMarker) >> 2;
        if(delta < 1)
        {
            delta = 1;
        }
        else if(delta > 6)
        {
            delta = 6;
        }
        HealthMarker += delta;
    }

    // turn inventory off after a certain amount of time
    if(inventory && !(--inventoryTics))
    {
        plr->readyArtifact = plr->inventory[plr->invPtr].type;
        inventory = false;
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
            GL_DrawPatch_CS(x + 8, y, PatchNumINumbers[val / 10].lump);
            GL_DrawPatch_CS(x, y, PatchNumNEGATIVE.lump);
        }
        else
        {
            GL_DrawPatch_CS(x + 8, y, PatchNumNEGATIVE.lump);
        }
        val = val % 10;
        GL_DrawPatch_CS(x + 16, y, PatchNumINumbers[val].lump);
        return;
    }
    if(val > 99)
    {
        GL_DrawPatch_CS(x, y, PatchNumINumbers[val / 100].lump);
    }
    val = val % 100;
    if(val > 9 || oldval > 99)
    {
        GL_DrawPatch_CS(x + 8, y, PatchNumINumbers[val / 10].lump);
    }
    val = val % 10;
    GL_DrawPatch_CS(x + 16, y, PatchNumINumbers[val].lump);

}

#if 0 // UNUSED
/**
 * Draws a three digit number using the red font
 */
static void DrRedINumber(signed int val, int x, int y)
{
    int         oldval;

    // Make sure it's a three digit number.
    if(val < -999)
        val = -999;
    if(val > 999)
        val = 999;

    oldval = val;
    if(val < 0)
    {
        val = 0;
    }
    if(val > 99)
    {
        GL_DrawPatch(x, y, W_GetNumForName("inred0") + val / 100);
    }
    val = val % 100;
    if(val > 9 || oldval > 99)
    {
        GL_DrawPatch(x + 8, y, W_GetNumForName("inred0") + val / 10);
    }
    val = val % 10;
    GL_DrawPatch(x + 16, y, W_GetNumForName("inred0") + val);
}
#endif

/**
 * Draws a three digit number using FontB
 */
static void DrBNumber(signed int val, int x, int y, float red, float green,
                      float blue, float alpha)
{
    lumppatch_t    *patch;
    int         xpos;
    int         oldval;

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
        patch = W_CacheLumpNum(FontBNumBase + val / 100, PU_CACHE);
        GL_DrawPatchLitAlpha(xpos + 8 - SHORT(patch->width) / 2, y +2, 0,
                             alpha * .4f,
                             FontBNumBase + val / 100);
        GL_SetColorAndAlpha(red, green, blue, alpha);
        GL_DrawPatch_CS(xpos + 6 - SHORT(patch->width) / 2, y,
                             FontBNumBase + val / 100);
        GL_SetColorAndAlpha(1, 1, 1, 1);
    }
    val = val % 100;
    xpos += 12;
    if(val > 9 || oldval > 99)
    {
        patch = W_CacheLumpNum(FontBNumBase + val / 10, PU_CACHE);
        GL_DrawPatchLitAlpha(xpos + 8 - SHORT(patch->width) / 2, y +2, 0,
                             alpha * .4f,
                             FontBNumBase + val / 10);
        GL_SetColorAndAlpha(red, green, blue, alpha);
        GL_DrawPatch_CS(xpos + 6 - SHORT(patch->width) / 2, y,
                             FontBNumBase + val / 10);
        GL_SetColorAndAlpha(1, 1, 1, 1);
    }
    val = val % 10;
    xpos += 12;
    patch = W_CacheLumpNum(FontBNumBase + val, PU_CACHE);
    GL_DrawPatchLitAlpha(xpos + 8 - SHORT(patch->width) / 2, y +2, 0,
                         alpha *.4f, FontBNumBase + val);
    GL_SetColorAndAlpha(red, green, blue, alpha);
    GL_DrawPatch_CS(xpos + 6 - SHORT(patch->width) / 2, y,
                             FontBNumBase + val);
    GL_SetColorAndAlpha(1, 1, 1, 1);
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
        GL_DrawPatch_CS(x, y, PatchNumSmNumbers[val / 100].lump);
        GL_DrawPatch_CS(x + 4, y, PatchNumSmNumbers[(val % 100) / 10].lump);
    }
    else if(val > 9)
    {
        GL_DrawPatch_CS(x + 4, y, PatchNumSmNumbers[val / 10].lump);
    }
    val %= 10;
    GL_DrawPatch_CS(x + 8, y, PatchNumSmNumbers[val].lump);
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

    if(leveltime & 16)
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
 *  Draws the whole statusbar backgound
 */
void ST_refreshBackground(void)
{
    int         x, y, w, h;
    float       cw, cw2, ch;
    player_t   *plyr = &players[CONSOLEPLAYER];
    float       alpha;

    if(st_blended)
    {
        alpha = cfg.statusbarAlpha - hudHideAmount;
        // Clamp
        CLAMP(alpha, 0.0f, 1.0f);
        if(!(alpha > 0))
            return;
    }
    else
        alpha = 1.0f;

    if(!(alpha < 1))
    {
        GL_DrawPatch(0, 134, PatchNumH2BAR.lump);
        GL_DrawPatch(0, 134, PatchNumH2TOP.lump);

        if(!inventory)
        {
            // Main interface
            if(!AM_IsMapActive(CONSOLEPLAYER))
            {
                GL_DrawPatch(38, 162, PatchNumSTATBAR.lump);

                if(plyr->pieces == 7)
                {
                    GL_DrawPatch(190, 162, PatchNumWEAPONFULL.lump);
                }
                else
                {
                    GL_DrawPatch(190, 162, PatchNumWEAPONSLOT.lump);
                }

                DrawWeaponPieces();
            }
            else
            {
                GL_DrawPatch(38, 162, PatchNumKEYBAR.lump);
                DrawKeyBar();
            }

        }
        else
        {
            GL_DrawPatch(38, 162, PatchNumINVBAR.lump);
        }

        DrawChain();
    }
    else
    {
        DGL_Color4f(1, 1, 1, alpha);

        GL_SetPatch(PatchNumH2BAR.lump, DGL_REPEAT, DGL_REPEAT);

        DGL_Begin(DGL_QUADS);

        // top
        x = 0;
        y = 135;
        w = 320;
        h = 27;
        ch = 0.41538461538461538461538461538462;

        DGL_TexCoord2f(0, 0);
        DGL_Vertex2f(x, y);
        DGL_TexCoord2f(1, 0);
        DGL_Vertex2f(x + w, y);
        DGL_TexCoord2f(1, ch);
        DGL_Vertex2f(x + w, y + h);
        DGL_TexCoord2f(0, ch);
        DGL_Vertex2f(x, y + h);

        // left statue
        x = 0;
        y = 162;
        w = 38;
        h = 38;
        cw = 0.11875f;
        ch = 0.41538461538461538461538461538462;

        DGL_TexCoord2f(0, ch);
        DGL_Vertex2f(x, y);
        DGL_TexCoord2f(cw, ch);
        DGL_Vertex2f(x + w, y);
        DGL_TexCoord2f(cw, 1);
        DGL_Vertex2f(x + w, y + h);
        DGL_TexCoord2f(0, 1);
        DGL_Vertex2f(x, y + h);

        // right statue
        x = 282;
        y = 162;
        w = 38;
        h = 38;
        cw = 0.88125f;
        ch = 0.41538461538461538461538461538462f;

        DGL_TexCoord2f(cw, ch);
        DGL_Vertex2f(x, y);
        DGL_TexCoord2f(1, ch);
        DGL_Vertex2f(x + w, y);
        DGL_TexCoord2f(1, 1);
        DGL_Vertex2f(x + w, y + h);
        DGL_TexCoord2f(cw, 1);
        DGL_Vertex2f(x, y + h);

        // bottom (behind the chain)
        x = 38;
        y = 192;
        w = 244;
        h = 8;
        cw = 0.11875f;
        cw2 = 0.88125f;
        ch = 0.87692307692307692307692307692308f;

        DGL_TexCoord2f(cw, ch);
        DGL_Vertex2f(x, y);
        DGL_TexCoord2f(cw2, ch);
        DGL_Vertex2f(x + w, y);
        DGL_TexCoord2f(cw2, 1);
        DGL_Vertex2f(x + w, y + h);
        DGL_TexCoord2f(cw, 1);
        DGL_Vertex2f(x, y + h);

        DGL_End();

        if(!inventory)
        {
            // Main interface
            if(!AM_IsMapActive(CONSOLEPLAYER))
            {
                if(deathmatch)
                {
                    GL_DrawPatch_CS(38, 162, PatchNumKILLS.lump);
                }

                // left of statbar (upto weapon puzzle display)
                GL_SetPatch(PatchNumSTATBAR.lump, DGL_CLAMP, DGL_CLAMP);
                DGL_Begin(DGL_QUADS);

                x = deathmatch ? 68 : 38;
                y = 162;
                w = deathmatch ? 122 : 152;
                h = 30;
                cw = deathmatch ? 0.12295081967213114754098360655738f : 0;
                cw2 = 0.62295081967213114754098360655738f;
                ch = 0.96774193548387096774193548387097f;

                DGL_TexCoord2f(cw, 0);
                DGL_Vertex2f(x, y);
                DGL_TexCoord2f(cw2, 0);
                DGL_Vertex2f(x + w, y);
                DGL_TexCoord2f(cw2, ch);
                DGL_Vertex2f(x + w, y + h);
                DGL_TexCoord2f(cw, ch);
                DGL_Vertex2f(x, y + h);

                // right of statbar (after weapon puzzle display)
                x = 247;
                y = 162;
                w = 35;
                h = 30;
                cw = 0.85655737704918032786885245901639f;
                ch = 0.96774193548387096774193548387097f;

                DGL_TexCoord2f(cw, 0);
                DGL_Vertex2f(x, y);
                DGL_TexCoord2f(1, 0);
                DGL_Vertex2f(x + w, y);
                DGL_TexCoord2f(1, ch);
                DGL_Vertex2f(x + w, y + h);
                DGL_TexCoord2f(cw, ch);
                DGL_Vertex2f(x, y + h);

                DGL_End();

                DrawWeaponPieces();
            }
            else
            {
                GL_DrawPatch_CS(38, 162, PatchNumKEYBAR.lump);
            }
        }
        else
        {
            // INVBAR
            GL_SetPatch(PatchNumINVBAR.lump, DGL_CLAMP, DGL_CLAMP);
            DGL_Begin(DGL_QUADS);

            x = 38;
            y = 162;
            w = 244;
            h = 30;
            ch = 0.96774193548387096774193548387097f;

            DGL_TexCoord2f(0, 0);
            DGL_Vertex2f(x, y);
            DGL_TexCoord2f(1, 0);
            DGL_Vertex2f(x + w, y);
            DGL_TexCoord2f(1, ch);
            DGL_Vertex2f(x + w, y + h);
            DGL_TexCoord2f(0, ch);
            DGL_Vertex2f(x, y + h);

            DGL_End();
        }

        DrawChain();
    }
}

/**
 * Unhides the current HUD display if hidden.
 *
 * @param event         The HUD Update Event type to check for triggering.
 */
void ST_HUDUnHide(hueevent_t event)
{
    if(event < HUE_FORCE || event > NUMHUDUNHIDEEVENTS)
        return;

    if(event == HUE_FORCE || cfg.hudUnHide[event])
    {
        hudHideTics = (cfg.hudTimer * TICSPERSEC);
        hudHideAmount = 0;
    }
}

/**
 * All drawing for the status bar starts and ends here
 */
void ST_doRefresh(void)
{
    st_firsttime = false;

    if(cfg.statusbarScale < 20 || (cfg.statusbarScale == 20 && showbar < 1.0f))
    {
        float fscale = cfg.statusbarScale / 20.0f;
        float h = 200 * (1 - fscale);

        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PushMatrix();
        DGL_Translatef(160 - 320 * fscale / 2, h /showbar, 0);
        DGL_Scalef(fscale, fscale, 1);
    }

    // draw status bar background
    ST_refreshBackground();

    // and refresh all widgets
    ST_drawWidgets(true);

    if(cfg.statusbarScale < 20 || (cfg.statusbarScale == 20 && showbar < 1.0f))
    {
        // Restore the normal modelview matrix.
        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PopMatrix();
    }
}

void ST_Drawer(int fullscreenmode, boolean refresh )
{
    st_firsttime = st_firsttime || refresh;
    st_statusbaron = (fullscreenmode < 2) ||
                      (AM_IsMapActive(CONSOLEPLAYER) &&
                       (cfg.automapHudDisplay == 0 || cfg.automapHudDisplay == 2));

    // Do palette shifts
    ST_doPaletteStuff(false);

    // Either slide the status bar in or fade out the fullscreen hud
    if(st_statusbaron)
    {
        if(hudalpha > 0.0f)
        {
            st_statusbaron = 0;
            hudalpha-=0.1f;
        }
        else if(showbar < 1.0f)
            showbar+=0.1f;
    }
    else
    {
        if(fullscreenmode == 3)
        {
            if(hudalpha > 0.0f)
            {
                hudalpha-=0.1f;
                fullscreenmode = 2;
            }
        }
        else
        {
            if(showbar > 0.0f)
            {
                showbar-=0.1f;
                st_statusbaron = 1;
            }
            else if(hudalpha < 1.0f)
                hudalpha+=0.1f;
        }
    }

    // Always try to render statusbar with alpha in fullscreen modes
    if(fullscreenmode)
        st_blended = 1;
    else
        st_blended = 0;

    if(st_statusbaron)
    {
        ST_doRefresh();
    }
    else if(fullscreenmode != 3
#ifdef DEMOCAM
        || (demoplayback && democam.mode)
#endif
                    )
    {
        ST_doFullscreenStuff();
    }

    DrawAnimatedIcons();
}

static void DrawAnimatedIcons(void)
{
    static boolean hitCenterFrame;
    int         leftoff = 0;
    int         frame;
    float       iconalpha = (st_statusbaron? 1: hudalpha) - ( 1 - cfg.hudIconAlpha);
    player_t   *plyr = &players[CONSOLEPLAYER];

    // If the fullscreen mana is drawn, we need to move the icons on the left
    // a bit to the right.
    if(cfg.hudShown[HUD_MANA] == 1 && cfg.screenBlocks > 10)
        leftoff = 42;

    Draw_BeginZoom(cfg.hudScale, 2, 2);

    // Wings of wrath
    if(plyr->powers[PT_FLIGHT])
    {
        if(plyr->powers[PT_FLIGHT] > BLINKTHRESHOLD ||
           !(plyr->powers[PT_FLIGHT] & 16))
        {
            frame = (levelTime / 3) & 15;
            if(plyr->plr->mo->flags2 & MF2_FLY)
            {
                if(hitCenterFrame && (frame != 15 && frame != 0))
                {
                    GL_DrawPatchLitAlpha(20 + leftoff, 19, 1, iconalpha,
                                         SpinFlylump.lump + 15);
                }
                else
                {
                    GL_DrawPatchLitAlpha(20 + leftoff, 19, 1, iconalpha,
                                         SpinFlylump.lump + frame);
                    hitCenterFrame = false;
                }
            }
            else
            {
                if(!hitCenterFrame && (frame != 15 && frame != 0))
                {
                    GL_DrawPatchLitAlpha(20 + leftoff, 19, 1, iconalpha,
                                         SpinFlylump.lump + frame);
                    hitCenterFrame = false;
                }
                else
                {
                    GL_DrawPatchLitAlpha(20 + leftoff, 19, 1, iconalpha,
                                         SpinFlylump.lump + 15);
                    hitCenterFrame = true;
                }
            }
        }
    }

    // Speed Boots
    if(plyr->powers[PT_SPEED])
    {
        if(plyr->powers[PT_SPEED] > BLINKTHRESHOLD ||
           !(plyr->powers[PT_SPEED] & 16))
        {
            frame = (levelTime / 3) & 15;
            GL_DrawPatchLitAlpha(60 + leftoff, 19, 1, iconalpha,
                                 SpinSpeedLump.lump + frame);
        }
    }

    Draw_EndZoom();

    Draw_BeginZoom(cfg.hudScale, 318, 2);

    // Defensive power
    if(plyr->powers[PT_INVULNERABILITY])
    {
        if(plyr->powers[PT_INVULNERABILITY] > BLINKTHRESHOLD ||
           !(plyr->powers[PT_INVULNERABILITY] & 16))
        {
            frame = (levelTime / 3) & 15;
            GL_DrawPatchLitAlpha(260, 19, 1, iconalpha,
                                 SpinDefenseLump.lump + frame);
        }
    }

    // Minotaur Active
    if(plyr->powers[PT_MINOTAUR])
    {
        if(plyr->powers[PT_MINOTAUR] > BLINKTHRESHOLD ||
           !(plyr->powers[PT_MINOTAUR] & 16))
        {
            frame = (levelTime / 3) & 15;
            GL_DrawPatchLitAlpha(300, 19, 1, iconalpha,
                                 SpinMinotaurLump.lump + frame);
        }
    }

    Draw_EndZoom();
}

/**
 * Sets the new palette based upon the current values of
 * CONSOLEPLAYER->damageCount and CONSOLEPLAYER->bonusCount.
 */
void ST_doPaletteStuff(boolean forceChange)
{
    static int  sb_palette = 0;
    int         palette;
    player_t   *plyr = &players[CONSOLEPLAYER];

    if(forceChange)
    {
        sb_palette = -1;
    }

    if(G_GetGameState() == GS_LEVEL)
    {
        plyr = &players[CONSOLEPLAYER];
        if(plyr->poisonCount)
        {
            palette = 0;
            palette = (plyr->poisonCount + 7) >> 3;
            if(palette >= NUMPOISONPALS)
            {
                palette = NUMPOISONPALS - 1;
            }
            palette += STARTPOISONPALS;
        }
        else if(plyr->damageCount)
        {
            palette = (plyr->damageCount + 7) >> 3;
            if(palette >= NUMREDPALS)
            {
                palette = NUMREDPALS - 1;
            }
            palette += STARTREDPALS;
        }
        else if(plyr->bonusCount)
        {
            palette = (plyr->bonusCount + 7) >> 3;
            if(palette >= NUMBONUSPALS)
            {
                palette = NUMBONUSPALS - 1;
            }
            palette += STARTBONUSPALS;
        }
        else if(plyr->plr->mo->flags2 & MF2_ICEDAMAGE)
        {                       // Frozen player
            palette = STARTICEPAL;
        }
        else
        {
            palette = 0;
        }
    }
    else
    {
        palette = 0;
    }
    if(palette != sb_palette)
    {
        sb_palette = palette;
        // $democam
        plyr->plr->filter = R_GetFilterColor(palette);
    }
}

void DrawChain(void)
{
    int         x, x2, y, w, w2, w3, h;
    int         gemoffset = 36;
    float       cw, cw2;
    float       healthPos;
    float       gemglow;

    healthPos = HealthMarker;
    if(healthPos < 0)
        healthPos = 0;
    if(healthPos > 100)
        healthPos = 100;

    gemglow = healthPos / 100;

    // draw the chain
    x = 44;
    y = 193;
    w = 232;
    h = 7;
    cw = (healthPos / 113) + 0.054f;

    GL_SetPatch(PatchNumCHAIN.lump, DGL_REPEAT, DGL_CLAMP);

    DGL_Color4f(1, 1, 1, statusbarCounterAlpha);

    DGL_Begin(DGL_QUADS);

    DGL_TexCoord2f( 0 - cw, 0);
    DGL_Vertex2f(x, y);

    DGL_TexCoord2f( 0.948f - cw, 0);
    DGL_Vertex2f(x + w, y);

    DGL_TexCoord2f( 0.948f - cw, 1);
    DGL_Vertex2f(x + w, y + h);

    DGL_TexCoord2f( 0 - cw, 1);
    DGL_Vertex2f(x, y + h);

    DGL_End();


    healthPos = ((healthPos * 256) / 117) - gemoffset;

    x = 44;
    y = 193;
    w2 = 86;
    h = 7;
    cw = 0;
    cw2 = 1;

    // calculate the size of the quad, position and tex coords
    if(x + healthPos < x)
    {
        x2 = x;
        w3 = w2 + healthPos;
        cw = (1.0f / w2) * (w2 - w3);
        cw2 = 1;
    }
    else if(x + healthPos + w2 > x + w)
    {
        x2 = x + healthPos;
        w3 = w2 - ((x + healthPos + w2) - (x + w));
        cw = 0;
        cw2 = (1.0f / w2) * (w2 - (w2 - w3));
    }
    else
    {
        x2 = x + healthPos;
        w3 = w2;
        cw = 0;
        cw2 = 1;
    }

    GL_SetPatch(PatchNumLIFEGEM, DGL_CLAMP, DGL_CLAMP);

    // Draw the life gem.
    DGL_Color4f(1, 1, 1, statusbarCounterAlpha);

    DGL_Begin(DGL_QUADS);

    DGL_TexCoord2f( cw, 0);
    DGL_Vertex2f(x2, y);

    DGL_TexCoord2f( cw2, 0);
    DGL_Vertex2f(x2 + w3, y);

    DGL_TexCoord2f( cw2, 1);
    DGL_Vertex2f(x2 + w3, y + h);

    DGL_TexCoord2f( cw, 1);
    DGL_Vertex2f(x2, y + h);

    DGL_End();

    // how about a glowing gem?
    GL_BlendMode(BM_ADD);
    DGL_Bind(Get(DD_DYNLIGHT_TEXTURE));

    GL_DrawRect(x + healthPos + 25, y - 3, 34, 18, 1, 0, 0,
                gemglow - (1 - statusbarCounterAlpha));

    GL_BlendMode(BM_NORMAL);
}

void ST_drawWidgets(boolean refresh)
{
    int         i;
    int         x;
    player_t   *plyr = &players[CONSOLEPLAYER];

    oldhealth = -1;
    if(!inventory)
    {
        if(!AM_IsMapActive(CONSOLEPLAYER))
        {
            // Frags
            if(deathmatch)
                    STlib_updateNum(&w_frags, refresh);
            else
                    STlib_updateNum(&w_health, refresh);

            // draw armor
            STlib_updateNum(&w_armor, refresh);

            // current artifact
            if(plyr->readyArtifact > 0)
            {
                STlib_updateMultIcon(&w_artici, refresh);
                if(!ArtifactFlash && plyr->inventory[plyr->invPtr].count > 1)
                    STlib_updateNum(&w_articount, refresh);
            }

            // manaA count
            if(manaACount > 0)
                STlib_updateNum(&w_manaACount, refresh);

            // manaB count
            if(manaBCount > 0)
                STlib_updateNum(&w_manaBCount, refresh);

            // manaA icon
            STlib_updateMultIcon(&w_manaAicon, refresh);

            // manaB icon
            STlib_updateMultIcon(&w_manaBicon, refresh);

            // manaA vial
            STlib_updateMultIcon(&w_manaAvial, refresh);

            // manaB vial
            STlib_updateMultIcon(&w_manaBvial, refresh);

            // Draw the mana bars
            GL_SetNoTexture();
            GL_DrawRect(95, 165, 3,
                        22 - (22 * plyr->ammo[0]) / MAX_MANA, 0, 0, 0,
                        statusbarCounterAlpha);
            GL_DrawRect(103, 165, 3,
                        22 - (22 * plyr->ammo[1]) / MAX_MANA, 0, 0, 0,
                        statusbarCounterAlpha);
        }
        else
        {
            DrawKeyBar();
        }
    }
    else
    {   // Draw Inventory

        x = plyr->invPtr - plyr->curPos;

        for(i = 0; i < NUMVISINVSLOTS; ++i)
        {
            if( plyr->inventory[x + i].type != arti_none)
            {
                STlib_updateMultIcon(&w_invslot[i], refresh);

                if( plyr->inventory[x + i].count > 1)
                    STlib_updateNum(&w_invslotcount[i], refresh);
            }
        }

        // Draw selector box
        GL_DrawPatch(ST_INVENTORYX + plyr->curPos * 31, 163, PatchNumSELECTBOX.lump);

        // Draw more left indicator
        if(x != 0)
            GL_DrawPatchLitAlpha(42, 163, 1, statusbarCounterAlpha,
                                 !(levelTime & 4) ? PatchNumINVLFGEM1.lump : PatchNumINVLFGEM2.lump);

        // Draw more right indicator
        if(plyr->inventorySlotNum - x > 7)
            GL_DrawPatchLitAlpha(269, 163, 1, statusbarCounterAlpha,
                                 !(levelTime & 4) ? PatchNumINVRTGEM1.lump : PatchNumINVRTGEM2.lump);
    }
}

void DrawKeyBar(void)
{
    int         i;
    int         xPosition;
    int         temp;
    player_t   *plyr = &players[CONSOLEPLAYER];

    xPosition = 46;
    for(i = 0; i < NUM_KEY_TYPES && xPosition <= 126; ++i)
    {
        if(plyr->keys & (1 << i))
        {
            GL_DrawPatchLitAlpha(xPosition, 163, 1, statusbarCounterAlpha,
                                 W_GetNumForName("keyslot1") + i);
            xPosition += 20;
        }
    }

    temp =
        PCLASS_INFO(plyr->class)->autoArmorSave + plyr->armorPoints[ARMOR_ARMOR] +
        plyr->armorPoints[ARMOR_SHIELD] +
        plyr->armorPoints[ARMOR_HELMET] +
        plyr->armorPoints[ARMOR_AMULET];

    for(i = 0; i < NUMARMOR; ++i)
    {
        if(!plyr->armorPoints[i])
            continue;

        if(plyr->armorPoints[i] <= (PCLASS_INFO(plyr->class)->armorIncrement[i] >> 2))
        {
            GL_DrawPatchLitAlpha(150 + 31 * i, 164, 1, statusbarCounterAlpha * 0.3,
                             W_GetNumForName("armslot1") + i);
        }
        else if(plyr->armorPoints[i] <=
                (PCLASS_INFO(plyr->class)->armorIncrement[i] >> 1))
        {
            GL_DrawPatchLitAlpha(150 + 31 * i, 164, 1, statusbarCounterAlpha * 0.6,
                                W_GetNumForName("armslot1") + i);
        }
        else
        {
            GL_DrawPatchLitAlpha(150 + 31 * i, 164, 1, statusbarCounterAlpha,
                                 W_GetNumForName("armslot1") + i);
        }
    }
}

static void DrawWeaponPieces(void)
{
    player_t   *plyr = &players[CONSOLEPLAYER];
    float       alpha;

    alpha = cfg.statusbarAlpha - hudHideAmount;
    // Clamp
    CLAMP(alpha, 0.0f, 1.0f);

    GL_DrawPatchLitAlpha(190, 162, 1, alpha, PatchNumWEAPONSLOT.lump);

    if(plyr->pieces == 7) // All pieces
        GL_DrawPatchLitAlpha(190, 162, 1, statusbarCounterAlpha, PatchNumWEAPONFULL.lump);
    else
    {
        if(plyr->pieces & WPIECE1)
        {
            GL_DrawPatchLitAlpha(PCLASS_INFO(cfg.playerClass[CONSOLEPLAYER])->pieceX[0], 162,
                            1, statusbarCounterAlpha, PatchNumPIECE1.lump);
        }
        if(plyr->pieces & WPIECE2)
        {
            GL_DrawPatchLitAlpha(PCLASS_INFO(cfg.playerClass[CONSOLEPLAYER])->pieceX[1], 162,
                            1, statusbarCounterAlpha, PatchNumPIECE2.lump);
        }
        if(plyr->pieces & WPIECE3)
        {
            GL_DrawPatchLitAlpha(PCLASS_INFO(cfg.playerClass[CONSOLEPLAYER])->pieceX[2], 162,
                            1, statusbarCounterAlpha, PatchNumPIECE3.lump);
        }
    }
}

void ST_doFullscreenStuff(void)
{
    int         i;
    int         x;
    int         temp;
    float       textalpha = hudalpha - hudHideAmount - ( 1 - cfg.hudColor[3]);
    float       iconalpha = hudalpha - hudHideAmount - ( 1 - cfg.hudIconAlpha);
    player_t   *plyr = &players[CONSOLEPLAYER];

#ifdef DEMOCAM
    if(demoplayback && democam.mode)
        return;
#endif

    if(cfg.hudShown[HUD_HEALTH])
    {
    Draw_BeginZoom(cfg.hudScale, 5, 198);
    if(plyr->plr->mo->health > 0)
    {
        DrBNumber(plyr->plr->mo->health, 5, 180,
                  cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textalpha);
    }
    else
    {
        DrBNumber(0, 5, 180, cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2],
                  textalpha);
    }
    Draw_EndZoom();
    }

    if(cfg.hudShown[HUD_MANA])
    {
        int     dim[2] = { PatchMANAAICONS[0].lump, PatchMANABICONS[0].lump };
        int     bright[2] = { PatchMANAAICONS[0].lump, PatchMANABICONS[0].lump };
        int     patches[2] = { 0, 0 };
        int     ypos = cfg.hudShown[HUD_MANA] == 2 ? 152 : 2;

        for(i = 0; i < 2; i++)
            if(plyr->ammo[i] == 0)
                patches[i] = dim[i];
        if(plyr->readyWeapon == WT_FIRST)
        {
            for(i = 0; i < 2; i++)
                patches[i] = dim[i];
        }
        if(plyr->readyWeapon == WT_SECOND)
        {
            if(!patches[0])
                patches[0] = bright[0];
            patches[1] = dim[1];
        }
        if(plyr->readyWeapon == WT_THIRD)
        {
            patches[0] = dim[0];
            if(!patches[1])
                patches[1] = bright[1];
        }
        if(plyr->readyWeapon == WT_FOURTH)
        {
            for(i = 0; i < 2; i++)
                if(!patches[i])
                    patches[i] = bright[i];
        }
        Draw_BeginZoom(cfg.hudScale, 2, ypos);
        for(i = 0; i < 2; i++)
        {
            GL_DrawPatchLitAlpha(2, ypos + i * 13, 1, iconalpha, patches[i]);
            DrINumber(plyr->ammo[i], 18, ypos + i * 13, 1, 1, 1, textalpha);
        }
        Draw_EndZoom();
    }

    if(deathmatch)
    {
        temp = 0;
        for(i = 0; i < MAXPLAYERS; i++)
        {
            if(players[i].plr->inGame)
            {
                temp += plyr->frags[i];
            }
        }
        Draw_BeginZoom(cfg.hudScale, 2, 198);
        DrINumber(temp, 45, 185, 1, 1, 1, textalpha);
        Draw_EndZoom();
    }
    if(!inventory)
    {
        if(cfg.hudShown[HUD_ARTI]){
            if(plyr->readyArtifact > 0)
            {
                    Draw_BeginZoom(cfg.hudScale, 318, 198);
                GL_DrawPatchLitAlpha(286, 170, 1, iconalpha/2, W_GetNumForName("ARTIBOX"));
                GL_DrawPatchLitAlpha(284, 169, 1, iconalpha,
                         W_GetNumForName(artifactlist[plyr->readyArtifact+5]));
                if(plyr->inventory[plyr->invPtr].count > 1)
                {
                    DrSmallNumber(plyr->inventory[plyr->invPtr].count,
                                  302, 192, 1, 1, 1, textalpha);
                }
                    Draw_EndZoom();
            }
        }
    }
    else
    {
        float invScale;

        invScale = MINMAX_OF(0.25f, cfg.hudScale - 0.25f, 0.8f);

        Draw_BeginZoom(invScale, 160, 198);
        x = plyr->invPtr - plyr->curPos;
        for(i = 0; i < 7; i++)
        {
            GL_DrawPatchLitAlpha(50 + i * 31, 168, 1, iconalpha/2, W_GetNumForName("ARTIBOX"));
            if(plyr->inventorySlotNum > x + i &&
               plyr->inventory[x + i].type != arti_none)
            {
                GL_DrawPatchLitAlpha(49 + i * 31, 167, 1, i==plyr->curPos? hudalpha : iconalpha,
                             W_GetNumForName(artifactlist[plyr->inventory
                                                       [x + i].type+5]));

                if(plyr->inventory[x + i].count > 1)
                {
                    DrSmallNumber(plyr->inventory[x + i].count, 66 + i * 31,
                                  188,1, 1, 1, i==plyr->curPos? hudalpha : textalpha/2);
                }
            }
        }
        GL_DrawPatchLitAlpha(50 + plyr->curPos * 31, 167, 1, hudalpha,PatchNumSELECTBOX.lump);
        if(x != 0)
        {
            GL_DrawPatchLitAlpha(40, 167, 1, iconalpha,
                         !(levelTime & 4) ? PatchNumINVLFGEM1.lump :
                         PatchNumINVLFGEM2.lump);
        }
        if(plyr->inventorySlotNum - x > 7)
        {
            GL_DrawPatchLitAlpha(268, 167, 1, iconalpha,
                         !(levelTime & 4) ? PatchNumINVRTGEM1.lump :
                         PatchNumINVRTGEM2.lump);
        }
        Draw_EndZoom();
    }
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
    GL_DrawPatch(100, 68, W_GetNumForName("teleicon"));
}

/**
 * Console command to show the hud if hidden.
 */
DEFCC(CCmdHUDShow)
{
    ST_HUDUnHide(HUE_FORCE);
    return true;
}

/**
 * Console command to change the size of the status bar.
 */
DEFCC(CCmdStatusBarSize)
{
    int     min = 1, max = 20, *val = &cfg.statusbarScale;

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
    R_SetViewSize(cfg.screenBlocks, 0);
    ST_HUDUnHide(HUE_FORCE); // so the user can see the change.
    return true;
}

/**
 * Changes the class of the given player. Will not work if the player
 * is currently morphed.
 */
void SB_ChangePlayerClass(player_t *player, int newclass)
{
    int         i;
    mobj_t     *oldMobj;
    spawnspot_t dummy;

    // Don't change if morphed.
    if(player->morphTics)
        return;

    if(newclass < 0 || newclass > 2)
        return;                 // Must be 0-2.
    player->class = newclass;
    // Take away armor.
    for(i = 0; i < NUMARMOR; i++)
        player->armorPoints[i] = 0;
    cfg.playerClass[player - players] = newclass;
    P_PostMorphWeapon(player, WT_FIRST);
    if(player == players + CONSOLEPLAYER)
        SB_SetClassData();
    player->update |= PSF_ARMOR_POINTS;

    // Respawn the player and destroy the old mobj.
    oldMobj = player->plr->mo;
    if(oldMobj)
    {   // Use a dummy as the spawn point.
        dummy.pos[VX] = oldMobj->pos[VX];
        dummy.pos[VY] = oldMobj->pos[VY];
        dummy.angle = oldMobj->angle;

        P_SpawnPlayer(&dummy, player - players);
        P_MobjRemove(oldMobj);
    }
}
