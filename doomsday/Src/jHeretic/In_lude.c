/*
   ========================
   =
   = IN_lude.c
   =
   ========================
 */

#include "jHeretic/Doomdef.h"
#include "jHeretic/Soundst.h"
#include "jHeretic/h_config.h"
#include "Common/hu_stuff.h"
#include "jHeretic/Mn_def.h"

#define NUMTEAMS	4			// Four colors, four teams.

typedef enum {
	SINGLE,
	COOPERATIVE,
	DEATHMATCH
} gametype_t;

typedef struct {
	int     members;
	int     frags[NUMTEAMS];
	int     totalFrags;
} teaminfo_t;

// Public functions

void    IN_Start(void);
void    IN_Stop(void);
void    IN_Ticker(void);
void    IN_Drawer(void);

boolean intermission;

// Private functions

void    IN_WaitStop(void);
void    IN_LoadPics(void);
void    IN_UnloadPics(void);
void    IN_CheckForSkip(void);
void    IN_InitStats(void);
void    IN_InitDeathmatchStats(void);
void    IN_InitNetgameStats(void);
void    IN_DrawOldLevel(void);
void    IN_DrawYAH(void);
void    IN_DrawStatBack(void);
void    IN_DrawSingleStats(void);
void    IN_DrawCoopStats(void);
void    IN_DrawDMStats(void);
void    IN_DrawNumber(int val, int x, int y, int digits, float r, float g, float b, float a);
void    IN_DrawTime(int x, int y, int h, int m, int s, float r, float g, float b, float a);

static boolean skipintermission;
int     interstate = 0;
int     intertime = -1;
static int oldintertime = 0;
static gametype_t gametype;

static int cnt;

static int time;
static int hours;
static int minutes;
static int seconds;

static int slaughterboy;		// in DM, the player with the most kills

static int killPercent[NUMTEAMS];
static int bonusPercent[NUMTEAMS];
static int secretPercent[NUMTEAMS];

static int playerTeam[MAXPLAYERS];
static teaminfo_t teamInfo[NUMTEAMS];

/*static patch_t *patchINTERPIC;
   static patch_t *patchBEENTHERE;
   static patch_t *patchGOINGTHERE;
   static patch_t *FontBNumbers[10];
   static patch_t *FontBNegative;
   static patch_t *FontBSlash;
   static patch_t *FontBPercent; */
static int interpic, beenthere, goingthere, numbers[10], negative, slash,
	percent;

//static int FontBLump;
//static int FontBLumpBase;
static int patchFaceOkayBase;
static int patchFaceDeadBase;

//static signed int totalFrags[NUMTEAMS];
static fixed_t dSlideX[NUMTEAMS];
static fixed_t dSlideY[NUMTEAMS];

static char *KillersText[] = { "K", "I", "L", "L", "E", "R", "S" };

//extern char *LevelNames[];

extern dpatch_t hu_font[HU_FONTSIZE];
extern dpatch_t hu_font_a[HU_FONTSIZE], hu_font_b[HU_FONTSIZE];

// default font colours
extern float deffontRGB[];
extern float deffontRGB2[];

typedef struct {
	int     x;
	int     y;
} yahpt_t;

static yahpt_t YAHspot[3][9] = {
	{
	 {172, 78},
	 {86, 90},
	 {73, 66},
	 {159, 95},
	 {148, 126},
	 {132, 54},
	 {131, 74},
	 {208, 138},
	 {52, 101}
	 },
	{
	 {218, 57},
	 {137, 81},
	 {155, 124},
	 {171, 68},
	 {250, 86},
	 {136, 98},
	 {203, 90},
	 {220, 140},
	 {279, 106}
	 },
	{
	 {86, 99},
	 {124, 103},
	 {154, 79},
	 {202, 83},
	 {178, 59},
	 {142, 58},
	 {219, 66},
	 {247, 57},
	 {107, 80}
	 }
};

//========================================================================
//
// IN_Start
//
//========================================================================

extern void AM_Stop(void);

void IN_Start(void)
{
	NetSv_Intermission(IMF_BEGIN, 0, 0);

	players[consoleplayer].messageTics = 1;
	players[consoleplayer].message = NULL;

	//  I_SetPalette(W_CacheLumpName("PLAYPAL", PU_CACHE));
	IN_LoadPics();
	IN_InitStats();
	intermission = true;
	interstate = -1;
	skipintermission = false;
	intertime = 0;
	oldintertime = 0;
	AM_Stop();
	S_StartMusic("intr", true);
}

//========================================================================
//
// IN_WaitStop
//
//========================================================================

void IN_WaitStop(void)
{
	if(!--cnt)
	{
		IN_Stop();
		G_WorldDone();
	}
}

//========================================================================
//
// IN_Stop
//
//========================================================================

void IN_Stop(void)
{
	NetSv_Intermission(IMF_END, 0, 0);

	intermission = false;
	IN_UnloadPics();
	GL_Update(DDUF_BORDER);
}

//========================================================================
//
// IN_InitStats
//
//      Initializes the stats for single player mode
//========================================================================

void IN_InitStats(void)
{
	int     i;
	int     j;
	signed int slaughterfrags;
	int     posnum;
	int     slaughtercount;
	int     teamcount;
	int     team;

	// Init team info.
	if(IS_NETGAME)
	{
		memset(teamInfo, 0, sizeof(teamInfo));
		memset(playerTeam, 0, sizeof(playerTeam));
		for(i = 0; i < MAXPLAYERS; i++)
		{
			if(!players[i].plr->ingame)
				continue;
			playerTeam[i] = cfg.PlayerColor[i];
			teamInfo[playerTeam[i]].members++;
		}
	}

	time = leveltime / 35;
	hours = time / 3600;
	time -= hours * 3600;
	minutes = time / 60;
	time -= minutes * 60;
	seconds = time;

#ifdef _DEBUG
	Con_Printf("%i %i %i\n", hours, minutes, seconds);
#endif

	if(!IS_NETGAME)
	{
		gametype = SINGLE;
	}
	else if( /*IS_NETGAME && */ !deathmatch)
	{
		gametype = COOPERATIVE;
		memset(killPercent, 0, sizeof(killPercent));
		memset(bonusPercent, 0, sizeof(bonusPercent));
		memset(secretPercent, 0, sizeof(secretPercent));
		for(i = 0; i < MAXPLAYERS; i++)
		{
			if(players[i].plr->ingame)
			{
				if(totalkills)
				{
					j = players[i].killcount * 100 / totalkills;
					if(j > killPercent[playerTeam[i]])
						killPercent[playerTeam[i]] = j;
				}
				if(totalitems)
				{
					j = players[i].itemcount * 100 / totalitems;
					if(j > bonusPercent[playerTeam[i]])
						bonusPercent[playerTeam[i]] = j;
				}
				if(totalsecret)
				{
					j = players[i].secretcount * 100 / totalsecret;
					if(j > secretPercent[playerTeam[i]])
						secretPercent[playerTeam[i]] = j;
				}
			}
		}
	}
	else
	{
		gametype = DEATHMATCH;
		slaughterboy = 0;
		slaughterfrags = -9999;
		posnum = 0;
		teamcount = 0;
		slaughtercount = 0;
		for(i = 0; i < MAXPLAYERS; i++)
		{
			team = playerTeam[i];
			if(players[i].plr->ingame)
			{
				for(j = 0; j < MAXPLAYERS; j++)
				{
					if(players[j].plr->ingame)
					{
						teamInfo[team].frags[playerTeam[j]] +=
							players[i].frags[j];
						teamInfo[team].totalFrags += players[i].frags[j];
					}
				}
				// Find out the largest number of frags.
				if(teamInfo[team].totalFrags > slaughterfrags)
					slaughterfrags = teamInfo[team].totalFrags;
			}
		}
		for(i = 0; i < NUMTEAMS; i++)
		{
			//posnum++;
			/*if(teamInfo[i].totalFrags > slaughterfrags)
			   {
			   slaughterboy = 1<<i;
			   slaughterfrags = teamInfo[i].totalFrags;
			   slaughtercount = 1;
			   }
			   else */
			if(!teamInfo[i].members)
				continue;

			dSlideX[i] = (43 * posnum * FRACUNIT) / 20;
			dSlideY[i] = (36 * posnum * FRACUNIT) / 20;
			posnum++;

			teamcount++;
			if(teamInfo[i].totalFrags == slaughterfrags)
			{
				slaughterboy |= 1 << i;
				slaughtercount++;
			}
		}
		if(teamcount == slaughtercount)
		{						// don't do the slaughter stuff if everyone is equal
			slaughterboy = 0;
		}
	}
}

//========================================================================
//
// IN_LoadPics
//
//========================================================================

void IN_LoadPics(void)
{
	int     i;

	switch (gameepisode)
	{
	case 1:
		interpic = W_GetNumForName("MAPE1");
		break;
	case 2:
		interpic = W_GetNumForName("MAPE2");
		break;
	case 3:
		interpic = W_GetNumForName("MAPE3");
		break;
	default:
		break;
	}
	beenthere = W_GetNumForName("IN_X");
	goingthere = W_GetNumForName("IN_YAH");

	for(i = 0; i < 10; i++)
	{
		numbers[i] = hu_font_b[i+15].lump;
	}
	negative = hu_font_b[13].lump;

	slash = hu_font_b[14].lump;
	percent = hu_font_b[5].lump;

	patchFaceOkayBase = W_GetNumForName("FACEA0");
	patchFaceDeadBase = W_GetNumForName("FACEB0");
}

//========================================================================
//
// IN_UnloadPics
//
//========================================================================

void IN_UnloadPics(void)
{
	/*  int i;

	   if(patchINTERPIC)
	   {
	   Z_ChangeTag(patchINTERPIC, PU_CACHE);
	   }
	   Z_ChangeTag(patchBEENTHERE, PU_CACHE);
	   Z_ChangeTag(patchGOINGTHERE, PU_CACHE);
	   for(i=0; i<10; i++)
	   {
	   Z_ChangeTag(FontBNumbers[i], PU_CACHE);
	   }
	   Z_ChangeTag(FontBNegative, PU_CACHE);
	   Z_ChangeTag(FontBSlash, PU_CACHE);
	   Z_ChangeTag(FontBPercent, PU_CACHE); */
}

//========================================================================
//
// IN_Ticker
//
//========================================================================

void IN_Ticker(void)
{
	if(!intermission)
	{
		return;
	}
	if(!IS_CLIENT)
	{
		if(interstate == 3)
		{
			IN_WaitStop();
			return;
		}
		IN_CheckForSkip();
	}
	intertime++;
	if(oldintertime < intertime)
	{
		interstate++;
		if(gameepisode > 3 && interstate >= 1)
		{						// Extended Wad levels:  skip directly to the next level
			interstate = 3;
		}
		switch (interstate)
		{
		case 0:
			oldintertime = intertime + 300;
			if(gameepisode > 3)
			{
				oldintertime = intertime + 1200;
			}
			break;
		case 1:
			oldintertime = intertime + 200;
			break;
		case 2:
			oldintertime = MAXINT;
			break;
		case 3:
			cnt = 10;
			break;
		default:
			break;
		}
	}
	if(skipintermission)
	{
		if(interstate == 0 && intertime < 150)
		{
			intertime = 150;
			skipintermission = false;
			NetSv_Intermission(IMF_TIME, 0, intertime);
			return;
		}
		else if(interstate < 2 && gameepisode < 4)
		{
			interstate = 2;
			skipintermission = false;
			S_StartSound(sfx_dorcls, NULL);
			NetSv_Intermission(IMF_STATE, interstate, 0);
			return;
		}
		interstate = 3;
		cnt = 10;
		skipintermission = false;
		S_StartSound(sfx_dorcls, NULL);
		NetSv_Intermission(IMF_STATE, interstate, 0);
	}
}

//========================================================================
//
// IN_CheckForSkip
//
//      Check to see if any player hit a key
//========================================================================

void IN_CheckForSkip(void)
{
	int     i;
	player_t *player;

	if(IS_CLIENT)
		return;

	for(i = 0, player = players; i < MAXPLAYERS; i++, player++)
	{
		if(players[i].plr->ingame)
		{
			if(player->cmd.attack)
			{
				if(!player->attackdown)
				{
					skipintermission = 1;
				}
				player->attackdown = true;
			}
			else
			{
				player->attackdown = false;
			}
			if(player->cmd.use)
			{
				if(!player->usedown)
				{
					skipintermission = 1;
				}
				player->usedown = true;
			}
			else
			{
				player->usedown = false;
			}
		}
	}
}

//========================================================================
//
// IN_Drawer
//
//========================================================================

void IN_Drawer(void)
{
	static int oldinterstate;

	if(!intermission || interstate < 0 || interstate > 3)
	{
		return;
	}
	if(interstate == 3)
	{
		return;
	}
	GL_Update(DDUF_FULLSCREEN);
	if(oldinterstate != 2 && interstate == 2)
	{
		S_LocalSound(sfx_pstop, NULL);
	}
	oldinterstate = interstate;
	switch (interstate)
	{
	case 0:					// draw stats
		IN_DrawStatBack();
		switch (gametype)
		{
		case SINGLE:
			IN_DrawSingleStats();
			break;
		case COOPERATIVE:
			IN_DrawCoopStats();
			break;
		case DEATHMATCH:
			IN_DrawDMStats();
			break;
		}
		break;
	case 1:					// leaving old level
		if(gameepisode < 4)
		{
			GL_DrawPatch(0, 0, interpic);
			IN_DrawOldLevel();
		}
		break;
	case 2:					// going to the next level
		if(gameepisode < 4)
		{
			GL_DrawPatch(0, 0, interpic);
			IN_DrawYAH();
		}
		break;
	case 3:					// waiting before going to the next level
		if(gameepisode < 4)
		{
			GL_DrawPatch(0, 0, interpic);
		}
		break;
	default:
		Con_Error("IN_lude:  Intermission state out of range.\n");
		break;
	}
}

//========================================================================
//
// IN_DrawStatBack
//
//========================================================================

void IN_DrawStatBack(void)
{
	gl.Color4f(1, 1, 1, 1);
	GL_SetFlat(R_FlatNumForName("FLOOR16"));
	GL_DrawRectTiled(0, 0, SCREENWIDTH, SCREENHEIGHT, 64, 64);
}

//========================================================================
//
// IN_DrawOldLevel
//
//========================================================================

void IN_DrawOldLevel(void)
{
	int     i;
	int     x;
	char   *levelname = P_GetShortLevelName(gameepisode, prevmap);

	x = 160 - M_StringWidth(levelname, hu_font_b) / 2;
	M_WriteText2(x, 3, levelname, hu_font_b, deffontRGB[0], deffontRGB[1], deffontRGB[2], 1);

	x = 160 - M_StringWidth("FINISHED", hu_font_a) / 2;
	M_WriteText2(x, 25, "FINISHED", hu_font_a, deffontRGB2[0], deffontRGB2[1],deffontRGB2[2], 1);

	if(prevmap == 9)
	{
		for(i = 0; i < gamemap - 1; i++)
		{
			GL_DrawPatch(YAHspot[gameepisode - 1][i].x,
						 YAHspot[gameepisode - 1][i].y, beenthere);
		}
		if(!(intertime & 16))
		{
			GL_DrawPatch(YAHspot[gameepisode - 1][8].x,
						 YAHspot[gameepisode - 1][8].y, beenthere);
		}
	}
	else
	{
		for(i = 0; i < prevmap - 1; i++)
		{
			GL_DrawPatch(YAHspot[gameepisode - 1][i].x,
						 YAHspot[gameepisode - 1][i].y, beenthere);
		}
		if(players[consoleplayer].didsecret)
		{
			GL_DrawPatch(YAHspot[gameepisode - 1][8].x,
						 YAHspot[gameepisode - 1][8].y, beenthere);
		}
		if(!(intertime & 16))
		{
			GL_DrawPatch(YAHspot[gameepisode - 1][prevmap - 1].x,
						 YAHspot[gameepisode - 1][prevmap - 1].y, beenthere);
		}
	}
}

//========================================================================
//
// IN_DrawYAH
//
//========================================================================

void IN_DrawYAH(void)
{
	int     i;
	int     x;
	char   *levelname = P_GetShortLevelName(gameepisode, gamemap);

	x = 160 - M_StringWidth("NOW ENTERING:", hu_font_a) / 2;
	M_WriteText2(x, 10, "NOW ENTERING:", hu_font_a, deffontRGB2[0], deffontRGB2[1], deffontRGB2[2], 1);

	x = 160 - M_StringWidth(levelname, hu_font_b) / 2;
	M_WriteText2(x, 20, levelname, hu_font_b, deffontRGB[0], deffontRGB[1], deffontRGB[2], 1);

	if(prevmap == 9)
	{
		prevmap = gamemap - 1;
	}
	for(i = 0; i < prevmap; i++)
	{
		GL_DrawPatch(YAHspot[gameepisode - 1][i].x,
					 YAHspot[gameepisode - 1][i].y, beenthere);
	}
	if(players[consoleplayer].didsecret)
	{
		GL_DrawPatch(YAHspot[gameepisode - 1][8].x,
					 YAHspot[gameepisode - 1][8].y, beenthere);
	}
	if(!(intertime & 16) || interstate == 3)
	{							// draw the destination 'X'
		GL_DrawPatch(YAHspot[gameepisode - 1][gamemap - 1].x,
					 YAHspot[gameepisode - 1][gamemap - 1].y, goingthere);
	}
}

//========================================================================
//
// IN_DrawSingleStats
//
//========================================================================

void IN_DrawSingleStats(void)
{

	int     x;
	static int sounds;
	char   *levelname = P_GetShortLevelName(gameepisode, prevmap);

	M_WriteText2(50, 65, "KILLS", hu_font_b, deffontRGB[0], deffontRGB[1], deffontRGB[2], 1);
	M_WriteText2(50, 90, "ITEMS", hu_font_b, deffontRGB[0], deffontRGB[1], deffontRGB[2], 1);
	M_WriteText2(50, 115, "SECRETS", hu_font_b, deffontRGB[0], deffontRGB[1], deffontRGB[2], 1);

	x = 160 - M_StringWidth(levelname, hu_font_b) / 2;
	M_WriteText2(x, 3, levelname, hu_font_b, deffontRGB[0], deffontRGB[1], deffontRGB[2], 1);

	x = 160 - M_StringWidth("FINISHED", hu_font_a) / 2;
	M_WriteText2(x, 25, "FINISHED", hu_font_a, deffontRGB2[0], deffontRGB2[1], deffontRGB2[2], 1);

	if(intertime < 30)
	{
		sounds = 0;
		return;
	}
	if(sounds < 1 && intertime >= 30)
	{
		S_LocalSound(sfx_dorcls, NULL);
		sounds++;
	}
	IN_DrawNumber(players[consoleplayer].killcount, 200, 65, 3, deffontRGB[0], deffontRGB[1], deffontRGB[2], 1);
	GL_DrawPatchLitAlpha(250, 67, 0, .4f, slash);
	gl.Color4f(deffontRGB[0], deffontRGB[1], deffontRGB[2], 1);
	GL_DrawPatch_CS(248, 65, slash);
	IN_DrawNumber(totalkills, 248, 65, 3, deffontRGB[0], deffontRGB[1], deffontRGB[2], 1);
	if(intertime < 60)
	{
		return;
	}
	if(sounds < 2 && intertime >= 60)
	{
		S_LocalSound(sfx_dorcls, NULL);
		sounds++;
	}
	IN_DrawNumber(players[consoleplayer].itemcount, 200, 90, 3, deffontRGB[0], deffontRGB[1], deffontRGB[2], 1);
	GL_DrawPatchLitAlpha(250, 92, 0, .4f, slash);
	gl.Color4f(deffontRGB[0], deffontRGB[1], deffontRGB[2], 1);
	GL_DrawPatch_CS(248, 90, slash);
	IN_DrawNumber(totalitems, 248, 90, 3, deffontRGB[0], deffontRGB[1], deffontRGB[2], 1);
	if(intertime < 90)
	{
		return;
	}
	if(sounds < 3 && intertime >= 90)
	{
		S_LocalSound(sfx_dorcls, NULL);
		sounds++;
	}
	IN_DrawNumber(players[consoleplayer].secretcount, 200, 115, 3, deffontRGB[0], deffontRGB[1], deffontRGB[2], 1);
	GL_DrawPatchLitAlpha(250, 117, 0, .4f, slash);
	gl.Color4f(deffontRGB[0], deffontRGB[1], deffontRGB[2], 1);
	GL_DrawPatch_CS(248, 115, slash);
	IN_DrawNumber(totalsecret, 248, 115, 3, deffontRGB[0], deffontRGB[1], deffontRGB[2], 1);
	if(intertime < 150)
	{
		return;
	}
	if(sounds < 4 && intertime >= 150)
	{
		S_LocalSound(sfx_dorcls, NULL);
		sounds++;
	}

	if(!ExtendedWAD || gameepisode < 4)
	{
		M_WriteText2(85, 160, "TIME", hu_font_b, deffontRGB[0], deffontRGB[1], deffontRGB[2], 1);
		IN_DrawTime(155, 160, hours, minutes, seconds, deffontRGB[0], deffontRGB[1], deffontRGB[2], 1);
	}
	else
	{
		x = 160 - M_StringWidth("NOW ENTERING:", hu_font_a) / 2;
		M_WriteText2(x, 160, "NOW ENTERING:", hu_font_a, deffontRGB2[0], deffontRGB2[1], deffontRGB2[2], 1);

		levelname = P_GetShortLevelName(gameepisode, gamemap);

		x = 160 - M_StringWidth(levelname, hu_font_b) / 2;
		M_WriteText2(x, 170, levelname, hu_font_b, deffontRGB[0], deffontRGB[1], deffontRGB[2], 1);

		skipintermission = false;
	}

}

//========================================================================
//
// IN_DrawCoopStats
//
//========================================================================

void IN_DrawCoopStats(void)
{
	int     i;
	int     x;
	int     ypos;
	char   *levelname = P_GetShortLevelName(gameepisode, prevmap);

	static int sounds;

	M_WriteText2(95, 35, "KILLS", hu_font_b, deffontRGB[0], deffontRGB[1], deffontRGB[2], 1);
	M_WriteText2(155, 35, "BONUS", hu_font_b, deffontRGB[0], deffontRGB[1], deffontRGB[2], 1);
	M_WriteText2(232, 35, "SECRET", hu_font_b, deffontRGB[0], deffontRGB[1], deffontRGB[2], 1);

	x = 160 - M_StringWidth(levelname, hu_font_b) / 2;
	M_WriteText2(x, 3, levelname, hu_font_b, deffontRGB[0], deffontRGB[1], deffontRGB[2], 1);

	x = 160 - M_StringWidth("FINISHED", hu_font_a) / 2;
	M_WriteText2(x, 25, "FINISHED", hu_font_a, deffontRGB2[0], deffontRGB2[1], deffontRGB2[2], 1);

	ypos = 50;
	for(i = 0; i < NUMTEAMS; i++)
	{
		if(teamInfo[i].members)	//players[i].plr->ingame)
		{
			GL_DrawPatchLitAlpha(27, ypos+2, 0, .4f, patchFaceOkayBase + i);
			gl.Color4f(deffontRGB[0], deffontRGB[1], deffontRGB[2], 1);
			GL_DrawPatch_CS(25, ypos, patchFaceOkayBase + i);
			if(intertime < 40)
			{
				sounds = 0;
				ypos += 37;
				continue;
			}
			else if(intertime >= 40 && sounds < 1)
			{
				S_LocalSound(sfx_dorcls, NULL);
				sounds++;
			}
			IN_DrawNumber(killPercent[i], 85, ypos + 10, 3, deffontRGB[0], deffontRGB[1], deffontRGB[2], 1);
			GL_DrawPatchLitAlpha(123, ypos + 12, 0, .4f, percent);
			gl.Color4f(deffontRGB[0], deffontRGB[1], deffontRGB[2], 1);
			GL_DrawPatch_CS(121, ypos + 10, percent);
			IN_DrawNumber(bonusPercent[i], 160, ypos + 10, 3, deffontRGB[0], deffontRGB[1], deffontRGB[2], 1);
			GL_DrawPatchLitAlpha(198, ypos + 12, 0, .4f, percent);
			gl.Color4f(deffontRGB[0], deffontRGB[1], deffontRGB[2], 1);
			GL_DrawPatch_CS(196, ypos + 10, percent);
			IN_DrawNumber(secretPercent[i], 237, ypos + 10, 3, deffontRGB[0], deffontRGB[1], deffontRGB[2], 1);
			GL_DrawPatchLitAlpha(275, ypos + 12, 0, .4f, percent);
			gl.Color4f(deffontRGB[0], deffontRGB[1], deffontRGB[2], 1);
			GL_DrawPatch_CS(273, ypos + 10, percent);
			ypos += 37;
		}
	}
}

//========================================================================
//
// IN_DrawDMStats
//
//========================================================================

void IN_DrawDMStats(void)
{
	int     i;
	int     j;
	int     ypos;
	int     xpos;
	int     kpos;

	//  int x;

	static int sounds;

	xpos = 90;
	ypos = 55;

	M_WriteText2(265, 30, "TOTAL", hu_font_b, deffontRGB[0], deffontRGB[1], deffontRGB[2], 1);
	M_WriteText2(140, 8, "VICTIMS", hu_font_a, deffontRGB2[0], deffontRGB2[1], deffontRGB2[2], 1);

	for(i = 0; i < 7; i++)
	{
		M_WriteText2(10, 80 + 9 * i, KillersText[i], hu_font_a, deffontRGB2[0], deffontRGB2[1], deffontRGB2[2], 1);
	}
	if(intertime < 20)
	{
		for(i = 0; i < NUMTEAMS; i++)
		{
			if(teamInfo[i].members)	//players[i].plr->ingame)
			{
				GL_DrawShadowedPatch(40,
									 ((ypos << FRACBITS) +
									  dSlideY[i] * intertime) >> FRACBITS,
									 patchFaceOkayBase + i);
				GL_DrawShadowedPatch(((xpos << FRACBITS) +
									  dSlideX[i] * intertime) >> FRACBITS, 18,
									 patchFaceDeadBase + i);
			}
		}
		sounds = 0;
		return;
	}
	if(intertime >= 20 && sounds < 1)
	{
		S_LocalSound(sfx_dorcls, NULL);
		sounds++;
	}
	if(intertime >= 100 && slaughterboy && sounds < 2)
	{
		S_LocalSound(sfx_wpnup, NULL);
		sounds++;
	}
	for(i = 0; i < NUMTEAMS; i++)
	{
		if(teamInfo[i].members)	//players[i].plr->ingame)
		{
			if(intertime < 100 || i == playerTeam[consoleplayer])
			{
				GL_DrawShadowedPatch(40, ypos, patchFaceOkayBase + i);
				GL_DrawShadowedPatch(xpos, 18, patchFaceDeadBase + i);
			}
			else
			{
				GL_DrawFuzzPatch(40, ypos, patchFaceOkayBase + i);
				GL_DrawFuzzPatch(xpos, 18, patchFaceDeadBase + i);
			}
			kpos = 86;
			for(j = 0; j < NUMTEAMS; j++)
			{
				if(teamInfo[j].members)	//players[j].plr->ingame)
				{
					IN_DrawNumber(teamInfo[i].frags[j]
								  /*players[i].frags[j] */ , kpos, ypos + 10,
								  3, deffontRGB[0], deffontRGB[1], deffontRGB[2], 1);
					kpos += 43;
				}
			}
			if(slaughterboy & (1 << i))
			{
				if(!(intertime & 16))
				{
					IN_DrawNumber(teamInfo[i].totalFrags, 263, ypos + 10, 3,
							deffontRGB[0], deffontRGB[1], deffontRGB[2], 1);
				}
			}
			else
			{
				IN_DrawNumber(teamInfo[i].totalFrags, 263, ypos + 10, 3,
							deffontRGB[0], deffontRGB[1], deffontRGB[2], 1);
			}
			ypos += 36;
			xpos += 43;
		}
	}
}

//========================================================================
//
// IN_DrawTime
//
//========================================================================

void IN_DrawTime(int x, int y, int h, int m, int s, float r, float g, float b, float a)
{
	if(h)
	{
		IN_DrawNumber(h, x, y, 2, r, g, b, a);

		M_WriteText2(x + 26, y, ":", hu_font_b, r, g, b, a);
	}
	x += 34;
	if(m || h)
	{
		IN_DrawNumber(m, x, y, 2, r, g, b, a);
	}
	x += 34;
	M_WriteText2(x-8, y, ":", hu_font_b, r, g, b, a);
	IN_DrawNumber(s, x, y, 2, r, g, b, a);
}

//========================================================================
//
// IN_DrawNumber
//
//========================================================================

void IN_DrawNumber(int val, int x, int y, int digits, float r, float g, float b, float a)
{
	int     xpos;
	int     oldval;
	int     realdigits;
	boolean neg;

	oldval = val;
	xpos = x;
	neg = false;
	realdigits = 1;

	if(val < 0)
	{							//...this should reflect negative frags
		val = -val;
		neg = true;
		if(val > 99)
		{
			val = 99;
		}
	}
	if(val > 9)
	{
		realdigits++;
		if(digits < realdigits)
		{
			realdigits = digits;
			val = 9;
		}
	}
	if(val > 99)
	{
		realdigits++;
		if(digits < realdigits)
		{
			realdigits = digits;
			val = 99;
		}
	}
	if(val > 999)
	{
		realdigits++;
		if(digits < realdigits)
		{
			realdigits = digits;
			val = 999;
		}
	}
	if(digits == 4)
	{
		GL_DrawPatchLitAlpha(xpos + 8 - hu_font_b[val / 1000].width / 2 - 12, y + 2, 0, .4f, numbers[val / 1000]);
		gl.Color4f(r, g, b, a);
		GL_DrawPatch_CS(xpos + 6 - hu_font_b[val / 1000].width / 2 - 12, y, numbers[val / 1000]);
	}
	if(digits > 2)
	{
		if(realdigits > 2)
		{
			GL_DrawPatchLitAlpha(xpos + 8 - hu_font_b[val / 100].width / 2, y+2, 0, .4f, numbers[val / 100]);
			gl.Color4f(r, g, b, a);
			GL_DrawPatch_CS(xpos + 6 - hu_font_b[val / 100].width / 2, y, numbers[val / 100]);
		}
		xpos += 12;
	}
	val = val % 100;
	if(digits > 1)
	{
		if(val > 9)
		{
			GL_DrawPatchLitAlpha(xpos + 8 - hu_font_b[val / 10].width / 2, y+2, 0, .4f, numbers[val / 10]);
			gl.Color4f(r, g, b, a);
			GL_DrawPatch_CS(xpos + 6 - hu_font_b[val / 10].width / 2, y, numbers[val / 10]);
		}
		else if(digits == 2 || oldval > 99)
		{
			GL_DrawPatchLitAlpha(xpos+2, y+2, 0, .4f, numbers[0]);
			gl.Color4f(r, g, b, a);
			GL_DrawPatch_CS(xpos, y, numbers[0]);
		}
		xpos += 12;
	}
	val = val % 10;
	GL_DrawPatchLitAlpha(xpos + 8 - hu_font_b[val].width / 2, y+2, 0, .4f, numbers[val]);
	gl.Color4f(r, g, b, a);
	GL_DrawPatch_CS(xpos + 6 - hu_font_b[val].width / 2, y, numbers[val]);
	if(neg)
	{
		GL_DrawPatchLitAlpha(xpos + 8 - hu_font_b[negative].width / 2 - 12 * (realdigits), y+2, 0, .4f, negative);
		gl.Color4f(r, g, b, a);
		GL_DrawPatch_CS(xpos + 6 - hu_font_b[negative].width / 2 - 12 * (realdigits), y, negative);
	}
}
