
//**************************************************************************
//**
//** p_anim.c : Heretic 2 : Raven Software, Corp.
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
#include "settings.h"

// MACROS ------------------------------------------------------------------

#define ANIM_SCRIPT_NAME "ANIMDEFS"
#define MAX_ANIM_DEFS 20
#define MAX_FRAME_DEFS 96
#define ANIM_FLAT 0
#define ANIM_TEXTURE 1
#define SCI_FLAT "flat"
#define SCI_TEXTURE "texture"
#define SCI_PIC "pic"
#define SCI_TICS "tics"
#define SCI_RAND "rand"

#define LIGHTNING_SPECIAL 	198
#define LIGHTNING_SPECIAL2 	199
#define SKYCHANGE_SPECIAL 	200

// TYPES -------------------------------------------------------------------

typedef struct
{
	int index;
	int tics;
} frameDef_t;

typedef struct
{
	int type;
	int index;
	int tics;
	int currentFrameDef;
	int startFrameDef;
	int endFrameDef;
} animDef_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void P_LightningFlash(void);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern fixed_t Sky1ColumnOffset;
extern fixed_t Sky2ColumnOffset;
extern int Sky1Texture;
extern boolean DoubleSky;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int Sky1Texture;
int Sky2Texture;
fixed_t Sky1ColumnOffset;
fixed_t Sky2ColumnOffset;
fixed_t Sky1ScrollDelta;
fixed_t Sky2ScrollDelta;
boolean DoubleSky;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean LevelHasLightning;
static int NextLightningFlash;
static int LightningFlash;
static int *LightningLightLevels;

// CODE --------------------------------------------------------------------

//==========================================================================
//
// P_AnimateSurfaces
//
//==========================================================================

void P_AnimateSurfaces(void)
{
	int i;
	line_t *line;

	// Update scrolling textures
	for(i = 0; i < numlinespecials; i++)
	{
		line = linespeciallist[i];
		switch(line->special)
		{
			case 100: // Scroll_Texture_Left
				sides[line->sidenum[0]].textureoffset += line->arg1<<10;
				break;
			case 101: // Scroll_Texture_Right
				sides[line->sidenum[0]].textureoffset -= line->arg1<<10;
				break;
			case 102: // Scroll_Texture_Up
				sides[line->sidenum[0]].rowoffset += line->arg1<<10;
				break;
			case 103: // Scroll_Texture_Down
				sides[line->sidenum[0]].rowoffset -= line->arg1<<10;
				break;
		}
	}

	// Update sky column offsets
	Sky1ColumnOffset += Sky1ScrollDelta;
	Sky2ColumnOffset += Sky2ScrollDelta;
	Rend_SkyParams(1, DD_OFFSET, FIX2FLT(Sky1ColumnOffset));
	Rend_SkyParams(0, DD_OFFSET, FIX2FLT(Sky2ColumnOffset));

	if(LevelHasLightning)
	{
		if(!NextLightningFlash || LightningFlash)
		{
			P_LightningFlash();
		}
		else
		{
			NextLightningFlash--;
		}
	}
}

//==========================================================================
//
// P_LightningFlash
//
//==========================================================================

static void P_LightningFlash(void)
{
	int i;
	sector_t *tempSec;
	int *tempLight;
	boolean foundSec;
	int flashLight;

	if(LightningFlash)
	{
		LightningFlash--;
		if(LightningFlash)
		{
			tempLight = LightningLightLevels;
			tempSec = sectors;
			for(i = 0; i < numsectors; i++, tempSec++)
			{
				if(tempSec->ceilingpic == skyflatnum 
					|| tempSec->special == LIGHTNING_SPECIAL
					|| tempSec->special == LIGHTNING_SPECIAL2)
				{
					if(*tempLight < tempSec->lightlevel-4)
					{
						tempSec->lightlevel -= 4;
					}
					tempLight++;
				}
			}
		}					
		else
		{ // remove the alternate lightning flash special
			tempLight = LightningLightLevels;
			tempSec = sectors;
			for(i = 0; i < numsectors; i++, tempSec++)
			{
				if(tempSec->ceilingpic == skyflatnum
					|| tempSec->special == LIGHTNING_SPECIAL
					|| tempSec->special == LIGHTNING_SPECIAL2)
				{
					tempSec->lightlevel = *tempLight;
					tempLight++;
				}
			}
			Rend_SkyParams(1, DD_DISABLE, 0);
			Rend_SkyParams(0, DD_ENABLE, 0);
			//Sky1Texture = P_GetMapSky1Texture(gamemap);		
		}
		return;
	}
	LightningFlash = (P_Random()&7)+8;
	flashLight = 200+(P_Random()&31);
	tempSec = sectors;
	tempLight = LightningLightLevels;
	foundSec = false;
	for(i = 0; i < numsectors; i++, tempSec++)
	{
		if(tempSec->ceilingpic == skyflatnum
					|| tempSec->special == LIGHTNING_SPECIAL
					|| tempSec->special == LIGHTNING_SPECIAL2)
		{
			*tempLight = tempSec->lightlevel;
			if(tempSec->special == LIGHTNING_SPECIAL)
			{ 
				tempSec->lightlevel += 64;
				if(tempSec->lightlevel > flashLight)
				{
					tempSec->lightlevel = flashLight;
				}
			}
			else if(tempSec->special == LIGHTNING_SPECIAL2)
			{
				tempSec->lightlevel += 32;
				if(tempSec->lightlevel > flashLight)
				{
					tempSec->lightlevel = flashLight;
				}
			}
			else
			{
				tempSec->lightlevel = flashLight;
			}
			if(tempSec->lightlevel < *tempLight)
			{
				tempSec->lightlevel = *tempLight;
			}
			tempLight++;
			foundSec = true;
		}
	}
	if(foundSec)
	{
		mobj_t *plrmo = players[displayplayer].plr->mo;
		mobj_t *crashorigin = NULL;
		// Set the alternate (lightning) sky.
		Rend_SkyParams(0, DD_DISABLE, 0);
		Rend_SkyParams(1, DD_ENABLE, 0);
		// If 3D sounds are active, position the clap somewhere above
		// the player.
		if(cfg.snd_3D && plrmo)
		{
			// SpawnMobj calls P_Random, and we don't want that the 
			// random number generator gets out of sync.
			//P_SaveRandom(); 
			crashorigin = P_SpawnMobj(plrmo->x + (16*(M_Random()-127) << FRACBITS),
				plrmo->y + (16*(M_Random()-127) << FRACBITS),
				plrmo->z + (4000 << FRACBITS), MT_CAMERA);
			//P_RestoreRandom();
			crashorigin->tics = 5 * 35;		// Five seconds will do.
		}
		// Make it loud!
		S_StartSound(SFX_THUNDER_CRASH | DDSF_NO_ATTENUATION, crashorigin);
	}
	// Calculate the next lighting flash
	if(!NextLightningFlash)
	{
		if(P_Random() < 50)
		{ // Immediate Quick flash
			NextLightningFlash = (P_Random()&15)+16;
		}
		else
		{
			if(P_Random() < 128 && !(leveltime&32))
			{
				NextLightningFlash = ((P_Random()&7)+2)*35;
			}
			else
			{
				NextLightningFlash = ((P_Random()&15)+5)*35;
			}
		}
	}
}

//==========================================================================
//
// P_ForceLightning
//
//==========================================================================

void P_ForceLightning(void)
{
	NextLightningFlash = 0;
}

//==========================================================================
//
// P_InitLightning
//
//==========================================================================

void P_InitLightning(void)
{
	int i;
	int secCount;

	if(!P_GetMapLightning(gamemap))
	{
		LevelHasLightning = false;
		LightningFlash = 0;
		return;
	}
	LightningFlash = 0;
	secCount = 0;
	for(i = 0; i < numsectors; i++)
	{
		if(sectors[i].ceilingpic == skyflatnum 
			|| sectors[i].special == LIGHTNING_SPECIAL
			|| sectors[i].special == LIGHTNING_SPECIAL2)
		{
			secCount++;
		}
	}
	if(secCount)
	{
		LevelHasLightning = true;
	}
	else
	{
		LevelHasLightning = false;
		return;
	}
	LightningLightLevels = (int *) Z_Malloc(secCount*sizeof(int), PU_LEVEL,
		NULL);
	NextLightningFlash = ((P_Random()&15)+5)*35; // don't flash at level start
}

//==========================================================================
//
// P_InitFTAnims
//
// Initialize flat and texture animation lists.
//
//==========================================================================

void P_InitFTAnims(void)
{
	int base;
	boolean ignore;
	boolean done;
	int AnimDefCount = 0;
	int groupNumber, picBase;
	int type, index;

	SC_Open(ANIM_SCRIPT_NAME);
	while(SC_GetString())
	{
		if(AnimDefCount == MAX_ANIM_DEFS)
		{
			Con_Error("P_InitFTAnims: too many AnimDefs.");
		}
		if(SC_Compare(SCI_FLAT))
		{
			type = ANIM_FLAT;
		}
		else if(SC_Compare(SCI_TEXTURE))
		{
			type = ANIM_TEXTURE;
		}
		else
		{
			SC_ScriptError(NULL);
		}
		SC_MustGetString(); // Name
		ignore = false;
		if(type == ANIM_FLAT)
		{
			if(W_CheckNumForName(sc_String) == -1)
			{
				ignore = true;
			}
			else
			{
				picBase = R_FlatNumForName(sc_String);
				groupNumber = R_CreateAnimGroup(DD_FLAT, 
					AGF_SMOOTH | AGF_FIRST_ONLY);
			}
		}
		else
		{ // Texture
			if(R_CheckTextureNumForName(sc_String) == -1)
			{
				ignore = true;
			}
			else
			{
				picBase = R_TextureNumForName(sc_String);
				groupNumber = R_CreateAnimGroup(DD_TEXTURE,
					AGF_SMOOTH | AGF_FIRST_ONLY);
			}
		}

		done = false;
		while(done == false)
		{
			if(SC_GetString())
			{
				if(SC_Compare(SCI_PIC))
				{
					SC_MustGetNumber();
					if(ignore == false)
					{
						index = picBase + sc_Number - 1;
					}
					SC_MustGetString();
					if(SC_Compare(SCI_TICS))
					{
						SC_MustGetNumber();
						if(ignore == false)
						{
							R_AddToAnimGroup(groupNumber, index, 
								sc_Number, 0);
						}
					}
					else if(SC_Compare(SCI_RAND))
					{
						SC_MustGetNumber();
						base = sc_Number;
						SC_MustGetNumber();
						if(ignore == false)
						{
							R_AddToAnimGroup(groupNumber, index, base, 
								sc_Number - base);
						}
					}
					else
					{
						SC_ScriptError(NULL);
					}
				}
				else
				{
					SC_UnGet();
					done = true;
				}
			}
			else
			{
				done = true;
			}
		}
	}
	SC_Close();
}

//
// Sky code
//
void P_InitSky(int map)
{
	Sky1Texture = P_GetMapSky1Texture(map);
	Sky2Texture = P_GetMapSky2Texture(map);
	Sky1ScrollDelta = P_GetMapSky1ScrollDelta(map);
	Sky2ScrollDelta = P_GetMapSky2ScrollDelta(map);
	Sky1ColumnOffset = 0;
	Sky2ColumnOffset = 0;
	DoubleSky = P_GetMapDoubleSky(map);

	// First disable all sky layers.
	Rend_SkyParams(DD_SKY, DD_DISABLE, 0);				

	// Sky2 is layer zero and Sky1 is layer one.
	Rend_SkyParams(0, DD_OFFSET, 0);
	Rend_SkyParams(1, DD_OFFSET, 0);
	if(DoubleSky)
	{
		Rend_SkyParams(0, DD_ENABLE, 0);
		Rend_SkyParams(0, DD_MASK, DD_NO);
		Rend_SkyParams(0, DD_TEXTURE, Sky2Texture);
		
		Rend_SkyParams(1, DD_ENABLE, 0);
		Rend_SkyParams(1, DD_MASK, DD_YES);
		Rend_SkyParams(1, DD_TEXTURE, Sky1Texture);
	}
	else
	{
		Rend_SkyParams(0, DD_ENABLE, 0);
		Rend_SkyParams(0, DD_MASK, DD_NO);
		Rend_SkyParams(0, DD_TEXTURE, Sky1Texture);
		
		Rend_SkyParams(1, DD_DISABLE, 0);
		Rend_SkyParams(1, DD_MASK, DD_NO);
		Rend_SkyParams(1, DD_TEXTURE, Sky2Texture);
	}
}

