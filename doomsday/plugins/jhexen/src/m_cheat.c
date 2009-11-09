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

#include <stdlib.h>
#include <string.h>

#include "jhexen.h"

#include "f_infine.h"
#include "d_net.h"
#include "g_common.h"
#include "p_player.h"
#include "am_map.h"
#include "hu_menu.h"
#include "hu_msg.h"
#include "dmu_lib.h"
#include "p_user.h"
#include "p_start.h"

#include "p_inventory.h"

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

typedef struct cheatseq_s {
    byte*           sequence;
    byte*           pos;
    int             args[2];
    int             currentArg;
} cheatseq_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void Cht_GodFunc(player_t* plr);
void Cht_NoClipFunc(player_t* plr);
void Cht_SuicideFunc(player_t* plr);
void Cht_GiveWeaponsFunc(player_t* plr);
void Cht_GiveArmorFunc(player_t* plr);
void Cht_GiveAmmoFunc(player_t* plr);
void Cht_HealthFunc(player_t* plr);
void Cht_GiveKeysFunc(player_t* plr);
void Cht_SoundFunc(player_t* plr);
void Cht_InventoryFunc(player_t* plr);
void Cht_PuzzleFunc(player_t* plr);
void Cht_InitFunc(player_t* plr);
boolean Cht_WarpFunc(player_t* plr, cheatseq_t* cheat);
void Cht_PigFunc(player_t* plr);
void Cht_MassacreFunc(player_t* plr);
void Cht_IDKFAFunc(player_t* plr);
void Cht_QuickenFunc1(player_t* plr);
void Cht_QuickenFunc2(player_t* plr);
void Cht_QuickenFunc3(player_t* plr);
void Cht_ClassFunc1(player_t* plr);
void Cht_ClassFunc2(player_t* plr, cheatseq_t* cheat);
void Cht_VersionFunc(player_t* plr);
void Cht_DebugFunc(player_t* plr);
void Cht_ScriptFunc1(player_t* plr);
void Cht_ScriptFunc2(player_t* plr);
void Cht_ScriptFunc3(player_t* plr, cheatseq_t* cheat);
void Cht_RevealFunc(player_t* plr);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

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
    CHEAT_ENCRYPT('c'),
    CHEAT_ENCRYPT('d'),
    CHEAT_ENCRYPT('t'),
    0xff, 0
};

static byte cheatTrackSeq2[] = {
    CHEAT_ENCRYPT('c'),
    CHEAT_ENCRYPT('d'),
    CHEAT_ENCRYPT('t'),
    0, 0, 0xff, 0
};

static cheatseq_t cheatGod = {cheatGodSeq, NULL, {0, 0}, 0};
static cheatseq_t cheatNoClip = {cheatNoClipSeq, NULL, {0, 0}, 0};
static cheatseq_t cheatWeapons = {cheatWeaponsSeq, NULL, {0, 0}, 0};
static cheatseq_t cheatHealth = {cheatHealthSeq, NULL, {0, 0}, 0};
static cheatseq_t cheatKeys = {cheatKeysSeq, NULL, {0, 0}, 0};
static cheatseq_t cheatSound = {cheatSoundSeq, NULL, {0, 0}, 0};
static cheatseq_t cheatInventory = {cheatInventorySeq, NULL, {0, 0}, 0};
static cheatseq_t cheatPuzzle = {cheatPuzzleSeq, NULL, {0, 0}, 0};
static cheatseq_t cheatWarp = {cheatWarpSeq, NULL, {0, 0}, 0};
static cheatseq_t cheatPig = {cheatPigSeq, NULL, {0, 0}, 0};
static cheatseq_t cheatMassacre = {cheatMassacreSeq, NULL, {0, 0}, 0};
static cheatseq_t cheatIDKFA = {cheatIDKFASeq, NULL, {0, 0}, 0};
static cheatseq_t cheatQuicken1 = {cheatQuickenSeq1, NULL, {0, 0}, 0};
static cheatseq_t cheatQuicken2 = {cheatQuickenSeq2, NULL, {0, 0}, 0};
static cheatseq_t cheatQuicken3 = {cheatQuickenSeq3, NULL, {0, 0}, 0};
static cheatseq_t cheatClass1 = {cheatClass1Seq, NULL, {0, 0}, 0};
static cheatseq_t cheatClass2 = {cheatClass2Seq, NULL, {0, 0}, 0};
static cheatseq_t cheatInit = {cheatInitSeq, NULL, {0, 0}, 0};
static cheatseq_t cheatVersion = {cheatVersionSeq, NULL, {0, 0}, 0};
static cheatseq_t cheatDebug = {cheatDebugSeq, NULL, {0, 0}, 0};
static cheatseq_t cheatScript1 = {cheatScriptSeq1, NULL, {0, 0}, 0};
static cheatseq_t cheatScript2 = {cheatScriptSeq2, NULL, {0, 0}, 0};
static cheatseq_t cheatScript3 = {cheatScriptSeq3, NULL, {0, 0}, 0};
static cheatseq_t cheatReveal = {cheatRevealSeq, NULL, {0, 0}, 0};

// CODE --------------------------------------------------------------------

static boolean cheatsEnabled(void)
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
 * @return              Non-zero iff the cheat was successful.
 */
static int checkCheat(cheatseq_t* cht, char key, boolean* eat)
{
    if(!cht->pos)
    {
        cht->pos = cht->sequence;
        cht->currentArg = 0;
    }

    if(*cht->pos == 0)
    {
        *eat = true;
        cht->args[cht->currentArg++] = key;
        cht->pos++;
    }
    else if(cheatLookup[key] == *cht->pos)
    {
        cht->pos++;
    }
    else
    {
        cht->pos = cht->sequence;
        cht->currentArg = 0;
    }

    if(*cht->pos == 0xff)
    {
        cht->pos = cht->sequence;
        cht->currentArg = 0;
        return true;
    }

    return false;
}

void Cht_Init(void)
{
    int i;

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
boolean Cht_Responder(event_t* ev)
{
    byte key = ev->data1;
    boolean eat = false;
    player_t* plr = &players[CONSOLEPLAYER];

    if(ev->type != EV_KEY || ev->state != EVS_DOWN)
        return false;

    if(G_GetGameState() != GS_MAP)
        return false;

    if(IS_NETGAME || gameSkill == SM_NIGHTMARE)
    {   // Can't cheat in a net-game, or in nightmare mode.
        return false;
    }

    if(plr->health <= 0)
    {   // Dead players can't cheat.
        return false;
    }

    if(checkCheat(&cheatGod, key, &eat))
    {
        Cht_GodFunc(plr);
        S_LocalSound(SFX_PLATFORM_STOP, NULL);
    }
    else if(checkCheat(&cheatNoClip, key, &eat))
    {
        Cht_NoClipFunc(plr);
        S_LocalSound(SFX_PLATFORM_STOP, NULL);
    }
    else if(checkCheat(&cheatWeapons, key, &eat))
    {
        Cht_GiveWeaponsFunc(plr);
        Cht_GiveAmmoFunc(plr);
        Cht_GiveArmorFunc(plr);

        P_SetMessage(plr, TXT_CHEATWEAPONS, false);
        S_LocalSound(SFX_PLATFORM_STOP, NULL);
    }
    else if(checkCheat(&cheatHealth, key, &eat))
    {
        Cht_HealthFunc(plr);
        S_LocalSound(SFX_PLATFORM_STOP, NULL);
    }
    else if(checkCheat(&cheatKeys, key, &eat))
    {
        Cht_GiveKeysFunc(plr);
        S_LocalSound(SFX_PLATFORM_STOP, NULL);
    }
    else if(checkCheat(&cheatSound, key, &eat))
    {
        Cht_SoundFunc(plr);
        S_LocalSound(SFX_PLATFORM_STOP, NULL);
    }
    else if(checkCheat(&cheatInventory, key, &eat))
    {
        Cht_InventoryFunc(plr);
        S_LocalSound(SFX_PLATFORM_STOP, NULL);
    }
    else if(checkCheat(&cheatPuzzle, key, &eat))
    {
        Cht_PuzzleFunc(plr);
        S_LocalSound(SFX_PLATFORM_STOP, NULL);
    }
    else if(checkCheat(&cheatWarp, key, &eat))
    {
        Cht_WarpFunc(plr, &cheatWarp);
        S_LocalSound(SFX_PLATFORM_STOP, NULL);
    }
    else if(checkCheat(&cheatPig, key, &eat))
    {
        Cht_PigFunc(plr);
        S_LocalSound(SFX_PLATFORM_STOP, NULL);
    }
    else if(checkCheat(&cheatMassacre, key, &eat))
    {
        Cht_MassacreFunc(plr);
        S_LocalSound(SFX_PLATFORM_STOP, NULL);
    }
    else if(checkCheat(&cheatIDKFA, key, &eat))
    {
        Cht_IDKFAFunc(plr);
        S_LocalSound(SFX_PLATFORM_STOP, NULL);
    }
    else if(checkCheat(&cheatQuicken1, key, &eat))
    {
        Cht_QuickenFunc1(plr);
        S_LocalSound(SFX_PLATFORM_STOP, NULL);
    }
    else if(checkCheat(&cheatQuicken2, key, &eat))
    {
        Cht_QuickenFunc2(plr);
        S_LocalSound(SFX_PLATFORM_STOP, NULL);
    }
    else if(checkCheat(&cheatQuicken3, key, &eat))
    {
        Cht_QuickenFunc3(plr);
        S_LocalSound(SFX_PLATFORM_STOP, NULL);
    }
    else if(checkCheat(&cheatClass1, key, &eat))
    {
        Cht_ClassFunc1(plr);
        S_LocalSound(SFX_PLATFORM_STOP, NULL);
    }
    else if(checkCheat(&cheatClass2, key, &eat))
    {
        Cht_ClassFunc2(plr, &cheatClass2);
        S_LocalSound(SFX_PLATFORM_STOP, NULL);
    }
    else if(checkCheat(&cheatInit, key, &eat))
    {
        Cht_InitFunc(plr);
        S_LocalSound(SFX_PLATFORM_STOP, NULL);
    }
    else if(checkCheat(&cheatVersion, key, &eat))
    {
        Cht_VersionFunc(plr);
        S_LocalSound(SFX_PLATFORM_STOP, NULL);
    }
    else if(checkCheat(&cheatDebug, key, &eat))
    {
        Cht_DebugFunc(plr);
        S_LocalSound(SFX_PLATFORM_STOP, NULL);
    }
    else if(checkCheat(&cheatScript1, key, &eat))
    {
        Cht_ScriptFunc1(plr);
        S_LocalSound(SFX_PLATFORM_STOP, NULL);
    }
    else if(checkCheat(&cheatScript2, key, &eat))
    {
        Cht_ScriptFunc2(plr);
        S_LocalSound(SFX_PLATFORM_STOP, NULL);
    }
    else if(checkCheat(&cheatScript3, key, &eat))
    {
        Cht_ScriptFunc3(plr, &cheatScript3);
        S_LocalSound(SFX_PLATFORM_STOP, NULL);
    }
    else if(checkCheat(&cheatReveal, key, &eat))
    {
        Cht_RevealFunc(plr);
        S_LocalSound(SFX_PLATFORM_STOP, NULL);
    }

    return eat;
}

void Cht_GodFunc(player_t* plr)
{
    plr->cheats ^= CF_GODMODE;
    plr->update |= PSF_STATE;

    P_SetMessage(plr,
                 ((P_GetPlayerCheats(plr) & CF_GODMODE) ? TXT_CHEATGODON : TXT_CHEATGODOFF), false);
}

void Cht_SuicideFunc(player_t* plr)
{
    P_DamageMobj(plr->plr->mo, NULL, NULL, 10000, false);
}

void Cht_GiveArmorFunc(player_t* plr)
{
    int i;

    plr->update |= PSF_ARMOR_POINTS;
    for(i = 0; i < NUMARMOR; ++i)
        plr->armorPoints[i] = PCLASS_INFO(plr->class)->armorIncrement[i];
}

void Cht_GiveWeaponsFunc(player_t* plr)
{
    int i;

    plr->update |= PSF_OWNED_WEAPONS;
    for(i = 0; i < NUM_WEAPON_TYPES; ++i)
    {
        plr->weapons[i].owned = true;
    }
}

void Cht_GiveAmmoFunc(player_t* plr)
{
    int i;

    plr->update |= PSF_AMMO;
    for(i = 0; i < NUM_AMMO_TYPES; ++i)
    {
        plr->ammo[i].owned = MAX_MANA;
    }
}

void Cht_GiveKeysFunc(player_t* plr)
{
    plr->update |= PSF_KEYS;
    plr->keys = 2047;
    P_SetMessage(plr, TXT_CHEATKEYS, false);
}

void Cht_NoClipFunc(player_t* plr)
{
    plr->cheats ^= CF_NOCLIP;
    plr->update |= PSF_STATE;
    P_SetMessage(plr,
                 ((P_GetPlayerCheats(plr) & CF_NOCLIP) ? TXT_CHEATNOCLIPON : TXT_CHEATNOCLIPOFF), false);
}

boolean Cht_WarpFunc(player_t* plr, cheatseq_t* cheat)
{
    int tens, ones, map;
    char mapName[9];

    tens = cheat->args[0] - '0';
    ones = cheat->args[1] - '0';
    if(tens < 0 || tens > 9 || ones < 0 || ones > 9)
    {   // Bad map
        P_SetMessage(plr, TXT_CHEATBADINPUT, false);
        return false;
    }

    //map = P_TranslateMap((cheat->args[0]-'0')*10+cheat->args[1]-'0');
    map = P_TranslateMap(tens * 10 + ones);
    if(map == -1)
    {   // Not found
        P_SetMessage(plr, TXT_CHEATNOMAP, false);
        return false;
    }

    if(map == gameMap)
    {   // Don't try to teleport to current map.
        P_SetMessage(plr, TXT_CHEATBADINPUT, false);
        return false;
    }

    // Search primary lumps.
    sprintf(mapName, "MAP%02d", map);
    if(W_CheckNumForName(mapName) == -1)
    {   // Can't find.
        P_SetMessage(plr, TXT_CHEATNOMAP, false);
        return false;
    }

    // So be it.
    P_SetMessage(plr, TXT_CHEATWARP, false);
    leaveMap = map;
    leavePosition = 0;
    G_WorldDone();

    // Clear the menu if open.
    Hu_MenuCommand(MCMD_CLOSE);
    briefDisabled = true;
    return true;
}

void Cht_SoundFunc(player_t* plr)
{
    debugSound = !debugSound;
    if(debugSound)
    {
        P_SetMessage(plr, TXT_CHEATSOUNDON, false);
    }
    else
    {
        P_SetMessage(plr, TXT_CHEATSOUNDOFF, false);
    }
}

void Cht_DebugFunc(player_t* plr)
{
    char lumpName[9], textBuffer[256];
    subsector_t* sub;

    if(!plr->plr->mo || !userGame)
        return;

    P_GetMapLumpName(gameEpisode, gameMap, lumpName);
    sprintf(textBuffer, "MAP [%s]  X:%g  Y:%g  Z:%g",
            lumpName, plr->plr->mo->pos[VX], plr->plr->mo->pos[VY],
            plr->plr->mo->pos[VZ]);
    P_SetMessage(plr, textBuffer, false);

    // Also print some information to the console.
    Con_Message(textBuffer);
    sub = plr->plr->mo->subsector;
    Con_Message("\nSubsector %i:\n", P_ToIndex(sub));
    Con_Message("  FloorZ:%g Material:%s\n",
                P_GetFloatp(sub, DMU_FLOOR_HEIGHT),
                P_GetMaterialName(P_GetPtrp(sub, DMU_FLOOR_MATERIAL)));
    Con_Message("  CeilingZ:%g Material:%s\n",
                P_GetFloatp(sub, DMU_CEILING_HEIGHT),
                P_GetMaterialName(P_GetPtrp(sub, DMU_CEILING_MATERIAL)));
    Con_Message("Player height:%g   Player radius:%g\n",
                plr->plr->mo->height, plr->plr->mo->radius);
}

void Cht_HealthFunc(player_t* plr)
{
    plr->update |= PSF_HEALTH;
    if(plr->morphTics)
    {
        plr->health = plr->plr->mo->health = MAXMORPHHEALTH;
    }
    else
    {
        plr->health = plr->plr->mo->health = maxHealth;
    }
    P_SetMessage(plr, TXT_CHEATHEALTH, false);
}

void Cht_InventoryFunc(player_t* plr)
{
    int i, j, plrNum = plr - players;

    for(i = IIT_NONE + 1; i < IIT_FIRSTPUZZITEM; ++i)
    {
        for(j = 0; j < 25; ++j)
        {
            P_InventoryGive(plrNum, i, false);
        }
    }

    P_SetMessage(plr, TXT_CHEATINVITEMS3, false);
}

void Cht_PuzzleFunc(player_t* plr)
{
    int i, plrNum = plr - players;

    for(i = IIT_FIRSTPUZZITEM; i < NUM_INVENTORYITEM_TYPES; ++i)
    {
        P_InventoryGive(plrNum, i, false);
    }

    P_SetMessage(plr, TXT_CHEATINVITEMS3, false);
}

void Cht_InitFunc(player_t* plr)
{
    G_DeferedInitNew(gameSkill, gameEpisode, gameMap);
    P_SetMessage(plr, TXT_CHEATWARP, false);
}

void Cht_PigFunc(player_t* plr)
{
    if(plr->morphTics)
    {
        P_UndoPlayerMorph(plr);
    }
    else
    {
        P_MorphPlayer(plr);
    }

    P_SetMessage(plr, "SQUEAL!!", false);
}

void Cht_MassacreFunc(player_t* plr)
{
    int count;
    char buf[80];

    count = P_Massacre();
    sprintf(buf, "%d MONSTERS KILLED\n", count);
    P_SetMessage(plr, buf, false);
}

void Cht_IDKFAFunc(player_t* plr)
{
    int i;

    if(plr->morphTics)
    {
        return;
    }

    for(i = 0; i < NUM_WEAPON_TYPES; ++i)
    {
        plr->weapons[i].owned = false;
    }

    plr->pendingWeapon = WT_FIRST;
    P_SetMessage(plr, TXT_CHEATIDKFA, false);
}

void Cht_QuickenFunc1(player_t* plr)
{
    P_SetMessage(plr, "TRYING TO CHEAT?  THAT'S ONE....", false);
}

void Cht_QuickenFunc2(player_t* plr)
{
    P_SetMessage(plr, "THAT'S TWO....", false);
}

void Cht_QuickenFunc3(player_t* plr)
{
    P_DamageMobj(plr->plr->mo, NULL, plr->plr->mo, 10000, false);
    P_SetMessage(plr, "THAT'S THREE!  TIME TO DIE.", false);
}

void Cht_ClassFunc1(player_t* plr)
{
    P_SetMessage(plr, "ENTER NEW PLAYER CLASS NUMBER", false);
}

void Cht_ClassFunc2(player_t* plr, cheatseq_t* cheat)
{
    P_PlayerChangeClass(plr, cheat->args[0] - '0');
}

void Cht_VersionFunc(player_t* plr)
{
    DD_Execute(false, "version");
}

void Cht_ScriptFunc1(player_t* plr)
{
    P_SetMessage(plr, "RUN WHICH SCRIPT(01-99)?", false);
}

void Cht_ScriptFunc2(player_t* plr)
{
    P_SetMessage(plr, "RUN WHICH SCRIPT(01-99)?", false);
}

void Cht_ScriptFunc3(player_t* plr, cheatseq_t* cheat)
{
    int script, tens, ones;
    byte args[3];
    char textBuffer[40];

    tens = cheat->args[0] - '0';
    ones = cheat->args[1] - '0';
    script = tens * 10 + ones;
    if(script < 1)
        return;
    if(script > 99)
        return;
    args[0] = args[1] = args[2] = 0;

    if(P_StartACS(script, 0, args, plr->plr->mo, NULL, 0))
    {
        sprintf(textBuffer, "RUNNING SCRIPT %.2d", script);
        P_SetMessage(plr, textBuffer, false);
    }
}

void Cht_RevealFunc(player_t* plr)
{
    AM_IncMapCheatLevel(AM_MapForPlayer(plr - players));
}

/**
 * This is the multipurpose cheat ccmd.
 */
DEFCC(CCmdCheat)
{
    size_t i;

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
        NetCl_CheatRequest("god");
    else
        Cht_GodFunc(&players[CONSOLEPLAYER]);
    return true;
}

DEFCC(CCmdCheatNoClip)
{
    if(IS_NETGAME)
        NetCl_CheatRequest("noclip");
    else
        Cht_NoClipFunc(&players[CONSOLEPLAYER]);
    return true;
}

static int suicideResponse(msgresponse_t response, void* context)
{
    if(response == MSG_YES)
        Cht_SuicideFunc(&players[CONSOLEPLAYER]);
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
        {   // When not in a netgame we'll ask the plr to confirm.
            player_t* plr = &players[CONSOLEPLAYER];

            if(plr->playerState == PST_DEAD)
                return false; // Already dead!

            Hu_MsgStart(MSG_YESNO, SUICIDEASK, suicideResponse, NULL);
        }
    }
    else
    {
        Hu_MsgStart(MSG_ANYKEY, SUICIDEOUTMAP, NULL, NULL);
    }

    return true;
}

DEFCC(CCmdCheatWarp)
{
    cheatseq_t cheat;
    int num;

    if(!cheatsEnabled())
        return false;

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
    Cht_WarpFunc(&players[CONSOLEPLAYER], &cheat);
    return true;
}

DEFCC(CCmdCheatReveal)
{
    int option;
    automapid_t map;

    if(!cheatsEnabled())
        return false;

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

DEFCC(CCmdCheatGive)
{
    char buf[100];
    player_t* plr = &players[CONSOLEPLAYER];
    size_t i, stuffLen;

    if(IS_CLIENT)
    {
        if(argc != 2)
            return false;

        sprintf(buf, "give %s", argv[1]);
        NetCl_CheatRequest(buf);
        return true;
    }

    if(!cheatsEnabled())
        return false;

    if(argc != 2 && argc != 3)
    {
        Con_Printf("Usage:\n  give (stuff)\n");
        Con_Printf("  give (stuff) (plr)\n");
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

        plr = &players[i];
    }

    if(G_GetGameState() != GS_MAP)
    {
        Con_Printf("Can only \"give\" when in a game!\n");
        return true;
    }

    if(!plr->plr->inGame)
        return true; // Can't give to a plr who's not playing.

    strcpy(buf, argv[1]); // Stuff is the 2nd arg.
    strlwr(buf);
    stuffLen = strlen(buf);
    for(i = 0; buf[i]; ++i)
    {
        switch(buf[i])
        {
        case 'i':
            Con_Printf("Items given.\n");
            Cht_InventoryFunc(plr);
            break;

        case 'h':
            Con_Printf("Health given.\n");
            Cht_HealthFunc(plr);
            break;

        case 'k':
            {
            boolean giveAll = true;

            if(i < stuffLen)
            {
                int idx;

                idx = ((int) buf[i+1]) - 48;
                if(idx >= 0 && idx < NUM_KEY_TYPES)
                {   // Give one specific key.
                    plr->update |= PSF_KEYS;
                    plr->keys |= (1 << idx);
                    giveAll = false;
                    i++;
                }
            }

            if(giveAll)
            {
                Con_Printf("All Keys given.\n");
                Cht_GiveKeysFunc(plr);
            }
            break;
            }
        case 'p':
            Con_Printf("Puzzle parts given.\n");
            Cht_PuzzleFunc(plr);
            break;

        case 'w':
            {
            boolean giveAll = true;

            if(i < stuffLen)
            {
                int idx;

                idx = ((int) buf[i+1]) - 48;
                if(idx >= 0 && idx < NUM_WEAPON_TYPES)
                {   // Give one specific weapon.
                    plr->update |= PSF_OWNED_WEAPONS;
                    plr->weapons[idx].owned = true;
                    giveAll = false;
                    i++;
                }
            }

            if(giveAll)
            {
                Con_Printf("All weapons given.\n");
                Cht_GiveWeaponsFunc(plr);
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

DEFCC(CCmdCheatMassacre)
{
    if(!cheatsEnabled())
        return false;

    Cht_MassacreFunc(&players[CONSOLEPLAYER]);
    return true;
}

DEFCC(CCmdCheatWhere)
{
    if(!cheatsEnabled())
        return false;

    Cht_DebugFunc(&players[CONSOLEPLAYER]);
    return true;
}

DEFCC(CCmdCheatPig)
{
    if(!cheatsEnabled())
        return false;

    Cht_PigFunc(&players[CONSOLEPLAYER]);
    return true;
}

DEFCC(CCmdCheatShadowcaster)
{
    cheatseq_t cheat;

    if(!cheatsEnabled())
        return false;

    cheat.args[0] = atoi(argv[1]) + '0';
    Cht_ClassFunc2(&players[CONSOLEPLAYER], &cheat);
    return true;
}

DEFCC(CCmdCheatRunScript)
{
    cheatseq_t cheat;
    int num;

    if(!cheatsEnabled())
        return false;

    num = atoi(argv[1]);
    cheat.args[0] = num / 10 + '0';
    cheat.args[1] = num % 10 + '0';
    Cht_ScriptFunc3(&players[CONSOLEPLAYER], &cheat);
    return true;
}
