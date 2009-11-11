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
 * m_cheat.c: Cheat sequence checking.
 */

// HEADER FILES ------------------------------------------------------------

#include <stdlib.h>
#include <string.h>

#include "jheretic.h"

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
    ((((a)&1)<<5)+ \
    (((a)&2)<<1)+ \
    (((a)&4)<<4)+ \
    (((a)&8)>>3)+ \
    (((a)&16)>>3)+ \
    (((a)&32)<<2)+ \
    (((a)&64)>>2)+ \
    (((a)&128)>>4))

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
void Cht_GiveAmmoFunc(player_t* plr);
void Cht_GiveArmorFunc(player_t* plr);
void Cht_PowerupFunc(player_t* plr);
void Cht_HealthFunc(player_t* plr);
void Cht_GiveKeysFunc(player_t* plr);
void Cht_InvItem1Func(player_t* plr);
void Cht_InvItem2Func(player_t* plr);
void Cht_InvItem3Func(player_t* plr, cheatseq_t* cheat);
boolean Cht_WarpFunc(player_t* plr, cheatseq_t* cheat);
void Cht_ChickenFunc(player_t* plr);
void Cht_MassacreFunc(player_t* plr);
void Cht_IDKFAFunc(player_t* plr);
void Cht_IDDQDFunc(player_t* plr);
void Cht_DebugFunc(player_t* plr);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static byte cheatLookup[256];

// Toggle god mode.
static byte cheatGodSeq[] = {
    CHEAT_ENCRYPT('q'),
    CHEAT_ENCRYPT('u'),
    CHEAT_ENCRYPT('i'),
    CHEAT_ENCRYPT('c'),
    CHEAT_ENCRYPT('k'),
    CHEAT_ENCRYPT('e'),
    CHEAT_ENCRYPT('n'),
    0xff
};

// Toggle no clipping mode.
static byte cheatNoClipSeq[] = {
    CHEAT_ENCRYPT('k'),
    CHEAT_ENCRYPT('i'),
    CHEAT_ENCRYPT('t'),
    CHEAT_ENCRYPT('t'),
    CHEAT_ENCRYPT('y'),
    0xff
};

// Get all weapons and ammo.
static byte cheatWeaponsSeq[] = {
    CHEAT_ENCRYPT('r'),
    CHEAT_ENCRYPT('a'),
    CHEAT_ENCRYPT('m'),
    CHEAT_ENCRYPT('b'),
    CHEAT_ENCRYPT('o'),
    0xff
};

// Toggle tome of power.
static byte cheatPowerSeq[] = {
    CHEAT_ENCRYPT('s'),
    CHEAT_ENCRYPT('h'),
    CHEAT_ENCRYPT('a'),
    CHEAT_ENCRYPT('z'),
    CHEAT_ENCRYPT('a'),
    CHEAT_ENCRYPT('m'),
    0xff, 0
};

// Get full health.
static byte cheatHealthSeq[] = {
    CHEAT_ENCRYPT('p'),
    CHEAT_ENCRYPT('o'),
    CHEAT_ENCRYPT('n'),
    CHEAT_ENCRYPT('c'),
    CHEAT_ENCRYPT('e'),
    0xff
};

// Get all keys.
static byte cheatKeysSeq[] = {
    CHEAT_ENCRYPT('s'),
    CHEAT_ENCRYPT('k'),
    CHEAT_ENCRYPT('e'),
    CHEAT_ENCRYPT('l'),
    0xff, 0
};

// Toggle sound debug info.
static byte cheatSoundSeq[] = {
    CHEAT_ENCRYPT('n'),
    CHEAT_ENCRYPT('o'),
    CHEAT_ENCRYPT('i'),
    CHEAT_ENCRYPT('s'),
    CHEAT_ENCRYPT('e'),
    0xff
};

// Toggle ticker.
static byte cheatTickerSeq[] = {
    CHEAT_ENCRYPT('t'),
    CHEAT_ENCRYPT('i'),
    CHEAT_ENCRYPT('c'),
    CHEAT_ENCRYPT('k'),
    CHEAT_ENCRYPT('e'),
    CHEAT_ENCRYPT('r'),
    0xff, 0
};

// Get an inventory item 1st stage (ask for type).
static byte cheatInvItem1Seq[] = {
    CHEAT_ENCRYPT('g'),
    CHEAT_ENCRYPT('i'),
    CHEAT_ENCRYPT('m'),
    CHEAT_ENCRYPT('m'),
    CHEAT_ENCRYPT('e'),
    0xff
};

// Get an inventory item 2nd stage (ask for count).
static byte cheatInvItem2Seq[] = {
    CHEAT_ENCRYPT('g'),
    CHEAT_ENCRYPT('i'),
    CHEAT_ENCRYPT('m'),
    CHEAT_ENCRYPT('m'),
    CHEAT_ENCRYPT('e'),
    0, 0xff, 0
};

// Get an inventory item final stage.
static byte cheatInvItem3Seq[] = {
    CHEAT_ENCRYPT('g'),
    CHEAT_ENCRYPT('i'),
    CHEAT_ENCRYPT('m'),
    CHEAT_ENCRYPT('m'),
    CHEAT_ENCRYPT('e'),
    0, 0, 0xff
};

// Warp to new level.
static byte cheatWarpSeq[] = {
    CHEAT_ENCRYPT('e'),
    CHEAT_ENCRYPT('n'),
    CHEAT_ENCRYPT('g'),
    CHEAT_ENCRYPT('a'),
    CHEAT_ENCRYPT('g'),
    CHEAT_ENCRYPT('e'),
    0, 0, 0xff, 0
};

// Save a screenshot.
static byte cheatChickenSeq[] = {
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

// Kill all monsters.
static byte cheatMassacreSeq[] = {
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

static byte cheatIDKFASeq[] = {
    CHEAT_ENCRYPT('i'),
    CHEAT_ENCRYPT('d'),
    CHEAT_ENCRYPT('k'),
    CHEAT_ENCRYPT('f'),
    CHEAT_ENCRYPT('a'),
    0xff, 0
};

static byte cheatIDDQDSeq[] = {
    CHEAT_ENCRYPT('i'),
    CHEAT_ENCRYPT('d'),
    CHEAT_ENCRYPT('d'),
    CHEAT_ENCRYPT('q'),
    CHEAT_ENCRYPT('d'),
    0xff, 0
};

static byte cheatAutomapSeq[] = {
    CHEAT_ENCRYPT('r'),
    CHEAT_ENCRYPT('a'),
    CHEAT_ENCRYPT('v'),
    CHEAT_ENCRYPT('m'),
    CHEAT_ENCRYPT('a'),
    CHEAT_ENCRYPT('p'),
    0xff, 0
};

static cheatseq_t cheatGod = {cheatGodSeq, NULL, {0, 0}, 0};
static cheatseq_t cheatNoClip = {cheatNoClipSeq, NULL, {0, 0}, 0};
static cheatseq_t cheatWeapons = {cheatWeaponsSeq, NULL, {0, 0}, 0};
static cheatseq_t cheatPower = {cheatPowerSeq, NULL, {0, 0}, 0};
static cheatseq_t cheatHealth = {cheatHealthSeq, NULL, {0, 0}, 0};
static cheatseq_t cheatKeys = {cheatKeysSeq, NULL, {0, 0}, 0};
static cheatseq_t cheatSound = {cheatSoundSeq, NULL, {0, 0}, 0};
static cheatseq_t cheatTicker = {cheatTickerSeq, NULL, {0, 0}, 0};
static cheatseq_t cheatInvItem1 = {cheatInvItem1Seq, NULL, {0, 0}, 0};
static cheatseq_t cheatInvItem2 = {cheatInvItem2Seq, NULL, {0, 0}, 0};
static cheatseq_t cheatInvItem3 = {cheatInvItem3Seq, NULL, {0, 0}, 0};
static cheatseq_t cheatWarp = {cheatWarpSeq, NULL, {0, 0}, 0};
static cheatseq_t cheatChicken = {cheatChickenSeq, NULL, {0, 0}, 0};
static cheatseq_t cheatMassacre = {cheatMassacreSeq, NULL, {0, 0}, 0};
static cheatseq_t cheatIDKFA = {cheatIDKFASeq, NULL, {0, 0}, 0};
static cheatseq_t cheatIDDQD = {cheatIDDQDSeq, NULL, {0, 0}, 0};
static cheatseq_t cheatAutomap = {cheatAutomapSeq, NULL, {0, 0}, 0};

// CODE --------------------------------------------------------------------

static boolean cheatsEnabled(void)
{
    if(IS_NETGAME && !IS_CLIENT && netSvAllowCheats)
        return true;

#ifdef _DEBUG
    return true;
#else
    return !(gameSkill == SM_NIGHTMARE || (IS_NETGAME /*&& !netcheat */ )
             || players[CONSOLEPLAYER].health <= 0);
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
        cht->args[cht->currentArg++] = key;
        cht->pos++;
        *eat = true;
    }
    else if(cheatLookup[key] == *cht->pos)
    {
        cht->pos++;
        *eat = true;
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
 * Responds to an input event if determined to be part of a cheat entry
 * sequence.
 *
 * @param ev            Ptr to the event to be checked.
 *
 * @return              @c true, if the event was 'eaten'.
 */
boolean Cht_Responder(event_t* ev)
{
    player_t* plr = &players[CONSOLEPLAYER];
    boolean eat = false;

    if(ev->type != EV_KEY || ev->state != EVS_DOWN)
        return false;

    if(G_GetGameState() != GS_MAP)
        return false;

    if(plr->health <= 0)
        return false; // Dead players can't cheat.

    if(!(IS_NETGAME && deathmatch))
    {
    automapid_t map = AM_MapForPlayer(CONSOLEPLAYER);

    if(AM_IsActive(map) && checkCheat(&cheatAutomap, ev->data1, &eat))
    {
        AM_IncMapCheatLevel(map);
        return true;
    }
    }

    if(IS_NETGAME)
        return false;

    if(gameSkill == SM_NIGHTMARE)
        return false;

    if(checkCheat(&cheatGod, ev->data1, &eat))
    {
        Cht_GodFunc(plr);
        S_LocalSound(SFX_DORCLS, NULL);
    }
    else if(checkCheat(&cheatNoClip, ev->data1, &eat))
    {
        Cht_NoClipFunc(plr);
        S_LocalSound(SFX_DORCLS, NULL);
    }
    else if(checkCheat(&cheatWeapons, ev->data1, &eat))
    {
        Cht_GiveWeaponsFunc(plr);
        Cht_GiveAmmoFunc(plr);
        Cht_GiveArmorFunc(plr);

        P_SetMessage(plr, TXT_CHEATWEAPONS, false);
        S_LocalSound(SFX_DORCLS, NULL);
    }
    else if(checkCheat(&cheatPower, ev->data1, &eat))
    {
        Cht_PowerupFunc(plr);
        S_LocalSound(SFX_DORCLS, NULL);
    }
    else if(checkCheat(&cheatHealth, ev->data1, &eat))
    {
        Cht_HealthFunc(plr);
        S_LocalSound(SFX_DORCLS, NULL);
    }
    else if(checkCheat(&cheatKeys, ev->data1, &eat))
    {
        Cht_GiveKeysFunc(plr);
        S_LocalSound(SFX_DORCLS, NULL);
    }
    else if(checkCheat(&cheatSound, ev->data1, &eat))
    {
        // Otherwise ignored.
        S_LocalSound(SFX_DORCLS, NULL);
    }
    else if(checkCheat(&cheatTicker, ev->data1, &eat))
    {
        // Otherwise ignored.
        S_LocalSound(SFX_DORCLS, NULL);
    }
    else if(checkCheat(&cheatInvItem1, ev->data1, &eat))
    {
        Cht_InvItem1Func(plr);
        S_LocalSound(SFX_DORCLS, NULL);
    }
    else if(checkCheat(&cheatInvItem2, ev->data1, &eat))
    {
        Cht_InvItem2Func(plr);
        S_LocalSound(SFX_DORCLS, NULL);
    }
    else if(checkCheat(&cheatInvItem3, ev->data1, &eat))
    {
        Cht_InvItem3Func(plr, &cheatInvItem3);
        S_LocalSound(SFX_DORCLS, NULL);
    }
    else if(checkCheat(&cheatWarp, ev->data1, &eat))
    {
        Cht_WarpFunc(plr, &cheatWarp);
        S_LocalSound(SFX_DORCLS, NULL);
    }
    else if(checkCheat(&cheatChicken, ev->data1, &eat))
    {
        Cht_ChickenFunc(plr);
        S_LocalSound(SFX_DORCLS, NULL);
    }
    else if(checkCheat(&cheatMassacre, ev->data1, &eat))
    {
        Cht_MassacreFunc(plr);
        S_LocalSound(SFX_DORCLS, NULL);
    }
    else if(checkCheat(&cheatIDKFA, ev->data1, &eat))
    {
        Cht_IDKFAFunc(plr);
        S_LocalSound(SFX_DORCLS, NULL);
    }
    else if(checkCheat(&cheatIDDQD, ev->data1, &eat))
    {
        Cht_IDDQDFunc(plr);
        S_LocalSound(SFX_DORCLS, NULL);
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
    plr->update |= PSF_ARMOR_POINTS | PSF_STATE;
    plr->armorPoints = 200;
    plr->armorType = 2;
}

void Cht_GiveWeaponsFunc(player_t* plr)
{
    int i;

    plr->update |= PSF_OWNED_WEAPONS;
    for(i = 0; i < NUM_WEAPON_TYPES; ++i)
    {
        if(weaponInfo[i][0].mode[0].gameModeBits & gameModeBits)
            plr->weapons[i].owned = true;
    }
}

void Cht_GiveAmmoFunc(player_t* plr)
{
    int i;

    plr->update |= PSF_MAX_AMMO | PSF_AMMO;
    if(!plr->backpack)
    {
        for(i = 0; i < NUM_AMMO_TYPES; ++i)
        {
            plr->ammo[i].max *= 2;
        }
        plr->backpack = true;
    }

    for(i = 0; i < NUM_AMMO_TYPES; ++i)
    {
        plr->ammo[i].owned = plr->ammo[i].max;
    }
}

void Cht_GiveKeysFunc(player_t* plr)
{
    plr->update |= PSF_KEYS;
    plr->keys[KT_YELLOW] = true;
    plr->keys[KT_GREEN] = true;
    plr->keys[KT_BLUE] = true;
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
    int epsd, map;

    epsd = cheat->args[0] - '0';
    map = cheat->args[1] - '0';

    // Catch invalid maps.
    if(!G_ValidateMap(&epsd, &map))
        return false;

    // So be it.
    P_SetMessage(plr, TXT_CHEATWARP, false);
    G_DeferedInitNew(gameSkill, epsd, map);

    // Clear the menu if open.
    Hu_MenuCommand(MCMD_CLOSE);
    briefDisabled = true;
    return true;
}

void Cht_PowerupFunc(player_t* plr)
{
    plr->update |= PSF_POWERS;
    if(plr->powers[PT_WEAPONLEVEL2])
    {
        plr->powers[PT_WEAPONLEVEL2] = 0;
        P_SetMessage(plr, TXT_CHEATPOWEROFF, false);
    }
    else
    {
        int plrnum = plr - players;

        P_InventoryGive(plrnum, IIT_TOMBOFPOWER, true);
        P_InventoryUse(plrnum, IIT_TOMBOFPOWER, true);
        P_SetMessage(plr, TXT_CHEATPOWERON, false);
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
        plr->health = plr->plr->mo->health = MAXCHICKENHEALTH;
    }
    else
    {
        plr->health = plr->plr->mo->health = maxHealth;
    }
    P_SetMessage(plr, TXT_CHEATHEALTH, false);
}

void Cht_InvItem1Func(player_t* plr)
{
    P_SetMessage(plr, TXT_CHEATINVITEMS1, false);
}

void Cht_InvItem2Func(player_t* plr)
{
    P_SetMessage(plr, TXT_CHEATINVITEMS2, false);
}

void Cht_InvItem3Func(player_t* plr, cheatseq_t* cheat)
{
    int i, count, plrnum = plr - players;
    inventoryitemtype_t type;

    type = cheat->args[0] - 'a' + 1;
    count = cheat->args[1] - '0';
    if(type > IIT_NONE && type < NUM_INVENTORYITEM_TYPES && count > 0 && count < 10)
    {
        if(gameMode == shareware && (type == IIT_SUPERHEALTH || type == IIT_TELEPORT))
        {
            P_SetMessage(plr, TXT_CHEATITEMSFAIL, false);
            return;
        }

        for(i = 0; i < count; ++i)
        {
            P_InventoryGive(plrnum, type, false);
        }
        P_SetMessage(plr, TXT_CHEATINVITEMS3, false);
    }
    else
    {   // Bad input
        P_SetMessage(plr, TXT_CHEATITEMSFAIL, false);
    }
}

void Cht_ChickenFunc(player_t* plr)
{
    if(plr->morphTics)
    {
        if(P_UndoPlayerMorph(plr))
        {
            P_SetMessage(plr, TXT_CHEATCHICKENOFF, false);
        }
    }
    else if(P_MorphPlayer(plr))
    {
        P_SetMessage(plr, TXT_CHEATCHICKENON, false);
    }
}

void Cht_MassacreFunc(player_t* plr)
{
    P_Massacre();
    P_SetMessage(plr, TXT_CHEATMASSACRE, false);
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

void Cht_IDDQDFunc(player_t* plr)
{
    P_DamageMobj(plr->plr->mo, NULL, plr->plr->mo, 10000, false);
    P_SetMessage(plr, TXT_CHEATIDDQD, false);
}

// This is the multipurpose cheat ccmd.
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
        Con_Printf(" a - ammo\n");
        Con_Printf(" i - items\n");
        Con_Printf(" h - health\n");
        Con_Printf(" k - keys\n");
        Con_Printf(" p - backpack full of ammo\n");
        Con_Printf(" r - armor\n");
        Con_Printf(" t - tomb of power\n");
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
        case 'a':
            {
            boolean giveAll = true;

            if(i < stuffLen)
            {
                int idx;

                idx = ((int) buf[i+1]) - 48;
                if(idx >= 0 && idx < NUM_AMMO_TYPES)
                {   // Give one specific ammo type.
                    plr->update |= PSF_AMMO;
                    plr->ammo[idx].owned = plr->ammo[idx].max;
                    giveAll = false;
                    i++;
                }
            }

            if(giveAll)
            {
                int j;

                Con_Printf("All ammo given.\n");

                plr->update |= PSF_AMMO;
                for(j = 0; j < NUM_AMMO_TYPES; ++j)
                    plr->ammo[j].owned = plr->ammo[j].max;
            }
            break;
            }

        case 'i': // Inventory items
            {
            int plrnum = plr - players;
            boolean giveAll = true;

            if(i < stuffLen)
            {
                inventoryitemtype_t type = (inventoryitemtype_t) (((int) buf[i+1]) - 48);

                if(type >= IIT_FIRST && type < NUM_INVENTORYITEM_TYPES)
                {   // Give one specific item.
                    if(!(gameMode = shareware &&
                         (type == IIT_SUPERHEALTH || type == IIT_TELEPORT)))
                    {
                        int j;

                        for(j = 0; j < MAXINVITEMCOUNT; ++j)
                        {
                            P_InventoryGive(plrnum, type, false);
                        }
                    }

                    giveAll = false;
                    i++;
                }
            }

            if(giveAll)
            {
                inventoryitemtype_t type;

                for(type = IIT_FIRST; type < NUM_INVENTORYITEM_TYPES; ++type)
                {
                    int i;

                    if(gameMode == shareware &&
                       (type == IIT_SUPERHEALTH || type == IIT_TELEPORT))
                    {
                        continue;
                    }

                    for(i = 0; i < MAXINVITEMCOUNT; ++i)
                    {
                        P_InventoryGive(plrnum, type, false);
                    }
                }

                Con_Printf("All items given.\n");
            }
            break;
            }

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
                    plr->keys[idx] = true;
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
            {
            int j;

            Con_Printf("Ammo backpack given.\n");

            if(!plr->backpack)
            {
                plr->update |= PSF_MAX_AMMO;

                for(j = 0; j < NUM_AMMO_TYPES; ++j)
                {
                    plr->ammo[j].max *= 2;
                }
                plr->backpack = true;
            }

            plr->update |= PSF_AMMO;
            for(j = 0; j < NUM_AMMO_TYPES; ++j)
                plr->ammo[j].owned = plr->ammo[j].max;
            break;
            }

        case 'r':
            Con_Printf("Full armor given.\n");
            plr->update |= PSF_ARMOR_POINTS;
            plr->armorPoints = 200;
            plr->armorType = 2;
            break;

        case 't':
            Cht_PowerupFunc(plr);
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
                    if(weaponInfo[idx][0].mode[0].gameModeBits & gameModeBits)
                    {
                        plr->update |= PSF_OWNED_WEAPONS;
                        plr->weapons[idx].owned = true;
                        giveAll = false;
                    }
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
    Cht_MassacreFunc(&players[CONSOLEPLAYER]);
    return true;
}

DEFCC(CCmdCheatWhere)
{
    Cht_DebugFunc(&players[CONSOLEPLAYER]);
    return true;
}

/**
 * Exit the current map and go to the intermission.
 */
DEFCC(CCmdCheatLeaveMap)
{
    if(!cheatsEnabled())
        return false;

    if(G_GetGameState() != GS_MAP)
    {
        S_LocalSound(SFX_CHAT, NULL);
        Con_Printf("Can only exit a map when in a game!\n");
        return true;
    }

    G_LeaveMap(G_GetMapNumber(gameEpisode, gameMap), 0, false);
    return true;
}

DEFCC(CCmdCheatPig)
{
    if(!cheatsEnabled())
        return false;

    Cht_ChickenFunc(&players[CONSOLEPLAYER]);
    return true;
}
