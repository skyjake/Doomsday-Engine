
//**************************************************************************
//**
//** sb_bar.c : Heretic 2 : Raven Software, Corp.
//**
//** $RCSfile$
//** $Revision$
//** $Date$
//** $Author$
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include "h2def.h"
#include "p_local.h"
#include "soundst.h"
#include "settings.h"
#include "d_net.h"

#ifdef DEMOCAM
#include "g_demo.h"
#endif

// MACROS ------------------------------------------------------------------

#define CHEAT_ENCRYPT(a) \
	((((a)&1)<<2)+ \
	(((a)&2)>>1)+ \
	(((a)&4)<<5)+ \
	(((a)&8)<<2)+ \
	(((a)&16)>>3)+ \
	(((a)&32)<<1)+ \
	(((a)&64)>>3)+ \
	(((a)&128)>>3))

// Cheat types for nofication.
typedef enum 
{
	CHT_GOD,
	CHT_NOCLIP,
	CHT_WEAPONS,
	CHT_HEALTH,
	CHT_KEYS,
	CHT_ARTIFACTS,
	CHT_PUZZLE
} cheattype_t;

// TYPES -------------------------------------------------------------------

typedef struct Cheat_s
{
	void (*func)(player_t *player, struct Cheat_s *cheat);
	byte *sequence;
	byte *pos;
	int args[2];
	int currentArg;
} Cheat_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void SB_PaletteFlash(boolean forceChange);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

//static void DrawSoundInfo(void);
static void DrINumber(signed int val, int x, int y);
static void DrRedINumber(signed int val, int x, int y);
static void DrBNumber(signed int val, int x, int y);
static void DrawCommonBar(void);
static void DrawMainBar(void);
static void DrawInventoryBar(void);
static void DrawKeyBar(void);
static void DrawWeaponPieces(void);
static void DrawFullScreenStuff(void);
static void DrawAnimatedIcons(void);
static boolean HandleCheats(byte key);
static boolean CheatAddKey(Cheat_t *cheat, byte key, boolean *eat);
static void CheatGodFunc(player_t *player, Cheat_t *cheat);
static void CheatNoClipFunc(player_t *player, Cheat_t *cheat);
static void CheatWeaponsFunc(player_t *player, Cheat_t *cheat);
static void CheatHealthFunc(player_t *player, Cheat_t *cheat);
static void CheatKeysFunc(player_t *player, Cheat_t *cheat);
static void CheatSoundFunc(player_t *player, Cheat_t *cheat);
static void CheatTickerFunc(player_t *player, Cheat_t *cheat);
static void CheatArtifactAllFunc(player_t *player, Cheat_t *cheat);
static void CheatPuzzleFunc(player_t *player, Cheat_t *cheat);
static void CheatWarpFunc(player_t *player, Cheat_t *cheat);
static void CheatPigFunc(player_t *player, Cheat_t *cheat);
static void CheatMassacreFunc(player_t *player, Cheat_t *cheat);
static void CheatIDKFAFunc(player_t *player, Cheat_t *cheat);
static void CheatQuickenFunc1(player_t *player, Cheat_t *cheat);
static void CheatQuickenFunc2(player_t *player, Cheat_t *cheat);
static void CheatQuickenFunc3(player_t *player, Cheat_t *cheat);
static void CheatClassFunc1(player_t *player, Cheat_t *cheat);
static void CheatClassFunc2(player_t *player, Cheat_t *cheat);
static void CheatInitFunc(player_t *player, Cheat_t *cheat);
static void CheatInitFunc(player_t *player, Cheat_t *cheat);
static void CheatVersionFunc(player_t *player, Cheat_t *cheat);
static void CheatDebugFunc(player_t *player, Cheat_t *cheat);
static void CheatScriptFunc1(player_t *player, Cheat_t *cheat);
static void CheatScriptFunc2(player_t *player, Cheat_t *cheat);
static void CheatScriptFunc3(player_t *player, Cheat_t *cheat);
static void CheatRevealFunc(player_t *player, Cheat_t *cheat);
static void CheatTrackFunc1(player_t *player, Cheat_t *cheat);
static void CheatTrackFunc2(player_t *player, Cheat_t *cheat);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

//extern	sfxinfo_t	S_sfx[];

extern byte *memscreen;
extern int ArmorIncrement[NUMCLASSES][NUMARMOR];
extern int AutoArmorSave[NUMCLASSES];

// PUBLIC DATA DECLARATIONS ------------------------------------------------

int DebugSound; // Debug flag for displaying sound info
boolean inventory;
int curpos;
int inv_ptr;
int ArtifactFlash;

/*#ifndef __WATCOMC__
boolean i_CDMusic; // in Watcom, defined in i_ibm
#endif*/

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static byte CheatLookup[256];
static int HealthMarker;
static player_t *CPlayer;
static int SpinFlylump;
static int SpinMinotaurLump;
static int SpinSpeedLump;
static int SpinDefenseLump;

static int FontBNumBase;
static int PlayPalette;

static int PatchNumH2BAR;
static int PatchNumH2TOP;
static int PatchNumLFEDGE;
static int PatchNumRTEDGE;
static int PatchNumKILLS;
static int PatchNumMANAVIAL1;
static int PatchNumMANAVIAL2;
static int PatchNumMANAVIALDIM1;
static int PatchNumMANAVIALDIM2;
static int PatchNumMANADIM1;
static int PatchNumMANADIM2;
static int PatchNumMANABRIGHT1;
static int PatchNumMANABRIGHT2;
static int PatchNumCHAIN;
static int PatchNumSTATBAR;
static int PatchNumKEYBAR;
static int PatchNumLIFEGEM;
static int PatchNumSELECTBOX;
static int PatchNumINumbers[10];
static int PatchNumNEGATIVE;
static int PatchNumSmNumbers[10];
static int PatchNumINVBAR;
static int PatchNumWEAPONSLOT;
static int PatchNumWEAPONFULL;
static int PatchNumPIECE1;
static int PatchNumPIECE2;
static int PatchNumPIECE3;
static int PatchNumINVLFGEM1;
static int PatchNumINVLFGEM2;
static int PatchNumINVRTGEM1;
static int PatchNumINVRTGEM2;


// Toggle god mode
static byte CheatGodSeq[] =
{
	CHEAT_ENCRYPT('s'),
	CHEAT_ENCRYPT('a'),
	CHEAT_ENCRYPT('t'),
	CHEAT_ENCRYPT('a'),
	CHEAT_ENCRYPT('n'),
	0xff
};

// Toggle no clipping mode
static byte CheatNoClipSeq[] =
{
	CHEAT_ENCRYPT('c'),
	CHEAT_ENCRYPT('a'),
	CHEAT_ENCRYPT('s'),
	CHEAT_ENCRYPT('p'),
	CHEAT_ENCRYPT('e'),
	CHEAT_ENCRYPT('r'),
	0xff
};

// Get all weapons and mana
static byte CheatWeaponsSeq[] =
{
	CHEAT_ENCRYPT('n'),
	CHEAT_ENCRYPT('r'),
	CHEAT_ENCRYPT('a'),
	0xff
};

// Get full health
static byte CheatHealthSeq[] =
{
	CHEAT_ENCRYPT('c'),
	CHEAT_ENCRYPT('l'),
	CHEAT_ENCRYPT('u'),
	CHEAT_ENCRYPT('b'),
	CHEAT_ENCRYPT('m'),
	CHEAT_ENCRYPT('e'),
	CHEAT_ENCRYPT('d'),
	0xff
};

// Get all keys
static byte CheatKeysSeq[] =
{
	CHEAT_ENCRYPT('l'),
	CHEAT_ENCRYPT('o'),
	CHEAT_ENCRYPT('c'),
	CHEAT_ENCRYPT('k'),
	CHEAT_ENCRYPT('s'),
	CHEAT_ENCRYPT('m'),
	CHEAT_ENCRYPT('i'),
	CHEAT_ENCRYPT('t'),
	CHEAT_ENCRYPT('h'),
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

// Get all artifacts
static byte CheatArtifactAllSeq[] =
{
	CHEAT_ENCRYPT('i'),
	CHEAT_ENCRYPT('n'),
	CHEAT_ENCRYPT('d'),
	CHEAT_ENCRYPT('i'),
	CHEAT_ENCRYPT('a'),
	CHEAT_ENCRYPT('n'),
	CHEAT_ENCRYPT('a'),
	0xff, 0
};

// Get all puzzle pieces
static byte CheatPuzzleSeq[] =
{
	CHEAT_ENCRYPT('s'),
	CHEAT_ENCRYPT('h'),
	CHEAT_ENCRYPT('e'),
	CHEAT_ENCRYPT('r'),
	CHEAT_ENCRYPT('l'),
	CHEAT_ENCRYPT('o'),
	CHEAT_ENCRYPT('c'),
	CHEAT_ENCRYPT('k'),
	0xff, 0
};

// Warp to new level
static byte CheatWarpSeq[] =
{
	CHEAT_ENCRYPT('v'),
	CHEAT_ENCRYPT('i'),
	CHEAT_ENCRYPT('s'),
	CHEAT_ENCRYPT('i'),
	CHEAT_ENCRYPT('t'),
	0, 0, 0xff, 0
};

// Become a pig
static byte CheatPigSeq[] =
{
	CHEAT_ENCRYPT('d'),
	CHEAT_ENCRYPT('e'),
	CHEAT_ENCRYPT('l'),
	CHEAT_ENCRYPT('i'),
	CHEAT_ENCRYPT('v'),
	CHEAT_ENCRYPT('e'),
	CHEAT_ENCRYPT('r'),
	CHEAT_ENCRYPT('a'),
	CHEAT_ENCRYPT('n'),
	CHEAT_ENCRYPT('c'),
	CHEAT_ENCRYPT('e'),
	0xff, 0
};

// Kill all monsters
static byte CheatMassacreSeq[] =
{
	CHEAT_ENCRYPT('b'),
	CHEAT_ENCRYPT('u'),
	CHEAT_ENCRYPT('t'),
	CHEAT_ENCRYPT('c'),
	CHEAT_ENCRYPT('h'),
	CHEAT_ENCRYPT('e'),
	CHEAT_ENCRYPT('r'),
	0xff, 0
};

static byte CheatIDKFASeq[] =
{
	CHEAT_ENCRYPT('c'),
	CHEAT_ENCRYPT('o'),
	CHEAT_ENCRYPT('n'),
	CHEAT_ENCRYPT('a'),
	CHEAT_ENCRYPT('n'),
	0xff, 0
};

static byte CheatQuickenSeq1[] =
{
	CHEAT_ENCRYPT('m'),
	CHEAT_ENCRYPT('a'),
	CHEAT_ENCRYPT('r'),
	CHEAT_ENCRYPT('t'),
	CHEAT_ENCRYPT('e'),
	CHEAT_ENCRYPT('k'),
	0xff, 0
};

static byte CheatQuickenSeq2[] =
{
	CHEAT_ENCRYPT('m'),
	CHEAT_ENCRYPT('a'),
	CHEAT_ENCRYPT('r'),
	CHEAT_ENCRYPT('t'),
	CHEAT_ENCRYPT('e'),
	CHEAT_ENCRYPT('k'),
	CHEAT_ENCRYPT('m'),
	CHEAT_ENCRYPT('a'),
	CHEAT_ENCRYPT('r'),
	CHEAT_ENCRYPT('t'),
	CHEAT_ENCRYPT('e'),
	CHEAT_ENCRYPT('k'),
	0xff, 0
};

static byte CheatQuickenSeq3[] =
{
	CHEAT_ENCRYPT('m'),
	CHEAT_ENCRYPT('a'),
	CHEAT_ENCRYPT('r'),
	CHEAT_ENCRYPT('t'),
	CHEAT_ENCRYPT('e'),
	CHEAT_ENCRYPT('k'),
	CHEAT_ENCRYPT('m'),
	CHEAT_ENCRYPT('a'),
	CHEAT_ENCRYPT('r'),
	CHEAT_ENCRYPT('t'),
	CHEAT_ENCRYPT('e'),
	CHEAT_ENCRYPT('k'),
	CHEAT_ENCRYPT('m'),
	CHEAT_ENCRYPT('a'),
	CHEAT_ENCRYPT('r'),
	CHEAT_ENCRYPT('t'),
	CHEAT_ENCRYPT('e'),
	CHEAT_ENCRYPT('k'),
	0xff, 0
};

// New class
static byte CheatClass1Seq[] = 
{
	CHEAT_ENCRYPT('s'),
	CHEAT_ENCRYPT('h'),
	CHEAT_ENCRYPT('a'),
	CHEAT_ENCRYPT('d'),
	CHEAT_ENCRYPT('o'),
	CHEAT_ENCRYPT('w'),
	CHEAT_ENCRYPT('c'),
	CHEAT_ENCRYPT('a'),
	CHEAT_ENCRYPT('s'),
	CHEAT_ENCRYPT('t'),
	CHEAT_ENCRYPT('e'),
	CHEAT_ENCRYPT('r'),
	0xff, 0
};

static byte CheatClass2Seq[] = 
{
	CHEAT_ENCRYPT('s'),
	CHEAT_ENCRYPT('h'),
	CHEAT_ENCRYPT('a'),
	CHEAT_ENCRYPT('d'),
	CHEAT_ENCRYPT('o'),
	CHEAT_ENCRYPT('w'),
	CHEAT_ENCRYPT('c'),
	CHEAT_ENCRYPT('a'),
	CHEAT_ENCRYPT('s'),
	CHEAT_ENCRYPT('t'),
	CHEAT_ENCRYPT('e'),
	CHEAT_ENCRYPT('r'),
	0, 0xff, 0
};

static byte CheatInitSeq[] =
{
	CHEAT_ENCRYPT('i'),
	CHEAT_ENCRYPT('n'),
	CHEAT_ENCRYPT('i'),
	CHEAT_ENCRYPT('t'),
	0xff, 0
};

static byte CheatVersionSeq[] =
{
	CHEAT_ENCRYPT('m'),
	CHEAT_ENCRYPT('r'),
	CHEAT_ENCRYPT('j'),
	CHEAT_ENCRYPT('o'),
	CHEAT_ENCRYPT('n'),
	CHEAT_ENCRYPT('e'),
	CHEAT_ENCRYPT('s'),
	0xff, 0
};

static byte CheatDebugSeq[] =
{
	CHEAT_ENCRYPT('w'),
	CHEAT_ENCRYPT('h'),
	CHEAT_ENCRYPT('e'),
	CHEAT_ENCRYPT('r'),
	CHEAT_ENCRYPT('e'),
	0xff, 0
};

static byte CheatScriptSeq1[] =
{
	CHEAT_ENCRYPT('p'),
	CHEAT_ENCRYPT('u'),
	CHEAT_ENCRYPT('k'),
	CHEAT_ENCRYPT('e'),
	0xff, 0
};

static byte CheatScriptSeq2[] =
{
	CHEAT_ENCRYPT('p'),
	CHEAT_ENCRYPT('u'),
	CHEAT_ENCRYPT('k'),
	CHEAT_ENCRYPT('e'),
	0, 0xff, 0
};

static byte CheatScriptSeq3[] =
{
	CHEAT_ENCRYPT('p'),
	CHEAT_ENCRYPT('u'),
	CHEAT_ENCRYPT('k'),
	CHEAT_ENCRYPT('e'),
	0, 0, 0xff,
};

static byte CheatRevealSeq[] =
{
	CHEAT_ENCRYPT('m'),
	CHEAT_ENCRYPT('a'),
	CHEAT_ENCRYPT('p'),
	CHEAT_ENCRYPT('s'),
	CHEAT_ENCRYPT('c'),
	CHEAT_ENCRYPT('o'),
	0xff, 0
};

static byte CheatTrackSeq1[] = 
{
	//CHEAT_ENCRYPT('`'),
	CHEAT_ENCRYPT('c'),
	CHEAT_ENCRYPT('d'),
	CHEAT_ENCRYPT('t'),
	0xff, 0
};

static byte CheatTrackSeq2[] = 
{
	//CHEAT_ENCRYPT('`'),
	CHEAT_ENCRYPT('c'),
	CHEAT_ENCRYPT('d'),
	CHEAT_ENCRYPT('t'),
	0, 0, 0xff, 0
};

static Cheat_t Cheats[] =
{
	{ CheatTrackFunc1, CheatTrackSeq1, NULL, 0, 0, 0 },
	{ CheatTrackFunc2, CheatTrackSeq2, NULL, 0, 0, 0 },
	{ CheatGodFunc, CheatGodSeq, NULL, 0, 0, 0 },
	{ CheatNoClipFunc, CheatNoClipSeq, NULL, 0, 0, 0 },
	{ CheatWeaponsFunc, CheatWeaponsSeq, NULL, 0, 0, 0 },
	{ CheatHealthFunc, CheatHealthSeq, NULL, 0, 0, 0 },
	{ CheatKeysFunc, CheatKeysSeq, NULL, 0, 0, 0 },
	{ CheatSoundFunc, CheatSoundSeq, NULL, 0, 0, 0 },
	{ CheatTickerFunc, CheatTickerSeq, NULL, 0, 0, 0 },
	{ CheatArtifactAllFunc, CheatArtifactAllSeq, NULL, 0, 0, 0 },
	{ CheatPuzzleFunc, CheatPuzzleSeq, NULL, 0, 0, 0 },
	{ CheatWarpFunc, CheatWarpSeq, NULL, 0, 0, 0 },
	{ CheatPigFunc, CheatPigSeq, NULL, 0, 0, 0 },
	{ CheatMassacreFunc, CheatMassacreSeq, NULL, 0, 0, 0 },
	{ CheatIDKFAFunc, CheatIDKFASeq, NULL, 0, 0, 0 },
	{ CheatQuickenFunc1, CheatQuickenSeq1, NULL, 0, 0, 0 },
	{ CheatQuickenFunc2, CheatQuickenSeq2, NULL, 0, 0, 0 },
	{ CheatQuickenFunc3, CheatQuickenSeq3, NULL, 0, 0, 0 },
	{ CheatClassFunc1, CheatClass1Seq, NULL, 0, 0, 0 },
	{ CheatClassFunc2, CheatClass2Seq, NULL, 0, 0, 0 },
	{ CheatInitFunc, CheatInitSeq, NULL, 0, 0, 0 },
	{ CheatVersionFunc, CheatVersionSeq, NULL, 0, 0, 0 },
	{ CheatDebugFunc, CheatDebugSeq, NULL, 0, 0, 0 },
	{ CheatScriptFunc1, CheatScriptSeq1, NULL, 0, 0, 0 },
	{ CheatScriptFunc2, CheatScriptSeq2, NULL, 0, 0, 0 },
	{ CheatScriptFunc3, CheatScriptSeq3, NULL, 0, 0, 0 },
	{ CheatRevealFunc, CheatRevealSeq, NULL, 0, 0, 0 },
	{ NULL, NULL, NULL, 0, 0, 0 } // Terminator
};

// CODE --------------------------------------------------------------------

//==========================================================================
//
// SB_Init
//
//==========================================================================

void SB_Init(void)
{
	int i;
	int startLump;

	PatchNumH2BAR = W_GetNumForName("H2BAR");
	PatchNumH2TOP = W_GetNumForName("H2TOP");
	PatchNumINVBAR = W_GetNumForName("INVBAR");
	PatchNumLFEDGE	= W_GetNumForName("LFEDGE");
	PatchNumRTEDGE	= W_GetNumForName("RTEDGE");
	PatchNumSTATBAR = W_GetNumForName("STATBAR");
	PatchNumKEYBAR = W_GetNumForName("KEYBAR"); 
	PatchNumSELECTBOX = W_GetNumForName("SELECTBOX");
	PatchNumMANAVIAL1 = W_GetNumForName("MANAVL1"); 
	PatchNumMANAVIAL2 = W_GetNumForName("MANAVL2");
	PatchNumMANAVIALDIM1 = W_GetNumForName("MANAVL1D");
	PatchNumMANAVIALDIM2 = W_GetNumForName("MANAVL2D");
	PatchNumMANADIM1 = W_GetNumForName("MANADIM1");
	PatchNumMANADIM2 = W_GetNumForName("MANADIM2");
	PatchNumMANABRIGHT1 = W_GetNumForName("MANABRT1");
	PatchNumMANABRIGHT2 = W_GetNumForName("MANABRT2");
	PatchNumINVLFGEM1 = W_GetNumForName("invgeml1");
	PatchNumINVLFGEM2 = W_GetNumForName("invgeml2");
	PatchNumINVRTGEM1 = W_GetNumForName("invgemr1");
	PatchNumINVRTGEM2 = W_GetNumForName("invgemr2");

	startLump = W_GetNumForName("IN0");
	for(i = 0; i < 10; i++)
	{
		PatchNumINumbers[i] = startLump + i;
	}
	PatchNumNEGATIVE = W_GetNumForName("NEGNUM");
	FontBNumBase = W_GetNumForName("FONTB16");
	startLump = W_GetNumForName("SMALLIN0");
	for(i = 0; i < 10; i++)
	{
		PatchNumSmNumbers[i] = startLump + i;
	}
	PlayPalette = W_GetNumForName("PLAYPAL");
	SpinFlylump = W_GetNumForName("SPFLY0");
	SpinMinotaurLump = W_GetNumForName("SPMINO0");
	SpinSpeedLump = W_GetNumForName("SPBOOT0");
	SpinDefenseLump = W_GetNumForName("SPSHLD0");

	for(i = 0; i < 256; i++)
	{
		CheatLookup[i] = CHEAT_ENCRYPT(i);
	}

	//if(deathmatch)
	//{
	PatchNumKILLS = W_GetNumForName("KILLS");
	//}
	SB_SetClassData();
}

//==========================================================================
//
// SB_SetClassData
//
//==========================================================================

void SB_SetClassData(void)
{
	int class;

	class = cfg.PlayerClass[consoleplayer]; // original player class (not pig)
	PatchNumWEAPONSLOT = W_GetNumForName("wpslot0")+class;
	PatchNumWEAPONFULL = W_GetNumForName("wpfull0")+class;
	PatchNumPIECE1 = W_GetNumForName("wpiecef1")+class;
	PatchNumPIECE2 = W_GetNumForName("wpiecef2")+class;
	PatchNumPIECE3 = W_GetNumForName("wpiecef3")+class;
	PatchNumCHAIN = W_GetNumForName("chain")+class;
	if(!netgame)
	{ // single player game uses red life gem (the second gem)
		PatchNumLIFEGEM = W_GetNumForName("lifegem")+MAXPLAYERS*class+1;
	}
	else
	{
		PatchNumLIFEGEM = W_GetNumForName("lifegem")+MAXPLAYERS*class+consoleplayer;
	}
	SB_state = -1;
	GL_Update(DDUF_FULLSCREEN);
}

//==========================================================================
//
// SB_Ticker
//
//==========================================================================

void SB_Ticker(void)
{
	int delta;
	int curHealth;

	if(!players[consoleplayer].plr->mo) return;

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
		else if(delta > 6)
		{
			delta = 6;
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
		else if(delta > 6)
		{
			delta = 6;
		}
		HealthMarker += delta;
	}
}

//==========================================================================
//
// DrINumber
//
// Draws a three digit number.
//
//==========================================================================

static void DrINumber(signed int val, int x, int y)
{
	int oldval;

	// Make sure it's a three digit number.
	if(val < -999) val = -999;
	if(val > 999) val = 999;

	oldval = val;
	if(val < 0)
	{
		val = -val;
		if(val > 99)
		{
			val = 99;
		}
		if(val > 9)
		{
			GL_DrawPatch(x+8, y, PatchNumINumbers[val/10]);
			GL_DrawPatch(x, y, PatchNumNEGATIVE);
		}
		else
		{
			GL_DrawPatch(x+8, y, PatchNumNEGATIVE);
		}
		val = val%10;
		GL_DrawPatch(x+16, y, PatchNumINumbers[val]);
		return;
	}
	if(val > 99)
	{
		GL_DrawPatch(x, y, PatchNumINumbers[val/100]);
	}
	val = val%100;
	if(val > 9 || oldval > 99)
	{
		GL_DrawPatch(x+8, y, PatchNumINumbers[val/10]);
	}
	val = val%10;
	GL_DrawPatch(x+16, y, PatchNumINumbers[val]);
}

//==========================================================================
//
// DrRedINumber
//
// Draws a three digit number using the red font
//
//==========================================================================

static void DrRedINumber(signed int val, int x, int y)
{
	int oldval;

	// Make sure it's a three digit number.
	if(val < -999) val = -999;
	if(val > 999) val = 999;

	oldval = val;
	if(val < 0)
	{
		val = 0;
	}
	if(val > 99)
	{
		GL_DrawPatch(x, y, W_GetNumForName("inred0")+val/100);
	}
	val = val%100;
	if(val > 9 || oldval > 99)
	{
		GL_DrawPatch(x+8, y, W_GetNumForName("inred0")+val/10);
	}
	val = val%10;
	GL_DrawPatch(x+16, y, W_GetNumForName("inred0")+val);
}

//==========================================================================
//
// DrBNumber
//
// Draws a three digit number using FontB
//
//==========================================================================

static void DrBNumber(signed int val, int x, int y)
{
	patch_t *patch;
	int xpos;
	int oldval;

	// Limit to three digits.
	if(val > 999) val = 999;
	if(val < -999) val = -999;

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

//==========================================================================
//
// DrSmallNumber
//
// Draws a small two digit number.
//
//==========================================================================

static void DrSmallNumber(int val, int x, int y)
{
	if(val <= 0)
	{
		return;
	}
	if(val > 999)
	{
		val %= 1000;
	}
	if(val > 99)
	{
		GL_DrawPatch(x, y, PatchNumSmNumbers[val/100]);
		GL_DrawPatch(x+4, y, PatchNumSmNumbers[(val%100)/10]);
	}
	else if(val > 9)
	{
		GL_DrawPatch(x+4, y, PatchNumSmNumbers[val/10]);
	}
	val %= 10;
	GL_DrawPatch(x+8, y, PatchNumSmNumbers[val]);
}

//==========================================================================
//
// DrawSoundInfo
//
// Displays sound debugging information.
//
//==========================================================================
#if 0
static void DrawSoundInfo(void)
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
		if(c->mo == NULL)
		{ // Channel is unused
			MN_DrTextA("------", xPos[0], y);
			continue;
		}
		sprintf(text, "%s", c->name);
		//M_ForceUppercase(text);
		strupr(text);
		MN_DrTextA(text, xPos[x++], y);
		sprintf(text, "%d", c->mo->type);
		MN_DrTextA(text, xPos[x++], y);
		sprintf(text, "%d", c->mo->x>>FRACBITS);
		MN_DrTextA(text, xPos[x++], y);
		sprintf(text, "%d", c->mo->y>>FRACBITS);
		MN_DrTextA(text, xPos[x++], y);
		sprintf(text, "%d", c->id);
		MN_DrTextA(text, xPos[x++], y);
		sprintf(text, "%d", S_sfx[c->id].usefulness);
		MN_DrTextA(text, xPos[x++], y);
		sprintf(text, "%d", c->distance);
		//MN_DrTextA(text, xPos[x++], y);
	}
	GL_Update(DDUF_FULLSCREEN | DDUF_BORDER);
}
#endif

//==========================================================================
//
// SB_Drawer
//
//==========================================================================

char patcharti[][10] =
{
	{ "ARTIBOX" },    	// none
	{ "ARTIINVU" },   	// invulnerability
	{ "ARTIPTN2" },   	// health
	{ "ARTISPHL" },   	// superhealth
	{ "ARTIHRAD" },		// healing radius
	{ "ARTISUMN" },   	// summon maulator
	{ "ARTITRCH" },   	// torch
	{ "ARTIPORK" },   	// egg
	{ "ARTISOAR" },   	// fly
	{ "ARTIBLST" },		// blast radius
	{ "ARTIPSBG" },		// poison bag
	{ "ARTITELO" },		// teleport other
	{ "ARTISPED" },  	// speed
	{ "ARTIBMAN" },		// boost mana
	{ "ARTIBRAC" },		// boost armor
	{ "ARTIATLP" },   	// teleport
	{ "ARTISKLL" },		// arti_puzzskull
	{ "ARTIBGEM" },		// arti_puzzgembig
	{ "ARTIGEMR" },		// arti_puzzgemred
	{ "ARTIGEMG" },		// arti_puzzgemgreen1
	{ "ARTIGMG2" },		// arti_puzzgemgreen2
	{ "ARTIGEMB" },		// arti_puzzgemblue1
	{ "ARTIGMB2" },		// arti_puzzgemblue2
	{ "ARTIBOK1" },		// arti_puzzbook1
	{ "ARTIBOK2" },		// arti_puzzbook2
	{ "ARTISKL2" },		// arti_puzzskull2
	{ "ARTIFWEP" },		// arti_puzzfweapon
	{ "ARTICWEP" },		// arti_puzzcweapon
	{ "ARTIMWEP" },		// arti_puzzmweapon
	{ "ARTIGEAR" },		// arti_puzzgear1
	{ "ARTIGER2" },		// arti_puzzgear2
	{ "ARTIGER3" },		// arti_puzzgear3
	{ "ARTIGER4" },		// arti_puzzgear4
};

int SB_state = -1;

static int oldarti = 0;
static int oldartiCount = 0;

/*
static int oldarti = 0;
static int oldartiCount = 0;
static int oldfrags = -9999;
static int oldmana1 = -1;
static int oldmana2 = -1;
static int oldarmor = -1;
static int oldhealth = -1;
static int oldlife = -1;
static int oldpieces = -1;
static int oldweapon = -1;
static int oldkeys = -1;
*/

extern boolean automapactive;

void SB_Drawer(void)
{
	CPlayer = &players[consoleplayer];
	if(Get(DD_VIEWWINDOW_HEIGHT) == SCREENHEIGHT && !automapactive
#ifdef DEMOCAM 
		&& (demoplayback && democam.mode)
#endif
		)
	{
		DrawFullScreenStuff();
	}
	else
	{
		float fscale = cfg.sbarscale/20.0f;

		// Setup special status bar matrix.
		if(cfg.sbarscale != 20)
		{
			// Update borders around status bar (could flicker otherwise).
			GL_Update(DDUF_BORDER);

			gl.MatrixMode(DGL_MODELVIEW);
			gl.PushMatrix();
			gl.Translatef(160 - 320*fscale/2, 200*(1-fscale), 0);
			gl.Scalef(fscale, fscale, 1);
		}
		
		GL_DrawPatch(0, 134, PatchNumH2BAR);

		DrawCommonBar();
		if(!inventory)
		{
			// Main interface
			if(!automapactive)
			{
				GL_DrawPatch(38, 162, PatchNumSTATBAR);
			}
			else
			{
				GL_DrawPatch(38, 162, PatchNumKEYBAR);
			}
			if(!automapactive)
			{
				DrawMainBar();
			}
			else
			{
				DrawKeyBar();
			}
		}
		else
		{
			DrawInventoryBar();
		}
		// Restore the old modelview matrix.
		if(cfg.sbarscale != 20)
		{
			gl.MatrixMode(DGL_MODELVIEW);
			gl.PopMatrix();
		}
	}
	SB_PaletteFlash(false);
	DrawAnimatedIcons();
}

//==========================================================================
//
// DrawAnimatedIcons
//
//==========================================================================

static void DrawAnimatedIcons(void)
{
	int leftoff = 0;
	int frame;
	static boolean hitCenterFrame;
//	extern int screenblocks;

	// If the fullscreen mana is drawn, we need to move the icons on the left
	// a bit to the right.
	if(cfg.showFullscreenMana==1 && cfg.screenblocks>10) leftoff = 42;

	// Wings of wrath
	if(CPlayer->powers[pw_flight])
	{
		if(CPlayer->powers[pw_flight] > BLINKTHRESHOLD
			|| !(CPlayer->powers[pw_flight]&16))
		{
			frame = (leveltime/3)&15;
			if(CPlayer->plr->mo->flags2&MF2_FLY)
			{
				if(hitCenterFrame && (frame != 15 && frame != 0))
				{
					GL_DrawPatch(20+leftoff, 19, SpinFlylump+15);
				}
				else
				{
					GL_DrawPatch(20+leftoff, 19, SpinFlylump+frame);
					hitCenterFrame = false;
				}
			}
			else
			{
				if(!hitCenterFrame && (frame != 15 && frame != 0))
				{
					GL_DrawPatch(20+leftoff, 19, SpinFlylump+frame);
					hitCenterFrame = false;
				}
				else
				{
					GL_DrawPatch(20+leftoff, 19, SpinFlylump+15);
					hitCenterFrame = true;
				}
			}
		}
		GL_Update(DDUF_TOP | DDUF_MESSAGES);
	}

	// Speed Boots
	if(CPlayer->powers[pw_speed])
	{
		if(CPlayer->powers[pw_speed] > BLINKTHRESHOLD
			|| !(CPlayer->powers[pw_speed]&16))
		{
			frame = (leveltime/3)&15;
			GL_DrawPatch(60+leftoff, 19, SpinSpeedLump+frame);
		}
		GL_Update(DDUF_TOP | DDUF_MESSAGES);
	}

	// Defensive power
	if(CPlayer->powers[pw_invulnerability])
	{
		if(CPlayer->powers[pw_invulnerability] > BLINKTHRESHOLD
			|| !(CPlayer->powers[pw_invulnerability]&16))
		{
			frame = (leveltime/3)&15;
			GL_DrawPatch(260, 19, SpinDefenseLump+frame);
		}
		GL_Update(DDUF_TOP | DDUF_MESSAGES);
	}

	// Minotaur Active
	if(CPlayer->powers[pw_minotaur])
	{
		if(CPlayer->powers[pw_minotaur] > BLINKTHRESHOLD
			|| !(CPlayer->powers[pw_minotaur]&16))
		{
			frame = (leveltime/3)&15;
			GL_DrawPatch(300, 19, SpinMinotaurLump+frame);
		}
		GL_Update(DDUF_TOP | DDUF_MESSAGES);
	}
}

//==========================================================================
//
// SB_PaletteFlash
//
// Sets the new palette based upon the current values of
// consoleplayer->damagecount and consoleplayer->bonuscount.
//
//==========================================================================

void SB_PaletteFlash(boolean forceChange)
{
	static int sb_palette = 0;
	int palette;

	if(forceChange)
	{ 
		sb_palette = -1;
	}
	if(gamestate == GS_LEVEL)
	{
		CPlayer = &players[consoleplayer];
		if(CPlayer->poisoncount)
		{
			palette = 0;
			palette = (CPlayer->poisoncount+7)>>3;
			if(palette >= NUMPOISONPALS)
			{
				palette = NUMPOISONPALS-1;
			}
			palette += STARTPOISONPALS;
		}
		else if(CPlayer->damagecount)
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
		else if(CPlayer->plr->mo->flags2&MF2_ICEDAMAGE)
		{ // Frozen player
			palette = STARTICEPAL;
		}
		else
		{
			palette = 0;
		}
	}
	else
	{
		palette = 0;
	}
	if(palette != sb_palette)
	{
		sb_palette = palette;
		// $democam
		CPlayer->plr->filter = H2_GetFilterColor(palette); 
	}
}

//==========================================================================
//
// DrawCommonBar
//
//==========================================================================

void DrawCommonBar(void)
{
	int healthPos;

	GL_DrawPatch(0, 134, PatchNumH2TOP);

	healthPos = HealthMarker;
	if(healthPos < 0)
	{
		healthPos = 0;
	}
	if(healthPos > 100)
	{
		healthPos = 100;
	}
	GL_DrawPatch(28+(((healthPos*196)/100)%9), 193, PatchNumCHAIN);
	GL_DrawPatch(7+((healthPos*11)/5), 193, PatchNumLIFEGEM);
	GL_DrawPatch(0, 193, PatchNumLFEDGE);
	GL_DrawPatch(277, 193, PatchNumRTEDGE);
}

//==========================================================================
//
// DrawMainBar
//
//==========================================================================

void DrawMainBar(void)
{
	int i;
	int temp;
	int manaPatchNum1, manaPatchNum2, manaVialPatchNum1, manaVialPatchNum2;

	manaPatchNum1 = manaPatchNum2 = manaVialPatchNum1 = manaVialPatchNum2 = -1;

	// Ready artifact
	if(ArtifactFlash)
	{
		GL_DrawPatch(148, 164, W_GetNumForName("useartia")+ArtifactFlash-1);
		ArtifactFlash--;
		oldarti = -1; // so that the correct artifact fills in after the flash
		GL_Update(DDUF_STATBAR);
	}
	else if(oldarti != CPlayer->readyArtifact
		|| oldartiCount != CPlayer->inventory[inv_ptr].count)
	{
		if(CPlayer->readyArtifact > 0)
		{
			GL_DrawPatch(143, 163, W_GetNumForName(patcharti[CPlayer->readyArtifact]));
			if(CPlayer->inventory[inv_ptr].count > 1)
			{
				DrSmallNumber(CPlayer->inventory[inv_ptr].count, 162, 184);
			}
		}
	}

	// Frags
	if(deathmatch)
	{
		temp = 0;
		for(i = 0; i < MAXPLAYERS; i++)
		{
			temp += CPlayer->frags[i];
		}
		GL_DrawPatch(38, 162, PatchNumKILLS);
		DrINumber(temp, 40, 176);
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
		if(temp >= 25)
		{
			DrINumber(temp, 40, 176);
		}
		else
		{
			DrRedINumber(temp, 40, 176);
		}
	}
	// Mana
	temp = CPlayer->mana[0];
	DrSmallNumber(temp, 79, 181);
	if(temp == 0)
	{ // Draw Dim Mana icon
		manaPatchNum1 = PatchNumMANADIM1;
	}
	GL_Update(DDUF_STATBAR);
	temp = CPlayer->mana[1];
	DrSmallNumber(temp, 111, 181);
	if(temp == 0)
	{ // Draw Dim Mana icon
		manaPatchNum2 = PatchNumMANADIM2;
	}		
	GL_Update(DDUF_STATBAR);
	// Update mana graphics based upon mana count/weapon type */
	if(CPlayer->readyweapon == WP_FIRST)
	{
		manaPatchNum1 = PatchNumMANADIM1;
		manaPatchNum2 = PatchNumMANADIM2;
		manaVialPatchNum1 = PatchNumMANAVIALDIM1;
		manaVialPatchNum2 = PatchNumMANAVIALDIM2;
	}
	else if(CPlayer->readyweapon == WP_SECOND)
	{
		// If there is mana for this weapon, make it bright!
		if(manaPatchNum1 == -1)
		{
			manaPatchNum1 = PatchNumMANABRIGHT1;
		}
		manaVialPatchNum1 = PatchNumMANAVIAL1;
		manaPatchNum2 = PatchNumMANADIM2;
		manaVialPatchNum2 = PatchNumMANAVIALDIM2;
	}
	else if(CPlayer->readyweapon == WP_THIRD)
	{
		manaPatchNum1 = PatchNumMANADIM1;
		manaVialPatchNum1 = PatchNumMANAVIALDIM1;
		// If there is mana for this weapon, make it bright!
		if(manaPatchNum2 == -1)
		{
			manaPatchNum2 = PatchNumMANABRIGHT2;
		}
		manaVialPatchNum2 = PatchNumMANAVIAL2;
	}
	else
	{
		manaVialPatchNum1 = PatchNumMANAVIAL1;
		manaVialPatchNum2 = PatchNumMANAVIAL2;
		// If there is mana for this weapon, make it bright!
		if(manaPatchNum1 == -1)
		{
			manaPatchNum1 = PatchNumMANABRIGHT1;
		}
		if(manaPatchNum2 == -1)
		{
			manaPatchNum2 = PatchNumMANABRIGHT2;
		}
	}
	GL_DrawPatch(77, 164, manaPatchNum1);
	GL_DrawPatch(110, 164, manaPatchNum2);
	GL_DrawPatch(94, 164, manaVialPatchNum1);
	GL_DrawPatch(102, 164, manaVialPatchNum2);
	
	GL_SetNoTexture();
	GL_DrawRect(95, 165, 3, 22-(22*CPlayer->mana[0])/MAX_MANA, 0,0,0,1);
	GL_DrawRect(103, 165, 3, 22-(22*CPlayer->mana[1])/MAX_MANA, 0,0,0,1);
	
	GL_Update(DDUF_STATBAR);
	
	// Armor
	temp = AutoArmorSave[CPlayer->class]
		+CPlayer->armorpoints[ARMOR_ARMOR]+CPlayer->armorpoints[ARMOR_SHIELD]
		+CPlayer->armorpoints[ARMOR_HELMET]+CPlayer->armorpoints[ARMOR_AMULET];
	DrINumber(FixedDiv(temp, 5*FRACUNIT)>>FRACBITS, 250, 176);
	
	DrawWeaponPieces();
}

//==========================================================================
//
// DrawInventoryBar
//
//==========================================================================

void DrawInventoryBar(void)
{
	int i;
	int x;

	x = inv_ptr-curpos;
	GL_DrawPatch(38, 162, PatchNumINVBAR);
	for(i = 0; i < 7; i++)
	{
		if(CPlayer->inventorySlotNum > x+i
			&& CPlayer->inventory[x+i].type != arti_none)
		{
			GL_DrawPatch(50+i*31, 163, W_GetNumForName(
				patcharti[CPlayer->inventory[x+i].type]));
			if(CPlayer->inventory[x+i].count > 1)
			{
				DrSmallNumber(CPlayer->inventory[x+i].count, 68+i*31, 185);
			}
		}
	}
	GL_DrawPatch(50+curpos*31, 163, PatchNumSELECTBOX);
	if(x != 0)
	{
		GL_DrawPatch(42, 163, !(leveltime&4) ? PatchNumINVLFGEM1 :
			PatchNumINVLFGEM2);
	}
	if(CPlayer->inventorySlotNum-x > 7)
	{
		GL_DrawPatch(269, 163, !(leveltime&4) ? PatchNumINVRTGEM1 :
			PatchNumINVRTGEM2);
	}
}

//==========================================================================
//
// DrawKeyBar
//
//==========================================================================

void DrawKeyBar(void)
{
	int i;
	int xPosition;
	int temp;

	xPosition = 46;
	for(i = 0; i < NUMKEYS && xPosition <= 126; i++)
	{
		if(CPlayer->keys&(1<<i))
		{
			GL_DrawPatch(xPosition, 163, W_GetNumForName("keyslot1")+i);
			xPosition += 20;
		}
	}
	temp = AutoArmorSave[CPlayer->class]
		+CPlayer->armorpoints[ARMOR_ARMOR]+CPlayer->armorpoints[ARMOR_SHIELD]
		+CPlayer->armorpoints[ARMOR_HELMET]+CPlayer->armorpoints[ARMOR_AMULET];
	for(i = 0; i < NUMARMOR; i++)
	{
		if(!CPlayer->armorpoints[i])
		{
			continue;
		}
		if(CPlayer->armorpoints[i] <= 
			(ArmorIncrement[CPlayer->class][i]>>2))
		{
			GL_DrawFuzzPatch(150+31*i, 164, 
				W_GetNumForName("armslot1")+i);
		}
		else if(CPlayer->armorpoints[i] <= 
			(ArmorIncrement[CPlayer->class][i]>>1))
		{
			GL_DrawAltFuzzPatch(150+31*i, 164, 
				W_GetNumForName("armslot1")+i);
		}
		else
		{
			GL_DrawPatch(150+31*i, 164, W_GetNumForName("armslot1")+i);
		}
	}
}

//==========================================================================
//
// DrawWeaponPieces
//
//==========================================================================

static int PieceX[NUMCLASSES][3] = 
{
	{ 190, 225, 234 },
	{ 190, 212, 225 },
	{ 190, 205, 224 },
	{ 0, 0, 0 }			// Pig is never used
};

static void DrawWeaponPieces(void)
{
	if(CPlayer->pieces == 7)
	{
		GL_DrawPatch(190, 162, PatchNumWEAPONFULL);
		return;
	}
	GL_DrawPatch(190, 162, PatchNumWEAPONSLOT);
	if(CPlayer->pieces&WPIECE1)
	{
		GL_DrawPatch(PieceX[cfg.PlayerClass[consoleplayer]][0], 162, 
			PatchNumPIECE1);
	}
	if(CPlayer->pieces&WPIECE2)
	{
		GL_DrawPatch(PieceX[cfg.PlayerClass[consoleplayer]][1], 162, 
			PatchNumPIECE2);
	}
	if(CPlayer->pieces&WPIECE3)
	{
		GL_DrawPatch(PieceX[cfg.PlayerClass[consoleplayer]][2], 162, 
			PatchNumPIECE3);
	}
}

//==========================================================================
//
// DrawFullScreenStuff
//
//==========================================================================

void DrawFullScreenStuff(void)
{
	int i;
	int x;
	int temp;
	
#ifdef DEMOCAM
	if(demoplayback && democam.mode) return;
#endif

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
		int dim[2] = { PatchNumMANADIM1, PatchNumMANADIM2 };
		int bright[2] = { PatchNumMANABRIGHT1, PatchNumMANABRIGHT2 };
		int patches[2] = { 0, 0 };
		int ypos = cfg.showFullscreenMana==2? 152 : 2;
		for(i=0; i<2; i++) if(CPlayer->mana[i] == 0) patches[i] = dim[i];		
		if(CPlayer->readyweapon == WP_FIRST)
		{
			for(i=0; i<2; i++) patches[i] = dim[i];
		}
		if(CPlayer->readyweapon == WP_SECOND)
		{
			if(!patches[0]) patches[0] = bright[0];
			patches[1] = dim[1];
		}
		if(CPlayer->readyweapon == WP_THIRD)
		{
			patches[0] = dim[0];
			if(!patches[1]) patches[1] = bright[1];
		}
		if(CPlayer->readyweapon == WP_FOURTH)
		{
			for(i=0; i<2; i++) if(!patches[i]) patches[i] = bright[i];
		}
		for(i=0; i<2; i++)
		{
			GL_DrawPatch(2, ypos + i*13, patches[i]);
			DrINumber(CPlayer->mana[i], 18, ypos + i*13);
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
			GL_DrawPatch(284, 169,
				W_GetNumForName(patcharti[CPlayer->readyArtifact]));
			if(CPlayer->inventory[inv_ptr].count > 1)
			{
				DrSmallNumber(CPlayer->inventory[inv_ptr].count, 302, 192);
			}
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
				GL_DrawPatch(49+i*31, 167, W_GetNumForName(
					patcharti[CPlayer->inventory[x+i].type]));//, PU_CACHE));

				if(CPlayer->inventory[x+i].count > 1)
				{
					DrSmallNumber(CPlayer->inventory[x+i].count, 66+i*31,
 						188);
				}
			}
		}
		GL_DrawPatch(50+curpos*31, 167, PatchNumSELECTBOX);
		if(x != 0)
		{
			GL_DrawPatch(40, 167, !(leveltime&4) ? PatchNumINVLFGEM1 :
				PatchNumINVLFGEM2);
		}
		if(CPlayer->inventorySlotNum-x > 7)
		{
			GL_DrawPatch(268, 167, !(leveltime&4) ? PatchNumINVRTGEM1 : PatchNumINVRTGEM2);
		}
	}
}


//==========================================================================
//
// Draw_TeleportIcon
//
//==========================================================================
void Draw_TeleportIcon(void)
{
	// Draw teleport icon and show it on screen.
	// We'll do it twice, and also clear the screen. 
	// This way there's be no flickering with video cards that use 
	// page flipping (progress bar!).
	int i;

	// Dedicated servers don't draw anything.
	if(IS_DEDICATED) return;

	for(i = 0; i < 2; i++)
	{
		gl.Clear(DGL_COLOR_BUFFER_BIT);
		GL_DrawRawScreen(W_CheckNumForName("TRAVLPIC"), 0, 0);
		GL_DrawPatch(100, 68, W_GetNumForName("teleicon"));
		if(i) break;
		gl.Show();
	}
	
	// Mark the next frame for fullscreen update.
	GL_Update(DDUF_FULLSCREEN);
}

//==========================================================================
//
// Draw_SaveIcon
//
//==========================================================================
void Draw_SaveIcon(void)
{
	GL_DrawPatch(100, 68, W_GetNumForName("saveicon"));
	GL_Update(DDUF_FULLSCREEN | DDUF_UPDATE);
	GL_Update(DDUF_FULLSCREEN);
}

//==========================================================================
//
// Draw_LoadIcon
//
//==========================================================================
void Draw_LoadIcon(void)
{
	GL_DrawPatch(100, 68, W_GetNumForName("loadicon"));
	GL_Update(DDUF_FULLSCREEN | DDUF_UPDATE);
	GL_Update(DDUF_FULLSCREEN);
}


//==========================================================================
//
// SB_Responder
//
//==========================================================================

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

static boolean canCheat()
{
	if(IS_NETGAME && !IS_CLIENT && netSvAllowCheats) return true;
#ifdef _DEBUG
	return true;
#else
	return !(gameskill == sk_nightmare || (netgame && !netcheat) 
		|| players[consoleplayer].health <= 0);
#endif
}

//==========================================================================
//
// HandleCheats
//
// Returns true if the caller should eat the key.
//
//==========================================================================

static boolean HandleCheats(byte key)
{
	int i;
	boolean eat;

	if(gameskill == sk_nightmare)
	{ // Can't cheat in nightmare mode
		return(false);
	}
	else if(netgame)
	{ // change CD track is the only cheat available in deathmatch
		eat = false;
/*		if(i_CDMusic)
		{
			if(CheatAddKey(&Cheats[0], key, &eat))
			{
				Cheats[0].func(&players[consoleplayer], &Cheats[0]);
				S_StartSound(SFX_PLATFORM_STOP, NULL);
			}
			if(CheatAddKey(&Cheats[1], key, &eat))
			{
				Cheats[1].func(&players[consoleplayer], &Cheats[1]);
				S_StartSound(SFX_PLATFORM_STOP, NULL);
			}
		}*/
		return eat;
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
			S_StartSound(SFX_PLATFORM_STOP, NULL);
		}
	}
	//printf( "end of handlecheat: %d (%d)\n",key,eat);
	return(eat);
}

//==========================================================================
//
// CheatAddkey
//
// Returns true if the added key completed the cheat, false otherwise.
//
//==========================================================================

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

//==========================================================================
//
// CHEAT FUNCTIONS
//
//==========================================================================

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
	if(player->cheats&CF_GODMODE)
	{
		P_SetMessage(player, TXT_CHEATGODON, true);
	}
	else
	{
		P_SetMessage(player, TXT_CHEATGODOFF, true);
	}
	SB_state = -1;
}

static void CheatNoClipFunc(player_t *player, Cheat_t *cheat)
{
	player->cheats ^= CF_NOCLIP;
	player->update |= PSF_STATE;
	if(player->cheats&CF_NOCLIP)
	{
		P_SetMessage(player, TXT_CHEATNOCLIPON, true);
	}
	else
	{
		P_SetMessage(player, TXT_CHEATNOCLIPOFF, true);
	}
}

static void CheatWeaponsFunc(player_t *player, Cheat_t *cheat)
{
	int i;

	player->update |= PSF_ARMOR_POINTS | PSF_OWNED_WEAPONS | PSF_AMMO;

	for(i = 0; i < NUMARMOR; i++)
	{
		player->armorpoints[i] = ArmorIncrement[player->class][i];
	}
	for(i = 0; i < NUMWEAPONS; i++)
	{
		player->weaponowned[i] = true;
	}
	for(i = 0; i < NUMMANA; i++)
	{
		player->mana[i] = MAX_MANA;
	}
	P_SetMessage(player, TXT_CHEATWEAPONS, true);
}

static void CheatHealthFunc(player_t *player, Cheat_t *cheat)
{
	player->update |= PSF_HEALTH;
	if(player->morphTics)
	{
		player->health = player->plr->mo->health = MAXMORPHHEALTH;
	}
	else
	{
		player->health = player->plr->mo->health = MAXHEALTH;
	}
	P_SetMessage(player, TXT_CHEATHEALTH, true);
}

static void CheatKeysFunc(player_t *player, Cheat_t *cheat)
{
	player->update |= PSF_KEYS;
	player->keys = 2047;
	P_SetMessage(player, TXT_CHEATKEYS, true);
}

static void CheatSoundFunc(player_t *player, Cheat_t *cheat)
{
	DebugSound = !DebugSound;
	if(DebugSound)
	{
		P_SetMessage(player, TXT_CHEATSOUNDON, true);
	}
	else
	{
		P_SetMessage(player, TXT_CHEATSOUNDOFF, true);
	}
}

static void CheatTickerFunc(player_t *player, Cheat_t *cheat)
{
/*	extern int DisplayTicker;

	DisplayTicker = !DisplayTicker;
	if(DisplayTicker)
	{
		P_SetMessage(player, TXT_CHEATTICKERON, true);
	}
	else
	{
		P_SetMessage(player, TXT_CHEATTICKEROFF, true);
	}*/
}

static void CheatArtifactAllFunc(player_t *player, Cheat_t *cheat)
{
	int i;
	int j;

	for(i = arti_none+1; i < arti_firstpuzzitem; i++)
	{
		for(j = 0; j < 25; j++)
		{
			P_GiveArtifact(player, i, NULL);
		}
	}
	P_SetMessage(player, TXT_CHEATARTIFACTS3, true);
}

static void CheatPuzzleFunc(player_t *player, Cheat_t *cheat)
{
	int i;

	for(i = arti_firstpuzzitem; i < NUMARTIFACTS; i++)
	{
		P_GiveArtifact(player, i, NULL);
	}
	P_SetMessage(player, TXT_CHEATARTIFACTS3, true);
}

static void CheatInitFunc(player_t *player, Cheat_t *cheat)
{
	G_DeferedInitNew(gameskill, gameepisode, gamemap);
	P_SetMessage(player, TXT_CHEATWARP, true);
}

static void CheatWarpFunc(player_t *player, Cheat_t *cheat)
{
	int tens;
	int ones;
	int map;
	char mapName[9];
	char auxName[128];
	FILE *fp;

	tens = cheat->args[0]-'0';
	ones = cheat->args[1]-'0';
	if(tens < 0 || tens > 9 || ones < 0 || ones > 9)
	{ // Bad map
		P_SetMessage(player, TXT_CHEATBADINPUT, true);
		return;
	}
	//map = P_TranslateMap((cheat->args[0]-'0')*10+cheat->args[1]-'0');
	map = P_TranslateMap(tens*10+ones);
	if(map == -1)
	{ // Not found
		P_SetMessage(player, TXT_CHEATNOMAP, true);
		return;
	}
	if(map == gamemap)
	{ // Don't try to teleport to current map
		P_SetMessage(player, TXT_CHEATBADINPUT, true);
		return;
	}
	if(DevMaps)
	{ // Search map development directory
		sprintf(auxName, "%sMAP%02d.WAD", DevMapsDir, map);
		fp = fopen(auxName, "rb");
		if(fp)
		{
			fclose(fp);
		}
		else
		{ // Can't find
			P_SetMessage(player, TXT_CHEATNOMAP, true);
			return;
		}
	}
	else
	{ // Search primary lumps
		sprintf(mapName, "MAP%02d", map);
		if(W_CheckNumForName(mapName) == -1)
		{ // Can't find
			P_SetMessage(player, TXT_CHEATNOMAP, true);
			return;
		}
	}
	P_SetMessage(player, TXT_CHEATWARP, true);
	G_TeleportNewMap(map, 0);
	//G_Completed(-1, -1);//map, 0);
}

static void CheatPigFunc(player_t *player, Cheat_t *cheat)
{
	extern boolean P_UndoPlayerMorph(player_t *player);

	if(player->morphTics)
	{
		P_UndoPlayerMorph(player);
	}
	else
	{
	 	P_MorphPlayer(player);
	}
	P_SetMessage(player, "SQUEAL!!", true);
}

static void CheatMassacreFunc(player_t *player, Cheat_t *cheat)
{
	int count;
	char buffer[80];

	count = P_Massacre();
	sprintf(buffer, "%d MONSTERS KILLED\n", count);
	P_SetMessage(player, buffer, true);
}

static void CheatIDKFAFunc(player_t *player, Cheat_t *cheat)
{
	int i;
	if(player->morphTics)
	{
		return;
	}
	for(i = 1; i < 8; i++)
	{
		player->weaponowned[i] = false;
	}
	player->pendingweapon = WP_FIRST;
	P_SetMessage(player, TXT_CHEATIDKFA, true);
}

static void CheatQuickenFunc1(player_t *player, Cheat_t *cheat)
{
	P_SetMessage(player, "TRYING TO CHEAT?  THAT'S ONE....", true);
}

static void CheatQuickenFunc2(player_t *player, Cheat_t *cheat)
{
	P_SetMessage(player, "THAT'S TWO....", true);
}

static void CheatQuickenFunc3(player_t *player, Cheat_t *cheat)
{
	P_DamageMobj(player->plr->mo, NULL, player->plr->mo, 10000);
	P_SetMessage(player, "THAT'S THREE!  TIME TO DIE.", true);
}

static void CheatClassFunc1(player_t *player, Cheat_t *cheat)
{	
	P_SetMessage(player, "ENTER NEW PLAYER CLASS (0 - 2)", true);
}

//==========================================================================
// SB_ChangePlayerClass
//	Changes the class of the given player. Will not work if the player
//	is currently morphed.
//==========================================================================
void SB_ChangePlayerClass(player_t *player, int newclass)
{
	int i;
	mobj_t *oldmobj;
	mapthing_t dummy;

	// Don't change if morphed.
	if(player->morphTics) return;

	if(newclass < 0 || newclass > 2) return; // Must be 0-2.
	player->class = newclass;
	// Take away armor.
	for(i = 0; i < NUMARMOR; i++) 
		player->armorpoints[i] = 0;
	cfg.PlayerClass[player - players] = newclass;
	P_PostMorphWeapon(player, WP_FIRST);
	if(player == players + consoleplayer) SB_SetClassData();
	player->update |= PSF_ARMOR_POINTS;

	// Respawn the player and destroy the old mobj.
	oldmobj = player->plr->mo;
	if(oldmobj)
	{
		// Use a dummy as the spawn point.
		dummy.x = oldmobj->x >> FRACBITS;
		dummy.y = oldmobj->y >> FRACBITS;
		// The +27 (45/2) makes the approximation properly averaged.
		dummy.angle = (short) ((float) oldmobj->angle / ANGLE_MAX * 360) + 27;
		P_SpawnPlayer(&dummy, player - players);
		P_RemoveMobj(oldmobj);
	}
}

static void CheatClassFunc2(player_t *player, Cheat_t *cheat)
{
	int class;

	if(player->morphTics)
	{ // don't change class if the player is morphed
		return;
	}
	class = cheat->args[0]-'0';
	if(class > 2 || class < 0)
	{
		P_SetMessage(player, "INVALID PLAYER CLASS", true);
		return;
	}
	SB_ChangePlayerClass(player, class);
}

static void CheatVersionFunc(player_t *player, Cheat_t *cheat)
{
	P_SetMessage(player, VERSIONTEXT, true);
}

static void CheatDebugFunc(player_t *player, Cheat_t *cheat)
{
	char textBuffer[50];
	sprintf(textBuffer, "MAP %d (%d)  X:%5d  Y:%5d  Z:%5d",
				P_GetMapWarpTrans(gamemap),
				gamemap,
				player->plr->mo->x >> FRACBITS,
				player->plr->mo->y >> FRACBITS,
				player->plr->mo->z >> FRACBITS);
	P_SetMessage(player, textBuffer, true);
}

static void CheatScriptFunc1(player_t *player, Cheat_t *cheat)
{
	P_SetMessage(player, "RUN WHICH SCRIPT(01-99)?", true);
}

static void CheatScriptFunc2(player_t *player, Cheat_t *cheat)
{
	P_SetMessage(player, "RUN WHICH SCRIPT(01-99)?", true);
}

static void CheatScriptFunc3(player_t *player, Cheat_t *cheat)
{
	int script;
	byte args[3];
	int tens, ones;
	char textBuffer[40];

	tens = cheat->args[0]-'0';
	ones = cheat->args[1]-'0';
	script = tens*10 + ones;
	if (script < 1) return;
	if (script > 99) return;
	args[0]=args[1]=args[2]=0;

	if(P_StartACS(script, 0, args, player->plr->mo, NULL, 0))
	{
		sprintf(textBuffer, "RUNNING SCRIPT %.2d", script);
		P_SetMessage(player, textBuffer, true);
	}
}

extern int cheating;

static void CheatRevealFunc(player_t *player, Cheat_t *cheat)
{
	cheating = (cheating+1) % 3;
}

//===========================================================================
//
// CheatTrackFunc1
//
//===========================================================================

static void CheatTrackFunc1(player_t *player, Cheat_t *cheat)
{
/*	char buffer[80];

	if(!i_CDMusic)
	{
		return;
	}
	if(gi.CD(DD_INIT, 0) == -1)
	{
		P_SetMessage(player, "ERROR INITIALIZING CD", true);
	}
	sprintf(buffer, "ENTER DESIRED CD TRACK (%.2d - %.2d):\n",
		gi.CD(DD_GET_FIRST_TRACK, 0), gi.CD(DD_GET_LAST_TRACK, 0));	
	P_SetMessage(player, buffer, true);*/
}

//===========================================================================
//
// CheatTrackFunc2
//
//===========================================================================

static void CheatTrackFunc2(player_t *player, Cheat_t *cheat)
{
/*	char buffer[80];
	int track;

	if(!i_CDMusic)
	{
		return;
	}
	track = (cheat->args[0]-'0')*10+(cheat->args[1]-'0');
	if(track < gi.CD(DD_GET_FIRST_TRACK, 0) || track > gi.CD(DD_GET_LAST_TRACK, 0))
	{
		P_SetMessage(player, "INVALID TRACK NUMBER\n", true);
		return;
	} 
	if(track == gi.CD(DD_GET_CURRENT_TRACK,0))
	{
		return;
	}
	if(gi.CD(DD_PLAY_LOOP, track))
	{
		sprintf(buffer, "ERROR WHILE TRYING TO PLAY CD TRACK: %.2d\n", track);
		P_SetMessage(player, buffer, true);
	}
	else
	{ // No error encountered while attempting to play the track
		sprintf(buffer, "PLAYING TRACK: %.2d\n", track);
		P_SetMessage(player, buffer, true);	
	}*/
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
		Con_Printf( "Usage: cheat (cheat)\nFor example, 'cheat visit21'.\n");
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

	// Check the arguments.
	if(argc != 2 && argc != 3)
		tellUsage = true;
	else if(!strnicmp(argv[1], "weapons", 1))
		CheatWeaponsFunc(players+target, NULL);
	else if(!strnicmp(argv[1], "health", 1))
		CheatHealthFunc(players+target, NULL);
	else if(!strnicmp(argv[1], "keys", 1))
		CheatKeysFunc(players+target, NULL);
	else if(!strnicmp(argv[1], "artifacts", 1))
		CheatArtifactAllFunc(players+target, NULL);
	else if(!strnicmp(argv[1], "puzzle", 1))
		CheatPuzzleFunc(players+target, NULL);
	else 
		tellUsage = true;

	if(tellUsage)
	{
		Con_Printf( "Usage: give weapons/health/keys/artifacts/puzzle\n");
		Con_Printf( "The first letter is enough, e.g. 'give h'.\n");
	}
	return true;
}

int CCmdCheatWarp(int argc, char **argv)
{
	Cheat_t cheat;
	int		num;

	if(!canCheat()) return false; // Can't cheat!
	if(argc != 2)
	{
		Con_Printf( "Usage: warp (num)\n");
		return true;
	}
	num = atoi(argv[1]);
	cheat.args[0] = num/10 + '0';
	cheat.args[1] = num%10 + '0';
	// We don't want that keys are repeated while we wait.
	DD_ClearKeyRepeaters();
	CheatWarpFunc(players+consoleplayer, &cheat);
	return true;
}

int CCmdCheatPig(int argc, char **argv)
{
	if(!canCheat()) return false; // Can't cheat!
	CheatPigFunc(players+consoleplayer, NULL);
	return true;
}

int CCmdCheatMassacre(int argc, char **argv)
{
	if(!canCheat()) return false; // Can't cheat!
	DD_ClearKeyRepeaters();
	CheatMassacreFunc(players+consoleplayer, NULL);
	return true;
}

int CCmdCheatShadowcaster(int argc, char **argv)
{
	Cheat_t	cheat;

	if(!canCheat()) return false; // Can't cheat!
	if(argc != 2)
	{
		Con_Printf( "Usage: class (0-2)\n");
		Con_Printf( "0=Fighter, 1=Cleric, 2=Mage.\n");
		return true;
	}
	cheat.args[0] = atoi(argv[1]) + '0';
	CheatClassFunc2(players + consoleplayer, &cheat);
	return true;
}

int CCmdCheatWhere(int argc, char **argv)
{
	//if(!canCheat()) return false; // Can't cheat!
	CheatDebugFunc(players+consoleplayer, NULL);
	return true;
}

int CCmdCheatRunScript(int argc, char **argv)
{
	Cheat_t cheat;
	int		num;

	if(!canCheat()) return false; // Can't cheat!
	if(argc != 2)
	{
		Con_Printf( "Usage: runscript (1-99)\n");
		return true;
	}
	num = atoi(argv[1]);
	cheat.args[0] = num/10 + '0';
	cheat.args[1] = num%10 + '0';
	CheatScriptFunc3(players+consoleplayer, &cheat);
	return true;
}

int CCmdCheatReveal(int argc, char **argv)
{
	int	option;

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

