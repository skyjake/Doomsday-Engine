
//**************************************************************************
//**
//** in_lude.c : Heretic 2 : Raven Software, Corp.
//**
//** $RCSfile$
//** $Revision$
//** $Date$
//** $Author$
//**
//**************************************************************************

#include "h2def.h"
#include "d_net.h"
#include <ctype.h>

// MACROS ------------------------------------------------------------------

#define	TEXTSPEED 3
#define	TEXTWAIT 140

// TYPES -------------------------------------------------------------------

typedef enum
{
	SINGLE,
	COOPERATIVE,
	DEATHMATCH
} gametype_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void IN_Start(void);
void IN_Ticker(void);
void IN_Drawer(void);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void WaitStop(void);
//static void Stop(void);
static void LoadPics(void);
static void UnloadPics(void);
static void CheckForSkip(void);
static void InitStats(void);
static void DrDeathTally(void);
static void DrNumber(int val, int x, int y, int wrapThresh);
static void DrNumberBold(int val, int x, int y, int wrapThresh);
//static void DrawHubText(void);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

int overrideHubMsg = 0; // override the hub transition message when 1

// PUBLIC DATA DECLARATIONS ------------------------------------------------

boolean intermission;
char ClusterMessage[MAX_INTRMSN_MESSAGE_SIZE];
int interstate = 0;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean skipintermission;
static int intertime = -1;
static gametype_t gametype;
static int cnt;
static int slaughterboy; // in DM, the player with the most kills
static int patchINTERPICLumpRS;	// A raw screen.
static int FontBNumbersLump[10];
static int FontBNegativeLump;
static int FontBSlashLump;
static int FontBPercentLump;

static int FontABaseLump;
static int FontBLump;
static int FontBLumpBase;

static signed int totalFrags[MAXPLAYERS];

static int HubCount;
//static char *HubText;

// CODE --------------------------------------------------------------------

//========================================================================
//
// IN_Start
//
//========================================================================

extern void AM_Stop (void);

void IN_Start(void)
{
	int i;

	AM_Stop();
	SN_StopAllSequences();	

	// InFine handles the text.
	if(!deathmatch) 
	{
		gameaction = ga_leavemap;
		return;
	}

	GL_SetFilter(0);
	InitStats();
	LoadPics();
	intermission = true;
	interstate = 0;
	skipintermission = false;
	intertime = 0;
	for(i = 0; i < MAXPLAYERS; i++)
	{
		players[i].messageTics = 0;
		players[i].message[0] = 0;
	}
}

//========================================================================
//
// WaitStop
//
//========================================================================

void WaitStop(void)
{
	if(!--cnt)
	{
		IN_Stop();
//		gamestate = GS_LEVEL;
//		G_DoLoadLevel();
		gameaction = ga_leavemap;
//		G_WorldDone();
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
	UnloadPics();
	SB_state = -1;
	GL_Update(DDUF_BORDER);
}

//========================================================================
//
// InitStats
//
// 	Initializes the stats for single player mode
//========================================================================

#if 0
static char *ClusMsgLumpNames[] =
{
	"clus1msg",
	"clus2msg",
	"clus3msg",
	"clus4msg",
	"clus5msg"
};
#endif

static void InitStats(void)
{
	int i;
	int j;
	signed int slaughterfrags;
	int posnum;
	int slaughtercount;
	int playercount;
#if 0
	int oldCluster;
	char *msgLumpName;
	int msgSize;
	int msgLump;
	extern int LeaveMap;
#endif

	if(!deathmatch)
	{
#if 0
		gametype = SINGLE;
		HubCount = 0;
		oldCluster = P_GetMapCluster(gamemap);
		if(oldCluster != P_GetMapCluster(LeaveMap))
		{
			if(oldCluster >= 1 && oldCluster <= 5)
			{
                if(!overrideHubMsg) {
    				msgLumpName = ClusMsgLumpNames[oldCluster-1];
    				msgLump = W_CheckNumForName(msgLumpName);
					if(msgLump < 0)
					{
						HubText = "correct hub message not found";
					}
					else
					{
						msgSize = W_LumpLength(msgLump);
						if(msgSize >= MAX_INTRMSN_MESSAGE_SIZE)
						{
							Con_Error("Cluster message too long (%s)", msgLumpName);
						}
						gi.W_ReadLump(msgLump, ClusterMessage);
						ClusterMessage[msgSize] = 0; // Append terminator
						HubText = ClusterMessage;
					}
                }
                else 
				{
				    HubText = "null hub message";
                }
   				HubCount = strlen(HubText)*TEXTSPEED + TEXTWAIT;
   				S_StartSongName("hub", true);
			}
		}
#endif
	}
	else
	{
		gametype = DEATHMATCH;
		slaughterboy = 0;
		slaughterfrags = -9999;
		posnum = 0;
		playercount = 0;
		slaughtercount = 0;
		for(i=0; i<MAXPLAYERS; i++)
		{
			totalFrags[i] = 0;
			if(players[i].plr->ingame)
			{
				playercount++;
				for(j=0; j<MAXPLAYERS; j++)
				{
					if(players[i].plr->ingame)
					{
						totalFrags[i] += players[i].frags[j];
					}
				}
				posnum++;
			}
			if(totalFrags[i] > slaughterfrags)
			{
				slaughterboy = 1<<i;
				slaughterfrags = totalFrags[i];
				slaughtercount = 1;
			}
			else if(totalFrags[i] == slaughterfrags)
			{
				slaughterboy |= 1<<i;
				slaughtercount++;
			}
		}
		if(playercount == slaughtercount)
		{ // don't do the slaughter stuff if everyone is equal
			slaughterboy = 0;
		}
		S_StartMusic("hub", true);
	}
}

//========================================================================
//
// LoadPics
//
//========================================================================

static void LoadPics(void)
{
	int i;

	if(HubCount || gametype == DEATHMATCH)
	{
		patchINTERPICLumpRS = W_GetNumForName("INTERPIC");
		FontBLumpBase = W_GetNumForName("FONTB16");
		for(i=0; i<10; i++)
		{
			FontBNumbersLump[i] = FontBLumpBase+i;
		}
		FontBLump = W_GetNumForName("FONTB_S")+1;
		FontBNegativeLump = W_GetNumForName("FONTB13");
		FontABaseLump = W_GetNumForName("FONTA_S")+1;
	
		FontBSlashLump = W_GetNumForName("FONTB15");
		FontBPercentLump = W_GetNumForName("FONTB05");
	}
}

//========================================================================
//
// UnloadPics
//
//========================================================================

static void UnloadPics(void)
{
//	int i;

	if(HubCount || gametype == DEATHMATCH)
	{
/*		Z_ChangeTag(patchINTERPIC, PU_CACHE);
		for(i=0; i<10; i++)
		{
			Z_ChangeTag(FontBNumbers[i], PU_CACHE);
		}
		Z_ChangeTag(FontBNegative, PU_CACHE);
		Z_ChangeTag(FontBSlash, PU_CACHE);
		Z_ChangeTag(FontBPercent, PU_CACHE);*/
	}
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
	if(interstate)
	{
		WaitStop();
		return;
	}
	skipintermission = false;
	CheckForSkip();
	intertime++;
	if(skipintermission || (gametype == SINGLE && !HubCount))
	{
		interstate = 1;
		NetSv_Intermission(IMF_STATE, interstate, 0);
		cnt = 10;
		skipintermission = false;
		//S_StartSound(NULL, sfx_dorcls);
	}
}

//========================================================================
//
// CheckForSkip
//
// 	Check to see if any player hit a key
//========================================================================

static void CheckForSkip(void)
{
  	int i;
	player_t *player;
	static boolean triedToSkip;

  	for(i = 0, player = players; i < MAXPLAYERS; i++, player++)
	{
    	if(players[i].plr->ingame)
    	{
			if(player->cmd.actions & BT_ATTACK)
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
			if(player->cmd.actions & BT_USE)
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
	if(deathmatch && intertime < 140)
	{ // wait for 4 seconds before allowing a skip
		if(skipintermission == 1)
		{
			triedToSkip = true;
			skipintermission = 0;
		}
	}
	else
	{
		if(triedToSkip)
		{
			skipintermission = 1;
			triedToSkip = false;
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
	if(!intermission)
	{
		return;
	}
	if(interstate)
	{
		return;
	}
	GL_Update(DDUF_FULLSCREEN);
	GL_DrawRawScreen(patchINTERPICLumpRS, 0, 0);

	if(gametype == SINGLE)
	{
#if 0
		if(HubCount)
		{
			DrawHubText();
		}
#endif
	}
	else
	{
		DrDeathTally();
	}
}

//========================================================================
//
// DrDeathTally
//
//========================================================================

#define TALLY_EFFECT_TICKS 20
#define TALLY_FINAL_X_DELTA (23*FRACUNIT)
#define TALLY_FINAL_Y_DELTA (13*FRACUNIT)
#define TALLY_START_XPOS (178*FRACUNIT)
#define TALLY_STOP_XPOS (90*FRACUNIT)
#define TALLY_START_YPOS (132*FRACUNIT)
#define TALLY_STOP_YPOS (83*FRACUNIT)
#define TALLY_TOP_X 85
#define TALLY_TOP_Y 9
#define TALLY_LEFT_X 7
#define TALLY_LEFT_Y 71
#define TALLY_TOTALS_X 291

static void DrDeathTally(void)
{
	int i, j;
	fixed_t xPos, yPos;
	fixed_t xDelta, yDelta;
	fixed_t xStart, scale;
	int x, y;
	boolean bold;
	static boolean showTotals;
	int temp;

	GL_DrawPatch(TALLY_TOP_X, TALLY_TOP_Y,	W_GetNumForName("tallytop"));
	GL_DrawPatch(TALLY_LEFT_X, TALLY_LEFT_Y, W_GetNumForName("tallylft"));

	if(intertime < TALLY_EFFECT_TICKS)
	{
		showTotals = false;
		scale = (intertime*FRACUNIT)/TALLY_EFFECT_TICKS;
		xDelta = FixedMul(scale, TALLY_FINAL_X_DELTA);
		yDelta = FixedMul(scale, TALLY_FINAL_Y_DELTA);
		xStart = TALLY_START_XPOS-FixedMul(scale,
			TALLY_START_XPOS-TALLY_STOP_XPOS);
		yPos = TALLY_START_YPOS-FixedMul(scale,
			TALLY_START_YPOS-TALLY_STOP_YPOS);
	}
	else
	{
		xDelta = TALLY_FINAL_X_DELTA;
		yDelta = TALLY_FINAL_Y_DELTA;
		xStart = TALLY_STOP_XPOS;
		yPos = TALLY_STOP_YPOS;
	}
	if(intertime >= TALLY_EFFECT_TICKS && showTotals == false)
	{
		showTotals = true;
		S_StartSound(SFX_PLATFORM_STOP, NULL);
	}
	y = yPos>>FRACBITS;
	for(i = 0; i < MAXPLAYERS; i++)
	{
		xPos = xStart;
		for(j = 0; j < MAXPLAYERS; j++, xPos += xDelta)
		{
			x = xPos>>FRACBITS;
			bold = (i == consoleplayer || j == consoleplayer);
			if(players[i].plr->ingame && players[j].plr->ingame)
			{
				if(bold)
				{
					DrNumberBold(players[i].frags[j], x, y, 100);
				}
				else
				{
					DrNumber(players[i].frags[j], x, y, 100);
				}
			}
			else
			{
				temp = MN_TextAWidth("--")/2;
				if(bold)
				{
					MN_DrTextAYellow("--", x-temp, y);
				}
				else
				{
					MN_DrTextA("--", x-temp, y);
				}
			}
		}
		if(showTotals && players[i].plr->ingame
			&& !((slaughterboy&(1<<i)) && !(intertime&16)))
		{
			DrNumber(totalFrags[i], TALLY_TOTALS_X, y, 1000);
		}
		yPos += yDelta;
		y = yPos>>FRACBITS;
	}
}

//==========================================================================
//
// DrNumber
//
//==========================================================================

static void DrNumber(int val, int x, int y, int wrapThresh)
{
	char buff[8] = "XX";

	if(!(val < -9 && wrapThresh < 1000))
	{
		sprintf(buff, "%d", val >= wrapThresh ? val%wrapThresh : val);
	}
	MN_DrTextA(buff, x-MN_TextAWidth(buff)/2, y);
}

//==========================================================================
//
// DrNumberBold
//
//==========================================================================

static void DrNumberBold(int val, int x, int y, int wrapThresh)
{
	char buff[8] = "XX";

	if(!(val < -9 && wrapThresh < 1000))
	{
		sprintf(buff, "%d", val >= wrapThresh ? val%wrapThresh : val);
	}
	MN_DrTextAYellow(buff, x-MN_TextAWidth(buff)/2, y);
}

//===========================================================================
//
// DrawHubText
//
//===========================================================================
#if 0
static void DrawHubText(void)
{
	int		count;
	char	*ch;
	int		c;
	int		cx, cy;
	patch_t *w;

	cy = 5;
	cx = 10;
	ch = HubText;
	count = (intertime-10)/TEXTSPEED;
	if (count < 0)
	{
		count = 0;
	}
	for(; count; count--)
	{
		c = *ch++;
		if(!c)
		{
			break;
		}
		if(c == '\n')
		{
			cx = 10;
			cy += 9;
			continue;
		}
		if(c < 32)
		{
			continue;
		}
		c = toupper(c);
		if(c == 32)
		{
			cx += 5;
			continue;
		}
		w = W_CacheLumpNum(FontABaseLump+c-33, PU_CACHE);
		if(cx+w->width > SCREENWIDTH)
		{
			break;
		}
		GL_DrawPatch(cx, cy, FontABaseLump+c-33);
		cx += w->width;
	}
}
#endif
