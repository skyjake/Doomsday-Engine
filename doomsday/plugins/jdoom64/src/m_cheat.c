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

#include "dmu_lib.h"
#include "f_infine.h"
#include "d_net.h"
#include "g_common.h"
#include "p_player.h"
#include "am_map.h"
#include "hu_menu.h"
#include "hu_msg.h"
#include "p_user.h"
#include "p_start.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

unsigned char cheat_mus_seq[] = {
    0xb2, 0x26, 0xb6, 0xae, 0xea, 1, 0, 0, 0xff
};

unsigned char cheat_choppers_seq[] = {
    0xb2, 0x26, 0xe2, 0x32, 0xf6, 0x2a, 0x2a, 0xa6, 0x6a, 0xea, 0xff // id...
};

unsigned char cheat_god_seq[] = {
    0xb2, 0x26, 0x26, 0xaa, 0x26, 0xff // iddqd
};

unsigned char cheat_ammo_seq[] = {
    0xb2, 0x26, 0xf2, 0x66, 0xa2, 0xff // idkfa
};

unsigned char cheat_ammonokey_seq[] = {
    0xb2, 0x26, 0x66, 0xa2, 0xff // idfa
};

// Smashing Pumpkins Into Small Piles Of Putried Debris.
unsigned char cheat_noclip_seq[] = {
    0xb2, 0x26, 0xea, 0x2a, 0xb2, // idspispopd
    0xea, 0x2a, 0xf6, 0x2a, 0x26, 0xff
};

//
unsigned char cheat_commercial_noclip_seq[] = {
    0xb2, 0x26, 0xe2, 0x36, 0xb2, 0x2a, 0xff // idclip
};

unsigned char cheat_powerup_seq[7][10] = {
    {0xb2, 0x26, 0x62, 0xa6, 0x32, 0xf6, 0x36, 0x26, 0x6e, 0xff}, // beholdv
    {0xb2, 0x26, 0x62, 0xa6, 0x32, 0xf6, 0x36, 0x26, 0xea, 0xff}, // beholds
    {0xb2, 0x26, 0x62, 0xa6, 0x32, 0xf6, 0x36, 0x26, 0xb2, 0xff}, // beholdi
    {0xb2, 0x26, 0x62, 0xa6, 0x32, 0xf6, 0x36, 0x26, 0x6a, 0xff}, // beholdr
    {0xb2, 0x26, 0x62, 0xa6, 0x32, 0xf6, 0x36, 0x26, 0xa2, 0xff}, // beholda
    {0xb2, 0x26, 0x62, 0xa6, 0x32, 0xf6, 0x36, 0x26, 0x36, 0xff}, // beholdl
    {0xb2, 0x26, 0x62, 0xa6, 0x32, 0xf6, 0x36, 0x26, 0xff} // behold
};

unsigned char cheat_clev_seq[] = {
    0xb2, 0x26, 0xe2, 0x36, 0xa6, 0x6e, 1, 0, 0, 0xff // idclev
};

unsigned char cheat_mypos_seq[] = {
    0xb2, 0x26, 0xb6, 0xba, 0x2a, 0xf6, 0xea, 0xff // idmypos
};

// jd64
unsigned char cheat_laser_seq[] = {
    0x26, 0x26, 0xea, 0x36, 0xb2, 0xa2, 0xff // ddslia
};

unsigned char cheat_amap_seq[] = {
    0xb2, 0x26, 0x26, 0x2e, 0xff // iddt
};

cheatseq_t cheat_mus = {cheat_mus_seq, 0};
cheatseq_t cheat_god = {cheat_god_seq, 0};
cheatseq_t cheat_ammo = {cheat_ammo_seq, 0};
cheatseq_t cheat_ammonokey = {cheat_ammonokey_seq, 0};
cheatseq_t cheat_noclip = {cheat_noclip_seq, 0};
cheatseq_t cheat_commercial_noclip = {cheat_commercial_noclip_seq, 0};

cheatseq_t cheat_powerup[7] = {
    {cheat_powerup_seq[0], 0},
    {cheat_powerup_seq[1], 0},
    {cheat_powerup_seq[2], 0},
    {cheat_powerup_seq[3], 0},
    {cheat_powerup_seq[4], 0},
    {cheat_powerup_seq[5], 0},
    {cheat_powerup_seq[6], 0}
};

cheatseq_t cheat_choppers = {cheat_choppers_seq, 0};
cheatseq_t cheat_clev = {cheat_clev_seq, 0};
cheatseq_t cheat_mypos = {cheat_mypos_seq, 0};
cheatseq_t cheat_laser = {cheat_laser_seq, 0}; // jd64
cheatseq_t cheat_amap = {cheat_amap_seq, 0};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static player_t *plyr;  // main player in game

static int firsttime = 1;
static unsigned char cheat_xlate_table[256];

// CODE --------------------------------------------------------------------

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
boolean Cht_Responder(event_t *ev)
{
    int                 i;

    if(G_GetGameState() != GS_MAP)
        return false;

    plyr = &players[CONSOLEPLAYER];

    if(ev->type == EV_KEY && ev->state == EVS_DOWN)
    {
        if(!IS_NETGAME)
        {
            // 'dqd' cheat for toggleable god mode.
            if(Cht_CheckCheat(&cheat_god, ev->data1))
            {
                Cht_GodFunc(plyr);
            }
            // 'fa' cheat for killer fucking arsenal.
            else if(Cht_CheckCheat(&cheat_ammonokey, ev->data1))
            {
                Cht_GiveFunc(plyr, true, true, true, false, &cheat_ammonokey);
                P_SetMessage(plyr, STSTR_FAADDED, false);
            }
            // 'kfa' cheat for key full ammo.
            else if(Cht_CheckCheat(&cheat_ammo, ev->data1))
            {
                Cht_GiveFunc(plyr, true, true, true, true, &cheat_ammo);
                P_SetMessage(plyr, STSTR_KFAADDED, false);
            }
            // 'mus' cheat for changing music.
            else if(Cht_CheckCheat(&cheat_mus, ev->data1))
            {
                char                buf[3];

                Cht_GetParam(&cheat_mus, buf);
                Cht_MusicFunc(plyr, buf); // Might set plyr->message.
            }
            // Simplified, accepting both "noclip" and "idspispopd".
            // no clipping mode cheat.
            else if(Cht_CheckCheat(&cheat_noclip, ev->data1) ||
                    Cht_CheckCheat(&cheat_commercial_noclip, ev->data1))
            {
                Cht_NoClipFunc(plyr);
            }
            // 'behold?' power-up cheats.
            for(i = 0; i < 6; ++i)
            {
                if(Cht_CheckCheat(&cheat_powerup[i], ev->data1))
                {
                    Cht_PowerUpFunc(plyr, i);
                    P_SetMessage(plyr, STSTR_BEHOLDX, false);
                }
            }

            // 'behold' power-up menu.
            if(Cht_CheckCheat(&cheat_powerup[6], ev->data1))
            {
                P_SetMessage(plyr, STSTR_BEHOLD, false);
            }
            // 'choppers' invulnerability & chainsaw.
            else if(Cht_CheckCheat(&cheat_choppers, ev->data1))
            {
                Cht_ChoppersFunc(plyr);
                P_SetMessage(plyr, STSTR_CHOPPERS, false);
            }
            // 'mypos' for player position.
            else if(Cht_CheckCheat(&cheat_mypos, ev->data1))
            {
                Cht_MyPosFunc(plyr);
            }
            // 'ddslia' for laser powerups. // jd64
            else if(Cht_CheckCheat(&cheat_laser, ev->data1))
            {
                cht_LaserFunc(plyr);
            }
        }

        if(Cht_CheckCheat(&cheat_clev, ev->data1))
        {   // 'clev' change-level cheat.
            char                buf[3];

            Cht_GetParam(&cheat_clev, buf);
            Cht_WarpFunc(plyr, buf);
        }
    }

    return false;
}

boolean can_cheat(void)
{
    return !IS_NETGAME;
}

/**
 * @return              Non-zero iff the cheat was successful.
 */
int Cht_CheckCheat(cheatseq_t *cht, char key)
{
    int                 i, rc = 0;

    if(firsttime)
    {
        firsttime = 0;

        for(i = 0; i < 256; ++i)
        {
            cheat_xlate_table[i] = SCRAMBLE(i);
        }
    }

    if(!cht->p)
        cht->p = cht->sequence; // Initialize if first time.

    if(*cht->p == 0)
        *(cht->p++) = key;
    else if(cheat_xlate_table[(int) key] == *cht->p)
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

void Cht_GetParam(cheatseq_t *cht, char *buffer)
{

    unsigned char          *p, c;

    p = cht->sequence;
    while(*(p++) != 1);

    do
    {
        c = *p;
        *(buffer++) = c;
        *(p++) = 0;
    }
    while(c && *p != 0xff);

    if(*p == 0xff)
        *buffer = 0;
}

void Cht_GodFunc(player_t *plyr)
{
    plyr->cheats ^= CF_GODMODE;
    plyr->update |= PSF_STATE;

    if(P_GetPlayerCheats(plyr) & CF_GODMODE)
    {
        if(plyr->plr->mo)
            plyr->plr->mo->health = maxHealth;

        plyr->health = godModeHealth;
        plyr->update |= PSF_HEALTH;
    }

    P_SetMessage(plyr,
                 ((P_GetPlayerCheats(plyr) & CF_GODMODE) ? STSTR_DQDON : STSTR_DQDOFF), false);
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

void Cht_GiveFunc(player_t *plyr, boolean weapons, boolean ammo,
                  boolean armor, boolean cards, cheatseq_t *cheat)
{
    int                 i;

    if(armor)
    {
        // Support idfa/idkfa DEH Misc values.
        if(cheat == &cheat_ammonokey)
        {
            plyr->armorPoints = armorPoints[2]; //200;
            plyr->armorType = armorClass[2]; //2;
        }
        else if(cheat == &cheat_ammo)
        {
            plyr->armorPoints = armorPoints[3];
            plyr->armorType = armorClass[3];
        }
        else
        {
            plyr->armorPoints = armorPoints[1];
            plyr->armorType = armorClass[1];
        }
        plyr->update |= PSF_STATE | PSF_ARMOR_POINTS;
    }

    if(weapons)
    {
        plyr->update |= PSF_OWNED_WEAPONS;
        for(i = 0; i < NUM_WEAPON_TYPES; ++i)
            plyr->weapons[i].owned = true;
    }

    if(ammo)
    {
        plyr->update |= PSF_AMMO;
        for(i = 0; i < NUM_AMMO_TYPES; ++i)
            plyr->ammo[i].owned = plyr->ammo[i].max;
    }

    if(cards)
    {
        plyr->update |= PSF_KEYS;
        for(i = 0; i < NUM_KEY_TYPES; ++i)
            plyr->keys[i] = true;
    }
}

void Cht_MusicFunc(player_t* plr, char* buf)
{
    int                 musnum = 1;

    if(buf)
        musnum += strtol(buf, NULL, 10) - 1;

    if(S_StartMusicNum(musnum, true))
    {
        P_SetMessage(plr, STSTR_MUS, false);
    }
    else
    {
        P_SetMessage(plyr, STSTR_NOMUS, false);
    }
}

void Cht_NoClipFunc(player_t *plyr)
{
    plyr->cheats ^= CF_NOCLIP;
    plyr->update |= PSF_STATE;
    P_SetMessage(plyr,
                 ((P_GetPlayerCheats(plyr) & CF_NOCLIP) ? STSTR_NCON : STSTR_NCOFF), false);
}

boolean Cht_WarpFunc(player_t *plyr, char *buf)
{
    int                 epsd, map;

    epsd = 1;
    map = (buf[0] - '0') * 10 + buf[1] - '0';

    // Catch invalid maps.
    if(!G_ValidateMap(&epsd, &map))
        return false;

    // So be it.
    P_SetMessage(plyr, STSTR_CLEV, false);
    G_DeferedInitNew(gs.skill, epsd, map);

    // Clear the menu if open.
    Hu_MenuCommand(MCMD_CLOSE);
    briefDisabled = true;
    return true;
}

boolean Cht_PowerUpFunc(player_t *plyr, int i)
{
    plyr->update |= PSF_POWERS;
    if(!plyr->powers[i])
    {
        return P_GivePower(plyr, i);
    }
    else if(i == PT_STRENGTH || i == PT_FLIGHT || i == PT_ALLMAP)
    {
        return !(P_TakePower(plyr, i));
    }
    else
    {
        plyr->powers[i] = 1;
        return true;
    }
}

void Cht_ChoppersFunc(player_t *plyr)
{
    plyr->weapons[WT_EIGHTH].owned = true;
    plyr->powers[PT_INVULNERABILITY] = true;
}

void Cht_MyPosFunc(player_t *plyr)
{
#define BUFSIZE         (52) // Dimensions given in characters.

    char                buf[BUFSIZE];

    sprintf(buf, "ang=0x%x;x,y,z=(0x%g,0x%g,0x%g)",
            players[CONSOLEPLAYER].plr->mo->angle,
            players[CONSOLEPLAYER].plr->mo->pos[VX],
            players[CONSOLEPLAYER].plr->mo->pos[VY],
            players[CONSOLEPLAYER].plr->mo->pos[VZ]);
    P_SetMessage(plyr, buf, false);

#undef BUFSIZE
}

/**
 * d64tc
 * kaiser - laser powerup cheat code ddslia for all laser powerups.
 * Each time the player enters the code, player gains a powerup.
 * when entered again, player recieves next powerup.
 */
void cht_LaserFunc(player_t *plyr)
{
    laserpw_t           arti;

    if(plyr->artifacts[it_laserpw1] && plyr->artifacts[it_laserpw2] &&
       plyr->artifacts[it_laserpw3])
    {
        P_SetMessage(plyr, STSTR_BEHOLDX, false); // No more!
        return;
    }

    if(!plyr->artifacts[it_laserpw1])
        arti = it_laserpw1;
    else if(!plyr->artifacts[it_laserpw2])
        arti = it_laserpw2;
    else
        arti = it_laserpw3;

    plyr->artifacts[arti] = 1;
    P_SetMessage(plyr, STSTR_BEHOLDX, false);
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
            lumpName, player->plr->mo->pos[VX], player->plr->mo->pos[VY],
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

/**
 * This is the multipurpose cheat ccmd.
 */
DEFCC(CCmdCheat)
{
    uint                i;

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
    {
        NetCl_CheatRequest("god");
    }
    else
    {
        Cht_GodFunc(&players[CONSOLEPLAYER]);
    }
    return true;
}

DEFCC(CCmdCheatNoClip)
{
    if(IS_NETGAME)
    {
        NetCl_CheatRequest("noclip");
    }
    else
    {
        Cht_NoClipFunc(&players[CONSOLEPLAYER]);
    }
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

DEFCC(CCmdCheatWarp)
{
    char                buf[10];

    if(!can_cheat())
        return false;

    if(argc != 2)
        return false;

    memset(buf, 0, sizeof(buf));
    sprintf(buf, "%.2i", atoi(argv[1]));

    Cht_WarpFunc(&players[CONSOLEPLAYER], buf);
    return true;
}

DEFCC(CCmdCheatReveal)
{
    int                 option;
    automapid_t         map;

    if(!can_cheat())
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

DEFCC(CCmdCheatGive)
{
    char                buf[100];
    player_t*           plyr = &players[CONSOLEPLAYER];
    size_t              i, stuffLen;

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
        Con_Printf("  give (stuff) (player)\n");
        Con_Printf("Stuff consists of one or more of (type:id). "
                   "If no id; give all of type:\n");
        Con_Printf(" a - ammo\n");
        Con_Printf(" b - berserk\n");
        Con_Printf(" f - the power of flight\n");
        Con_Printf(" g - light amplification visor\n");
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
                Con_Printf("All ammo given.\n");
                Cht_GiveFunc(plyr, 0, true, 0, 0, NULL);
            }
            break;
            }
        case 'b':
            if(Cht_PowerUpFunc(plyr, PT_STRENGTH))
                Con_Printf("Your vision blurs! Yaarrrgh!!\n");
            break;

        case 'f':
            if(Cht_PowerUpFunc(plyr, PT_FLIGHT))
                Con_Printf("You leap into the air, yet you do not fall...\n");
            break;

        case 'g':
            Con_Printf("Light amplification visor given.\n");
            Cht_PowerUpFunc(plyr, PT_INFRARED);
            break;

        case 'i':
            Con_Printf("You feel invincible!\n");
            Cht_PowerUpFunc(plyr, PT_INVULNERABILITY);
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
                Con_Printf("Key cards and skulls given.\n");
                Cht_GiveFunc(plyr, 0, 0, 0, true, NULL);
            }
            break;
            }
        case 'm':
            Con_Printf("Computer area map given.\n");
            Cht_PowerUpFunc(plyr, PT_ALLMAP);
            break;

        case 'p':
            Con_Printf("Ammo backpack given.\n");
            P_GiveBackpack(plyr);
            break;

        case 'r':
            Con_Printf("Full armor given.\n");
            Cht_GiveFunc(plyr, 0, 0, true, 0, NULL);
            break;

        case 's':
            Con_Printf("Radiation shielding suit given.\n");
            Cht_PowerUpFunc(plyr, PT_IRONFEET);
            break;

        case 'v':
            Con_Printf("You are suddenly almost invisible!\n");
            Cht_PowerUpFunc(plyr, PT_INVISIBILITY);
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
                    P_GiveWeapon(plyr, idx, false);
                    giveAll = false;
                    i++;
                }
            }

            if(giveAll)
            {
                Con_Printf("All weapons given.\n");
                Cht_GiveFunc(plyr, true, 0, 0, 0, NULL);
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
    if(!can_cheat())
        return false; // Can't cheat!

    CheatDebugFunc(players + CONSOLEPLAYER, NULL);
    return true;
}

/**
 * Exit the current map and go to the intermission.
 */
DEFCC(CCmdCheatLeaveMap)
{
    if(!can_cheat())
        return false; // Can't cheat!

    if(G_GetGameState() != GS_MAP)
    {
        S_LocalSound(SFX_OOF, NULL);
        Con_Printf("Can only exit a map when in a game!\n");
        return true;
    }

    // Exit the map.
    G_LeaveMap(G_GetMapNumber(gs.episode, gs.map.id), 0, false);

    return true;
}
