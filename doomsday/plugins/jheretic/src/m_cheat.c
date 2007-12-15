/**\file
 *\section License
 * License: Raven
 * Online License Link: http://www.dengine.net/raven_license/End_User_License_Hexen_Source_Code.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2007 Daniel Swanson <danij@dengine.net>
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
 * m_cheat.c: Cheat sequence checking.
 */

// HEADER FILES ------------------------------------------------------------

#include "jheretic.h"

#include "f_infine.h"
#include "d_net.h"
#include "g_common.h"
#include "p_player.h"
#include "p_inventory.h"
#include "am_map.h"
#include "hu_menu.h"

// MACROS ------------------------------------------------------------------

#define CHEAT_ENCRYPT(a) \
    ((((a)&1)<<5)+ \
    (((a)&2)<<1)+ \
    (((a)&4)<<4)+ \
    (((a)&8)>>3)+ \
    (((a)&16)>>3)+ \
    (((a)&32)<<2)+ \
    (((a)&64)>>2)+ \
    (((a)&128)>>4))

// TYPES -------------------------------------------------------------------

typedef struct Cheat_s {
    void    (*func) (player_t *player, struct Cheat_s *cheat);
    byte   *sequence;
    byte   *pos;
    int     args[2];
    int     currentArg;
} Cheat_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

boolean Cht_Responder(event_t *ev);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static boolean CheatAddKey(Cheat_t * cheat, byte key, boolean *eat);
static void CheatGodFunc(player_t *player, Cheat_t * cheat);
static void CheatNoClipFunc(player_t *player, Cheat_t * cheat);
static void CheatWeaponsFunc(player_t *player, Cheat_t * cheat);
static void CheatPowerFunc(player_t *player, Cheat_t * cheat);
static void CheatHealthFunc(player_t *player, Cheat_t * cheat);
static void CheatKeysFunc(player_t *player, Cheat_t * cheat);
static void CheatSoundFunc(player_t *player, Cheat_t * cheat);
static void CheatTickerFunc(player_t *player, Cheat_t * cheat);
static void CheatArtifact1Func(player_t *player, Cheat_t * cheat);
static void CheatArtifact2Func(player_t *player, Cheat_t * cheat);
static void CheatArtifact3Func(player_t *player, Cheat_t * cheat);
static void CheatWarpFunc(player_t *player, Cheat_t * cheat);
static void CheatChickenFunc(player_t *player, Cheat_t * cheat);
static void CheatMassacreFunc(player_t *player, Cheat_t * cheat);
static void CheatIDKFAFunc(player_t *player, Cheat_t * cheat);
static void CheatIDDQDFunc(player_t *player, Cheat_t * cheat);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int messageResponse;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

byte cheatcount;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static byte CheatLookup[256];

// Toggle god mode
static byte CheatGodSeq[] = {
    CHEAT_ENCRYPT('q'),
    CHEAT_ENCRYPT('u'),
    CHEAT_ENCRYPT('i'),
    CHEAT_ENCRYPT('c'),
    CHEAT_ENCRYPT('k'),
    CHEAT_ENCRYPT('e'),
    CHEAT_ENCRYPT('n'),
    0xff
};

// Toggle no clipping mode
static byte CheatNoClipSeq[] = {
    CHEAT_ENCRYPT('k'),
    CHEAT_ENCRYPT('i'),
    CHEAT_ENCRYPT('t'),
    CHEAT_ENCRYPT('t'),
    CHEAT_ENCRYPT('y'),
    0xff
};

// Get all weapons and ammo
static byte CheatWeaponsSeq[] = {
    CHEAT_ENCRYPT('r'),
    CHEAT_ENCRYPT('a'),
    CHEAT_ENCRYPT('m'),
    CHEAT_ENCRYPT('b'),
    CHEAT_ENCRYPT('o'),
    0xff
};

// Toggle tome of power
static byte CheatPowerSeq[] = {
    CHEAT_ENCRYPT('s'),
    CHEAT_ENCRYPT('h'),
    CHEAT_ENCRYPT('a'),
    CHEAT_ENCRYPT('z'),
    CHEAT_ENCRYPT('a'),
    CHEAT_ENCRYPT('m'),
    0xff, 0
};

// Get full health
static byte CheatHealthSeq[] = {
    CHEAT_ENCRYPT('p'),
    CHEAT_ENCRYPT('o'),
    CHEAT_ENCRYPT('n'),
    CHEAT_ENCRYPT('c'),
    CHEAT_ENCRYPT('e'),
    0xff
};

// Get all keys
static byte CheatKeysSeq[] = {
    CHEAT_ENCRYPT('s'),
    CHEAT_ENCRYPT('k'),
    CHEAT_ENCRYPT('e'),
    CHEAT_ENCRYPT('l'),
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

// Get an artifact 1st stage (ask for type)
static byte CheatArtifact1Seq[] = {
    CHEAT_ENCRYPT('g'),
    CHEAT_ENCRYPT('i'),
    CHEAT_ENCRYPT('m'),
    CHEAT_ENCRYPT('m'),
    CHEAT_ENCRYPT('e'),
    0xff
};

// Get an artifact 2nd stage (ask for count)
static byte CheatArtifact2Seq[] = {
    CHEAT_ENCRYPT('g'),
    CHEAT_ENCRYPT('i'),
    CHEAT_ENCRYPT('m'),
    CHEAT_ENCRYPT('m'),
    CHEAT_ENCRYPT('e'),
    0, 0xff, 0
};

// Get an artifact final stage
static byte CheatArtifact3Seq[] = {
    CHEAT_ENCRYPT('g'),
    CHEAT_ENCRYPT('i'),
    CHEAT_ENCRYPT('m'),
    CHEAT_ENCRYPT('m'),
    CHEAT_ENCRYPT('e'),
    0, 0, 0xff
};

// Warp to new level
static byte CheatWarpSeq[] = {
    CHEAT_ENCRYPT('e'),
    CHEAT_ENCRYPT('n'),
    CHEAT_ENCRYPT('g'),
    CHEAT_ENCRYPT('a'),
    CHEAT_ENCRYPT('g'),
    CHEAT_ENCRYPT('e'),
    0, 0, 0xff, 0
};

// Save a screenshot
static byte CheatChickenSeq[] = {
    CHEAT_ENCRYPT('c'),
    CHEAT_ENCRYPT('o'),
    CHEAT_ENCRYPT('c'),
    CHEAT_ENCRYPT('k'),
    CHEAT_ENCRYPT('a'),
    CHEAT_ENCRYPT('d'),
    CHEAT_ENCRYPT('o'),
    CHEAT_ENCRYPT('o'),
    CHEAT_ENCRYPT('d'),
    CHEAT_ENCRYPT('l'),
    CHEAT_ENCRYPT('e'),
    CHEAT_ENCRYPT('d'),
    CHEAT_ENCRYPT('o'),
    CHEAT_ENCRYPT('o'),
    0xff, 0
};

// Kill all monsters
static byte CheatMassacreSeq[] = {
    CHEAT_ENCRYPT('m'),
    CHEAT_ENCRYPT('a'),
    CHEAT_ENCRYPT('s'),
    CHEAT_ENCRYPT('s'),
    CHEAT_ENCRYPT('a'),
    CHEAT_ENCRYPT('c'),
    CHEAT_ENCRYPT('r'),
    CHEAT_ENCRYPT('e'),
    0xff, 0
};

static byte CheatIDKFASeq[] = {
    CHEAT_ENCRYPT('i'),
    CHEAT_ENCRYPT('d'),
    CHEAT_ENCRYPT('k'),
    CHEAT_ENCRYPT('f'),
    CHEAT_ENCRYPT('a'),
    0xff, 0
};

static byte CheatIDDQDSeq[] = {
    CHEAT_ENCRYPT('i'),
    CHEAT_ENCRYPT('d'),
    CHEAT_ENCRYPT('d'),
    CHEAT_ENCRYPT('q'),
    CHEAT_ENCRYPT('d'),
    0xff, 0
};

static char cheat_amap[] = { 'r', 'a', 'v', 'm', 'a', 'p' };

static Cheat_t Cheats[] = {
    {CheatGodFunc, CheatGodSeq, NULL, {0, 0}, 0},
    {CheatNoClipFunc, CheatNoClipSeq, NULL, {0, 0}, 0},
    {CheatWeaponsFunc, CheatWeaponsSeq, NULL, {0, 0}, 0},
    {CheatPowerFunc, CheatPowerSeq, NULL, {0, 0}, 0},
    {CheatHealthFunc, CheatHealthSeq, NULL, {0, 0}, 0},
    {CheatKeysFunc, CheatKeysSeq, NULL, {0, 0}, 0},
    {CheatSoundFunc, CheatSoundSeq, NULL, {0, 0}, 0},
    {CheatTickerFunc, CheatTickerSeq, NULL, {0, 0}, 0},
    {CheatArtifact1Func, CheatArtifact1Seq, NULL, {0, 0}, 0},
    {CheatArtifact2Func, CheatArtifact2Seq, NULL, {0, 0}, 0},
    {CheatArtifact3Func, CheatArtifact3Seq, NULL, {0, 0}, 0},
    {CheatWarpFunc, CheatWarpSeq, NULL, {0, 0}, 0},
    {CheatChickenFunc, CheatChickenSeq, NULL, {0, 0}, 0},
    {CheatMassacreFunc, CheatMassacreSeq, NULL, {0, 0}, 0},
    {CheatIDKFAFunc, CheatIDKFASeq, NULL, {0, 0}, 0},
    {CheatIDDQDFunc, CheatIDDQDSeq, NULL, {0, 0}, 0},
    {NULL, NULL, NULL, {0, 0}, 0}   // Terminator
};

// CODE --------------------------------------------------------------------

void Cht_Init(void)
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
boolean Cht_Responder(event_t *ev)
{
    int     i;
    byte key = ev->data1;
    boolean eat;

    if(G_GetGameState() != GS_LEVEL || ev->type != EV_KEY || ev->state != EVS_DOWN)
        return false;

    if(IS_NETGAME || gameskill == SM_NIGHTMARE)
    {                           // Can't cheat in a net-game, or in nightmare mode
        return false;
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
            S_LocalSound(sfx_dorcls, NULL);
        }
    }

    if(AM_IsMapActive(consoleplayer))
    {
        if(ev->state == EVS_DOWN)
        {
            if(!deathmatch && cheat_amap[cheatcount] == ev->data1)
                cheatcount++;
            else
                cheatcount = 0;

            if(cheatcount == 6)
            {
                AM_IncMapCheatLevel(consoleplayer);
            }
            return false;
        }
        else if(ev->state == EVS_UP)
            return false;
        else if(ev->state == EVS_REPEAT)
            return true;
    }

    return (eat);
}

static boolean canCheat()
{
    if(IS_NETGAME && !IS_CLIENT && netSvAllowCheats)
        return true;
    return !(gameskill == SM_NIGHTMARE || (IS_NETGAME /*&& !netcheat */ )
             || players[consoleplayer].health <= 0);
}

static boolean CheatAddKey(Cheat_t * cheat, byte key, boolean *eat)
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

void Cht_GodFunc(player_t *player)
{
    CheatGodFunc(player, NULL);
}

void Cht_NoClipFunc(player_t *player)
{
    CheatNoClipFunc(player, NULL);
}

void Cht_SuicideFunc(player_t *plyr)
{
    P_DamageMobj(plyr->plr->mo, NULL, NULL, 10000);
}

boolean SuicideResponse(int option, void *data)
{
    if(messageResponse == 1) // Yes
    {
        M_StopMessage();
        Hu_MenuCommand(MCMD_CLOSE);
        Cht_SuicideFunc(&players[consoleplayer]);
        return true;
    }
    else if(messageResponse == -1 || messageResponse == -2)
    {
        M_StopMessage();
        Hu_MenuCommand(MCMD_CLOSE);
        return true;
    }
    return false;
}

static void CheatGodFunc(player_t *player, Cheat_t * cheat)
{
    player->cheats ^= CF_GODMODE;
    player->update |= PSF_STATE;
    if(P_GetPlayerCheats(player) & CF_GODMODE)
    {
        P_SetMessage(player, TXT_CHEATGODON, false);
    }
    else
    {
        P_SetMessage(player, TXT_CHEATGODOFF, false);
    }
}

static void CheatNoClipFunc(player_t *player, Cheat_t * cheat)
{
    player->cheats ^= CF_NOCLIP;
    player->update |= PSF_STATE;
    if(P_GetPlayerCheats(player) & CF_NOCLIP)
    {
        P_SetMessage(player, TXT_CHEATNOCLIPON, false);
    }
    else
    {
        P_SetMessage(player, TXT_CHEATNOCLIPOFF, false);
    }
}

static void CheatWeaponsFunc(player_t *player, Cheat_t * cheat)
{
    int     i;

    player->update |=
        PSF_ARMOR_POINTS | PSF_STATE | PSF_MAX_AMMO | PSF_AMMO |
        PSF_OWNED_WEAPONS;

    player->armorpoints = 200;
    player->armortype = 2;
    if(!player->backpack)
    {
        for(i = 0; i < NUM_AMMO_TYPES; i++)
        {
            player->maxammo[i] *= 2;
        }
        player->backpack = true;
    }
    for(i = 0; i < NUM_WEAPON_TYPES; i++)
    {
        if(weaponinfo[i][0].mode[0].gamemodebits & gamemodebits)
            player->weaponowned[i] = true;
    }

    for(i = 0; i < NUM_AMMO_TYPES; i++)
    {
        player->ammo[i] = player->maxammo[i];
    }
    P_SetMessage(player, TXT_CHEATWEAPONS, false);
}

static void CheatPowerFunc(player_t *player, Cheat_t * cheat)
{
    player->update |= PSF_POWERS;
    if(player->powers[PT_WEAPONLEVEL2])
    {
        player->powers[PT_WEAPONLEVEL2] = 0;
        P_SetMessage(player, TXT_CHEATPOWEROFF, false);
    }
    else
    {
        P_UseArtifactOnPlayer(player, arti_tomeofpower);
        P_SetMessage(player, TXT_CHEATPOWERON, false);
    }
}

static void CheatHealthFunc(player_t *player, Cheat_t * cheat)
{
    player->update |= PSF_HEALTH;
    if(player->morphTics)
    {
        player->health = player->plr->mo->health = MAXCHICKENHEALTH;
    }
    else
    {
        player->health = player->plr->mo->health = MAXHEALTH;
    }
    P_SetMessage(player, TXT_CHEATHEALTH, false);
}

static void CheatKeysFunc(player_t *player, Cheat_t * cheat)
{
    extern int playerkeys;

    player->update |= PSF_KEYS;
    player->keys[KT_YELLOW] = true;
    player->keys[KT_GREEN] = true;
    player->keys[KT_BLUE] = true;
    playerkeys = 7;             // Key refresh flags
    P_SetMessage(player, TXT_CHEATKEYS, false);
}

static void CheatSoundFunc(player_t *player, Cheat_t * cheat)
{
    /*  DebugSound = !DebugSound;
       if(DebugSound)
       {
       P_SetMessage(player, TXT_CHEATSOUNDON, false);
       }
       else
       {
       P_SetMessage(player, TXT_CHEATSOUNDOFF, false);
       } */
}

static void CheatTickerFunc(player_t *player, Cheat_t * cheat)
{
     /*
       extern int DisplayTicker;

       DisplayTicker = !DisplayTicker;
       if(DisplayTicker)
       {
       P_SetMessage(player, TXT_CHEATTICKERON, false);
       }
       else
       {
       P_SetMessage(player, TXT_CHEATTICKEROFF, false);
       } */
}

static void CheatArtifact1Func(player_t *player, Cheat_t * cheat)
{
    P_SetMessage(player, TXT_CHEATARTIFACTS1, false);
}

static void CheatArtifact2Func(player_t *player, Cheat_t * cheat)
{
    P_SetMessage(player, TXT_CHEATARTIFACTS2, false);
}

static void CheatArtifact3Func(player_t *player, Cheat_t * cheat)
{
    int     i;
    artitype_e type;
    int     count;

    type = cheat->args[0] - 'a' + 1;
    count = cheat->args[1] - '0';
    if(type == 26 && count == 0)
    {                           // All artifacts
        for(type = arti_none + 1; type < NUMARTIFACTS; type++)
        {
            if(gamemode == shareware && (type == arti_superhealth || type == arti_teleport))
            {
                continue;
            }
            for(i = 0; i < MAXARTICOUNT; i++)
            {
                P_GiveArtifact(player, type, NULL);
            }
        }
        P_SetMessage(player, TXT_CHEATARTIFACTS3, false);
    }
    else if(type > arti_none && type < NUMARTIFACTS && count > 0 && count < 10)
    {
        if(gamemode == shareware && (type == arti_superhealth || type == arti_teleport))
        {
            P_SetMessage(player, TXT_CHEATARTIFACTSFAIL, false);
            return;
        }
        for(i = 0; i < count; i++)
        {
            P_GiveArtifact(player, type, NULL);
        }
        P_SetMessage(player, TXT_CHEATARTIFACTS3, false);
    }
    else
    {                           // Bad input
        P_SetMessage(player, TXT_CHEATARTIFACTSFAIL, false);
    }
}

static void CheatWarpFunc(player_t *player, Cheat_t * cheat)
{
    int     episode;
    int     map;

    episode = cheat->args[0] - '0';
    map = cheat->args[1] - '0';
    if(G_ValidateMap(&episode, &map))
    {
        G_DeferedInitNew(gameskill, episode, map);
        Hu_MenuCommand(MCMD_CLOSE);
        P_SetMessage(player, TXT_CHEATWARP, false);
    }
}

static void CheatChickenFunc(player_t *player, Cheat_t * cheat)
{
    extern boolean P_UndoPlayerMorph(player_t *player);

    if(player->morphTics)
    {
        if(P_UndoPlayerMorph(player))
        {
            P_SetMessage(player, TXT_CHEATCHICKENOFF, false);
        }
    }
    else if(P_MorphPlayer(player))
    {
        P_SetMessage(player, TXT_CHEATCHICKENON, false);
    }
}

static void CheatMassacreFunc(player_t *player, Cheat_t * cheat)
{
    P_Massacre();
    P_SetMessage(player, TXT_CHEATMASSACRE, false);
}

static void CheatIDKFAFunc(player_t *player, Cheat_t * cheat)
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
    player->pendingweapon = WT_FIRST;
    P_SetMessage(player, TXT_CHEATIDKFA, false);
}

static void CheatIDDQDFunc(player_t *player, Cheat_t * cheat)
{
    P_DamageMobj(player->plr->mo, NULL, player->plr->mo, 10000);
    P_SetMessage(player, TXT_CHEATIDDQD, false);
}

static void CheatDebugFunc(player_t *player, cheat_t * cheat)
{
    char    lumpName[9];
    char    textBuffer[256];
    subsector_t *sub;

    if(!player->plr->mo || !usergame)
        return;

    P_GetMapLumpName(gameepisode, gamemap, lumpName);
    sprintf(textBuffer, "MAP [%s]  X:%g  Y:%g  Z:%g",
            lumpName,
            player->plr->mo->pos[VX],
            player->plr->mo->pos[VY],
            player->plr->mo->pos[VZ]);
    P_SetMessage(player, textBuffer, false);

    // Also print some information to the console.
    Con_Message(textBuffer);
    sub = player->plr->mo->subsector;
    Con_Message("\nSubsector %i:\n", P_ToIndex(sub));
    Con_Message("  Floorz:%g material:%d\n",
                P_GetFloatp(sub, DMU_FLOOR_HEIGHT),
                P_GetIntp(sub, DMU_FLOOR_MATERIAL));
    Con_Message("  Ceilingz:%g material:%d\n",
                P_GetFloatp(sub, DMU_CEILING_HEIGHT),
                P_GetIntp(sub, DMU_CEILING_MATERIAL));
    Con_Message("Player height:%g   Player radius:%g\n",
                player->plr->mo->height, player->plr->mo->radius);
}

// This is the multipurpose cheat ccmd.
DEFCC(CCmdCheat)
{
    unsigned int i;

    // Give each of the characters in argument two to the SB event handler.
    for(i = 0; i < strlen(argv[1]); i++)
    {
        event_t ev;

        ev.type = EV_KEY;
        ev.state = EVS_DOWN;
        ev.data1 = argv[1][i];
        ev.data2 = ev.data3 = 0;
        Cht_Responder(&ev);
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
    if(G_GetGameState() != GS_LEVEL)
    {
        S_LocalSound(sfx_chat, NULL);
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
        Hu_MenuCommand(MCMD_CLOSE);
        M_StartMessage("Are you sure you want to suicide?\n\nPress Y or N.",
                       SuicideResponse, true);
    }
    return true;
}

DEFCC(CCmdCheatGive)
{
    int     tellUsage = false;
    int     target = consoleplayer;

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

    // Check the arguments.
    if(argc == 3)
    {
        target = atoi(argv[2]);
        if(target < 0 || target >= MAXPLAYERS)
            return false;
    }

    if(G_GetGameState() != GS_LEVEL)
    {
        Con_Printf("Can only \"give\" when in a game!\n");
        return true;
    }

    if(!players[target].plr->ingame)
        return true; // Can't give to a player who's not playing

    if(argc != 2 && argc != 3)
        tellUsage = true;
    else if(!strnicmp(argv[1], "weapons", 1))
        CheatWeaponsFunc(players + target, NULL);
    else if(!strnicmp(argv[1], "health", 1))
        CheatHealthFunc(players + target, NULL);
    else if(!strnicmp(argv[1], "keys", 1))
        CheatKeysFunc(players + target, NULL);
    else if(!strnicmp(argv[1], "artifacts", 1))
    {
        Cheat_t cheat;

        cheat.args[0] = 'z';
        cheat.args[1] = '0';
        CheatArtifact3Func(players + target, &cheat);
    }
    else
        tellUsage = true;

    if(tellUsage)
    {
        Con_Printf("Usage: give weapons/health/keys/artifacts\n");
        Con_Printf("The first letter is enough, e.g. 'give h'.\n");
    }
    return true;
}

DEFCC(CCmdCheatWarp)
{
    Cheat_t cheat;
    int     num;

    if(!canCheat())
        return false;           // Can't cheat!
    if(argc == 2)
    {
        num = atoi(argv[1]);
        cheat.args[0] = num / 10 + '0';
        cheat.args[1] = num % 10 + '0';
    }
    else if(argc == 3)
    {
        cheat.args[0] = atoi(argv[1]) % 10 + '0';
        cheat.args[1] = atoi(argv[2]) % 10 + '0';
    }
    else
    {
        Con_Printf("Usage: warp (num)\n");
        return true;
    }
    // We don't want that keys are repeated while we wait.
    DD_ClearKeyRepeaters();
    CheatWarpFunc(players + consoleplayer, &cheat);
    return true;
}

/*
 * Exit the current level and goto the intermission.
 */
DEFCC(CCmdCheatExitLevel)
{
    if(!canCheat())
        return false;           // Can't cheat!
    if(G_GetGameState() != GS_LEVEL)
    {
        S_LocalSound(sfx_chat, NULL);
        Con_Printf("Can only exit a level when in a game!\n");
        return true;
    }

    // Exit the level.
    G_LeaveLevel(G_GetLevelNumber(gameepisode, gamemap), 0, false);

    return true;
}

DEFCC(CCmdCheatPig)
{
    if(!canCheat())
        return false;           // Can't cheat!
    CheatChickenFunc(players + consoleplayer, NULL);
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

DEFCC(CCmdCheatWhere)
{
    if(!canCheat())
        return false;           // Can't cheat!
    CheatDebugFunc(players+consoleplayer, NULL);
    return true;
}

DEFCC(CCmdCheatReveal)
{
    int     option;

    if(!canCheat())
        return false;           // Can't cheat!

    // Reset them (for 'nothing'). :-)
    AM_SetCheatLevel(consoleplayer, 0);
    players[consoleplayer].powers[PT_ALLMAP] = false;
    option = atoi(argv[1]);
    if(option < 0 || option > 3)
        return false;

    if(option == 1)
        players[consoleplayer].powers[PT_ALLMAP] = true;
    else if(option != 0)
        AM_SetCheatLevel(consoleplayer, option -1);

    return true;
}
