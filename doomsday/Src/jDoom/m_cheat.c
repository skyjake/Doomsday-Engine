// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
//
// DESCRIPTION:
//	Cheat sequence checking.
//
//-----------------------------------------------------------------------------

#include "m_cheat.h"
#include "s_sound.h"
#include "dstrings.h"
#include "g_game.h"
#include "p_inter.h"
#include "p_local.h"
#include "p_setup.h"
#include "f_infine.h"
#include "d_net.h"

// Dimensions given in characters.
#define ST_MSGWIDTH			52

//
// CHEAT SEQUENCE PACKAGE
//

static int		firsttime = 1;
static unsigned char	cheat_xlate_table[256];


//
// Called in st_stuff module, which handles the input.
// Returns a 1 if the cheat was successful, 0 if failed.
//
int
cht_CheckCheat
( cheatseq_t*	cht,
 char		key )
{
    int i;
    int rc = 0;
	
    if (firsttime)
    {
		firsttime = 0;
		for (i=0;i<256;i++) cheat_xlate_table[i] = SCRAMBLE(i);
    }
	
    if (!cht->p)
		cht->p = cht->sequence; // initialize if first time
	
    if (*cht->p == 0)
		*(cht->p++) = key;
    else if
		(cheat_xlate_table[(unsigned char)key] == *cht->p) cht->p++;
    else
		cht->p = cht->sequence;
	
    if (*cht->p == 1)
		cht->p++;
    else if (*cht->p == 0xff) // end of sequence character
    {
		cht->p = cht->sequence;
		rc = 1;
    }
	
    return rc;
}

void
cht_GetParam
( cheatseq_t*	cht,
 char*		buffer )
{
	
    unsigned char *p, c;
	
    p = cht->sequence;
    while (*(p++) != 1);
    
    do
    {
		c = *p;
		*(buffer++) = c;
		*(p++) = 0;
    }
    while (c && *p!=0xff );
	
    if (*p==0xff)
		*buffer = 0;
	
}


void cht_GodFunc(player_t *plyr)
{
	plyr->cheats ^= CF_GODMODE;
	plyr->update |= PSF_STATE;
	if (plyr->cheats & CF_GODMODE)
	{
		if (plyr->plr->mo) plyr->plr->mo->health = maxhealth;
		plyr->health = maxhealth;
		plyr->update |= PSF_HEALTH;
	}
	P_SetMessage(plyr, (plyr->cheats & CF_GODMODE)? 
		STSTR_DQDON : STSTR_DQDOFF);
}

void cht_GiveFunc(player_t *plyr, boolean weapons, boolean ammo,
				  boolean armor, boolean cards)
{
	int		i;

	if(armor)
	{
		plyr->armorpoints = armorpoints[1]; //200;
		plyr->armortype = 2;
		plyr->update |= PSF_STATE | PSF_ARMOR_POINTS;
	}
	if(weapons)
	{
		plyr->update |= PSF_OWNED_WEAPONS;
		for (i=0;i<NUMWEAPONS;i++)
			plyr->weaponowned[i] = true;
	}
	if(ammo)
	{
		plyr->update |= PSF_AMMO;
		for (i=0;i<NUMAMMO;i++)
			plyr->ammo[i] = plyr->maxammo[i];
	}
	if(cards)
	{
		plyr->update |= PSF_KEYS;
		for (i=0;i<NUMCARDS;i++)
			plyr->cards[i] = true;
	}
}

void cht_MusicFunc(player_t *plyr, char *buf)
{
	int	off, musnum;

	if (gamemode == commercial)
	{
		off = (buf[0]-'0')*10 + buf[1]-'0';
		musnum = mus_runnin + off - 1;
		if (off < 1 || off > 35)
			P_SetMessage(plyr, STSTR_NOMUS);
		else
			S_StartMusicNum(musnum, true);
	}
	else
	{
		off = (buf[0]-'1')*9 + (buf[1]-'1');
		musnum = mus_e1m1 + off;
		if (off > 31)
			P_SetMessage(plyr, STSTR_NOMUS);
		else
			S_StartMusicNum(musnum, true);
	}
}

void cht_NoClipFunc(player_t *plyr)
{
	plyr->cheats ^= CF_NOCLIP;
	plyr->update |= PSF_STATE;
	P_SetMessage(plyr, (plyr->cheats & CF_NOCLIP)? STSTR_NCON : STSTR_NCOFF);
}

boolean cht_WarpFunc(player_t *plyr, char *buf)
{
	int		epsd, map;

	if (gamemode == commercial)
	{
		epsd = 1;
		map = (buf[0] - '0')*10 + buf[1] - '0';
	}
	else
	{
		epsd = buf[0] - '0';
		map = buf[1] - '0';
	}
	
	// Catch invalid maps.
	if(!G_ValidateMap(&epsd, &map)) return false;

	// So be it.
	P_SetMessage(plyr, STSTR_CLEV);
	G_DeferedInitNew(gameskill, epsd, map);
	brief_disabled = true;
	return true;
}

void cht_PowerUpFunc(player_t *plyr, int i)
{
	plyr->update |= PSF_POWERS;
	if (!plyr->powers[i])
		P_GivePower( plyr, i);
	else if (i!=pw_strength)
		plyr->powers[i] = 1;
	else
		plyr->powers[i] = 0;
}

void cht_ChoppersFunc(player_t *plyr)
{
	plyr->weaponowned[wp_chainsaw] = true;
	plyr->powers[pw_invulnerability] = true;
}

void cht_PosFunc(player_t *plyr)
{
	static char	buf[ST_MSGWIDTH];

	sprintf(buf, "ang=0x%x;x,y=(0x%x,0x%x)",
		players[consoleplayer].plr->mo->angle,
		players[consoleplayer].plr->mo->x,
		players[consoleplayer].plr->mo->y);
	P_SetMessage(plyr, buf);
}



