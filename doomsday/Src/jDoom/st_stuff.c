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
// $Log$
// Revision 1.13  2005/05/29 13:07:39  danij
// Fullscreen no HUD mode. Fullscreen floating status bar mode. Alpha blended HUD and statusbar. Show Doom guy's face in fullscreen HUD.
//
// Revision 1.12  2005/01/03 13:35:32  skyjake
// Mac issues resolved
//
// Revision 1.11  2004/05/30 08:42:42  skyjake
// Tweaked indentation style
//
// Revision 1.10  2004/05/29 18:19:59  skyjake
// Refined indentation style
//
// Revision 1.9  2004/05/29 09:53:29  skyjake
// Consistent style (using GNU Indent)
//
// Revision 1.8  2004/05/28 19:52:58  skyjake
// Finished switch from branch-1-7 to trunk, hopefully everything is fine
//
// Revision 1.5.2.2  2004/05/16 10:01:37  skyjake
// Merged good stuff from branch-nix for the final 1.7.15
//
// Revision 1.5.2.1.2.2  2003/11/22 18:09:10  skyjake
// Cleanup
//
// Revision 1.5.2.1.2.1  2003/11/19 17:07:14  skyjake
// Modified to compile with gcc and -DUNIX
//
// Revision 1.5.2.1  2003/09/07 22:21:04  skyjake
// STARMS background is not used in deathmatch, cleanup
//
// Revision 1.5  2003/08/24 00:16:48  skyjake
// Netgame-aware cheats, "give p" => backpack
//
// Revision 1.4  2003/08/18 16:40:37  skyjake
// Precache fonts
//
// Revision 1.3  2003/06/03 15:19:52  skyjake
// Cleanups
//
// Revision 1.2  2003/02/27 23:14:33  skyjake
// Obsolete jDoom files removed
//
// Revision 1.1  2003/02/26 19:22:09  skyjake
// Initial checkin
//
//
// DESCRIPTION:
//  Status bar code.
//  Does the face/direction indicator animatin.
//  Does palette indicators as well (red pain/berserk, bright pickup)
//
//-----------------------------------------------------------------------------

#ifdef WIN32
#  pragma warning(disable:4761)
#endif

#include <stdio.h>
#include <stdlib.h>

#include "doomdef.h"
#include "d_config.h"
#include "d_net.h"

#include "m_random.h"

#include "g_game.h"

#include "st_stuff.h"
#include "st_lib.h"
#include "r_local.h"

#include "p_local.h"
#include "p_inter.h"

#include "am_map.h"
#include "m_menu.h"
#include "m_cheat.h"
#include "hu_stuff.h"

#include "s_sound.h"

// State.
#include "doomstat.h"

// Data.
#include "dstrings.h"

//
// STATUS BAR DATA
//

enum {
	HOT_TLEFT,
	HOT_TRIGHT,
	HOT_BRIGHT,
	HOT_BLEFT
};

// Radiation suit, green shift.
#define RADIATIONPAL		13

// N/256*100% probability
//  that the normal face state will change
#define ST_FACEPROBABILITY		96

// For Responder
#define ST_TOGGLECHAT		KEY_ENTER

// Location of status bar
#define ST_X				0
#define ST_X2				104

#define ST_FX  			144
#define ST_FY  			169

// Should be set to patch width
//  for tall numbers later on
#define ST_TALLNUMWIDTH		(tallnum[0]->width)

// Number of status faces.
#define ST_NUMPAINFACES		5
#define ST_NUMSTRAIGHTFACES	3
#define ST_NUMTURNFACES		2
#define ST_NUMSPECIALFACES		3

#define ST_FACESTRIDE \
          (ST_NUMSTRAIGHTFACES+ST_NUMTURNFACES+ST_NUMSPECIALFACES)

#define ST_NUMEXTRAFACES		2

#define ST_NUMFACES \
          (ST_FACESTRIDE*ST_NUMPAINFACES+ST_NUMEXTRAFACES)

#define ST_TURNOFFSET		(ST_NUMSTRAIGHTFACES)
#define ST_OUCHOFFSET		(ST_TURNOFFSET + ST_NUMTURNFACES)
#define ST_EVILGRINOFFSET		(ST_OUCHOFFSET + 1)
#define ST_RAMPAGEOFFSET		(ST_EVILGRINOFFSET + 1)
#define ST_GODFACE			(ST_NUMPAINFACES*ST_FACESTRIDE)
#define ST_DEADFACE			(ST_GODFACE+1)

#define ST_FACESX			143
#define ST_FACESY			168

#define ST_EVILGRINCOUNT		(2*TICRATE)
#define ST_STRAIGHTFACECOUNT	(TICRATE/2)
#define ST_TURNCOUNT		(1*TICRATE)
#define ST_OUCHCOUNT		(1*TICRATE)
#define ST_RAMPAGEDELAY		(2*TICRATE)

#define ST_MUCHPAIN			20

// Location and size of statistics,
//  justified according to widget type.
// Problem is, within which space? STbar? Screen?
// Note: this could be read in by a lump.
//       Problem is, is the stuff rendered
//       into a buffer,
//       or into the frame buffer?

// AMMO number pos.
#define ST_AMMOWIDTH		3
#define ST_AMMOX			44
#define ST_AMMOY			171

// HEALTH number pos.
#define ST_HEALTHWIDTH		3
#define ST_HEALTHX			90
#define ST_HEALTHY			171

// Weapon pos.
#define ST_ARMSX			111
#define ST_ARMSY			172
#define ST_ARMSBGX			104
#define ST_ARMSBGY			168
#define ST_ARMSXSPACE		12
#define ST_ARMSYSPACE		10

// Frags pos.
#define ST_FRAGSX			138
#define ST_FRAGSY			171
#define ST_FRAGSWIDTH		2

// ARMOR number pos.
#define ST_ARMORWIDTH		3
#define ST_ARMORX			221
#define ST_ARMORY			171

// Key icon positions.
#define ST_KEY0WIDTH		8
#define ST_KEY0HEIGHT		5
#define ST_KEY0X			239
#define ST_KEY0Y			171
#define ST_KEY1WIDTH		ST_KEY0WIDTH
#define ST_KEY1X			239
#define ST_KEY1Y			181
#define ST_KEY2WIDTH		ST_KEY0WIDTH
#define ST_KEY2X			239
#define ST_KEY2Y			191

// Ammunition counter.
#define ST_AMMO0WIDTH		3
#define ST_AMMO0HEIGHT		6
#define ST_AMMO0X			288
#define ST_AMMO0Y			173
#define ST_AMMO1WIDTH		ST_AMMO0WIDTH
#define ST_AMMO1X			288
#define ST_AMMO1Y			179
#define ST_AMMO2WIDTH		ST_AMMO0WIDTH
#define ST_AMMO2X			288
#define ST_AMMO2Y			191
#define ST_AMMO3WIDTH		ST_AMMO0WIDTH
#define ST_AMMO3X			288
#define ST_AMMO3Y			185

// Indicate maximum ammunition.
// Only needed because backpack exists.
#define ST_MAXAMMO0WIDTH		3
#define ST_MAXAMMO0HEIGHT		5
#define ST_MAXAMMO0X		314
#define ST_MAXAMMO0Y		173
#define ST_MAXAMMO1WIDTH		ST_MAXAMMO0WIDTH
#define ST_MAXAMMO1X		314
#define ST_MAXAMMO1Y		179
#define ST_MAXAMMO2WIDTH		ST_MAXAMMO0WIDTH
#define ST_MAXAMMO2X		314
#define ST_MAXAMMO2Y		191
#define ST_MAXAMMO3WIDTH		ST_MAXAMMO0WIDTH
#define ST_MAXAMMO3X		314
#define ST_MAXAMMO3Y		185

// pistol
#define ST_WEAPON0X			110
#define ST_WEAPON0Y			172

// shotgun
#define ST_WEAPON1X			122
#define ST_WEAPON1Y			172

// chain gun
#define ST_WEAPON2X			134
#define ST_WEAPON2Y			172

// missile launcher
#define ST_WEAPON3X			110
#define ST_WEAPON3Y			181

// plasma gun
#define ST_WEAPON4X			122
#define ST_WEAPON4Y			181

 // bfg
#define ST_WEAPON5X			134
#define ST_WEAPON5Y			181

// WPNS title
#define ST_WPNSX			109
#define ST_WPNSY			191

 // DETH title
#define ST_DETHX			109
#define ST_DETHY			191

#define ST_MAPWIDTH	\
    (strlen(mapnames[(gameepisode-1)*9+(gamemap-1)]))

#define ST_MAPTITLEX \
    (SCREENWIDTH - ST_MAPWIDTH * ST_CHATFONTWIDTH)

#define ST_MAPTITLEY		0
#define ST_MAPHEIGHT		1

// slide statusbar amount 1.0 is fully open
static float showbar = 0.0f;

// fullscreen hud alpha value
static float hudalpha = 0.0f;

// main player in game
static player_t *plyr;

// ST_Start() has just been called
static boolean st_firsttime;

// whether to use alpha blending
static boolean st_blended = false;

// used to execute ST_Init() only once
static int veryfirsttime = 1;

// lump number for PLAYPAL
static int lu_palette;

// used for timing
static unsigned int st_clock;

// used for making messages go away
static int st_msgcounter = 0;

// used when in chat 
static st_chatstateenum_t st_chatstate;

// whether in automap or first-person
static st_stateenum_t st_gamestate;

// whether left-side main status bar is active
static boolean st_statusbaron;

// whether status bar chat is active
static boolean st_chat;

// value of st_chat before message popped up
static boolean st_oldchat;

// whether chat window has the cursor on
static boolean st_cursoron;

// !deathmatch
static boolean st_notdeathmatch;

// !deathmatch && st_statusbaron
static boolean st_armson;

// !deathmatch
static boolean st_fragson;

// main bar left
static dpatch_t sbar;

// 0-9, tall numbers
static dpatch_t tallnum[10];

// tall % sign
static dpatch_t tallpercent;

// 0-9, short, yellow (,different!) numbers
static dpatch_t shortnum[10];

// 3 key-cards, 3 skulls
static dpatch_t keys[NUMKEYS];

// face status patches
static dpatch_t faces[ST_NUMFACES];

// face background
static dpatch_t faceback;

 // main bar right
static dpatch_t armsbg;

// weapon ownership patches
static dpatch_t arms[6][2];

// ready-weapon widget
static st_number_t w_ready;

 // in deathmatch only, summary of frags stats
static st_number_t w_frags;

// health widget
static st_percent_t w_health;

// arms background
static st_binicon_t w_armsbg;

// weapon ownership widgets
static st_multicon_t w_arms[6];

// face status widget
static st_multicon_t w_faces;

// keycard widgets
static st_multicon_t w_keyboxes[3];

// armor widget
static st_percent_t w_armor;

// ammo widgets
static st_number_t w_ammo[4];

// max ammo widgets
static st_number_t w_maxammo[4];

 // number of frags so far in deathmatch
static int st_fragscount;

// used to use appopriately pained face
static int st_oldhealth = -1;

// used for evil grin
static boolean oldweaponsowned[NUMWEAPONS];

 // count until face changes
static int st_facecount = 0;

// current face index, used by w_faces
static int st_faceindex = 0;

// holds key-type for each key box on bar
static int keyboxes[3];

// a random number per tick
static int st_randomnumber;

// 
extern char *mapnames[];

//
// STATUS BAR CODE
//
void    ST_Stop(void);

void ST_refreshBackground(void)
{

	int x, y, w, h;
	float cw, cw2, ch;

	GL_SetPatch(sbar.lump);

	if(st_blended && ((cfg.statusbarAlpha < 1.0f) && (cfg.statusbarAlpha > 0.0f)))
	{
		// Alpha blended status bar, we'll need to cut it up into smaller bits...

		gl.Color4f(1, 1, 1, cfg.statusbarAlpha);

		gl.Begin(DGL_QUADS);

			// (up to faceback if deathmatch, else ST_ARMS)
			x = ST_X;
			y = ST_Y;
			w = st_armson ? 104 : 143;
			h = 32;
			cw = st_armson ? 0.325f : 0.446875f;

			gl.TexCoord2f(0, 0);
			gl.Vertex2f(x, y);
			gl.TexCoord2f(cw, 0);
			gl.Vertex2f(x + w, y);
			gl.TexCoord2f(cw, 1);
			gl.Vertex2f(x + w, y + h);
			gl.TexCoord2f(0, 1);
			gl.Vertex2f(x, y + h);

			if(IS_NETGAME)
			{
				// (fiddly little bit above faceback)
				x = ST_X + 144;
				y = ST_Y;
				w = 35;
				h = 1;
				cw = 0.446875f;
				cw2 = 0.55625f;
				ch = 0.03125f;

				gl.TexCoord2f(cw, 0);
				gl.Vertex2f(x, y);
				gl.TexCoord2f(cw2, 0);
				gl.Vertex2f(x + w, y);
				gl.TexCoord2f(cw2, ch);
				gl.Vertex2f(x + w, y + h);
				gl.TexCoord2f(cw, ch);
				gl.Vertex2f(x, y + h);

				// (after faceback)
				x = ST_X + 178;
				y = ST_Y;
				w = 142;
				h = 32;
				cw = 0.55625f;

			} else {
				// (including area behind the face)
				x = ST_X + 144;
				y = ST_Y;
				w = 176;
				h = 32;
				cw = 0.45f;
			}

			gl.TexCoord2f(cw, 0);
			gl.Vertex2f(x, y);
			gl.TexCoord2f(1, 0);
			gl.Vertex2f(x + w, y);
			gl.TexCoord2f(1, 1);
			gl.Vertex2f(x + w, y + h);
			gl.TexCoord2f(cw, 1);
			gl.Vertex2f(x, y + h);

			gl.End();

			if(st_armson)  // arms baground
			{
				GL_DrawPatch_CS(ST_ARMSBGX, ST_ARMSBGY, armsbg.lump);
			}

			if(IS_NETGAME) // faceback
				GL_DrawPatch_CS(ST_FX, ST_Y+1, faceback.lump);

	} else if(cfg.statusbarAlpha != 0.0f){

			// we can just render the full thing as normal
			GL_DrawPatch(ST_X, ST_Y, sbar.lump);

			if(st_armson)  // arms baground
				GL_DrawPatch(ST_ARMSBGX, ST_ARMSBGY, armsbg.lump);

			if(IS_NETGAME) // faceback
				GL_DrawPatch(ST_FX, ST_Y+1, faceback.lump);
	}

}

// Respond to keyboard input events,
//  intercept cheats.
boolean ST_Responder(event_t *ev)
{

	// Filter automap on/off.
	if(ev->type == ev_keyup && ((ev->data1 & 0xffff0000) == AM_MSGHEADER))
	{
		switch (ev->data1)
		{
		case AM_MSGENTERED:
			st_gamestate = AutomapState;
			st_firsttime = true;
			break;

		case AM_MSGEXITED:
			st_gamestate = FirstPersonState;
			break;
		}
	}

	// if a user keypress...
	else if(ev->type == ev_keydown)
	{
		cht_Responder(ev);
	}

	return false;
}

int ST_calcPainOffset(void)
{
	int     health;
	static int lastcalc;
	static int oldhealth = -1;

	health = plyr->health > 100 ? 100 : plyr->health;

	if(health != oldhealth)
	{
		lastcalc = ST_FACESTRIDE * (((100 - health) * ST_NUMPAINFACES) / 101);
		oldhealth = health;
	}
	return lastcalc;
}

//
// This is a not-very-pretty routine which handles
//  the face states and their timing.
// the precedence of expressions is:
//  dead > evil grin > turned head > straight ahead
//
void ST_updateFaceWidget(void)
{
	int     i;
	angle_t badguyangle;
	angle_t diffang;
	static int lastattackdown = -1;
	static int priority = 0;
	boolean doevilgrin;

	if(priority < 10)
	{
		// dead
		if(!plyr->health)
		{
			priority = 9;
			st_faceindex = ST_DEADFACE;
			st_facecount = 1;
		}
	}

	if(priority < 9)
	{
		if(plyr->bonuscount)
		{
			// picking up bonus
			doevilgrin = false;

			for(i = 0; i < NUMWEAPONS; i++)
			{
				if(oldweaponsowned[i] != plyr->weaponowned[i])
				{
					doevilgrin = true;
					oldweaponsowned[i] = plyr->weaponowned[i];
				}
			}
			if(doevilgrin)
			{
				// evil grin if just picked up weapon
				priority = 8;
				st_facecount = ST_EVILGRINCOUNT;
				st_faceindex = ST_calcPainOffset() + ST_EVILGRINOFFSET;
			}
		}

	}

	if(priority < 8)
	{
		if(plyr->damagecount && plyr->attacker &&
		   plyr->attacker != plyr->plr->mo)
		{
			// being attacked
			priority = 7;

			if(plyr->health - st_oldhealth > ST_MUCHPAIN)
			{
				st_facecount = ST_TURNCOUNT;
				st_faceindex = ST_calcPainOffset() + ST_OUCHOFFSET;
			}
			else
			{
				badguyangle =
					R_PointToAngle2(plyr->plr->mo->x, plyr->plr->mo->y,
									plyr->attacker->x, plyr->attacker->y);

				if(badguyangle > plyr->plr->mo->angle)
				{
					// whether right or left
					diffang = badguyangle - plyr->plr->mo->angle;
					i = diffang > ANG180;
				}
				else
				{
					// whether left or right
					diffang = plyr->plr->mo->angle - badguyangle;
					i = diffang <= ANG180;
				}				// confusing, aint it?

				st_facecount = ST_TURNCOUNT;
				st_faceindex = ST_calcPainOffset();

				if(diffang < ANG45)
				{
					// head-on    
					st_faceindex += ST_RAMPAGEOFFSET;
				}
				else if(i)
				{
					// turn face right
					st_faceindex += ST_TURNOFFSET;
				}
				else
				{
					// turn face left
					st_faceindex += ST_TURNOFFSET + 1;
				}
			}
		}
	}

	if(priority < 7)
	{
		// getting hurt because of your own damn stupidity
		if(plyr->damagecount)
		{
			if(plyr->health - st_oldhealth > ST_MUCHPAIN)
			{
				priority = 7;
				st_facecount = ST_TURNCOUNT;
				st_faceindex = ST_calcPainOffset() + ST_OUCHOFFSET;
			}
			else
			{
				priority = 6;
				st_facecount = ST_TURNCOUNT;
				st_faceindex = ST_calcPainOffset() + ST_RAMPAGEOFFSET;
			}

		}

	}

	if(priority < 6)
	{
		// rapid firing
		if(plyr->attackdown)
		{
			if(lastattackdown == -1)
				lastattackdown = ST_RAMPAGEDELAY;
			else if(!--lastattackdown)
			{
				priority = 5;
				st_faceindex = ST_calcPainOffset() + ST_RAMPAGEOFFSET;
				st_facecount = 1;
				lastattackdown = 1;
			}
		}
		else
			lastattackdown = -1;

	}

	if(priority < 5)
	{
		// invulnerability
		if((plyr->cheats & CF_GODMODE) || plyr->powers[pw_invulnerability])
		{
			priority = 4;

			st_faceindex = ST_GODFACE;
			st_facecount = 1;

		}

	}

	// look left or look right if the facecount has timed out
	if(!st_facecount)
	{
		st_faceindex = ST_calcPainOffset() + (st_randomnumber % 3);
		st_facecount = ST_STRAIGHTFACECOUNT;
		priority = 0;
	}

	st_facecount--;

}

void ST_updateWidgets(void)
{
	static int largeammo = 1994;	// means "n/a"
	int     i;

	// must redirect the pointer if the ready weapon has changed.
	if(weaponinfo[plyr->readyweapon].ammo == am_noammo)
		w_ready.num = &largeammo;
	else
		w_ready.num = &plyr->ammo[weaponinfo[plyr->readyweapon].ammo];

	w_ready.data = plyr->readyweapon;

	// update keycard multiple widgets
	for(i = 0; i < 3; i++)
	{
		keyboxes[i] = plyr->keys[i] ? i : -1;

		if(plyr->keys[i + 3])
			keyboxes[i] = i + 3;
	}

	// refresh everything if this is him coming back to life
	ST_updateFaceWidget();

	// used by the w_armsbg widget
	st_notdeathmatch = !deathmatch;

	// used by w_arms[] widgets
	st_armson = st_statusbaron && !deathmatch;

	// used by w_frags widget
	st_fragson = deathmatch && st_statusbaron;
	st_fragscount = 0;

	for(i = 0; i < MAXPLAYERS; i++)
	{
		if(i != consoleplayer)
			st_fragscount += plyr->frags[i];
		else
			st_fragscount -= plyr->frags[i];
	}

	// get rid of chat window if up because of message
	if(!--st_msgcounter)
		st_chat = st_oldchat;

}

void ST_Ticker(void)
{

	st_clock++;
	st_randomnumber = M_Random();
	ST_updateWidgets();
	st_oldhealth = plyr->health;

}

#define FMAKERGBA(r,g,b,a) ( (byte)(0xff*r) + ((byte)(0xff*g)<<8) + ((byte)(0xff*b)<<16) + ((byte)(0xff*a)<<24) )

int D_GetFilterColor(int filter)
{
	int     rgba = 0;

	// We have to choose the right color and alpha.
	if(filter >= STARTREDPALS && filter < STARTREDPALS + NUMREDPALS)
		// Red.
		rgba = FMAKERGBA(1, 0, 0, filter / 9.0);
	else if(filter >= STARTBONUSPALS && filter < STARTBONUSPALS + NUMBONUSPALS)
		// Gold.
		rgba = FMAKERGBA(1, .8, .5, (filter - STARTBONUSPALS + 1) / 16.0);
	else if(filter == RADIATIONPAL)
		// Green.
		rgba = FMAKERGBA(0, .7, 0, .15f);
	else if(filter)
		Con_Error("D_SetFilter: Real strange filter number: %d.\n", filter);
	return rgba;
}

static int st_palette = 0;

void ST_doPaletteStuff(void)
{
	int     palette;
	int     cnt;
	int     bzc;

	cnt = plyr->damagecount;

	if(plyr->powers[pw_strength])
	{
		// slowly fade the berzerk out
		bzc = 12 - (plyr->powers[pw_strength] >> 6);

		if(bzc > cnt)
			cnt = bzc;
	}

	if(cnt)
	{
		palette = (cnt + 7) >> 3;

		if(palette >= NUMREDPALS)
			palette = NUMREDPALS - 1;

		palette += STARTREDPALS;
	}

	else if(plyr->bonuscount)
	{
		palette = (plyr->bonuscount + 7) >> 3;

		if(palette >= NUMBONUSPALS)
			palette = NUMBONUSPALS - 1;

		palette += STARTBONUSPALS;
	}

	else if(plyr->powers[pw_ironfeet] > 4 * 32 ||
			plyr->powers[pw_ironfeet] & 8)
		palette = RADIATIONPAL;
	else
		palette = 0;

	if(palette != st_palette)
	{
		st_palette = palette;
		plyr->plr->filter = D_GetFilterColor(palette);	// $democam
	}
}

void ST_drawWidgets(boolean refresh)
{
	int     i;

	// used by w_arms[] widgets
	st_armson = st_statusbaron && !deathmatch;

	// used by w_frags widget
	st_fragson = deathmatch && st_statusbaron;

	STlib_updateNum(&w_ready, refresh);

	for(i = 0; i < 4; i++)
	{
		STlib_updateNum(&w_ammo[i], refresh);
		STlib_updateNum(&w_maxammo[i], refresh);
	}

	STlib_updatePercent(&w_health, refresh);
	STlib_updatePercent(&w_armor, refresh);

	for(i = 0; i < 6; i++)
		STlib_updateMultIcon(&w_arms[i], refresh);

	STlib_updateMultIcon(&w_faces, refresh);

	for(i = 0; i < 3; i++)
		STlib_updateMultIcon(&w_keyboxes[i], refresh);

	STlib_updateNum(&w_frags, refresh);

}

void ST_doRefresh(void)
{
	st_firsttime = false;

	if(cfg.sbarscale < 20 || (cfg.sbarscale == 20 && showbar < 1.0f))
	{
		float fscale = cfg.sbarscale / 20.0f;
		float h = 200 * (1 - fscale);

		gl.MatrixMode(DGL_MODELVIEW);
		gl.PushMatrix();
		gl.Translatef(160 - 320 * fscale / 2, h /showbar, 0);
		gl.Scalef(fscale, fscale, 1);
	}

	// draw status bar background
	ST_refreshBackground();

	// and refresh all widgets
	ST_drawWidgets(true);

	if(cfg.sbarscale < 20 || (cfg.sbarscale == 20 && showbar < 1.0f))
	{
		// Restore the normal modelview matrix.
		gl.MatrixMode(DGL_MODELVIEW);
		gl.PopMatrix();
	}
}

void ST_HUDSpriteSize(int sprite, int *w, int *h)
{
	spriteinfo_t sprInfo;

	R_GetSpriteInfo(sprite, 0, &sprInfo);
	*w = sprInfo.width;
	*h = sprInfo.height;

	if(sprite == SPR_ROCK)
	{
		// Must scale it a bit.
		*w /= 1.5;
		*h /= 1.5;
	}
}

void ST_drawHUDSprite(int sprite, int x, int y, int hotspot, float alpha)
{
	spriteinfo_t sprInfo;
	int     w, h;

	R_GetSpriteInfo(sprite, 0, &sprInfo);
	w = sprInfo.width;
	h = sprInfo.height;
	if(sprite == SPR_ROCK)
	{
		w /= 1.5;
		h /= 1.5;
	}
	switch (hotspot)
	{
	case HOT_BRIGHT:
		y -= h;

	case HOT_TRIGHT:
		x -= w;
		break;

	case HOT_BLEFT:
		y -= h;
		break;
	}
	gl.Color4f(1, 1, 1, alpha );
	GL_DrawPSprite(x, y, sprite == SPR_ROCK ? 1 / 1.5 : 1, false,
				   sprInfo.lump);
}

void ST_doFullscreenStuff(void)
{
	player_t *plr = &players[displayplayer];
	char    buf[20];
	int     w, h, pos = 0, spr, i;
	int     h_width = 320 / cfg.hudScale, h_height = 200 / cfg.hudScale;
	float textalpha = hudalpha - ( 1 - cfg.hudColor[3]);
	float iconalpha = hudalpha - ( 1 - cfg.hudIconAlpha);
	int     ammo_sprite[NUMAMMO] = {
		SPR_AMMO,
		SPR_SBOX,
		SPR_CELL,
		SPR_ROCK
	};

	if(IS_NETGAME && deathmatch && cfg.hudShown[HUD_FRAGS])
	{
		// Display the frag counter.
		i = 199 - 8;
		if(cfg.hudShown[HUD_HEALTH] || cfg.hudShown[HUD_AMMO])
		{
			i -= 18 * cfg.hudScale;
		}
		sprintf(buf, "FRAGS:%i", st_fragscount);
		M_WriteText2(2, i, buf, hu_font_a, cfg.hudColor[0], cfg.hudColor[1],
					 cfg.hudColor[2], textalpha);
	}


	// Setup the scaling matrix.
	gl.MatrixMode(DGL_MODELVIEW);
	gl.PushMatrix();
	gl.Scalef(cfg.hudScale, cfg.hudScale, 1);

	// draw the visible HUD data, first health
	if(cfg.hudShown[HUD_HEALTH])
	{
		ST_drawHUDSprite(SPR_STIM, 2, h_height - 2, HOT_BLEFT, iconalpha);
		ST_HUDSpriteSize(SPR_STIM, &w, &h);
		sprintf(buf, "%i%%", plr->health);
		M_WriteText2(w + 4, h_height - 14, buf, hu_font_b, cfg.hudColor[0],
					 cfg.hudColor[1], cfg.hudColor[2], textalpha);
		pos = 60;
	}

	if(cfg.hudShown[HUD_AMMO] &&
	   weaponinfo[plr->readyweapon].ammo != am_noammo)
	{
		spr = ammo_sprite[weaponinfo[plr->readyweapon].ammo];
		ST_drawHUDSprite(spr, pos + 2, h_height - 2, HOT_BLEFT, iconalpha);
		ST_HUDSpriteSize(spr, &w, &h);
		sprintf(buf, "%i", plr->ammo[weaponinfo[plr->readyweapon].ammo]);
		M_WriteText2(pos + w + 4, h_height - 14, buf, hu_font_b,
					 cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textalpha);
	}

	// doom guy's face | use a bit of extra scale
	if(cfg.hudShown[HUD_FACE])
	{
		pos = (h_width/2) -(faceback.width/2) + 6;

Draw_BeginZoom(0.7f, pos , h_height - 1);
		gl.Color4f(1,1,1,iconalpha);
		if(IS_NETGAME)
			GL_DrawPatch_CS( pos, h_height - faceback.height + 1, faceback.lump);

		GL_DrawPatch_CS( pos, h_height - faceback.height, faces[st_faceindex].lump);
Draw_EndZoom();
	}

	pos = h_width - 1;
	if(cfg.hudShown[HUD_ARMOR])
	{
		sprintf(buf, "%i%%", plr->armorpoints);
		spr = plr->armortype == 2 ? SPR_ARM2 : SPR_ARM1;
		ST_drawHUDSprite(spr, h_width - 49, h_height - 2, HOT_BRIGHT, iconalpha);
		ST_HUDSpriteSize(spr, &w, &h);

		M_WriteText2(h_width - M_StringWidth(buf, hu_font_b) - 2, h_height - 14, buf, hu_font_b,
					 cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textalpha);
		pos = h_width - w - 52;
	}

	// Keys  | use a bit of extra scale
	if(cfg.hudShown[HUD_KEYS])
	{
Draw_BeginZoom(0.75f, pos , h_height - 2);
		for(i = 0; i < 3; i++)
		{
			spr = 0;
			if(plr->
			   keys[i == 0 ? it_redcard : i ==
					 1 ? it_yellowcard : it_bluecard])
				spr = i == 0 ? SPR_RKEY : i == 1 ? SPR_YKEY : SPR_BKEY;
			if(plr->
			   keys[i == 0 ? it_redskull : i ==
					 1 ? it_yellowskull : it_blueskull])
				spr = i == 0 ? SPR_RSKU : i == 1 ? SPR_YSKU : SPR_BSKU;
			if(spr)
			{
				ST_drawHUDSprite(spr, pos, h_height - 2, HOT_BRIGHT,iconalpha);
				ST_HUDSpriteSize(spr, &w, &h);
				pos -= w + 2;
			}
		}
Draw_EndZoom();
	}

	gl.MatrixMode(DGL_MODELVIEW);
	gl.PopMatrix();
}

void ST_Drawer(int fullscreenmode, boolean refresh)
{
	st_firsttime = st_firsttime || refresh;
	st_statusbaron = (fullscreenmode < 2) || ( automapactive && (cfg.automapHudDisplay == 0 || cfg.automapHudDisplay == 2) );

	// Do palette shifts
	ST_doPaletteStuff();

	// Either slide the status bar in or fade out the fullscreen hud
	if(st_statusbaron)
	{
		if(hudalpha > 0.0f)
		{
			st_statusbaron = 0;
			hudalpha-=0.1f;
		} else 	if( showbar < 1.0f)
			showbar+=0.1f;
	} else {
		if (fullscreenmode == 3)
		{
			if( hudalpha > 0.0f)
			{
				hudalpha-=0.1f;
				fullscreenmode = 2;
			}
		} else{	
			if( showbar > 0.0f)
			{
				showbar-=0.1f;
				st_statusbaron = 1;
			} else if(hudalpha < 1.0f)
				hudalpha+=0.1f;
		}
	}

	// Always try to render statusbar with alpha in fullscreen modes
	if(fullscreenmode)
		st_blended = 1;
	else
		st_blended = 0;

	if(st_statusbaron){
		ST_doRefresh();
	} else if (fullscreenmode != 3){
		ST_doFullscreenStuff();
	}
}

void ST_loadGraphics(void)
{

	int     i;
	int     j;
	int     facenum;

	char    namebuf[9];

	// Load the numbers, tall and short
	for(i = 0; i < 10; i++)
	{
		sprintf(namebuf, "STTNUM%d", i);
		R_CachePatch(&tallnum[i], namebuf);

		sprintf(namebuf, "STYSNUM%d", i);
		R_CachePatch(&shortnum[i], namebuf);
	}

	// Load percent key.
	// Note: why not load STMINUS here, too?
	R_CachePatch(&tallpercent, "STTPRCNT");

	// key cards
	for(i = 0; i < NUMKEYS; i++)
	{
		sprintf(namebuf, "STKEYS%d", i);
		R_CachePatch(&keys[i], namebuf);
	}

	// arms background
	R_CachePatch(&armsbg, "STARMS");

	// arms ownership widgets
	for(i = 0; i < 6; i++)
	{
		sprintf(namebuf, "STGNUM%d", i + 2);

		// gray #
		R_CachePatch(&arms[i][0], namebuf);

		// yellow #
		memcpy(&arms[i][1], &shortnum[i + 2], sizeof(dpatch_t));
	}

	// face backgrounds for different color players
	sprintf(namebuf, "STFB%d", consoleplayer);
	R_CachePatch(&faceback, namebuf);

	// status bar background bits
	R_CachePatch(&sbar, "STBAR");

	// face states
	facenum = 0;
	for(i = 0; i < ST_NUMPAINFACES; i++)
	{
		for(j = 0; j < ST_NUMSTRAIGHTFACES; j++)
		{
			sprintf(namebuf, "STFST%d%d", i, j);
			R_CachePatch(&faces[facenum++], namebuf);
		}
		sprintf(namebuf, "STFTR%d0", i);	// turn right
		R_CachePatch(&faces[facenum++], namebuf);
		sprintf(namebuf, "STFTL%d0", i);	// turn left
		R_CachePatch(&faces[facenum++], namebuf);
		sprintf(namebuf, "STFOUCH%d", i);	// ouch!
		R_CachePatch(&faces[facenum++], namebuf);
		sprintf(namebuf, "STFEVL%d", i);	// evil grin ;)
		R_CachePatch(&faces[facenum++], namebuf);
		sprintf(namebuf, "STFKILL%d", i);	// pissed off
		R_CachePatch(&faces[facenum++], namebuf);
	}
	R_CachePatch(&faces[facenum++], "STFGOD0");
	R_CachePatch(&faces[facenum++], "STFDEAD0");
}

void ST_updateGraphics(void)
{
	char    namebuf[9];

	// face backgrounds for different color players
	sprintf(namebuf, "STFB%d", cfg.PlayerColor[consoleplayer]);
	R_CachePatch(&faceback, namebuf);
}

void ST_loadData(void)
{
	lu_palette = W_GetNumForName("PLAYPAL");
	ST_loadGraphics();
}

void ST_initData(void)
{
	int     i;

	st_firsttime = true;
	plyr = &players[consoleplayer];

	st_clock = 0;
	st_chatstate = StartChatState;
	st_gamestate = FirstPersonState;

	st_statusbaron = true;
	st_oldchat = st_chat = false;
	st_cursoron = false;

	st_faceindex = 0;
	st_palette = -1;

	st_oldhealth = -1;

	for(i = 0; i < NUMWEAPONS; i++)
	{
		oldweaponsowned[i] = plyr->weaponowned[i];
	}

	for(i = 0; i < 3; i++)
	{
		keyboxes[i] = -1;
	}

	STlib_init();
}

void ST_createWidgets(void)
{
	int     i;

	// ready weapon ammo
	STlib_initNum(&w_ready, ST_AMMOX, ST_AMMOY, tallnum,
				  &plyr->ammo[weaponinfo[plyr->readyweapon].ammo],
				  &st_statusbaron, ST_AMMOWIDTH, &cfg.statusbarCounterAlpha);

	// the last weapon type
	w_ready.data = plyr->readyweapon;

	// health percentage
	STlib_initPercent(&w_health, ST_HEALTHX, ST_HEALTHY, tallnum,
					  &plyr->health, &st_statusbaron, &tallpercent, &cfg.statusbarCounterAlpha);

	// weapons owned
	for(i = 0; i < 6; i++)
	{
		STlib_initMultIcon(&w_arms[i], ST_ARMSX + (i % 3) * ST_ARMSXSPACE,
						   ST_ARMSY + (i / 3) * ST_ARMSYSPACE, arms[i],
						   (int *) &plyr->weaponowned[i + 1], &st_armson, &cfg.statusbarCounterAlpha);
	}

	// frags sum
	STlib_initNum(&w_frags, ST_FRAGSX, ST_FRAGSY, tallnum, &st_fragscount,
				  &st_fragson, ST_FRAGSWIDTH, &cfg.statusbarCounterAlpha);

	// faces
	STlib_initMultIcon(&w_faces, ST_FACESX, ST_FACESY, faces, &st_faceindex,
					   &st_statusbaron, &cfg.statusbarCounterAlpha);

	// armor percentage - should be colored later
	STlib_initPercent(&w_armor, ST_ARMORX, ST_ARMORY, tallnum,
					  &plyr->armorpoints, &st_statusbaron, &tallpercent, &cfg.statusbarCounterAlpha);

	// keyboxes 0-2
	STlib_initMultIcon(&w_keyboxes[0], ST_KEY0X, ST_KEY0Y, keys, &keyboxes[0],
					   &st_statusbaron, &cfg.statusbarCounterAlpha);

	STlib_initMultIcon(&w_keyboxes[1], ST_KEY1X, ST_KEY1Y, keys, &keyboxes[1],
					   &st_statusbaron, &cfg.statusbarCounterAlpha);

	STlib_initMultIcon(&w_keyboxes[2], ST_KEY2X, ST_KEY2Y, keys, &keyboxes[2],
					   &st_statusbaron, &cfg.statusbarCounterAlpha);

	// ammo count (all four kinds)
	STlib_initNum(&w_ammo[0], ST_AMMO0X, ST_AMMO0Y, shortnum, &plyr->ammo[0],
				  &st_statusbaron, ST_AMMO0WIDTH, &cfg.statusbarCounterAlpha);

	STlib_initNum(&w_ammo[1], ST_AMMO1X, ST_AMMO1Y, shortnum, &plyr->ammo[1],
				  &st_statusbaron, ST_AMMO1WIDTH, &cfg.statusbarCounterAlpha);

	STlib_initNum(&w_ammo[2], ST_AMMO2X, ST_AMMO2Y, shortnum, &plyr->ammo[2],
				  &st_statusbaron, ST_AMMO2WIDTH, &cfg.statusbarCounterAlpha);

	STlib_initNum(&w_ammo[3], ST_AMMO3X, ST_AMMO3Y, shortnum, &plyr->ammo[3],
				  &st_statusbaron, ST_AMMO3WIDTH, &cfg.statusbarCounterAlpha);

	// max ammo count (all four kinds)
	STlib_initNum(&w_maxammo[0], ST_MAXAMMO0X, ST_MAXAMMO0Y, shortnum,
				  &plyr->maxammo[0], &st_statusbaron, ST_MAXAMMO0WIDTH, &cfg.statusbarCounterAlpha);

	STlib_initNum(&w_maxammo[1], ST_MAXAMMO1X, ST_MAXAMMO1Y, shortnum,
				  &plyr->maxammo[1], &st_statusbaron, ST_MAXAMMO1WIDTH, &cfg.statusbarCounterAlpha);

	STlib_initNum(&w_maxammo[2], ST_MAXAMMO2X, ST_MAXAMMO2Y, shortnum,
				  &plyr->maxammo[2], &st_statusbaron, ST_MAXAMMO2WIDTH, &cfg.statusbarCounterAlpha);

	STlib_initNum(&w_maxammo[3], ST_MAXAMMO3X, ST_MAXAMMO3Y, shortnum,
		&plyr->maxammo[3], &st_statusbaron, ST_MAXAMMO3WIDTH, &cfg.statusbarCounterAlpha);

}

static boolean st_stopped = true;

void ST_Start(void)
{

	if(!st_stopped)
		ST_Stop();

	ST_initData();
	ST_createWidgets();
	st_stopped = false;

}

void ST_Stop(void)
{
	if(st_stopped)
		return;
	st_stopped = true;
}

void ST_Init(void)
{
	veryfirsttime = 0;
	ST_loadData();
}
