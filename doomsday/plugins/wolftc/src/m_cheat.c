/* $Id: m_cheat.c 3318 2006-06-13 02:18:47Z danij $
 *
 * Copyright (C) 1993-1996 by id Software, Inc.
 *
 * This source is available for distribution and/or modification
 * only under the terms of the DOOM Source Code License as
 * published by id Software. All rights reserved.
 *
 * The source is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
 * for more details.
 */

/*
 * m_cheat.c: Cheat sequence checking.
 */

// HEADER FILES ------------------------------------------------------------

#include "m_cheat.h"
#include "s_sound.h"
#include "dstrings.h"
#include "g_game.h"
#include "p_inter.h"
#include "p_local.h"
#include "p_setup.h"
#include "f_infine.h"
#include "d_net.h"
#include "st_stuff.h"
#include "mn_def.h"

#include "g_common.h"
#include "p_player.h"

// MACROS ------------------------------------------------------------------

#define ST_MSGWIDTH         52 // Dimensions given in characters.

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern boolean automapactive;
extern int cheating;

extern int messageResponse;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// Massive bunches of cheat shit
//  to keep it from being easy to figure them out.
// Yeah, right...
unsigned char cheat_mus_seq[] = {
    0xb2, 0x26, 0xb6, 0xae, 0xea, 1, 0, 0, 0xff
};

unsigned char cheat_choppers_seq[] = {
    0xb2, 0x26, 0xe2, 0x32, 0xf6, 0x2a, 0x2a, 0xa6, 0x6a, 0xea, 0xff    // id...
};

unsigned char cheat_god_seq[] = {
    0xb2, 0x26, 0x26, 0xaa, 0x26, 0xff  // iddqd
};

unsigned char cheat_ammo_seq[] = {
    0xb2, 0x26, 0xf2, 0x66, 0xa2, 0xff  // idkfa
};

unsigned char cheat_ammonokey_seq[] = {
    0xb2, 0x26, 0x66, 0xa2, 0xff    // idfa
};

// Smashing Pumpkins Into Small Piles Of Putried Debris.
unsigned char cheat_noclip_seq[] = {
    0xb2, 0x26, 0xea, 0x2a, 0xb2,   // idspispopd
    0xea, 0x2a, 0xf6, 0x2a, 0x26, 0xff
};

//
unsigned char cheat_commercial_noclip_seq[] = {
    0xb2, 0x26, 0xe2, 0x36, 0xb2, 0x2a, 0xff    // idclip
};

unsigned char cheat_powerup_seq[7][10] = {
    {0xb2, 0x26, 0x62, 0xa6, 0x32, 0xf6, 0x36, 0x26, 0x6e, 0xff},   // beholdv
    {0xb2, 0x26, 0x62, 0xa6, 0x32, 0xf6, 0x36, 0x26, 0xea, 0xff},   // beholds
    {0xb2, 0x26, 0x62, 0xa6, 0x32, 0xf6, 0x36, 0x26, 0xb2, 0xff},   // beholdi
    {0xb2, 0x26, 0x62, 0xa6, 0x32, 0xf6, 0x36, 0x26, 0x6a, 0xff},   // beholdr
    {0xb2, 0x26, 0x62, 0xa6, 0x32, 0xf6, 0x36, 0x26, 0xa2, 0xff},   // beholda
    {0xb2, 0x26, 0x62, 0xa6, 0x32, 0xf6, 0x36, 0x26, 0x36, 0xff},   // beholdl
    {0xb2, 0x26, 0x62, 0xa6, 0x32, 0xf6, 0x36, 0x26, 0xff}  // behold
};

unsigned char cheat_clev_seq[] = {
    0xb2, 0x26, 0xe2, 0x36, 0xa6, 0x6e, 1, 0, 0, 0xff   // idclev
};

// my position cheat
unsigned char cheat_mypos_seq[] = {
    0xb2, 0x26, 0xb6, 0xba, 0x2a, 0xf6, 0xea, 0xff  // idmypos
};

unsigned char cheat_amap_seq[] = { 0xb2, 0x26, 0x26, 0x2e, 0xff };  // iddt

// Now what?
cheatseq_t cheat_mus = { cheat_mus_seq, 0 };
cheatseq_t cheat_god = { cheat_god_seq, 0 };
cheatseq_t cheat_ammo = { cheat_ammo_seq, 0 };
cheatseq_t cheat_ammonokey = { cheat_ammonokey_seq, 0 };
cheatseq_t cheat_noclip = { cheat_noclip_seq, 0 };
cheatseq_t cheat_commercial_noclip = { cheat_commercial_noclip_seq, 0 };

cheatseq_t cheat_powerup[7] = {
    {cheat_powerup_seq[0], 0},
    {cheat_powerup_seq[1], 0},
    {cheat_powerup_seq[2], 0},
    {cheat_powerup_seq[3], 0},
    {cheat_powerup_seq[4], 0},
    {cheat_powerup_seq[5], 0},
    {cheat_powerup_seq[6], 0}
};

cheatseq_t cheat_choppers = { cheat_choppers_seq, 0 };
cheatseq_t cheat_clev = { cheat_clev_seq, 0 };
cheatseq_t cheat_mypos = { cheat_mypos_seq, 0 };

cheatseq_t cheat_amap = { cheat_amap_seq, 0 };

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static player_t *plyr;  // main player in game

static int firsttime = 1;
static unsigned char cheat_xlate_table[256];

// CODE --------------------------------------------------------------------

void cht_Init(void)
{
    // Nothing to do
}

/*
 * Responds to user input to see if a cheat sequence
 * has been entered. Events are never eaten.
 *
 * @parm ev: ptr to the event to be checked
 */
boolean cht_Responder(event_t *ev)
{
    int i;

    if(gamestate != GS_LEVEL)
        return false;

    plyr = &players[consoleplayer];

    if(gameskill != sk_nightmare && (ev->type == EV_KEY && ev->state == EVS_DOWN))
    {
        if(!IS_NETGAME)
        {
            // b. - enabled for more debug fun.
            // if (gameskill != sk_nightmare) {

            // 'dqd' cheat for toggleable god mode
            if(cht_CheckCheat(&cheat_god, ev->data1))
            {
                cht_GodFunc(plyr);
            }
            // 'fa' cheat for killer fucking arsenal
            else if(cht_CheckCheat(&cheat_ammonokey, ev->data1))
            {
                cht_GiveFunc(plyr, true, true, true, false, &cheat_ammonokey);
                P_SetMessage(plyr, STSTR_FAADDED, false);
            }
            // 'kfa' cheat for key full ammo
            else if(cht_CheckCheat(&cheat_ammo, ev->data1))
            {
                cht_GiveFunc(plyr, true, true, true, true, &cheat_ammo);
                P_SetMessage(plyr, STSTR_KFAADDED, false);
            }
            // 'mus' cheat for changing music
            else if(cht_CheckCheat(&cheat_mus, ev->data1))
            {
                char    buf[3];

                P_SetMessage(plyr, STSTR_MUS, false);
                cht_GetParam(&cheat_mus, buf);
                cht_MusicFunc(plyr, buf);   // Might set plyr->message.
            }
            // Simplified, accepting both "noclip" and "idspispopd".
            // no clipping mode cheat
            else if(cht_CheckCheat(&cheat_noclip, ev->data1) ||
                    cht_CheckCheat(&cheat_commercial_noclip, ev->data1))
            {
                cht_NoClipFunc(plyr);
            }
            // 'behold?' power-up cheats
            for(i = 0; i < 6; i++)
            {
                if(cht_CheckCheat(&cheat_powerup[i], ev->data1))
                {
                    cht_PowerUpFunc(plyr, i);
                    P_SetMessage(plyr, STSTR_BEHOLDX, false);
                }
            }

            // 'behold' power-up menu
            if(cht_CheckCheat(&cheat_powerup[6], ev->data1))
            {
                P_SetMessage(plyr, STSTR_BEHOLD, false);
            }
            // 'choppers' invulnerability & chainsaw
            else if(cht_CheckCheat(&cheat_choppers, ev->data1))
            {
                cht_ChoppersFunc(plyr);
                P_SetMessage(plyr, STSTR_CHOPPERS, false);
            }
            // 'mypos' for player position
            else if(cht_CheckCheat(&cheat_mypos, ev->data1))
            {
                cht_PosFunc(plyr);
            }
        }

        if(cht_CheckCheat(&cheat_clev, ev->data1))
        {   // 'clev' change-level cheat
            char    buf[3];

            cht_GetParam(&cheat_clev, buf);
            cht_WarpFunc(plyr, buf);
        }
    }

    if(automapactive && ev->type == EV_KEY)
    {
        if(ev->state == EVS_DOWN)
        {
            if(!deathmatch && cht_CheckCheat(&cheat_amap, (char) ev->data1))
            {
                cheating = (cheating + 1) % 4;
                return false;
            }
        }
        else if(ev->state == EVS_UP)
        {
            return false;
        }
        else if(ev->state == EVS_REPEAT)
            return true;
    }

    return false;
}

/*
 * Returns a 1 if the cheat was successful, 0 if failed.
 */
int cht_CheckCheat(cheatseq_t * cht, char key)
{
    int     i;
    int     rc = 0;

    if(firsttime)
    {
        firsttime = 0;
        for(i = 0; i < 256; i++)
            cheat_xlate_table[i] = SCRAMBLE(i);
    }

    if(!cht->p)
        cht->p = cht->sequence; // initialize if first time

    if(*cht->p == 0)
        *(cht->p++) = key;
    else if(cheat_xlate_table[key] == *cht->p)
        cht->p++;
    else
        cht->p = cht->sequence;

    if(*cht->p == 1)
        cht->p++;
    else if(*cht->p == 0xff)    // end of sequence character
    {
        cht->p = cht->sequence;
        rc = 1;
    }
    return rc;
}

void cht_GetParam(cheatseq_t * cht, char *buffer)
{

    unsigned char *p, c;

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

void cht_GodFunc(player_t *plyr)
{
    plyr->cheats ^= CF_GODMODE;
    plyr->update |= PSF_STATE;
    if(plyr->cheats & CF_GODMODE)
    {
        if(plyr->plr->mo)
            plyr->plr->mo->health = maxhealth;
        plyr->health = godmodehealth;
        plyr->update |= PSF_HEALTH;
    }
    P_SetMessage(plyr,
                 ((plyr->cheats & CF_GODMODE) ? STSTR_DQDON : STSTR_DQDOFF), false);
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

void cht_GiveFunc(player_t *plyr, boolean weapons, boolean ammo, boolean armor,
                  boolean cards, cheatseq_t *cheat)
{
    int     i;

    if(armor)
    {
        // Support idfa/idkfa DEH Misc values
        if(cheat == &cheat_ammonokey)
        {
            plyr->armorpoints = armorpoints[2]; //200;
            plyr->armortype = armorclass[2];    //2;
        }
        else if(cheat == &cheat_ammo)
        {
            plyr->armorpoints = armorpoints[3];
            plyr->armortype = armorclass[3];
        }
        else
        {
            plyr->armorpoints = armorpoints[1];
            plyr->armortype = armorclass[1];
        }
        plyr->update |= PSF_STATE | PSF_ARMOR_POINTS;
    }
    if(weapons)
    {
        plyr->update |= PSF_OWNED_WEAPONS;
        for(i = 0; i < NUMWEAPONS; i++)
            plyr->weaponowned[i] = true;
    }
    if(ammo)
    {
        plyr->update |= PSF_AMMO;
        for(i = 0; i < NUMAMMO; i++)
            plyr->ammo[i] = plyr->maxammo[i];
    }
    if(cards)
    {
        plyr->update |= PSF_KEYS;
        for(i = 0; i < NUMKEYS; i++)
            plyr->keys[i] = true;
    }
}

void cht_MusicFunc(player_t *plyr, char *buf)
{
    int     off, musnum;

    if(gamemode == commercial)
    {
        off = (buf[0] - '0') * 10 + buf[1] - '0';
        musnum = mus_map01 + off - 1;
        if(off < 1 || off > 35)
            P_SetMessage(plyr, STSTR_NOMUS, false);
        else
            S_StartMusicNum(musnum, true);
    }
    else
    {
        off = (buf[0] - '1') * 9 + (buf[1] - '1');
        musnum = mus_e1m1 + off;
        if(off > 31)
            P_SetMessage(plyr, STSTR_NOMUS, false);
        else
            S_StartMusicNum(musnum, true);
    }
}

void cht_NoClipFunc(player_t *plyr)
{
    plyr->cheats ^= CF_NOCLIP;
    plyr->update |= PSF_STATE;
    P_SetMessage(plyr, ((plyr->cheats & CF_NOCLIP) ? STSTR_NCON : STSTR_NCOFF), false);
}

boolean cht_WarpFunc(player_t *plyr, char *buf)
{
    int     epsd, map;

    if(gamemode == commercial)
    {
        epsd = 1;
        map = (buf[0] - '0') * 10 + buf[1] - '0';
    }
    else
    {
        epsd = buf[0] - '0';
        map = buf[1] - '0';
    }

    // Catch invalid maps.
    if(!G_ValidateMap(&epsd, &map))
        return false;

    // So be it.
    P_SetMessage(plyr, STSTR_CLEV, false);
    G_DeferedInitNew(gameskill, epsd, map);

    // Clear the menu if open
    M_ClearMenus();
    brief_disabled = true;
    return true;
}

boolean cht_PowerUpFunc(player_t *plyr, int i)
{
    plyr->update |= PSF_POWERS;
    if(!plyr->powers[i])
    {
        return P_GivePower(plyr, i);
    }
    else if(i == pw_strength || i == pw_flight)
    {
        return !(P_TakePower(plyr, i));
    }
    else
    {
        plyr->powers[i] = 1;
        return true;
    }
}

void cht_ChoppersFunc(player_t *plyr)
{
    plyr->weaponowned[wp_chainsaw] = true;
    plyr->powers[pw_invulnerability] = true;
}

void cht_PosFunc(player_t *plyr)
{
    static char buf[ST_MSGWIDTH];

    sprintf(buf, "ang=0x%x;x,y,z=(0x%x,0x%x,0x%x)",
            players[consoleplayer].plr->mo->angle,
            players[consoleplayer].plr->mo->pos[VX],
            players[consoleplayer].plr->mo->pos[VY],
            players[consoleplayer].plr->mo->pos[VZ]);
    P_SetMessage(plyr, buf, false);
}

static void CheatDebugFunc(player_t *player, cheat_t * cheat)
{
    char    lumpName[9];
    char    textBuffer[256];
    subsector_t *sub;

    if(!player->plr->mo || !usergame)
        return;

    P_GetMapLumpName(gameepisode, gamemap, lumpName);
    sprintf(textBuffer, "MAP [%s]  X:%5d  Y:%5d  Z:%5d",
            lumpName,
            player->plr->mo->pos[VX] >> FRACBITS,
            player->plr->mo->pos[VY] >> FRACBITS,
            player->plr->mo->pos[VZ] >> FRACBITS);
    P_SetMessage(player, textBuffer, false);

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

/*
 * This is the multipurpose cheat ccmd.
 */
DEFCC(CCmdCheat)
{
    unsigned int i;

    if(argc != 2)
    {
        // Usage information.
        Con_Printf("Usage: cheat (cheat)\nFor example, 'cheat idclev25'.\n");
        return true;
    }
    // Give each of the characters in argument two to the ST event handler.
    for(i = 0; i < strlen(argv[1]); i++)
    {
        event_t ev;

        ev.type = EV_KEY;
        ev.state = EVS_DOWN;
        ev.data1 = argv[1][i];
        ev.data2 = ev.data3 = 0;
        cht_Responder(&ev);
    }
    return true;
}

boolean can_cheat(void)
{
    return !IS_NETGAME;
}

DEFCC(CCmdCheatGod)
{
    if(IS_NETGAME)
    {
        NetCl_CheatRequest("god");
    }
    else
    {
        cht_GodFunc(&players[consoleplayer]);
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
        cht_NoClipFunc(&players[consoleplayer]);
    }
    return true;
}

DEFCC(CCmdCheatSuicide)
{
    if(gamestate != GS_LEVEL)
    {
        S_LocalSound(sfx_oof, NULL);
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

DEFCC(CCmdCheatWarp)
{
    char    buf[10];

    if(!can_cheat())
        return false;
    memset(buf, 0, sizeof(buf));
    if(gamemode == commercial)
    {
        if(argc != 2)
            return false;
        sprintf(buf, "%.2i", atoi(argv[1]));
    }
    else
    {
        if(argc == 2)
        {
            if(strlen(argv[1]) < 2)
                return false;
            strncpy(buf, argv[1], 2);
        }
        else if(argc == 3)
        {
            buf[0] = argv[1][0];
            buf[1] = argv[2][0];
        }
        else
            return false;
    }
    cht_WarpFunc(&players[consoleplayer], buf);
    return true;
}

DEFCC(CCmdCheatReveal)
{
    extern int cheating;
    int     option;

    if(!can_cheat())
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

DEFCC(CCmdCheatGive)
{
    char    buf[100];
    int     i;
    int     weapNum;
    player_t *plyr = &players[consoleplayer];

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
        Con_Printf("Stuff consists of one or more of:\n");
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
        Con_Printf(" 0-8 - weapon\n");
        Con_Printf("Example: 'give arw' corresponds the cheat IDFA.\n");
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
            Con_Printf("Ammo given.\n");
            cht_GiveFunc(plyr, 0, true, 0, 0, NULL);
            break;

        case 'b':
            if(cht_PowerUpFunc(plyr, pw_strength))
                Con_Printf("Your vision blurs! Yaarrrgh!!\n");
            break;

        case 'f':
            if(cht_PowerUpFunc(plyr, pw_flight))
                Con_Printf("You leap into the air, yet you do not fall...\n");
            break;

        case 'g':
            Con_Printf("Light amplification visor given.\n");
            cht_PowerUpFunc(plyr, pw_infrared);
            break;

        case 'i':
            Con_Printf("You feel invincible!\n");
            cht_PowerUpFunc(plyr, pw_invulnerability);
            break;

        case 'k':
            Con_Printf("Key cards and skulls given.\n");
            cht_GiveFunc(plyr, 0, 0, 0, true, NULL);
            break;

        case 'm':
            Con_Printf("Computer area map given.\n");
            cht_PowerUpFunc(plyr, pw_allmap);
            break;

        case 'p':
            Con_Printf("Ammo backpack given.\n");
            P_GiveBackpack(plyr);
            break;

        case 'r':
            Con_Printf("Full armor given.\n");
            cht_GiveFunc(plyr, 0, 0, true, 0, NULL);
            break;

        case 's':
            Con_Printf("Radiation shielding suit given.\n");
            cht_PowerUpFunc(plyr, pw_ironfeet);
            break;

        case 'v':
            Con_Printf("You are suddenly almost invisible!\n");
            cht_PowerUpFunc(plyr, pw_invisibility);
            break;

        case 'w':
            Con_Printf("Weapons given.\n");
            cht_GiveFunc(plyr, true, 0, 0, 0, NULL);
            break;

        default:
            // Individual Weapon
            weapNum = ((int) buf[i]) - 48;
            if(weapNum >= 0 && weapNum < NUMWEAPONS)
            {
               P_GiveWeapon(plyr, weapNum, false);
               return true;

            }
            // Unrecognized
            Con_Printf("What do you mean, '%c'?\n", buf[i]);
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
        return false;           // Can't cheat!
    CheatDebugFunc(players + consoleplayer, NULL);
    return true;
}

/*
 * Exit the current level and goto the intermission.
 */
DEFCC(CCmdCheatExitLevel)
{
    if(!can_cheat())
        return false;           // Can't cheat!
    if(gamestate != GS_LEVEL)
    {
        S_LocalSound(sfx_oof, NULL);
        Con_Printf("Can only exit a level when in a game!\n");
        return true;
    }

    // Exit the level.
    G_LeaveLevel(G_GetLevelNumber(gameepisode, gamemap), 0, false);

    return true;
}
