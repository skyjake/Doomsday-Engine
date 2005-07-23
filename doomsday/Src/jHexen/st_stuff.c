
//**************************************************************************
//**
//** st_stuff.c
//** Based on sb_bar.c : Heretic 2 : Raven Software, Corp.
//**
//** $RCSfile$
//** $Revision$
//** $Date$
//** $Author$
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include "jHexen/h2def.h"
#include "jHexen/p_local.h"
#include "jHexen/soundst.h"
#include "Common/hu_stuff.h"
#include "jHexen/st_stuff.h"
#include "x_config.h"
#include "Common/st_lib.h"
#include "d_net.h"

#ifdef DEMOCAM
#  include "g_demo.h"
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
typedef enum {
	CHT_GOD,
	CHT_NOCLIP,
	CHT_WEAPONS,
	CHT_HEALTH,
	CHT_KEYS,
	CHT_ARTIFACTS,
	CHT_PUZZLE
} cheattype_t;

// inventory
#define ST_INVENTORYX			50
#define ST_INVENTORYY			163

// how many inventory slots are visible
#define NUMVISINVSLOTS 7

// invslot artifact count (relative to each slot)
#define ST_INVCOUNTOFFX			30
#define ST_INVCOUNTOFFY			22

// current artifact (sbbar)
#define ST_ARTIFACTWIDTH	24
#define ST_ARTIFACTX			143
#define ST_ARTIFACTY			163

// current artifact count (sbar)
#define ST_ARTIFACTCWIDTH	2
#define ST_ARTIFACTCX			174
#define ST_ARTIFACTCY			184

// HEALTH number pos.
#define ST_HEALTHWIDTH		3
#define ST_HEALTHX			64
#define ST_HEALTHY			176

// MANA A
#define ST_MANAAWIDTH		3
#define ST_MANAAX			91
#define ST_MANAAY			181

// MANA A ICON
#define ST_MANAAICONX			77
#define ST_MANAAICONY			164

// MANA A VIAL
#define ST_MANAAVIALX			94
#define ST_MANAAVIALY			164

// MANA B
#define ST_MANABWIDTH		3
#define ST_MANABX			123
#define ST_MANABY			181

// MANA B ICON
#define ST_MANABICONX			110
#define ST_MANABICONY			164

// MANA B VIAL
#define ST_MANABVIALX			102
#define ST_MANABVIALY			164

// ARMOR number pos.
#define ST_ARMORWIDTH		2
#define ST_ARMORX			274
#define ST_ARMORY			176

// Frags pos.
#define ST_FRAGSWIDTH		3
#define ST_FRAGSX			64
#define ST_FRAGSY			176

// TYPES -------------------------------------------------------------------

typedef struct Cheat_s {
	void    (*func) (player_t *player, struct Cheat_s * cheat);
	byte   *sequence;
	byte   *pos;
	int     args[2];
	int     currentArg;
} Cheat_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void DrINumber(signed int val, int x, int y, float r, float g, float b, float a);
static void DrBNumber(signed int val, int x, int y, float Red, float Green, float Blue, float Alpha);
static void DrawChain(void);
void ST_drawWidgets(boolean refresh);
static void DrawKeyBar(void);
static void DrawWeaponPieces(void);
static void ST_doFullscreenStuff(void);
static void DrawAnimatedIcons(void);
static boolean HandleCheats(byte key);
static boolean CheatAddKey(Cheat_t * cheat, byte key, boolean *eat);
static void CheatGodFunc(player_t *player, Cheat_t * cheat);
static void CheatNoClipFunc(player_t *player, Cheat_t * cheat);
static void CheatWeaponsFunc(player_t *player, Cheat_t * cheat);
static void CheatHealthFunc(player_t *player, Cheat_t * cheat);
static void CheatKeysFunc(player_t *player, Cheat_t * cheat);
static void CheatSoundFunc(player_t *player, Cheat_t * cheat);
static void CheatTickerFunc(player_t *player, Cheat_t * cheat);
static void CheatArtifactAllFunc(player_t *player, Cheat_t * cheat);
static void CheatPuzzleFunc(player_t *player, Cheat_t * cheat);
static void CheatWarpFunc(player_t *player, Cheat_t * cheat);
static void CheatPigFunc(player_t *player, Cheat_t * cheat);
static void CheatMassacreFunc(player_t *player, Cheat_t * cheat);
static void CheatIDKFAFunc(player_t *player, Cheat_t * cheat);
static void CheatQuickenFunc1(player_t *player, Cheat_t * cheat);
static void CheatQuickenFunc2(player_t *player, Cheat_t * cheat);
static void CheatQuickenFunc3(player_t *player, Cheat_t * cheat);
static void CheatClassFunc1(player_t *player, Cheat_t * cheat);
static void CheatClassFunc2(player_t *player, Cheat_t * cheat);
static void CheatInitFunc(player_t *player, Cheat_t * cheat);
static void CheatInitFunc(player_t *player, Cheat_t * cheat);
static void CheatVersionFunc(player_t *player, Cheat_t * cheat);
static void CheatDebugFunc(player_t *player, Cheat_t * cheat);
static void CheatScriptFunc1(player_t *player, Cheat_t * cheat);
static void CheatScriptFunc2(player_t *player, Cheat_t * cheat);
static void CheatScriptFunc3(player_t *player, Cheat_t * cheat);
static void CheatRevealFunc(player_t *player, Cheat_t * cheat);
static void CheatTrackFunc1(player_t *player, Cheat_t * cheat);
static void CheatTrackFunc2(player_t *player, Cheat_t * cheat);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

//extern    sfxinfo_t   S_sfx[];

extern byte *memscreen;
extern int ArmorIncrement[NUMCLASSES][NUMARMOR];
extern int AutoArmorSave[NUMCLASSES];

extern boolean automapactive;

// PUBLIC DATA DECLARATIONS ------------------------------------------------

int     DebugSound;				// Debug flag for displaying sound info
boolean inventory;
int     curpos;
int     inv_ptr;
int     ArtifactFlash;

// ST_Start() has just been called
static boolean st_firsttime;

// fullscreen hud alpha value
static float hudalpha = 0.0f;

// slide statusbar amount 1.0 is fully open
static float showbar = 0.0f;

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

// current armor level
static int armorlevel = 0;

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

// current mana A icon index. 0 = none
static int st_manaAicon = 0;

// current mana B icon index. 0 = none
static int st_manaBicon = 0;

// current mana A vial index. 0 = none
static int st_manaAvial = 0;

// current mana B vial index. 0 = none
static int st_manaBvial = 0;

// current mana A icon widget
static st_multicon_t w_manaAicon;

// current mana B icon widget
static st_multicon_t w_manaBicon;

// current mana A vial widget
static st_multicon_t w_manaAvial;

// current mana B vial widget
static st_multicon_t w_manaBvial;

// health widget
static st_number_t w_health;

// in deathmatch only, summary of frags stats
static st_number_t w_frags;

// armor widget
static st_number_t w_armor;

// current mana level
static int manaACount = 0;
static int manaBCount = 0;

// mana A counter widget
static st_number_t w_manaACount;

// mana B widget
static st_number_t w_manaBCount;

// used by the mana glympf and vials
//static int     manaPatchNum1, manaPatchNum2, manaVialPatchNum1, manaVialPatchNum2;

 // number of frags so far in deathmatch
static int st_fragscount;

// !deathmatch
static boolean st_fragson;


/*#ifndef __WATCOMC__
   boolean i_CDMusic; // in Watcom, defined in i_ibm
   #endif */

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// whether to use alpha blending
static boolean st_blended = false;

static byte CheatLookup[256];
static int HealthMarker;
static player_t *CPlayer;


static int FontBNumBase;
int lu_palette;

static int oldarti = 0;
static int oldartiCount = 0;

static int oldhealth = -1;

/*
   static int oldarti = 0;
   static int oldartiCount = 0;
   static int oldfrags = -9999;
   static int oldmana1 = -1;
   static int oldmana2 = -1;
   static int oldarmor = -1;
   static int oldlife = -1;
   static int oldpieces = -1;
   static int oldweapon = -1;
   static int oldkeys = -1;
 */

static dpatch_t PatchNumH2BAR;
static dpatch_t PatchNumH2TOP;
static dpatch_t PatchNumLFEDGE;
static dpatch_t PatchNumRTEDGE;
static dpatch_t PatchNumKILLS;
//static dpatch_t PatchNumMANAVIAL1;
//static dpatch_t PatchNumMANAVIAL2;
//static dpatch_t PatchNumMANAVIALDIM1;
//static dpatch_t PatchNumMANAVIALDIM2;
static dpatch_t PatchNumMANADIM1;
static dpatch_t PatchNumMANADIM2;
static dpatch_t PatchNumMANABRIGHT1;
static dpatch_t PatchNumMANABRIGHT2;
static dpatch_t PatchNumCHAIN;
static dpatch_t PatchNumSTATBAR;
static dpatch_t PatchNumKEYBAR;
static dpatch_t PatchNumSELECTBOX;
static dpatch_t PatchNumINumbers[10];
static dpatch_t PatchNumNEGATIVE;
static dpatch_t PatchNumSmNumbers[10];
static dpatch_t PatchMANAAVIALS[2];
static dpatch_t PatchMANABVIALS[2];
static dpatch_t PatchMANAAICONS[2];
static dpatch_t PatchMANABICONS[2];
static dpatch_t PatchNumINVBAR;
static dpatch_t PatchNumWEAPONSLOT;
static dpatch_t PatchNumWEAPONFULL;
static dpatch_t PatchNumPIECE1;
static dpatch_t PatchNumPIECE2;
static dpatch_t PatchNumPIECE3;
static dpatch_t PatchNumINVLFGEM1;
static dpatch_t PatchNumINVLFGEM2;
static dpatch_t PatchNumINVRTGEM1;
static dpatch_t PatchNumINVRTGEM2;

static dpatch_t     PatchARTIFACTS[38];

static dpatch_t SpinFlylump;
static dpatch_t SpinMinotaurLump;
static dpatch_t SpinSpeedLump;
static dpatch_t SpinDefenseLump;

static int PatchNumLIFEGEM;

char    artifactlist[][10] = {
	{"USEARTIA"},				// use artifact flash
	{"USEARTIB"},
	{"USEARTIC"},
	{"USEARTID"},
	{"USEARTIE"},
	{"ARTIBOX"},				// none
	{"ARTIINVU"},				// invulnerability
	{"ARTIPTN2"},				// health
	{"ARTISPHL"},				// superhealth
	{"ARTIHRAD"},				// healing radius
	{"ARTISUMN"},				// summon maulator
	{"ARTITRCH"},				// torch
	{"ARTIPORK"},				// egg
	{"ARTISOAR"},				// fly
	{"ARTIBLST"},				// blast radius
	{"ARTIPSBG"},				// poison bag
	{"ARTITELO"},				// teleport other
	{"ARTISPED"},				// speed
	{"ARTIBMAN"},				// boost mana
	{"ARTIBRAC"},				// boost armor
	{"ARTIATLP"},				// teleport
	{"ARTISKLL"},				// arti_puzzskull
	{"ARTIBGEM"},				// arti_puzzgembig
	{"ARTIGEMR"},				// arti_puzzgemred
	{"ARTIGEMG"},				// arti_puzzgemgreen1
	{"ARTIGMG2"},				// arti_puzzgemgreen2
	{"ARTIGEMB"},				// arti_puzzgemblue1
	{"ARTIGMB2"},				// arti_puzzgemblue2
	{"ARTIBOK1"},				// arti_puzzbook1
	{"ARTIBOK2"},				// arti_puzzbook2
	{"ARTISKL2"},				// arti_puzzskull2
	{"ARTIFWEP"},				// arti_puzzfweapon
	{"ARTICWEP"},				// arti_puzzcweapon
	{"ARTIMWEP"},				// arti_puzzmweapon
	{"ARTIGEAR"},				// arti_puzzgear1
	{"ARTIGER2"},				// arti_puzzgear2
	{"ARTIGER3"},				// arti_puzzgear3
	{"ARTIGER4"},				// arti_puzzgear4
};

// Toggle god mode
static byte CheatGodSeq[] = {
	CHEAT_ENCRYPT('s'),
	CHEAT_ENCRYPT('a'),
	CHEAT_ENCRYPT('t'),
	CHEAT_ENCRYPT('a'),
	CHEAT_ENCRYPT('n'),
	0xff
};

// Toggle no clipping mode
static byte CheatNoClipSeq[] = {
	CHEAT_ENCRYPT('c'),
	CHEAT_ENCRYPT('a'),
	CHEAT_ENCRYPT('s'),
	CHEAT_ENCRYPT('p'),
	CHEAT_ENCRYPT('e'),
	CHEAT_ENCRYPT('r'),
	0xff
};

// Get all weapons and mana
static byte CheatWeaponsSeq[] = {
	CHEAT_ENCRYPT('n'),
	CHEAT_ENCRYPT('r'),
	CHEAT_ENCRYPT('a'),
	0xff
};

// Get full health
static byte CheatHealthSeq[] = {
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
static byte CheatKeysSeq[] = {
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

// Get all artifacts
static byte CheatArtifactAllSeq[] = {
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
static byte CheatPuzzleSeq[] = {
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
static byte CheatWarpSeq[] = {
	CHEAT_ENCRYPT('v'),
	CHEAT_ENCRYPT('i'),
	CHEAT_ENCRYPT('s'),
	CHEAT_ENCRYPT('i'),
	CHEAT_ENCRYPT('t'),
	0, 0, 0xff, 0
};

// Become a pig
static byte CheatPigSeq[] = {
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
static byte CheatMassacreSeq[] = {
	CHEAT_ENCRYPT('b'),
	CHEAT_ENCRYPT('u'),
	CHEAT_ENCRYPT('t'),
	CHEAT_ENCRYPT('c'),
	CHEAT_ENCRYPT('h'),
	CHEAT_ENCRYPT('e'),
	CHEAT_ENCRYPT('r'),
	0xff, 0
};

static byte CheatIDKFASeq[] = {
	CHEAT_ENCRYPT('c'),
	CHEAT_ENCRYPT('o'),
	CHEAT_ENCRYPT('n'),
	CHEAT_ENCRYPT('a'),
	CHEAT_ENCRYPT('n'),
	0xff, 0
};

static byte CheatQuickenSeq1[] = {
	CHEAT_ENCRYPT('m'),
	CHEAT_ENCRYPT('a'),
	CHEAT_ENCRYPT('r'),
	CHEAT_ENCRYPT('t'),
	CHEAT_ENCRYPT('e'),
	CHEAT_ENCRYPT('k'),
	0xff, 0
};

static byte CheatQuickenSeq2[] = {
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

static byte CheatQuickenSeq3[] = {
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
static byte CheatClass1Seq[] = {
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

static byte CheatClass2Seq[] = {
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

static byte CheatInitSeq[] = {
	CHEAT_ENCRYPT('i'),
	CHEAT_ENCRYPT('n'),
	CHEAT_ENCRYPT('i'),
	CHEAT_ENCRYPT('t'),
	0xff, 0
};

static byte CheatVersionSeq[] = {
	CHEAT_ENCRYPT('m'),
	CHEAT_ENCRYPT('r'),
	CHEAT_ENCRYPT('j'),
	CHEAT_ENCRYPT('o'),
	CHEAT_ENCRYPT('n'),
	CHEAT_ENCRYPT('e'),
	CHEAT_ENCRYPT('s'),
	0xff, 0
};

static byte CheatDebugSeq[] = {
	CHEAT_ENCRYPT('w'),
	CHEAT_ENCRYPT('h'),
	CHEAT_ENCRYPT('e'),
	CHEAT_ENCRYPT('r'),
	CHEAT_ENCRYPT('e'),
	0xff, 0
};

static byte CheatScriptSeq1[] = {
	CHEAT_ENCRYPT('p'),
	CHEAT_ENCRYPT('u'),
	CHEAT_ENCRYPT('k'),
	CHEAT_ENCRYPT('e'),
	0xff, 0
};

static byte CheatScriptSeq2[] = {
	CHEAT_ENCRYPT('p'),
	CHEAT_ENCRYPT('u'),
	CHEAT_ENCRYPT('k'),
	CHEAT_ENCRYPT('e'),
	0, 0xff, 0
};

static byte CheatScriptSeq3[] = {
	CHEAT_ENCRYPT('p'),
	CHEAT_ENCRYPT('u'),
	CHEAT_ENCRYPT('k'),
	CHEAT_ENCRYPT('e'),
	0, 0, 0xff,
};

static byte CheatRevealSeq[] = {
	CHEAT_ENCRYPT('m'),
	CHEAT_ENCRYPT('a'),
	CHEAT_ENCRYPT('p'),
	CHEAT_ENCRYPT('s'),
	CHEAT_ENCRYPT('c'),
	CHEAT_ENCRYPT('o'),
	0xff, 0
};

static byte CheatTrackSeq1[] = {
	//CHEAT_ENCRYPT('`'),
	CHEAT_ENCRYPT('c'),
	CHEAT_ENCRYPT('d'),
	CHEAT_ENCRYPT('t'),
	0xff, 0
};

static byte CheatTrackSeq2[] = {
	//CHEAT_ENCRYPT('`'),
	CHEAT_ENCRYPT('c'),
	CHEAT_ENCRYPT('d'),
	CHEAT_ENCRYPT('t'),
	0, 0, 0xff, 0
};

static Cheat_t Cheats[] = {
	{CheatTrackFunc1, CheatTrackSeq1, NULL, {0, 0}, 0},
	{CheatTrackFunc2, CheatTrackSeq2, NULL, {0, 0}, 0},
	{CheatGodFunc, CheatGodSeq, NULL, {0, 0}, 0},
	{CheatNoClipFunc, CheatNoClipSeq, NULL, {0, 0}, 0},
	{CheatWeaponsFunc, CheatWeaponsSeq, NULL, {0, 0}, 0},
	{CheatHealthFunc, CheatHealthSeq, NULL, {0, 0}, 0},
	{CheatKeysFunc, CheatKeysSeq, NULL, {0, 0}, 0},
	{CheatSoundFunc, CheatSoundSeq, NULL, {0, 0}, 0},
	{CheatTickerFunc, CheatTickerSeq, NULL, {0, 0}, 0},
	{CheatArtifactAllFunc, CheatArtifactAllSeq, NULL, {0, 0}, 0},
	{CheatPuzzleFunc, CheatPuzzleSeq, NULL, {0, 0}, 0},
	{CheatWarpFunc, CheatWarpSeq, NULL, {0, 0}, 0},
	{CheatPigFunc, CheatPigSeq, NULL, {0, 0}, 0},
	{CheatMassacreFunc, CheatMassacreSeq, NULL, {0, 0}, 0},
	{CheatIDKFAFunc, CheatIDKFASeq, NULL, {0, 0}, 0},
	{CheatQuickenFunc1, CheatQuickenSeq1, NULL, {0, 0}, 0},
	{CheatQuickenFunc2, CheatQuickenSeq2, NULL, {0, 0}, 0},
	{CheatQuickenFunc3, CheatQuickenSeq3, NULL, {0, 0}, 0},
	{CheatClassFunc1, CheatClass1Seq, NULL, {0, 0}, 0},
	{CheatClassFunc2, CheatClass2Seq, NULL, {0, 0}, 0},
	{CheatInitFunc, CheatInitSeq, NULL, {0, 0}, 0},
	{CheatVersionFunc, CheatVersionSeq, NULL, {0, 0}, 0},
	{CheatDebugFunc, CheatDebugSeq, NULL, {0, 0}, 0},
	{CheatScriptFunc1, CheatScriptSeq1, NULL, {0, 0}, 0},
	{CheatScriptFunc2, CheatScriptSeq2, NULL, {0, 0}, 0},
	{CheatScriptFunc3, CheatScriptSeq3, NULL, {0, 0}, 0},
	{CheatRevealFunc, CheatRevealSeq, NULL, {0, 0}, 0},
	{NULL, NULL, NULL, {0, 0}, 0}	// Terminator
};

// CODE --------------------------------------------------------------------

void ST_loadGraphics(void)
{
	int     i;

	char    namebuf[9];

	FontBNumBase = W_GetNumForName("FONTB16");	// to be removed

	R_CachePatch(&PatchNumH2BAR, "H2BAR");
	R_CachePatch(&PatchNumH2TOP, "H2TOP");
	R_CachePatch(&PatchNumINVBAR, "INVBAR");
	R_CachePatch(&PatchNumLFEDGE, "LFEDGE");
	R_CachePatch(&PatchNumRTEDGE, "RTEDGE");
	R_CachePatch(&PatchNumSTATBAR, "STATBAR");
	R_CachePatch(&PatchNumKEYBAR, "KEYBAR");
	R_CachePatch(&PatchNumSELECTBOX, "SELECTBOX");

	R_CachePatch(&PatchMANAAVIALS[0], "MANAVL1D");
	R_CachePatch(&PatchMANABVIALS[0], "MANAVL2D");
	R_CachePatch(&PatchMANAAVIALS[1], "MANAVL1");
	R_CachePatch(&PatchMANABVIALS[1], "MANAVL2");

	R_CachePatch(&PatchMANAAICONS[0], "MANADIM1");
	R_CachePatch(&PatchMANABICONS[0], "MANADIM2");
	R_CachePatch(&PatchMANAAICONS[1], "MANABRT1");
	R_CachePatch(&PatchMANABICONS[1], "MANABRT2");

	R_CachePatch(&PatchNumINVLFGEM1, "invgeml1");
	R_CachePatch(&PatchNumINVLFGEM2, "invgeml2");
	R_CachePatch(&PatchNumINVRTGEM1, "invgemr1");
	R_CachePatch(&PatchNumINVRTGEM2, "invgemr2");
	R_CachePatch(&PatchNumNEGATIVE, "NEGNUM");
	R_CachePatch(&PatchNumKILLS, "KILLS");
	R_CachePatch(&SpinFlylump, "SPFLY0");
	R_CachePatch(&SpinMinotaurLump, "SPMINO0");
	R_CachePatch(&SpinSpeedLump, "SPBOOT0");
	R_CachePatch(&SpinDefenseLump, "SPSHLD0");

	for(i = 0; i < 10; i++)
	{
		sprintf(namebuf, "IN%d", i);
		R_CachePatch(&PatchNumINumbers[i], namebuf);
	}

	for(i = 0; i < 10; i++)
	{
		sprintf(namebuf, "SMALLIN%d", i);
		R_CachePatch(&PatchNumSmNumbers[i], namebuf);
	}

	// artifact icons (+5 for the use-artifact flash patches)
	for(i = 0; i < (NUMARTIFACTS + 5); i++)
	{
		sprintf(namebuf, "%s", artifactlist[i]);
		R_CachePatch(&PatchARTIFACTS[i], namebuf);
	}

}

//==========================================================================
//
// SB_Init
//
//==========================================================================

void ST_loadData(void)
{
	int     i;

	lu_palette = W_GetNumForName("PLAYPAL");

	for(i = 0; i < 256; i++)
	{
		CheatLookup[i] = CHEAT_ENCRYPT(i);
	}

	SB_SetClassData();

	ST_loadGraphics();
}

void ST_initData(void)
{
	int i;

	st_firsttime = true;
	plyr = &players[consoleplayer];

	st_clock = 0;
	st_chatstate = StartChatState;
	st_gamestate = FirstPersonState;

	st_statusbaron = true;
	st_oldchat = st_chat = false;
	st_cursoron = false;

	for(i = 0; i < NUMVISINVSLOTS; i++)
	{
		st_invslot[i] = 0;
		st_invslotcount[i] = 0;
	}

	STlib_init();
}

void ST_createWidgets(void)
{
	int i, width, temp;

	// health num
	STlib_initNum(&w_health, ST_HEALTHX, ST_HEALTHY, PatchNumINumbers,
					  &plyr->health, &st_statusbaron, ST_HEALTHWIDTH, &cfg.statusbarCounterAlpha);

	// frags sum
	STlib_initNum(&w_frags, ST_FRAGSX, ST_FRAGSY, PatchNumINumbers, &st_fragscount,
				  &st_fragson, ST_FRAGSWIDTH, &cfg.statusbarCounterAlpha);

	// armor num - should be colored later
	STlib_initNum(&w_armor, ST_ARMORX, ST_ARMORY, PatchNumINumbers,
					  &armorlevel, &st_statusbaron, ST_ARMORWIDTH, &cfg.statusbarCounterAlpha );

	// manaA count
	STlib_initNum(&w_manaACount, ST_MANAAX, ST_MANAAY, PatchNumSmNumbers,
					  &manaACount, &st_statusbaron, ST_MANAAWIDTH, &cfg.statusbarCounterAlpha);

	// manaB count
	STlib_initNum(&w_manaBCount, ST_MANABX, ST_MANABY, PatchNumSmNumbers,
					  &manaBCount, &st_statusbaron, ST_MANABWIDTH, &cfg.statusbarCounterAlpha);

	// current mana A icon
	STlib_initMultIcon(&w_manaAicon, ST_MANAAICONX, ST_MANAAICONY, PatchMANAAICONS, &st_manaAicon,
					   &st_statusbaron, &cfg.statusbarCounterAlpha);

	// current mana B icon
	STlib_initMultIcon(&w_manaBicon, ST_MANABICONX, ST_MANABICONY, PatchMANABICONS, &st_manaBicon,
					   &st_statusbaron, &cfg.statusbarCounterAlpha);

	// current mana A vial
	STlib_initMultIcon(&w_manaAvial, ST_MANAAVIALX, ST_MANAAVIALY, PatchMANAAVIALS, &st_manaAvial,
					   &st_statusbaron, &cfg.statusbarCounterAlpha);

	// current mana B vial
	STlib_initMultIcon(&w_manaBvial, ST_MANABVIALX, ST_MANABVIALY, PatchMANABVIALS, &st_manaBvial,
					   &st_statusbaron, &cfg.statusbarCounterAlpha);

	// current artifact (stbar not inventory)
	STlib_initMultIcon(&w_artici, ST_ARTIFACTX, ST_ARTIFACTY, PatchARTIFACTS, &st_artici,
					   &st_statusbaron, &cfg.statusbarCounterAlpha);

	// current artifact count
	STlib_initNum(&w_articount, ST_ARTIFACTCX, ST_ARTIFACTCY, PatchNumSmNumbers,
					  &oldartiCount, &st_statusbaron, ST_ARTIFACTCWIDTH, &cfg.statusbarCounterAlpha);

	// inventory slots
	width = PatchARTIFACTS[5].width + 1;
	temp = 0;

	for (i = 0; i < NUMVISINVSLOTS; i++){
		// inventory slot icon
		STlib_initMultIcon(&w_invslot[i], ST_INVENTORYX + temp , ST_INVENTORYY, PatchARTIFACTS, &st_invslot[i],
					   &st_statusbaron, &cfg.statusbarCounterAlpha);

		// inventory slot counter
		STlib_initNum(&w_invslotcount[i], ST_INVENTORYX + temp + ST_INVCOUNTOFFX, ST_INVENTORYY + ST_INVCOUNTOFFY, PatchNumSmNumbers,
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

//==========================================================================
//
// SB_SetClassData
//
//==========================================================================

void SB_SetClassData(void)
{
	int     class;
	char    namebuf[9];

	class = cfg.PlayerClass[consoleplayer];	// original player class (not pig)

	sprintf(namebuf, "wpslot%d", 0 + class);
	R_CachePatch(&PatchNumWEAPONSLOT, namebuf);

	sprintf(namebuf, "wpfull%d", 0 + class);
	R_CachePatch(&PatchNumWEAPONFULL, namebuf);

	switch(class)
	{
		case 0:	// fighter
			R_CachePatch(&PatchNumPIECE1, "wpiecef1");
			R_CachePatch(&PatchNumPIECE2, "wpiecef2");
			R_CachePatch(&PatchNumPIECE3, "wpiecef3");
			R_CachePatch(&PatchNumCHAIN, "chain");
			break;

		case 1:	// cleric
			R_CachePatch(&PatchNumPIECE1, "wpiecec1");
			R_CachePatch(&PatchNumPIECE2, "wpiecec2");
			R_CachePatch(&PatchNumPIECE3, "wpiecec3");
			R_CachePatch(&PatchNumCHAIN, "chain2");
			break;

		case 2:	// mage
			R_CachePatch(&PatchNumPIECE1, "wpiecem1");
			R_CachePatch(&PatchNumPIECE2, "wpiecem2");
			R_CachePatch(&PatchNumPIECE3, "wpiecem3");
			R_CachePatch(&PatchNumCHAIN, "chain3");
			break;

		default:
			break;
	}

	if(!IS_NETGAME)
	{							// single player game uses red life gem (the second gem)
		PatchNumLIFEGEM = W_GetNumForName("lifegem") + MAXPLAYERS * class + 1;
	}
	else
	{
		PatchNumLIFEGEM = W_GetNumForName("lifegem") + MAXPLAYERS * class + consoleplayer;
	}

	SB_state = -1;
	GL_Update(DDUF_FULLSCREEN);
}

void ST_updateWidgets(void)
{
	int     i, x;

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
		if(plyr->readyArtifact > 0)
		{
			st_artici = plyr->readyArtifact + 5;
		}
		oldarti = plyr->readyArtifact;
		oldartiCount = plyr->inventory[inv_ptr].count;
	}

	// Armor
	armorlevel = FixedDiv(
		AutoArmorSave[plyr->class] + plyr->armorpoints[ARMOR_ARMOR] +
		plyr->armorpoints[ARMOR_SHIELD] +
		plyr->armorpoints[ARMOR_HELMET] +
		plyr->armorpoints[ARMOR_AMULET], 5 * FRACUNIT) >> FRACBITS;

	// mana A
	manaACount = plyr->mana[0];

	// mana B
	manaBCount = plyr->mana[1];

	st_manaAicon = st_manaBicon = st_manaAvial = st_manaBvial = -1;

	// Mana
	if(plyr->mana[0] == 0)				// Draw Dim Mana icon
		st_manaAicon = 0;

	if(plyr->mana[1] == 0)				// Draw Dim Mana icon						
		st_manaBicon = 0;


	// Update mana graphics based upon mana count weapon type
	if(plyr->readyweapon == WP_FIRST)
	{
		st_manaAicon = 0;
		st_manaBicon = 0;

		st_manaAvial = 0;
		st_manaBvial = 0;
	}
	else if(plyr->readyweapon == WP_SECOND)
	{
		// If there is mana for this weapon, make it bright!
		if(st_manaAicon == -1)
		{
			st_manaAicon = 1;
		}

		st_manaAvial = 1;

		st_manaBicon = 0;
		st_manaBvial = 0;
	}
	else if(plyr->readyweapon == WP_THIRD)
	{
		st_manaAicon = 0;
		st_manaAvial = 0;

		// If there is mana for this weapon, make it bright!
		if(st_manaBicon == -1)
		{
			st_manaBicon = 1;
		}

		st_manaBvial = 1;
	}
	else
	{
		st_manaAvial = 1;
		st_manaBvial = 1;

		// If there is mana for this weapon, make it bright!
		if(st_manaAicon == -1)
		{
			st_manaAicon = 1;
		}
		if(st_manaBicon == -1)
		{
			st_manaBicon = 1;
		}
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

//==========================================================================
//
// SB_Ticker
//
//==========================================================================

void ST_Ticker(void)
{
	int     delta;
	int     curHealth;

	if(!players[consoleplayer].plr->mo)
		return;

	ST_updateWidgets();

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
		else if(delta > 6)
		{
			delta = 6;
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

static void DrINumber(signed int val, int x, int y, float r, float g, float b, float a)
{
	int     oldval;

	gl.Color4f(r,g,b,a);

	// Make sure it's a three digit number.
	if(val < -999)
		val = -999;
	if(val > 999)
		val = 999;

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
			GL_DrawPatch_CS(x + 8, y, PatchNumINumbers[val / 10].lump);
			GL_DrawPatch_CS(x, y, PatchNumNEGATIVE.lump);
		}
		else
		{
			GL_DrawPatch_CS(x + 8, y, PatchNumNEGATIVE.lump);
		}
		val = val % 10;
		GL_DrawPatch_CS(x + 16, y, PatchNumINumbers[val].lump);
		return;
	}
	if(val > 99)
	{
		GL_DrawPatch_CS(x, y, PatchNumINumbers[val / 100].lump);
	}
	val = val % 100;
	if(val > 9 || oldval > 99)
	{
		GL_DrawPatch_CS(x + 8, y, PatchNumINumbers[val / 10].lump);
	}
	val = val % 10;
	GL_DrawPatch_CS(x + 16, y, PatchNumINumbers[val].lump);

}

//==========================================================================
//
// DrRedINumber
//
// Draws a three digit number using the red font
//
//==========================================================================

#if 0 // UNUSED
static void DrRedINumber(signed int val, int x, int y)
{
	int     oldval;

	// Make sure it's a three digit number.
	if(val < -999)
		val = -999;
	if(val > 999)
		val = 999;

	oldval = val;
	if(val < 0)
	{
		val = 0;
	}
	if(val > 99)
	{
		GL_DrawPatch(x, y, W_GetNumForName("inred0") + val / 100);
	}
	val = val % 100;
	if(val > 9 || oldval > 99)
	{
		GL_DrawPatch(x + 8, y, W_GetNumForName("inred0") + val / 10);
	}
	val = val % 10;
	GL_DrawPatch(x + 16, y, W_GetNumForName("inred0") + val);
}
#endif

//==========================================================================
//
// DrBNumber
//
// Draws a three digit number using FontB
//
//==========================================================================

static void DrBNumber(signed int val, int x, int y, float red, float green, float blue, float alpha)
{
	patch_t *patch;
	int     xpos;
	int     oldval;

	// Limit to three digits.
	if(val > 999)
		val = 999;
	if(val < -999)
		val = -999;

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

//==========================================================================
//
// DrSmallNumber
//
// Draws a small two digit number.
//
//==========================================================================

static void DrSmallNumber(int val, int x, int y, float r, float g, float b, float a)
{
	gl.Color4f(r,g,b,a);

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
		GL_DrawPatch_CS(x, y, PatchNumSmNumbers[val / 100].lump);
		GL_DrawPatch_CS(x + 4, y, PatchNumSmNumbers[(val % 100) / 10].lump);
	}
	else if(val > 9)
	{
		GL_DrawPatch_CS(x + 4, y, PatchNumSmNumbers[val / 10].lump);
	}
	val %= 10;
	GL_DrawPatch_CS(x + 8, y, PatchNumSmNumbers[val].lump);
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
	int     i;
	SoundInfo_t s;
	ChanInfo_t *c;
	char    text[32];
	int     x;
	int     y;
	int     xPos[7] = { 1, 75, 112, 156, 200, 230, 260 };

	if(leveltime & 16)
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
		y = 40 + i * 10;
		if(c->mo == NULL)
		{						// Channel is unused
			MN_DrTextA("------", xPos[0], y);
			continue;
		}
		sprintf(text, "%s", c->name);
		//M_ForceUppercase(text);
		strupr(text);
		MN_DrTextA(text, xPos[x++], y);
		sprintf(text, "%d", c->mo->type);
		MN_DrTextA(text, xPos[x++], y);
		sprintf(text, "%d", c->mo->x >> FRACBITS);
		MN_DrTextA(text, xPos[x++], y);
		sprintf(text, "%d", c->mo->y >> FRACBITS);
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

int     SB_state = -1;

/*
 *   ST_refreshBackground
 *
 *	Draws the whole statusbar backgound
 */
void ST_refreshBackground(void)
{
	int x, y, w, h;
	float cw, cw2, ch;

	if(st_blended && ((cfg.statusbarAlpha < 1.0f) && (cfg.statusbarAlpha > 0.0f)))
	{
		gl.Color4f(1, 1, 1, cfg.statusbarAlpha);

		GL_SetPatch(PatchNumH2BAR.lump);

		gl.Begin(DGL_QUADS);

		// top
		x = 0;
		y = 135;
		w = 320;
		h = 27;
		ch = 0.41538461538461538461538461538462f;

		gl.TexCoord2f(0, 0);
		gl.Vertex2f(x, y);
		gl.TexCoord2f(1, 0);
		gl.Vertex2f(x + w, y);
		gl.TexCoord2f(1, ch);
		gl.Vertex2f(x + w, y + h);
		gl.TexCoord2f(0, ch);
		gl.Vertex2f(x, y + h);

		// left statue
		x = 0;
		y = 162;
		w = 38;
		h = 38;
		cw = 0.11875f;
		ch = 0.41538461538461538461538461538462f;

		gl.TexCoord2f(0, ch);
		gl.Vertex2f(x, y);
		gl.TexCoord2f(cw, ch);
		gl.Vertex2f(x + w, y);
		gl.TexCoord2f(cw, 1);
		gl.Vertex2f(x + w, y + h);
		gl.TexCoord2f(0, 1);
		gl.Vertex2f(x, y + h);

		// right statue
		x = 282;
		y = 162;
		w = 38;
		h = 38;
		cw = 0.88125f;
		ch = 0.41538461538461538461538461538462f;

		gl.TexCoord2f(cw, ch);
		gl.Vertex2f(x, y);
		gl.TexCoord2f(1, ch);
		gl.Vertex2f(x + w, y);
		gl.TexCoord2f(1, 1);
		gl.Vertex2f(x + w, y + h);
		gl.TexCoord2f(cw, 1);
		gl.Vertex2f(x, y + h);

		// bottom (behind the chain)
		x = 38;
		y = 192;
		w = 244;
		h = 8;
		cw = 0.11875f;
		cw2 = 0.88125f;
		ch = 0.87692307692307692307692307692308f;

		gl.TexCoord2f(cw, ch);
		gl.Vertex2f(x, y);
		gl.TexCoord2f(cw2, ch);
		gl.Vertex2f(x + w, y);
		gl.TexCoord2f(cw2, 1);
		gl.Vertex2f(x + w, y + h);
		gl.TexCoord2f(cw, 1);
		gl.Vertex2f(x, y + h);

		gl.End();

		if(!inventory)
		{
			// Main interface
			if(!automapactive)
			{
				if(deathmatch)
				{
					GL_DrawPatch_CS(38, 162, PatchNumKILLS.lump);
				}

				// left of statbar (upto weapon puzzle display)
				GL_SetPatch(PatchNumSTATBAR.lump);
				gl.Begin(DGL_QUADS);

				x = deathmatch ? 68 : 38;
				y = 162;
				w = deathmatch ? 122 : 152;
				h = 30;
				cw = deathmatch ? 0.12295081967213114754098360655738f : 0;
				cw2 = 0.62295081967213114754098360655738f;
				ch = 0.96774193548387096774193548387097f;

				gl.TexCoord2f(cw, 0);
				gl.Vertex2f(x, y);
				gl.TexCoord2f(cw2, 0);
				gl.Vertex2f(x + w, y);
				gl.TexCoord2f(cw2, ch);
				gl.Vertex2f(x + w, y + h);
				gl.TexCoord2f(cw, ch);
				gl.Vertex2f(x, y + h);

				// right of statbar (after weapon puzzle display)
				x = 247;
				y = 162;
				w = 35;
				h = 30;
				cw = 0.85655737704918032786885245901639f;
				ch = 0.96774193548387096774193548387097f;

				gl.TexCoord2f(cw, 0);
				gl.Vertex2f(x, y);
				gl.TexCoord2f(1, 0);
				gl.Vertex2f(x + w, y);
				gl.TexCoord2f(1, ch);
				gl.Vertex2f(x + w, y + h);
				gl.TexCoord2f(cw, ch);
				gl.Vertex2f(x, y + h);

				gl.End();

				DrawWeaponPieces();
			}
			else
			{
				GL_DrawPatch_CS(38, 162, PatchNumKEYBAR.lump);
			}

		} else {
			// INVBAR
			GL_SetPatch(PatchNumINVBAR.lump);
			gl.Begin(DGL_QUADS);

			x = 38;
			y = 162;
			w = 244;
			h = 30;
			ch = 0.96774193548387096774193548387097f;

			gl.TexCoord2f(0, 0);
			gl.Vertex2f(x, y);
			gl.TexCoord2f(1, 0);
			gl.Vertex2f(x + w, y);
			gl.TexCoord2f(1, ch);
			gl.Vertex2f(x + w, y + h);
			gl.TexCoord2f(0, ch);
			gl.Vertex2f(x, y + h);

			gl.End();
		}

		DrawChain();

	} else if (cfg.statusbarAlpha != 0){

		GL_DrawPatch(0, 134, PatchNumH2BAR.lump);

		GL_DrawPatch(0, 134, PatchNumH2TOP.lump);

		if(!inventory)
		{
			// Main interface
			if(!automapactive)
			{
				GL_DrawPatch(38, 162, PatchNumSTATBAR.lump);

				if(CPlayer->pieces == 7)
				{
					GL_DrawPatch(190, 162, PatchNumWEAPONFULL.lump);

				} else {
					GL_DrawPatch(190, 162, PatchNumWEAPONSLOT.lump);
				}

				DrawWeaponPieces();
			}
			else
			{
				GL_DrawPatch(38, 162, PatchNumKEYBAR.lump);
				DrawKeyBar();
			}

		} else {
			GL_DrawPatch(38, 162, PatchNumINVBAR.lump);
		}

		DrawChain();
	}
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
}

void ST_Drawer(int fullscreenmode, boolean refresh )
{
	st_firsttime = st_firsttime || refresh;
	st_statusbaron = (fullscreenmode < 2) || ( automapactive && (cfg.automapHudDisplay == 0 || cfg.automapHudDisplay == 2) );

	CPlayer = &players[consoleplayer];

	// Do palette shifts
	ST_doPaletteStuff(false);

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
	} else if (fullscreenmode != 3
#ifdef DEMOCAM
	    || (demoplayback && democam.mode)
#endif
					){
		ST_doFullscreenStuff();
	}

	DrawAnimatedIcons();
}

//==========================================================================
//
// DrawAnimatedIcons
//
//==========================================================================

static void DrawAnimatedIcons(void)
{
	int     leftoff = 0;
	int     frame;
	static boolean hitCenterFrame;
	float iconalpha = (st_statusbaron? 1: hudalpha) - ( 1 - cfg.hudIconAlpha);

	//  extern int screenblocks;

	// If the fullscreen mana is drawn, we need to move the icons on the left
	// a bit to the right.
	if(cfg.hudShown[HUD_MANA] == 1 && cfg.screenblocks > 10)
		leftoff = 42;

    Draw_BeginZoom(cfg.hudScale, 2, 2);
    
	// Wings of wrath
	if(CPlayer->powers[pw_flight])
	{
		if(CPlayer->powers[pw_flight] > BLINKTHRESHOLD ||
		   !(CPlayer->powers[pw_flight] & 16))
		{
			frame = (leveltime / 3) & 15;
			if(CPlayer->plr->mo->flags2 & MF2_FLY)
			{
				if(hitCenterFrame && (frame != 15 && frame != 0))
				{
					GL_DrawPatchLitAlpha(20 + leftoff, 19, 1, iconalpha, SpinFlylump.lump + 15);
				}
				else
				{
					GL_DrawPatchLitAlpha(20 + leftoff, 19, 1, iconalpha, SpinFlylump.lump + frame);
					hitCenterFrame = false;
				}
			}
			else
			{
				if(!hitCenterFrame && (frame != 15 && frame != 0))
				{
					GL_DrawPatchLitAlpha(20 + leftoff, 19, 1, iconalpha, SpinFlylump.lump + frame);
					hitCenterFrame = false;
				}
				else
				{
					GL_DrawPatchLitAlpha(20 + leftoff, 19, 1, iconalpha, SpinFlylump.lump + 15);
					hitCenterFrame = true;
				}
			}
		}
		GL_Update(DDUF_TOP | DDUF_MESSAGES);
	}

	// Speed Boots
	if(CPlayer->powers[pw_speed])
	{
		if(CPlayer->powers[pw_speed] > BLINKTHRESHOLD ||
		   !(CPlayer->powers[pw_speed] & 16))
		{
			frame = (leveltime / 3) & 15;
			GL_DrawPatchLitAlpha(60 + leftoff, 19, 1, iconalpha, SpinSpeedLump.lump + frame);
		}
		GL_Update(DDUF_TOP | DDUF_MESSAGES);
	}

    Draw_EndZoom();

    Draw_BeginZoom(cfg.hudScale, 318, 2);
    
	// Defensive power
	if(CPlayer->powers[pw_invulnerability])
	{
		if(CPlayer->powers[pw_invulnerability] > BLINKTHRESHOLD ||
		   !(CPlayer->powers[pw_invulnerability] & 16))
		{
			frame = (leveltime / 3) & 15;
			GL_DrawPatchLitAlpha(260, 19, 1, iconalpha, SpinDefenseLump.lump + frame);
		}
		GL_Update(DDUF_TOP | DDUF_MESSAGES);
	}

	// Minotaur Active
	if(CPlayer->powers[pw_minotaur])
	{
		if(CPlayer->powers[pw_minotaur] > BLINKTHRESHOLD ||
		   !(CPlayer->powers[pw_minotaur] & 16))
		{
			frame = (leveltime / 3) & 15;
			GL_DrawPatchLitAlpha(300, 19, 1, iconalpha, SpinMinotaurLump.lump + frame);
		}
		GL_Update(DDUF_TOP | DDUF_MESSAGES);
	}

    Draw_EndZoom();
}

//==========================================================================
//
// ST_doPaletteStuff
//
// Sets the new palette based upon the current values of
// consoleplayer->damagecount and consoleplayer->bonuscount.
//
//==========================================================================

void ST_doPaletteStuff(boolean forceChange)
{
	static int sb_palette = 0;
	int     palette;

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
			palette = (CPlayer->poisoncount + 7) >> 3;
			if(palette >= NUMPOISONPALS)
			{
				palette = NUMPOISONPALS - 1;
			}
			palette += STARTPOISONPALS;
		}
		else if(CPlayer->damagecount)
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
		else if(CPlayer->plr->mo->flags2 & MF2_ICEDAMAGE)
		{						// Frozen player
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
// DrawChain
//
//==========================================================================

void DrawChain(void)
{
	float     healthPos;
	float     gemglow;

	int x, x2, y, w, w2, w3, h;
	int gemoffset = 36;
	float cw, cw2;

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

	// draw the chain
	x = 44;
	y = 193;
	w = 232;
	h = 7;
	cw = (healthPos / 113) + 0.054f;

	GL_SetPatch(PatchNumCHAIN.lump);

	gl.TexParameter(DGL_WRAP_S, DGL_REPEAT);

	gl.Color4f(1, 1, 1, cfg.statusbarCounterAlpha);

	gl.Begin(DGL_QUADS);

	gl.TexCoord2f( 0 - cw, 0);
	gl.Vertex2f(x, y);

	gl.TexCoord2f( 0.948f - cw, 0);
	gl.Vertex2f(x + w, y);

	gl.TexCoord2f( 0.948f - cw, 1);
	gl.Vertex2f(x + w, y + h);

	gl.TexCoord2f( 0 - cw, 1);
	gl.Vertex2f(x, y + h);

	gl.End();


	healthPos = ((healthPos * 256) / 117) - gemoffset;

	x = 44;
	y = 193;
	w2 = 86;
	h = 7;
	cw = 0;
	cw2 = 1;

	// calculate the size of the quad, position and tex coords
	if ((x + healthPos) < x){
		x2 = x;
		w3 = w2 + healthPos;
		cw = (1.0f / w2) * (w2 - w3);
		cw2 = 1;
	} else if((x + healthPos + w2) > (x + w) ){
		x2 = x + healthPos;
		w3 = w2 - ((x + healthPos + w2) - (x + w));
		cw = 0;
		cw2 = (1.0f / w2) * (w2 - (w2 - w3));
	} else {
		x2 = x + healthPos;
		w3 = w2;
		cw = 0;
		cw2 = 1;
	}

	GL_SetPatch(PatchNumLIFEGEM);

	// draw the life gem
	gl.Color4f(1, 1, 1, cfg.statusbarCounterAlpha);

	gl.Begin(DGL_QUADS);

	gl.TexCoord2f( cw, 0);
	gl.Vertex2f(x2, y);

	gl.TexCoord2f( cw2, 0);
	gl.Vertex2f(x2 + w3, y);

	gl.TexCoord2f( cw2, 1);
	gl.Vertex2f(x2 + w3, y + h);

	gl.TexCoord2f( cw, 1);
	gl.Vertex2f(x2, y + h);

	gl.End();

	// how about a glowing gem?
	gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_ONE);
	gl.Bind(Get(DD_DYNLIGHT_TEXTURE));

	GL_DrawRect(x + healthPos + 25, y - 3, 34, 18, 1, 0, 0, gemglow - (1 - cfg.statusbarCounterAlpha));

	gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_ONE_MINUS_SRC_ALPHA);

	GL_Update(DDUF_STATBAR);
}

//==========================================================================
//
// ST_drawWidgets
//
//==========================================================================

void ST_drawWidgets(boolean refresh)
{
	int     i;
	int     x;

	oldhealth = -1;
	if(!inventory)
	{
	    if(!automapactive){

		// Frags
		if(deathmatch)
				STlib_updateNum(&w_frags, refresh);
		else
				STlib_updateNum(&w_health, refresh);

		// draw armor
		STlib_updateNum(&w_armor, refresh);

		// current artifact
		if(CPlayer->readyArtifact > 0){
			STlib_updateMultIcon(&w_artici, refresh);
			if(!ArtifactFlash && CPlayer->inventory[inv_ptr].count > 1)
				STlib_updateNum(&w_articount, refresh);
		}

		// manaA count
		if(manaACount > 0)
			STlib_updateNum(&w_manaACount, refresh);

		// manaB count
		if(manaBCount > 0)
			STlib_updateNum(&w_manaBCount, refresh);

		// manaA icon
		STlib_updateMultIcon(&w_manaAicon, refresh);

		// manaB icon
		STlib_updateMultIcon(&w_manaBicon, refresh);

		// manaA vial
		STlib_updateMultIcon(&w_manaAvial, refresh);

		// manaB vial
		STlib_updateMultIcon(&w_manaBvial, refresh);

		// Draw the mana bars
		GL_SetNoTexture();
		GL_DrawRect(95, 165, 3, 22 - (22 * CPlayer->mana[0]) / MAX_MANA, 0, 0, 0, cfg.statusbarAlpha);
		GL_DrawRect(103, 165, 3, 22 - (22 * CPlayer->mana[1]) / MAX_MANA, 0, 0, 0, cfg.statusbarAlpha);

	    } else {
		DrawKeyBar();
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
		GL_DrawPatch(ST_INVENTORYX + curpos * 31, 163, PatchNumSELECTBOX.lump);

		// Draw more left indicator
		if(x != 0)
			GL_DrawPatchLitAlpha(42, 163, 1, cfg.statusbarCounterAlpha, !(leveltime & 4) ? PatchNumINVLFGEM1.lump : PatchNumINVLFGEM2.lump);

		// Draw more right indicator
		if(CPlayer->inventorySlotNum - x > 7)
			GL_DrawPatchLitAlpha(269, 163, 1, cfg.statusbarCounterAlpha, !(leveltime & 4) ? PatchNumINVRTGEM1.lump : PatchNumINVRTGEM2.lump);
	}
}

//==========================================================================
//
// DrawKeyBar
//
//==========================================================================

void DrawKeyBar(void)
{
	int     i;
	int     xPosition;
	int     temp;

	xPosition = 46;
	for(i = 0; i < NUMKEYS && xPosition <= 126; i++)
	{
		if(CPlayer->keys & (1 << i))
		{
			GL_DrawPatchLitAlpha(xPosition, 163, 1, cfg.statusbarCounterAlpha, W_GetNumForName("keyslot1") + i);
			xPosition += 20;
		}
	}
	temp =
		AutoArmorSave[CPlayer->class] + CPlayer->armorpoints[ARMOR_ARMOR] +
		CPlayer->armorpoints[ARMOR_SHIELD] +
		CPlayer->armorpoints[ARMOR_HELMET] +
		CPlayer->armorpoints[ARMOR_AMULET];
	for(i = 0; i < NUMARMOR; i++)
	{
		if(!CPlayer->armorpoints[i])
		{
			continue;
		}
		if(CPlayer->armorpoints[i] <= (ArmorIncrement[CPlayer->class][i] >> 2))
		{
			GL_DrawPatchLitAlpha(150 + 31 * i, 164, 1, cfg.statusbarCounterAlpha * 0.3, 
							 W_GetNumForName("armslot1") + i);
		}
		else if(CPlayer->armorpoints[i] <=
				(ArmorIncrement[CPlayer->class][i] >> 1))
		{
			GL_DrawPatchLitAlpha(150 + 31 * i, 164, 1, cfg.statusbarCounterAlpha * 0.6, 
								W_GetNumForName("armslot1") + i);
		}
		else
		{
			GL_DrawPatchLitAlpha(150 + 31 * i, 164, 1, cfg.statusbarCounterAlpha, W_GetNumForName("armslot1") + i);
		}
	}
}

//==========================================================================
//
// DrawWeaponPieces
//
//==========================================================================

static int PieceX[NUMCLASSES][3] = {
	{190, 225, 234},
	{190, 212, 225},
	{190, 205, 224},
	{0, 0, 0}					// Pig is never used
};

static void DrawWeaponPieces(void)
{

	GL_DrawPatchLitAlpha(190, 162, 1, cfg.statusbarAlpha, PatchNumWEAPONSLOT.lump);

	if(plyr->pieces == 7) // All pieces
		GL_DrawPatchLitAlpha(190, 162, 1, cfg.statusbarCounterAlpha, PatchNumWEAPONFULL.lump);
	else
	{
		if(CPlayer->pieces & WPIECE1)
		{
			GL_DrawPatchLitAlpha(PieceX[cfg.PlayerClass[consoleplayer]][0], 162,
							1, cfg.statusbarCounterAlpha, PatchNumPIECE1.lump);
		}
		if(CPlayer->pieces & WPIECE2)
		{
			GL_DrawPatchLitAlpha(PieceX[cfg.PlayerClass[consoleplayer]][1], 162,
							1, cfg.statusbarCounterAlpha, PatchNumPIECE2.lump);
		}
		if(CPlayer->pieces & WPIECE3)
		{
			GL_DrawPatchLitAlpha(PieceX[cfg.PlayerClass[consoleplayer]][2], 162,
							1, cfg.statusbarCounterAlpha, PatchNumPIECE3.lump);
		}
	}
}

//==========================================================================
//
// ST_doFullscreenStuff
//
//==========================================================================

void ST_doFullscreenStuff(void)
{
	int     i;
	int     x;
	int     temp;
	float textalpha = hudalpha - ( 1 - cfg.hudColor[3]);
	float iconalpha = hudalpha - ( 1 - cfg.hudIconAlpha);

#ifdef DEMOCAM
	if(demoplayback && democam.mode)
		return;
#endif

    if(cfg.hudShown[HUD_HEALTH])
    {
    Draw_BeginZoom(cfg.hudScale, 5, 198);
	if(CPlayer->plr->mo->health > 0)
	{
		DrBNumber(CPlayer->plr->mo->health, 5, 180,
					  cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textalpha);
	}
	else
	{
		DrBNumber(0, 5, 180, cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textalpha);
	}
    Draw_EndZoom();
    }

	if(cfg.hudShown[HUD_MANA])
	{
		int     dim[2] = { PatchNumMANADIM1.lump, PatchNumMANADIM2.lump };
		int     bright[2] = { PatchNumMANABRIGHT1.lump, PatchNumMANABRIGHT2.lump };
		int     patches[2] = { 0, 0 };
		int     ypos = cfg.hudShown[HUD_MANA] == 2 ? 152 : 2;

		for(i = 0; i < 2; i++)
			if(CPlayer->mana[i] == 0)
				patches[i] = dim[i];
		if(CPlayer->readyweapon == WP_FIRST)
		{
			for(i = 0; i < 2; i++)
				patches[i] = dim[i];
		}
		if(CPlayer->readyweapon == WP_SECOND)
		{
			if(!patches[0])
				patches[0] = bright[0];
			patches[1] = dim[1];
		}
		if(CPlayer->readyweapon == WP_THIRD)
		{
			patches[0] = dim[0];
			if(!patches[1])
				patches[1] = bright[1];
		}
		if(CPlayer->readyweapon == WP_FOURTH)
		{
			for(i = 0; i < 2; i++)
				if(!patches[i])
					patches[i] = bright[i];
		}
        Draw_BeginZoom(cfg.hudScale, 2, ypos);
        for(i = 0; i < 2; i++)
		{
			GL_DrawPatchLitAlpha(2, ypos + i * 13, 1, iconalpha, patches[i]);
			DrINumber(CPlayer->mana[i], 18, ypos + i * 13, 1, 1, 1, textalpha);
		}
        Draw_EndZoom();
	}

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
		if(cfg.hudShown[HUD_ARTI]){
			if(CPlayer->readyArtifact > 0)
			{
            	    Draw_BeginZoom(cfg.hudScale, 318, 198);
				GL_DrawPatchLitAlpha(286, 170, 1, iconalpha/2, W_GetNumForName("ARTIBOX"));
				GL_DrawPatchLitAlpha(284, 169, 1, iconalpha,
						 W_GetNumForName(artifactlist[CPlayer->readyArtifact+5]));
				if(CPlayer->inventory[inv_ptr].count > 1)
				{
					DrSmallNumber(CPlayer->inventory[inv_ptr].count, 302, 192, 1, 1, 1, textalpha);
				}
            	    Draw_EndZoom();
			}
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
				GL_DrawPatchLitAlpha(49 + i * 31, 167, 1, i==curpos? hudalpha : iconalpha,
                             W_GetNumForName(artifactlist[CPlayer->inventory
                                                       [x + i].type+5]));

				if(CPlayer->inventory[x + i].count > 1)
				{
					DrSmallNumber(CPlayer->inventory[x + i].count, 66 + i * 31,
								  188,1, 1, 1, i==curpos? hudalpha : textalpha/2);
				}
			}
		}
		GL_DrawPatchLitAlpha(50 + curpos * 31, 167, 1, hudalpha,PatchNumSELECTBOX.lump);
		if(x != 0)
		{
			GL_DrawPatchLitAlpha(40, 167, 1, iconalpha,
						 !(leveltime & 4) ? PatchNumINVLFGEM1.lump :
						 PatchNumINVLFGEM2.lump);
		}
		if(CPlayer->inventorySlotNum - x > 7)
		{
			GL_DrawPatchLitAlpha(268, 167, 1, iconalpha,
						 !(leveltime & 4) ? PatchNumINVRTGEM1.lump :
						 PatchNumINVRTGEM2.lump);
		}
        Draw_EndZoom();
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
	int     i;

	// Dedicated servers don't draw anything.
	if(IS_DEDICATED)
		return;

	for(i = 0; i < 2; i++)
	{
		gl.Clear(DGL_COLOR_BUFFER_BIT);
		GL_DrawRawScreen(W_CheckNumForName("TRAVLPIC"), 0, 0);
		GL_DrawPatch(100, 68, W_GetNumForName("teleicon"));
		if(i)
			break;
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

static boolean canCheat()
{
	if(IS_NETGAME && !IS_CLIENT && netSvAllowCheats)
		return true;
#ifdef _DEBUG
	return true;
#else
	return !(gameskill == sk_nightmare || (IS_NETGAME && !netcheat) ||
			 players[consoleplayer].health <= 0);
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
	int     i;
	boolean eat;

	if(gameskill == sk_nightmare)
	{							// Can't cheat in nightmare mode
		return (false);
	}
	else if(IS_NETGAME)
	{							// change CD track is the only cheat available in deathmatch
		eat = false;
		/*      if(i_CDMusic)
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
		   } */
		return eat;
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
			S_StartSound(SFX_PLATFORM_STOP, NULL);
		}
	}
	//printf( "end of handlecheat: %d (%d)\n",key,eat);
	return (eat);
}

//==========================================================================
//
// CheatAddkey
//
// Returns true if the added key completed the cheat, false otherwise.
//
//==========================================================================

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
	SB_state = -1;
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
	P_SetMessage(player, TXT_CHEATWEAPONS);
}

static void CheatHealthFunc(player_t *player, Cheat_t * cheat)
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
	P_SetMessage(player, TXT_CHEATHEALTH);
}

static void CheatKeysFunc(player_t *player, Cheat_t * cheat)
{
	player->update |= PSF_KEYS;
	player->keys = 2047;
	P_SetMessage(player, TXT_CHEATKEYS);
}

static void CheatSoundFunc(player_t *player, Cheat_t * cheat)
{
	DebugSound = !DebugSound;
	if(DebugSound)
	{
		P_SetMessage(player, TXT_CHEATSOUNDON);
	}
	else
	{
		P_SetMessage(player, TXT_CHEATSOUNDOFF);
	}
}

static void CheatTickerFunc(player_t *player, Cheat_t * cheat)
{
	/*  extern int DisplayTicker;

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

static void CheatArtifactAllFunc(player_t *player, Cheat_t * cheat)
{
	int     i;
	int     j;

	for(i = arti_none + 1; i < arti_firstpuzzitem; i++)
	{
		for(j = 0; j < 25; j++)
		{
			P_GiveArtifact(player, i, NULL);
		}
	}
	P_SetMessage(player, TXT_CHEATARTIFACTS3);
}

static void CheatPuzzleFunc(player_t *player, Cheat_t * cheat)
{
	int     i;

	for(i = arti_firstpuzzitem; i < NUMARTIFACTS; i++)
	{
		P_GiveArtifact(player, i, NULL);
	}
	P_SetMessage(player, TXT_CHEATARTIFACTS3);
}

static void CheatInitFunc(player_t *player, Cheat_t * cheat)
{
	G_DeferedInitNew(gameskill, gameepisode, gamemap);
	P_SetMessage(player, TXT_CHEATWARP);
}

static void CheatWarpFunc(player_t *player, Cheat_t * cheat)
{
	int     tens;
	int     ones;
	int     map;
	char    mapName[9];
	char    auxName[128];
	FILE   *fp;

	tens = cheat->args[0] - '0';
	ones = cheat->args[1] - '0';
	if(tens < 0 || tens > 9 || ones < 0 || ones > 9)
	{							// Bad map
		P_SetMessage(player, TXT_CHEATBADINPUT);
		return;
	}
	//map = P_TranslateMap((cheat->args[0]-'0')*10+cheat->args[1]-'0');
	map = P_TranslateMap(tens * 10 + ones);
	if(map == -1)
	{							// Not found
		P_SetMessage(player, TXT_CHEATNOMAP);
		return;
	}
	if(map == gamemap)
	{							// Don't try to teleport to current map
		P_SetMessage(player, TXT_CHEATBADINPUT);
		return;
	}
	if(DevMaps)
	{							// Search map development directory
		sprintf(auxName, "%sMAP%02d.WAD", DevMapsDir, map);
		fp = fopen(auxName, "rb");
		if(fp)
		{
			fclose(fp);
		}
		else
		{						// Can't find
			P_SetMessage(player, TXT_CHEATNOMAP);
			return;
		}
	}
	else
	{							// Search primary lumps
		sprintf(mapName, "MAP%02d", map);
		if(W_CheckNumForName(mapName) == -1)
		{						// Can't find
			P_SetMessage(player, TXT_CHEATNOMAP);
			return;
		}
	}
	P_SetMessage(player, TXT_CHEATWARP);
	G_TeleportNewMap(map, 0);
	//G_Completed(-1, -1);//map, 0);
}

static void CheatPigFunc(player_t *player, Cheat_t * cheat)
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
	P_SetMessage(player, "SQUEAL!!");
}

static void CheatMassacreFunc(player_t *player, Cheat_t * cheat)
{
	int     count;
	char    buffer[80];

	count = P_Massacre();
	sprintf(buffer, "%d MONSTERS KILLED\n", count);
	P_SetMessage(player, buffer);
}

static void CheatIDKFAFunc(player_t *player, Cheat_t * cheat)
{
	int     i;

	if(player->morphTics)
	{
		return;
	}
	for(i = 1; i < 8; i++)
	{
		player->weaponowned[i] = false;
	}
	player->pendingweapon = WP_FIRST;
	P_SetMessage(player, TXT_CHEATIDKFA);
}

static void CheatQuickenFunc1(player_t *player, Cheat_t * cheat)
{
	P_SetMessage(player, "TRYING TO CHEAT?  THAT'S ONE....");
}

static void CheatQuickenFunc2(player_t *player, Cheat_t * cheat)
{
	P_SetMessage(player, "THAT'S TWO....");
}

static void CheatQuickenFunc3(player_t *player, Cheat_t * cheat)
{
	P_DamageMobj(player->plr->mo, NULL, player->plr->mo, 10000);
	P_SetMessage(player, "THAT'S THREE!  TIME TO DIE.");
}

static void CheatClassFunc1(player_t *player, Cheat_t * cheat)
{
	P_SetMessage(player, "ENTER NEW PLAYER CLASS (0 - 2)");
}

//==========================================================================
// SB_ChangePlayerClass
//  Changes the class of the given player. Will not work if the player
//  is currently morphed.
//==========================================================================
void SB_ChangePlayerClass(player_t *player, int newclass)
{
	int     i;
	mobj_t *oldmobj;
	mapthing_t dummy;

	// Don't change if morphed.
	if(player->morphTics)
		return;

	if(newclass < 0 || newclass > 2)
		return;					// Must be 0-2.
	player->class = newclass;
	// Take away armor.
	for(i = 0; i < NUMARMOR; i++)
		player->armorpoints[i] = 0;
	cfg.PlayerClass[player - players] = newclass;
	P_PostMorphWeapon(player, WP_FIRST);
	if(player == players + consoleplayer)
		SB_SetClassData();
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

static void CheatClassFunc2(player_t *player, Cheat_t * cheat)
{
	int     class;

	if(player->morphTics)
	{							// don't change class if the player is morphed
		return;
	}
	class = cheat->args[0] - '0';
	if(class > 2 || class < 0)
	{
		P_SetMessage(player, "INVALID PLAYER CLASS");
		return;
	}
	SB_ChangePlayerClass(player, class);
}

static void CheatVersionFunc(player_t *player, Cheat_t * cheat)
{
	P_SetMessage(player, VERSIONTEXT);
}

static void CheatDebugFunc(player_t *player, Cheat_t * cheat)
{
	char    textBuffer[50];

	sprintf(textBuffer, "MAP %d (%d)  X:%5d  Y:%5d  Z:%5d",
			P_GetMapWarpTrans(gamemap), gamemap,
			player->plr->mo->x >> FRACBITS, player->plr->mo->y >> FRACBITS,
			player->plr->mo->z >> FRACBITS);
	P_SetMessage(player, textBuffer);
}

static void CheatScriptFunc1(player_t *player, Cheat_t * cheat)
{
	P_SetMessage(player, "RUN WHICH SCRIPT(01-99)?");
}

static void CheatScriptFunc2(player_t *player, Cheat_t * cheat)
{
	P_SetMessage(player, "RUN WHICH SCRIPT(01-99)?");
}

static void CheatScriptFunc3(player_t *player, Cheat_t * cheat)
{
	int     script;
	byte    args[3];
	int     tens, ones;
	char    textBuffer[40];

	tens = cheat->args[0] - '0';
	ones = cheat->args[1] - '0';
	script = tens * 10 + ones;
	if(script < 1)
		return;
	if(script > 99)
		return;
	args[0] = args[1] = args[2] = 0;

	if(P_StartACS(script, 0, args, player->plr->mo, NULL, 0))
	{
		sprintf(textBuffer, "RUNNING SCRIPT %.2d", script);
		P_SetMessage(player, textBuffer);
	}
}

extern int cheating;

static void CheatRevealFunc(player_t *player, Cheat_t * cheat)
{
	cheating = (cheating + 1) % 3;
}

//===========================================================================
//
// CheatTrackFunc1
//
//===========================================================================

static void CheatTrackFunc1(player_t *player, Cheat_t * cheat)
{
	/*  char buffer[80];

	   if(!i_CDMusic)
	   {
	   return;
	   }
	   if(gi.CD(DD_INIT, 0) == -1)
	   {
	   P_SetMessage(player, "ERROR INITIALIZING CD");
	   }
	   sprintf(buffer, "ENTER DESIRED CD TRACK (%.2d - %.2d):\n",
	   gi.CD(DD_GET_FIRST_TRACK, 0), gi.CD(DD_GET_LAST_TRACK, 0));  
	   P_SetMessage(player, buffer); */
}

//===========================================================================
//
// CheatTrackFunc2
//
//===========================================================================

static void CheatTrackFunc2(player_t *player, Cheat_t * cheat)
{
	/*  char buffer[80];
	   int track;

	   if(!i_CDMusic)
	   {
	   return;
	   }
	   track = (cheat->args[0]-'0')*10+(cheat->args[1]-'0');
	   if(track < gi.CD(DD_GET_FIRST_TRACK, 0) || track > gi.CD(DD_GET_LAST_TRACK, 0))
	   {
	   P_SetMessage(player, "INVALID TRACK NUMBER\n");
	   return;
	   } 
	   if(track == gi.CD(DD_GET_CURRENT_TRACK,0))
	   {
	   return;
	   }
	   if(gi.CD(DD_PLAY_LOOP, track))
	   {
	   sprintf(buffer, "ERROR WHILE TRYING TO PLAY CD TRACK: %.2d\n", track);
	   P_SetMessage(player, buffer);
	   }
	   else
	   { // No error encountered while attempting to play the track
	   sprintf(buffer, "PLAYING TRACK: %.2d\n", track);
	   P_SetMessage(player, buffer);  
	   } */
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
		Con_Printf("Usage: cheat (cheat)\nFor example, 'cheat visit21'.\n");
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

	// Check the arguments.
	if(argc != 2 && argc != 3)
		tellUsage = true;
	else if(!strnicmp(argv[1], "weapons", 1))
		CheatWeaponsFunc(players + target, NULL);
	else if(!strnicmp(argv[1], "health", 1))
		CheatHealthFunc(players + target, NULL);
	else if(!strnicmp(argv[1], "keys", 1))
		CheatKeysFunc(players + target, NULL);
	else if(!strnicmp(argv[1], "artifacts", 1))
		CheatArtifactAllFunc(players + target, NULL);
	else if(!strnicmp(argv[1], "puzzle", 1))
		CheatPuzzleFunc(players + target, NULL);
	else
		tellUsage = true;

	if(tellUsage)
	{
		Con_Printf("Usage: give weapons/health/keys/artifacts/puzzle\n");
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
	if(argc != 2)
	{
		Con_Printf("Usage: warp (num)\n");
		return true;
	}
	num = atoi(argv[1]);
	cheat.args[0] = num / 10 + '0';
	cheat.args[1] = num % 10 + '0';
	// We don't want that keys are repeated while we wait.
	DD_ClearKeyRepeaters();
	CheatWarpFunc(players + consoleplayer, &cheat);
	return true;
}

int CCmdCheatPig(int argc, char **argv)
{
	if(!canCheat())
		return false;			// Can't cheat!
	CheatPigFunc(players + consoleplayer, NULL);
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

int CCmdCheatShadowcaster(int argc, char **argv)
{
	Cheat_t cheat;

	if(!canCheat())
		return false;			// Can't cheat!
	if(argc != 2)
	{
		Con_Printf("Usage: class (0-2)\n");
		Con_Printf("0=Fighter, 1=Cleric, 2=Mage.\n");
		return true;
	}
	cheat.args[0] = atoi(argv[1]) + '0';
	CheatClassFunc2(players + consoleplayer, &cheat);
	return true;
}

int CCmdCheatWhere(int argc, char **argv)
{
	//if(!canCheat()) return false; // Can't cheat!
	CheatDebugFunc(players + consoleplayer, NULL);
	return true;
}

int CCmdCheatRunScript(int argc, char **argv)
{
	Cheat_t cheat;
	int     num;

	if(!canCheat())
		return false;			// Can't cheat!
	if(argc != 2)
	{
		Con_Printf("Usage: runscript (1-99)\n");
		return true;
	}
	num = atoi(argv[1]);
	cheat.args[0] = num / 10 + '0';
	cheat.args[1] = num % 10 + '0';
	CheatScriptFunc3(players + consoleplayer, &cheat);
	return true;
}

int CCmdCheatReveal(int argc, char **argv)
{
	int     option;

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
