/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
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
 * m_cheat.c:
 */

// HEADER FILES ------------------------------------------------------------

#include "jhexen.h"

#include "dmu_lib.h"
#include "p_inventory.h"
#include "p_player.h"
#include "p_map.h"
#include "am_map.h"
#include "g_common.h"
#include "hu_menu.h"
#include "hu_msg.h"
#include "p_user.h"

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
    CHT_INVENTORY_ITEMS,
    CHT_PUZZLE
} cheattype_t;

typedef struct cheat_s {
    void              (*func) (player_t *player, struct cheat_s * cheat);
    byte               *sequence;
    byte               *pos;
    int                 args[2];
    int                 currentArg;
} cheat_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static boolean CheatAddKey(cheat_t *cheat, byte key, boolean *eat);
static void CheatGodFunc(player_t *player, cheat_t *cheat);
static void CheatNoClipFunc(player_t *player, cheat_t *cheat);
static void CheatWeaponsFunc(player_t *player, cheat_t *cheat);
static void CheatHealthFunc(player_t *player, cheat_t *cheat);
static void CheatKeysFunc(player_t *player, cheat_t *cheat);
static void CheatSoundFunc(player_t *player, cheat_t *cheat);
static void CheatInventoryFunc(player_t *player, cheat_t *cheat);
static void CheatPuzzleFunc(player_t *player, cheat_t *cheat);
static void CheatWarpFunc(player_t *player, cheat_t *cheat);
static void CheatPigFunc(player_t *player, cheat_t *cheat);
static void CheatMassacreFunc(player_t *player, cheat_t *cheat);
static void CheatIDKFAFunc(player_t *player, cheat_t *cheat);
static void CheatQuickenFunc1(player_t *player, cheat_t *cheat);
static void CheatQuickenFunc2(player_t *player, cheat_t *cheat);
static void CheatQuickenFunc3(player_t *player, cheat_t *cheat);
static void CheatClassFunc1(player_t *player, cheat_t *cheat);
static void CheatClassFunc2(player_t *player, cheat_t *cheat);
static void CheatInitFunc(player_t *player, cheat_t *cheat);
static void CheatInitFunc(player_t *player, cheat_t *cheat);
static void CheatVersionFunc(player_t *player, cheat_t *cheat);
static void CheatDebugFunc(player_t *player, cheat_t *cheat);
static void CheatScriptFunc1(player_t *player, cheat_t *cheat);
static void CheatScriptFunc2(player_t *player, cheat_t *cheat);
static void CheatScriptFunc3(player_t *player, cheat_t *cheat);
static void CheatRevealFunc(player_t *player, cheat_t *cheat);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int ShowKillsCount;
static int ShowKills;
static byte cheatLookup[256];

// Toggle god mode
static byte cheatGodSeq[] = {
    CHEAT_ENCRYPT('s'),
    CHEAT_ENCRYPT('a'),
    CHEAT_ENCRYPT('t'),
    CHEAT_ENCRYPT('a'),
    CHEAT_ENCRYPT('n'),
    0xff
};

// Toggle no clipping mode
static byte cheatNoClipSeq[] = {
    CHEAT_ENCRYPT('c'),
    CHEAT_ENCRYPT('a'),
    CHEAT_ENCRYPT('s'),
    CHEAT_ENCRYPT('p'),
    CHEAT_ENCRYPT('e'),
    CHEAT_ENCRYPT('r'),
    0xff
};

// Get all weapons and mana
static byte cheatWeaponsSeq[] = {
    CHEAT_ENCRYPT('n'),
    CHEAT_ENCRYPT('r'),
    CHEAT_ENCRYPT('a'),
    0xff
};

// Get full health
static byte cheatHealthSeq[] = {
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
static byte cheatKeysSeq[] = {
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
static byte cheatSoundSeq[] = {
    CHEAT_ENCRYPT('n'),
    CHEAT_ENCRYPT('o'),
    CHEAT_ENCRYPT('i'),
    CHEAT_ENCRYPT('s'),
    CHEAT_ENCRYPT('e'),
    0xff
};

// Toggle ticker
static byte cheatTickerSeq[] = {
    CHEAT_ENCRYPT('t'),
    CHEAT_ENCRYPT('i'),
    CHEAT_ENCRYPT('c'),
    CHEAT_ENCRYPT('k'),
    CHEAT_ENCRYPT('e'),
    CHEAT_ENCRYPT('r'),
    0xff, 0
};

// Get all inventory items
static byte cheatInventorySeq[] = {
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
static byte cheatPuzzleSeq[] = {
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

// Warp to new map
static byte cheatWarpSeq[] = {
    CHEAT_ENCRYPT('v'),
    CHEAT_ENCRYPT('i'),
    CHEAT_ENCRYPT('s'),
    CHEAT_ENCRYPT('i'),
    CHEAT_ENCRYPT('t'),
    0, 0, 0xff, 0
};

// Become a pig
static byte cheatPigSeq[] = {
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
static byte cheatMassacreSeq[] = {
    CHEAT_ENCRYPT('b'),
    CHEAT_ENCRYPT('u'),
    CHEAT_ENCRYPT('t'),
    CHEAT_ENCRYPT('c'),
    CHEAT_ENCRYPT('h'),
    CHEAT_ENCRYPT('e'),
    CHEAT_ENCRYPT('r'),
    0xff, 0
};

static byte cheatIDKFASeq[] = {
    CHEAT_ENCRYPT('c'),
    CHEAT_ENCRYPT('o'),
    CHEAT_ENCRYPT('n'),
    CHEAT_ENCRYPT('a'),
    CHEAT_ENCRYPT('n'),
    0xff, 0
};

static byte cheatQuickenSeq1[] = {
    CHEAT_ENCRYPT('m'),
    CHEAT_ENCRYPT('a'),
    CHEAT_ENCRYPT('r'),
    CHEAT_ENCRYPT('t'),
    CHEAT_ENCRYPT('e'),
    CHEAT_ENCRYPT('k'),
    0xff, 0
};

static byte cheatQuickenSeq2[] = {
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

static byte cheatQuickenSeq3[] = {
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
static byte cheatClass1Seq[] = {
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

static byte cheatClass2Seq[] = {
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

static byte cheatInitSeq[] = {
    CHEAT_ENCRYPT('i'),
    CHEAT_ENCRYPT('n'),
    CHEAT_ENCRYPT('i'),
    CHEAT_ENCRYPT('t'),
    0xff, 0
};

static byte cheatVersionSeq[] = {
    CHEAT_ENCRYPT('m'),
    CHEAT_ENCRYPT('r'),
    CHEAT_ENCRYPT('j'),
    CHEAT_ENCRYPT('o'),
    CHEAT_ENCRYPT('n'),
    CHEAT_ENCRYPT('e'),
    CHEAT_ENCRYPT('s'),
    0xff, 0
};

static byte cheatDebugSeq[] = {
    CHEAT_ENCRYPT('w'),
    CHEAT_ENCRYPT('h'),
    CHEAT_ENCRYPT('e'),
    CHEAT_ENCRYPT('r'),
    CHEAT_ENCRYPT('e'),
    0xff, 0
};

static byte cheatScriptSeq1[] = {
    CHEAT_ENCRYPT('p'),
    CHEAT_ENCRYPT('u'),
    CHEAT_ENCRYPT('k'),
    CHEAT_ENCRYPT('e'),
    0xff, 0
};

static byte cheatScriptSeq2[] = {
    CHEAT_ENCRYPT('p'),
    CHEAT_ENCRYPT('u'),
    CHEAT_ENCRYPT('k'),
    CHEAT_ENCRYPT('e'),
    0, 0xff, 0
};

static byte cheatScriptSeq3[] = {
    CHEAT_ENCRYPT('p'),
    CHEAT_ENCRYPT('u'),
    CHEAT_ENCRYPT('k'),
    CHEAT_ENCRYPT('e'),
    0, 0, 0xff,
};

static byte cheatRevealSeq[] = {
    CHEAT_ENCRYPT('m'),
    CHEAT_ENCRYPT('a'),
    CHEAT_ENCRYPT('p'),
    CHEAT_ENCRYPT('s'),
    CHEAT_ENCRYPT('c'),
    CHEAT_ENCRYPT('o'),
    0xff, 0
};

static byte cheatTrackSeq1[] = {
    //CHEAT_ENCRYPT('`'),
    CHEAT_ENCRYPT('c'),
    CHEAT_ENCRYPT('d'),
    CHEAT_ENCRYPT('t'),
    0xff, 0
};

static byte cheatTrackSeq2[] = {
    //CHEAT_ENCRYPT('`'),
    CHEAT_ENCRYPT('c'),
    CHEAT_ENCRYPT('d'),
    CHEAT_ENCRYPT('t'),
    0, 0, 0xff, 0
};

static char cheatKills[] = { 'k', 'i', 'l', 'l', 's' };

static cheat_t cheats[] = {
    {CheatGodFunc, cheatGodSeq, NULL, {0, 0}, 0},
    {CheatNoClipFunc, cheatNoClipSeq, NULL, {0, 0}, 0},
    {CheatWeaponsFunc, cheatWeaponsSeq, NULL, {0, 0}, 0},
    {CheatHealthFunc, cheatHealthSeq, NULL, {0, 0}, 0},
    {CheatKeysFunc, cheatKeysSeq, NULL, {0, 0}, 0},
    {CheatSoundFunc, cheatSoundSeq, NULL, {0, 0}, 0},
    {CheatInventoryFunc, cheatInventorySeq, NULL, {0, 0}, 0},
    {CheatPuzzleFunc, cheatPuzzleSeq, NULL, {0, 0}, 0},
    {CheatWarpFunc, cheatWarpSeq, NULL, {0, 0}, 0},
    {CheatPigFunc, cheatPigSeq, NULL, {0, 0}, 0},
    {CheatMassacreFunc, cheatMassacreSeq, NULL, {0, 0}, 0},
    {CheatIDKFAFunc, cheatIDKFASeq, NULL, {0, 0}, 0},
    {CheatQuickenFunc1, cheatQuickenSeq1, NULL, {0, 0}, 0},
    {CheatQuickenFunc2, cheatQuickenSeq2, NULL, {0, 0}, 0},
    {CheatQuickenFunc3, cheatQuickenSeq3, NULL, {0, 0}, 0},
    {CheatClassFunc1, cheatClass1Seq, NULL, {0, 0}, 0},
    {CheatClassFunc2, cheatClass2Seq, NULL, {0, 0}, 0},
    {CheatInitFunc, cheatInitSeq, NULL, {0, 0}, 0},
    {CheatVersionFunc, cheatVersionSeq, NULL, {0, 0}, 0},
    {CheatDebugFunc, cheatDebugSeq, NULL, {0, 0}, 0},
    {CheatScriptFunc1, cheatScriptSeq1, NULL, {0, 0}, 0},
    {CheatScriptFunc2, cheatScriptSeq2, NULL, {0, 0}, 0},
    {CheatScriptFunc3, cheatScriptSeq3, NULL, {0, 0}, 0},
    {CheatRevealFunc, cheatRevealSeq, NULL, {0, 0}, 0},
    {NULL, NULL, NULL, {0, 0}, 0} // Terminator.
};

// CODE --------------------------------------------------------------------

void Cht_Init(void)
{
    int                 i;

    for(i = 0; i < 256; ++i)
    {
        cheatLookup[i] = CHEAT_ENCRYPT(i);
    }
}

/**
 * Responds to user input to see if a cheat sequence has been entered.
 *
 * @param ev            Ptr to the event to be checked.
 * @return              @c true, if the caller should eat the key.
 */
boolean Cht_Responder(event_t *ev)
{
    int                 i;
    byte                key = ev->data1;
    boolean             eat;
    automapid_t         map;

    if(ev->type != EV_KEY || ev->state != EVS_DOWN)
        return false;

    if(G_GetGameState() != GS_MAP)
        return false;

    if(gameSkill == SM_NIGHTMARE)
    {   // Can't cheat in nightmare mode
        return false;
    }
    else if(IS_NETGAME)
    {   // change CD track is the only cheat available in deathmatch
        eat = false;
        return eat;
    }

    if(players[CONSOLEPLAYER].health <= 0)
    {   // Dead players can't cheat.
        return false;
    }

    eat = false;
    for(i = 0; cheats[i].func != NULL; ++i)
    {
        if(CheatAddKey(&cheats[i], key, &eat))
        {
            cheats[i].func(&players[CONSOLEPLAYER], &cheats[i]);
            S_StartSound(SFX_PLATFORM_STOP, NULL);
        }
    }

    map = AM_MapForPlayer(CONSOLEPLAYER);
    if(AM_IsActive(map))
    {
        if(ev->state == EVS_DOWN)
        {
            if(cheatKills[ShowKillsCount] == ev->data1 && IS_NETGAME
               && deathmatch)
            {
                ShowKillsCount++;
                if(ShowKillsCount == 5)
                {
                    ShowKillsCount = 0;
                    ShowKills = !ShowKills;
                }
            }
            else
            {
                ShowKillsCount = 0;
            }
            return false;
        }
        else if(ev->state == EVS_UP)
        {
            return false;
        }
        else if(ev->state == EVS_REPEAT)
            return true;
    }

    return eat;
}

static boolean canCheat(void)
{
    extern boolean netCheatParm;

    if(IS_NETGAME && !IS_CLIENT && netSvAllowCheats)
        return true;

#ifdef _DEBUG
    return true;
#else
    return !(gameSkill == SM_NIGHTMARE || (IS_NETGAME && !netCheatParm) ||
             players[CONSOLEPLAYER].health <= 0);
#endif
}

/**
 * @return              @c true, if the added key completed the cheat.
 */
static boolean CheatAddKey(cheat_t *cheat, byte key, boolean *eat)
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
    else if(cheatLookup[key] == *cheat->pos)
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
        return true;
    }

    return false;
}

void Cht_GodFunc(player_t *player)
{
    CheatGodFunc(player, NULL);
}

void Cht_NoClipFunc(player_t *player)
{
    CheatNoClipFunc(player, NULL);
}

static void CheatGodFunc(player_t *player, cheat_t *cheat)
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

static void CheatNoClipFunc(player_t *player, cheat_t *cheat)
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

void Cht_SuicideFunc(player_t *plyr)
{
    P_DamageMobj(plyr->plr->mo, NULL, NULL, 10000, false);
}

int Cht_SuicideResponse(msgresponse_t response, void* context)
{
    if(response == MSG_YES)
    {
        Cht_SuicideFunc(&players[CONSOLEPLAYER]);
    }

    return true;
}

static void CheatWeaponsFunc(player_t *player, cheat_t *cheat)
{
    int                 i;

    player->update |= PSF_ARMOR_POINTS | PSF_OWNED_WEAPONS | PSF_AMMO;

    for(i = 0; i < NUMARMOR; ++i)
    {
        player->armorPoints[i] = PCLASS_INFO(player->class)->armorIncrement[i];
    }

    for(i = 0; i < NUM_WEAPON_TYPES; ++i)
    {
        player->weapons[i].owned = true;
    }

    for(i = 0; i < NUM_AMMO_TYPES; ++i)
    {
        player->ammo[i].owned = MAX_MANA;
    }

    P_SetMessage(player, TXT_CHEATWEAPONS, false);
}

static void CheatHealthFunc(player_t *player, cheat_t *cheat)
{
    player->update |= PSF_HEALTH;
    if(player->morphTics)
    {
        player->health = player->plr->mo->health = MAXMORPHHEALTH;
    }
    else
    {
        player->health = player->plr->mo->health = maxHealth;
    }
    P_SetMessage(player, TXT_CHEATHEALTH, false);
}

static void CheatKeysFunc(player_t *player, cheat_t *cheat)
{
    player->update |= PSF_KEYS;
    player->keys = 2047;
    P_SetMessage(player, TXT_CHEATKEYS, false);
}

static void CheatSoundFunc(player_t *player, cheat_t *cheat)
{
    debugSound = !debugSound;
    if(debugSound)
    {
        P_SetMessage(player, TXT_CHEATSOUNDON, false);
    }
    else
    {
        P_SetMessage(player, TXT_CHEATSOUNDOFF, false);
    }
}

static void CheatInventoryFunc(player_t* player, cheat_t* cheat)
{
    int                 i, j, plrNum = player - players;

    for(i = IIT_NONE + 1; i < IIT_FIRSTPUZZITEM; ++i)
    {
        for(j = 0; j < 25; ++j)
        {
            P_InventoryGive(plrNum, i, false);
        }
    }

    P_SetMessage(player, TXT_CHEATINVITEMS3, false);
}

static void CheatPuzzleFunc(player_t* player, cheat_t* cheat)
{
    int                 i, plrNum = player - players;

    for(i = IIT_FIRSTPUZZITEM; i < NUM_INVENTORYITEM_TYPES; ++i)
    {
        P_InventoryGive(plrNum, i, false);
    }

    P_SetMessage(player, TXT_CHEATINVITEMS3, false);
}

static void CheatInitFunc(player_t* player, cheat_t* cheat)
{
    G_DeferedInitNew(gameSkill, gameEpisode, gameMap);
    P_SetMessage(player, TXT_CHEATWARP, false);
}

static void CheatWarpFunc(player_t* player, cheat_t* cheat)
{
    int                 tens, ones;
    int                 map;
    char                mapName[9];

    tens = cheat->args[0] - '0';
    ones = cheat->args[1] - '0';
    if(tens < 0 || tens > 9 || ones < 0 || ones > 9)
    {   // Bad map
        P_SetMessage(player, TXT_CHEATBADINPUT, false);
        return;
    }

    //map = P_TranslateMap((cheat->args[0]-'0')*10+cheat->args[1]-'0');
    map = P_TranslateMap(tens * 10 + ones);
    if(map == -1)
    {   // Not found
        P_SetMessage(player, TXT_CHEATNOMAP, false);
        return;
    }

    if(map == gameMap)
    {   // Don't try to teleport to current map.
        P_SetMessage(player, TXT_CHEATBADINPUT, false);
        return;
    }

    // Search primary lumps.
    sprintf(mapName, "MAP%02d", map);
    if(W_CheckNumForName(mapName) == -1)
    {   // Can't find.
        P_SetMessage(player, TXT_CHEATNOMAP, false);
        return;
    }

    P_SetMessage(player, TXT_CHEATWARP, false);

    Hu_MenuCommand(MCMD_CLOSE);

    leaveMap = map;
    leavePosition = 0;
    G_WorldDone();
}

static void CheatPigFunc(player_t* player, cheat_t* cheat)
{
    if(player->morphTics)
    {
        P_UndoPlayerMorph(player);
    }
    else
    {
        P_MorphPlayer(player);
    }

    P_SetMessage(player, "SQUEAL!!", false);
}

static void CheatMassacreFunc(player_t* player, cheat_t* cheat)
{
    int                 count;
    char                buffer[80];

    count = P_Massacre();
    sprintf(buffer, "%d MONSTERS KILLED\n", count);
    P_SetMessage(player, buffer, false);
}

static void CheatIDKFAFunc(player_t* player, cheat_t* cheat)
{
    int                 i;

    if(player->morphTics)
    {
        return;
    }

    for(i = 1; i < 8; ++i)
    {
        player->weapons[i].owned = false;
    }

    player->pendingWeapon = WT_FIRST;
    P_SetMessage(player, TXT_CHEATIDKFA, false);
}

static void CheatQuickenFunc1(player_t* player, cheat_t* cheat)
{
    P_SetMessage(player, "TRYING TO CHEAT?  THAT'S ONE....", false);
}

static void CheatQuickenFunc2(player_t* player, cheat_t* cheat)
{
    P_SetMessage(player, "THAT'S TWO....", false);
}

static void CheatQuickenFunc3(player_t* player, cheat_t* cheat)
{
    P_DamageMobj(player->plr->mo, NULL, player->plr->mo, 10000, false);
    P_SetMessage(player, "THAT'S THREE!  TIME TO DIE.", false);
}

static void CheatClassFunc1(player_t* player, cheat_t* cheat)
{
    P_SetMessage(player, "ENTER NEW PLAYER CLASS NUMBER", false);
}

static void CheatClassFunc2(player_t* player, cheat_t* cheat)
{
    P_PlayerChangeClass(player, cheat->args[0] - '0');
}

static void CheatVersionFunc(player_t* player, cheat_t* cheat)
{
    DD_Execute(false, "version");
}

static void CheatDebugFunc(player_t* player, cheat_t* cheat)
{
    char                lumpName[9];
    char                textBuffer[256];
    subsector_t*        sub;

    if(!player->plr->mo || !userGame)
        return;

    P_GetMapLumpName(gameEpisode, gameMap, lumpName);
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
    Con_Message("  FloorZ:%g Material:%s\n", P_GetFloatp(sub, DMU_FLOOR_HEIGHT),
                P_GetMaterialName(P_GetPtrp(sub, DMU_FLOOR_MATERIAL)));
    Con_Message("  CeilingZ:%g Material:%s\n", P_GetFloatp(sub, DMU_CEILING_HEIGHT),
                P_GetMaterialName(P_GetPtrp(sub, DMU_CEILING_MATERIAL)));
    Con_Message("Player height:%g   Player radius:%g\n",
                player->plr->mo->height, player->plr->mo->radius);
}

static void CheatScriptFunc1(player_t *player, cheat_t *cheat)
{
    P_SetMessage(player, "RUN WHICH SCRIPT(01-99)?", false);
}

static void CheatScriptFunc2(player_t *player, cheat_t * cheat)
{
    P_SetMessage(player, "RUN WHICH SCRIPT(01-99)?", false);
}

static void CheatScriptFunc3(player_t *player, cheat_t * cheat)
{
    int                 script;
    byte                args[3];
    int                 tens, ones;
    char                textBuffer[40];

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
        P_SetMessage(player, textBuffer, false);
    }
}

static void CheatRevealFunc(player_t *player, cheat_t *cheat)
{
    AM_IncMapCheatLevel(AM_MapForPlayer(player - players));
}

/**
 * This is the multipurpose cheat ccmd.
 */
DEFCC(CCmdCheat)
{
    uint                i;

    // Give each of the characters in argument two to the SB event handler.
    for(i = 0; i < strlen(argv[1]); ++i)
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
        return false; // Can't cheat!

    CheatGodFunc(players + CONSOLEPLAYER, NULL);
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
        return false; // Can't cheat!

    CheatNoClipFunc(players + CONSOLEPLAYER, NULL);
    return true;
}

DEFCC(CCmdCheatSuicide)
{
    if(G_GetGameState() == GS_MAP)
    {
        if(IS_NETGAME)
        {
            NetCl_CheatRequest("suicide");
        }
        else
        {   // When not in a netgame we'll ask the player to confirm.
            player_t*           plr = &players[CONSOLEPLAYER];

            if(plr->playerState == PST_DEAD)
                return false; // Already dead!

            Hu_MsgStart(MSG_YESNO, SUICIDEASK, Cht_SuicideResponse, NULL);
        }
    }
    else
    {
        Hu_MsgStart(MSG_ANYKEY, SUICIDEOUTMAP, NULL, NULL);
    }

    return true;
}

DEFCC(CCmdCheatGive)
{
    char                buf[100];
    player_t*           plyr = &players[CONSOLEPLAYER];
    size_t              i, stuffLen;

    if(IS_CLIENT)
    {
        char                buf[100];

        if(argc != 2)
            return false;

        sprintf(buf, "give %s", argv[1]);
        NetCl_CheatRequest(buf);
        return true;
    }

    if(!canCheat())
        return false; // Can't cheat!

    if(argc != 2 && argc != 3)
    {
        Con_Printf("Usage:\n  give (stuff)\n");
        Con_Printf("  give (stuff) (player)\n");
        Con_Printf("Stuff consists of one or more of (type:id). "
                   "If no id; give all of type:\n");
        Con_Printf(" i - items\n");
        Con_Printf(" h - health\n");
        Con_Printf(" k - keys\n");
        Con_Printf(" p - puzzle\n");
        Con_Printf(" w - weapons\n");
        Con_Printf("Example: 'give ikw' gives items, keys and weapons.\n");
        Con_Printf("Example: 'give w2k1' gives weapon two and key one.\n");
        return true;
    }

    if(argc == 3)
    {
        i = atoi(argv[2]);
        if(i < 0 || i >= MAXPLAYERS)
            return false;
        plyr = &players[i];
    }

    if(G_GetGameState() != GS_MAP)
    {
        Con_Printf("Can only \"give\" when in a game!\n");
        return true;
    }

    if(!plyr->plr->inGame)
        return true; // Can't give to a player who's not playing.

    strcpy(buf, argv[1]); // Stuff is the 2nd arg.
    strlwr(buf);
    stuffLen = strlen(buf);
    for(i = 0; buf[i]; ++i)
    {
        switch(buf[i])
        {
        case 'i':
            Con_Printf("Items given.\n");
            CheatInventoryFunc(plyr, NULL);
            break;

        case 'h':
            Con_Printf("Health given.\n");
            CheatHealthFunc(plyr, NULL);
            break;

        case 'k':
            {
            boolean             giveAll = true;

            if(i < stuffLen)
            {
                int                 idx;

                idx = ((int) buf[i+1]) - 48;
                if(idx >= 0 && idx < NUM_KEY_TYPES)
                {   // Give one specific key.
                    plyr->update |= PSF_KEYS;
                    plyr->keys |= (1 << idx);
                    giveAll = false;
                    i++;
                }
            }

            if(giveAll)
            {
                Con_Printf("All Keys given.\n");
                CheatKeysFunc(plyr, NULL);
            }
            break;
            }
        case 'p':
            Con_Printf("Puzzle parts given.\n");
            CheatPuzzleFunc(plyr, NULL);
            break;

        case 'w':
            {
            boolean             giveAll = true;

            if(i < stuffLen)
            {
                int                 idx;

                idx = ((int) buf[i+1]) - 48;
                if(idx >= 0 && idx < NUM_WEAPON_TYPES)
                {   // Give one specific weapon.
                    plyr->update |= PSF_OWNED_WEAPONS;
                    plyr->weapons[idx].owned = true;
                    giveAll = false;
                    i++;
                }
            }

            if(giveAll)
            {
                Con_Printf("All weapons given.\n");
                CheatWeaponsFunc(plyr, NULL);
            }
            break;
            }
        default:
            // Unrecognized
            Con_Printf("What do you mean, '%c'?\n", buf[i]);
            break;
        }
    }

    return true;
}

DEFCC(CCmdCheatWarp)
{
    cheat_t             cheat;
    int                 num;

    if(!canCheat())
        return false; // Can't cheat!

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
    CheatWarpFunc(players + CONSOLEPLAYER, &cheat);
    return true;
}

DEFCC(CCmdCheatPig)
{
    if(!canCheat())
        return false; // Can't cheat!

    CheatPigFunc(players + CONSOLEPLAYER, NULL);
    return true;
}

DEFCC(CCmdCheatMassacre)
{
    if(!canCheat())
        return false; // Can't cheat!

    DD_ClearKeyRepeaters(); // Why?
    CheatMassacreFunc(players + CONSOLEPLAYER, NULL);
    return true;
}

DEFCC(CCmdCheatShadowcaster)
{
    cheat_t             cheat;

    if(!canCheat())
        return false; // Can't cheat!

    cheat.args[0] = atoi(argv[1]) + '0';
    CheatClassFunc2(players + CONSOLEPLAYER, &cheat);
    return true;
}

DEFCC(CCmdCheatWhere)
{
    if(!canCheat())
        return false; // Can't cheat!
    CheatDebugFunc(players + CONSOLEPLAYER, NULL);
    return true;
}

DEFCC(CCmdCheatRunScript)
{
    cheat_t             cheat;
    int                 num;

    if(!canCheat())
        return false; // Can't cheat!

    num = atoi(argv[1]);
    cheat.args[0] = num / 10 + '0';
    cheat.args[1] = num % 10 + '0';
    CheatScriptFunc3(players + CONSOLEPLAYER, &cheat);
    return true;
}

DEFCC(CCmdCheatReveal)
{
    int                 option;
    automapid_t         map;

    if(!canCheat())
        return false; // Can't cheat!

    // Reset them (for 'nothing'). :-)
    map = AM_MapForPlayer(CONSOLEPLAYER);
    AM_SetCheatLevel(map, 0);
    AM_RevealMap(map, false);
    option = atoi(argv[1]);
    if(option < 0 || option > 3)
        return false;

    if(option == 1)
        AM_RevealMap(map, true);
    else if(option != 0)
        AM_SetCheatLevel(map, option -1);

    return true;
}
