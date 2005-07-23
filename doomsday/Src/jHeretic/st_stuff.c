
// SB_bar.c

#include "jHeretic/Doomdef.h"
#include "jHeretic/P_local.h"
#include "jHeretic/Soundst.h"
#include "jHeretic/st_stuff.h"
#include "jHeretic/h_config.h"
#include "Common/hu_stuff.h"
#include "Common/st_lib.h"

// Macros

// Location and size of statistics,
//  justified according to widget type.
// Problem is, within which space? STbar? Screen?
// Note: this could be read in by a lump.
//       Problem is, is the stuff rendered
//       into a buffer,
//       or into the frame buffer?

// current ammo icon(sbbar)
#define ST_AMMOIMGWIDTH		24
#define ST_AMMOICONX			111
#define ST_AMMOICONY			172

// inventory
#define ST_INVENTORYX			50
#define ST_INVENTORYY			160

// how many inventory slots are visible
#define NUMVISINVSLOTS 7

// invslot artifact count (relative to each slot)
#define ST_INVCOUNTOFFX			27
#define ST_INVCOUNTOFFY			22

// current artifact (sbbar)
#define ST_ARTIFACTWIDTH	24
#define ST_ARTIFACTX			179
#define ST_ARTIFACTY			160

// current artifact count (sbar)
#define ST_ARTIFACTCWIDTH	2
#define ST_ARTIFACTCX			209
#define ST_ARTIFACTCY			182

// AMMO number pos.
#define ST_AMMOWIDTH		3
#define ST_AMMOX			135
#define ST_AMMOY			162

// ARMOR number pos.
#define ST_ARMORWIDTH		3
#define ST_ARMORX			254
#define ST_ARMORY			170

// HEALTH number pos.
#define ST_HEALTHWIDTH		3
#define ST_HEALTHX			85
#define ST_HEALTHY			170

// Key icon positions.
#define ST_KEY0WIDTH		10
#define ST_KEY0HEIGHT		6
#define ST_KEY0X			153
#define ST_KEY0Y			164
#define ST_KEY1WIDTH		ST_KEY0WIDTH
#define ST_KEY1X			153
#define ST_KEY1Y			172
#define ST_KEY2WIDTH		ST_KEY0WIDTH
#define ST_KEY2X			153
#define ST_KEY2Y			180

// Frags pos.
#define ST_FRAGSX			85
#define ST_FRAGSY			171
#define ST_FRAGSWIDTH		2

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

typedef struct Cheat_s {
	void    (*func) (player_t *player, struct Cheat_s * cheat);
	byte   *sequence;
	byte   *pos;
	int     args[2];
	int     currentArg;
} Cheat_t;

// Private Functions

//static void DrawSoundInfo(void);
//static void ShadeLine(int x, int y, int height, int shade);
static void ShadeChain(void);
static void DrINumber(signed int val, int x, int y, float r, float g, float b, float a);
static void DrBNumber(signed int val, int x, int y, float Red, float Green, float Blue, float Alpha);
static void DrawChain(void);
void ST_drawWidgets(boolean refresh);
//static void DrawInventoryItems(void);
static void ST_doFullscreenStuff(void);
static boolean HandleCheats(byte key);
static boolean CheatAddKey(Cheat_t * cheat, byte key, boolean *eat);
static void CheatGodFunc(player_t *player, Cheat_t * cheat);
static void CheatNoClipFunc(player_t *player, Cheat_t * cheat);
static void CheatWeaponsFunc(player_t *player, Cheat_t * cheat);
static void CheatPowerFunc(player_t *player, Cheat_t * cheat);
static void CheatHealthFunc(player_t *player, Cheat_t * cheat);
static void CheatKeysFunc(player_t *player, Cheat_t * cheat);
static void CheatSoundFunc(player_t *player, Cheat_t * cheat);
static void CheatTickerFunc(player_t *player, Cheat_t * cheat);
static void CheatArtifact1Func(player_t *player, Cheat_t * cheat);
static void CheatArtifact2Func(player_t *player, Cheat_t * cheat);
static void CheatArtifact3Func(player_t *player, Cheat_t * cheat);
static void CheatWarpFunc(player_t *player, Cheat_t * cheat);
static void CheatChickenFunc(player_t *player, Cheat_t * cheat);
static void CheatMassacreFunc(player_t *player, Cheat_t * cheat);
static void CheatIDKFAFunc(player_t *player, Cheat_t * cheat);
static void CheatIDDQDFunc(player_t *player, Cheat_t * cheat);

// Public Data

// slide statusbar amount 1.0 is fully open
static float showbar = 0.0f;

// fullscreen hud alpha value
static float hudalpha = 0.0f;

//boolean DebugSound; // debug flag for displaying sound info

// ST_Start() has just been called
static boolean st_firsttime;

// whether left-side main status bar is active
static boolean st_statusbaron;

// main player in game
static player_t *plyr;

// used for timing
static unsigned int st_clock;

// used when in chat 
static st_chatstateenum_t st_chatstate;

// whether in automap or first-person
static st_stateenum_t st_gamestate;

// whether status bar chat is active
static boolean st_chat;

// value of st_chat before message popped up
static boolean st_oldchat;

// whether chat window has the cursor on
static boolean st_cursoron;

// current inventory slot indices. 0 = none
static int st_invslot[NUMVISINVSLOTS];

// current inventory slot count indices. 0 = none
static int st_invslotcount[NUMVISINVSLOTS];

// current artifact index. 0 = none
static int st_artici = 0;

// current artifact widget
static st_multicon_t w_artici;

// current artifact count widget
static st_number_t w_articount;

// inventory slot widgets
static st_multicon_t w_invslot[NUMVISINVSLOTS];

// inventory slot count widgets
static st_number_t w_invslotcount[NUMVISINVSLOTS];

// current ammo icon index.
static int st_ammoicon;

// current ammo icon widget
static st_multicon_t w_ammoicon;

// ready-weapon widget
static st_number_t w_ready;

 // in deathmatch only, summary of frags stats
static st_number_t w_frags;

// health widget
static st_number_t w_health;

// armor widget
static st_number_t w_armor;

// keycard widgets
static st_binicon_t w_keyboxes[3];

// holds key-type for each key box on bar
static boolean	keyboxes[3];

 // number of frags so far in deathmatch
static int st_fragscount;

// !deathmatch
static boolean st_fragson;

boolean inventory;
int     curpos;
int     inv_ptr;
int     ArtifactFlash;

// Private Data

// whether to use alpha blending
static boolean st_blended = false;

static int HealthMarker;
static int ChainWiggle;
static player_t *CPlayer;
int     lu_palette;

static int oldarti = 0;
static int oldartiCount = 0;
//static int oldfrags = -9999;
static int oldammo = -1;
//static int oldarmor = -1;
static int oldweapon = -1;
static int oldhealth = -1;
//static int oldlife = -1;
//static int oldkeys = -1;

// ammo patch names
char    ammopic[][10] = {
	{"INAMGLD"},
	{"INAMBOW"},
	{"INAMBST"},
	{"INAMRAM"},
	{"INAMPNX"},
	{"INAMLOB"}
};

// Artifact patch names
char    artifactlist[][10] = {
	{"USEARTIA"},				// use artifact flash
	{"USEARTIB"},
	{"USEARTIC"},
	{"USEARTID"},
	{"USEARTIE"},
	{"ARTIBOX"},				// none
	{"ARTIINVU"},				// invulnerability
	{"ARTIINVS"},				// invisibility
	{"ARTIPTN2"},				// health
	{"ARTISPHL"},				// superhealth
	{"ARTIPWBK"},				// tomeofpower
	{"ARTITRCH"},				// torch
	{"ARTIFBMB"},				// firebomb
	{"ARTIEGGC"},				// egg
	{"ARTISOAR"},				// fly
	{"ARTIATLP"}				// teleport
};

//static dpatch_t     PatchLTFACE;
//static dpatch_t     PatchRTFACE;
static dpatch_t     PatchBARBACK;
static dpatch_t     PatchCHAIN;
static dpatch_t     PatchSTATBAR;
static dpatch_t     PatchLIFEGEM;
static dpatch_t     PatchLTFCTOP;
static dpatch_t     PatchRTFCTOP;
static dpatch_t     PatchSELECTBOX;
static dpatch_t     PatchINVLFGEM1;
static dpatch_t     PatchINVLFGEM2;
static dpatch_t     PatchINVRTGEM1;
static dpatch_t     PatchINVRTGEM2;
static dpatch_t     PatchINumbers[10];
static dpatch_t     PatchNEGATIVE;
static dpatch_t     PatchSmNumbers[10];
//static dpatch_t     PatchBLACKSQ;
static dpatch_t     PatchINVBAR;
//static dpatch_t     PatchARMCLEAR;
//static dpatch_t     PatchCHAINBACK;
static dpatch_t     PatchAMMOICONS[11];
static dpatch_t     PatchARTIFACTS[16];
static dpatch_t     spinbooklump;
static dpatch_t     spinflylump;

// 3 keys
static dpatch_t keys[NUMKEYS];

//byte *ShadeTables;
extern byte *screen;
int     FontBNumBase;

static byte CheatLookup[256];

// Toggle god mode
static byte CheatGodSeq[] = {
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
static byte CheatNoClipSeq[] = {
	CHEAT_ENCRYPT('k'),
	CHEAT_ENCRYPT('i'),
	CHEAT_ENCRYPT('t'),
	CHEAT_ENCRYPT('t'),
	CHEAT_ENCRYPT('y'),
	0xff
};

// Get all weapons and ammo
static byte CheatWeaponsSeq[] = {
	CHEAT_ENCRYPT('r'),
	CHEAT_ENCRYPT('a'),
	CHEAT_ENCRYPT('m'),
	CHEAT_ENCRYPT('b'),
	CHEAT_ENCRYPT('o'),
	0xff
};

// Toggle tome of power
static byte CheatPowerSeq[] = {
	CHEAT_ENCRYPT('s'),
	CHEAT_ENCRYPT('h'),
	CHEAT_ENCRYPT('a'),
	CHEAT_ENCRYPT('z'),
	CHEAT_ENCRYPT('a'),
	CHEAT_ENCRYPT('m'),
	0xff, 0
};

// Get full health
static byte CheatHealthSeq[] = {
	CHEAT_ENCRYPT('p'),
	CHEAT_ENCRYPT('o'),
	CHEAT_ENCRYPT('n'),
	CHEAT_ENCRYPT('c'),
	CHEAT_ENCRYPT('e'),
	0xff
};

// Get all keys
static byte CheatKeysSeq[] = {
	CHEAT_ENCRYPT('s'),
	CHEAT_ENCRYPT('k'),
	CHEAT_ENCRYPT('e'),
	CHEAT_ENCRYPT('l'),
	0xff, 0
};

// Toggle sound debug info
static byte CheatSoundSeq[] = {
	CHEAT_ENCRYPT('n'),
	CHEAT_ENCRYPT('o'),
	CHEAT_ENCRYPT('i'),
	CHEAT_ENCRYPT('s'),
	CHEAT_ENCRYPT('e'),
	0xff
};

// Toggle ticker
static byte CheatTickerSeq[] = {
	CHEAT_ENCRYPT('t'),
	CHEAT_ENCRYPT('i'),
	CHEAT_ENCRYPT('c'),
	CHEAT_ENCRYPT('k'),
	CHEAT_ENCRYPT('e'),
	CHEAT_ENCRYPT('r'),
	0xff, 0
};

// Get an artifact 1st stage (ask for type)
static byte CheatArtifact1Seq[] = {
	CHEAT_ENCRYPT('g'),
	CHEAT_ENCRYPT('i'),
	CHEAT_ENCRYPT('m'),
	CHEAT_ENCRYPT('m'),
	CHEAT_ENCRYPT('e'),
	0xff
};

// Get an artifact 2nd stage (ask for count)
static byte CheatArtifact2Seq[] = {
	CHEAT_ENCRYPT('g'),
	CHEAT_ENCRYPT('i'),
	CHEAT_ENCRYPT('m'),
	CHEAT_ENCRYPT('m'),
	CHEAT_ENCRYPT('e'),
	0, 0xff, 0
};

// Get an artifact final stage
static byte CheatArtifact3Seq[] = {
	CHEAT_ENCRYPT('g'),
	CHEAT_ENCRYPT('i'),
	CHEAT_ENCRYPT('m'),
	CHEAT_ENCRYPT('m'),
	CHEAT_ENCRYPT('e'),
	0, 0, 0xff
};

// Warp to new level
static byte CheatWarpSeq[] = {
	CHEAT_ENCRYPT('e'),
	CHEAT_ENCRYPT('n'),
	CHEAT_ENCRYPT('g'),
	CHEAT_ENCRYPT('a'),
	CHEAT_ENCRYPT('g'),
	CHEAT_ENCRYPT('e'),
	0, 0, 0xff, 0
};

// Save a screenshot
static byte CheatChickenSeq[] = {
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
static byte CheatMassacreSeq[] = {
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

static byte CheatIDKFASeq[] = {
	CHEAT_ENCRYPT('i'),
	CHEAT_ENCRYPT('d'),
	CHEAT_ENCRYPT('k'),
	CHEAT_ENCRYPT('f'),
	CHEAT_ENCRYPT('a'),
	0xff, 0
};

static byte CheatIDDQDSeq[] = {
	CHEAT_ENCRYPT('i'),
	CHEAT_ENCRYPT('d'),
	CHEAT_ENCRYPT('d'),
	CHEAT_ENCRYPT('q'),
	CHEAT_ENCRYPT('d'),
	0xff, 0
};

static Cheat_t Cheats[] = {
	{CheatGodFunc, CheatGodSeq, NULL, {0, 0}, 0},
	{CheatNoClipFunc, CheatNoClipSeq, NULL, {0, 0}, 0},
	{CheatWeaponsFunc, CheatWeaponsSeq, NULL, {0, 0}, 0},
	{CheatPowerFunc, CheatPowerSeq, NULL, {0, 0}, 0},
	{CheatHealthFunc, CheatHealthSeq, NULL, {0, 0}, 0},
	{CheatKeysFunc, CheatKeysSeq, NULL, {0, 0}, 0},
	{CheatSoundFunc, CheatSoundSeq, NULL, {0, 0}, 0},
	{CheatTickerFunc, CheatTickerSeq, NULL, {0, 0}, 0},
	{CheatArtifact1Func, CheatArtifact1Seq, NULL, {0, 0}, 0},
	{CheatArtifact2Func, CheatArtifact2Seq, NULL, {0, 0}, 0},
	{CheatArtifact3Func, CheatArtifact3Seq, NULL, {0, 0}, 0},
	{CheatWarpFunc, CheatWarpSeq, NULL, {0, 0}, 0},
	{CheatChickenFunc, CheatChickenSeq, NULL, {0, 0}, 0},
	{CheatMassacreFunc, CheatMassacreSeq, NULL, {0, 0}, 0},
	{CheatIDKFAFunc, CheatIDKFASeq, NULL, {0, 0}, 0},
	{CheatIDDQDFunc, CheatIDDQDSeq, NULL, {0, 0}, 0},
	{NULL, NULL, NULL, {0, 0}, 0}	// Terminator
};

void ST_loadGraphics()
{
	int     i;

	char    namebuf[9];

	R_CachePatch(&PatchBARBACK, "BARBACK");
	R_CachePatch(&PatchINVBAR, "INVBAR");
	R_CachePatch(&PatchCHAIN, "CHAIN");

	if(deathmatch)
	{
		R_CachePatch(&PatchSTATBAR, "STATBAR");
	}
	else
	{
		R_CachePatch(&PatchSTATBAR, "LIFEBAR");
	}
	if(!IS_NETGAME)
	{							// single player game uses red life gem
		R_CachePatch(&PatchLIFEGEM, "LIFEGEM2");
	}
	else
	{
		sprintf(namebuf, "LIFEGEM%d", consoleplayer);
		R_CachePatch(&PatchLIFEGEM, namebuf);
	}

	R_CachePatch(&PatchLTFCTOP, "LTFCTOP");
	R_CachePatch(&PatchRTFCTOP, "RTFCTOP");
	R_CachePatch(&PatchSELECTBOX, "SELECTBOX");
	R_CachePatch(&PatchINVLFGEM1, "INVGEML1");
	R_CachePatch(&PatchINVLFGEM2, "INVGEML2");
	R_CachePatch(&PatchINVRTGEM1, "INVGEMR1");
	R_CachePatch(&PatchINVRTGEM2, "INVGEMR2");
	R_CachePatch(&PatchNEGATIVE, "NEGNUM");
	R_CachePatch(&spinbooklump, "SPINBK0");
	R_CachePatch(&spinflylump, "SPFLY0");

	for(i = 0; i < 10; i++)
	{
		sprintf(namebuf, "IN%d", i);
		R_CachePatch(&PatchINumbers[i], namebuf);

	}

	for(i = 0; i < 10; i++)
	{
		sprintf(namebuf, "SMALLIN%d", i);
		R_CachePatch(&PatchSmNumbers[i], namebuf);
	}

	// artifact icons (+5 for the use artifact flash patches)
	for(i = 0; i < (NUMARTIFACTS + 5); i++)
	{
		sprintf(namebuf, "%s", artifactlist[i]);
		R_CachePatch(&PatchARTIFACTS[i], namebuf);
	}

	// ammo icons
	for(i = 0; i < 10; i++)
	{
		sprintf(namebuf, "%s", ammopic[i]);
		R_CachePatch(&PatchAMMOICONS[i], namebuf);
	}

	// key cards
	R_CachePatch(&keys[0], "ykeyicon");
	R_CachePatch(&keys[1], "gkeyicon");
	R_CachePatch(&keys[2], "bkeyicon");

	FontBNumBase = W_GetNumForName("FONTB16");
}

void ST_loadData()
{
	int     i;

	lu_palette = W_GetNumForName("PLAYPAL");

	for(i = 0; i < 256; i++)
	{
		CheatLookup[i] = CHEAT_ENCRYPT(i);
	}

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

	st_artici = 0;

	st_ammoicon = 0;

	st_statusbaron = true;
	st_oldchat = st_chat = false;
	st_cursoron = false;

	for(i = 0; i < 3; i++)
	{
		keyboxes[i] = false;
	}

	for(i = 0; i < NUMVISINVSLOTS; i++)
	{
		st_invslot[i] = 0;
		st_invslotcount[i] = 0;
	}

	STlib_init();
}

void ST_updateWidgets(void)
{
	static int largeammo = 1994;	// means "n/a"
	int     i;
	int	temp;
	int     x;

	// must redirect the pointer if the ready weapon has changed.
	if(wpnlev1info[plyr->readyweapon].ammo == am_noammo)
		w_ready.num = &largeammo;
	else
		w_ready.num = &plyr->ammo[wpnlev1info[plyr->readyweapon].ammo];

	w_ready.data = plyr->readyweapon;

	temp = plyr->ammo[wpnlev1info[plyr->readyweapon].ammo];
	if(oldammo != temp || oldweapon != plyr->readyweapon)
		st_ammoicon = plyr->readyweapon - 1;

	// update keycard multiple widgets
	for(i = 0; i < 3; i++)
	{
		keyboxes[i] = plyr->keys[i] ? true : false;

	}

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

	// current artifact
	if(ArtifactFlash)
	{
		st_artici = 5 - ArtifactFlash;
		ArtifactFlash--;
		oldarti = -1;			// so that the correct artifact fills in after the flash
	}
	else if(oldarti != plyr->readyArtifact ||
			oldartiCount != plyr->inventory[inv_ptr].count)
	{
		if(CPlayer->readyArtifact > 0)
		{
			st_artici = plyr->readyArtifact + 5;
		}
		oldarti = plyr->readyArtifact;
		oldartiCount = plyr->inventory[inv_ptr].count;
	}

	// update the inventory

	x = inv_ptr - curpos;

	for(i = 0; i < NUMVISINVSLOTS; i++)
	{
		st_invslot[i] = plyr->inventory[x + i].type +5;  // plus 5 for useartifact patches
		st_invslotcount[i] = plyr->inventory[x + i].count;
	}

/*
	// get rid of chat window if up because of message
	if(!--st_msgcounter)
		st_chat = st_oldchat;
*/

}

void ST_createWidgets(void)
{
	int i;
	int width, temp;

	// ready weapon ammo
	STlib_initNum(&w_ready, ST_AMMOX, ST_AMMOY, PatchINumbers,
				  &plyr->ammo[wpnlev1info[plyr->readyweapon].ammo],
				  &st_statusbaron, ST_AMMOWIDTH, &cfg.statusbarCounterAlpha);

	// ready weapon icon
	STlib_initMultIcon(&w_ammoicon, ST_AMMOICONX, ST_AMMOICONY, PatchAMMOICONS, &st_ammoicon,
					   &st_statusbaron, &cfg.statusbarCounterAlpha);

	// the last weapon type
	w_ready.data = plyr->readyweapon;


	// health num
	STlib_initNum(&w_health, ST_HEALTHX, ST_HEALTHY, PatchINumbers,
					  &plyr->health, &st_statusbaron, ST_HEALTHWIDTH, &cfg.statusbarCounterAlpha);

	// armor percentage - should be colored later
	STlib_initNum(&w_armor, ST_ARMORX, ST_ARMORY, PatchINumbers,
					  &plyr->armorpoints, &st_statusbaron, ST_ARMORWIDTH, &cfg.statusbarCounterAlpha );

	// frags sum
	STlib_initNum(&w_frags, ST_FRAGSX, ST_FRAGSY, PatchINumbers, &st_fragscount,
				  &st_fragson, ST_FRAGSWIDTH, &cfg.statusbarCounterAlpha);

	// keyboxes 0-2
	STlib_initBinIcon(&w_keyboxes[0], ST_KEY0X, ST_KEY0Y, &keys[0], &keyboxes[0],
					   &keyboxes[0], 0, &cfg.statusbarCounterAlpha);

	STlib_initBinIcon(&w_keyboxes[1], ST_KEY1X, ST_KEY1Y, &keys[1], &keyboxes[1],
					   &keyboxes[1], 0, &cfg.statusbarCounterAlpha);

	STlib_initBinIcon(&w_keyboxes[2], ST_KEY2X, ST_KEY2Y, &keys[2], &keyboxes[2],
					   &keyboxes[2], 0, &cfg.statusbarCounterAlpha);

	// current artifact (stbar not inventory)
	STlib_initMultIcon(&w_artici, ST_ARTIFACTX, ST_ARTIFACTY, PatchARTIFACTS, &st_artici,
					   &st_statusbaron, &cfg.statusbarCounterAlpha);

	// current artifact count
	STlib_initNum(&w_articount, ST_ARTIFACTCX, ST_ARTIFACTCY, PatchSmNumbers,
					  &oldartiCount, &st_statusbaron, ST_ARTIFACTCWIDTH, &cfg.statusbarCounterAlpha);

	// inventory slots
	width = PatchARTIFACTS[5].width + 1;
	temp = 0;

	for (i = 0; i < NUMVISINVSLOTS; i++){
		// inventory slot icon
		STlib_initMultIcon(&w_invslot[i], ST_INVENTORYX + temp , ST_INVENTORYY, PatchARTIFACTS, &st_invslot[i],
					   &st_statusbaron, &cfg.statusbarCounterAlpha);

		// inventory slot count
		STlib_initNum(&w_invslotcount[i], ST_INVENTORYX + temp + ST_INVCOUNTOFFX, ST_INVENTORYY + ST_INVCOUNTOFFY, PatchSmNumbers,
					  &st_invslotcount[i], &st_statusbaron, ST_ARTIFACTCWIDTH, &cfg.statusbarCounterAlpha);

		temp += width;
	}
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
	ST_loadData();
}

//---------------------------------------------------------------------------
//
// PROC SB_Ticker
//
//---------------------------------------------------------------------------

void ST_Ticker(void)
{
	int     delta;
	int     curHealth;
	static int tomePlay = 0;

	ST_updateWidgets();

	if(leveltime & 1)
	{
		ChainWiggle = P_Random() & 1;
	}
	curHealth = players[consoleplayer].plr->mo->health;
	if(curHealth < 0)
	{
		curHealth = 0;
	}
	if(curHealth < HealthMarker)
	{
		delta = (HealthMarker - curHealth) >> 2;
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
		delta = (curHealth - HealthMarker) >> 2;
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
	   players[consoleplayer].powers[pw_weaponlevel2] < cfg.tomeSound * 35)
	{
		int     timeleft = players[consoleplayer].powers[pw_weaponlevel2] / 35;

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

static void DrINumber(signed int val, int x, int y, float r, float g, float b, float a)
{
	int     oldval;

	gl.Color4f(r,g,b,a);

	// Limit to 999.
	if(val > 999)
		val = 999;

	oldval = val;
	if(val < 0)
	{
		if(val < -9)
		{
			GL_DrawPatch_CS(x + 1, y + 1, W_GetNumForName("LAME"));
		}
		else
		{
			val = -val;
			GL_DrawPatch_CS(x + 18, y, PatchINumbers[val].lump);
			GL_DrawPatch_CS(x + 9, y, PatchNEGATIVE.lump);
		}
		return;
	}
	if(val > 99)
	{
		GL_DrawPatch_CS(x, y, PatchINumbers[val / 100].lump);
	}
	val = val % 100;
	if(val > 9 || oldval > 99)
	{
		GL_DrawPatch_CS(x + 9, y, PatchINumbers[val / 10].lump);
	}
	val = val % 10;
	GL_DrawPatch_CS(x + 18, y, PatchINumbers[val].lump);
}

//---------------------------------------------------------------------------
//
// PROC DrBNumber
//
// Draws a three digit number
//
//---------------------------------------------------------------------------

static void DrBNumber(signed int val, int x, int y, float red, float green, float blue, float alpha)
{
	patch_t *patch;
	int     xpos;
	int     oldval;

	oldval = val;
	xpos = x;
	if(val < 0)
	{
		val = 0;
	}
	if(val > 99)
	{
		patch = W_CacheLumpNum(FontBNumBase + val / 100, PU_CACHE);

		GL_DrawPatchLitAlpha(xpos + 8 - SHORT(patch->width) / 2, y +2, 0, .4f,
							 FontBNumBase + val / 100);
		GL_SetColorAndAlpha(red, green, blue, alpha);
		GL_DrawPatch_CS(xpos + 6 - SHORT(patch->width) / 2, y,
							 FontBNumBase + val / 100);
		GL_SetColorAndAlpha(1, 1, 1, 1);
	}
	val = val % 100;
	xpos += 12;
	if(val > 9 || oldval > 99)
	{
		patch = W_CacheLumpNum(FontBNumBase + val / 10, PU_CACHE);

		GL_DrawPatchLitAlpha(xpos + 8 - SHORT(patch->width) / 2, y +2, 0, .4f,
							 FontBNumBase + val / 10);
		GL_SetColorAndAlpha(red, green, blue, alpha);
		GL_DrawPatch_CS(xpos + 6 - SHORT(patch->width) / 2, y,
							 FontBNumBase + val / 10);
		GL_SetColorAndAlpha(1, 1, 1, 1);
	}
	val = val % 10;
	xpos += 12;
	patch = W_CacheLumpNum(FontBNumBase + val, PU_CACHE);

	GL_DrawPatchLitAlpha(xpos + 8 - SHORT(patch->width) / 2, y +2, 0, .4f,
							 FontBNumBase + val);
	GL_SetColorAndAlpha(red, green, blue, alpha);
	GL_DrawPatch_CS(xpos + 6 - SHORT(patch->width) / 2, y,
							 FontBNumBase + val);
	GL_SetColorAndAlpha(1, 1, 1, 1);
}

//---------------------------------------------------------------------------
//
// PROC DrSmallNumber
//
// Draws a small two digit number.
//
//---------------------------------------------------------------------------

static void _DrSmallNumber(int val, int x, int y, boolean skipone, float r, float g, float b, float a)
{
	gl.Color4f(r,g,b,a);

	if(skipone && val == 1)
	{
		return;
	}
	if(val > 9)
	{
		GL_DrawPatch_CS(x, y, PatchSmNumbers[val / 10].lump);
	}
	val = val % 10;
	GL_DrawPatch_CS(x + 4, y, PatchSmNumbers[val].lump);
}

static void DrSmallNumber(int val, int x, int y, float r, float g, float b, float a)
{
	_DrSmallNumber(val, x, y, true, r,g,b,a);
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
   } */

//---------------------------------------------------------------------------
//
// PROC ShadeChain
//
//---------------------------------------------------------------------------

static void ShadeChain(void)
{
	float shadea = (cfg.statusbarCounterAlpha + cfg.statusbarAlpha) /3;

	gl.Disable(DGL_TEXTURING);
	gl.Begin(DGL_QUADS);

	// The left shader.
	gl.Color4f(0, 0, 0, shadea);
	gl.Vertex2f(20, 200);
	gl.Vertex2f(20, 190);
	gl.Color4f(0, 0, 0, 0);
	gl.Vertex2f(35, 190);
	gl.Vertex2f(35, 200);

	// The right shader.
	gl.Vertex2f(277, 200);
	gl.Vertex2f(277, 190);
	gl.Color4f(0, 0, 0, shadea);
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


int     playerkeys = 0;

extern boolean automapactive;

/*
 *   ST_refreshBackground
 *
 *	Draws the whole statusbar backgound
 */
void ST_refreshBackground(void)
{
	if(st_blended && ((cfg.statusbarAlpha < 1.0f) && (cfg.statusbarAlpha > 0.0f)))
	{
		gl.Color4f(1, 1, 1, cfg.statusbarAlpha);

		// topbits
		GL_DrawPatch_CS(0, 148, PatchLTFCTOP.lump);
		GL_DrawPatch_CS(290, 148, PatchRTFCTOP.lump);

		GL_SetPatch(PatchBARBACK.lump);

		// top border
		GL_DrawCutRectTiled(34, 158, 248, 2, 320, 42, 34, 0, 0, 158, 0, 0);

		// chain background
		GL_DrawCutRectTiled(34, 191, 248, 9, 320, 42, 34, 33, 0, 191, 16, 8);

		// faces
		if(players[consoleplayer].cheats & CF_GODMODE)
		{
			// If GOD mode we need to cut windows
			GL_DrawCutRectTiled(0, 158, 34, 42, 320, 42, 0, 0, 16, 167, 16, 8);
			GL_DrawCutRectTiled(282, 158, 38, 42, 320, 42, 282, 0, 287, 167, 16, 8);

			GL_DrawPatch_CS(16, 167, W_GetNumForName("GOD1"));
			GL_DrawPatch_CS(287, 167, W_GetNumForName("GOD2"));
		} else {
			GL_DrawCutRectTiled(0, 158, 34, 42, 320, 42, 0, 0, 0, 158, 0, 0);
			GL_DrawCutRectTiled(282, 158, 38, 42, 320, 42, 282, 0, 0, 158, 0, 0);
		}

		if(!inventory)
			GL_DrawPatch_CS(34, 160, PatchSTATBAR.lump);
		else
			GL_DrawPatch_CS(34, 160, PatchINVBAR.lump);

		DrawChain();

 	} else if(cfg.statusbarAlpha != 0.0f){
		// we can just render the full thing as normal

		// topbits
		GL_DrawPatch(0, 148, PatchLTFCTOP.lump);
		GL_DrawPatch(290, 148, PatchRTFCTOP.lump);

		// faces
		GL_DrawPatch(0, 158, PatchBARBACK.lump);

		if(players[consoleplayer].cheats & CF_GODMODE)
		{
			GL_DrawPatch(16, 167, W_GetNumForName("GOD1"));
			GL_DrawPatch(287, 167, W_GetNumForName("GOD2"));
		}

		if(!inventory)
			GL_DrawPatch(34, 160, PatchSTATBAR.lump);
		else
			GL_DrawPatch(34, 160, PatchINVBAR.lump);

		DrawChain();
	}
}

void ST_drawIcons(void)
{
	int     frame;
	static boolean hitCenterFrame;
	float iconalpha = hudalpha - ( 1 - cfg.hudIconAlpha);
	float textalpha = hudalpha - ( 1 - cfg.hudColor[3]);

    Draw_BeginZoom(cfg.hudScale, 2, 2);

	// Flight icons
	if(CPlayer->powers[pw_flight])
	{
		int     offset = (cfg.hudShown[HUD_AMMO] && cfg.screenblocks > 10 &&
						  CPlayer->readyweapon > 0 &&
						  CPlayer->readyweapon < 7) ? 43 : 0;
		if(CPlayer->powers[pw_flight] > BLINKTHRESHOLD ||
		   !(CPlayer->powers[pw_flight] & 16))
		{
			frame = (leveltime / 3) & 15;
			if(CPlayer->plr->mo->flags2 & MF2_FLY)
			{
				if(hitCenterFrame && (frame != 15 && frame != 0))
				{
					GL_DrawPatchLitAlpha(20 + offset, 17, 1, iconalpha, spinflylump.lump + 15);
				}
				else
				{
					GL_DrawPatchLitAlpha(20 + offset, 17, 1, iconalpha, spinflylump.lump + frame);
					hitCenterFrame = false;
				}
			}
			else
			{
				if(!hitCenterFrame && (frame != 15 && frame != 0))
				{
					GL_DrawPatchLitAlpha(20 + offset, 17, 1, iconalpha, spinflylump.lump + frame);
					hitCenterFrame = false;
				}
				else
				{
					GL_DrawPatchLitAlpha(20 + offset, 17, 1, iconalpha, spinflylump.lump + 15);
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

    Draw_EndZoom();

    Draw_BeginZoom(cfg.hudScale, 318, 2);

	if(CPlayer->powers[pw_weaponlevel2] && !CPlayer->chickenTics)
	{
		if(cfg.tomeCounter || CPlayer->powers[pw_weaponlevel2] > BLINKTHRESHOLD
		   || !(CPlayer->powers[pw_weaponlevel2] & 16))
		{
			frame = (leveltime / 3) & 15;
			if(cfg.tomeCounter && CPlayer->powers[pw_weaponlevel2] < 35)
			{
				gl.Color4f(1, 1, 1, CPlayer->powers[pw_weaponlevel2] / 35.0f);
			}
			GL_DrawPatchLitAlpha(300, 17, 1, iconalpha, spinbooklump.lump + frame);
			GL_Update(DDUF_TOP | DDUF_MESSAGES);
		}
		else
		{
			GL_Update(DDUF_TOP | DDUF_MESSAGES);
		}
		if(CPlayer->powers[pw_weaponlevel2] < cfg.tomeCounter * 35)
		{
			_DrSmallNumber(1 + CPlayer->powers[pw_weaponlevel2] / 35, 303, 30,
						   false,1,1,1,textalpha);
		}
	}

    Draw_EndZoom();
}

/*
 *   ST_doRefresh
 *
 *	All drawing for the status bar starts and ends here
 */
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

	ST_drawIcons();
}

void ST_Drawer(int fullscreenmode, boolean refresh )
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

	gl.Color4f(1,1,1,1);
	ST_drawIcons();
}

// sets the new palette based upon current values of player->damagecount
// and player->bonuscount
void ST_doPaletteStuff(void)
{
	static int sb_palette = 0;
	int     palette;

	//  byte *pal;

	CPlayer = &players[consoleplayer];

	if(CPlayer->damagecount)
	{
		palette = (CPlayer->damagecount + 7) >> 3;
		if(palette >= NUMREDPALS)
		{
			palette = NUMREDPALS - 1;
		}
		palette += STARTREDPALS;
	}
	else if(CPlayer->bonuscount)
	{
		palette = (CPlayer->bonuscount + 7) >> 3;
		if(palette >= NUMBONUSPALS)
		{
			palette = NUMBONUSPALS - 1;
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
		//pal = (byte *)W_CacheLumpNum(lu_palette, PU_CACHE)+palette*768;
		//H_SetFilter(palette);
		CPlayer->plr->filter = H_GetFilterColor(palette);	// $democam
		//      I_SetPalette(pal);
	}
}

//---------------------------------------------------------------------------
//
// PROC DrawChain
//
//---------------------------------------------------------------------------

void DrawChain(void)
{
	int     chainY;
	float   healthPos;
	float   gemglow;

	int x, y, w, h;
	float cw;

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

		gemglow = healthPos / 100;
		chainY =
			(HealthMarker ==
			 CPlayer->plr->mo->health) ? 191 : 191 + ChainWiggle;

		// draw the chain

		x = 21;
		y = chainY;
		w = 271;
		h = 8;
		cw = (healthPos / 118) + 0.018f;

		GL_SetPatch(PatchCHAIN.lump);

		gl.TexParameter(DGL_WRAP_S, DGL_REPEAT);

		gl.Color4f(1, 1, 1, cfg.statusbarCounterAlpha);

		gl.Begin(DGL_QUADS);

		gl.TexCoord2f( 0 - cw, 0);
		gl.Vertex2f(x, y);

		gl.TexCoord2f( 0.916f - cw, 0);
		gl.Vertex2f(x + w, y);

		gl.TexCoord2f( 0.916f - cw, 1);
		gl.Vertex2f(x + w, y + h);

		gl.TexCoord2f( 0 - cw, 1);
		gl.Vertex2f(x, y + h);

		gl.End();

		// draw the life gem
		healthPos = (healthPos * 256) / 102;

		GL_DrawPatchLitAlpha( x + healthPos, chainY, 1, cfg.statusbarCounterAlpha, PatchLIFEGEM.lump);

		ShadeChain();

		// how about a glowing gem?
		gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_ONE);
		gl.Bind(Get(DD_DYNLIGHT_TEXTURE));

		GL_DrawRect(x + healthPos - 11, chainY - 6, 41, 24, 1, 0, 0, gemglow - (1 - cfg.statusbarCounterAlpha));

		gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_ONE_MINUS_SRC_ALPHA);
		gl.Color4f(1, 1, 1, 1);

		GL_Update(DDUF_STATBAR);
	}
}

//---------------------------------------------------------------------------
//
// PROC DrawWidgets
//
//---------------------------------------------------------------------------

void ST_drawWidgets(boolean refresh)
{
	int     x, i;
	int	temp;

	oldhealth = -1;
	if(!inventory)
	{
		oldarti = 0;
		// Draw all the counters

		// Frags
		if(deathmatch)
				STlib_updateNum(&w_frags, refresh);
		else
				STlib_updateNum(&w_health, refresh);

		// draw armor
		STlib_updateNum(&w_armor, refresh);

		// draw keys
		for(i = 0; i < 3; i++)
			STlib_updateBinIcon(&w_keyboxes[i], refresh);

		// draw ammo for current weapon
		temp = plyr->ammo[wpnlev1info[plyr->readyweapon].ammo];

		if( (oldammo != temp || oldweapon != plyr->readyweapon) && temp
				&& plyr->readyweapon > 0 && plyr->readyweapon < 7)
		{
			STlib_updateNum(&w_ready, refresh);
			STlib_updateMultIcon(&w_ammoicon, refresh);
		}

		// current artifact
		if(plyr->readyArtifact > 0){
			STlib_updateMultIcon(&w_artici, refresh);
			if(!ArtifactFlash && plyr->inventory[inv_ptr].count > 1)
				STlib_updateNum(&w_articount, refresh);
		}
	}
	else
	{	// Draw Inventory

		x = inv_ptr - curpos;

		for(i = 0; i < NUMVISINVSLOTS; i++){
			if( plyr->inventory[x + i].type != arti_none)
			{
				STlib_updateMultIcon(&w_invslot[i], refresh);

				if( plyr->inventory[x + i].count > 1)
					STlib_updateNum(&w_invslotcount[i], refresh);
			}
		}

		// Draw selector box
		GL_DrawPatch(ST_INVENTORYX + curpos * 31, 189, PatchSELECTBOX.lump);

		// Draw more left indicator
		if(x != 0)
			GL_DrawPatchLitAlpha(38, 159, 1, cfg.statusbarCounterAlpha, !(leveltime & 4) ? PatchINVLFGEM1.lump : PatchINVLFGEM2.lump);

		// Draw more right indicator
		if(CPlayer->inventorySlotNum - x > 7)
			GL_DrawPatchLitAlpha(269, 159, 1, cfg.statusbarCounterAlpha, !(leveltime & 4) ? PatchINVRTGEM1.lump : PatchINVRTGEM2.lump);
	}

	ST_drawIcons();
}

void ST_doFullscreenStuff(void)
{
	int     i;
	int     x;
	int     temp;
	float textalpha = hudalpha - ( 1 - cfg.hudColor[3]);
	float iconalpha = hudalpha - ( 1 - cfg.hudIconAlpha);

	GL_Update(DDUF_FULLSCREEN);
	if(cfg.hudShown[HUD_AMMO])
	{
		temp = CPlayer->ammo[wpnlev1info[CPlayer->readyweapon].ammo];
		if(CPlayer->readyweapon > 0 && CPlayer->readyweapon < 7)
		{
   		    Draw_BeginZoom(cfg.hudScale, 2, 2);
			GL_DrawPatchLitAlpha(-1, 0, 1, iconalpha,
						 W_GetNumForName(ammopic[CPlayer->readyweapon - 1]));
			DrINumber(temp, 18, 2, 1, 1, 1, textalpha);

                    Draw_EndZoom();
		}
		GL_Update(DDUF_TOP);
	}
   Draw_BeginZoom(cfg.hudScale, 2, 198);
	if(cfg.hudShown[HUD_HEALTH])
	{
		if(CPlayer->plr->mo->health > 0)
		{
			DrBNumber(CPlayer->plr->mo->health, 2, 180,
					  cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textalpha);
		}
		else
		{
			DrBNumber(0, 2, 180, cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textalpha);
		}
	}
	if(cfg.hudShown[HUD_ARMOR])
	{
		if(cfg.hudShown[HUD_HEALTH] && cfg.hudShown[HUD_KEYS]) temp = 158;
		else
		if(!cfg.hudShown[HUD_HEALTH] && cfg.hudShown[HUD_KEYS]) temp = 176;
		else
		if(cfg.hudShown[HUD_HEALTH] && !cfg.hudShown[HUD_KEYS]) temp = 168;
		else
		if(!cfg.hudShown[HUD_HEALTH] && !cfg.hudShown[HUD_KEYS]) temp = 186;

		DrINumber(CPlayer->armorpoints, 6, temp, 1, 1, 1, textalpha);
	}
	if(cfg.hudShown[HUD_KEYS])
	{
		x = 6;

		// Draw keys above health?
		if(CPlayer->keys[key_yellow])
		{
			GL_DrawPatchLitAlpha(x, cfg.hudShown[HUD_HEALTH]? 172 : 190, 1, iconalpha, W_GetNumForName("ykeyicon"));
			x += 11;
		}
		if(CPlayer->keys[key_green])
		{
			GL_DrawPatchLitAlpha(x, cfg.hudShown[HUD_HEALTH]? 172 : 190, 1, iconalpha, W_GetNumForName("gkeyicon"));
			x += 11;
		}
		if(CPlayer->keys[key_blue])
		{
			GL_DrawPatchLitAlpha(x, cfg.hudShown[HUD_HEALTH]? 172 : 190, 1, iconalpha, W_GetNumForName("bkeyicon"));
		}
	}
    Draw_EndZoom();

	if(deathmatch)
	{
		temp = 0;
		for(i = 0; i < MAXPLAYERS; i++)
		{
			if(players[i].plr->ingame)
			{
				temp += CPlayer->frags[i];
			}
		}
            Draw_BeginZoom(cfg.hudScale, 2, 198);
		DrINumber(temp, 45, 185, 1, 1, 1, textalpha);
            Draw_EndZoom();
	}
	if(!inventory)
	{
		if(cfg.hudShown[HUD_ARTI] && CPlayer->readyArtifact > 0)
		{
   		    Draw_BeginZoom(cfg.hudScale, 318, 198);

			GL_DrawPatchLitAlpha(286, 166, 1, iconalpha/2, W_GetNumForName("ARTIBOX"));
			GL_DrawPatchLitAlpha(286, 166, 1, iconalpha,
						 W_GetNumForName(artifactlist[CPlayer->readyArtifact + 5]));  //plus 5 for useartifact flashes
			DrSmallNumber(CPlayer->inventory[inv_ptr].count, 307, 188, 1, 1, 1, textalpha);
		    Draw_EndZoom();
		}
	}
	else
	{
        Draw_BeginZoom(cfg.hudScale, 160, 198);
		x = inv_ptr - curpos;
		for(i = 0; i < 7; i++)
		{
			GL_DrawPatchLitAlpha(50 + i * 31, 168, 1, iconalpha/2, W_GetNumForName("ARTIBOX"));
			if(CPlayer->inventorySlotNum > x + i &&
			   CPlayer->inventory[x + i].type != arti_none)
			{
				GL_DrawPatchLitAlpha(50 + i * 31, 168, 1, i==curpos? hudalpha : iconalpha,
							 W_GetNumForName(artifactlist[CPlayer->inventory[x + i].
											  type +5])); //plus 5 for useartifact flashes
				DrSmallNumber(CPlayer->inventory[x + i].count, 69 + i * 31, 190, 1, 1, 1, i==curpos? hudalpha : textalpha/2);
			}
		}
		GL_DrawPatchLitAlpha(50 + curpos * 31, 197, 1, hudalpha, PatchSELECTBOX.lump);
		if(x != 0)
		{
			GL_DrawPatchLitAlpha(38, 167, 1, iconalpha,
						 !(leveltime & 4) ? PatchINVLFGEM1.lump : PatchINVLFGEM2.lump);
		}
		if(CPlayer->inventorySlotNum - x > 7)
		{
			GL_DrawPatchLitAlpha(269, 167, 1, iconalpha,
						 !(leveltime & 4) ? PatchINVRTGEM1.lump : PatchINVRTGEM2.lump);
		}
        Draw_EndZoom();
	}
}

//--------------------------------------------------------------------------
//
// FUNC SB_Responder
//
//--------------------------------------------------------------------------

boolean ST_Responder(event_t *event)
{
	if(event->type == ev_keydown)
	{
		if(HandleCheats(event->data1))
		{						// Need to eat the key
			return (true);
		}
	}
	return (false);
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
	int     i;
	boolean eat;

	if(IS_NETGAME || gameskill == sk_nightmare)
	{							// Can't cheat in a net-game, or in nightmare mode
		return (false);
	}
	if(players[consoleplayer].health <= 0)
	{							// Dead players can't cheat
		return (false);
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
	return (eat);
}

//--------------------------------------------------------------------------
//
// FUNC CheatAddkey
//
// Returns true if the added key completed the cheat, false otherwise.
//
//--------------------------------------------------------------------------

static boolean CheatAddKey(Cheat_t * cheat, byte key, boolean *eat)
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
		return (true);
	}
	return (false);
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

static void CheatGodFunc(player_t *player, Cheat_t * cheat)
{
	player->cheats ^= CF_GODMODE;
	player->update |= PSF_STATE;
	if(player->cheats & CF_GODMODE)
	{
		P_SetMessage(player, TXT_CHEATGODON);
	}
	else
	{
		P_SetMessage(player, TXT_CHEATGODOFF);
	}
}

static void CheatNoClipFunc(player_t *player, Cheat_t * cheat)
{
	player->cheats ^= CF_NOCLIP;
	player->update |= PSF_STATE;
	if(player->cheats & CF_NOCLIP)
	{
		P_SetMessage(player, TXT_CHEATNOCLIPON);
	}
	else
	{
		P_SetMessage(player, TXT_CHEATNOCLIPOFF);
	}
}

static void CheatWeaponsFunc(player_t *player, Cheat_t * cheat)
{
	int     i;

	player->update |=
		PSF_ARMOR_POINTS | PSF_STATE | PSF_MAX_AMMO | PSF_AMMO |
		PSF_OWNED_WEAPONS;

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
	for(i = 0; i < NUMWEAPONS - 1; i++)
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
	P_SetMessage(player, TXT_CHEATWEAPONS);
}

static void CheatPowerFunc(player_t *player, Cheat_t * cheat)
{
	player->update |= PSF_POWERS;
	if(player->powers[pw_weaponlevel2])
	{
		player->powers[pw_weaponlevel2] = 0;
		P_SetMessage(player, TXT_CHEATPOWEROFF);
	}
	else
	{
		P_UseArtifact(player, arti_tomeofpower);
		P_SetMessage(player, TXT_CHEATPOWERON);
	}
}

static void CheatHealthFunc(player_t *player, Cheat_t * cheat)
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
	P_SetMessage(player, TXT_CHEATHEALTH);
}

static void CheatKeysFunc(player_t *player, Cheat_t * cheat)
{
	extern int playerkeys;

	player->update |= PSF_KEYS;
	player->keys[key_yellow] = true;
	player->keys[key_green] = true;
	player->keys[key_blue] = true;
	playerkeys = 7;				// Key refresh flags
	P_SetMessage(player, TXT_CHEATKEYS);
}

static void CheatSoundFunc(player_t *player, Cheat_t * cheat)
{
	/*  DebugSound = !DebugSound;
	   if(DebugSound)
	   {
	   P_SetMessage(player, TXT_CHEATSOUNDON);
	   }
	   else
	   {
	   P_SetMessage(player, TXT_CHEATSOUNDOFF);
	   } */
}

static void CheatTickerFunc(player_t *player, Cheat_t * cheat)
{								/* 
								   extern int DisplayTicker;

								   DisplayTicker = !DisplayTicker;
								   if(DisplayTicker)
								   {
								   P_SetMessage(player, TXT_CHEATTICKERON);
								   }
								   else
								   {
								   P_SetMessage(player, TXT_CHEATTICKEROFF);
								   } */
}

static void CheatArtifact1Func(player_t *player, Cheat_t * cheat)
{
	P_SetMessage(player, TXT_CHEATARTIFACTS1);
}

static void CheatArtifact2Func(player_t *player, Cheat_t * cheat)
{
	P_SetMessage(player, TXT_CHEATARTIFACTS2);
}

static void CheatArtifact3Func(player_t *player, Cheat_t * cheat)
{
	int     i;
	int     j;
	artitype_t type;
	int     count;

	type = cheat->args[0] - 'a' + 1;
	count = cheat->args[1] - '0';
	if(type == 26 && count == 0)
	{							// All artifacts
		for(i = arti_none + 1; i < NUMARTIFACTS; i++)
		{
			if(shareware && (i == arti_superhealth || i == arti_teleport))
			{
				continue;
			}
			for(j = 0; j < 16; j++)
			{
				P_GiveArtifact(player, i, NULL);
			}
		}
		P_SetMessage(player, TXT_CHEATARTIFACTS3);
	}
	else if(type > arti_none && type < NUMARTIFACTS && count > 0 && count < 10)
	{
		if(shareware && (type == arti_superhealth || type == arti_teleport))
		{
			P_SetMessage(player, TXT_CHEATARTIFACTSFAIL);
			return;
		}
		for(i = 0; i < count; i++)
		{
			P_GiveArtifact(player, type, NULL);
		}
		P_SetMessage(player, TXT_CHEATARTIFACTS3);
	}
	else
	{							// Bad input
		P_SetMessage(player, TXT_CHEATARTIFACTSFAIL);
	}
}

static void CheatWarpFunc(player_t *player, Cheat_t * cheat)
{
	int     episode;
	int     map;

	episode = cheat->args[0] - '0';
	map = cheat->args[1] - '0';
	if(M_ValidEpisodeMap(episode, map))
	{
		G_DeferedInitNew(gameskill, episode, map);
		P_SetMessage(player, TXT_CHEATWARP);
	}
}

static void CheatChickenFunc(player_t *player, Cheat_t * cheat)
{
	extern boolean P_UndoPlayerChicken(player_t *player);

	if(player->chickenTics)
	{
		if(P_UndoPlayerChicken(player))
		{
			P_SetMessage(player, TXT_CHEATCHICKENOFF);
		}
	}
	else if(P_ChickenMorphPlayer(player))
	{
		P_SetMessage(player, TXT_CHEATCHICKENON);
	}
}

static void CheatMassacreFunc(player_t *player, Cheat_t * cheat)
{
	P_Massacre();
	P_SetMessage(player, TXT_CHEATMASSACRE);
}

static void CheatIDKFAFunc(player_t *player, Cheat_t * cheat)
{
	int     i;

	if(player->chickenTics)
	{
		return;
	}
	for(i = 1; i < 8; i++)
	{
		player->weaponowned[i] = false;
	}
	player->pendingweapon = wp_staff;
	P_SetMessage(player, TXT_CHEATIDKFA);
}

static void CheatIDDQDFunc(player_t *player, Cheat_t * cheat)
{
	P_DamageMobj(player->plr->mo, NULL, player->plr->mo, 10000);
	P_SetMessage(player, TXT_CHEATIDDQD);
}

//===========================================================================
//
// Console Commands
//
//===========================================================================

// This is the multipurpose cheat ccmd.
int CCmdCheat(int argc, char **argv)
{
	unsigned int i;

	if(argc != 2)
	{
		// Usage information.
		Con_Printf("Usage: cheat (cheat)\nFor example, 'cheat engage21'.\n");
		return true;
	}
	// Give each of the characters in argument two to the SB event handler.
	for(i = 0; i < strlen(argv[1]); i++)
	{
		event_t ev;

		ev.type = ev_keydown;
		ev.data1 = argv[1][i];
		ev.data2 = ev.data3 = 0;
		ST_Responder(&ev);
	}
	return true;
}

static boolean canCheat()
{
	if(IS_NETGAME && !IS_CLIENT && netSvAllowCheats)
		return true;
	return !(gameskill == sk_nightmare || (IS_NETGAME /*&& !netcheat */ )
			 || players[consoleplayer].health <= 0);
}

int CCmdCheatGod(int argc, char **argv)
{
	if(IS_NETGAME)
	{
		NetCl_CheatRequest("god");
		return true;
	}
	if(!canCheat())
		return false;			// Can't cheat!
	CheatGodFunc(players + consoleplayer, NULL);
	return true;
}

int CCmdCheatClip(int argc, char **argv)
{
	if(IS_NETGAME)
	{
		NetCl_CheatRequest("noclip");
		return true;
	}
	if(!canCheat())
		return false;			// Can't cheat!
	CheatNoClipFunc(players + consoleplayer, NULL);
	return true;
}

int CCmdCheatGive(int argc, char **argv)
{
	int     tellUsage = false;
	int     target = consoleplayer;

	if(IS_CLIENT)
	{
		char    buf[100];

		if(argc != 2)
			return false;
		sprintf(buf, "give %s", argv[1]);
		NetCl_CheatRequest(buf);
		return true;
	}

	if(!canCheat())
		return false;			// Can't cheat!

	// Check the arguments.
	if(argc == 3)
	{
		target = atoi(argv[2]);
		if(target < 0 || target >= MAXPLAYERS || !players[target].plr->ingame)
			return false;
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
	int     num;

	if(!canCheat())
		return false;			// Can't cheat!
	if(argc == 2)
	{
		num = atoi(argv[1]);
		cheat.args[0] = num / 10 + '0';
		cheat.args[1] = num % 10 + '0';
	}
	else if(argc == 3)
	{
		cheat.args[0] = atoi(argv[1]) % 10 + '0';
		cheat.args[1] = atoi(argv[2]) % 10 + '0';
	}
	else
	{
		Con_Printf("Usage: warp (num)\n");
		return true;
	}
	// We don't want that keys are repeated while we wait.
	DD_ClearKeyRepeaters();
	CheatWarpFunc(players + consoleplayer, &cheat);
	return true;
}

int CCmdCheatPig(int argc, char **argv)
{
	if(!canCheat())
		return false;			// Can't cheat!
	CheatChickenFunc(players + consoleplayer, NULL);
	return true;
}

int CCmdCheatMassacre(int argc, char **argv)
{
	if(!canCheat())
		return false;			// Can't cheat!
	DD_ClearKeyRepeaters();
	CheatMassacreFunc(players + consoleplayer, NULL);
	return true;
}

int CCmdCheatWhere(int argc, char **argv)
{
	/*  if(!canCheat()) return false; // Can't cheat!
	   CheatDebugFunc(players+consoleplayer, NULL); */
	return true;
}

int CCmdCheatReveal(int argc, char **argv)
{
	int     option;
	extern int cheating;

	if(!canCheat())
		return false;			// Can't cheat!
	if(argc != 2)
	{
		Con_Printf("Usage: reveal (0-3)\n");
		Con_Printf("0=nothing, 1=show unseen, 2=full map, 3=map+things\n");
		return true;
	}
	// Reset them (for 'nothing'). :-)
	cheating = 0;
	players[consoleplayer].powers[pw_allmap] = false;
	option = atoi(argv[1]);
	if(option < 0 || option > 3)
		return false;
	if(option == 1)
		players[consoleplayer].powers[pw_allmap] = true;
	else if(option == 2)
		cheating = 1;
	else if(option == 3)
		cheating = 2;
	return true;
}
