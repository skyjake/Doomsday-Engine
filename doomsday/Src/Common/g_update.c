
//**************************************************************************
//**
//** G_UPDATE.C
//**
//** Routines to call when updating the state of the engine
//** (when loading/unloading WADs and definitions).
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#if __JDOOM__
#include "doomdef.h"
#include "doomstat.h"
#include "p_setup.h"
#include "p_local.h"
#include "m_menu.h"
#include "dstrings.h"
#include "s_sound.h"
#endif

#if __JHERETIC__
#include "Doomdef.h"
#include "S_sound.h"
#include "Soundst.h"
#endif

#if __JHEXEN__
#include "h2def.h"
#include "p_local.h"
#endif

#include <ctype.h>
#include "hu_pspr.h"

// MACROS ------------------------------------------------------------------

#if __JHEXEN__
#define thinkercap					(*gi.thinkercap)
#endif

#define MANGLE_STATE(x)		((state_t*) ((x)? (x)-states : -1))
#define RESTORE_STATE(x)	((int)(x)==-1? NULL : &states[(int)(x)])

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

#if __JHEXEN__
void S_InitScript(void);
#endif

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

//===========================================================================
// ScanWord
//===========================================================================
static char *ScanWord(char *ptr, char *buf)
{
	if(ptr)
	{
		// Skip whitespace at beginning.
		while(*ptr && isspace(*ptr)) ptr++;
		while(*ptr && !isspace(*ptr)) *buf++ = *ptr++;
	}
	*buf = 0;
	return ptr;
}

//===========================================================================
// G_SetGlowing
//===========================================================================
void G_SetGlowing(void)
{
	char	*ptr;
	char	buf[50];
	
	if(!ArgCheck("-noglow"))
	{
		ptr = GET_TXT(TXT_RENDER_GLOWFLATS);
		// Set some glowing textures.
		for(ptr = ScanWord(ptr, buf); *buf; ptr = ScanWord(ptr, buf))
		{
			// Is there such a flat?
			if(W_CheckNumForName(buf) == -1) continue;
			Set(DD_TEXTURE_GLOW, 
				DD_TGLOW_PARM(R_FlatNumForName(buf), false, true));
		}
		ptr = GET_TXT(TXT_RENDER_GLOWTEXTURES);
		for(ptr = ScanWord(ptr, buf); *buf; ptr = ScanWord(ptr, buf))
		{
			// Is there such a flat?
			if(R_CheckTextureNumForName(buf) == -1) continue;
			Set(DD_TEXTURE_GLOW, 
				DD_TGLOW_PARM(R_TextureNumForName(buf), 
				true, true));
		}
	}
}

//===========================================================================
// G_MangleState
//	Called before the engine re-inits the definitions. After that all the
//	state, info, etc. pointers will be obsolete.
//===========================================================================
void G_MangleState(void)
{
	thinker_t *it;
	mobj_t *mo;
	int i, k;

	for(it = thinkercap.next; it != &thinkercap && it; it = it->next)
	{
		if(it->function != P_MobjThinker) continue;
		mo = (mobj_t*) it;
		mo->state = MANGLE_STATE(mo->state);
		mo->info = (mobjinfo_t*) (mo->info - mobjinfo);
	}
	for(i = 0; i < MAXPLAYERS; i++)
		for(k = 0; k < NUMPSPRITES; k++)
			players[i].psprites[k].state = MANGLE_STATE
				(players[i].psprites[k].state);
}

//===========================================================================
// G_RestoreState
//===========================================================================
void G_RestoreState(void)
{
	thinker_t *it;
	mobj_t *mo;
	int i, k;

	for(it = thinkercap.next; it != &thinkercap && it; it = it->next)
	{
		if(it->function != P_MobjThinker) continue;
		mo = (mobj_t*) it;
		mo->state = RESTORE_STATE(mo->state);
		mo->info = &mobjinfo[ (int) mo->info];
	}
	for(i = 0; i < MAXPLAYERS; i++)
		for(k = 0; k < NUMPSPRITES; k++)
			players[i].psprites[k].state = RESTORE_STATE
				(players[i].psprites[k].state);
	HU_UpdatePsprites();
}

//===========================================================================
// G_UpdateState
//	Handles engine updates and renderer restarts.
//===========================================================================
void G_UpdateState(int step)
{
	switch(step)
	{
	case DD_GAME_MODE:
		// Set the game mode string.
#ifdef __JDOOM__
		D_IdentifyVersion();
#elif __JHERETIC__
		H_IdentifyVersion();
#else // __JHEXEN__
		H2_IdentifyVersion();
#endif
		break;

	case DD_PRE:
		G_MangleState();
		break;
	
	case DD_POST:
		G_RestoreState();
		P_Init();
#if __JDOOM__
		// FIXME: Detect gamemode changes (doom -> doom2, for instance).
		XG_Update();
		M_Init();
		S_LevelMusic();
#elif __JHERETIC__
		XG_Update();
		SB_Init(); // Updates the status bar patches.
		MN_Init();
		S_LevelMusic();
#elif __JHEXEN__
		SB_Init(); // Updates the status bar patches.
		MN_Init();
		S_InitScript();
		SN_InitSequenceScript();
#endif
		G_SetGlowing();
		break;

	case DD_RENDER_RESTART_PRE:
#if __JDOOM__
		// Free the menufog texture.
		M_UnloadData();
#endif
		break;

	case DD_RENDER_RESTART_POST:
#if __JDOOM__
		// Reload the menufog texture.
		M_LoadData();
#endif
		break;
	}
}

