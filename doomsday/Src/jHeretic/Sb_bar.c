
// SB_bar.c

#include "Doomdef.h"
#include "P_local.h"
#include "Soundst.h"
#include "settings.h"

// Macros

#define CHEAT_ENCRYPT(a) \
	((((a)&1)<<5)+ \
	(((a)&2)<<1)+ \
	(((a)&4)<<4)+ \
	(((a)&8)>>3)+ \
	(((a)&16)>>3)+ \
	(((a)&32)<<2)+ \
	(((a)&64)>>2)+ \
	(((a)&128)>>4))

// Types

typedef struct Cheat_s
{
	void (*func)(player_t *player, struct Cheat_s *cheat);
	byte *sequence;
	byte *pos;
	int args[2];
	int currentArg;
} Cheat_t;

// Private Functions

//static void DrawSoundInfo(void);
//static void ShadeLine(int x, int y, int height, int shade);
static void ShadeChain(void);
static void DrINumber(signed int val, int x, int y);
static void DrBNumber(signed int val, int x, int y);
static void DrawCommonBar(void);
static void DrawMainBar(void);
static void DrawInventoryBar(void);
static void DrawFullScreenStuff(void);
static boolean HandleCheats(byte key);
static boolean CheatAddKey(Cheat_t *cheat, byte key, boolean *eat);
static void CheatGodFunc(player_t *player, Cheat_t *cheat);
static void CheatNoClipFunc(player_t *player, Cheat_t *cheat);
static void CheatWeaponsFunc(player_t *player, Cheat_t *cheat);
static void CheatPowerFunc(player_t *player, Cheat_t *cheat);
static void CheatHealthFunc(player_t *player, Cheat_t *cheat);
static void CheatKeysFunc(player_t *player, Cheat_t *cheat);
static void CheatSoundFunc(player_t *player, Cheat_t *cheat);
static void CheatTickerFunc(player_t *player, Cheat_t *cheat);
static void CheatArtifact1Func(player_t *player, Cheat_t *cheat);
static void CheatArtifact2Func(player_t *player, Cheat_t *cheat);
static void CheatArtifact3Func(player_t *player, Cheat_t *cheat);
static void CheatWarpFunc(player_t *player, Cheat_t *cheat);
static void CheatChickenFunc(player_t *player, Cheat_t *cheat);
static void CheatMassacreFunc(player_t *player, Cheat_t *cheat);
static void CheatIDKFAFunc(player_t *player, Cheat_t *cheat);
static void CheatIDDQDFunc(player_t *player, Cheat_t *cheat);

// Public Data

/*int sbarscale = 20;
int showFullscreenMana = 1, showFullscreenArmor = 1;
int tomeCounter = 10, tomeSound = 3;*/

//boolean DebugSound; // debug flag for displaying sound info

boolean inventory;
int curpos;
int inv_ptr;
int ArtifactFlash;

// Private Data

static int HealthMarker;
static int ChainWiggle;
static player_t *CPlayer;
int playpalette;

int PatchLTFACE;
int PatchRTFACE;
int PatchBARBACK;
int PatchCHAIN;
int PatchSTATBAR;
int PatchLIFEGEM;
//int PatchEMPWEAP;
//int PatchLIL4BOX;
int PatchLTFCTOP;
int PatchRTFCTOP;
//int PatchARMORBOX;
//int PatchARTIBOX;
int PatchSELECTBOX;
//int PatchKILLSPIC;
//int PatchMANAPIC;
//int PatchPOWERICN;
int PatchINVLFGEM1;
int PatchINVLFGEM2;
int PatchINVRTGEM1;
int PatchINVRTGEM2;
int PatchINumbers[10];
int PatchNEGATIVE;
int PatchSmNumbers[10];
int PatchBLACKSQ;
int PatchINVBAR;
int PatchARMCLEAR;
int PatchCHAINBACK;
//byte *ShadeTables;
extern byte *screen;
int FontBNumBase;
int spinbooklump;
int spinflylump;

static byte CheatLookup[256];

// Toggle god mode
static byte CheatGodSeq[] =
{
	CHEAT_ENCRYPT('q'),
	CHEAT_ENCRYPT('u'),
	CHEAT_ENCRYPT('i'),
	CHEAT_ENCRYPT('c'),
	CHEAT_ENCRYPT('k'),
	CHEAT_ENCRYPT('e'),
	CHEAT_ENCRYPT('n'),
	0xff
};

// Toggle no clipping mode
static byte CheatNoClipSeq[] =
{
	CHEAT_ENCRYPT('k'),
	CHEAT_ENCRYPT('i'),
	CHEAT_ENCRYPT('t'),
	CHEAT_ENCRYPT('t'),
	CHEAT_ENCRYPT('y'),
	0xff
};

// Get all weapons and ammo
static byte CheatWeaponsSeq[] =
{
	CHEAT_ENCRYPT('r'),
	CHEAT_ENCRYPT('a'),
	CHEAT_ENCRYPT('m'),
	CHEAT_ENCRYPT('b'),
	CHEAT_ENCRYPT('o'),
	0xff
};

// Toggle tome of power
static byte CheatPowerSeq[] =
{
	CHEAT_ENCRYPT('s'),
	CHEAT_ENCRYPT('h'),
	CHEAT_ENCRYPT('a'),
	CHEAT_ENCRYPT('z'),
	CHEAT_ENCRYPT('a'),
	CHEAT_ENCRYPT('m'),
	0xff, 0
};

// Get full health
static byte CheatHealthSeq[] =
{
	CHEAT_ENCRYPT('p'),
	CHEAT_ENCRYPT('o'),
	CHEAT_ENCRYPT('n'),
	CHEAT_ENCRYPT('c'),
	CHEAT_ENCRYPT('e'),
	0xff
};

// Get all keys
static byte CheatKeysSeq[] =
{
	CHEAT_ENCRYPT('s'),
	CHEAT_ENCRYPT('k'),
	CHEAT_ENCRYPT('e'),
	CHEAT_ENCRYPT('l'),
	0xff, 0
};

// Toggle sound debug info
static byte CheatSoundSeq[] =
{
	CHEAT_ENCRYPT('n'),
	CHEAT_ENCRYPT('o'),
	CHEAT_ENCRYPT('i'),
	CHEAT_ENCRYPT('s'),
	CHEAT_ENCRYPT('e'),
	0xff
};

// Toggle ticker
static byte CheatTickerSeq[] =
{
	CHEAT_ENCRYPT('t'),
	CHEAT_ENCRYPT('i'),
	CHEAT_ENCRYPT('c'),
	CHEAT_ENCRYPT('k'),
	CHEAT_ENCRYPT('e'),
	CHEAT_ENCRYPT('r'),
	0xff, 0
};

// Get an artifact 1st stage (ask for type)
static byte CheatArtifact1Seq[] =
{
	CHEAT_ENCRYPT('g'),
	CHEAT_ENCRYPT('i'),
	CHEAT_ENCRYPT('m'),
	CHEAT_ENCRYPT('m'),
	CHEAT_ENCRYPT('e'),
	0xff
};

// Get an artifact 2nd stage (ask for count)
static byte CheatArtifact2Seq[] =
{
	CHEAT_ENCRYPT('g'),
	CHEAT_ENCRYPT('i'),
	CHEAT_ENCRYPT('m'),
	CHEAT_ENCRYPT('m'),
	CHEAT_ENCRYPT('e'),
	0, 0xff, 0
};

// Get an artifact final stage
static byte CheatArtifact3Seq[] =
{
	CHEAT_ENCRYPT('g'),
	CHEAT_ENCRYPT('i'),
	CHEAT_ENCRYPT('m'),
	CHEAT_ENCRYPT('m'),
	CHEAT_ENCRYPT('e'),
	0, 0, 0xff
};

// Warp to new level
static byte CheatWarpSeq[] =
{
	CHEAT_ENCRYPT('e'),
	CHEAT_ENCRYPT('n'),
	CHEAT_ENCRYPT('g'),
	CHEAT_ENCRYPT('a'),
	CHEAT_ENCRYPT('g'),
	CHEAT_ENCRYPT('e'),
	0, 0, 0xff, 0
};

// Save a screenshot
static byte CheatChickenSeq[] =
{
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

// Kill all monsters
static byte CheatMassacreSeq[] =
{
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

static byte CheatIDKFASeq[] =
{
	CHEAT_ENCRYPT('i'),
	CHEAT_ENCRYPT('d'),
	CHEAT_ENCRYPT('k'),
	CHEAT_ENCRYPT('f'),
	CHEAT_ENCRYPT('a'),
	0xff, 0
};

static byte CheatIDDQDSeq[] =
{
	CHEAT_ENCRYPT('i'),
	CHEAT_ENCRYPT('d'),
	CHEAT_ENCRYPT('d'),
	CHEAT_ENCRYPT('q'),
	CHEAT_ENCRYPT('d'),
	0xff, 0
};

static Cheat_t Cheats[] =
{
	{ CheatGodFunc, CheatGodSeq, NULL, 0, 0, 0 },
	{ CheatNoClipFunc, CheatNoClipSeq, NULL, 0, 0, 0 },
	{ CheatWeaponsFunc, CheatWeaponsSeq, NULL, 0, 0, 0 },
	{ CheatPowerFunc, CheatPowerSeq, NULL, 0, 0, 0 },
	{ CheatHealthFunc, CheatHealthSeq, NULL, 0, 0, 0 },
	{ CheatKeysFunc, CheatKeysSeq, NULL, 0, 0, 0 },
	{ CheatSoundFunc, CheatSoundSeq, NULL, 0, 0, 0 },
	{ CheatTickerFunc, CheatTickerSeq, NULL, 0, 0, 0 },
	{ CheatArtifact1Func, CheatArtifact1Seq, NULL, 0, 0, 0 },
	{ CheatArtifact2Func, CheatArtifact2Seq, NULL, 0, 0, 0 },
	{ CheatArtifact3Func, CheatArtifact3Seq, NULL, 0, 0, 0 },
	{ CheatWarpFunc, CheatWarpSeq, NULL, 0, 0, 0 },
	{ CheatChickenFunc, CheatChickenSeq, NULL, 0, 0, 0 },
	{ CheatMassacreFunc, CheatMassacreSeq, NULL, 0, 0, 0 },
	{ CheatIDKFAFunc, CheatIDKFASeq, NULL, 0, 0, 0 },
	{ CheatIDDQDFunc, CheatIDDQDSeq, NULL, 0, 0, 0 },
	{ NULL, NULL, NULL, 0, 0, 0 } // Terminator
};

//---------------------------------------------------------------------------
//
// PROC SB_Init
//
//---------------------------------------------------------------------------

void SB_Init(void)
{
	int i;
	int startLump;

	PatchLTFACE = W_GetNumForName("LTFACE");
	PatchRTFACE = W_GetNumForName("RTFACE");
	PatchBARBACK = W_GetNumForName("BARBACK");
	PatchINVBAR = W_GetNumForName("INVBAR");
	PatchCHAIN = W_GetNumForName("CHAIN");
	if(deathmatch)
	{
		PatchSTATBAR = W_GetNumForName("STATBAR");
	}
	else
	{
		PatchSTATBAR = W_GetNumForName("LIFEBAR");
	}
	if(!IS_NETGAME)
	{ // single player game uses red life gem
		PatchLIFEGEM = W_GetNumForName("LIFEGEM2");
	}
	else
	{
		PatchLIFEGEM = W_GetNumForName("LIFEGEM0") + consoleplayer;
	}
	PatchLTFCTOP = W_GetNumForName("LTFCTOP");
	PatchRTFCTOP = W_GetNumForName("RTFCTOP");
	PatchSELECTBOX = W_GetNumForName("SELECTBOX");
	PatchINVLFGEM1 = W_GetNumForName("INVGEML1");
	PatchINVLFGEM2 = W_GetNumForName("INVGEML2");
	PatchINVRTGEM1 = W_GetNumForName("INVGEMR1");
	PatchINVRTGEM2 = W_GetNumForName("INVGEMR2");
	PatchBLACKSQ = W_GetNumForName("BLACKSQ");
	PatchARMCLEAR = W_GetNumForName("ARMCLEAR");
	PatchCHAINBACK = W_GetNumForName("CHAINBACK");
	startLump = W_GetNumForName("IN0");
	for(i = 0; i < 10; i++)
	{
		PatchINumbers[i] = startLump+i;
	}
	PatchNEGATIVE = W_GetNumForName("NEGNUM");
	FontBNumBase = W_GetNumForName("FONTB16");
	startLump = W_GetNumForName("SMALLIN0");
	for(i = 0; i < 10; i++)
	{
		PatchSmNumbers[i] = startLump+i;
	}
	playpalette = W_GetNumForName("PLAYPAL");
	spinbooklump = W_GetNumForName("SPINBK0");
	spinflylump = W_GetNumForName("SPFLY0");
	for(i = 0; i < 256; i++)
	{
		CheatLookup[i] = CHEAT_ENCRYPT(i);
	}
}

//---------------------------------------------------------------------------
//
// PROC SB_Ticker
//
//---------------------------------------------------------------------------

void SB_Ticker(void)
{
	int delta;
	int curHealth;
	static int tomePlay = 0;

	if(leveltime&1)
	{
		ChainWiggle = P_Random()&1;
	}
	curHealth = players[consoleplayer].plr->mo->health;
	if(curHealth < 0)
	{
		curHealth = 0;
	}
	if(curHealth < HealthMarker)
	{
		delta = (HealthMarker-curHealth)>>2;
		if(delta < 1)
		{
			delta = 1;
		}
		else if(delta > 8)
		{
			delta = 8;
		}
		HealthMarker -= delta;
	}
	else if(curHealth > HealthMarker)
	{
		delta = (curHealth-HealthMarker)>>2;
		if(delta < 1)
		{
			delta = 1;
		}
		else if(delta > 8)
		{
			delta = 8;
		}
		HealthMarker += delta;
	}
	// Tome of Power countdown sound.
	if(players[consoleplayer].powers[pw_weaponlevel2] && 
		players[consoleplayer].powers[pw_weaponlevel2] < cfg.tomeSound*35)
	{
		int timeleft = players[consoleplayer].powers[pw_weaponlevel2]/35;
		if(tomePlay != timeleft)
		{
			tomePlay = timeleft;
			S_LocalSound(sfx_keyup, NULL);
		}
	}
}

//---------------------------------------------------------------------------
//
// PROC DrINumber
//
// Draws a three digit (!) number. Limited to 999.
//
//---------------------------------------------------------------------------

static void DrINumber(signed int val, int x, int y)
{
	int oldval;

	// Limit to 999.
	if(val > 999) val = 999;

	oldval = val;
	if(val < 0)
	{
		if(val < -9)
		{
			GL_DrawPatch(x+1, y+1, W_GetNumForName("LAME"));
		}
		else
		{
			val = -val;
			GL_DrawPatch(x+18, y, PatchINumbers[val]);
			GL_DrawPatch(x+9, y, PatchNEGATIVE);
		}
		return;
	}
	if(val > 99)
	{
		GL_DrawPatch(x, y, PatchINumbers[val/100]);
	}
	val = val%100;
	if(val > 9 || oldval > 99)
	{
		GL_DrawPatch(x+9, y, PatchINumbers[val/10]);
	}
	val = val%10;
	GL_DrawPatch(x+18, y, PatchINumbers[val]);
}

//---------------------------------------------------------------------------
//
// PROC DrBNumber
//
// Draws a three digit number using FontB
//
//---------------------------------------------------------------------------

static void DrBNumber(signed int val, int x, int y)
{
	patch_t *patch;
	int xpos;
	int oldval;

	oldval = val;
	xpos = x;
	if(val < 0)
	{
		val = 0;
	}
	if(val > 99)
	{
		patch = W_CacheLumpNum(FontBNumBase+val/100, PU_CACHE);
		GL_DrawShadowedPatch(xpos+6-patch->width/2, y, FontBNumBase+val/100);
	}
	val = val%100;
	xpos += 12;
	if(val > 9 || oldval > 99)
	{
		patch = W_CacheLumpNum(FontBNumBase+val/10, PU_CACHE);
		GL_DrawShadowedPatch(xpos+6-patch->width/2, y, FontBNumBase+val/10);
	}
	val = val%10;
	xpos += 12;
	patch = W_CacheLumpNum(FontBNumBase+val, PU_CACHE);
	GL_DrawShadowedPatch(xpos+6-patch->width/2, y, FontBNumBase+val);
}

//---------------------------------------------------------------------------
//
// PROC DrSmallNumber
//
// Draws a small two digit number.
//
//---------------------------------------------------------------------------

static void _DrSmallNumber(int val, int x, int y, boolean skipone)
{
	if(skipone && val == 1)
	{
		return;
	}
	if(val > 9)
	{
		GL_DrawPatch(x, y, PatchSmNumbers[val/10]);
	}
	val = val%10;
	GL_DrawPatch(x+4, y, PatchSmNumbers[val]);
}

static void DrSmallNumber(int val, int x, int y)
{
	_DrSmallNumber(val, x, y, true);
}

//---------------------------------------------------------------------------
//
// PROC ShadeLine
//
//---------------------------------------------------------------------------

/*static void ShadeLine(int x, int y, int height, int shade)
{
	byte *dest;
	byte *shades;

	shades = colormaps+9*256+shade*2*256;
	dest = screen+y*SCREENWIDTH+x;
	while(height--)
	{
		*(dest) = *(shades+*dest);
		dest += SCREENWIDTH;
	}
}*/

//---------------------------------------------------------------------------
//
// PROC ShadeChain
//
//---------------------------------------------------------------------------

static void ShadeChain(void)
{
/*	int i;

	for(i = 0; i < 16; i++)
	{
		ShadeLine(277+i, 190, 10, i/2);
		ShadeLine(19+i, 190, 10, 7-(i/2));
	}*/

	gl.Disable(DGL_TEXTURING);
	gl.Begin(DGL_QUADS);

	// The left shader.
	gl.Color4f(0, 0, 0, .6f);
	gl.Vertex2f(20, 200);
	gl.Vertex2f(20, 190);
	gl.Color4f(0, 0, 0, 0);
	gl.Vertex2f(35, 190);
	gl.Vertex2f(35, 200);

	// The right shader.
	gl.Vertex2f(277, 200);
	gl.Vertex2f(277, 190);
	gl.Color4f(0, 0, 0, .6f);
	gl.Vertex2f(293, 190);
	gl.Vertex2f(293, 200);

	gl.End();
	gl.Enable(DGL_TEXTURING);
}

//---------------------------------------------------------------------------
//
// PROC DrawSoundInfo
//
// Displays sound debugging information.
//
//---------------------------------------------------------------------------

/*static void DrawSoundInfo(void)
{
	int i;
	SoundInfo_t s;
	ChanInfo_t *c;
	char text[32];
	int x;
	int y;
	int xPos[7] = {1, 75, 112, 156, 200, 230, 260};

	if(leveltime&16)
	{
		MN_DrTextA("*** SOUND DEBUG INFO ***", xPos[0], 20);
	}
	S_GetChannelInfo(&s);
	if(s.channelCount == 0)
	{
		return;
	}
	x = 0;
	MN_DrTextA("NAME", xPos[x++], 30);
	MN_DrTextA("MO.T", xPos[x++], 30);
	MN_DrTextA("MO.X", xPos[x++], 30);
	MN_DrTextA("MO.Y", xPos[x++], 30);
	MN_DrTextA("ID", xPos[x++], 30);
	MN_DrTextA("PRI", xPos[x++], 30);
	MN_DrTextA("DIST", xPos[x++], 30);
	for(i = 0; i < s.channelCount; i++)
	{
		c = &s.chan[i];
		x = 0;
		y = 40+i*10;
		sprintf(text, "%s", c->name);
		M_ForceUppercase(text);
		MN_DrTextA(text, xPos[x++], y);
		if(c->mo) 
		{
			sprintf(text, "%d", c->mo->type);
			MN_DrTextA(text, xPos[x++], y);
			sprintf(text, "%d", c->mo->x>>FRACBITS);
			MN_DrTextA(text, xPos[x++], y);
			sprintf(text, "%d", c->mo->y>>FRACBITS);
			MN_DrTextA(text, xPos[x++], y);
		}
		sprintf(text, "%d", c->id);
		MN_DrTextA(text, xPos[x++], y);
		sprintf(text, "%d", c->priority);
		MN_DrTextA(text, xPos[x++], y);
		sprintf(text, "%d", c->distance);
		MN_DrTextA(text, xPos[x++], y);
	}
	GL_Update(DDUF_FULLSCREEN | DDUF_BORDER);
}
*/
//---------------------------------------------------------------------------
//
// PROC SB_Drawer
//
//---------------------------------------------------------------------------

char patcharti[][10] =
{
	{"ARTIBOX"},    // none
	{"ARTIINVU"},   // invulnerability
	{"ARTIINVS"},   // invisibility
	{"ARTIPTN2"},   // health
	{"ARTISPHL"},   // superhealth
	{"ARTIPWBK"},   // tomeofpower
	{"ARTITRCH"},   // torch
	{"ARTIFBMB"},   // firebomb
	{"ARTIEGGC"},   // egg
	{"ARTISOAR"},   // fly
	{"ARTIATLP"}    // teleport
};

char ammopic[][10] =
{
	{"INAMGLD"},
	{"INAMBOW"},
	{"INAMBST"},
	{"INAMRAM"},
	{"INAMPNX"},
	{"INAMLOB"}
};

static int oldarti = 0;
static int oldartiCount = 0;
static int oldfrags = -9999;
static int oldammo = -1;
static int oldarmor = -1;
static int oldweapon = -1;
static int oldhealth = -1;
static int oldlife = -1;
static int oldkeys = -1;

int playerkeys = 0;

extern boolean automapactive;

void SB_Drawer(void)
{
	int frame;
	static boolean hitCenterFrame;

	CPlayer = &players[consoleplayer];
	if(Get(DD_VIEWWINDOW_HEIGHT) == SCREENHEIGHT && !automapactive)
	{
		DrawFullScreenStuff();
	}
	else
	{
		float fscale = cfg.sbarscale/20.0f;
		// Setup special status bar matrix.
		if(cfg.sbarscale != 20)
		{
			gl.MatrixMode(DGL_MODELVIEW);
			gl.PushMatrix();
			gl.Translatef(160 - 320*fscale/2, 200*(1-fscale), 0);
			gl.Scalef(fscale, fscale, 1);
		}

		GL_DrawPatch(0, 158, PatchBARBACK);
		if(players[consoleplayer].cheats&CF_GODMODE)
		{
			GL_DrawPatch(16, 167, W_GetNumForName("GOD1"));
			GL_DrawPatch(287, 167, W_GetNumForName("GOD2"));
		}
		oldhealth = -1;

		DrawCommonBar();
		if(!inventory)
		{
			// Main interface
			GL_DrawPatch(34, 160, PatchSTATBAR);
			oldarti = 0;
			oldammo = -1;
			oldarmor = -1;
			oldweapon = -1;
			oldfrags = -9999; //can't use -1, 'cuz of negative frags
			oldlife = -1;
			oldkeys = -1;

			DrawMainBar();
		}
		else
		{
			GL_DrawPatch(34, 160, PatchINVBAR);

			DrawInventoryBar();
		}
	
		// Restore the old modelview matrix.
		if(cfg.sbarscale != 20)
		{
			gl.MatrixMode(DGL_MODELVIEW);
			gl.PopMatrix();
		}
	}
	SB_PaletteFlash();

	// Flight icons
	if(CPlayer->powers[pw_flight])
	{
		int offset = (cfg.showFullscreenMana && cfg.screenblocks>10 
			&& CPlayer->readyweapon > 0 && CPlayer->readyweapon < 7)? 43 : 0;
		if(CPlayer->powers[pw_flight] > BLINKTHRESHOLD
			|| !(CPlayer->powers[pw_flight]&16))
		{
			frame = (leveltime/3)&15;
			if(CPlayer->plr->mo->flags2&MF2_FLY)
			{
				if(hitCenterFrame && (frame != 15 && frame != 0))
				{
					GL_DrawPatch(20+offset, 17, spinflylump+15);
				}
				else
				{
					GL_DrawPatch(20+offset, 17, spinflylump+frame);
					hitCenterFrame = false;
				}
			}
			else
			{
				if(!hitCenterFrame && (frame != 15 && frame != 0))
				{
					GL_DrawPatch(20+offset, 17, spinflylump+frame);
					hitCenterFrame = false;
				}
				else
				{
					GL_DrawPatch(20+offset, 17, spinflylump+15);
					hitCenterFrame = true;
				}
			}
			GL_Update(DDUF_TOP | DDUF_MESSAGES);
		}
		else
		{
			GL_Update(DDUF_TOP | DDUF_MESSAGES);
		}
	}

	if(CPlayer->powers[pw_weaponlevel2] && !CPlayer->chickenTics)
	{
		if(cfg.tomeCounter || CPlayer->powers[pw_weaponlevel2] > BLINKTHRESHOLD
			|| !(CPlayer->powers[pw_weaponlevel2]&16))
		{
			frame = (leveltime/3)&15;
			if(cfg.tomeCounter && CPlayer->powers[pw_weaponlevel2] < 35)
			{
				gl.Color4f(1, 1, 1, CPlayer->powers[pw_weaponlevel2] / 35.0f);
			}
			GL_DrawPatch_CS(300, 17, spinbooklump+frame);
			GL_Update(DDUF_TOP | DDUF_MESSAGES);
		}
		else
		{
			GL_Update(DDUF_TOP | DDUF_MESSAGES);
		}
		if(CPlayer->powers[pw_weaponlevel2] < cfg.tomeCounter*35)
		{
			_DrSmallNumber(1+CPlayer->powers[pw_weaponlevel2]/35, 303, 30, false);
		}
	}
}

// sets the new palette based upon current values of player->damagecount
// and player->bonuscount
void SB_PaletteFlash(void)
{
	static int sb_palette = 0;
	int palette;
//	byte *pal;

	CPlayer = &players[consoleplayer];

	if(CPlayer->damagecount)
	{
		palette = (CPlayer->damagecount+7)>>3;
		if(palette >= NUMREDPALS)
		{
			palette = NUMREDPALS-1;
		}
		palette += STARTREDPALS;
	}
	else if(CPlayer->bonuscount)
	{
		palette = (CPlayer->bonuscount+7)>>3;
		if(palette >= NUMBONUSPALS)
		{
			palette = NUMBONUSPALS-1;
		}
		palette += STARTBONUSPALS;
	}
	else
	{
		palette = 0;
	}
	if(palette != sb_palette)
	{
		sb_palette = palette;
		//pal = (byte *)W_CacheLumpNum(playpalette, PU_CACHE)+palette*768;
		//H_SetFilter(palette);
		CPlayer->plr->filter = H_GetFilterColor(palette); // $democam
//		I_SetPalette(pal);
	}
}

//---------------------------------------------------------------------------
//
// PROC DrawCommonBar
//
//---------------------------------------------------------------------------

void DrawCommonBar(void)
{
	int chainY;
	int healthPos;

	GL_DrawPatch(0, 148, PatchLTFCTOP);
	GL_DrawPatch(290, 148, PatchRTFCTOP);

	if(oldhealth != HealthMarker)
	{
		oldhealth = HealthMarker;
		healthPos = HealthMarker;
		if(healthPos < 0)
		{
			healthPos = 0;
		}
		if(healthPos > 100)
		{
			healthPos = 100;
		}
		healthPos = (healthPos*256)/100;
		chainY = (HealthMarker == CPlayer->plr->mo->health) ? 191 : 191+ChainWiggle;
		GL_DrawPatch(0, 190, PatchCHAINBACK);
		GL_DrawPatch(2+(healthPos%17), chainY, PatchCHAIN);
		GL_DrawPatch(17+healthPos, chainY, PatchLIFEGEM);
		GL_DrawPatch(0, 190, PatchLTFACE);
		GL_DrawPatch(276, 190, PatchRTFACE);
		ShadeChain();
		GL_Update(DDUF_STATBAR);
	}
}

//---------------------------------------------------------------------------
//
// PROC DrawMainBar
//
//---------------------------------------------------------------------------

void DrawMainBar(void)
{
	int i;
	int temp;

	// Ready artifact
	if(ArtifactFlash)
	{
		GL_DrawPatch(180, 161, PatchBLACKSQ);
		GL_DrawPatch(182, 161, W_GetNumForName("useartia") + ArtifactFlash - 1);
		ArtifactFlash--;
		oldarti = -1; // so that the correct artifact fills in after the flash
		//UpdateState |= I_STATBAR;
		GL_Update(DDUF_STATBAR);
	}
	else if(oldarti != CPlayer->readyArtifact
		|| oldartiCount != CPlayer->inventory[inv_ptr].count)
	{
		GL_DrawPatch(180, 161, PatchBLACKSQ);
		if(CPlayer->readyArtifact > 0)
		{
			GL_DrawPatch(179,160, W_GetNumForName(patcharti[CPlayer->readyArtifact]));
			DrSmallNumber(CPlayer->inventory[inv_ptr].count, 201, 182);
		}
		oldarti = CPlayer->readyArtifact;
		oldartiCount = CPlayer->inventory[inv_ptr].count;
		//UpdateState |= I_STATBAR;
		GL_Update(DDUF_STATBAR);
	}

	// Frags
	if(deathmatch)
	{
		temp = 0;
		for(i = 0; i < MAXPLAYERS; i++)
		{
			temp += CPlayer->frags[i];
		}
		if(temp != oldfrags)
		{
			GL_DrawPatch(57, 171, PatchARMCLEAR);
			DrINumber(temp, 61, 170);
			oldfrags = temp;
			//UpdateState |= I_STATBAR;
			GL_Update(DDUF_STATBAR);
		}
	}
	else
	{
		temp = HealthMarker;
		if(temp < 0)
		{
			temp = 0;
		}
		else if(temp > 100)
		{
			temp = 100;
		}
		if(oldlife != temp)
		{
			oldlife = temp;
			GL_DrawPatch(57, 171, PatchARMCLEAR);
			DrINumber(temp, 61, 170);
			//UpdateState |= I_STATBAR;
			GL_Update(DDUF_STATBAR);
		}
	}

	// Keys
	if(oldkeys != playerkeys)
	{
		if(CPlayer->keys[key_yellow])
		{
			GL_DrawPatch(153, 164, W_GetNumForName("ykeyicon"));
		}
		if(CPlayer->keys[key_green])
		{
			GL_DrawPatch(153, 172, W_GetNumForName("gkeyicon"));
		}
		if(CPlayer->keys[key_blue])
		{
			GL_DrawPatch(153, 180, W_GetNumForName("bkeyicon"));
		}
		oldkeys = playerkeys;
		//UpdateState |= I_STATBAR;
		GL_Update(DDUF_STATBAR);
	}
	// Ammo
	temp = CPlayer->ammo[wpnlev1info[CPlayer->readyweapon].ammo];
	if(oldammo != temp || oldweapon != CPlayer->readyweapon)
	{
		GL_DrawPatch(108, 161, PatchBLACKSQ);
		if(temp && CPlayer->readyweapon > 0 && CPlayer->readyweapon < 7)
		{
			DrINumber(temp, 109, 162);
			GL_DrawPatch(111, 172, W_GetNumForName(ammopic[CPlayer->readyweapon-1]));
		}
		oldammo = temp;
		oldweapon = CPlayer->readyweapon;
		//UpdateState |= I_STATBAR;
		GL_Update(DDUF_STATBAR);
	}

	// Armor
	if(oldarmor != CPlayer->armorpoints)
	{
		GL_DrawPatch(224, 171, PatchARMCLEAR);
		DrINumber(CPlayer->armorpoints, 228, 170);
		oldarmor = CPlayer->armorpoints;
		GL_Update(DDUF_STATBAR);
	}
}

//---------------------------------------------------------------------------
//
// PROC DrawInventoryBar
//
//---------------------------------------------------------------------------

void DrawInventoryBar(void)
{
	int i;
	int x;

	x = inv_ptr-curpos;
	//UpdateState |= I_STATBAR;
	GL_Update(DDUF_STATBAR);
	GL_DrawPatch(34, 160, PatchINVBAR);
	for(i = 0; i < 7; i++)
	{
		//V_DrawPatch(50+i*31, 160, W_CacheLumpName("ARTIBOX", PU_CACHE));
		if(CPlayer->inventorySlotNum > x+i
			&& CPlayer->inventory[x+i].type != arti_none)
		{
			GL_DrawPatch(50+i*31, 160, 
				W_GetNumForName(patcharti[CPlayer->inventory[x+i].type]));
			DrSmallNumber(CPlayer->inventory[x+i].count, 69+i*31, 182);
		}
	}
	GL_DrawPatch(50+curpos*31, 189, PatchSELECTBOX);
	if(x != 0)
	{
		GL_DrawPatch(38, 159,!(leveltime&4) ? PatchINVLFGEM1 :
			PatchINVLFGEM2);
	}
	if(CPlayer->inventorySlotNum-x > 7)
	{
		GL_DrawPatch(269, 159,!(leveltime&4) ?
			PatchINVRTGEM1 : PatchINVRTGEM2);
	}
}

void DrawFullScreenStuff(void)
{
	int i;
	int x;
	int temp;

	GL_Update(DDUF_FULLSCREEN);
	if(CPlayer->plr->mo->health > 0)
	{
		DrBNumber(CPlayer->plr->mo->health, 5, 180);
	}
	else
	{
		DrBNumber(0, 5, 180);
	}

	if(cfg.showFullscreenMana)
	{
		temp = CPlayer->ammo[wpnlev1info[CPlayer->readyweapon].ammo];
		if(CPlayer->readyweapon > 0 && CPlayer->readyweapon < 7)
		{
			GL_DrawPatch(-1, 0, W_GetNumForName(ammopic[CPlayer->readyweapon-1]));
			DrINumber(temp, 18, 2);
		}
		GL_Update(DDUF_TOP);
	}
	if(cfg.showFullscreenArmor)
	{
		DrINumber(CPlayer->armorpoints, 10, cfg.showFullscreenKeys? 
			160 : 168);
	}
	if(cfg.showFullscreenKeys)
	{
		x = 9;
		temp = 172;
		// Draw keys above armor.
		if(CPlayer->keys[key_yellow])
		{
			GL_DrawPatch(x, temp, W_GetNumForName("ykeyicon"));
			x += 11;
		}
		if(CPlayer->keys[key_green])
		{
			GL_DrawPatch(x, temp, W_GetNumForName("gkeyicon"));
			x += 11;
		}
		if(CPlayer->keys[key_blue])
		{
			GL_DrawPatch(x, temp, W_GetNumForName("bkeyicon"));
		}
	}

	if(deathmatch)
	{
		temp = 0;
		for(i=0; i<MAXPLAYERS; i++)
		{
			if(players[i].plr->ingame)
			{
				temp += CPlayer->frags[i];
			}
		}
		DrINumber(temp, 45, 185);
	}
	if(!inventory)
	{
		if(CPlayer->readyArtifact > 0)
		{
			GL_DrawFuzzPatch(286, 170, W_GetNumForName("ARTIBOX"));
			GL_DrawPatch(286, 170,
				W_GetNumForName(patcharti[CPlayer->readyArtifact]));
			DrSmallNumber(CPlayer->inventory[inv_ptr].count, 307, 192);
		}
	}
	else
	{
		x = inv_ptr-curpos;
		for(i = 0; i < 7; i++)
		{
			GL_DrawFuzzPatch(50+i*31, 168, W_GetNumForName("ARTIBOX"));
			if(CPlayer->inventorySlotNum > x+i
				&& CPlayer->inventory[x+i].type != arti_none)
			{
				GL_DrawPatch(50+i*31, 168, W_GetNumForName(
					patcharti[CPlayer->inventory[x+i].type]));
				DrSmallNumber(CPlayer->inventory[x+i].count, 69+i*31, 190);
			}
		}
		GL_DrawPatch(50+curpos*31, 197, PatchSELECTBOX);
		if(x != 0)
		{
			GL_DrawPatch(38, 167, !(leveltime&4) ? PatchINVLFGEM1 :
				PatchINVLFGEM2);
		}
		if(CPlayer->inventorySlotNum-x > 7)
		{
			GL_DrawPatch(269, 167, !(leveltime&4) ?
				PatchINVRTGEM1 : PatchINVRTGEM2);
		}
	}
}

//--------------------------------------------------------------------------
//
// FUNC SB_Responder
//
//--------------------------------------------------------------------------

boolean SB_Responder(event_t *event)
{
	if(event->type == ev_keydown)
	{
		if(HandleCheats(event->data1))
		{ // Need to eat the key
			return(true);
		}
	}
	return(false);
}

//--------------------------------------------------------------------------
//
// FUNC HandleCheats
//
// Returns true if the caller should eat the key.
//
//--------------------------------------------------------------------------

static boolean HandleCheats(byte key)
{
	int i;
	boolean eat;

	if(IS_NETGAME || gameskill == sk_nightmare)
	{ // Can't cheat in a net-game, or in nightmare mode
		return(false);
	}
	if(players[consoleplayer].health <= 0)
	{ // Dead players can't cheat
		return(false);
	}
	eat = false;
	for(i = 0; Cheats[i].func != NULL; i++)
	{
		if(CheatAddKey(&Cheats[i], key, &eat))
		{
			Cheats[i].func(&players[consoleplayer], &Cheats[i]);
			S_LocalSound(sfx_dorcls, NULL);
		}
	}
	return(eat);
}

//--------------------------------------------------------------------------
//
// FUNC CheatAddkey
//
// Returns true if the added key completed the cheat, false otherwise.
//
//--------------------------------------------------------------------------

static boolean CheatAddKey(Cheat_t *cheat, byte key, boolean *eat)
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
	else if(CheatLookup[key] == *cheat->pos)
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
		return(true);
	}
	return(false);
}

//--------------------------------------------------------------------------
//
// CHEAT FUNCTIONS
//
//--------------------------------------------------------------------------

void cht_GodFunc(player_t *player)
{
	CheatGodFunc(player, NULL);
}

void cht_NoClipFunc(player_t *player)
{
	CheatNoClipFunc(player, NULL);
}

static void CheatGodFunc(player_t *player, Cheat_t *cheat)
{
	player->cheats ^= CF_GODMODE;
	player->update |= PSF_STATE;
	if(player->cheats & CF_GODMODE)
	{
		P_SetMessage(player, TXT_CHEATGODON, false);
	}
	else
	{
		P_SetMessage(player, TXT_CHEATGODOFF, false);
	}
}

static void CheatNoClipFunc(player_t *player, Cheat_t *cheat)
{
	player->cheats ^= CF_NOCLIP;
	player->update |= PSF_STATE;
	if(player->cheats&CF_NOCLIP)
	{
		P_SetMessage(player, TXT_CHEATNOCLIPON, false);
	}
	else
	{
		P_SetMessage(player, TXT_CHEATNOCLIPOFF, false);
	}
}

static void CheatWeaponsFunc(player_t *player, Cheat_t *cheat)
{
	int i;

	player->update |= PSF_ARMOR_POINTS | PSF_STATE | PSF_MAX_AMMO
		| PSF_AMMO | PSF_OWNED_WEAPONS;

	player->armorpoints = 200;
	player->armortype = 2;
	if(!player->backpack)
	{
		for(i = 0; i < NUMAMMO; i++)
		{
			player->maxammo[i] *= 2;
		}
		player->backpack = true;
	}
	for(i = 0; i < NUMWEAPONS-1; i++)
	{
		player->weaponowned[i] = true;
	}
	if(shareware)
	{
		player->weaponowned[wp_skullrod] = false;
		player->weaponowned[wp_phoenixrod] = false;
		player->weaponowned[wp_mace] = false;
	}
	for(i = 0; i < NUMAMMO; i++)
	{
		player->ammo[i] = player->maxammo[i];
	}
	P_SetMessage(player, TXT_CHEATWEAPONS, false);
}

static void CheatPowerFunc(player_t *player, Cheat_t *cheat)
{
	player->update |= PSF_POWERS;
	if(player->powers[pw_weaponlevel2])
	{
		player->powers[pw_weaponlevel2] = 0;
		P_SetMessage(player, TXT_CHEATPOWEROFF, false);
	}
	else
	{
		P_UseArtifact(player, arti_tomeofpower);
		P_SetMessage(player, TXT_CHEATPOWERON, false);
	}
}

static void CheatHealthFunc(player_t *player, Cheat_t *cheat)
{
	player->update |= PSF_HEALTH;
	if(player->chickenTics)
	{
		player->health = player->plr->mo->health = MAXCHICKENHEALTH;
	}
	else
	{
		player->health = player->plr->mo->health = MAXHEALTH;
	}
	P_SetMessage(player, TXT_CHEATHEALTH, false);
}

static void CheatKeysFunc(player_t *player, Cheat_t *cheat)
{
	extern int playerkeys;

	player->update |= PSF_KEYS;
	player->keys[key_yellow] = true;
	player->keys[key_green] = true;
	player->keys[key_blue] = true;
	playerkeys = 7; // Key refresh flags
	P_SetMessage(player, TXT_CHEATKEYS, false);
}

static void CheatSoundFunc(player_t *player, Cheat_t *cheat)
{
/*	DebugSound = !DebugSound;
	if(DebugSound)
	{
		P_SetMessage(player, TXT_CHEATSOUNDON, false);
	}
	else
	{
		P_SetMessage(player, TXT_CHEATSOUNDOFF, false);
	}*/
}

static void CheatTickerFunc(player_t *player, Cheat_t *cheat)
{/*	
	extern int DisplayTicker;

	DisplayTicker = !DisplayTicker;
	if(DisplayTicker)
	{
		P_SetMessage(player, TXT_CHEATTICKERON, false);
	}
	else
	{
		P_SetMessage(player, TXT_CHEATTICKEROFF, false);
	}*/
}

static void CheatArtifact1Func(player_t *player, Cheat_t *cheat)
{
	P_SetMessage(player, TXT_CHEATARTIFACTS1, false);
}

static void CheatArtifact2Func(player_t *player, Cheat_t *cheat)
{
	P_SetMessage(player, TXT_CHEATARTIFACTS2, false);
}

static void CheatArtifact3Func(player_t *player, Cheat_t *cheat)
{
	int i;
	int j;
	artitype_t type;
	int count;

	type = cheat->args[0]-'a'+1;
	count = cheat->args[1]-'0';
	if(type == 26 && count == 0)
	{ // All artifacts
		for(i = arti_none+1; i < NUMARTIFACTS; i++)
		{
			if(shareware && (i == arti_superhealth
				|| i == arti_teleport))
			{
				continue;
			}
			for(j = 0; j < 16; j++)
			{
				P_GiveArtifact(player, i, NULL);
			}
		}
		P_SetMessage(player, TXT_CHEATARTIFACTS3, false);
	}
	else if(type > arti_none && type < NUMARTIFACTS
		&& count > 0 && count < 10)
	{
		if(shareware && (type == arti_superhealth || type == arti_teleport))
		{
			P_SetMessage(player, TXT_CHEATARTIFACTSFAIL, false);
			return;
		}
		for(i = 0; i < count; i++)
		{
			P_GiveArtifact(player, type, NULL);
		}
		P_SetMessage(player, TXT_CHEATARTIFACTS3, false);
	}
	else
	{ // Bad input
		P_SetMessage(player, TXT_CHEATARTIFACTSFAIL, false);
	}
}

static void CheatWarpFunc(player_t *player, Cheat_t *cheat)
{
	int episode;
	int map;

	episode = cheat->args[0]-'0';
	map = cheat->args[1]-'0';
	if(M_ValidEpisodeMap(episode, map))
	{
		G_DeferedInitNew(gameskill, episode, map);
		P_SetMessage(player, TXT_CHEATWARP, false);
	}
}

static void CheatChickenFunc(player_t *player, Cheat_t *cheat)
{
	extern boolean P_UndoPlayerChicken(player_t *player);

	if(player->chickenTics)
	{
		if(P_UndoPlayerChicken(player))
		{
			P_SetMessage(player, TXT_CHEATCHICKENOFF, false);
		}
	}
	else if(P_ChickenMorphPlayer(player))
	{
		P_SetMessage(player, TXT_CHEATCHICKENON, false);
	}
}

static void CheatMassacreFunc(player_t *player, Cheat_t *cheat)
{
	P_Massacre();
	P_SetMessage(player, TXT_CHEATMASSACRE, false);
}

static void CheatIDKFAFunc(player_t *player, Cheat_t *cheat)
{
	int i;
	if(player->chickenTics)
	{
		return;
	}
	for(i = 1; i < 8; i++)
	{
		player->weaponowned[i] = false;
	}
	player->pendingweapon = wp_staff;
	P_SetMessage(player, TXT_CHEATIDKFA, true);
}

static void CheatIDDQDFunc(player_t *player, Cheat_t *cheat)
{
	P_DamageMobj(player->plr->mo, NULL, player->plr->mo, 10000);
	P_SetMessage(player, TXT_CHEATIDDQD, true);
}

//===========================================================================
//
// Console Commands
//
//===========================================================================

// This is the multipurpose cheat ccmd.
int CCmdCheat(int argc, char **argv)
{
	unsigned int		i;

	if(argc != 2)
	{
		// Usage information.
		Con_Printf( "Usage: cheat (cheat)\nFor example, 'cheat engage21'.\n");
		return true;
	}
	// Give each of the characters in argument two to the SB event handler.
	for(i=0; i<strlen(argv[1]); i++)
	{
		event_t ev;
		ev.type = ev_keydown;
		ev.data1 = argv[1][i];
		ev.data2 = ev.data3 = 0;
		SB_Responder(&ev);
	}
	return true;
}

static boolean canCheat()
{
	if(IS_NETGAME && !IS_CLIENT && netSvAllowCheats) return true;
	return !(gameskill == sk_nightmare || (IS_NETGAME /*&& !netcheat*/) 
		|| players[consoleplayer].health <= 0);
}

int CCmdCheatGod(int argc, char **argv)
{
	if(IS_NETGAME)
	{
		NetCl_CheatRequest("god");
		return true;
	}
	if(!canCheat()) return false; // Can't cheat!
	CheatGodFunc(players+consoleplayer, NULL);
	return true;
}

int CCmdCheatClip(int argc, char **argv)
{
	if(IS_NETGAME)
	{
		NetCl_CheatRequest("noclip");
		return true;
	}
	if(!canCheat()) return false; // Can't cheat!
	CheatNoClipFunc(players+consoleplayer, NULL);
	return true;
}

int CCmdCheatGive(int argc, char **argv)
{
	int tellUsage = false;
	int target = consoleplayer;

	if(IS_CLIENT)
	{
		char buf[100];
		if(argc != 2) return false;
		sprintf(buf, "give %s", argv[1]);
		NetCl_CheatRequest(buf);
		return true;
	}

	if(!canCheat()) return false; // Can't cheat!

	// Check the arguments.
	if(argc == 3) 
	{
		target = atoi(argv[2]);
		if(target < 0 || target >= MAXPLAYERS 
			|| !players[target].plr->ingame) return false;
	}
	if(argc != 2 && argc != 3)
		tellUsage = true;
	else if(!strnicmp(argv[1], "weapons", 1))
		CheatWeaponsFunc(players + target, NULL);
	else if(!strnicmp(argv[1], "health", 1))
		CheatHealthFunc(players + target, NULL);
	else if(!strnicmp(argv[1], "keys", 1))
		CheatKeysFunc(players + target, NULL);
	else if(!strnicmp(argv[1], "artifacts", 1))
	{
		Cheat_t cheat;
		cheat.args[0] = 'z';
		cheat.args[1] = '0';
		CheatArtifact3Func(players + target, &cheat);
	}
	else 
		tellUsage = true;

	if(tellUsage)
	{
		Con_Printf("Usage: give weapons/health/keys/artifacts\n");
		Con_Printf("The first letter is enough, e.g. 'give h'.\n");
	}
	return true;
}

int CCmdCheatWarp(int argc, char **argv)
{
	Cheat_t cheat;
	int		num;

	if(!canCheat()) return false; // Can't cheat!
	if(argc == 2)
	{
		num = atoi(argv[1]);
		cheat.args[0] = num/10 + '0';
		cheat.args[1] = num%10 + '0';
	}
	else if(argc == 3)
	{
		cheat.args[0] = atoi(argv[1])%10 + '0';
		cheat.args[1] = atoi(argv[2])%10 + '0';
	}
	else 
	{
		Con_Printf( "Usage: warp (num)\n");
		return true;
	}
	// We don't want that keys are repeated while we wait.
	DD_ClearKeyRepeaters();
	CheatWarpFunc(players+consoleplayer, &cheat);
	return true;
}

int CCmdCheatPig(int argc, char **argv)
{
	if(!canCheat()) return false; // Can't cheat!
	CheatChickenFunc(players+consoleplayer, NULL);
	return true;
}

int CCmdCheatMassacre(int argc, char **argv)
{
	if(!canCheat()) return false; // Can't cheat!
	DD_ClearKeyRepeaters();
	CheatMassacreFunc(players+consoleplayer, NULL);
	return true;
}

int CCmdCheatWhere(int argc, char **argv)
{
/*	if(!canCheat()) return false; // Can't cheat!
	CheatDebugFunc(players+consoleplayer, NULL);*/
	return true;
}

int CCmdCheatReveal(int argc, char **argv)
{
	int	option;
	extern int cheating;

	if(!canCheat()) return false; // Can't cheat!
	if(argc != 2)
	{
		Con_Printf( "Usage: reveal (0-3)\n");
		Con_Printf( "0=nothing, 1=show unseen, 2=full map, 3=map+things\n");
		return true;
	}
	// Reset them (for 'nothing'). :-)
	cheating = 0;
	players[consoleplayer].powers[pw_allmap] = false;
	option = atoi(argv[1]);
	if(option < 0 || option > 3) return false;
	if(option == 1)
		players[consoleplayer].powers[pw_allmap] = true;
	else if(option == 2)
		cheating = 1;
	else if(option == 3)
		cheating = 2;
	return true;
}

