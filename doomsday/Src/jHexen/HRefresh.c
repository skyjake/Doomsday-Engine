
//**************************************************************************
//**
//** HREFRESH.C
//**
//** Hexen-specific refresh stuff.
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "h2def.h"
#include "p_local.h"
#include "settings.h"
#include "f_infine.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// This could hold much more detailed information...
typedef struct
{
	char	name[9];	// Name of the texture.
	int		type;		// Which type?
} textype_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void X_Drawer(void);
void R_SetAllDoomsdayFlags(void);
void H2_AdvanceDemo(void);
void MN_DrCenterTextA_CS(char *text, int center_x, int y);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void PageDrawer(void);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern boolean automapactive;
extern boolean MenuActive;
extern boolean askforquit;
extern boolean advancedemo;
extern float lookOffset;
extern int actual_leveltime;

extern boolean dontrender;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int demosequence;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int pagetic;
static char *pagename;

static textype_t	*texTypes = 0;
static int			numTexTypes = 0;

// CODE --------------------------------------------------------------------

/*
==============
=
= R_SetViewSize
=
= Don't really change anything here, because i might be in the middle of
= a refresh.  The change will take effect next refresh.
=
==============
*/

boolean	setsizeneeded;
//int		setblocks/*, setdetail*/;

void R_SetViewSize (int blocks, int detail)
{
	setsizeneeded = true;
	cfg.setblocks = blocks;
	GL_Update(DDUF_BORDER);
}

void R_HandleSectorSpecials()
{
	sector_t	*sect = sectors;
	int			i, scrollOffset = leveltime>>1 & 63;

	for(i=0; i<numsectors; i++, sect++)
	{
		switch(sect->special)
		{ // Handle scrolling flats
		case 201: case 202: case 203: // Scroll_North_xxx
			sect->flatoffy = (63-scrollOffset) << (sect->special-201);
			break;
		case 204: case 205: case 206: // Scroll_East_xxx
			sect->flatoffx = (63-scrollOffset) << (sect->special-204);
			break;
		case 207: case 208: case 209: // Scroll_South_xxx
			sect->flatoffy = scrollOffset << (sect->special-207);
			break;
		case 210: case 211: case 212: // Scroll_West_xxx
			sect->flatoffx = scrollOffset << (sect->special-210);
			break;
		case 213: case 214: case 215: // Scroll_NorthWest_xxx
			sect->flatoffx = scrollOffset << (sect->special-213);
			sect->flatoffy = (63-scrollOffset) << (sect->special-213);
			break;
		case 216: case 217: case 218: // Scroll_NorthEast_xxx
			sect->flatoffx = (63-scrollOffset) << (sect->special-216);
			sect->flatoffy = (63-scrollOffset) << (sect->special-216);
			break;
		case 219: case 220: case 221: // Scroll_SouthEast_xxx
			sect->flatoffx = (63-scrollOffset) << (sect->special-219);
			sect->flatoffy = scrollOffset << (sect->special-219);
			break;
		case 222: case 223: case 224: // Scroll_SouthWest_xxx
			sect->flatoffx = scrollOffset << (sect->special-222);
			sect->flatoffy = scrollOffset << (sect->special-222);
			break;
		default:
			sect->flatoffx = sect->flatoffy = 0;
			break;
		}
	}
}

//==========================================================================
// R_DrawMapTitle
//==========================================================================
void R_DrawMapTitle(void)
{
	float alpha = 1;
	int y = 12;
	char *lname, *lauthor;

/*	char buf[80];
	sprintf(buf, "png: %i\n", 
		(players[consoleplayer].plr->mo->ddflags & DDMF_NOGRAVITY) != 0);
	MN_DrTextA_CS(buf, 60, 60);*/

	if(!cfg.mapTitle || actual_leveltime > 6*35) return;
	
	// Make the text a bit smaller.
	gl.MatrixMode(DGL_MODELVIEW);
	gl.PushMatrix();
	gl.Translatef(160, y, 0);
	gl.Scalef(.75f, .75f, 1);	// Scale to 3/4
	gl.Translatef(-160, -y, 0);

	if(actual_leveltime < 35) 
		alpha = actual_leveltime/35.0f;
	if(actual_leveltime > 5*35) 
		alpha = 1 - (actual_leveltime-5*35)/35.0f;

	lname = (char*) Get(DD_MAP_NAME);
	lauthor = (char*) Get(DD_MAP_AUTHOR);

	// Use stardard map name if DED didn't define it.
	if(!lname) lname = P_GetMapName(gamemap);

	gl.Color4f(1, 1, 1, alpha);
	if(lname) 
	{
		MN_DrTextB_CS(lname, 160 - MN_TextBWidth(lname)/2, y);
		y += 20;
	}
	gl.Color4f(.5f, .5f, .5f, alpha);
	if(lauthor)
	{
		MN_DrTextA_CS(lauthor, 160-MN_TextAWidth(lauthor)/2, y);
	}

	gl.MatrixMode(DGL_MODELVIEW);
	gl.PopMatrix();
}

//==========================================================================
//
// G_Drawer
//
//==========================================================================

void G_Drawer(void)
{
	player_t	*vplayer = &players[displayplayer];
	boolean		iscam = (vplayer->plr->flags & DDPF_CAMERA) != 0; // $democam

	// Change the view size if needed.
/*	if(setsizeneeded)
	{
		setsizeneeded = false;*/
	// $democam: can be set on every frame
	if(cfg.setblocks > 10 || iscam)
	{
		// Full screen.
		R_ViewWindow(0, 0, 320, 200);
	}
	else
	{
		int w = cfg.setblocks*32;
		int h = cfg.setblocks*(200-SBARHEIGHT*cfg.sbarscale/20)/10;
		R_ViewWindow(160-(w>>1), (200-SBARHEIGHT*cfg.sbarscale/20-h)>>1, w, h);
	}
	
	// Do buffered drawing
	switch(gamestate)
	{
		case GS_LEVEL:
			// Clients should be a little careful about the first frames.
			if(IS_CLIENT && (!Get(DD_GAME_READY) || !Get(DD_GOTFRAME))) 
				break;

			// Good luck trying to render the view without a viewpoint...
			if(!vplayer->plr->mo) break; 

			if(leveltime < 2)
			{
				// Don't render too early; the first couple of frames 
				// might be a bit unstable -- this should be considered
				// a bug, but since there's an easy fix...
				break;
			}
			if(automapactive)
			{
				AM_Drawer();
			}
			else
			{
				boolean special200 = false;
				R_HandleSectorSpecials();
				// Set flags for the renderer.
				if(IS_CLIENT) R_SetAllDoomsdayFlags();
				GL_SetFilter(vplayer->plr->filter); // $democam
				// The display player cannot be seen.
				vplayer->plr->mo->ddflags |= DDMF_DONTDRAW;
				// Check for the sector special 200: use sky2.
				// I wonder where this is used?
				if(vplayer->plr->mo->subsector->sector->special == 200)
				{
					special200 = true;
					Rend_SkyParams(0, DD_DISABLE, 0);
					Rend_SkyParams(1, DD_ENABLE, 0);
				}
				// How about a bit of quake?
				if(localQuakeHappening[displayplayer] && !paused)
				{
					int intensity = localQuakeHappening[displayplayer];
					Set(DD_VIEWX_OFFSET, ((M_Random() % (intensity<<2))
							-(intensity<<1))<<FRACBITS);
					Set(DD_VIEWY_OFFSET, ((M_Random() % (intensity<<2))
							-(intensity<<1))<<FRACBITS);
					vplayer->plr->mo->ddflags |= DDMF_DONTDRAW;
				}
				else
				{
					Set(DD_VIEWX_OFFSET, 0);
					Set(DD_VIEWY_OFFSET, 0);
				}
				// The view angle offset.
				Set(DD_VIEWANGLE_OFFSET, ANGLE_MAX * -lookOffset);
				// Render the view.
				if(!dontrender)
					R_RenderPlayerView(vplayer->plr);
				if(special200)
				{
					Rend_SkyParams(0, DD_ENABLE, 0);
					Rend_SkyParams(1, DD_DISABLE, 0);
				}
				if(!iscam) X_Drawer(); // Draw the crosshair.
				R_DrawMapTitle();
			}
			GL_Update(DDUF_FULLSCREEN);
			if(!iscam) SB_Drawer(); // $democam
			// We'll draw the chat text *after* the status bar to
			// be a bit clearer.
			CT_Drawer();
			break;
		
		case GS_INTERMISSION:
			IN_Drawer();
			break;
		
		case GS_INFINE:
			FI_Drawer();
			GL_Update(DDUF_FULLSCREEN);
			break;

		case GS_DEMOSCREEN:
			PageDrawer();
			break;

		case GS_WAITING:
			GL_DrawRawScreen(W_GetNumForName("TITLE"), 0, 0);
			gl.Color3f(1, 1, 1);
			MN_DrCenterTextA_CS("WAITING... PRESS ESC FOR MENU", 160, 188);
			GL_Update(DDUF_FULLSCREEN);
			break;
	}

	if(paused && !MenuActive && !askforquit)
	{
		if(!netgame)
		{
			GL_DrawPatch(160, Get(DD_VIEWWINDOW_Y)+5, 
				W_GetNumForName("PAUSED"));
		}
		else
		{
			GL_DrawPatch(160, 70, W_GetNumForName("PAUSED"));
		}
	}
}

//==========================================================================
//
// PageDrawer
//
//==========================================================================

static void PageDrawer(void)
{
	if(!pagename) return;
	GL_DrawRawScreen(W_GetNumForName(pagename), 0, 0);
	if(demosequence == 1)
	{
		GL_DrawPatch(4, 160, W_GetNumForName("ADVISOR"));
	}
	GL_Update(DDUF_FULLSCREEN);
}

#define FMAKERGBA(r,g,b,a) ( (byte)(0xff*r) + ((byte)(0xff*g)<<8) + ((byte)(0xff*b)<<16) + ((byte)(0xff*a)<<24) )

int H2_GetFilterColor(int filter)
{
	//int rgba = 0;

	// We have to choose the right color and alpha.
	if(filter >= STARTREDPALS && filter < STARTREDPALS+NUMREDPALS) 
		// Red?
		return FMAKERGBA(1, 0, 0, filter/8.0);	// Full red with filter 8.
	else if(filter >= STARTBONUSPALS && filter < STARTBONUSPALS+NUMBONUSPALS) 
		// Light Yellow?
		return FMAKERGBA(1, 1, .5, (filter-STARTBONUSPALS+1)/16.0); 
	else if(filter >= STARTPOISONPALS && filter < STARTPOISONPALS+NUMPOISONPALS)
		// Green?
		return FMAKERGBA(0, 1, 0, (filter-STARTPOISONPALS+1)/16.0);
	else if(filter >= STARTSCOURGEPAL)
		// Orange?
		return FMAKERGBA(1, .5, 0, (STARTSCOURGEPAL+3-filter)/6.0);
	else if(filter >= STARTHOLYPAL)
		// White?
		return FMAKERGBA(1, 1, 1, (STARTHOLYPAL+3-filter)/6.0);
	else if(filter == STARTICEPAL)
		// Light blue?
		return FMAKERGBA(.5f, .5f, 1, .4f);
	else if(filter)
		Con_Error("H2_GetFilterColor: Strange filter number: %d.\n", filter);
	return 0;
}

void H2_SetFilter(int filter)
{
	GL_SetFilter(H2_GetFilterColor(filter));		
}

void H2_EndFrame(void)
{
	SN_UpdateActiveSequences();
/*	S_UpdateSounds(players[displayplayer].plr->mo); */
}

void H2_ConsoleBg(int *width, int *height)
{
	extern int consoleFlat;
	extern float consoleZoom;

	GL_SetFlat(consoleFlat + W_CheckNumForName("F_START")+1);
	*width = 64*consoleZoom;
	*height = 64*consoleZoom;
}


//==========================================================================
//
// H2_PageTicker
//
//==========================================================================

void H2_PageTicker(void)
{
	if(--pagetic < 0)
	{
		H2_AdvanceDemo();
	}
}

//==========================================================================
//
// H2_DoAdvanceDemo
//
//==========================================================================

void H2_DoAdvanceDemo(void)
{
	players[consoleplayer].playerstate = PST_LIVE; // don't reborn
	advancedemo = false;
	usergame = false; // can't save/end game here
	paused = false;
	gameaction = ga_nothing;
	demosequence = (demosequence+1)%7;
	switch(demosequence)
	{
		case 0:
			pagetic = 280;
			gamestate = GS_DEMOSCREEN;
			pagename = "TITLE";
			S_StartMusic("hexen", true);
			break;
		case 1:
			pagetic = 210;
			gamestate = GS_DEMOSCREEN;
			pagename = "TITLE";
			break;
		case 2:
			GL_Update(DDUF_BORDER | DDUF_FULLSCREEN);
			G_DeferedPlayDemo("demo1");
			break;
		case 3:
			pagetic = 200;
			gamestate = GS_DEMOSCREEN;
			pagename = "CREDIT";
			break;
		case 4:
			GL_Update(DDUF_BORDER | DDUF_FULLSCREEN);
			G_DeferedPlayDemo("demo2");
			break;
		case 5:
			pagetic = 200;
			gamestate = GS_DEMOSCREEN;
			pagename = "CREDIT";
			break;
		case 6:
			GL_Update(DDUF_BORDER | DDUF_FULLSCREEN);			
			G_DeferedPlayDemo("demo3");
			break;
	}
}


//==========================================================================
//
//
//
//==========================================================================

/*
========================
=
= R_DrawPSprite
=
========================
*/

player_t *viewplayer;

// Y-adjustment values for full screen (4 weapons)
/*int PSpriteSY[NUMCLASSES][NUMWEAPONS] =
{
	{ 0, -12*FRACUNIT, -10*FRACUNIT, 10*FRACUNIT }, // Fighter
	{ -8*FRACUNIT, 10*FRACUNIT, 10*FRACUNIT, 0 }, // Cleric
	{ 9*FRACUNIT, 20*FRACUNIT, 20*FRACUNIT, 20*FRACUNIT }, // Mage 
	{ 10*FRACUNIT, 10*FRACUNIT, 10*FRACUNIT, 10*FRACUNIT } // Pig
};*/

/*void R_DrawPSprite (pspdef_t *psp)
{
	fixed_t		tx;
	int			x1; 
	spriteinfo_t sprinfo;
	float		light, alpha;
	int			y;

	// Get the sprite info.
	gi.GetSpriteInfo(psp->state->sprite, psp->state->frame, &sprinfo);

	// Calculate edges of the shape.
	tx = psp->sx - (160 + lookOffset*1300) * FRACUNIT;

	tx -= sprinfo.offset;
	x1 = ((160<<FRACBITS)+tx ) >>FRACBITS;
	// Set the OpenGL color & alpha.
	light = 1;
	alpha = 1;
	if(viewplayer->powers[pw_invulnerability] && viewplayer->class
		== PCLASS_CLERIC)
	{
		if(viewplayer->powers[pw_invulnerability] > 4*32)
		{
			if(viewplayer->plr->mo->flags2 & MF2_DONTDRAW)
			{ // don't draw the psprite
				alpha = .333f;
			}
			else if(viewplayer->plr->mo->flags & MF_SHADOW)
			{
				alpha = .666f;
			}
		}
		else if(viewplayer->powers[pw_invulnerability]&8)
		{
			alpha = .333f;
		}
	}	
	else if(psp->state->frame & FF_FULLBRIGHT)
	{
		// Full bright
		light = 1;
	}
	else
	{
		// local light
		light = viewplayer->plr->mo->subsector->sector->lightlevel / 255.0;
	}

	// Do some OpenGL rendering, oh yeah.
	y = -(sprinfo.topOffset>>FRACBITS)+(psp->sy>>FRACBITS);
	if(Get(DD_VIEWWINDOW_HEIGHT) == SCREENHEIGHT)
	{
		y += PSpriteSY[viewplayer->class][players[consoleplayer].readyweapon] >> FRACBITS;
	}
	else 
	{
		y -= (39*sbarscale)/40;
		y += (PSpriteSY[viewplayer->class][players[consoleplayer].readyweapon] >> FRACBITS) * (20-sbarscale)/20.0f;
	}
	light += .1f;	// Add some extra light.
	GL_SetColorAndAlpha(light, light, light, alpha);
	GL_DrawPSprite(x1, y, 1, sprinfo.flip, sprinfo.lump);
}*/

/*
========================
=
= R_DrawPlayerSprites
=
========================
*/

void R_DrawPlayerSprites(ddplayer_t *viewplr)
{
/*	int			i;
	pspdef_t	*psp;

	viewplayer = (player_t*) viewplr->extradata;
	for (i=0, psp=viewplayer->psprites ; i<NUMPSPRITES ; i++,psp++)
		if (psp->state)
			R_DrawPSprite (psp);*/
}

//==========================================================================
// Texture types

#if 0

static void readline(char *buffer, int len, FILE *file)
{
	int		p;

	fgets(buffer, len, file);
	p = strlen(buffer)-1;
	if(buffer[p] == '\n') buffer[p] = 0;
}

static char *firstchar(char *buffer)
{
	int		i = 0;

	while(isspace(buffer[i]) && buffer[i]) i++;
	return buffer + i;
}

void R_LoadTextureTypes()
{
	FILE	*file;
	char	buff[256], *ptr;
	int		curtype = TEXTYPE_UNKNOWN;
	textype_t *tt;

	if((file=fopen("textypes.txt", "rt")) == NULL)
		return;

	for(readline(buff, 255, file); !feof(file); readline(buff, 255, file))
	{
		ptr = firstchar(buff);
		if(*ptr == '#') continue; // A comment.
		if(*ptr == '*')
		{
			ptr = firstchar(ptr+1);
			if(!stricmp(ptr, "metal"))
				curtype = TEXTYPE_METAL;
			else if(!stricmp(ptr, "rock"))
				curtype = TEXTYPE_ROCK;
			else if(!stricmp(ptr, "wood"))
				curtype = TEXTYPE_WOOD;
			else if(!stricmp(ptr, "cloth"))
				curtype = TEXTYPE_CLOTH;
			continue;
		}
		// Allocate a new textype entry.
		texTypes = realloc(texTypes, sizeof(textype_t) * ++numTexTypes);
		tt = texTypes + numTexTypes-1;
		memset(tt, 0, sizeof(*tt));
		strncpy(tt->name, ptr, 8);
		tt->type = curtype;
	}
	Con_Message( "%d texture types loaded.\n", numTexTypes);
	fclose(file);
}

void R_FreeTextypeTypes()
{
	free(texTypes);
	texTypes = 0;
	numTexTypes = 0;
}

int R_TextureTypeForName(char *name)
{
	int		i;

	for(i=0; i<numTexTypes; i++)
		if(!strnicmp(name, texTypes[i].name, 8))
			return texTypes[i].type;
	return TEXTYPE_UNKNOWN;
}

#endif