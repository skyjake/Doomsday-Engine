/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2003-2005 Samuel Villarreal <svkaiser@gmail.com>
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

/**
 * m_cheat.c: Cheat sequence checking.
 */

// HEADER FILES ------------------------------------------------------------

#include <stdlib.h>
#include <string.h>

#include "jdoom64.h"

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

#define SCRAMBLE(a) \
((((a)&1)<<7) + (((a)&2)<<5) + ((a)&4) + (((a)&8)<<1) \
 + (((a)&16)>>1) + ((a)&32) + (((a)&64)>>5) + (((a)&128)>>7))

// TYPES -------------------------------------------------------------------

typedef struct {
    unsigned char*  sequence;
    unsigned char*  p;
} cheatseq_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void Cht_GiveArmorFunc(player_t* plr, cheatseq_t* cheat);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean firstTime = true;
static unsigned char cheatLookup[256];

static unsigned char cheatMusSeq[] = {
    0xb2, 0x26, 0xb6, 0xae, 0xea, 1, 0, 0, 0xff // idmus%%
};

static unsigned char cheatChoppersSeq[] = {
    0xb2, 0x26, 0xe2, 0x32, 0xf6, 0x2a, 0x2a, 0xa6, 0x6a, 0xea, 0xff // idchoppers
};

static unsigned char cheatGodSeq[] = {
    0xb2, 0x26, 0x26, 0xaa, 0x26, 0xff // iddqd
};

static unsigned char cheatAmmoSeq[] = {
    0xb2, 0x26, 0xf2, 0x66, 0xa2, 0xff // idkfa
};

static unsigned char cheatAmmoNoKeySeq[] = {
    0xb2, 0x26, 0x66, 0xa2, 0xff // idfa
};

static unsigned char cheatNoClipSeq[] = {
    0xb2, 0x26, 0xea, 0x2a, 0xb2, 0xea, 0x2a, 0xf6, 0x2a, 0x26, 0xff // idspispopd
};

static unsigned char cheatCommercialNoClipSeq[] = {
    0xb2, 0x26, 0xe2, 0x36, 0xb2, 0x2a, 0xff // idclip
};

static unsigned char cheatPowerupSeq[7][10] = {
    {0xb2, 0x26, 0x62, 0xa6, 0x32, 0xf6, 0x36, 0x26, 0x6e, 0xff}, // idbeholdv
    {0xb2, 0x26, 0x62, 0xa6, 0x32, 0xf6, 0x36, 0x26, 0xea, 0xff}, // idbeholds
    {0xb2, 0x26, 0x62, 0xa6, 0x32, 0xf6, 0x36, 0x26, 0xb2, 0xff}, // idbeholdi
    {0xb2, 0x26, 0x62, 0xa6, 0x32, 0xf6, 0x36, 0x26, 0x6a, 0xff}, // idbeholdr
    {0xb2, 0x26, 0x62, 0xa6, 0x32, 0xf6, 0x36, 0x26, 0xa2, 0xff}, // idbeholda
    {0xb2, 0x26, 0x62, 0xa6, 0x32, 0xf6, 0x36, 0x26, 0x36, 0xff}, // idbeholdl
    {0xb2, 0x26, 0x62, 0xa6, 0x32, 0xf6, 0x36, 0x26, 0xff} // idbehold
};

static unsigned char cheatChangeMapSeq[] = {
    0xb2, 0x26, 0xe2, 0x36, 0xa6, 0x6e, 1, 0, 0, 0xff // idclev
};

static unsigned char cheatMyPosSeq[] = {
    0xb2, 0x26, 0xb6, 0xba, 0x2a, 0xf6, 0xea, 0xff // idmypos
};

static unsigned char cheatAutomapSeq[] = {
    0xb2, 0x26, 0x26, 0x2e, 0xff // iddt
};

static unsigned char cheatLaserSeq[] = {
    0x26, 0x26, 0xea, 0x36, 0xb2, 0xa2, 0xff // ddslia
};

static cheatseq_t cheatMus = {cheatMusSeq, 0};
static cheatseq_t cheatGod = {cheatGodSeq, 0};
static cheatseq_t cheatAmmo = {cheatAmmoSeq, 0};
static cheatseq_t cheatAmmoNoKey = {cheatAmmoNoKeySeq, 0};
static cheatseq_t cheatNoClip = {cheatNoClipSeq, 0};
static cheatseq_t cheatCommercialNoClip = {cheatCommercialNoClipSeq, 0};

static cheatseq_t cheatPowerup[7] = {
    {cheatPowerupSeq[0], 0},
    {cheatPowerupSeq[1], 0},
    {cheatPowerupSeq[2], 0},
    {cheatPowerupSeq[3], 0},
    {cheatPowerupSeq[4], 0},
    {cheatPowerupSeq[5], 0},
    {cheatPowerupSeq[6], 0}
};

static cheatseq_t cheatChoppers = {cheatChoppersSeq, 0};
static cheatseq_t cheatChangeMap = {cheatChangeMapSeq, 0};
static cheatseq_t cheatMyPos = {cheatMyPosSeq, 0};
static cheatseq_t cheatAutomap = {cheatAutomapSeq, 0};
static cheatseq_t cheatLaser = {cheatLaserSeq, 0};

// CODE --------------------------------------------------------------------

static boolean cheatsEnabled(void)
{
    return !IS_NETGAME;
}

/**
 * @return              Non-zero iff the cheat was successful.
 */
static int checkCheat(cheatseq_t* cht, char key)
{
    int i, rc = 0;

    if(firstTime)
    {
        firstTime = false;

        for(i = 0; i < 256; ++i)
            cheatLookup[i] = SCRAMBLE(i);
    }

    if(!cht->p)
        cht->p = cht->sequence; // Initialize if first time.

    if(*cht->p == 0)
        *(cht->p++) = key;
    else if(cheatLookup[(int) key] == *cht->p)
        cht->p++;
    else
        cht->p = cht->sequence;

    if(*cht->p == 1)
    {
        cht->p++;
    }
    else if(*cht->p == 0xff) // End of sequence character.
    {
        cht->p = cht->sequence;
        rc = 1;
    }

    return rc;
}

static void getParam(cheatseq_t* cht, char* buf)
{
    unsigned char* p, c;

    p = cht->sequence;
    while(*(p++) != 1);

    do
    {
        c = *p;
        *(buf++) = c;
        *(p++) = 0;
    }
    while(c && *p != 0xff);

    if(*p == 0xff)
        *buf = 0;
}

void Cht_Init(void)
{
    // Nothing to do
}

/**
 * Responds to user input to see if a cheat sequence has been entered.
 *
 * @param ev            Ptr to the event to be checked.
 *
 * @return              @c true if the event was eaten else @c false.
 */
boolean Cht_Responder(event_t* ev)
{
    int i;
    player_t* plr;

    if(G_GetGameState() != GS_MAP)
        return false;

    plr = &players[CONSOLEPLAYER];

    if(ev->type == EV_KEY && ev->state == EVS_DOWN)
    {
        if(!IS_NETGAME)
        {
            // 'dqd' cheat for toggleable god mode.
            if(checkCheat(&cheatGod, ev->data1))
            {
                Cht_GodFunc(plr);
                return true;
            }
            // 'fa' cheat for killer fucking arsenal.
            else if(checkCheat(&cheatAmmoNoKey, ev->data1))
            {
                Cht_GiveWeaponsFunc(plr);
                Cht_GiveAmmoFunc(plr);
                Cht_GiveArmorFunc(plr, &cheatAmmoNoKey);

                P_SetMessage(plr, STSTR_FAADDED, false);
                return true;
            }
            // 'kfa' cheat for key full ammo.
            else if(checkCheat(&cheatAmmo, ev->data1))
            {
                Cht_GiveWeaponsFunc(plr);
                Cht_GiveAmmoFunc(plr);
                Cht_GiveKeysFunc(plr);
                Cht_GiveArmorFunc(plr, &cheatAmmo);

                P_SetMessage(plr, STSTR_KFAADDED, false);
                return true;
            }
            // 'mus' cheat for changing music.
            else if(checkCheat(&cheatMus, ev->data1))
            {
                char buf[3];

                getParam(&cheatMus, buf);

                if(Cht_MusicFunc(plr, buf))
                {
                    P_SetMessage(plr, STSTR_MUS, false);
                }
                else
                {
                    P_SetMessage(plr, STSTR_NOMUS, false);
                }
                return true;
            }
            // no clipping mode cheat.
            else if(checkCheat(&cheatNoClip, ev->data1) ||
                    checkCheat(&cheatCommercialNoClip, ev->data1))
            {
                Cht_NoClipFunc(plr);
                return true;
            }
            // 'behold?' power-up cheats.
            for(i = 0; i < 6; ++i)
            {
                if(checkCheat(&cheatPowerup[i], ev->data1))
                {
                    Cht_PowerUpFunc(plr, i);
                    P_SetMessage(plr, STSTR_BEHOLDX, false);
                    return true;
                }
            }

            // 'behold' power-up menu.
            if(checkCheat(&cheatPowerup[6], ev->data1))
            {
                P_SetMessage(plr, STSTR_BEHOLD, false);
                return true;
            }
            // 'choppers' invulnerability & chainsaw.
            else if(checkCheat(&cheatChoppers, ev->data1))
            {
                Cht_ChoppersFunc(plr);
                P_SetMessage(plr, STSTR_CHOPPERS, false);
                return true;
            }
            // 'mypos' for plr position.
            else if(checkCheat(&cheatMyPos, ev->data1))
            {
                Cht_MyPosFunc(plr);
                return true;
            }
            // 'ddslia' for laser powerups. // jd64
            else if(checkCheat(&cheatLaser, ev->data1))
            {
                Cht_LaserFunc(plr);
                return true;
            }
        }

        if(checkCheat(&cheatChangeMap, ev->data1))
        {   // 'clev' change map cheat.
            char buf[3];

            getParam(&cheatChangeMap, buf);
            Cht_WarpFunc(plr, buf);
            return true;
        }
    }

    if(!deathmatch && ev->type == EV_KEY && ev->state == EVS_DOWN)
    {
        automapid_t map = AM_MapForPlayer(CONSOLEPLAYER);

        if(AM_IsActive(map) && checkCheat(&cheatAutomap, (char) ev->data1))
        {
            AM_IncMapCheatLevel(map);
            return true;
        }
    }

    return false;
}

void Cht_GodFunc(player_t* plr)
{
    plr->cheats ^= CF_GODMODE;
    plr->update |= PSF_STATE;

    if(P_GetPlayerCheats(plr) & CF_GODMODE)
    {
        if(plr->plr->mo)
            plr->plr->mo->health = maxHealth;
        plr->health = godModeHealth;
        plr->update |= PSF_HEALTH;
    }

    P_SetMessage(plr,
                 ((P_GetPlayerCheats(plr) & CF_GODMODE) ? STSTR_DQDON : STSTR_DQDOFF), false);
}

void Cht_SuicideFunc(player_t* plr)
{
    P_DamageMobj(plr->plr->mo, NULL, NULL, 10000, false);
}

void Cht_GiveArmorFunc(player_t* plr, cheatseq_t* cheat)
{
    // Support idfa/idkfa DEH Misc values.
    if(cheat == &cheatAmmoNoKey)
    {
        plr->armorPoints = armorPoints[2]; //200;
        plr->armorType = armorClass[2]; //2;
    }
    else if(cheat == &cheatAmmo)
    {
        plr->armorPoints = armorPoints[3];
        plr->armorType = armorClass[3];
    }
    else
    {
        plr->armorPoints = armorPoints[1];
        plr->armorType = armorClass[1];
    }
    plr->update |= PSF_STATE | PSF_ARMOR_POINTS;
}

void Cht_GiveWeaponsFunc(player_t* plr)
{
    int i;

    plr->update |= PSF_OWNED_WEAPONS;
    for(i = 0; i < NUM_WEAPON_TYPES; ++i)
        plr->weapons[i].owned = true;
}

void Cht_GiveAmmoFunc(player_t* plr)
{
    int i;

    plr->update |= PSF_AMMO;
    for(i = 0; i < NUM_AMMO_TYPES; ++i)
        plr->ammo[i].owned = plr->ammo[i].max;
}

void Cht_GiveKeysFunc(player_t* plr)
{
    int i;

    plr->update |= PSF_KEYS;
    for(i = 0; i < NUM_KEY_TYPES; ++i)
        plr->keys[i] = true;
}

int Cht_MusicFunc(player_t* plr, char* buf)
{
    int musnum = buf ? strtol(buf, NULL, 10) : 1;
    return S_StartMusicNum(musnum, true);
}

void Cht_NoClipFunc(player_t* plr)
{
    plr->cheats ^= CF_NOCLIP;
    plr->update |= PSF_STATE;
    P_SetMessage(plr,
                 ((P_GetPlayerCheats(plr) & CF_NOCLIP) ? STSTR_NCON : STSTR_NCOFF), false);
}

boolean Cht_WarpFunc(player_t* plr, char* buf)
{
    int epsd, map;

    epsd = 1;
    map = (buf[0] - '0') * 10 + buf[1] - '0';

    // Catch invalid maps.
    if(!G_ValidateMap(&epsd, &map))
        return false;

    // So be it.
    P_SetMessage(plr, STSTR_CLEV, false);
    G_DeferedInitNew(gameSkill, epsd, map);

    // Clear the menu if open.
    Hu_MenuCommand(MCMD_CLOSE);
    briefDisabled = true;
    return true;
}

boolean Cht_PowerUpFunc(player_t* plr, int i)
{
    plr->update |= PSF_POWERS;
    if(!plr->powers[i])
    {
        return P_GivePower(plr, i);
    }
    else if(i == PT_STRENGTH || i == PT_FLIGHT || i == PT_ALLMAP)
    {
        return !(P_TakePower(plr, i));
    }
    else
    {
        plr->powers[i] = 1;
        return true;
    }
}

void Cht_ChoppersFunc(player_t* plr)
{
    plr->weapons[WT_EIGHTH].owned = true;
    plr->powers[PT_INVULNERABILITY] = true;
}

void Cht_MyPosFunc(player_t* plr)
{
    char buf[80];

    sprintf(buf, "ang=0x%x;x,y,z=(%g,%g,%g)",
            players[CONSOLEPLAYER].plr->mo->angle,
            players[CONSOLEPLAYER].plr->mo->pos[VX],
            players[CONSOLEPLAYER].plr->mo->pos[VY],
            players[CONSOLEPLAYER].plr->mo->pos[VZ]);
    P_SetMessage(plr, buf, false);
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

/**
 * Laser powerup cheat code ddslia for all laser powerups.
 * Each time the plr enters the code, plr gains a powerup.
 * When entered again, plr recieves next powerup.
 */
void Cht_LaserFunc(player_t* p)
{
    if(P_InventoryGive(p - players, IIT_DEMONKEY1, true))
    {
        P_SetMessage(p, STSTR_BEHOLDX, false);
        return;
    }

    if(P_InventoryGive(p - players, IIT_DEMONKEY2, true))
    {
        P_SetMessage(p, STSTR_BEHOLDX, false);
        return;
    }

    if(P_InventoryGive(p - players, IIT_DEMONKEY3, true))
        P_SetMessage(p, STSTR_BEHOLDX, false);
}

/**
 * This is the multipurpose cheat ccmd.
 */
DEFCC(CCmdCheat)
{
    size_t i;

    // Give each of the characters in argument two to the ST event handler.
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
    char buf[10];

    if(!cheatsEnabled())
        return false;

    if(argc != 2)
        return false;

    memset(buf, 0, sizeof(buf));
    sprintf(buf, "%.2i", atoi(argv[1]));

    // We don't want that keys are repeated while we wait.
    DD_ClearKeyRepeaters();
    Cht_WarpFunc(&players[CONSOLEPLAYER], buf);
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

    if(IS_NETGAME && !netSvAllowCheats)
        return false;

    if(argc != 2 && argc != 3)
    {
        Con_Printf("Usage:\n  give (stuff)\n");
        Con_Printf("  give (stuff) (plr)\n");
        Con_Printf("Stuff consists of one or more of (type:id). "
                   "If no id; give all of type:\n");
        Con_Printf(" a - ammo\n");
        Con_Printf(" b - berserk\n");
        Con_Printf(" f - the power of flight\n");
        Con_Printf(" g - light amplification visor\n");
        Con_Printf(" h - health\n");
        Con_Printf(" i - invulnerability\n");
        Con_Printf(" k - key cards/skulls\n");
        Con_Printf(" m - computer area map\n");
        Con_Printf(" p - backpack full of ammo\n");
        Con_Printf(" r - armor\n");
        Con_Printf(" s - radiation shielding suit\n");
        Con_Printf(" v - invisibility\n");
        Con_Printf(" w - weapons\n");
        Con_Printf("Example: 'give arw' corresponds the cheat IDFA.\n");
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
                Con_Printf("All ammo given.\n");
                Cht_GiveAmmoFunc(plr);
            }
            break;
            }
        case 'b':
            if(Cht_PowerUpFunc(plr, PT_STRENGTH))
                Con_Printf("Your vision blurs! Yaarrrgh!!\n");
            break;

        case 'f':
            if(Cht_PowerUpFunc(plr, PT_FLIGHT))
                Con_Printf("You leap into the air, yet you do not fall...\n");
            break;

        case 'g':
            Con_Printf("Light amplification visor given.\n");
            Cht_PowerUpFunc(plr, PT_INFRARED);
            break;

        case 'h':
            Con_Printf("Health given.\n");
            P_GiveBody(plr, maxHealth);
            break;

        case 'i':
            Con_Printf("You feel invincible!\n");
            Cht_PowerUpFunc(plr, PT_INVULNERABILITY);
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
                Con_Printf("Key cards and skulls given.\n");
                Cht_GiveKeysFunc(plr);
            }
            break;
            }
        case 'm':
            Con_Printf("Computer area map given.\n");
            Cht_PowerUpFunc(plr, PT_ALLMAP);
            break;

        case 'p':
            Con_Printf("Ammo backpack given.\n");
            P_GiveBackpack(plr);
            break;

        case 'r':
            Con_Printf("Full armor given.\n");
            Cht_GiveArmorFunc(plr, NULL);
            break;

        case 's':
            Con_Printf("Radiation shielding suit given.\n");
            Cht_PowerUpFunc(plr, PT_IRONFEET);
            break;

        case 'v':
            Con_Printf("You are suddenly almost invisible!\n");
            Cht_PowerUpFunc(plr, PT_INVISIBILITY);
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
                    P_GiveWeapon(plr, idx, false);
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
    Con_Printf("%i monsters killed.\n", P_Massacre());
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
        S_LocalSound(SFX_OOF, NULL);
        Con_Printf("Can only exit a map when in a game!\n");
        return true;
    }

    G_LeaveMap(G_GetMapNumber(gameEpisode, gameMap), 0, false);

    return true;
}
