
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
#include "d_netjd.h"
#include "m_swap.h"
#include "v_video.h"
#include "s_sound.h"
#include "hu_stuff.h"

#elif __JHERETIC__
#include "doomdef.h"
#include "am_map.h"
#include "s_sound.h"
#include "soundst.h"

#elif __JHEXEN__
#include "h2def.h"
#include "am_map.h"
#include "settings.h"
#endif

#include "f_infine.h"

// MACROS ------------------------------------------------------------------

#define MAX_TOKEN_LEN	8192
#define MAX_SEQUENCE	64
#define MAX_PICS		64
#define MAX_TEXT		8

#define FI_REPEAT		-2

// TYPES -------------------------------------------------------------------

typedef struct
{
	char *token;
	int operands;
	void (*func)(void);
	boolean when_skipping;
} ficmd_t;

typedef struct
{
	float value;
	float target;
	int steps;
} fivalue_t;

typedef char handle_t[32];

typedef struct
{
	boolean used;
	handle_t handle;
	fivalue_t color[4];
	fivalue_t scale[2];
	fivalue_t x, y;
	struct {
		char is_patch : 1;	// Raw image or patch.
		char done : 1;		// Animation finished (or repeated).
	} flags;
	int seq;
	int seq_wait[MAX_SEQUENCE], seq_timer;
	short lump[MAX_SEQUENCE];
	char flip[MAX_SEQUENCE];
	short sound[MAX_SEQUENCE];
} fipic_t;

typedef struct
{
	boolean used;
	handle_t handle;
	fivalue_t color[4];
	fivalue_t scale[2];
	fivalue_t x, y;
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

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void FI_InitValue(fivalue_t *val, float num);
int FI_TextObjectLength(fitext_t *tex);

// Command functions.
void FIC_End(void);
void FIC_BGFlat(void);
void FIC_NoBGFlat(void);
void FIC_Wait(void);
void FIC_WaitText(void);
void FIC_WaitAnim(void);
void FIC_Tic(void);
void FIC_InTime(void);
void FIC_Color(void);
void FIC_Pause(void);
void FIC_CanSkip(void);
void FIC_NoSkip(void);
void FIC_SkipHere(void);
void FIC_If(void);
void FIC_IfNot(void);
void FIC_SkipTo(void);
void FIC_Marker(void);
void FIC_Image(void);
void FIC_ImageAt(void);
void FIC_DeletePic(void);
void FIC_Patch(void);
void FIC_SetPatch(void);
void FIC_Anim(void);
void FIC_AnimImage(void);
void FIC_StateAnim(void);
void FIC_Repeat(void);
void FIC_ClearAnim(void);
void FIC_PicSound(void);
void FIC_PicOffX(void);
void FIC_PicOffY(void);
void FIC_PicRGB(void);
void FIC_PicAlpha(void);
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
void FIC_PicScaleX(void);
void FIC_PicScaleY(void);
void FIC_PicScale(void);
void FIC_TextScaleX(void);
void FIC_TextScaleY(void);
void FIC_TextScale(void);

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
	{ "end", 0,			FIC_End },		
	{ "if", 1,			FIC_If },		// if (value-id)
	{ "ifnot", 1,		FIC_IfNot },	// ifnot (value-id)
	{ "goto", 1,		FIC_SkipTo },	// goto (marker)
	{ "marker", 1,		FIC_Marker, true },
	{ "in", 1,			FIC_InTime },	// in (time)
	{ "pause", 0,		FIC_Pause },	
	{ "tic", 0,			FIC_Tic },		
	{ "wait", 1,		FIC_Wait },		// wait (time)
	{ "waittext", 1,	FIC_WaitText },	// waittext (handle)
	{ "waitanim", 1,	FIC_WaitAnim }, // waitanim (handle)
	{ "canskip", 0,		FIC_CanSkip },
	{ "noskip", 0,		FIC_NoSkip },
	{ "skiphere", 0,	FIC_SkipHere, true },	
	
	// Screen Control
	{ "color", 3,		FIC_Color },	// color (red) (green) (blue)
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

	// Pics
	{ "delpic", 1,		FIC_DeletePic }, // delpic (handle)
	{ "image", 2,		FIC_Image },	// image (handle) (raw-image-lump)
	{ "imageat", 4,		FIC_ImageAt },	// imageat (handle) (x) (y) (raw)
	{ "patch", 4,		FIC_Patch },	// patch (handle) (x) (y) (patch)
	{ "set", 2,			FIC_SetPatch },	// set (handle) (lump)
	{ "x", 2,			FIC_PicOffX },	// x (handle) (x)
	{ "y", 2,			FIC_PicOffY },	// y (handle) (y)
	{ "sx", 2,			FIC_PicScaleX }, // sx (handle) (x)
	{ "sy", 2,			FIC_PicScaleY }, // sy (handle) (y)
	{ "scale", 3,		FIC_PicScale }, // scale (handle) (x) (y)
	{ "rgb", 4,			FIC_PicRGB },	// rgb (handle) (r) (g) (b)
	{ "alpha", 2,		FIC_PicAlpha },	// alpha (handle) (alpha)
	{ "clranim", 1,		FIC_ClearAnim }, // clranim (handle)
	{ "anim", 3,		FIC_Anim },		// anim (handle) (patch) (time)
	{ "imageanim", 3,	FIC_AnimImage }, // imageanim (hndl) (raw-img) (time)
	{ "picsound", 2,	FIC_PicSound },	// picsound (hndl) (sound)
	{ "repeat", 1,		FIC_Repeat },	// repeat (handle)
	{ "states", 3,		FIC_StateAnim }, // states (handle) (state) (count)

	// Text	
	{ "deltext", 1,		FIC_DeleteText }, // deltext (hndl)
	{ "text", 4,		FIC_Text },		// text (hndl) (x) (y) (string)
	{ "textdef", 4,		FIC_TextFromDef }, // textdef (hndl) (x) (y) (txt-id)
	{ "textlump", 4,	FIC_TextFromLump }, // textlump (hndl) (x) (y) (lump)
	{ "settext", 2,		FIC_SetText },	// settext (handle) (newtext)
	{ "settextdef", 2,	FIC_SetTextDef }, // settextdef (handle) (txt-id)
	{ "precolor", 4,	FIC_TextColor }, // precolor (num) (r) (g) (b)
	{ "textrgb", 4,		FIC_TextRGB },	// textrgb (handle) (r) (g) (b)
	{ "textalpha", 2,	FIC_TextAlpha }, // textalpha (handle) (alpha)
	{ "tx", 2,			FIC_TextOffX },	// tx (handle) (x)
	{ "ty", 2,			FIC_TextOffY },	// ty (handle) (y)
	{ "tsx", 2,			FIC_TextScaleX }, // tsx (handle) (x)
	{ "tsy", 2,			FIC_TextScaleY }, // tsy (handle) (y)
	{ "textscale", 3,	FIC_TextScale }, // textscale (handle) (x) (y)
	{ "center", 1,		FIC_TextCenter }, // center (handle)
	{ "nocenter", 1,	FIC_TextNoCenter }, // nocenter (handle)
	{ "scroll", 2,		FIC_TextScroll }, // scroll (handle) (speed)
	{ "pos", 2,			FIC_TextPos },	// pos (handle) (pos)
	{ "rate", 2,		FIC_TextRate }, // rate (handle) (rate)
	{ "fonta", 1,		FIC_FontA },	// fonta (handle)
	{ "fontb", 1,		FIC_FontB },	// fontb (handle)
	{ "linehgt", 2,		FIC_TextLineHeight }, // linehgt (hndl) (hgt)
	{ NULL, 0, NULL }
};

static char *script, *cp;
static boolean is_after;
static int fi_timer;

static boolean fi_canskip, fi_skipping;
static int fi_wait;
static boolean fi_paused, fi_gotoskip, fi_skipnext;
static char fi_token[MAX_TOKEN_LEN];
static handle_t fi_goto;
static fitext_t *fi_waitingtext;
static fipic_t *fi_waitingpic;
static boolean fi_conditions[NUM_FICONDS];

// The InFine state:
static int fi_intime;
static int fi_bgflat;		// Background flat.
static fivalue_t fi_bgcolor[3];
static fivalue_t fi_imgcolor[4];
static fivalue_t fi_imgoffset[2];
static fivalue_t fi_filter[4];
static fivalue_t fi_textcolor[9][3];
static fipic_t fi_pics[MAX_PICS], fi_dummypic;
static fitext_t fi_text[MAX_TEXT], fi_dummytext;

#if !__JDOOM__
static int FontABase, FontBBase;
#endif

// CODE --------------------------------------------------------------------

//===========================================================================
// FI_StartScript
//	Start playing the given script.
//===========================================================================
void FI_StartScript(char *finalescript, boolean after)
{
	int i, c;

#ifdef _DEBUG
	Con_Printf("FI_StartScript: aft=%i '%.30s'\n", after, finalescript);
#endif

	if(!IS_CLIENT)
	{
		// We are able to figure out the truth values of all the 
		// conditions.
		fi_conditions[FICOND_SECRET] = (secretexit != 0);

#ifdef __JHEXEN__
		// Current hub has been completed?
		fi_conditions[FICOND_LEAVEHUB] 
			= (P_GetMapCluster(gamemap) != P_GetMapCluster(LeaveMap));
#else
		fi_conditions[FICOND_LEAVEHUB] = false;
#endif
	}

	// Tell clients to start this script.
	NetSv_Finale(FINF_BEGIN | (after? FINF_AFTER : 0), finalescript,
		fi_conditions, NUM_FICONDS);

	// Init InFine state.
#if !__JDOOM__
	players[consoleplayer].messageTics = 1;
#endif
#if __JHEXEN__
	strcpy(players[consoleplayer].message, "");
#else
	players[consoleplayer].message = NULL;
#endif
    gameaction = ga_nothing;
    gamestate = GS_INFINE;
    automapactive = false;
	fi_active = true;
	// Nothing is drawn until a cmd has been executed.
	fi_cmd_executed = false;	
	is_after = after;
	cp = script = finalescript;	
	fi_timer = 0;
	fi_canskip = true;		// By default skipping is enabled.
	fi_skipping = false;
	fi_wait = 0;			// Not waiting for anything.
	fi_intime = 0;			// Interpolation is off.
	fi_bgflat = -1;			// No background flat.
	fi_paused = false;
	fi_gotoskip = false;
	fi_skipnext = false;
	fi_waitingtext = NULL;
	fi_waitingpic = NULL;
	memset(fi_goto, 0, sizeof(fi_goto));
	GL_SetFilter(0);		// Clear the current filter.
	for(c = 0; c < 3; c++) FI_InitValue(fi_bgcolor + c, 1);
	memset(fi_pics, 0, sizeof(fi_pics));
	memset(fi_imgoffset, 0, sizeof(fi_imgoffset));
	memset(fi_text, 0, sizeof(fi_text));
	memset(&fi_dummytext, 0, sizeof(fi_dummytext));
	memset(fi_filter, 0, sizeof(fi_filter));
	for(i = 0; i < 9; i++)
		for(c = 0; c < 3; c++)
			FI_InitValue(&fi_textcolor[i][c], 1);
#ifndef __JDOOM__
	FontABase = W_GetNumForName("FONTA_S") + 1;
	FontBBase = W_GetNumForName("FONTB_S") + 1;
#endif
}

//===========================================================================
// FI_End
//	Stop playing the script and go to next game state.
//===========================================================================
void FI_End(void)
{
	if(!fi_canskip || !fi_active) return;

	fi_active = false;

#ifdef _DEBUG
	Con_Printf("FI_End\n");
#endif

	// Tell clients to stop the finale.
	NetSv_Finale(FINF_END, 0, NULL, 0);
	if(is_after) // A level has been completed.
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
	else // Enter the level, this was a briefing.
	{
		gamestate = GS_LEVEL;
		levelstarttic = gametic;
		leveltime = actual_leveltime = 0;
		// Restart the current map's song.
		S_LevelMusic();
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
	fi_conditions[index] = value;
#ifdef _DEBUG
	Con_Printf("FI_SetCondition: %i = %s\n", index, value? "true" : "false");
#endif
}

//===========================================================================
// CCmdStopInFine
//===========================================================================
int CCmdStopInFine(int argc, char **argv)
{
	if(gamestate != GS_INFINE) return false;
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
// FI_GetFinaleGame
//	Usually the Game number is zero, but Plutonia and TNT are a bit of a
//	problem since they must be able to coexist alongside regular Doom II.
//===========================================================================
int FI_GetFinaleGame(void)
{
/*#if __JDOOM__
	if(gamemode == commercial)
	{
		if(gamemission == pack_plut) return 1;
		if(gamemission == pack_tnt) return 2;
	}
#endif*/
	return 0;
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

	fin.game = FI_GetFinaleGame();
	if(!Def_Get(DD_DEF_FINALE_BEFORE, mid, &fin)) return false;

	FI_StartScript(fin.script, false);
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
	fin.game = FI_GetFinaleGame();
	if(!Def_Get(DD_DEF_FINALE_AFTER, mid, &fin)) return false;

	FI_StartScript(fin.script, true);
	return true;
}

//===========================================================================
// FI_GetToken
//===========================================================================
char *FI_GetToken(void)
{
	char *out;

	// Skip whitespace.
	while(*cp && isspace(*cp)) cp++;
	if(!*cp) return NULL;	// The end has been reached.
	out = fi_token;
	if(*cp == '"') // A string?
	{
		for(cp++; *cp; cp++)
		{
			if(*cp == '"')
			{
				cp++;
				// Convert double quotes to single ones.
				if(*cp != '"') break;
			}
			*out++ = *cp;
		}
	}
	else
	{
		while(!isspace(*cp) && *cp) *out++ = *cp++;
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
//===========================================================================
void FI_Execute(char *cmd)
{
	int	i, k;
	char *oldcp;

	if(!strcmp(cmd, ";")) return; // Allowed garbage.

	// We're now going to execute a command.
	fi_cmd_executed = true;

	// Is this a command we know how to execute?
	for(i = 0; fi_commands[i].token; i++)
		if(!stricmp(cmd, fi_commands[i].token))
		{
			// Check that there are enough operands.
			oldcp = cp;
			for(k = fi_commands[i].operands; k > 0; k--)
				if(!FI_GetToken()) 
				{
					cp = oldcp;
					Con_Message("FI_Execute: cmd \"%s\" has too few operands.\n", 
						fi_commands[i].token);
					break;
				}
			// Should we skip this command?
			if(fi_skipnext 
				|| (fi_skipping || fi_gotoskip) 
					&& !fi_commands[i].when_skipping) 
			{
				fi_skipnext = false;
				return;
			}
			// If there were enough operands, execute the command.
			cp = oldcp;
			if(!k) fi_commands[i].func();
			return;
		}
	// The command was not found!
	Con_Message("FI_Execute: unknown cmd \"%s\".\n", cmd);
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
	val->steps = fi_intime;
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
// FI_GetPic
//===========================================================================
fipic_t *FI_GetPic(char *handle)
{
	int i;
	fipic_t *unused = NULL;

	for(i = 0; i < MAX_PICS; i++)
	{
		if(!fi_pics[i].used)
		{
			if(!unused) unused = fi_pics + i;
			continue;
		}
		if(!stricmp(fi_pics[i].handle, handle))
			return fi_pics + i;
	}
	
	// Allocate an empty one.
	if(!unused) 
	{
		Con_Message("FI_GetPic: No room for \"%s\".", handle);
		return &fi_dummypic; // No more space.
	}
	memset(unused, 0, sizeof(*unused));
	strncpy(unused->handle, handle, sizeof(unused->handle) - 1);
	unused->used = true;
	for(i = 0; i < 4; i++) FI_InitValue(&unused->color[i], 1);
	for(i = 0; i < 2; i++) FI_InitValue(&unused->scale[i], 1);
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
		if(!fi_text[i].used)
		{
			if(!unused) unused = fi_text + i;
			continue;
		}
		if(!stricmp(fi_text[i].handle, handle))
			return fi_text + i;
	}
	
	// Allocate an empty one.
	if(!unused) 
	{
		Con_Message("FI_GetText: No room for \"%s\".", handle);
		return &fi_dummytext; // No more space.
	}
	if(unused->text) Z_Free(unused->text);
	memset(unused, 0, sizeof(*unused));
	strncpy(unused->handle, handle, sizeof(unused->handle) - 1);
	unused->used = true;
	unused->wait = 3;
#if __JDOOM__
	unused->lineheight = 11;
	FI_InitValue(&unused->color[0], 1); // Red text by default.
#else
	unused->lineheight = 9;
	// White text.
	for(i = 0; i < 3; i++) FI_InitValue(&unused->color[i], 1); 
#endif
	FI_InitValue(&unused->color[3], 1); // Opaque.
	for(i = 0; i < 2; i++) FI_InitValue(&unused->scale[i], 1);
	return unused;
}

//===========================================================================
// FI_SetText
//===========================================================================
void FI_SetText(fitext_t *tex, char *str)
{
	int len = strlen(str) + 1;

	if(tex->text) Z_Free(tex->text); // Free any previous text.
	tex->text = Z_Malloc(len, PU_LEVEL, 0);
	memcpy(tex->text, str, len);
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

	fi_timer++;
	
	// Interpolateable values.
	FI_ValueArrayThink(fi_bgcolor, 3);
	FI_ValueArrayThink(fi_imgoffset, 2);
	FI_ValueArrayThink(fi_filter, 4);
	for(i = 0; i < 9; i++) FI_ValueArrayThink(fi_textcolor[i], 3);
	for(i = 0, pic = fi_pics; i < MAX_PICS; i++, pic++)
	{
		if(!pic->used) continue;
		FI_ValueThink(&pic->x);
		FI_ValueThink(&pic->y);
		FI_ValueArrayThink(pic->scale, 2);
		FI_ValueArrayThink(pic->color, 4);
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
	for(i = 0, tex = fi_text; i < MAX_TEXT; i++, tex++)
	{
		if(!tex->used) continue;
		FI_ValueThink(&tex->x);
		FI_ValueThink(&tex->y);
		FI_ValueArrayThink(tex->scale, 2);
		FI_ValueArrayThink(tex->color, 4);
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
				tex->y.target -= 1;
				tex->y.steps = tex->scroll_wait;
			}
		}
		// Is the text object fully visible?
		tex->flags.all_visible 
			= (!tex->wait || tex->pos >= FI_TextObjectLength(tex));
	}

	// If we're waiting, don't execute any commands.
	if(fi_wait && --fi_wait) return;

	// If we're paused we can't really do anything.
	if(fi_paused) return;

	// If we're waiting for a text to finish typing, do nothing.
	if(fi_waitingtext)
	{
		if(!fi_waitingtext->flags.all_visible) return;
		fi_waitingtext = NULL;
	}

	// Waiting for an animation to reach its end?
	if(fi_waitingpic)
	{
		if(!fi_waitingpic->flags.done) return;
		fi_waitingpic = NULL;
	}

	// Execute commands until a wait time is set or we reach the end of
	// the script. If the end is reached, the finale really ends (FI_End).
	while(!fi_wait 
		&& !fi_waitingtext
		&& !fi_waitingpic
		&& !(last = !FI_ExecuteNextCommand()));
	
	// The script has ended!
	if(last) FI_End();
}

//===========================================================================
// FI_SkipRequest
//	The user has requested a skip. Returns true if the skip was done.
//===========================================================================
int FI_SkipRequest(void)
{
	fi_waitingtext = NULL; // Stop waiting for things.
	fi_waitingpic = NULL;
	if(fi_paused)
	{
		// Un-pause.
		fi_paused = false;
		fi_wait = 0;
		return true;
	}		
	if(fi_canskip)
	{
		// Start skipping ahead.
		fi_skipping = true;	
		fi_wait = 0;
		return true;
	}
	return false;
}

//===========================================================================
// FI_Responder
//===========================================================================
int FI_Responder(event_t *ev)
{
	// If we can't skip, there's no interaction of any kind.
	// Also, during the first second disallow skipping.
	if(!fi_canskip && !fi_paused
		|| IS_CLIENT	
		|| fi_timer < 35) return false;

	if(ev->type != ev_keydown
		&& ev->type != ev_mousebdown
		&& ev->type != ev_joybdown) return false;

	// We're not interested in the Escape key.
	if(ev->type == ev_keydown && ev->data1 == DDKEY_ESCAPE) 
		return false;
	
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
// FI_UseTextColor
//===========================================================================
void FI_UseTextColor(fitext_t *tex, int idx)
{
	if(!idx)
	{
		// The default color of the text.
		gl.Color4f(tex->color[0].value, tex->color[1].value,
			tex->color[2].value, tex->color[3].value);
	}
	else
	{
		gl.Color4f(fi_textcolor[idx - 1][0].value,
			fi_textcolor[idx - 1][1].value,
			fi_textcolor[idx - 1][2].value,
			tex->color[3].value);
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

	for(cnt = 0, ptr = tex->text; *ptr; ptr++)
	{
		if(*ptr == '\\') // Escape?
		{
			if(!*++ptr) break;
			if((*ptr >= '0' && *ptr <= '9')
				|| *ptr == 'w' || *ptr == 'W'
				|| *ptr == 'p' || *ptr == 'P'
				|| *ptr == 'n' || *ptr == 'N') continue;
		}
		cnt++;	// An actual character.
	}
	return cnt;
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
	gl.Translatef(tex->x.value, tex->y.value, 0);	
	gl.Scalef(tex->scale[0].value, tex->scale[1].value, 1);

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
		if(tex->scale[1].value * y + tex->y.value >= -tex->scale[1].value * tex->lineheight
			&& tex->scale[1].value * y + tex->y.value < 200)
		{
			x += FI_DrawChar(tex->flags.centered? x - linew/2 : x, y, ch,
				tex->flags.font_b);
		}
		cnt++;	// Actual character drawn.
	}
	
	gl.MatrixMode(DGL_MODELVIEW);
	gl.PopMatrix();
}

//===========================================================================
// FI_Drawer
//	Drawing is the most complex task here.
//===========================================================================
void FI_Drawer(void)
{
	int i, sq;
	fipic_t *pic;
	fitext_t *tex;

	// Don't draw anything until we are sure the script has started.
	if(!fi_active || !fi_cmd_executed) return;

	// Draw the background.
	if(fi_bgflat >= 0)
	{
		gl.Color3f(fi_bgcolor[0].value, fi_bgcolor[1].value, 
			fi_bgcolor[2].value);
		GL_SetFlat(fi_bgflat);
		GL_DrawRectTiled(0, 0, 320, 200, 64, 64);
	}
	else
	{
		// Just clear the screen, then.
		gl.Disable(DGL_TEXTURING);
		GL_DrawRect(0, 0, 320, 200, fi_bgcolor[0].value, 
			fi_bgcolor[1].value, fi_bgcolor[2].value, 1);
		gl.Enable(DGL_TEXTURING);
	}

	// Draw images.
	for(i = 0, pic = fi_pics; i < MAX_PICS; i++, pic++)
	{
		if(!pic->used) continue;
		sq = pic->seq;

		GL_SetNoTexture(); // Hmm...
		gl.Color4f(pic->color[0].value, pic->color[1].value, 
			pic->color[2].value, pic->color[3].value);
		
		// Draw it.
		if(pic->flags.is_patch) 
		{
			// Setup the transformation matrix we need.
			gl.MatrixMode(DGL_MODELVIEW);
			gl.PushMatrix();
			gl.Translatef(pic->x.value - fi_imgoffset[0].value, 
				pic->y.value - fi_imgoffset[1].value, 0);
			gl.Scalef((pic->flip[sq]? -1 : 1) * pic->scale[0].value, 
				pic->scale[1].value, 1);

			GL_DrawPatch_CS(0, 0, pic->lump[sq]);

			// Restore original transformation.
			gl.MatrixMode(DGL_MODELVIEW);
			gl.PopMatrix();
		}
		else
		{
			GL_DrawRawScreen_CS(pic->lump[sq], 
				pic->x.value - fi_imgoffset[0].value, 
				pic->y.value - fi_imgoffset[1].value,
				(pic->flip[sq]? -1 : 1) * pic->scale[0].value,
				pic->scale[1].value);
		}
	}

	// Draw text.
	for(i = 0, tex = fi_text; i < MAX_TEXT; i++, tex++)
	{
		if(!tex->used || !tex->text) continue;
		FI_DrawText(tex);
	}

	// Filter on top of everything.
	if(fi_filter[0].value > 0 || fi_filter[1].value > 0 
		|| fi_filter[2].value > 0 || fi_filter[3].value > 0)
	{
		// Only draw if necessary.
		gl.Disable(DGL_TEXTURING);
		gl.Color4f(fi_filter[0].value, fi_filter[1].value,
			fi_filter[2].value, fi_filter[3].value);
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

void FIC_End(void)
{
	fi_wait = 1;
	FI_End();
}

void FIC_BGFlat(void)
{
	fi_bgflat = W_CheckNumForName(FI_GetToken());
}

void FIC_NoBGFlat(void)
{
	fi_bgflat = -1;
}

void FIC_InTime(void)
{
	fi_intime = FI_GetTics();
}

void FIC_Tic(void)
{
	fi_wait = 1;
}

void FIC_Wait(void)
{
	fi_wait = FI_GetTics();
}

void FIC_WaitText(void)
{
	fi_waitingtext = FI_GetText(FI_GetToken());
}

void FIC_WaitAnim(void)
{
	fi_waitingpic = FI_GetPic(FI_GetToken());
}

void FIC_Color(void)
{
	int i;
	for(i = 0; i < 3; i++) FI_SetValue(fi_bgcolor + i, FI_GetFloat());
}

void FIC_Pause(void)
{
	fi_paused = true;
	fi_wait = 1;
}

void FIC_CanSkip(void)
{
	fi_canskip = true;
}

void FIC_NoSkip(void)
{
	fi_canskip = false;
}

void FIC_SkipHere(void)
{
	fi_skipping = false;
}

void FIC_If(void)
{
	boolean val = false;

	FI_GetToken();
	// Let's see if we know this id.
	if(!stricmp(fi_token, "secret"))
	{
		// Secret exit was used?
		val = fi_conditions[FICOND_SECRET];
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
		val = fi_conditions[FICOND_LEAVEHUB];
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
	// Skip the next command if the value is false.
	fi_skipnext = !val;
}

void FIC_IfNot(void)
{
	// This is the same as "if" but the skip condition is the opposite.
	FIC_If();
	fi_skipnext = !fi_skipnext;
}

void FIC_SkipTo(void)
{
	memset(fi_goto, 0, sizeof(fi_goto));
	strncpy(fi_goto, FI_GetToken(), sizeof(fi_goto) - 1);
	// Start skipping until the marker is found.
	fi_gotoskip = true;
	// Rewind the script so we can jump backwards.
	cp = script;
}

void FIC_Marker(void)
{
	FI_GetToken(); 
	// Does it match the goto string?
	if(!stricmp(fi_goto, fi_token)) fi_gotoskip = false;
}

void FIC_DeletePic(void)
{
	fipic_t *pic = FI_GetPic(FI_GetToken());
	pic->used = false;
}

void FIC_Image(void)
{
	fipic_t *pic = FI_GetPic(FI_GetToken());
	
	FI_ClearAnimation(pic);
	pic->lump[0] = W_CheckNumForName(FI_GetToken());
	pic->flags.is_patch = false;
}

void FIC_ImageAt(void)
{
	fipic_t *pic = FI_GetPic(FI_GetToken());

	FI_InitValue(&pic->x, FI_GetFloat());
	FI_InitValue(&pic->y, FI_GetFloat());
	FI_ClearAnimation(pic);
	pic->lump[0] = W_CheckNumForName(FI_GetToken());
	pic->flags.is_patch = false;
}

void FIC_Patch(void)
{
	fipic_t *pic = FI_GetPic(FI_GetToken());

	FI_InitValue(&pic->x, FI_GetFloat());
	FI_InitValue(&pic->y, FI_GetFloat());
	FI_ClearAnimation(pic);
	pic->lump[0] = W_CheckNumForName(FI_GetToken());
	pic->flags.is_patch = true;
}

void FIC_SetPatch(void)
{
	fipic_t *pic = FI_GetPic(FI_GetToken());
	pic->lump[0] = W_CheckNumForName(FI_GetToken());
	pic->flags.is_patch = true;
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

void FIC_PicOffX(void)
{
	fipic_t *pic = FI_GetPic(FI_GetToken());
	FI_SetValue(&pic->x, FI_GetFloat());
}

void FIC_PicOffY(void)
{
	fipic_t *pic = FI_GetPic(FI_GetToken());
	FI_SetValue(&pic->y, FI_GetFloat());
}

void FIC_PicRGB(void)
{
	fipic_t *pic = FI_GetPic(FI_GetToken());
	int i;
	
	for(i = 0; i < 3; i++) FI_SetValue(pic->color + i, FI_GetFloat());
}

void FIC_PicAlpha(void)
{
	fipic_t *pic = FI_GetPic(FI_GetToken());
	FI_SetValue(pic->color + 3, FI_GetFloat());
}

void FIC_OffsetX(void)
{
	FI_SetValue(fi_imgoffset, FI_GetFloat());
}

void FIC_OffsetY(void)
{
	FI_SetValue(fi_imgoffset + 1, FI_GetFloat());
}

void FIC_Sound(void)
{
	int num = Def_Get(DD_DEF_SOUND, FI_GetToken(), NULL);
	if(num > 0) S_LocalSound(num, NULL);
}

void FIC_SoundAt(void)
{
	int num = Def_Get(DD_DEF_SOUND, FI_GetToken(), NULL);
	int vol = (int)(FI_GetFloat() * 128);

	if(vol > 127) vol = 127;
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
	for(i = 0; i < 4; i++) FI_SetValue(fi_filter + i, FI_GetFloat());
}

void FIC_Text(void)
{
	fitext_t *tex = FI_GetText(FI_GetToken());

	FI_InitValue(&tex->x, FI_GetFloat());
	FI_InitValue(&tex->y, FI_GetFloat());
	FI_SetText(tex, FI_GetToken());
	tex->pos = 0; // Restart the text.
}

void FIC_TextFromDef(void)
{
	fitext_t *tex = FI_GetText(FI_GetToken());
	char *str;

	FI_InitValue(&tex->x, FI_GetFloat());
	FI_InitValue(&tex->y, FI_GetFloat());
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
	
	FI_InitValue(&tex->x, FI_GetFloat());
	FI_InitValue(&tex->y, FI_GetFloat());
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
	tex->used = false;
}

void FIC_TextColor(void)
{
	int idx = FI_GetInteger(), c;

	if(idx < 1) idx = 1;
	if(idx > 9) idx = 9;
	for(c = 0; c < 3; c++) 
		FI_SetValue(&fi_textcolor[idx - 1][c], FI_GetFloat());
}

void FIC_TextRGB(void)
{
	int i;
	fitext_t *tex = FI_GetText(FI_GetToken());
	for(i = 0; i < 3; i++) FI_SetValue(&tex->color[i], FI_GetFloat());
}

void FIC_TextAlpha(void)
{
	fitext_t *tex = FI_GetText(FI_GetToken());
	FI_SetValue(&tex->color[3], FI_GetFloat());
}

void FIC_TextOffX(void)
{
	fitext_t *tex = FI_GetText(FI_GetToken());
	FI_SetValue(&tex->x, FI_GetFloat());
}

void FIC_TextOffY(void)
{
	fitext_t *tex = FI_GetText(FI_GetToken());
	FI_SetValue(&tex->y, FI_GetFloat());
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

void FIC_PicScaleX(void)
{
	fipic_t *pic = FI_GetPic(FI_GetToken());
	FI_SetValue(&pic->scale[0], FI_GetFloat());
}

void FIC_PicScaleY(void)
{
	fipic_t *pic = FI_GetPic(FI_GetToken());
	FI_SetValue(&pic->scale[1], FI_GetFloat());
}

void FIC_PicScale(void)
{
	fipic_t *pic = FI_GetPic(FI_GetToken());
	FI_SetValue(&pic->scale[0], FI_GetFloat());
	FI_SetValue(&pic->scale[1], FI_GetFloat());
}

void FIC_TextScaleX(void)
{
	fitext_t *tex = FI_GetText(FI_GetToken());
	FI_SetValue(&tex->scale[0], FI_GetFloat());
}

void FIC_TextScaleY(void)
{
	fitext_t *tex = FI_GetText(FI_GetToken());
	FI_SetValue(&tex->scale[1], FI_GetFloat());
}

void FIC_TextScale(void)
{
	fitext_t *tex = FI_GetText(FI_GetToken());
	FI_SetValue(&tex->scale[0], FI_GetFloat());
	FI_SetValue(&tex->scale[1], FI_GetFloat());
}
