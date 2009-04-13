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

#include "jheretic.h"

#include "dmu_lib.h"
#include "f_infine.h"
#include "d_net.h"
#include "g_common.h"
#include "p_player.h"
#include "p_inventory.h"
#include "am_map.h"
#include "hu_menu.h"
#include "hu_msg.h"
#include "p_user.h"

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
    void          (*func) (player_t* player, struct cheatseq_s* cheat);
    byte*           sequence;
    byte*           pos;
    int             args[2];
    int             currentArg;
} cheatseq_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

boolean Cht_Responder(event_t* ev);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static boolean cheatAddKey(cheatseq_t* cheat, byte key, boolean* eat);

static void cheatGodFunc(player_t* player, cheatseq_t* cheat);
static void cheatNoClipFunc(player_t* player, cheatseq_t* cheat);
static void cheatWeaponsFunc(player_t* player, cheatseq_t* cheat);
static void cheatPowerFunc(player_t* player, cheatseq_t* cheat);
static void cheatHealthFunc(player_t* player, cheatseq_t* cheat);
static void cheatKeysFunc(player_t* player, cheatseq_t* cheat);
static void cheatSoundFunc(player_t* player, cheatseq_t* cheat);
static void cheatTickerFunc(player_t* player, cheatseq_t* cheat);
static void cheatArtifact1Func(player_t* player, cheatseq_t* cheat);
static void cheatArtifact2Func(player_t* player, cheatseq_t* cheat);
static void cheatArtifact3Func(player_t* player, cheatseq_t* cheat);
static void cheatWarpFunc(player_t* player, cheatseq_t* cheat);
static void cheatChickenFunc(player_t* player, cheatseq_t* cheat);
static void cheatMassacreFunc(player_t* player, cheatseq_t* cheat);
static void cheatIDKFAFunc(player_t* player, cheatseq_t* cheat);
static void cheatIDDQDFunc(player_t* player, cheatseq_t* cheat);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

byte cheatCount;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static byte cheatLookUp[256];

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

// Get an artifact 1st stage (ask for type).
static byte cheatArtifact1Seq[] = {
    CHEAT_ENCRYPT('g'),
    CHEAT_ENCRYPT('i'),
    CHEAT_ENCRYPT('m'),
    CHEAT_ENCRYPT('m'),
    CHEAT_ENCRYPT('e'),
    0xff
};

// Get an artifact 2nd stage (ask for count).
static byte cheatArtifact2Seq[] = {
    CHEAT_ENCRYPT('g'),
    CHEAT_ENCRYPT('i'),
    CHEAT_ENCRYPT('m'),
    CHEAT_ENCRYPT('m'),
    CHEAT_ENCRYPT('e'),
    0, 0xff, 0
};

// Get an artifact final stage.
static byte cheatArtifact3Seq[] = {
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

static char cheatAutomap[] = { 'r', 'a', 'v', 'm', 'a', 'p' };

static cheatseq_t cheats[] = {
    {cheatGodFunc, cheatGodSeq, NULL, {0, 0}, 0},
    {cheatNoClipFunc, cheatNoClipSeq, NULL, {0, 0}, 0},
    {cheatWeaponsFunc, cheatWeaponsSeq, NULL, {0, 0}, 0},
    {cheatPowerFunc, cheatPowerSeq, NULL, {0, 0}, 0},
    {cheatHealthFunc, cheatHealthSeq, NULL, {0, 0}, 0},
    {cheatKeysFunc, cheatKeysSeq, NULL, {0, 0}, 0},
    {cheatSoundFunc, cheatSoundSeq, NULL, {0, 0}, 0},
    {cheatTickerFunc, cheatTickerSeq, NULL, {0, 0}, 0},
    {cheatArtifact1Func, cheatArtifact1Seq, NULL, {0, 0}, 0},
    {cheatArtifact2Func, cheatArtifact2Seq, NULL, {0, 0}, 0},
    {cheatArtifact3Func, cheatArtifact3Seq, NULL, {0, 0}, 0},
    {cheatWarpFunc, cheatWarpSeq, NULL, {0, 0}, 0},
    {cheatChickenFunc, cheatChickenSeq, NULL, {0, 0}, 0},
    {cheatMassacreFunc, cheatMassacreSeq, NULL, {0, 0}, 0},
    {cheatIDKFAFunc, cheatIDKFASeq, NULL, {0, 0}, 0},
    {cheatIDDQDFunc, cheatIDDQDSeq, NULL, {0, 0}, 0},
    {NULL, NULL, NULL, {0, 0}, 0} // Terminator.
};

// CODE --------------------------------------------------------------------

void Cht_Init(void)
{
    int                 i;

    for(i = 0; i < 256; i++)
    {
        cheatLookUp[i] = CHEAT_ENCRYPT(i);
    }
}

/**
 * Responds to user input to see if a cheat sequence has been entered.
 *
 * @param               Ptr to the event to be checked.
 *
 * @return              @c true, if the caller should eat the key.
 */
boolean Cht_Responder(event_t* ev)
{
    int                 i;
    byte                key = ev->data1;
    boolean             eat;
    automapid_t         map;

    if(G_GetGameState() != GS_MAP)
        return false;

    if(ev->type != EV_KEY || ev->state != EVS_DOWN)
        return false;

    if(IS_NETGAME || gs.skill == SM_NIGHTMARE)
    {   // Can't cheat in a net-game, or in nightmare mode.
        return false;
    }

    if(players[CONSOLEPLAYER].health <= 0)
    {   // Dead players can't cheat.
        return false;
    }

    eat = false;
    for(i = 0; cheats[i].func != NULL; ++i)
    {
        if(cheatAddKey(&cheats[i], key, &eat))
        {
            cheats[i].func(&players[CONSOLEPLAYER], &cheats[i]);
            S_LocalSound(SFX_DORCLS, NULL);
        }
    }

    map = AM_MapForPlayer(CONSOLEPLAYER);
    if(AM_IsActive(map))
    {
        if(ev->state == EVS_DOWN)
        {
            if(!GAMERULES.deathmatch && cheatAutomap[cheatCount] == ev->data1)
                cheatCount++;
            else
                cheatCount = 0;

            if(cheatCount == 6)
            {
                AM_IncMapCheatLevel(map);
            }
            return false;
        }
        else if(ev->state == EVS_UP)
        {
            return false;
        }
        else if(ev->state == EVS_REPEAT)
        {
            return true;
        }
    }

    return eat;
}

static boolean canCheat(void)
{
    if(IS_NETGAME && !IS_CLIENT && netSvAllowCheats)
        return true;

    return !(gs.skill == SM_NIGHTMARE || (IS_NETGAME /*&& !netcheat */ )
             || players[CONSOLEPLAYER].health <= 0);
}

static boolean cheatAddKey(cheatseq_t *cheat, byte key, boolean *eat)
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
    else if(cheatLookUp[key] == *cheat->pos)
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
    cheatGodFunc(player, NULL);
}

void Cht_NoClipFunc(player_t *player)
{
    cheatNoClipFunc(player, NULL);
}

void Cht_SuicideFunc(player_t *plyr)
{
    P_DamageMobj(plyr->plr->mo, NULL, NULL, 10000, false);
}

int Cht_SuicideResponse(msgresponse_t response, void* context)
{
    if(response == MSG_YES) // Yes
    {
        Cht_SuicideFunc(&players[CONSOLEPLAYER]);
        return true;
    }

    return true;
}

static void cheatGodFunc(player_t *player, cheatseq_t * cheat)
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

static void cheatNoClipFunc(player_t *player, cheatseq_t * cheat)
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

static void cheatWeaponsFunc(player_t *player, cheatseq_t *cheat)
{
    int                 i;

    player->update |=
        PSF_ARMOR_POINTS | PSF_STATE | PSF_MAX_AMMO | PSF_AMMO |
        PSF_OWNED_WEAPONS;

    player->armorPoints = 200;
    player->armorType = 2;
    if(!player->backpack)
    {
        for(i = 0; i < NUM_AMMO_TYPES; ++i)
        {
            player->ammo[i].max *= 2;
        }
        player->backpack = true;
    }
    for(i = 0; i < NUM_WEAPON_TYPES; ++i)
    {
        if(weaponInfo[i][0].mode[0].gameModeBits & gs.gameModeBits)
            player->weapons[i].owned = true;
    }

    for(i = 0; i < NUM_AMMO_TYPES; ++i)
    {
        player->ammo[i].owned = player->ammo[i].max;
    }
    P_SetMessage(player, TXT_CHEATWEAPONS, false);
}

static void cheatPowerFunc(player_t *player, cheatseq_t *cheat)
{
    player->update |= PSF_POWERS;
    if(player->powers[PT_WEAPONLEVEL2])
    {
        player->powers[PT_WEAPONLEVEL2] = 0;
        P_SetMessage(player, TXT_CHEATPOWEROFF, false);
    }
    else
    {
        P_InventoryGive(player, AFT_TOMBOFPOWER);
        P_InventoryUse(player, AFT_TOMBOFPOWER);
        P_SetMessage(player, TXT_CHEATPOWERON, false);
    }
}

static void cheatHealthFunc(player_t* player, cheatseq_t* cheat)
{
    player->update |= PSF_HEALTH;
    if(player->morphTics)
    {
        player->health = player->plr->mo->health = MAXCHICKENHEALTH;
    }
    else
    {
        player->health = player->plr->mo->health = maxHealth;
    }
    P_SetMessage(player, TXT_CHEATHEALTH, false);
}

static void cheatKeysFunc(player_t* player, cheatseq_t* cheat)
{
    player->update |= PSF_KEYS;
    player->keys[KT_YELLOW] = true;
    player->keys[KT_GREEN] = true;
    player->keys[KT_BLUE] = true;
    P_SetMessage(player, TXT_CHEATKEYS, false);
}

static void cheatSoundFunc(player_t* player, cheatseq_t* cheat)
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

static void cheatTickerFunc(player_t* player, cheatseq_t* cheat)
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

static void cheatArtifact1Func(player_t *player, cheatseq_t * cheat)
{
    P_SetMessage(player, TXT_CHEATARTIFACTS1, false);
}

static void cheatArtifact2Func(player_t *player, cheatseq_t * cheat)
{
    P_SetMessage(player, TXT_CHEATARTIFACTS2, false);
}

static void cheatArtifact3Func(player_t *player, cheatseq_t * cheat)
{
    int     i;
    artitype_e type;
    int     count;

    type = cheat->args[0] - 'a' + 1;
    count = cheat->args[1] - '0';
    if(type == 26 && count == 0)
    {                           // All artifacts
        for(type = AFT_NONE + 1; type < NUM_ARTIFACT_TYPES; type++)
        {
            if(gs.gameMode == shareware && (type == AFT_SUPERHEALTH || type == AFT_TELEPORT))
            {
                continue;
            }

            for(i = 0; i < MAXARTICOUNT; i++)
            {
                P_InventoryGive(player, type);
            }
        }
        P_SetMessage(player, TXT_CHEATARTIFACTS3, false);
    }
    else if(type > AFT_NONE && type < NUM_ARTIFACT_TYPES && count > 0 && count < 10)
    {
        if(gs.gameMode == shareware && (type == AFT_SUPERHEALTH || type == AFT_TELEPORT))
        {
            P_SetMessage(player, TXT_CHEATARTIFACTSFAIL, false);
            return;
        }

        for(i = 0; i < count; ++i)
        {
            P_InventoryGive(player, type);
        }
        P_SetMessage(player, TXT_CHEATARTIFACTS3, false);
    }
    else
    {                           // Bad input
        P_SetMessage(player, TXT_CHEATARTIFACTSFAIL, false);
    }
}

static void cheatWarpFunc(player_t *player, cheatseq_t *cheat)
{
    int                 episode;
    int                 map;

    episode = cheat->args[0] - '0';
    map = cheat->args[1] - '0';
    if(G_ValidateMap(&episode, &map))
    {
        G_DeferedInitNew(gs.skill, episode, map);
        Hu_MenuCommand(MCMD_CLOSE);
        P_SetMessage(player, TXT_CHEATWARP, false);
    }
}

static void cheatChickenFunc(player_t *player, cheatseq_t *cheat)
{
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

static void cheatMassacreFunc(player_t *player, cheatseq_t * cheat)
{
    P_Massacre();
    P_SetMessage(player, TXT_CHEATMASSACRE, false);
}

static void cheatIDKFAFunc(player_t *player, cheatseq_t * cheat)
{
    int     i;

    if(player->morphTics)
    {
        return;
    }
    for(i = 1; i < NUM_WEAPON_TYPES; ++i)
    {
        player->weapons[i].owned = false;
    }
    player->pendingWeapon = WT_FIRST;
    P_SetMessage(player, TXT_CHEATIDKFA, false);
}

static void cheatIDDQDFunc(player_t* player, cheatseq_t* cheat)
{
    P_DamageMobj(player->plr->mo, NULL, player->plr->mo, 10000, false);
    P_SetMessage(player, TXT_CHEATIDDQD, false);
}

static void CheatDebugFunc(player_t* player, cheatseq_t* cheat)
{
    char                lumpName[9];
    char                textBuffer[256];
    subsector_t*        sub;

    if(!player->plr->mo || !gs.userGame)
        return;

    P_GetMapLumpName(gs.episode, gs.map.id, lumpName);
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
    cheatGodFunc(players + CONSOLEPLAYER, NULL);
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
    cheatNoClipFunc(players + CONSOLEPLAYER, NULL);
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

            if(plr->pState == PST_DEAD)
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
        Con_Printf(" a - ammo\n");
        Con_Printf(" f - artifacts\n");
        Con_Printf(" h - health\n");
        Con_Printf(" k - keys\n");
        Con_Printf(" p - backpack full of ammo\n");
        Con_Printf(" r - armor\n");
        Con_Printf(" t - tomb of power\n");
        Con_Printf(" w - weapons\n");
        Con_Printf("Example: 'give akw' gives artifacts, keys and weapons.\n");
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
        case 'a':
            {
            boolean             giveAll = true;

            if(i < stuffLen)
            {
                int                 idx;

                idx = ((int) buf[i+1]) - 48;
                if(idx >= 0 && idx < NUM_AMMO_TYPES)
                {   // Give one specific ammo type.
                    plyr->update |= PSF_AMMO;
                    plyr->ammo[idx].owned = plyr->ammo[idx].max;
                    giveAll = false;
                    i++;
                }
            }

            if(giveAll)
            {
                int                 j;

                Con_Printf("All ammo given.\n");

                plyr->update |= PSF_AMMO;
                for(j = 0; j < NUM_AMMO_TYPES; ++j)
                    plyr->ammo[j].owned = plyr->ammo[j].max;
            }
            break;
            }

        case 'f':
            {
            cheatseq_t cheat;

            Con_Printf("Artifacts given.\n");

            cheat.args[0] = 'z';
            cheat.args[1] = '0';
            cheatArtifact3Func(plyr, &cheat);
            break;
            }

        case 'h':
            Con_Printf("Health given.\n");
            cheatHealthFunc(plyr, NULL);
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
                    plyr->keys[idx] = true;
                    giveAll = false;
                    i++;
                }
            }

            if(giveAll)
            {
                Con_Printf("All Keys given.\n");
                cheatKeysFunc(plyr, NULL);
            }
            break;
            }

        case 'p':
            {
            int                 j;

            Con_Printf("Ammo backpack given.\n");

            if(!plyr->backpack)
            {
                plyr->update |= PSF_MAX_AMMO;

                for(j = 0; j < NUM_AMMO_TYPES; ++j)
                {
                    plyr->ammo[j].max *= 2;
                }
                plyr->backpack = true;
            }

            plyr->update |= PSF_AMMO;
            for(j = 0; j < NUM_AMMO_TYPES; ++j)
                plyr->ammo[j].owned = plyr->ammo[j].max;
            break;
            }

        case 'r':
            Con_Printf("Full armor given.\n");
            plyr->update |= PSF_ARMOR_POINTS;
            plyr->armorPoints = 200;
            plyr->armorType = 2;
            break;

        case 't':
            cheatPowerFunc(plyr, NULL);
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
                    if(weaponInfo[idx][0].mode[0].gameModeBits & gs.gameModeBits)
                    {
                        plyr->update |= PSF_OWNED_WEAPONS;
                        plyr->weapons[idx].owned = true;
                        giveAll = false;
                    }
                    i++;
                }
            }

            if(giveAll)
            {
                Con_Printf("All weapons given.\n");
                cheatWeaponsFunc(plyr, NULL);
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
    cheatseq_t cheat;
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
    cheatWarpFunc(players + CONSOLEPLAYER, &cheat);
    return true;
}

/**
 * Exit the current map and go to the intermission.
 */
DEFCC(CCmdCheatLeaveMap)
{
    if(!canCheat())
        return false; // Can't cheat!

    if(G_GetGameState() != GS_MAP)
    {
        S_LocalSound(SFX_CHAT, NULL);
        Con_Printf("Can only exit a map when in a game!\n");
        return true;
    }

    // Exit the level.
    G_LeaveMap(G_GetMapNumber(gs.episode, gs.map.id), 0, false);

    return true;
}

DEFCC(CCmdCheatPig)
{
    if(!canCheat())
        return false; // Can't cheat!

    cheatChickenFunc(players + CONSOLEPLAYER, NULL);
    return true;
}

DEFCC(CCmdCheatMassacre)
{
    if(!canCheat())
        return false; // Can't cheat!

    DD_ClearKeyRepeaters();
    cheatMassacreFunc(players + CONSOLEPLAYER, NULL);
    return true;
}

DEFCC(CCmdCheatWhere)
{
    if(!canCheat())
        return false; // Can't cheat!

    CheatDebugFunc(players+CONSOLEPLAYER, NULL);
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
