
//**************************************************************************
//**
//** F_INFINE.C
//**
//** The "In Fine" finale engine.
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include <ctype.h>

#if __JDOOM__
#include "doomdef.h"
#include "doomstat.h"
#include "d_netJD.h"
#include "m_swap.h"
#include "v_video.h"
#include "s_sound.h"
#include "hu_stuff.h"

#elif __JHERETIC__
#include "Doomdef.h"
#include "Am_map.h"
#include "S_sound.h"
#include "Soundst.h"

#elif __JHEXEN__
#include "h2def.h"
#include "am_map.h"
#include "settings.h"
#endif

#include "f_infine.h"

// MACROS ------------------------------------------------------------------

#define STACK_SIZE		16		// Size of the InFine state stack.
#define MAX_TOKEN_LEN	8192
#define MAX_SEQUENCE	64
#define MAX_PICS		128
#define MAX_TEXT		64
#define MAX_HANDLERS	128

#define FI_REPEAT		-2

// TYPES -------------------------------------------------------------------

typedef char handle_t[32];

typedef struct {
	char *token;
	int operands;
	void (*func)(void);
	boolean when_skipping;
	boolean when_cond_skipping;	// Skipping because condition failed.
} ficmd_t;

typedef struct {
	float value;
	float target;
	int steps;
} fivalue_t;

typedef struct {
	boolean used;
	handle_t handle;
	fivalue_t color[4];
	fivalue_t scale[2];
	fivalue_t x, y;
	fivalue_t angle;
	//byte centered[2];		// Should be centered in X/Y direction?
} fiobj_t;

typedef struct {
	fiobj_t object;
	struct {
		char is_patch : 1;	// Raw image or patch.
		char done : 1;		// Animation finished (or repeated).
		char is_rect : 1;
	} flags;
	int seq;
	int seq_wait[MAX_SEQUENCE], seq_timer;
	short lump[MAX_SEQUENCE];
	char flip[MAX_SEQUENCE];
	short sound[MAX_SEQUENCE];

	// For rectangle-objects.
	fivalue_t other_color[4];
	fivalue_t edge_color[4];
	fivalue_t other_edge_color[4];
} fipic_t;

typedef struct {
	fiobj_t object;
	struct { 
		char centered : 1;
		char font_b : 1;
		char all_visible : 1;
	} flags;
	int scroll_wait, scroll_timer; // Automatic scrolling upwards.
	int pos;
	int wait, timer;
	int lineheight;
	char *text;
} fitext_t;

typedef struct {
	int code;
	handle_t marker;
} fihandler_t;

typedef struct {
	char *script;		// A copy of the script.
	char *cp;			// The command cursor.
	infinemode_t mode;
	int overlay_gamestate;	// Overlay scripts run only in one gamemode.
	int timer;
	boolean conditions[NUM_FICONDS];
	int intime;
	boolean canskip, skipping;
	int dolevel;		// Level of DO-skipping.
	int wait;
	boolean suspended, paused, eatevents, showmenu;
	boolean gotoskip, skipnext, lastskipped;
	handle_t gototarget;
	fitext_t *waitingtext;
	fipic_t *waitingpic;
	fihandler_t keyhandlers[MAX_HANDLERS];
	int bgflat;
	fivalue_t bgcolor[4];
	fivalue_t imgcolor[4];
	fivalue_t imgoffset[2];
	fivalue_t filter[4];
	fivalue_t textcolor[9][3];
	fipic_t pics[MAX_PICS];
	fitext_t text[MAX_TEXT];
} fistate_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void FI_InitValue(fivalue_t *val, float num);
int FI_TextObjectLength(fitext_t *tex);

// Command functions.
void FIC_Do(void);
void FIC_End(void);
void FIC_BGFlat(void);
void FIC_NoBGFlat(void);
void FIC_Wait(void);
void FIC_WaitText(void);
void FIC_WaitAnim(void);
void FIC_Tic(void);
void FIC_InTime(void);
void FIC_Color(void);
void FIC_ColorAlpha(void);
void FIC_Pause(void);
void FIC_CanSkip(void);
void FIC_NoSkip(void);
void FIC_SkipHere(void);
void FIC_Events(void);
void FIC_NoEvents(void);
void FIC_OnKey(void);
void FIC_UnsetKey(void);
void FIC_If(void);
void FIC_IfNot(void);
void FIC_Else(void);
void FIC_GoTo(void);
void FIC_Marker(void);
void FIC_Image(void);
void FIC_ImageAt(void);
void FIC_Delete(void);
void FIC_Patch(void);
void FIC_SetPatch(void);
void FIC_Anim(void);
void FIC_AnimImage(void);
void FIC_StateAnim(void);
void FIC_Repeat(void);
void FIC_ClearAnim(void);
void FIC_PicSound(void);
void FIC_ObjectOffX(void);
void FIC_ObjectOffY(void);
void FIC_ObjectScaleX(void);
void FIC_ObjectScaleY(void);
void FIC_ObjectScale(void);
void FIC_ObjectScaleXY(void);
void FIC_ObjectRGB(void);
void FIC_ObjectAlpha(void);
void FIC_ObjectAngle(void);
void FIC_Rect(void);
void FIC_FillColor(void);
void FIC_EdgeColor(void);
void FIC_OffsetX(void);
void FIC_OffsetY(void);
void FIC_Sound(void);
void FIC_SoundAt(void);
void FIC_SeeSound(void);
void FIC_DieSound(void);
void FIC_Music(void);
void FIC_MusicOnce(void);
void FIC_Filter(void);
void FIC_Text(void);
void FIC_TextFromDef(void);
void FIC_TextFromLump(void);
void FIC_SetText(void);
void FIC_SetTextDef(void);
void FIC_DeleteText(void);
void FIC_FontA(void);
void FIC_FontB(void);
void FIC_TextColor(void);
void FIC_TextRGB(void);
void FIC_TextAlpha(void);
void FIC_TextOffX(void);
void FIC_TextOffY(void);
void FIC_TextCenter(void);
void FIC_TextNoCenter(void);
void FIC_TextScroll(void);
void FIC_TextPos(void);
void FIC_TextRate(void);
void FIC_TextLineHeight(void);
void FIC_NoMusic(void);
void FIC_TextScaleX(void);
void FIC_TextScaleY(void);
void FIC_TextScale(void);
void FIC_PlayDemo(void);
void FIC_Command(void);
void FIC_ShowMenu(void);
void FIC_NoShowMenu(void);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int		actual_leveltime;
extern boolean	secretexit;

#if __JHEXEN__
extern int		LeaveMap;
#endif

// PUBLIC DATA DEFINITIONS -------------------------------------------------

boolean brief_disabled = false;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

boolean fi_active = false;
boolean fi_cmd_executed = false;	// Set to true after first command.

// Time is measured in seconds.
// Colors are floating point and [0,1].
static ficmd_t fi_commands[] =
{
	// Run Control
	{ "DO", 0,			FIC_Do, true, true },
	{ "END", 0,			FIC_End },		
	{ "IF", 1,			FIC_If },		// if (value-id)
	{ "IFNOT", 1,		FIC_IfNot },	// ifnot (value-id)
	{ "ELSE", 0,		FIC_Else },
	{ "GOTO", 1,		FIC_GoTo },	// goto (marker)
	{ "MARKER", 1,		FIC_Marker, true },
	{ "in", 1,			FIC_InTime },	// in (time)
	{ "pause", 0,		FIC_Pause },	
	{ "tic", 0,			FIC_Tic },		
	{ "wait", 1,		FIC_Wait },		// wait (time)
	{ "waittext", 1,	FIC_WaitText },	// waittext (handle)
	{ "waitanim", 1,	FIC_WaitAnim }, // waitanim (handle)
	{ "canskip", 0,		FIC_CanSkip },
	{ "noskip", 0,		FIC_NoSkip },
	{ "skiphere", 0,	FIC_SkipHere, true },
	{ "events", 0,		FIC_Events },
	{ "noevents", 0,	FIC_NoEvents },
	{ "onkey", 2,		FIC_OnKey },	// onkey (keyname) (marker)
	{ "unsetkey", 1,	FIC_UnsetKey },	// unsetkey (keyname)
	
	// Screen Control
	{ "color", 3,		FIC_Color },	// color (red) (green) (blue)
	{ "coloralpha", 4,	FIC_ColorAlpha }, // coloralpha (r) (g) (b) (a)
	{ "flat", 1,		FIC_BGFlat },	// flat (flat-lump)
	{ "noflat", 0,		FIC_NoBGFlat },
	{ "offx", 1,		FIC_OffsetX },	// offx (x)
	{ "offy", 1,		FIC_OffsetY },	// offy (y)
	{ "filter", 4,		FIC_Filter },	// filter (r) (g) (b) (a)

	// Audio
	{ "sound", 1,		FIC_Sound },	// sound (snd)
	{ "soundat", 2,		FIC_SoundAt },	// soundat (snd) (vol:0-1)
	{ "seesound", 1,	FIC_SeeSound },	// seesound (mobjtype)
	{ "diesound", 1,	FIC_DieSound },	// diesound (mobjtype)
	{ "music", 1,		FIC_Music },	// music (musicname)
	{ "musiconce", 1,	FIC_MusicOnce }, // musiconce (musicname)
	{ "nomusic", 0,		FIC_NoMusic },

	// Objects
	{ "del", 1,			FIC_Delete }, // del (handle)
	{ "x", 2,			FIC_ObjectOffX },	// x (handle) (x)
	{ "y", 2,			FIC_ObjectOffY },	// y (handle) (y)
	{ "sx", 2,			FIC_ObjectScaleX }, // sx (handle) (x)
	{ "sy", 2,			FIC_ObjectScaleY }, // sy (handle) (y)
	{ "scale", 2,		FIC_ObjectScale },	// scale (handle) (factor)
	{ "scalexy", 3,		FIC_ObjectScaleXY }, // scalexy (handle) (x) (y)
	{ "rgb", 4,			FIC_ObjectRGB },	// rgb (handle) (r) (g) (b)
	{ "alpha", 2,		FIC_ObjectAlpha },	// alpha (handle) (alpha)
	{ "angle", 2,		FIC_ObjectAngle },	// angle (handle) (degrees)

	// Rects
	{ "rect", 5,		FIC_Rect },		// rect (hndl) (x) (y) (w) (h)
	{ "fillcolor", 6,	FIC_FillColor }, // fillcolor (h) (top/bottom/both) (r) (g) (b) (a)
	{ "edgecolor", 6,	FIC_EdgeColor }, // edgecolor (h) (top/bottom/both) (r) (g) (b) (a)

	// Pics
	{ "image", 2,		FIC_Image },	// image (handle) (raw-image-lump)
	{ "imageat", 4,		FIC_ImageAt },	// imageat (handle) (x) (y) (raw)
	{ "patch", 4,		FIC_Patch },	// patch (handle) (x) (y) (patch)
	{ "set", 2,			FIC_SetPatch },	// set (handle) (lump)
	{ "clranim", 1,		FIC_ClearAnim }, // clranim (handle)
	{ "anim", 3,		FIC_Anim },		// anim (handle) (patch) (time)
	{ "imageanim", 3,	FIC_AnimImage }, // imageanim (hndl) (raw-img) (time)
	{ "picsound", 2,	FIC_PicSound },	// picsound (hndl) (sound)
	{ "repeat", 1,		FIC_Repeat },	// repeat (handle)
	{ "states", 3,		FIC_StateAnim }, // states (handle) (state) (count)

	// Text	
	{ "text", 4,		FIC_Text },		// text (hndl) (x) (y) (string)
	{ "textdef", 4,		FIC_TextFromDef }, // textdef (hndl) (x) (y) (txt-id)
	{ "textlump", 4,	FIC_TextFromLump }, // textlump (hndl) (x) (y) (lump)
	{ "settext", 2,		FIC_SetText },	// settext (handle) (newtext)
	{ "settextdef", 2,	FIC_SetTextDef }, // settextdef (handle) (txt-id)
	{ "precolor", 4,	FIC_TextColor }, // precolor (num) (r) (g) (b)
	{ "center", 1,		FIC_TextCenter }, // center (handle)
	{ "nocenter", 1,	FIC_TextNoCenter }, // nocenter (handle)
	{ "scroll", 2,		FIC_TextScroll }, // scroll (handle) (speed)
	{ "pos", 2,			FIC_TextPos },	// pos (handle) (pos)
	{ "rate", 2,		FIC_TextRate }, // rate (handle) (rate)
	{ "fonta", 1,		FIC_FontA },	// fonta (handle)
	{ "fontb", 1,		FIC_FontB },	// fontb (handle)
	{ "linehgt", 2,		FIC_TextLineHeight }, // linehgt (hndl) (hgt)

	// Game Control
	{ "playdemo", 1,	FIC_PlayDemo },	// playdemo (filename)
	{ "cmd", 1,			FIC_Command },	// cmd (console command)
	{ "trigger", 0,		FIC_ShowMenu },
	{ "notrigger", 0,	FIC_NoShowMenu },

	// Deprecated Pic commands
	{ "delpic", 1,		FIC_Delete },	// delpic (handle)

	// Deprecated Text commands
	{ "deltext", 1,		FIC_DeleteText }, // deltext (hndl)
	{ "textrgb", 4,		FIC_TextRGB },	// textrgb (handle) (r) (g) (b)
	{ "textalpha", 2,	FIC_TextAlpha }, // textalpha (handle) (alpha)
	{ "tx", 2,			FIC_TextOffX },	// tx (handle) (x)
	{ "ty", 2,			FIC_TextOffY },	// ty (handle) (y)
	{ "tsx", 2,			FIC_TextScaleX }, // tsx (handle) (x)
	{ "tsy", 2,			FIC_TextScaleY }, // tsy (handle) (y)
	{ "textscale", 3,	FIC_TextScale }, // textscale (handle) (x) (y)

	{ NULL, 0, NULL }
};

static fistate_t fi_statestack[STACK_SIZE];
static fistate_t *fi;	// Pointer to the current state in the stack.
static char fi_token[MAX_TOKEN_LEN];
static fipic_t fi_dummypic;
static fitext_t fi_dummytext;

static boolean condition_presets[NUM_FICONDS];

#if !__JDOOM__
static int FontABase, FontBBase;
#endif

// CODE --------------------------------------------------------------------

//===========================================================================
// FI_ClearState
//	Clear the InFine state to the default, blank state.
//	The 'fi' pointer must be set before calling this function.
//===========================================================================
void FI_ClearState(void)
{
	int i, c;

#if !__JDOOM__
	players[consoleplayer].messageTics = 1;
#endif
#if __JHEXEN__
	strcpy(players[consoleplayer].message, "");
#else
	players[consoleplayer].message = NULL;
#endif

    // General game state.
	gameaction = ga_nothing;
    if(fi->mode != FIMODE_OVERLAY) 
	{
		gamestate = GS_INFINE;
		automapactive = false;
	}

	fi_active = true;
	fi_cmd_executed = false;// Nothing is drawn until a cmd has been executed.

	fi->suspended = false;
	fi->timer = 0;
	fi->canskip = true;		// By default skipping is enabled.
	fi->skipping = false;
	fi->wait = 0;			// Not waiting for anything.
	fi->intime = 0;			// Interpolation is off.
	fi->bgflat = -1;			// No background flat.
	fi->paused = false;
	fi->gotoskip = false;
	fi->skipnext = false;

	fi->waitingtext = NULL;
	fi->waitingpic = NULL;
	memset(fi->gototarget, 0, sizeof(fi->gototarget));
	GL_SetFilter(0);		// Clear the current filter.
	for(i = 0; i < 4; i++) 
	{
		FI_InitValue(fi->bgcolor + i, 1);
	}
	memset(fi->pics, 0, sizeof(fi->pics));
	memset(fi->imgoffset, 0, sizeof(fi->imgoffset));
	memset(fi->text, 0, sizeof(fi->text));
	memset(fi->filter, 0, sizeof(fi->filter));
	for(i = 0; i < 9; i++)
	{
		for(c = 0; c < 3; c++) FI_InitValue(&fi->textcolor[i][c], 1);
	}
}

//===========================================================================
// FI_NewState
//===========================================================================
void FI_NewState(const char *script)
{
	int size;

	if(!fi)
	{
		// Start from the bottom of the stack.
		fi = fi_statestack;
	}
	else
	{
		// Get the next state from the stack.
		fi++;

		if(fi == fi_statestack + STACK_SIZE)
		{
			Con_Error("FI_NewState: InFine state stack overflow.\n");
		}
	}

#ifdef _DEBUG
	// Is the stack leaking?
	Con_Printf("FI_NewState: Assigned index %i.\n", fi - fi_statestack);
#endif

	memset(fi, 0, sizeof(*fi));

	// Take a copy of the script.
	size = strlen(script) + 1;
	fi->script = Z_Malloc(size, PU_STATIC, 0);
	memcpy(fi->script, script, size);

	// Init the cursor, too.
	fi->cp = fi->script;
}

//===========================================================================
// FI_PopState
//===========================================================================
void FI_PopState(void)
{
	int i;

#ifdef _DEBUG
	Con_Printf("FI_PopState: fi=%p (%i)\n", fi, fi - fi_statestack);
#endif
	if(!fi)
	{
#ifdef _DEBUG
		Con_Printf("FI_PopState: Pop in NULL state!\n");
#endif
		return;
	}

	// Free the current state.
	Z_Free(fi->script);

	// Free all text strings.
	for(i = 0; i < MAX_TEXT; i++)
	{
		if(fi->text[i].text)
		{
			Z_Free(fi->text[i].text);
		}
	}

	memset(fi, 0, sizeof(*fi));

	// Should we go back to NULL?
	if(fi == fi_statestack)
	{
		fi = NULL;
		fi_active = false;
	}
	else
	{
		// Return to previous state.
		fi--;
	}
}

//===========================================================================
// FI_Reset
//	Reset the entire InFine state stack. This is called when a new game
//	is started.
//===========================================================================
void FI_Reset(void)
{
/*	if(IS_CLIENT)
	{
#ifdef _DEBUG
		Con_Printf("FI_Reset: Called in client mode! (not reseting)\n");
#endif
		return;
	}*/

	// The state is suspended when the PlayDemo command is used.
	// Being suspended means that InFine is currently not active, but 
	// will be restored at a later time.
	if(fi && fi->suspended) return;
	
	// Pop all the states. 
	while(fi) FI_PopState();	

	fi_active = false;
}

//===========================================================================
// FI_Start
//	Start playing the given script.
//===========================================================================
void FI_Start(char *finalescript, infinemode_t mode)
{
	int i;

	if(mode == FIMODE_LOCAL && IS_DEDICATED)
	{
		// Dedicated servers don't play local scripts.
#ifdef _DEBUG
		Con_Printf("FI_Start: No local scripts in dedicated mode.\n");
#endif
		return;
	}

#ifdef _DEBUG
	Con_Printf("FI_Start: mode=%i '%.30s'\n", mode, finalescript);
#endif

	// Init InFine state.
	FI_NewState(finalescript);
	fi->mode = mode;
	FI_ClearState();

	if(!IS_CLIENT)
	{
		// We are able to figure out the truth values of all the 
		// conditions.
		fi->conditions[FICOND_SECRET] = (secretexit != 0);

#ifdef __JHEXEN__
		// Current hub has been completed?
		fi->conditions[FICOND_LEAVEHUB] 
			= (P_GetMapCluster(gamemap) != P_GetMapCluster(LeaveMap));
#else
		// Only Hexen has hubs.
		fi->conditions[FICOND_LEAVEHUB] = false;
#endif
	}
	else
	{
		// Clients use the server-provided presets. We may not have 
		// enough info to figure out the real values otherwise.
		for(i = 0; i < NUM_FICONDS; i++)
		{
			fi->conditions[i] = condition_presets[i];
		}
	}

	if(mode == FIMODE_OVERLAY)
	{
		// Overlay scripts stop when the gamemode changes.
		fi->overlay_gamestate = gamestate;
	}
	
	if(mode != FIMODE_LOCAL)
	{
		// Tell clients to start this script.
		NetSv_Finale(FINF_BEGIN | 
			 (mode == FIMODE_AFTER? FINF_AFTER 
			: mode == FIMODE_OVERLAY? FINF_OVERLAY 
			: 0), finalescript, fi->conditions, NUM_FICONDS);
	}

#ifndef __JDOOM__
	FontABase = W_GetNumForName("FONTA_S") + 1;
	FontBBase = W_GetNumForName("FONTB_S") + 1;
#endif
	memset(&fi_dummytext, 0, sizeof(fi_dummytext));
}

//===========================================================================
// FI_End
//	Stop playing the script and go to next game state.
//===========================================================================
void FI_End(void)
{
	int oldMode;

	if(!fi_active || !fi->canskip) return;

	oldMode = fi->mode;

	// This'll set fi to NULL.
	FI_PopState();

#ifdef _DEBUG
	Con_Printf("FI_End\n");
#endif

	if(oldMode != FIMODE_LOCAL)
	{
		// Tell clients to stop the finale.
		NetSv_Finale(FINF_END, 0, NULL, 0);
	}

	// If no more scripts are left, go to the next game mode.
	if(!fi_active)
	{
		if(oldMode == FIMODE_AFTER) // A level has been completed.
		{
			if(IS_CLIENT) 
			{
#ifdef __JHEXEN__
				Draw_TeleportIcon();
#endif
				return;
			}
			gameaction = ga_completed;
		}
		else if(oldMode == FIMODE_BEFORE) 
		{
			// Enter the level, this was a briefing.
			gamestate = GS_LEVEL;
			levelstarttic = gametic;
			leveltime = actual_leveltime = 0;
			// Restart the current map's song.
			S_LevelMusic();
		}
		else if(oldMode == FIMODE_LOCAL)
		{
			gamestate = GS_WAITING;
		}
	}
}

//===========================================================================
// FI_SetCondition
//	Set the truth value of a condition. Used by clients after they've
//	received a GPT_FINALE2 packet.
//===========================================================================
void FI_SetCondition(int index, boolean value)
{
	if(index < 0 || index >= NUM_FICONDS) return;
	condition_presets[index] = value;
#ifdef _DEBUG
	Con_Printf("FI_SetCondition: %i = %s\n", index, value? "true" : "false");
#endif
}

//===========================================================================
// CCmdStartInFine
//===========================================================================
int CCmdStartInFine(int argc, char **argv)
{
	char *script;

	if(fi_active) return false;
	if(argc != 2)
	{
		Con_Printf("Usage: %s (script-id)\n", argv[0]);
		return true;
	}
	if(!Def_Get(DD_DEF_FINALE, argv[1], &script))
	{
		Con_Printf("Script \"%s\" is not defined.\n", argv[1]);
		return false;
	}
	// The overlay mode doesn't affect the current game mode.
	FI_Start(script, gamestate == GS_LEVEL? FIMODE_OVERLAY : FIMODE_LOCAL);
	return true;
}

//===========================================================================
// CCmdStopInFine
//===========================================================================
int CCmdStopInFine(int argc, char **argv)
{
	if(!fi_active) return false;
	fi->canskip = true;
	FI_End();
	return true;
}

//===========================================================================
// FI_GetMapID
//===========================================================================
void FI_GetMapID(char *dest, int ep, int map)
{
#if __JDOOM__
	if(gamemode == commercial)
		sprintf(dest, "MAP%02i", map);
	else
		sprintf(dest, "E%iM%i", ep, map);
#elif __JHERETIC__
	sprintf(dest, "E%iM%i", ep, map);
#elif __JHEXEN__
	sprintf(dest, "MAP%02i", map);
#endif
}

//===========================================================================
// FI_Briefing
//	Check if there is a finale before the map and play it.
//	Returns true if a finale was begun.
//===========================================================================
int FI_Briefing(int episode, int map)
{
	char mid[20];
	ddfinale_t fin;

	// If we're already in the INFINE state, don't start a finale.
	if(brief_disabled 
		|| gamestate == GS_INFINE 
		|| IS_CLIENT
		|| Get(DD_PLAYBACK)) return false; 

	// Is there such a finale definition?
	FI_GetMapID(mid, episode, map);

	if(!Def_Get(DD_DEF_FINALE_BEFORE, mid, &fin)) return false;

	FI_Start(fin.script, FIMODE_BEFORE);
	return true;
}

//===========================================================================
// FI_Debriefing
//	Check if there is a finale after the map and play it.
//	Returns true if a finale was begun.
//===========================================================================
int FI_Debriefing(int episode, int map)
{
	char mid[20];
	ddfinale_t fin;
	
	// If we're already in the INFINE state, don't start a finale.
	if(brief_disabled 
		|| gamestate == GS_INFINE 
		|| IS_CLIENT
		|| Get(DD_PLAYBACK)) return false; 

	// Is there such a finale definition?
	FI_GetMapID(mid, episode, map);
	if(!Def_Get(DD_DEF_FINALE_AFTER, mid, &fin)) return false;

	FI_Start(fin.script, FIMODE_AFTER);
	return true;
}

//===========================================================================
// FI_DemoEnds
//===========================================================================
void FI_DemoEnds(void)
{
	if(fi && fi->suspended)
	{
		// Restore the InFine state.
		fi->suspended = false;
		fi_active = true;
		gamestate = GS_INFINE;
		gameaction = ga_nothing;
		automapactive = false;
	}
}

//===========================================================================
// FI_GetToken
//===========================================================================
char *FI_GetToken(void)
{
	char *out;

	if(!fi) return NULL;

	// Skip whitespace.
	while(*fi->cp && isspace(*fi->cp)) fi->cp++;
	if(!*fi->cp) return NULL;	// The end has been reached.
	out = fi_token;
	if(*fi->cp == '"') // A string?
	{
		for(fi->cp++; *fi->cp; fi->cp++)
		{
			if(*fi->cp == '"')
			{
				fi->cp++;
				// Convert double quotes to single ones.
				if(*fi->cp != '"') break;
			}
			*out++ = *fi->cp;
		}
	}
	else
	{
		while(!isspace(*fi->cp) && *fi->cp) *out++ = *fi->cp++;
	}
	*out++ = 0;
	return fi_token;
}

//===========================================================================
// FI_GetInteger
//===========================================================================
int FI_GetInteger(void)
{
	return strtol(FI_GetToken(), NULL, 0);
}

//===========================================================================
// FI_GetFloat
//===========================================================================
float FI_GetFloat(void)
{
	return strtod(FI_GetToken(), NULL);
}

//===========================================================================
// FI_GetTics
//	Reads the next token, which should be floating point number. It is 
//	considered seconds, and converted to tics.
//===========================================================================
int FI_GetTics(void)
{
	return (int) (FI_GetFloat()*35 + 0.5);
}

//===========================================================================
// FI_Execute
//	Execute one (the next) command, advance script cursor.
//===========================================================================
void FI_Execute(char *cmd)
{
	int	i, k;
	char *oldcp;

	// Semicolon terminates DO-blocks.
	if(!strcmp(cmd, ";")) 
	{
		if(fi->dolevel > 0) 
		{
			if(--fi->dolevel == 0)
			{
				// The DO-skip has been completed.
				fi->skipnext = false;
				fi->lastskipped = true;
			}
		}
		return; 
	}

	// We're now going to execute a command.
	fi_cmd_executed = true;

	// Is this a command we know how to execute?
	for(i = 0; fi_commands[i].token; i++)
		if(!stricmp(cmd, fi_commands[i].token))
		{
			// Check that there are enough operands.
			// k stays at zero if the number of operands is correct.
			oldcp = fi->cp;
			for(k = fi_commands[i].operands; k > 0; k--)
				if(!FI_GetToken()) 
				{
					fi->cp = oldcp;
					Con_Message("FI_Execute: \"%s\" has too few operands.\n", 
						fi_commands[i].token);
					break;
				}
			// Should we skip this command?
			if((fi->skipnext && !fi_commands[i].when_cond_skipping)	||
			   ((fi->skipping || fi->gotoskip)
				&& !fi_commands[i].when_skipping)) 
			{
				// While not DO-skipping, the condskip has now been done.
				if(!fi->dolevel) 
				{
					if(fi->skipnext) fi->lastskipped = true;
					fi->skipnext = false;
				}
				return;
			}
			// If there were enough operands, execute the command.
			fi->cp = oldcp;
			if(!k) fi_commands[i].func();

			// The END command may clear the current state.
			if(!fi) return;

			// Now we've executed the latest command.
			fi->lastskipped = false;
			return;
		}
	// The command was not found!
	Con_Message("FI_Execute: Unknown command \"%s\".\n", cmd);
}

//===========================================================================
// FI_ExecuteNextCommand
//	Returns true if a command was found. Only returns false if there are
//	no more commands in the script.
//===========================================================================
int FI_ExecuteNextCommand(void)
{
	char *cmd = FI_GetToken();

	if(!cmd) return false;
	FI_Execute(cmd);
	return true;
}

//===========================================================================
// FI_InitValue
//===========================================================================
void FI_InitValue(fivalue_t *val, float num)
{
	val->target = val->value = num;
	val->steps = 0;
}

//===========================================================================
// FI_SetValue
//===========================================================================
void FI_SetValue(fivalue_t *val, float num)
{
	val->target = num;
	val->steps = fi->intime;
	if(!val->steps) val->value = val->target;
}

//===========================================================================
// FI_ValueThink
//===========================================================================
void FI_ValueThink(fivalue_t *val)
{
	if(val->steps <= 0)
	{
		val->steps = 0;
		val->value = val->target;
		return;
	}
	val->value += (val->target - val->value) / val->steps;
	val->steps--;
}

//===========================================================================
// FI_ValueArrayThink
//===========================================================================
void FI_ValueArrayThink(fivalue_t *val, int num)
{
	int i;
	for(i = 0; i < num; i++) FI_ValueThink(val + i);
}

//===========================================================================
// FI_GetHandler
//===========================================================================
fihandler_t *FI_GetHandler(int code)
{
	int i;
	fihandler_t *vacant = NULL;
	
	for(i = 0; i < MAX_HANDLERS; i++)
	{
		// Use this if a suitable handler is not already set?
		if(!vacant && !fi->keyhandlers[i].code) 
			vacant = fi->keyhandlers + i;

		if(fi->keyhandlers[i].code == code)
		{
			return fi->keyhandlers + i;
		}
	}

	// May be NULL, if no more handlers available.
	return vacant;
}

//===========================================================================
// FI_ClearAnimation
//===========================================================================
void FI_ClearAnimation(fipic_t *pic)
{
	memset(pic->lump, -1, sizeof(pic->lump));
	memset(pic->flip, 0, sizeof(pic->flip));
	memset(pic->sound, -1, sizeof(pic->sound));
	memset(pic->seq_wait, 0, sizeof(pic->seq_wait));
	pic->seq = 0;
	pic->flags.done = true;
}

//===========================================================================
// FI_GetNextSeq
//===========================================================================
int FI_GetNextSeq(fipic_t *pic)
{
	int i;

	for(i = 0; i < MAX_SEQUENCE; i++) if(pic->lump[i] <= 0) break;
	return i;
}

//===========================================================================
// FI_FindPic
//===========================================================================
fipic_t *FI_FindPic(const char *handle)
{
	int i;

	if(!handle) return NULL;

	for(i = 0; i < MAX_PICS; i++)
	{
		if(fi->pics[i].object.used
			&& !stricmp(fi->pics[i].object.handle, handle))
		{
			return fi->pics + i;
		}
	}
	return NULL;
}

//===========================================================================
// FI_FindText
//===========================================================================
fitext_t *FI_FindText(const char *handle)
{
	int i;

	for(i = 0; i < MAX_TEXT; i++)
	{
		if(fi->text[i].object.used
			&& !stricmp(fi->text[i].object.handle, handle))
		{
			return fi->text + i;
		}
	}
	return NULL;
}

//===========================================================================
// FI_FindObject
//===========================================================================
fiobj_t *FI_FindObject(const char *handle)
{
	fipic_t *pic;
	fitext_t *text;

	// First check all pics.
	if((pic = FI_FindPic(handle)) != NULL)
	{
		return &pic->object;
	}
		
	// Then check text objects.
	if((text = FI_FindText(handle)) != NULL)
	{
		return &text->object;
	}

	return NULL;
}

//===========================================================================
// FI_GetPic
//===========================================================================
fipic_t *FI_GetPic(const char *handle)
{
	int i;
	fipic_t *unused = NULL;

	for(i = 0; i < MAX_PICS; i++)
	{
		if(!fi->pics[i].object.used)
		{
			if(!unused) unused = fi->pics + i;
			continue;
		}
		if(!stricmp(fi->pics[i].object.handle, handle))
			return fi->pics + i;
	}
	
	// Allocate an empty one.
	if(!unused) 
	{
		Con_Message("FI_GetPic: No room for \"%s\".", handle);
		return &fi_dummypic; // No more space.
	}
	memset(unused, 0, sizeof(*unused));
	strncpy(unused->object.handle, handle, sizeof(unused->object.handle) - 1);
	unused->object.used = true;
	for(i = 0; i < 4; i++) FI_InitValue(&unused->object.color[i], 1);
	for(i = 0; i < 2; i++) FI_InitValue(&unused->object.scale[i], 1);
	FI_ClearAnimation(unused);
	return unused;
}

//===========================================================================
// FI_GetText
//===========================================================================
fitext_t *FI_GetText(char *handle)
{
	fitext_t *unused = NULL;
	int i;

	for(i = 0; i < MAX_TEXT; i++)
	{
		if(!fi->text[i].object.used)
		{
			if(!unused) unused = fi->text + i;
			continue;
		}
		if(!stricmp(fi->text[i].object.handle, handle))
			return fi->text + i;
	}
	
	// Allocate an empty one.
	if(!unused) 
	{
		Con_Message("FI_GetText: No room for \"%s\".", handle);
		return &fi_dummytext; // No more space.
	}
	if(unused->text) Z_Free(unused->text);
	memset(unused, 0, sizeof(*unused));
	strncpy(unused->object.handle, handle, sizeof(unused->object.handle) - 1);
	unused->object.used = true;
	unused->wait = 3;
#if __JDOOM__
	unused->lineheight = 11;
	FI_InitValue(&unused->object.color[0], 1); // Red text by default.
#else
	unused->lineheight = 9;
	// White text.
	for(i = 0; i < 3; i++) FI_InitValue(&unused->object.color[i], 1); 
#endif
	FI_InitValue(&unused->object.color[3], 1); // Opaque.
	for(i = 0; i < 2; i++) FI_InitValue(&unused->object.scale[i], 1);
	return unused;
}

//===========================================================================
// FI_SetText
//===========================================================================
void FI_SetText(fitext_t *tex, char *str)
{
	int len = strlen(str) + 1;

	if(tex->text) Z_Free(tex->text); // Free any previous text.
	tex->text = Z_Malloc(len, PU_STATIC, 0);
	memcpy(tex->text, str, len);
}

//===========================================================================
// FI_ObjectThink
//===========================================================================
void FI_ObjectThink(fiobj_t *obj)
{
	FI_ValueThink(&obj->x);
	FI_ValueThink(&obj->y);
	FI_ValueArrayThink(obj->scale, 2);
	FI_ValueArrayThink(obj->color, 4);
	FI_ValueThink(&obj->angle);
}

//===========================================================================
// FI_Ticker
//===========================================================================
void FI_Ticker(void)
{
	int i, k, last;
	fipic_t *pic;
	fitext_t *tex;

	if(!fi_active) return;

	if(fi->mode == FIMODE_OVERLAY)
	{
		// Has the game mode changed?
		if(fi->overlay_gamestate != gamestate)
		{
			// Overlay scripts don't survive this...
			FI_End();
			return;
		}
	}

	fi->timer++;
	
	// Interpolateable values.
	FI_ValueArrayThink(fi->bgcolor, 4);
	FI_ValueArrayThink(fi->imgoffset, 2);
	FI_ValueArrayThink(fi->filter, 4);
	for(i = 0; i < 9; i++) FI_ValueArrayThink(fi->textcolor[i], 3);
	for(i = 0, pic = fi->pics; i < MAX_PICS; i++, pic++)
	{
		if(!pic->object.used) continue;
		FI_ObjectThink(&pic->object);
		FI_ValueArrayThink(pic->other_color, 4);
		FI_ValueArrayThink(pic->edge_color, 4);
		FI_ValueArrayThink(pic->other_edge_color, 4);
		// If animating, decrease the sequence timer.
		if(pic->seq_wait[pic->seq])	
		{
			if(--pic->seq_timer <= 0)
			{
				// Advance the sequence position. k = next pos.
				k = pic->seq + 1;
				if(k == MAX_SEQUENCE 
					|| pic->lump[k] == FI_REPEAT)
				{
					// Rewind back to beginning.
					k = 0;
					pic->flags.done = true;
				}
				else if(pic->lump[k] <= 0)
				{
					// This is the end. Stop sequence.
					pic->seq_wait[k = pic->seq] = 0;
					pic->flags.done = true;
				}
				// Advance to the next pos.
				pic->seq = k;
				pic->seq_timer = pic->seq_wait[k];
				// Play a sound?
				if(pic->sound[k] > 0) S_LocalSound(pic->sound[k], NULL);
			}
		}
	}
	// Text objects.
	for(i = 0, tex = fi->text; i < MAX_TEXT; i++, tex++)
	{
		if(!tex->object.used) continue;
		FI_ObjectThink(&tex->object);
		if(tex->wait)
		{
			if(--tex->timer <= 0)
			{
				tex->timer = tex->wait;
				tex->pos++;
			}
		}
		if(tex->scroll_wait)
		{
			if(--tex->scroll_timer <= 0)
			{
				tex->scroll_timer = tex->scroll_wait;
				tex->object.y.target -= 1;
				tex->object.y.steps = tex->scroll_wait;
			}
		}
		// Is the text object fully visible?
		tex->flags.all_visible 
			= (!tex->wait || tex->pos >= FI_TextObjectLength(tex));
	}

	// If we're waiting, don't execute any commands.
	if(fi->wait && --fi->wait) return;

	// If we're paused we can't really do anything.
	if(fi->paused) return;

	// If we're waiting for a text to finish typing, do nothing.
	if(fi->waitingtext)
	{
		if(!fi->waitingtext->flags.all_visible) return;
		fi->waitingtext = NULL;
	}

	// Waiting for an animation to reach its end?
	if(fi->waitingpic)
	{
		if(!fi->waitingpic->flags.done) return;
		fi->waitingpic = NULL;
	}

	// Execute commands until a wait time is set or we reach the end of
	// the script. If the end is reached, the finale really ends (FI_End).
	while(fi_active 
		&& !fi->wait 
		&& !fi->waitingtext
		&& !fi->waitingpic
		&& !(last = !FI_ExecuteNextCommand()));
	
	// The script has ended!
	if(last) FI_End();
}

//===========================================================================
// FI_SkipTo
//===========================================================================
void FI_SkipTo(const char *marker)
{
	memset(fi->gototarget, 0, sizeof(fi->gototarget));
	strncpy(fi->gototarget, marker, sizeof(fi->gototarget) - 1);

	// Start skipping until the marker is found.
	fi->gotoskip = true;

	// Stop any waiting.
	fi->wait = 0;

	// Rewind the script so we can jump anywhere.
	fi->cp = fi->script;
}

//===========================================================================
// FI_SkipRequest
//	The user has requested a skip. Returns true if the skip was done.
//===========================================================================
int FI_SkipRequest(void)
{
	fi->waitingtext = NULL; // Stop waiting for things.
	fi->waitingpic = NULL;
	if(fi->paused)
	{
		// Un-pause.
		fi->paused = false;
		fi->wait = 0;
		return true;
	}		
	if(fi->canskip)
	{
		// Start skipping ahead.
		fi->skipping = true;	
		fi->wait = 0;
		return true;
	}
	return fi->eatevents;
}

//===========================================================================
// FI_IsMenuTrigger
//	Returns true if the event should open the menu.
//===========================================================================
boolean FI_IsMenuTrigger(event_t *ev)
{
	if(!fi_active) return false;
	return fi->showmenu;
}

//===========================================================================
// FI_AteEvent
//===========================================================================
int FI_AteEvent(event_t *ev)
{
	// We'll never eat key/mb/jb up events.
	if(ev->type == ev_keyup 
		|| ev->type == ev_mousebup
		|| ev->type == ev_joybup) return false;

	return fi->eatevents;
}

//===========================================================================
// FI_Responder
//===========================================================================
int FI_Responder(event_t *ev)
{
	int i;

	if(!fi_active || IS_CLIENT) return false;

	// During the first ~second disallow all events/skipping.
	if(fi->timer < 20) return FI_AteEvent(ev);

	if(ev->type == ev_keydown && ev->data1)
	{
		// Any handlers for this key event?
		for(i = 0; i < MAX_HANDLERS; i++)
		{
			if(fi->keyhandlers[i].code == ev->data1)
			{
				FI_SkipTo(fi->keyhandlers[i].marker);
				return FI_AteEvent(ev);
			}
		}
	}

	// If we can't skip, there's no interaction of any kind.
	if(!fi->canskip && !fi->paused) return FI_AteEvent(ev);

	// We are only interested in key/button presses.
	if(ev->type != ev_keydown
		&& ev->type != ev_mousebdown
		&& ev->type != ev_joybdown) return FI_AteEvent(ev);

	// We're not interested in the Escape key.
	if(ev->type == ev_keydown && ev->data1 == DDKEY_ESCAPE) 
		return FI_AteEvent(ev);

	// Servers tell clients to skip.
	NetSv_Finale(FINF_SKIP, 0, NULL, 0);
	return FI_SkipRequest();
}

//===========================================================================
// FI_FilterChar
//===========================================================================
int FI_FilterChar(int ch)
{
	// Filter it.
	ch = toupper(ch);
	if(ch == '_') ch = '[';
	else if(ch == '\\') ch = '/';
	else if(ch < 32 || ch > 'Z') ch = 32; // We don't have this char.
	return ch;
}

//===========================================================================
// FI_CharWidth
//===========================================================================
int FI_CharWidth(int ch, boolean fontb)
{
	ch = FI_FilterChar(ch);
#if __JDOOM__
	if(ch < 33) return 4;
	return fontb? hu_font_b[ch - HU_FONTSTART].width 
		: hu_font_a[ch - HU_FONTSTART].width;
#else
	if(ch < 33) return 5;
	return ((patch_t*) W_CacheLumpNum((fontb? FontBBase : FontABase) 
		+ ch - 33, PU_CACHE))->width;
#endif
}

//===========================================================================
// FI_GetLineWidth
//===========================================================================
int FI_GetLineWidth(char *text, boolean fontb)
{
	int width = 0;

	for(; *text; text++)
	{
		if(*text == '\\')
		{
			if(!*++text) break;
			if(*text == 'n') break;
			if(*text >= '0' && *text <= '9') continue;
			if(*text == 'w'	|| *text == 'W'
				|| *text == 'p' || *text == 'P') continue;
		}
		width += FI_CharWidth(*text, fontb);
	}
	return width;
}

//===========================================================================
// FI_DrawChar
//===========================================================================
int FI_DrawChar(int x, int y, int ch, boolean fontb)
{
	int lump;

	ch = FI_FilterChar(ch);
#if __JDOOM__
	if(fontb)
		lump = hu_font_b[ch - HU_FONTSTART].lump;
	else
		lump = hu_font_a[ch - HU_FONTSTART].lump;
#else
	lump = (fontb? FontBBase : FontABase) + ch - 33;
#endif
	// Draw the character. Don't try to draw spaces.
	if(ch > 32) GL_DrawPatch_CS(x, y, lump);
	return FI_CharWidth(ch, fontb);
}

//===========================================================================
// FI_UseColor
//===========================================================================
void FI_UseColor(fivalue_t *color, int components)
{
	if(components == 3)
	{
		gl.Color3f(color[0].value, color[1].value, color[2].value);
	}
	else if(components == 4)
	{
		gl.Color4f(color[0].value, color[1].value, color[2].value, 
			color[3].value);
	}
}

//===========================================================================
// FI_UseTextColor
//===========================================================================
void FI_UseTextColor(fitext_t *tex, int idx)
{
	if(!idx)
	{
		// The default color of the text.
		FI_UseColor(tex->object.color, 4);
	}
	else
	{
		gl.Color4f(fi->textcolor[idx - 1][0].value,
			fi->textcolor[idx - 1][1].value,
			fi->textcolor[idx - 1][2].value,
			tex->object.color[3].value);
	}
}

//===========================================================================
// FI_TextObjectLength
//	Returns the length as a counter.
//===========================================================================
int FI_TextObjectLength(fitext_t *tex)
{
	int cnt;
	char *ptr;
	float secondLen = (tex->wait? 35.0f/tex->wait : 0);

	for(cnt = 0, ptr = tex->text; *ptr; ptr++)
	{
		if(*ptr == '\\') // Escape?
		{
			if(!*++ptr) break;
			if(*ptr == 'w') cnt += secondLen/2; 
			if(*ptr == 'W') cnt += secondLen;
			if(*ptr == 'p') cnt += 5 * secondLen;
			if(*ptr == 'P') cnt += 10 * secondLen;
			if((*ptr >= '0' && *ptr <= '9')
				|| *ptr == 'n' || *ptr == 'N') continue;
		}
		cnt++;	// An actual character.
	}
	return cnt;
}

//===========================================================================
// FI_Rotate
//===========================================================================
void FI_Rotate(float angle)
{
	// Counter the VGA aspect ratio.
	gl.Scalef(1, 200.0f/240.0f, 1);
	gl.Rotatef(angle, 0, 0, 1);
	gl.Scalef(1, 240.0f/200.0f, 1);
}

//===========================================================================
// FI_DrawText
//===========================================================================
void FI_DrawText(fitext_t *tex)
{
	int cnt, x = 0, y = 0;
	char *ptr;
	int linew = -1;
	int ch;

	gl.MatrixMode(DGL_MODELVIEW);
	gl.PushMatrix();
	gl.Translatef(tex->object.x.value, tex->object.y.value, 0);	
	FI_Rotate(tex->object.angle.value);
	gl.Scalef(tex->object.scale[0].value, tex->object.scale[1].value, 1);

	// Set color zero (the normal color).
	FI_UseTextColor(tex, 0);

	// Draw it.
	for(cnt = 0, ptr = tex->text; *ptr && (!tex->wait || cnt < tex->pos); 
		ptr++)
	{
		if(linew < 0) linew = FI_GetLineWidth(ptr, tex->flags.font_b);
		ch = *ptr;
		if(*ptr == '\\') // Escape?
		{
			if(!*++ptr) break;
			// Change of color.
			if(*ptr >= '0' && *ptr <= '9') 
			{
				FI_UseTextColor(tex, *ptr - '0');
				continue;
			}
			// 'w' = half a second wait, 'W' = second's wait
			if(*ptr == 'w' || *ptr == 'W') // Wait?
			{
				if(tex->wait) cnt += (int) (35.0/tex->wait
					/ (*ptr == 'w'? 2 : 1));
				continue;
			}
			// 'p' = 5 second wait, 'P' = 10 second wait
			if(*ptr == 'p' || *ptr == 'P') // Longer pause?
			{
				if(tex->wait) cnt += (int) (35.0/tex->wait
					* (*ptr == 'p'? 5 : 10));
				continue;
			}
			if(*ptr == 'n' || *ptr == 'N') // Newline?
			{
				x = 0;
				y += tex->lineheight;
				linew = -1;
				cnt++; // Include newlines in the wait count.
				continue;
			}
			if(*ptr == '_') ch = ' ';
		}
		// Let's do Y-clipping (in case of tall text blocks).
		if(tex->object.scale[1].value * y + tex->object.y.value 
			>= -tex->object.scale[1].value * tex->lineheight
			&& tex->object.scale[1].value * y + tex->object.y.value < 200)
		{
			x += FI_DrawChar(tex->flags.centered? x - linew/2 : x, y, 
				ch, tex->flags.font_b);
		}
		cnt++;	// Actual character drawn.
	}
	
	gl.MatrixMode(DGL_MODELVIEW);
	gl.PopMatrix();
}

//===========================================================================
// FI_GetTurnCenter
//===========================================================================
void FI_GetTurnCenter(fipic_t *pic, float *center)
{
	if(pic->flags.is_rect)
	{
		center[VX] = center[VY] = .5f;
	}
	else if(pic->flags.is_patch) 
	{
		spriteinfo_t info;
		R_GetPatchInfo(pic->lump[pic->seq], &info);
		center[VX] = info.width/2 - info.offset;
		center[VY] = info.height/2 - info.topOffset;
	}
	else
	{
		center[VX] = 160;
		center[VY] = 100;
	}

	center[VX] *= pic->object.scale[VX].value;
	center[VY] *= pic->object.scale[VY].value;
}

//===========================================================================
// FI_Drawer
//	Drawing is the most complex task here.
//===========================================================================
void FI_Drawer(void)
{
	int i, sq;
	float mid[2];
	fipic_t *pic;
	fitext_t *tex;

	// Don't draw anything until we are sure the script has started.
	if(!fi_active || !fi_cmd_executed) return;

	// Draw the background.
	if(fi->bgflat >= 0)
	{
		FI_UseColor(fi->bgcolor, 4);
		GL_SetFlat(fi->bgflat);
		GL_DrawRectTiled(0, 0, 320, 200, 64, 64);
	}
	else
	{
		// Just clear the screen, then.
		gl.Disable(DGL_TEXTURING);
		GL_DrawRect(0, 0, 320, 200, fi->bgcolor[0].value, 
			fi->bgcolor[1].value, fi->bgcolor[2].value, 
			fi->bgcolor[3].value);
		gl.Enable(DGL_TEXTURING);
	}

	// Draw images.
	for(i = 0, pic = fi->pics; i < MAX_PICS; i++, pic++)
	{
		// Fully transparent pics will not be drawn.
		if(!pic->object.used 
			|| pic->object.color[3].value == 0) continue;

		sq = pic->seq;

		GL_SetNoTexture(); // Hmm...
		FI_UseColor(pic->object.color, 4);
		FI_GetTurnCenter(pic, mid);
		
		// Setup the transformation.
		gl.MatrixMode(DGL_MODELVIEW);
		gl.PushMatrix();
		gl.Translatef(pic->object.x.value - fi->imgoffset[0].value, 
			pic->object.y.value - fi->imgoffset[1].value, 0);
		gl.Translatef(mid[VX], mid[VY], 0);
		FI_Rotate(pic->object.angle.value);
		// Move to origin.
		gl.Translatef(-mid[VX], -mid[VY], 0);
		gl.Scalef((pic->flip[sq]? -1 : 1) * pic->object.scale[0].value, 
			pic->object.scale[1].value, 1);
		
		// Draw it.
		if(pic->flags.is_rect)
		{
			// The fill.
			gl.Disable(DGL_TEXTURING);

			gl.Begin(DGL_QUADS);
			FI_UseColor(pic->object.color, 4);
			gl.Vertex2f(0, 0);
			gl.Vertex2f(1, 0);
			FI_UseColor(pic->other_color, 4);
			gl.Vertex2f(1, 1);
			gl.Vertex2f(0, 1);
			gl.End();			
			
			gl.Begin(DGL_LINES);
			FI_UseColor(pic->edge_color, 4);
			gl.Vertex2f(0, 0);
			gl.Vertex2f(1, 0);
			gl.Vertex2f(1, 0);
			FI_UseColor(pic->other_edge_color, 4);
			gl.Vertex2f(1, 1);
			gl.Vertex2f(1, 1);
			gl.Vertex2f(0, 1);
			gl.Vertex2f(0, 1);
			FI_UseColor(pic->edge_color, 4);
			gl.Vertex2f(0, 0);
			gl.End();

			gl.Enable(DGL_TEXTURING);
		}
		else if(pic->flags.is_patch) 
		{
			GL_DrawPatch_CS(0, 0, pic->lump[sq]);
		}
		else
		{
			// FIXME: The raw screen drawer should not ignore rotation.
			// It should allow the caller to set up a transformation matrix.
			GL_DrawRawScreen_CS(pic->lump[sq], 
				pic->object.x.value - fi->imgoffset[0].value, 
				pic->object.y.value - fi->imgoffset[1].value,
				(pic->flip[sq]? -1 : 1) * pic->object.scale[0].value,
				pic->object.scale[1].value);
		}

		// Restore original transformation.
		gl.MatrixMode(DGL_MODELVIEW);
		gl.PopMatrix();
	}

	// Draw text.
	for(i = 0, tex = fi->text; i < MAX_TEXT; i++, tex++)
	{
		if(!tex->object.used || !tex->text) continue;
		FI_DrawText(tex);
	}

	// Filter on top of everything.
	if(/*fi->filter[0].value > 0 || fi->filter[1].value > 0 
		|| fi->filter[2].value > 0 || */
		fi->filter[3].value > 0)
	{
		// Only draw if necessary.
		gl.Disable(DGL_TEXTURING);
		FI_UseColor(fi->filter, 4);
		gl.Begin(DGL_QUADS);
		gl.Vertex2f(0, 0);
		gl.Vertex2f(320, 0);
		gl.Vertex2f(320, 200);
		gl.Vertex2f(0, 200);
		gl.End();
		gl.Enable(DGL_TEXTURING);
	}
}

//===========================================================================
// Command functions can only call FI_GetToken once for each operand.
// Otherwise the script cursor ends up in the wrong place.
//===========================================================================

void FIC_Do(void)
{
	// This command is called even when (cond)skipping.
	if(fi->skipnext)
	{
		// A conditional skip has been issued. 
		// We'll go into DO-skipping mode. skipnext won't be cleared
		// until the matching semicolon is found.
		fi->dolevel++;
	}
}

void FIC_End(void)
{
	fi->wait = 1;
	FI_End();
}

void FIC_BGFlat(void)
{
	fi->bgflat = W_CheckNumForName(FI_GetToken());
}

void FIC_NoBGFlat(void)
{
	fi->bgflat = -1;
}

void FIC_InTime(void)
{
	fi->intime = FI_GetTics();
}

void FIC_Tic(void)
{
	fi->wait = 1;
}

void FIC_Wait(void)
{
	fi->wait = FI_GetTics();
}

void FIC_WaitText(void)
{
	fi->waitingtext = FI_GetText(FI_GetToken());
}

void FIC_WaitAnim(void)
{
	fi->waitingpic = FI_GetPic(FI_GetToken());
}

void FIC_Color(void)
{
	int i;
	for(i = 0; i < 3; i++) FI_SetValue(fi->bgcolor + i, FI_GetFloat());
}

void FIC_ColorAlpha(void)
{
	int i;
	for(i = 0; i < 4; i++) FI_SetValue(fi->bgcolor + i, FI_GetFloat());
}

void FIC_Pause(void)
{
	fi->paused = true;
	fi->wait = 1;
}

void FIC_CanSkip(void)
{
	fi->canskip = true;
}

void FIC_NoSkip(void)
{
	fi->canskip = false;
}

void FIC_SkipHere(void)
{
	fi->skipping = false;
}

void FIC_Events(void)
{
	// Script will eat all input events.
	fi->eatevents = true;
}

void FIC_NoEvents(void)
{
	// Script will pass unprocessed events to other responders.
	fi->eatevents = false;
}

void FIC_OnKey(void)
{
	int code;
	fihandler_t *handler;

	// First argument is the key identifier.
	code = DD_GetKeyCode(FI_GetToken());
	
	// Read the marker name into fi_token.
	FI_GetToken();

	// Find an empty handler.
	if((handler = FI_GetHandler(code)) != NULL)
	{
		handler->code = code;
		strncpy(handler->marker, fi_token, sizeof(handler->marker) - 1); 
	}
}

void FIC_UnsetKey(void)
{
	fihandler_t *handler = FI_GetHandler( DD_GetKeyCode(FI_GetToken()) );

	if(handler)
	{
		handler->code = 0;
		memset(handler->marker, 0, sizeof(handler->marker));
	}
}

void FIC_If(void)
{
	boolean val = false;

	FI_GetToken();
	// Let's see if we know this id.
	if(!stricmp(fi_token, "secret"))
	{
		// Secret exit was used?
		val = fi->conditions[FICOND_SECRET];
	}
	else if(!stricmp(fi_token, "netgame"))
	{
		val = IS_NETGAME;
	}
	else if(!stricmp(fi_token, "deathmatch"))
	{
		val = deathmatch != false;
	}
	else if(!stricmp(fi_token, "shareware"))
	{
#if __JDOOM__
		val = (gamemode == shareware);
#elif __JHERETIC__
		val = shareware != false;
#else
		val = false; // Hexen has no shareware.
#endif
	}
	// Generic game mode string checking.
	else if(!strnicmp(fi_token, "mode:", 5))
	{
		val = !stricmp(fi_token + 5, G_Get(DD_GAME_MODE));
	}
#if __JDOOM__
	// Game modes.
	else if(!stricmp(fi_token, "ultimate"))
	{
		val = (gamemode == retail);
	}
	else if(!stricmp(fi_token, "commercial"))
	{
		val = (gamemode == commercial);
	}
#endif
	else if(!stricmp(fi_token, "leavehub"))
	{
		// Current hub has been completed?
		val = fi->conditions[FICOND_LEAVEHUB];
	}
#if __JHEXEN__
	// Player classes.
	else if(!stricmp(fi_token, "fighter"))
		val = (cfg.PlayerClass[consoleplayer] == PCLASS_FIGHTER);
	else if(!stricmp(fi_token, "cleric"))
		val = (cfg.PlayerClass[consoleplayer] == PCLASS_CLERIC);
	else if(!stricmp(fi_token, "mage"))
		val = (cfg.PlayerClass[consoleplayer] == PCLASS_MAGE);
#endif
	else
	{
		Con_Message("FIC_If: Unknown condition \"%s\".\n", fi_token);
	}
	// Skip the next command if the value is false.
	fi->skipnext = !val;
}

void FIC_IfNot(void)
{
	// This is the same as "if" but the skip condition is the opposite.
	FIC_If();
	fi->skipnext = !fi->skipnext;
}

void FIC_Else(void)
{
	// The only time the ELSE condition doesn't skip is immediately 
	// after a skip.
	fi->skipnext = !fi->lastskipped;
}

void FIC_GoTo(void)
{
	FI_SkipTo(FI_GetToken());
}

void FIC_Marker(void)
{
	FI_GetToken(); 
	// Does it match the goto string?
	if(!stricmp(fi->gototarget, fi_token)) fi->gotoskip = false;
}

void FIC_Delete(void)
{
	fiobj_t *obj = FI_FindObject(FI_GetToken());

	if(obj)
	{
		obj->used = false;
	}
}

void FIC_Image(void)
{
	fipic_t *pic = FI_GetPic(FI_GetToken());
	
	FI_ClearAnimation(pic);
	pic->lump[0] = W_CheckNumForName(FI_GetToken());
	pic->flags.is_patch = false;
	pic->flags.is_rect = false;
}

void FIC_ImageAt(void)
{
	fipic_t *pic = FI_GetPic(FI_GetToken());

	FI_InitValue(&pic->object.x, FI_GetFloat());
	FI_InitValue(&pic->object.y, FI_GetFloat());
	FI_ClearAnimation(pic);
	pic->lump[0] = W_CheckNumForName(FI_GetToken());
	pic->flags.is_patch = false;
	pic->flags.is_rect = false;
}

void FIC_Patch(void)
{
	fipic_t *pic = FI_GetPic(FI_GetToken());

	FI_InitValue(&pic->object.x, FI_GetFloat());
	FI_InitValue(&pic->object.y, FI_GetFloat());
	FI_ClearAnimation(pic);
	pic->lump[0] = W_CheckNumForName(FI_GetToken());
	pic->flags.is_patch = true;
	pic->flags.is_rect = false;
}

void FIC_SetPatch(void)
{
	fipic_t *pic = FI_GetPic(FI_GetToken());
	pic->lump[0] = W_CheckNumForName(FI_GetToken());
	pic->flags.is_patch = true;
	pic->flags.is_rect = false;
}

void FIC_ClearAnim(void)
{
	fipic_t *pic = FI_GetPic(FI_GetToken());
	FI_ClearAnimation(pic);
}

void FIC_Anim(void)
{
	fipic_t *pic = FI_GetPic(FI_GetToken());
	int i, lump, time;
	
	lump = W_CheckNumForName(FI_GetToken());
	time = FI_GetTics();
	// Find the next sequence spot.
	i = FI_GetNextSeq(pic);
	if(i == MAX_SEQUENCE) return; // Can't do it...
	pic->lump[i] = lump;
	pic->seq_wait[i] = time;
	pic->flags.is_patch = true;
	pic->flags.done = false;
}

void FIC_AnimImage(void)
{
	fipic_t *pic = FI_GetPic(FI_GetToken());
	int i, lump, time;
	
	lump = W_CheckNumForName(FI_GetToken());
	time = FI_GetTics();
	// Find the next sequence spot.
	i = FI_GetNextSeq(pic);
	if(i == MAX_SEQUENCE) return; // Can't do it...
	pic->lump[i] = lump;
	pic->seq_wait[i] = time;
	pic->flags.is_patch = false;
	pic->flags.is_rect = false;
	pic->flags.done = false;
}

void FIC_Repeat(void)
{
	fipic_t *pic = FI_GetPic(FI_GetToken());
	int i = FI_GetNextSeq(pic);
	if(i == MAX_SEQUENCE) return;
	pic->lump[i] = FI_REPEAT;
}

void FIC_StateAnim(void)
{
	fipic_t *pic = FI_GetPic(FI_GetToken());
	int i;
	int st = Def_Get(DD_DEF_STATE, FI_GetToken(), 0);
	int count = FI_GetInteger();
	spriteinfo_t sinf;

	// Animate N states starting from the given one.
	pic->flags.is_patch = true;
	pic->flags.is_rect = false;
	pic->flags.done = false;
	for(; count > 0 && st > 0; count--)
	{
		i = FI_GetNextSeq(pic);
		if(i == MAX_SEQUENCE) break; // No room!
		R_GetSpriteInfo(states[st].sprite, states[st].frame & 0x7fff, &sinf);
		pic->lump[i] = sinf.realLump;
		pic->flip[i] = sinf.flip;
		pic->seq_wait[i] = states[st].tics;
		if(pic->seq_wait[i] == 0) pic->seq_wait[i] = 1;
		// Go to the next state.
		st = states[st].nextstate;		
	}
}

void FIC_PicSound(void)
{
	fipic_t *pic = FI_GetPic(FI_GetToken());
	int i;

	i = FI_GetNextSeq(pic) - 1;
	if(i < 0) i = 0;
	pic->sound[i] = Def_Get(DD_DEF_SOUND, FI_GetToken(), 0);
}

void FIC_ObjectOffX(void)
{
	fiobj_t *obj = FI_FindObject(FI_GetToken());
	float value = FI_GetFloat();

	if(obj)
	{
		FI_SetValue(&obj->x, value);
	}
}

void FIC_ObjectOffY(void)
{
	fiobj_t *obj = FI_FindObject(FI_GetToken());
	float value = FI_GetFloat();

	if(obj)
	{
		FI_SetValue(&obj->y, value);
	}
}

void FIC_ObjectRGB(void)
{
	fiobj_t *obj = FI_FindObject(FI_GetToken());
	fipic_t *pic = FI_FindPic(obj? obj->handle : NULL);
	int i;
	
	for(i = 0; i < 3; i++) 
	{
		if(obj)
		{
			float value = FI_GetFloat();
			FI_SetValue(obj->color + i, value);

			if(pic && pic->flags.is_rect)
			{
				// This affects all the colors.
				FI_SetValue(pic->other_color + i, value);
				FI_SetValue(pic->edge_color + i, value);
				FI_SetValue(pic->other_edge_color + i, value);
			}
		}
		else
		{
			FI_GetFloat();
		}
	}
}

void FIC_ObjectAlpha(void)
{
	fiobj_t *obj = FI_FindObject(FI_GetToken());
	fipic_t *pic = FI_FindPic(obj? obj->handle : NULL);
	float value = FI_GetFloat();

	if(obj)
	{
		FI_SetValue(obj->color + 3, value);

		if(pic && pic->flags.is_rect)
		{
			FI_SetValue(pic->other_color + 3, value);
			/*FI_SetValue(pic->edge_color + 3, value);
			FI_SetValue(pic->other_edge_color + 3, value);*/
		}
	}
}

void FIC_ObjectScaleX(void)
{
	fiobj_t *obj = FI_FindObject(FI_GetToken());
	float value = FI_GetFloat();

	if(obj)
	{
		FI_SetValue(&obj->scale[0], value);
	}
}

void FIC_ObjectScaleY(void)
{
	fiobj_t *obj = FI_FindObject(FI_GetToken());
	float value = FI_GetFloat();

	if(obj)
	{
		FI_SetValue(&obj->scale[1], value);
	}
}

void FIC_ObjectScale(void)
{
	fiobj_t *obj = FI_FindObject(FI_GetToken());
	float value = FI_GetFloat();

	if(obj)
	{
		FI_SetValue(&obj->scale[0], value);
		FI_SetValue(&obj->scale[1], value);
	}
}

void FIC_ObjectScaleXY(void)
{
	fiobj_t *obj = FI_FindObject(FI_GetToken());
	float x = FI_GetFloat();
	float y = FI_GetFloat();

	if(obj)
	{
		FI_SetValue(&obj->scale[0], x);
		FI_SetValue(&obj->scale[1], y);
	}
}

void FIC_ObjectAngle(void)
{
	fiobj_t *obj = FI_FindObject(FI_GetToken());
	float value = FI_GetFloat();

	if(obj)
	{
		FI_SetValue(&obj->angle, value);
	}
}

void FIC_Rect(void)
{
	fipic_t *pic = FI_GetPic(FI_GetToken());
	int i;

	// Position and size.
	FI_InitValue(&pic->object.x, FI_GetFloat());
	FI_InitValue(&pic->object.y, FI_GetFloat());
	FI_InitValue(&pic->object.scale[0], FI_GetFloat());
	FI_InitValue(&pic->object.scale[1], FI_GetFloat());

	pic->flags.is_rect = true;
	pic->flags.is_patch = false;
	pic->flags.done = true;

	// Default colors.
	for(i = 0; i < 4; i++)
	{
		FI_InitValue(&pic->object.color[i], 1);
		FI_InitValue(&pic->other_color[i], 1);
		// Edge alpha is zero by default.
		FI_InitValue(&pic->edge_color[i], i < 3? 1 : 0);
		FI_InitValue(&pic->other_edge_color[i], i < 3? 1 : 0);
	}
}

void FIC_FillColor(void)
{
	fiobj_t *obj = FI_FindObject(FI_GetToken());
	fipic_t *pic;
	int which = 0;
	int i;
	float color;
	
	if(!obj)
	{
		// Skip the parms.
		for(i = 0; i < 5; i++) FI_GetToken();
		return;
	}

	pic = FI_GetPic(obj->handle);

	// Which colors to modify?
	FI_GetToken();
	if(!stricmp(fi_token, "top")) which |= 1;
	else if(!stricmp(fi_token, "bottom")) which |= 2;
	else which = 3;

	for(i = 0; i < 4; i++) 
	{
		color = FI_GetFloat();

		if(which & 1) FI_SetValue(obj->color + i, color);
		if(which & 2) FI_SetValue(pic->other_color + i, color);
	}
}

void FIC_EdgeColor(void)
{
	fiobj_t *obj = FI_FindObject(FI_GetToken());
	fipic_t *pic;
	int which = 0;
	int i;
	float color;
	
	if(!obj)
	{
		// Skip the parms.
		for(i = 0; i < 5; i++) FI_GetToken();
		return;
	}

	pic = FI_GetPic(obj->handle);

	// Which colors to modify?
	FI_GetToken();
	if(!stricmp(fi_token, "top")) which |= 1;
	else if(!stricmp(fi_token, "bottom")) which |= 2;
	else which = 3;

	for(i = 0; i < 4; i++) 
	{
		color = FI_GetFloat();

		if(which & 1) FI_SetValue(pic->edge_color + i, color);
		if(which & 2) FI_SetValue(pic->other_edge_color + i, color);
	}
}

void FIC_OffsetX(void)
{
	FI_SetValue(fi->imgoffset, FI_GetFloat());
}

void FIC_OffsetY(void)
{
	FI_SetValue(fi->imgoffset + 1, FI_GetFloat());
}

void FIC_Sound(void)
{
	int num = Def_Get(DD_DEF_SOUND, FI_GetToken(), NULL);
	if(num > 0) S_LocalSound(num, NULL);
}

void FIC_SoundAt(void)
{
	int num = Def_Get(DD_DEF_SOUND, FI_GetToken(), NULL);
	float vol = FI_GetFloat();

	if(vol > 1) vol = 1;
	if(vol > 0 && num > 0) S_LocalSoundAtVolume(num, NULL, vol);
}

void FIC_SeeSound(void)
{
	int num = Def_Get(DD_DEF_MOBJ, FI_GetToken(), NULL);
	if(num < 0 || mobjinfo[num].seesound <= 0) return;
	S_LocalSound(mobjinfo[num].seesound, NULL);	
}

void FIC_DieSound(void)
{
	int num = Def_Get(DD_DEF_MOBJ, FI_GetToken(), NULL);
	if(num < 0 || mobjinfo[num].deathsound <= 0) return;
	S_LocalSound(mobjinfo[num].deathsound, NULL);	
}

void FIC_Music(void)
{
	S_StartMusic(FI_GetToken(), true);
}

void FIC_MusicOnce(void)
{
	S_StartMusic(FI_GetToken(), false);
}

void FIC_Filter(void)
{
	int i;
	for(i = 0; i < 4; i++) FI_SetValue(fi->filter + i, FI_GetFloat());
}

void FIC_Text(void)
{
	fitext_t *tex = FI_GetText(FI_GetToken());

	FI_InitValue(&tex->object.x, FI_GetFloat());
	FI_InitValue(&tex->object.y, FI_GetFloat());
	FI_SetText(tex, FI_GetToken());
	tex->pos = 0; // Restart the text.
}

void FIC_TextFromDef(void)
{
	fitext_t *tex = FI_GetText(FI_GetToken());
	char *str;

	FI_InitValue(&tex->object.x, FI_GetFloat());
	FI_InitValue(&tex->object.y, FI_GetFloat());
	if(!Def_Get(DD_DEF_TEXT, FI_GetToken(), &str))
		str = "(undefined)"; // Not found!
	FI_SetText(tex, str);
	tex->pos = 0; // Restart the text.
}

void FIC_TextFromLump(void)
{
	fitext_t *tex = FI_GetText(FI_GetToken());
	int lnum, buflen, i, incount;
	char *data, *str, *out;
	
	FI_InitValue(&tex->object.x, FI_GetFloat());
	FI_InitValue(&tex->object.y, FI_GetFloat());
	lnum = W_CheckNumForName(FI_GetToken());
	if(lnum < 0)
		FI_SetText(tex, "(not found)");
	else
	{
		// Load the lump.
		data = W_CacheLumpNum(lnum, PU_STATIC);
		incount = W_LumpLength(lnum);
		str = Z_Malloc(buflen = 2*incount + 1, PU_STATIC, 0);
		memset(str, 0, buflen);
		for(i = 0, out = str; i < incount; i++)
		{
			if(data[i] == '\n')
			{
				*out++ = '\\';
				*out++ = 'n';
			}
			else
			{
				*out++ = data[i];
			}
		}
		W_ChangeCacheTag(lnum, PU_CACHE);
		FI_SetText(tex, str);
		Z_Free(str);
	}
	tex->pos = 0; // Restart.
}

void FIC_SetText(void)
{
	fitext_t *tex = FI_GetText(FI_GetToken());
	FI_SetText(tex, FI_GetToken());
}

void FIC_SetTextDef(void)
{
	fitext_t *tex = FI_GetText(FI_GetToken());
	char *str;

	if(!Def_Get(DD_DEF_TEXT, FI_GetToken(), &str))
		str = "(undefined)"; // Not found!
	FI_SetText(tex, str);
}

void FIC_DeleteText(void)
{
	fitext_t *tex = FI_GetText(FI_GetToken());
	tex->object.used = false;
	if(tex->text)
	{
		// Free the memory allocated for the text string.
		Z_Free(tex->text);
		tex->text = NULL;
	}
}

void FIC_TextColor(void)
{
	int idx = FI_GetInteger(), c;

	if(idx < 1) idx = 1;
	if(idx > 9) idx = 9;
	for(c = 0; c < 3; c++) 
		FI_SetValue(&fi->textcolor[idx - 1][c], FI_GetFloat());
}

void FIC_TextRGB(void)
{
	int i;
	fitext_t *tex = FI_GetText(FI_GetToken());
	for(i = 0; i < 3; i++) 
		FI_SetValue(&tex->object.color[i], FI_GetFloat());
}

void FIC_TextAlpha(void)
{
	fitext_t *tex = FI_GetText(FI_GetToken());
	FI_SetValue(&tex->object.color[3], FI_GetFloat());
}

void FIC_TextOffX(void)
{
	fitext_t *tex = FI_GetText(FI_GetToken());
	FI_SetValue(&tex->object.x, FI_GetFloat());
}

void FIC_TextOffY(void)
{
	fitext_t *tex = FI_GetText(FI_GetToken());
	FI_SetValue(&tex->object.y, FI_GetFloat());
}

void FIC_TextCenter(void)
{
	fitext_t *tex = FI_GetText(FI_GetToken());
	tex->flags.centered = true;
}

void FIC_TextNoCenter(void)
{
	fitext_t *tex = FI_GetText(FI_GetToken());
	tex->flags.centered = false;
}

void FIC_TextScroll(void)
{
	fitext_t *tex = FI_GetText(FI_GetToken());
	tex->scroll_timer = 0;
	tex->scroll_wait = FI_GetInteger();
}

void FIC_TextPos(void)
{
	fitext_t *tex = FI_GetText(FI_GetToken());
	tex->pos = FI_GetInteger();
}

void FIC_TextRate(void)
{
	fitext_t *tex = FI_GetText(FI_GetToken());
	tex->wait = FI_GetInteger();
}

void FIC_TextLineHeight(void)
{
	fitext_t *tex = FI_GetText(FI_GetToken());
	tex->lineheight = FI_GetInteger();
}

void FIC_FontA(void)
{
	fitext_t *tex = FI_GetText(FI_GetToken());	
	tex->flags.font_b = false;
	// Set line height to font A.
#if __JDOOM__
	tex->lineheight = 11;
#else
	tex->lineheight = 9;
#endif
}

void FIC_FontB(void)
{
	fitext_t *tex = FI_GetText(FI_GetToken());
	tex->flags.font_b = true;
#if __JDOOM__
	tex->lineheight = 15;
#else
	tex->lineheight = 20;
#endif
}

void FIC_NoMusic(void)
{
	// Stop the currently playing song.
	S_StopMusic();
}

void FIC_TextScaleX(void)
{
	fitext_t *tex = FI_GetText(FI_GetToken());
	FI_SetValue(&tex->object.scale[0], FI_GetFloat());
}

void FIC_TextScaleY(void)
{
	fitext_t *tex = FI_GetText(FI_GetToken());
	FI_SetValue(&tex->object.scale[1], FI_GetFloat());
}

void FIC_TextScale(void)
{
	fitext_t *tex = FI_GetText(FI_GetToken());
	FI_SetValue(&tex->object.scale[0], FI_GetFloat());
	FI_SetValue(&tex->object.scale[1], FI_GetFloat());
}

void FIC_PlayDemo(void)
{
	// Mark the current state as suspended, so we know to resume it when
	// the demo ends.
	fi->suspended = true;
	fi_active = false;

	// The only argument is the demo file name.
	// Start playing the demo.
	if(!Con_Executef(true, "playdemo \"%s\"", FI_GetToken()))
	{
		// Demo playback failed. Here we go again...
		FI_DemoEnds();
	}
}

void FIC_Command(void)
{
	Con_Executef(false, FI_GetToken());
}

void FIC_ShowMenu(void)
{
	fi->showmenu = true;
}

void FIC_NoShowMenu(void)
{
	fi->showmenu = false;
}

