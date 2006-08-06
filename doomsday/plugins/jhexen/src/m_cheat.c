/* DE1: $Id: template.c 2645 2006-01-21 12:58:39Z skyjake $
 * Copyright (C) 1999- Activision
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

/*
 * m_cheat.c : Cheat sequence checking.
 *
 */

// HEADER FILES ------------------------------------------------------------

#include "h2def.h"

// MACROS ------------------------------------------------------------------

#define CHEAT_ENCRYPT(a) \
    ((((a)&1)<<2)+ \
    (((a)&2)>>1)+ \
    (((a)&4)<<5)+ \
    (((a)&8)<<2)+ \
    (((a)&16)>>3)+ \
    (((a)&32)<<1)+ \
    (((a)&64)>>3)+ \
    (((a)&128)>>3))

// TYPES -------------------------------------------------------------------

// Cheat types for nofication.
typedef enum cheattype_e {
    CHT_GOD,
    CHT_NOCLIP,
    CHT_WEAPONS,
    CHT_HEALTH,
    CHT_KEYS,
    CHT_ARTIFACTS,
    CHT_PUZZLE
} cheattype_t;

typedef struct cheat_s {
    void    (*func) (player_t *player, struct cheat_s * cheat);
    byte   *sequence;
    byte   *pos;
    int     args[2];
    int     currentArg;
} cheat_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

boolean     cht_Responder(event_t *ev);
static boolean CheatAddKey(cheat_t * cheat, byte key, boolean *eat);
static void CheatGodFunc(player_t *player, cheat_t * cheat);
static void CheatNoClipFunc(player_t *player, cheat_t * cheat);
static void CheatWeaponsFunc(player_t *player, cheat_t * cheat);
static void CheatHealthFunc(player_t *player, cheat_t * cheat);
static void CheatKeysFunc(player_t *player, cheat_t * cheat);
static void CheatSoundFunc(player_t *player, cheat_t * cheat);
static void CheatTickerFunc(player_t *player, cheat_t * cheat);
static void CheatArtifactAllFunc(player_t *player, cheat_t * cheat);
static void CheatPuzzleFunc(player_t *player, cheat_t * cheat);
static void CheatWarpFunc(player_t *player, cheat_t * cheat);
static void CheatPigFunc(player_t *player, cheat_t * cheat);
static void CheatMassacreFunc(player_t *player, cheat_t * cheat);
static void CheatIDKFAFunc(player_t *player, cheat_t * cheat);
static void CheatQuickenFunc1(player_t *player, cheat_t * cheat);
static void CheatQuickenFunc2(player_t *player, cheat_t * cheat);
static void CheatQuickenFunc3(player_t *player, cheat_t * cheat);
static void CheatClassFunc1(player_t *player, cheat_t * cheat);
static void CheatClassFunc2(player_t *player, cheat_t * cheat);
static void CheatInitFunc(player_t *player, cheat_t * cheat);
static void CheatInitFunc(player_t *player, cheat_t * cheat);
static void CheatVersionFunc(player_t *player, cheat_t * cheat);
static void CheatDebugFunc(player_t *player, cheat_t * cheat);
static void CheatScriptFunc1(player_t *player, cheat_t * cheat);
static void CheatScriptFunc2(player_t *player, cheat_t * cheat);
static void CheatScriptFunc3(player_t *player, cheat_t * cheat);
static void CheatRevealFunc(player_t *player, cheat_t * cheat);
static void CheatTrackFunc1(player_t *player, cheat_t * cheat);
static void CheatTrackFunc2(player_t *player, cheat_t * cheat);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int  messageResponse;
extern boolean automapactive;
extern int  cheating;

extern boolean ShowKills;
extern unsigned ShowKillsCount;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static byte CheatLookup[256];

// Toggle god mode
static byte CheatGodSeq[] = {
    CHEAT_ENCRYPT('s'),
    CHEAT_ENCRYPT('a'),
    CHEAT_ENCRYPT('t'),
    CHEAT_ENCRYPT('a'),
    CHEAT_ENCRYPT('n'),
    0xff
};

// Toggle no clipping mode
static byte CheatNoClipSeq[] = {
    CHEAT_ENCRYPT('c'),
    CHEAT_ENCRYPT('a'),
    CHEAT_ENCRYPT('s'),
    CHEAT_ENCRYPT('p'),
    CHEAT_ENCRYPT('e'),
    CHEAT_ENCRYPT('r'),
    0xff
};

// Get all weapons and mana
static byte CheatWeaponsSeq[] = {
    CHEAT_ENCRYPT('n'),
    CHEAT_ENCRYPT('r'),
    CHEAT_ENCRYPT('a'),
    0xff
};

// Get full health
static byte CheatHealthSeq[] = {
    CHEAT_ENCRYPT('c'),
    CHEAT_ENCRYPT('l'),
    CHEAT_ENCRYPT('u'),
    CHEAT_ENCRYPT('b'),
    CHEAT_ENCRYPT('m'),
    CHEAT_ENCRYPT('e'),
    CHEAT_ENCRYPT('d'),
    0xff
};

// Get all keys
static byte CheatKeysSeq[] = {
    CHEAT_ENCRYPT('l'),
    CHEAT_ENCRYPT('o'),
    CHEAT_ENCRYPT('c'),
    CHEAT_ENCRYPT('k'),
    CHEAT_ENCRYPT('s'),
    CHEAT_ENCRYPT('m'),
    CHEAT_ENCRYPT('i'),
    CHEAT_ENCRYPT('t'),
    CHEAT_ENCRYPT('h'),
    0xff, 0
};

// Toggle sound debug info
static byte CheatSoundSeq[] = {
    CHEAT_ENCRYPT('n'),
    CHEAT_ENCRYPT('o'),
    CHEAT_ENCRYPT('i'),
    CHEAT_ENCRYPT('s'),
    CHEAT_ENCRYPT('e'),
    0xff
};

// Toggle ticker
static byte CheatTickerSeq[] = {
    CHEAT_ENCRYPT('t'),
    CHEAT_ENCRYPT('i'),
    CHEAT_ENCRYPT('c'),
    CHEAT_ENCRYPT('k'),
    CHEAT_ENCRYPT('e'),
    CHEAT_ENCRYPT('r'),
    0xff, 0
};

// Get all artifacts
static byte CheatArtifactAllSeq[] = {
    CHEAT_ENCRYPT('i'),
    CHEAT_ENCRYPT('n'),
    CHEAT_ENCRYPT('d'),
    CHEAT_ENCRYPT('i'),
    CHEAT_ENCRYPT('a'),
    CHEAT_ENCRYPT('n'),
    CHEAT_ENCRYPT('a'),
    0xff, 0
};

// Get all puzzle pieces
static byte CheatPuzzleSeq[] = {
    CHEAT_ENCRYPT('s'),
    CHEAT_ENCRYPT('h'),
    CHEAT_ENCRYPT('e'),
    CHEAT_ENCRYPT('r'),
    CHEAT_ENCRYPT('l'),
    CHEAT_ENCRYPT('o'),
    CHEAT_ENCRYPT('c'),
    CHEAT_ENCRYPT('k'),
    0xff, 0
};

// Warp to new level
static byte CheatWarpSeq[] = {
    CHEAT_ENCRYPT('v'),
    CHEAT_ENCRYPT('i'),
    CHEAT_ENCRYPT('s'),
    CHEAT_ENCRYPT('i'),
    CHEAT_ENCRYPT('t'),
    0, 0, 0xff, 0
};

// Become a pig
static byte CheatPigSeq[] = {
    CHEAT_ENCRYPT('d'),
    CHEAT_ENCRYPT('e'),
    CHEAT_ENCRYPT('l'),
    CHEAT_ENCRYPT('i'),
    CHEAT_ENCRYPT('v'),
    CHEAT_ENCRYPT('e'),
    CHEAT_ENCRYPT('r'),
    CHEAT_ENCRYPT('a'),
    CHEAT_ENCRYPT('n'),
    CHEAT_ENCRYPT('c'),
    CHEAT_ENCRYPT('e'),
    0xff, 0
};

// Kill all monsters
static byte CheatMassacreSeq[] = {
    CHEAT_ENCRYPT('b'),
    CHEAT_ENCRYPT('u'),
    CHEAT_ENCRYPT('t'),
    CHEAT_ENCRYPT('c'),
    CHEAT_ENCRYPT('h'),
    CHEAT_ENCRYPT('e'),
    CHEAT_ENCRYPT('r'),
    0xff, 0
};

static byte CheatIDKFASeq[] = {
    CHEAT_ENCRYPT('c'),
    CHEAT_ENCRYPT('o'),
    CHEAT_ENCRYPT('n'),
    CHEAT_ENCRYPT('a'),
    CHEAT_ENCRYPT('n'),
    0xff, 0
};

static byte CheatQuickenSeq1[] = {
    CHEAT_ENCRYPT('m'),
    CHEAT_ENCRYPT('a'),
    CHEAT_ENCRYPT('r'),
    CHEAT_ENCRYPT('t'),
    CHEAT_ENCRYPT('e'),
    CHEAT_ENCRYPT('k'),
    0xff, 0
};

static byte CheatQuickenSeq2[] = {
    CHEAT_ENCRYPT('m'),
    CHEAT_ENCRYPT('a'),
    CHEAT_ENCRYPT('r'),
    CHEAT_ENCRYPT('t'),
    CHEAT_ENCRYPT('e'),
    CHEAT_ENCRYPT('k'),
    CHEAT_ENCRYPT('m'),
    CHEAT_ENCRYPT('a'),
    CHEAT_ENCRYPT('r'),
    CHEAT_ENCRYPT('t'),
    CHEAT_ENCRYPT('e'),
    CHEAT_ENCRYPT('k'),
    0xff, 0
};

static byte CheatQuickenSeq3[] = {
    CHEAT_ENCRYPT('m'),
    CHEAT_ENCRYPT('a'),
    CHEAT_ENCRYPT('r'),
    CHEAT_ENCRYPT('t'),
    CHEAT_ENCRYPT('e'),
    CHEAT_ENCRYPT('k'),
    CHEAT_ENCRYPT('m'),
    CHEAT_ENCRYPT('a'),
    CHEAT_ENCRYPT('r'),
    CHEAT_ENCRYPT('t'),
    CHEAT_ENCRYPT('e'),
    CHEAT_ENCRYPT('k'),
    CHEAT_ENCRYPT('m'),
    CHEAT_ENCRYPT('a'),
    CHEAT_ENCRYPT('r'),
    CHEAT_ENCRYPT('t'),
    CHEAT_ENCRYPT('e'),
    CHEAT_ENCRYPT('k'),
    0xff, 0
};

// New class
static byte CheatClass1Seq[] = {
    CHEAT_ENCRYPT('s'),
    CHEAT_ENCRYPT('h'),
    CHEAT_ENCRYPT('a'),
    CHEAT_ENCRYPT('d'),
    CHEAT_ENCRYPT('o'),
    CHEAT_ENCRYPT('w'),
    CHEAT_ENCRYPT('c'),
    CHEAT_ENCRYPT('a'),
    CHEAT_ENCRYPT('s'),
    CHEAT_ENCRYPT('t'),
    CHEAT_ENCRYPT('e'),
    CHEAT_ENCRYPT('r'),
    0xff, 0
};

static byte CheatClass2Seq[] = {
    CHEAT_ENCRYPT('s'),
    CHEAT_ENCRYPT('h'),
    CHEAT_ENCRYPT('a'),
    CHEAT_ENCRYPT('d'),
    CHEAT_ENCRYPT('o'),
    CHEAT_ENCRYPT('w'),
    CHEAT_ENCRYPT('c'),
    CHEAT_ENCRYPT('a'),
    CHEAT_ENCRYPT('s'),
    CHEAT_ENCRYPT('t'),
    CHEAT_ENCRYPT('e'),
    CHEAT_ENCRYPT('r'),
    0, 0xff, 0
};

static byte CheatInitSeq[] = {
    CHEAT_ENCRYPT('i'),
    CHEAT_ENCRYPT('n'),
    CHEAT_ENCRYPT('i'),
    CHEAT_ENCRYPT('t'),
    0xff, 0
};

static byte CheatVersionSeq[] = {
    CHEAT_ENCRYPT('m'),
    CHEAT_ENCRYPT('r'),
    CHEAT_ENCRYPT('j'),
    CHEAT_ENCRYPT('o'),
    CHEAT_ENCRYPT('n'),
    CHEAT_ENCRYPT('e'),
    CHEAT_ENCRYPT('s'),
    0xff, 0
};

static byte CheatDebugSeq[] = {
    CHEAT_ENCRYPT('w'),
    CHEAT_ENCRYPT('h'),
    CHEAT_ENCRYPT('e'),
    CHEAT_ENCRYPT('r'),
    CHEAT_ENCRYPT('e'),
    0xff, 0
};

static byte CheatScriptSeq1[] = {
    CHEAT_ENCRYPT('p'),
    CHEAT_ENCRYPT('u'),
    CHEAT_ENCRYPT('k'),
    CHEAT_ENCRYPT('e'),
    0xff, 0
};

static byte CheatScriptSeq2[] = {
    CHEAT_ENCRYPT('p'),
    CHEAT_ENCRYPT('u'),
    CHEAT_ENCRYPT('k'),
    CHEAT_ENCRYPT('e'),
    0, 0xff, 0
};

static byte CheatScriptSeq3[] = {
    CHEAT_ENCRYPT('p'),
    CHEAT_ENCRYPT('u'),
    CHEAT_ENCRYPT('k'),
    CHEAT_ENCRYPT('e'),
    0, 0, 0xff,
};

static byte CheatRevealSeq[] = {
    CHEAT_ENCRYPT('m'),
    CHEAT_ENCRYPT('a'),
    CHEAT_ENCRYPT('p'),
    CHEAT_ENCRYPT('s'),
    CHEAT_ENCRYPT('c'),
    CHEAT_ENCRYPT('o'),
    0xff, 0
};

static byte CheatTrackSeq1[] = {
    //CHEAT_ENCRYPT('`'),
    CHEAT_ENCRYPT('c'),
    CHEAT_ENCRYPT('d'),
    CHEAT_ENCRYPT('t'),
    0xff, 0
};

static byte CheatTrackSeq2[] = {
    //CHEAT_ENCRYPT('`'),
    CHEAT_ENCRYPT('c'),
    CHEAT_ENCRYPT('d'),
    CHEAT_ENCRYPT('t'),
    0, 0, 0xff, 0
};

static char cheat_kills[] = { 'k', 'i', 'l', 'l', 's' };

static cheat_t Cheats[] = {
    {CheatTrackFunc1, CheatTrackSeq1, NULL, {0, 0}, 0},
    {CheatTrackFunc2, CheatTrackSeq2, NULL, {0, 0}, 0},
    {CheatGodFunc, CheatGodSeq, NULL, {0, 0}, 0},
    {CheatNoClipFunc, CheatNoClipSeq, NULL, {0, 0}, 0},
    {CheatWeaponsFunc, CheatWeaponsSeq, NULL, {0, 0}, 0},
    {CheatHealthFunc, CheatHealthSeq, NULL, {0, 0}, 0},
    {CheatKeysFunc, CheatKeysSeq, NULL, {0, 0}, 0},
    {CheatSoundFunc, CheatSoundSeq, NULL, {0, 0}, 0},
    {CheatTickerFunc, CheatTickerSeq, NULL, {0, 0}, 0},
    {CheatArtifactAllFunc, CheatArtifactAllSeq, NULL, {0, 0}, 0},
    {CheatPuzzleFunc, CheatPuzzleSeq, NULL, {0, 0}, 0},
    {CheatWarpFunc, CheatWarpSeq, NULL, {0, 0}, 0},
    {CheatPigFunc, CheatPigSeq, NULL, {0, 0}, 0},
    {CheatMassacreFunc, CheatMassacreSeq, NULL, {0, 0}, 0},
    {CheatIDKFAFunc, CheatIDKFASeq, NULL, {0, 0}, 0},
    {CheatQuickenFunc1, CheatQuickenSeq1, NULL, {0, 0}, 0},
    {CheatQuickenFunc2, CheatQuickenSeq2, NULL, {0, 0}, 0},
    {CheatQuickenFunc3, CheatQuickenSeq3, NULL, {0, 0}, 0},
    {CheatClassFunc1, CheatClass1Seq, NULL, {0, 0}, 0},
    {CheatClassFunc2, CheatClass2Seq, NULL, {0, 0}, 0},
    {CheatInitFunc, CheatInitSeq, NULL, {0, 0}, 0},
    {CheatVersionFunc, CheatVersionSeq, NULL, {0, 0}, 0},
    {CheatDebugFunc, CheatDebugSeq, NULL, {0, 0}, 0},
    {CheatScriptFunc1, CheatScriptSeq1, NULL, {0, 0}, 0},
    {CheatScriptFunc2, CheatScriptSeq2, NULL, {0, 0}, 0},
    {CheatScriptFunc3, CheatScriptSeq3, NULL, {0, 0}, 0},
    {CheatRevealFunc, CheatRevealSeq, NULL, {0, 0}, 0},
    {NULL, NULL, NULL, {0, 0}, 0}   // Terminator
};

// CODE --------------------------------------------------------------------

void cht_Init(void)
{
    int     i;

    for(i = 0; i < 256; i++)
    {
        CheatLookup[i] = CHEAT_ENCRYPT(i);
    }
}

/*
 * Responds to user input to see if a cheat sequence has been entered.
 *
 * @param       ev          ptr to the event to be checked
 * @returnval   boolean     (True) if the caller should eat the key.
 */
boolean cht_Responder(event_t *ev)
{
    int     i;
    byte    key = ev->data1;
    boolean eat;

    if(gamestate != GS_LEVEL || ev->type != ev_keydown)
        return false;

    if(gameskill == sk_nightmare)
    {   // Can't cheat in nightmare mode
        return false;
    }
    else if(IS_NETGAME)
    {   // change CD track is the only cheat available in deathmatch
        eat = false;
        /*      if(i_CDMusic)
           {
           if(CheatAddKey(&Cheats[0], key, &eat))
           {
           Cheats[0].func(&players[consoleplayer], &Cheats[0]);
           S_StartSound(SFX_PLATFORM_STOP, NULL);
           }
           if(CheatAddKey(&Cheats[1], key, &eat))
           {
           Cheats[1].func(&players[consoleplayer], &Cheats[1]);
           S_StartSound(SFX_PLATFORM_STOP, NULL);
           }
           } */
        return eat;
    }

    if(players[consoleplayer].health <= 0)
    {                           // Dead players can't cheat
        return false;
    }

    eat = false;
    for(i = 0; Cheats[i].func != NULL; i++)
    {
        if(CheatAddKey(&Cheats[i], key, &eat))
        {
            Cheats[i].func(&players[consoleplayer], &Cheats[i]);
            S_StartSound(SFX_PLATFORM_STOP, NULL);
        }
    }

    if(automapactive)
    {
        if(ev->type == ev_keydown)
        {
            if(cheat_kills[ShowKillsCount] == ev->data1 && IS_NETGAME
               && deathmatch)
            {
                ShowKillsCount++;
                if(ShowKillsCount == 5)
                {
                    ShowKillsCount = 0;
                    ShowKills ^= 1;
                }
            }
            else
            {
                ShowKillsCount = 0;
            }
            return false;
        }
        else if(ev->type == ev_keyup)
        {
            return false;
        }
        else if(ev->type == ev_keyrepeat)
            return true;
    }

    return eat;
}

static boolean canCheat()
{
    if(IS_NETGAME && !IS_CLIENT && netSvAllowCheats)
        return true;

#ifdef _DEBUG
    return true;
#else
    return !(gameskill == sk_nightmare || (IS_NETGAME && !netcheat) ||
             players[consoleplayer].health <= 0);
#endif
}


/*
 * Returns true if the added key completed the cheat, false otherwise.
 */
static boolean CheatAddKey(cheat_t * cheat, byte key, boolean *eat)
{
    if(!cheat->pos)
    {
        cheat->pos = cheat->sequence;
        cheat->currentArg = 0;
    }
    if(*cheat->pos == 0)
    {
        *eat = true;
        cheat->args[cheat->currentArg++] = key;
        cheat->pos++;
    }
    else if(CheatLookup[key] == *cheat->pos)
    {
        cheat->pos++;
    }
    else
    {
        cheat->pos = cheat->sequence;
        cheat->currentArg = 0;
    }
    if(*cheat->pos == 0xff)
    {
        cheat->pos = cheat->sequence;
        cheat->currentArg = 0;
        return (true);
    }
    return (false);
}

void cht_GodFunc(player_t *player)
{
    CheatGodFunc(player, NULL);
}

void cht_NoClipFunc(player_t *player)
{
    CheatNoClipFunc(player, NULL);
}

static void CheatGodFunc(player_t *player, cheat_t * cheat)
{
    player->cheats ^= CF_GODMODE;
    player->update |= PSF_STATE;
    if(player->cheats & CF_GODMODE)
    {
        P_SetMessage(player, TXT_CHEATGODON);
    }
    else
    {
        P_SetMessage(player, TXT_CHEATGODOFF);
    }
    SB_state = -1;
}

static void CheatNoClipFunc(player_t *player, cheat_t * cheat)
{
    player->cheats ^= CF_NOCLIP;
    player->update |= PSF_STATE;
    if(player->cheats & CF_NOCLIP)
    {
        P_SetMessage(player, TXT_CHEATNOCLIPON);
    }
    else
    {
        P_SetMessage(player, TXT_CHEATNOCLIPOFF);
    }
}

void cht_SuicideFunc(player_t *plyr)
{
    P_DamageMobj(plyr->plr->mo, NULL, NULL, 10000);
}

boolean SuicideResponse(int option, void *data)
{
    if(messageResponse == 1) // Yes
    {
        GL_Update(DDUF_BORDER);
        M_StopMessage();
        M_ClearMenus();
        cht_SuicideFunc(&players[consoleplayer]);
        return true;
    }
    else if(messageResponse == -1 || messageResponse == -2)
    {
        M_StopMessage();
        M_ClearMenus();
        return true;
    }
    return false;
}

static void CheatWeaponsFunc(player_t *player, cheat_t * cheat)
{
    int     i;

    player->update |= PSF_ARMOR_POINTS | PSF_OWNED_WEAPONS | PSF_AMMO;

    for(i = 0; i < NUMARMOR; i++)
    {
        player->armorpoints[i] = PCLASS_INFO(player->class)->armorincrement[i];
    }

    for(i = 0; i < NUMWEAPONS; i++)
    {
        player->weaponowned[i] = true;
    }

    for(i = 0; i < NUMAMMO; i++)
    {
        player->ammo[i] = MAX_MANA;
    }

    P_SetMessage(player, TXT_CHEATWEAPONS);
}

static void CheatHealthFunc(player_t *player, cheat_t * cheat)
{
    player->update |= PSF_HEALTH;
    if(player->morphTics)
    {
        player->health = player->plr->mo->health = MAXMORPHHEALTH;
    }
    else
    {
        player->health = player->plr->mo->health = MAXHEALTH;
    }
    P_SetMessage(player, TXT_CHEATHEALTH);
}

static void CheatKeysFunc(player_t *player, cheat_t * cheat)
{
    player->update |= PSF_KEYS;
    player->keys = 2047;
    P_SetMessage(player, TXT_CHEATKEYS);
}

static void CheatSoundFunc(player_t *player, cheat_t * cheat)
{
    DebugSound = !DebugSound;
    if(DebugSound)
    {
        P_SetMessage(player, TXT_CHEATSOUNDON);
    }
    else
    {
        P_SetMessage(player, TXT_CHEATSOUNDOFF);
    }
}

static void CheatTickerFunc(player_t *player, cheat_t * cheat)
{
    /*  extern int DisplayTicker;

       DisplayTicker = !DisplayTicker;
       if(DisplayTicker)
       {
       P_SetMessage(player, TXT_CHEATTICKERON);
       }
       else
       {
       P_SetMessage(player, TXT_CHEATTICKEROFF);
       } */
}

static void CheatArtifactAllFunc(player_t *player, cheat_t * cheat)
{
    int     i;
    int     j;

    for(i = arti_none + 1; i < arti_firstpuzzitem; i++)
    {
        for(j = 0; j < 25; j++)
        {
            P_GiveArtifact(player, i, NULL);
        }
    }
    P_SetMessage(player, TXT_CHEATARTIFACTS3);
}

static void CheatPuzzleFunc(player_t *player, cheat_t * cheat)
{
    int     i;

    for(i = arti_firstpuzzitem; i < NUMARTIFACTS; i++)
    {
        P_GiveArtifact(player, i, NULL);
    }
    P_SetMessage(player, TXT_CHEATARTIFACTS3);
}

static void CheatInitFunc(player_t *player, cheat_t * cheat)
{
    G_DeferedInitNew(gameskill, gameepisode, gamemap);
    P_SetMessage(player, TXT_CHEATWARP);
}

static void CheatWarpFunc(player_t *player, cheat_t * cheat)
{
    int     tens;
    int     ones;
    int     map;
    char    mapName[9];
    char    auxName[128];
    FILE   *fp;

    tens = cheat->args[0] - '0';
    ones = cheat->args[1] - '0';
    if(tens < 0 || tens > 9 || ones < 0 || ones > 9)
    {   // Bad map
        P_SetMessage(player, TXT_CHEATBADINPUT);
        return;
    }
    //map = P_TranslateMap((cheat->args[0]-'0')*10+cheat->args[1]-'0');
    map = P_TranslateMap(tens * 10 + ones);
    if(map == -1)
    {   // Not found
        P_SetMessage(player, TXT_CHEATNOMAP);
        return;
    }
    if(map == gamemap)
    {   // Don't try to teleport to current map
        P_SetMessage(player, TXT_CHEATBADINPUT);
        return;
    }
    if(DevMaps)
    {   // Search map development directory
        sprintf(auxName, "%sMAP%02d.WAD", DevMapsDir, map);
        fp = fopen(auxName, "rb");
        if(fp)
        {
            fclose(fp);
        }
        else
        {   // Can't find
            P_SetMessage(player, TXT_CHEATNOMAP);
            return;
        }
    }
    else
    {   // Search primary lumps
        sprintf(mapName, "MAP%02d", map);
        if(W_CheckNumForName(mapName) == -1)
        {   // Can't find
            P_SetMessage(player, TXT_CHEATNOMAP);
            return;
        }
    }
    P_SetMessage(player, TXT_CHEATWARP);

    M_ClearMenus();
    G_TeleportNewMap(map, 0);
}

static void CheatPigFunc(player_t *player, cheat_t * cheat)
{
    extern boolean P_UndoPlayerMorph(player_t *player);

    if(player->morphTics)
    {
        P_UndoPlayerMorph(player);
    }
    else
    {
        P_MorphPlayer(player);
    }
    P_SetMessage(player, "SQUEAL!!");
}

static void CheatMassacreFunc(player_t *player, cheat_t * cheat)
{
    int     count;
    char    buffer[80];

    count = P_Massacre();
    sprintf(buffer, "%d MONSTERS KILLED\n", count);
    P_SetMessage(player, buffer);
}

static void CheatIDKFAFunc(player_t *player, cheat_t * cheat)
{
    int     i;

    if(player->morphTics)
    {
        return;
    }
    for(i = 1; i < 8; i++)
    {
        player->weaponowned[i] = false;
    }
    player->pendingweapon = WP_FIRST;
    P_SetMessage(player, TXT_CHEATIDKFA);
}

static void CheatQuickenFunc1(player_t *player, cheat_t * cheat)
{
    P_SetMessage(player, "TRYING TO CHEAT?  THAT'S ONE....");
}

static void CheatQuickenFunc2(player_t *player, cheat_t * cheat)
{
    P_SetMessage(player, "THAT'S TWO....");
}

static void CheatQuickenFunc3(player_t *player, cheat_t * cheat)
{
    P_DamageMobj(player->plr->mo, NULL, player->plr->mo, 10000);
    P_SetMessage(player, "THAT'S THREE!  TIME TO DIE.");
}

static void CheatClassFunc1(player_t *player, cheat_t * cheat)
{
    P_SetMessage(player, "ENTER NEW PLAYER CLASS (0 - 2)");
}

static void CheatClassFunc2(player_t *player, cheat_t * cheat)
{
    int     class;

    if(player->morphTics)
    {                           // don't change class if the player is morphed
        return;
    }
    class = cheat->args[0] - '0';
    if(class > 2 || class < 0)
    {
        P_SetMessage(player, "INVALID PLAYER CLASS");
        return;
    }
    SB_ChangePlayerClass(player, class);
}

static void CheatVersionFunc(player_t *player, cheat_t * cheat)
{
    P_SetMessage(player, VERSIONTEXT);
}

static void CheatDebugFunc(player_t *player, cheat_t * cheat)
{
    char    textBuffer[256];
    subsector_t *sub;

    if(!player->plr->mo)
        return;

    sprintf(textBuffer, "MAP %d (%d)  X:%5d  Y:%5d  Z:%5d",
            P_GetMapWarpTrans(gamemap), gamemap,
            player->plr->mo->pos[VX] >> FRACBITS,
            player->plr->mo->pos[VY] >> FRACBITS,
            player->plr->mo->pos[VZ] >> FRACBITS);
    P_SetMessage(player, textBuffer);

    // Also print some information to the console.
    Con_Message(textBuffer);
    sub = player->plr->mo->subsector;
    Con_Message("\nSubsector %i:\n", P_ToIndex(sub));
    Con_Message("  Floorz:%d pic:%d\n", P_GetIntp(sub, DMU_FLOOR_HEIGHT),
                P_GetIntp(sub, DMU_FLOOR_TEXTURE));
    Con_Message("  Ceilingz:%d pic:%d\n", P_GetIntp(sub, DMU_CEILING_HEIGHT),
                P_GetIntp(sub, DMU_CEILING_TEXTURE));
    Con_Message("Player height:%x   Player radius:%x\n",
                player->plr->mo->height, player->plr->mo->radius);
}

static void CheatScriptFunc1(player_t *player, cheat_t * cheat)
{
    P_SetMessage(player, "RUN WHICH SCRIPT(01-99)?");
}

static void CheatScriptFunc2(player_t *player, cheat_t * cheat)
{
    P_SetMessage(player, "RUN WHICH SCRIPT(01-99)?");
}

static void CheatScriptFunc3(player_t *player, cheat_t * cheat)
{
    int     script;
    byte    args[3];
    int     tens, ones;
    char    textBuffer[40];

    tens = cheat->args[0] - '0';
    ones = cheat->args[1] - '0';
    script = tens * 10 + ones;
    if(script < 1)
        return;
    if(script > 99)
        return;
    args[0] = args[1] = args[2] = 0;

    if(P_StartACS(script, 0, args, player->plr->mo, NULL, 0))
    {
        sprintf(textBuffer, "RUNNING SCRIPT %.2d", script);
        P_SetMessage(player, textBuffer);
    }
}

static void CheatRevealFunc(player_t *player, cheat_t * cheat)
{
    cheating = (cheating + 1) % 4;
}

static void CheatTrackFunc1(player_t *player, cheat_t * cheat)
{
    /*  char buffer[80];

       if(!i_CDMusic)
       {
       return;
       }
       if(gi.CD(DD_INIT, 0) == -1)
       {
       P_SetMessage(player, "ERROR INITIALIZING CD");
       }
       sprintf(buffer, "ENTER DESIRED CD TRACK (%.2d - %.2d):\n",
       gi.CD(DD_GET_FIRST_TRACK, 0), gi.CD(DD_GET_LAST_TRACK, 0));
       P_SetMessage(player, buffer); */
}

static void CheatTrackFunc2(player_t *player, cheat_t * cheat)
{
    /*  char buffer[80];
       int track;

       if(!i_CDMusic)
       {
       return;
       }
       track = (cheat->args[0]-'0')*10+(cheat->args[1]-'0');
       if(track < gi.CD(DD_GET_FIRST_TRACK, 0) || track > gi.CD(DD_GET_LAST_TRACK, 0))
       {
       P_SetMessage(player, "INVALID TRACK NUMBER\n");
       return;
       }
       if(track == gi.CD(DD_GET_CURRENT_TRACK,0))
       {
       return;
       }
       if(gi.CD(DD_PLAY_LOOP, track))
       {
       sprintf(buffer, "ERROR WHILE TRYING TO PLAY CD TRACK: %.2d\n", track);
       P_SetMessage(player, buffer);
       }
       else
       { // No error encountered while attempting to play the track
       sprintf(buffer, "PLAYING TRACK: %.2d\n", track);
       P_SetMessage(player, buffer);
       } */
}

/*
 * This is the multipurpose cheat ccmd.
 */
DEFCC(CCmdCheat)
{
    unsigned int i;

    if(argc != 2)
    {
        // Usage information.
        Con_Printf("Usage: cheat (cheat)\nFor example, 'cheat visit21'.\n");
        return true;
    }
    // Give each of the characters in argument two to the SB event handler.
    for(i = 0; i < strlen(argv[1]); i++)
    {
        event_t ev;

        ev.type = ev_keydown;
        ev.data1 = argv[1][i];
        ev.data2 = ev.data3 = 0;
        cht_Responder(&ev);
    }
    return true;
}

DEFCC(CCmdCheatGod)
{
    if(IS_NETGAME)
    {
        NetCl_CheatRequest("god");
        return true;
    }
    if(!canCheat())
        return false;           // Can't cheat!
    CheatGodFunc(players + consoleplayer, NULL);
    return true;
}

DEFCC(CCmdCheatClip)
{
    if(IS_NETGAME)
    {
        NetCl_CheatRequest("noclip");
        return true;
    }
    if(!canCheat())
        return false;           // Can't cheat!
    CheatNoClipFunc(players + consoleplayer, NULL);
    return true;
}

DEFCC(CCmdCheatSuicide)
{
    if(gamestate != GS_LEVEL)
    {
        S_LocalSound(SFX_CHAT, NULL);
        Con_Printf("Can only suicide when in a game!\n");
        return true;
    }

    if(IS_NETGAME)
    {
        NetCl_CheatRequest("suicide");
    }
    else
    {
        // When not in a netgame we'll ask the player to confirm.
        Con_Open(false);
        menuactive = false;
        M_StartMessage("Are you sure you want to suicide?\n\nPress Y or N.",
                       SuicideResponse, true);
    }
    return true;
}

DEFCC(CCmdCheatGive)
{
    char    buf[100];
    int     i;
    int     weapNum;
    player_t *plyr = &players[consoleplayer];

    if(IS_CLIENT)
    {
        char    buf[100];

        if(argc != 2)
            return false;
        sprintf(buf, "give %s", argv[1]);
        NetCl_CheatRequest(buf);
        return true;
    }

    if(!canCheat())
        return false;           // Can't cheat!

    if(argc != 2 && argc != 3)
    {
        Con_Printf("Usage:\n  give (stuff)\n");
        Con_Printf("  give (stuff) (player)\n");
        Con_Printf("Stuff consists of one or more of:\n");
        Con_Printf(" a - artifacts\n");
        Con_Printf(" h - health\n");
        Con_Printf(" k - keys\n");
        Con_Printf(" p - puzzle\n");
        Con_Printf(" w - weapons\n");
        Con_Printf(" 0-4 - weapon\n");
        return true;
    }
    if(argc == 3)
    {
        i = atoi(argv[2]);
        if(i < 0 || i >= MAXPLAYERS || !players[i].plr->ingame)
            return false;
        plyr = &players[i];
    }
    strcpy(buf, argv[1]);       // Stuff is the 2nd arg.
    strlwr(buf);
    for(i = 0; buf[i]; i++)
    {
        switch (buf[i])
        {
        case 'a':
            Con_Printf("Artifacts given.\n");
            CheatArtifactAllFunc(plyr, NULL);
            break;

        case 'h':
            Con_Printf("Health given.\n");
            CheatHealthFunc(plyr, NULL);
            break;

        case 'k':
            Con_Printf("Keys given.\n");
            CheatKeysFunc(plyr, NULL);
            break;

        case 'p':
            Con_Printf("Puzzle parts given.\n");
            CheatPuzzleFunc(plyr, NULL);
            break;

        case 'w':
            Con_Printf("Weapons given.\n");
            CheatWeaponsFunc(plyr, NULL);
            break;

        default:
            // Individual Weapon
            weapNum = ((int) buf[i]) - 48;
            if(weapNum >= 0 && weapNum < NUMWEAPONS)
            {
               plyr->weaponowned[weapNum] = true;
            }
            else// Unrecognized
                Con_Printf("What do you mean, '%c'?\n", buf[i]);
        }
    }

    return true;
}

DEFCC(CCmdCheatWarp)
{
    cheat_t cheat;
    int     num;

    if(!canCheat())
        return false;           // Can't cheat!

    if(argc != 2)
    {
        Con_Printf("Usage: warp (num)\n");
        return true;
    }
    num = atoi(argv[1]);
    cheat.args[0] = num / 10 + '0';
    cheat.args[1] = num % 10 + '0';
    // We don't want that keys are repeated while we wait.
    DD_ClearKeyRepeaters();
    CheatWarpFunc(players + consoleplayer, &cheat);
    return true;
}

DEFCC(CCmdCheatPig)
{
    if(!canCheat())
        return false;           // Can't cheat!
    CheatPigFunc(players + consoleplayer, NULL);
    return true;
}

DEFCC(CCmdCheatMassacre)
{
    if(!canCheat())
        return false;           // Can't cheat!
    DD_ClearKeyRepeaters();
    CheatMassacreFunc(players + consoleplayer, NULL);
    return true;
}

DEFCC(CCmdCheatShadowcaster)
{
    cheat_t cheat;

    if(!canCheat())
        return false;           // Can't cheat!
    if(argc != 2)
    {
        Con_Printf("Usage: class (0-2)\n");
        Con_Printf("0=Fighter, 1=Cleric, 2=Mage.\n");
        return true;
    }
    cheat.args[0] = atoi(argv[1]) + '0';
    CheatClassFunc2(players + consoleplayer, &cheat);
    return true;
}

DEFCC(CCmdCheatWhere)
{
    //if(!canCheat()) return false; // Can't cheat!
    CheatDebugFunc(players + consoleplayer, NULL);
    return true;
}

DEFCC(CCmdCheatRunScript)
{
    cheat_t cheat;
    int     num;

    if(!canCheat())
        return false;           // Can't cheat!
    if(argc != 2)
    {
        Con_Printf("Usage: runscript (1-99)\n");
        return true;
    }
    num = atoi(argv[1]);
    cheat.args[0] = num / 10 + '0';
    cheat.args[1] = num % 10 + '0';
    CheatScriptFunc3(players + consoleplayer, &cheat);
    return true;
}

DEFCC(CCmdCheatReveal)
{
    int     option;

    if(!canCheat())
        return false;           // Can't cheat!
    if(argc != 2)
    {
        Con_Printf("Usage: reveal (0-4)\n");
        Con_Printf("0=nothing, 1=show unseen, 2=full map, 3=map+things, 4=show subsectors\n");
        return true;
    }
    // Reset them (for 'nothing'). :-)
    cheating = 0;
    players[consoleplayer].powers[pw_allmap] = false;
    option = atoi(argv[1]);
    if(option < 0 || option > 4)
        return false;
    if(option == 1)
        players[consoleplayer].powers[pw_allmap] = true;
    else if(option != 0)
        cheating = option -1;

    return true;
}
