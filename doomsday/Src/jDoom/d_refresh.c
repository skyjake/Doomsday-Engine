#include <ctype.h>
#include "doomdef.h"
#include "doomstat.h"
#include "st_stuff.h"
#include "wi_stuff.h"
#include "hu_stuff.h"
#include "hu_pspr.h"
#include "am_map.h"
#include "r_local.h"
#include "d_config.h"
#include "x_hair.h"
#include "Mn_def.h"
#include "m_menu.h"
#include "g_common.h"
#include "d_net.h"
#include "f_infine.h"

// Macros -------------------------------------------------------------------

#define viewheight	Get(DD_VIEWWINDOW_HEIGHT)

// External Data ------------------------------------------------------------

extern char MobjLightOffsets[NUMMOBJTYPES];
extern int actual_leveltime;

// Private Data -------------------------------------------------------------

static boolean	setsizeneeded;
static int		setblocks;
static int		setdetail;

// Code ---------------------------------------------------------------------

//
// R_InitTranslationTables
// Creates the translation tables to map
//  the green color ramp to gray, brown, red.
// Assumes a given structure of the PLAYPAL.
// Could be read from a lump instead.
//
void R_InitTranslation(void)
{
	byte	*translationtables = (byte*) Get(DD_TRANSLATIONTABLES_ADDRESS);
    int		i;
  
    // translate just the 16 green colors
    for (i=0 ; i<256 ; i++)
    {
		if (i >= 0x70 && i<= 0x7f)
		{
			// map green ramp to gray, brown, red
			translationtables[i] = 0x60 + (i&0xf);
			translationtables[i+256] = 0x40 + (i&0xf);
			translationtables[i+512] = 0x20 + (i&0xf);
		}
		else
		{
			// Keep all other colors as is.
			translationtables[i] = translationtables[i+256] 
				= translationtables[i+512] = i;
		}
    }
}


void R_DrawSpecialFilter(void)
{
	player_t *player = &players[displayplayer];

	if(player->powers[pw_invulnerability])
	{
		float max = 30;
		float str, r, g, b;
		int t = player->powers[pw_invulnerability];
		if(t < max)
			str = t/max;
		else if(t < 4*32 && !(t&8))
			str = .7f;
		else if(t > INVULNTICS-max)
			str = (INVULNTICS-t)/max;
		else
			str = 1; // Full inversion.
		// Draw an inversing filter. 
		gl.Disable(DGL_TEXTURING);
		gl.Func(DGL_BLENDING, DGL_ONE_MINUS_DST_COLOR, DGL_ONE_MINUS_SRC_COLOR);
		r = str*2;
		g = str*2 - .4;
		b = str*2 - .8;
		if(r < 0) r = 0;
		if(r > 1) r = 1;
		if(g < 0) g = 0;
		if(g > 1) g = 1;
		if(b < 0) b = 0;
		if(b > 1) b = 1;
		GL_DrawRect(0, 0, 320, 200, r, g, b, 1);

		// Restore the normal rendering state.
		gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_ONE_MINUS_SRC_ALPHA);
		gl.Enable(DGL_TEXTURING);
	}
}

// Show map name and author.
void R_DrawLevelTitle(void)
{
	float alpha = 1;
	int y = 12;
	char *lname, *lauthor, *ptr;
	
#if 0
	char buf[80];
	//int st = players[consoleplayer].plr->psprites[0].state;
	sprintf(buf, "p0:%p/%p p1:%p/%p", /*st==0? "bob" : st==1? "fire" : st==2? "down" : "up"*/
		players[0].psprites[0].state,
		players[0].psprites[1].state,
		players[1].psprites[0].state,
		players[1].psprites[1].state);
	/*sprintf(buf, "p%i r%i", players[consoleplayer].pendingweapon,
		players[consoleplayer].readyweapon);*/
	M_WriteText(10, 50, buf);
#endif
	
	if(!cfg.levelTitle || actual_leveltime > 6*35) return;
	
	// Make the text a bit smaller.
	gl.MatrixMode(DGL_MODELVIEW);
	gl.PushMatrix();
	gl.Translatef(160, y, 0);
	gl.Scalef(.75f, .75f, 1);
	gl.Translatef(-160, -y, 0);

	if(actual_leveltime < 35) 
		alpha = actual_leveltime/35.0f;
	if(actual_leveltime > 5*35) 
		alpha = 1 - (actual_leveltime-5*35)/35.0f;

	lname = (char*) Get(DD_MAP_NAME);
	lauthor = (char*) Get(DD_MAP_AUTHOR);
	// Plutonia and TNT are special cases.
	if(gamemission == pack_plut)
	{
		lname = mapnamesp[gamemap - 1];
		lauthor = PLUT_AUTHOR;
	}
	else if(gamemission == pack_tnt)
	{
		lname = mapnamest[gamemap - 1];
		lauthor = TNT_AUTHOR;
	}
	gl.Color4f(1, 1, 1, alpha);
	if(lname) 
	{
		ptr = strchr(lname, ':'); // Skip the E#M# or Level #.
		if(ptr)
		{
			lname = ptr + 1;
			while(*lname && isspace(*lname)) lname++;
		}
		M_WriteText2(160-M_StringWidth(lname, hu_font_b)/2, y, lname,
			hu_font_b, -1, -1, -1);
		y += 14; //9;
	}
	gl.Color4f(.5f, .5f, .5f, alpha);
	if(lauthor 
		&& (!cfg.hideAuthorIdSoft 
			|| stricmp(lauthor, "id software")))
	{
		M_WriteText2(160-M_StringWidth(lauthor, hu_font)/2, y, lauthor,
			hu_font_a, -1, -1, -1);
	}

	gl.MatrixMode(DGL_MODELVIEW);
	gl.PopMatrix();
}

//
// R_SetViewSize
// Do not really change anything here,
//  because it might be in the middle of a refresh.
// The change will take effect next refresh.
//
void R_SetViewSize(int blocks, int detail)
{
    setsizeneeded = true;
    setblocks = blocks;
    setdetail = detail;
}


//
// D_Display
//  draw current display, possibly wiping it from the previous
//

// wipegamestate can be set to -1 to force a wipe on the next draw
gamestate_t		wipegamestate = GS_DEMOSCREEN;
extern boolean	inhelpscreens;
extern float	lookOffset;

void R_SetAllDoomsdayFlags();

void D_Display (void)
{
    static  boolean			viewactivestate = false;
    static  boolean			menuactivestate = false;
    static  boolean			inhelpscreensstate = false;
    static  boolean			fullscreen = false;
    static  gamestate_t		oldgamestate = -1;
    int						y;
    boolean					redrawsbar;
	player_t				*player = &players[displayplayer];
	boolean					iscam = (player->plr->flags & DDPF_CAMERA) != 0; // $democam

    if(nodrawers) return;		// for comparative timing / profiling
		
    redrawsbar = false;

	// $democam: can be set on every frame
	if(setblocks > 10 || iscam)
	{
		// Full screen.
		R_ViewWindow(0, 0, 320, 200);
	}
	else
	{
		int w = setblocks*32;
		int h = setblocks*(200-ST_HEIGHT*cfg.sbarscale/20)/10;
		R_ViewWindow(160-(w>>1), (200-ST_HEIGHT*cfg.sbarscale/20-h)>>1, w, h);
	}

	// Do buffered drawing.
	switch (gamestate)
    {
	case GS_LEVEL:
		if(IS_CLIENT && (!Get(DD_GAME_READY) || !Get(DD_GOTFRAME))) 
			break;
		if(leveltime < 2)
		{
			// Don't render too early; the first couple of frames 
			// might be a bit unstable -- this should be considered
			// a bug, but since there's an easy fix...
			break;
		}
		if(!automapactive || cfg.automapAlpha < 1)
		{
			// Draw the player view.
			if(IS_CLIENT) 
			{
				// Server updates mobj flags in NetSv_Ticker.
				R_SetAllDoomsdayFlags();
			}
			// Don't draw self.
			Set(DD_VIEWANGLE_OFFSET, ANGLE_MAX * -lookOffset);
			GL_SetFilter(players[displayplayer].plr->filter); // $democam
			// How about fullbright?
			Set(DD_FULLBRIGHT, (player->powers[pw_infrared] > 4*32)
				|| (player->powers[pw_infrared]&8) 
				|| player->powers[pw_invulnerability] > 30);
		
			// Render the view with possible custom filters.
			R_RenderPlayerView(players[displayplayer].plr);

			R_DrawSpecialFilter(); 
			// Crosshair.
			if(!iscam) X_Drawer(); // $democam
		}
		if(automapactive) AM_Drawer ();

		// Level information is shown for a few seconds in the 
		// beginning of a level.
		R_DrawLevelTitle();

		if((viewheight != 200))
			redrawsbar = true;
		if(inhelpscreensstate && !inhelpscreens)
			redrawsbar = true;              // just put away the help screen
		if(!iscam) ST_Drawer(viewheight == 200, redrawsbar); // $democam
		fullscreen = viewheight == 200;
		HU_Drawer();
		// Need to update the borders?
		if(oldgamestate != GS_LEVEL 
			|| ((Get(DD_VIEWWINDOW_WIDTH) != 320 
				|| menuactive || cfg.sbarscale < 20)))
		{
			// Update the borders.
			GL_Update(DDUF_BORDER);
		}
		break;

	case GS_INTERMISSION:
		WI_Drawer ();
		break;

	case GS_WAITING:
		gl.Clear(DGL_COLOR_BUFFER_BIT);
		M_WriteText2(5, 188, "WAITING... PRESS ESC FOR MENU", 
			hu_font_a, 1, 0, 0);

	default:
		break;
	}

	GL_Update(DDUF_FULLSCREEN);

    menuactivestate = menuactive;
    viewactivestate = viewactive;
    inhelpscreensstate = inhelpscreens;
    oldgamestate = wipegamestate = gamestate;
    
    // draw pause pic (but not if InFine active)
    if(paused && !fi_active)
    {
		if(automapactive)
			y = 4;
		else
			y = viewwindowy+4;
	
		WI_DrawPatch(126, y, W_GetNumForName("M_PAUSE"));
    }

	// InFine is drawn whenever active.
	FI_Drawer();
}


//===========================================================================
// P_SetDoomsdayFlags
//===========================================================================
void P_SetDoomsdayFlags(mobj_t *mo)
{
	// Client mobjs can't be set here.
	if(IS_CLIENT && mo->ddflags & DDMF_REMOTE) return;
			
	// Reset the flags for a new frame.
	mo->ddflags &= DDMF_CLEAR_MASK;
	
	// Local objects aren't sent to clients.
	if(mo->flags & MF_LOCAL) mo->ddflags |= DDMF_LOCAL;
	if(mo->flags & MF_SOLID) mo->ddflags |= DDMF_SOLID;
	if(mo->flags & MF_NOGRAVITY) mo->ddflags |= DDMF_NOGRAVITY;
	if(mo->flags & MF_MISSILE) 
	{
		mo->ddflags |= DDMF_MISSILE;
	}
	if(mo->type == MT_LIGHTSOURCE) 
		mo->ddflags |= DDMF_ALWAYSLIT | DDMF_DONTDRAW;
	if(mo->info && mo->info->flags2 & MF2_ALWAYSLIT)
		mo->ddflags |= DDMF_ALWAYSLIT;
	
	// $democam: cameramen are invisible
	if(P_IsCamera(mo)) mo->ddflags |= DDMF_DONTDRAW;
	
	// The torches often go into the ceiling. This'll prevent
	// them from 'jumping'.
	if(mo->type == MT_MISC41 || mo->type == MT_MISC42 
		|| mo->type == MT_MISC43 // tall torches
		|| mo->type == MT_MISC44 || mo->type == MT_MISC45
		|| mo->type == MT_MISC46) // short torches
		mo->ddflags |= DDMF_NOFITBOTTOM;
	
	if(mo->flags & MF_BRIGHTSHADOW)
		mo->ddflags |= DDMF_BRIGHTSHADOW;
	else if(mo->flags & MF_SHADOW)
		mo->ddflags |= DDMF_SHADOW;
	
	if((mo->flags & MF_VIEWALIGN && !(mo->flags & MF_MISSILE)) 
		|| mo->flags & MF_FLOAT 
		|| (mo->flags & MF_MISSILE && !(mo->flags & MF_VIEWALIGN)))
		mo->ddflags |= DDMF_VIEWALIGN;
	
	mo->ddflags |= mo->flags & MF_TRANSLATION;
}

//===========================================================================
// R_SetAllDoomsdayFlags
//===========================================================================
void R_SetAllDoomsdayFlags()
{
	int			i;
	sector_t	*sec = sectors;
	mobj_t		*iter;

	// Only visible things are in the sector thinglists, so this is good.
	for(i = 0; i < numsectors; i++, sec++)
		for(iter = sec->thinglist; iter; iter = iter->snext)
			P_SetDoomsdayFlags(iter);
}



