// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// $Log$
// Revision 1.1  2003/02/26 19:21:48  skyjake
// Initial checkin
//
// Revision 1.1  2002/09/29 01:11:46  Jaakko
// Added Doomsday sources
//
//
// DESCRIPTION:
//	DOOM selection menu, options, episode etc.
//	Sliders and icons. Kinda widget stuff.
//
//-----------------------------------------------------------------------------

#pragma warning(disable:4018)

static const char
rcsid[] = "$Id$";

//#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <ctype.h>
#include <io.h>

#include <math.h>

#include "doomdef.h"
#include "dstrings.h"

#include "d_main.h"
#include "d_config.h"

#include "i_system.h"
#include "i_video.h"
#include "v_video.h"

#include "r_local.h"
#include "p_saveg.h"

#include "hu_stuff.h"

#include "g_game.h"

#include "m_argv.h"
#include "m_swap.h"

#include "s_sound.h"

#include "doomstat.h"
#include "x_hair.h"

#include "m_menu.h"
#include "mn_def.h"


void P_SetMessage(player_t *player, char *msg);

typedef struct
{
	int	width, height;
} MenuRes_t;

// Macros.
#define		NUMSAVESLOTS	8
#define		CVAR(typ, x)	(*(typ*)Con_GetVariable(x)->ptr)


extern boolean		message_dontfuckwithme;
extern boolean		chat_on;		// in heads-up code

extern int joyaxis[3];

//
// defaulted values
//
int			mouseSensitivity;       // has default

// Show messages has default, 0 = off, 1 = on
int			showMessages = true;

int			menuFogTexture = 0;	
static float mfSpeeds[2] = { 1*.05f, 1*-.085f };
//static float mfSpeeds[2] = { 1*.05f, 2*-.085f };
static float mfAngle[2] = { 93, 12 };
static float mfPosAngle[2] = { 35, 77 };
static float mfPos[2][2];
static float mfAlpha = 0;
float		menuScale = .8f;

// Blocky mode, has default, 0 = high, 1 = normal
int			detailLevel;		
int			screenblocks = 10;		// has default

// temp for screenblocks (0-9)
int			screenSize;		

// -1 = no quicksave slot picked!
int			quickSaveSlot;          

 // 1 = message to be printed
int			messageToPrint;
// ...and here is the message string!
char*			messageString;		

// message x & y
int			messx;			
int			messy;
int			messageLastMenuActive;

// timed message = no input from user
boolean			messageNeedsInput;     

void    (*messageRoutine)(int response);

#define SAVESTRINGSIZE 	24

static char *yesno[3] = { "NO", "YES", "MAYBE?" };

char gammamsg[5][81];

// we are going to be entering a savegame string
int			saveStringEnter;              
int             	saveSlot;	// which slot to save in
int			saveCharIndex;	// which char we're editing
// old save description before edit
char			saveOldString[SAVESTRINGSIZE];  

boolean			inhelpscreens;
boolean			menuactive;

#define SKULLXOFF		-32

extern boolean		sendpause;
char			savegamestrings[10][SAVESTRINGSIZE];

char	endstring[160];
char	*episodemsg;


// Alpha level for the entire menu. Used primarily by M_WriteText2().
float	menu_alpha = 1;
int		menu_color = 0;
float	skull_angle = 0;
int		MenuTime;
int		typein_time = 0;

//static float flashcolor[3] = { 1, .7f, .1f };


//
// MENU TYPEDEFS
//

short		itemOn;				// menu item skull is on
short		skullAnimCounter;	// skull animation counter
short		whichSkull;			// which skull to draw

// graphic name of skulls
// warning: initializer-string for array of chars is too long
char    skullName[2][9] = {"M_SKULL1","M_SKULL2"};

/*static MenuRes_t resolutions[] =
{
	320, 240,
	640, 480,
	800, 600,
	1024, 768,
	1152, 864,
	1280, 1024,
	1600, 1200,
	0, 0	// The terminator.
};
static int selRes = 0;	// Will be determined when needed.
*/

// current menudef
Menu_t*	currentMenu;                          

//
// PROTOTYPES
//
void M_NewGame(int choice);
void M_Episode(int choice);
void M_ChooseSkill(int choice);
void M_LoadGame(int choice);
void M_SaveGame(int choice);
void M_Options(int choice);
void M_EndGame(int choice);
void M_ReadThis(int choice);
void M_ReadThis2(int choice);
void M_QuitDOOM(int choice);

void M_OpenDCP(int choice);
void M_ChangeMessages(int choice);
void M_AlwaysRun(int option);
void M_LookSpring(int option);
void M_NoAutoAim(int option);
void M_AllowJump(int option);
void M_HUDInfo(int option);
void M_HUDScale(int option);
void M_HUDRed(int option);
void M_HUDGreen(int option);
void M_HUDBlue(int option);
void M_Xhair(int option);
void M_XhairSize(int option);
void M_ChangeSensitivity(int choice);
void M_SfxVol(int choice);
void M_MusicVol(int choice);
//void M_MusicDevice(int choice);
void M_3DSounds(int choice);
void M_ReverbVol(int choice);
void M_SfxFreq(int choice);
void M_Sfx16bit(int choice);
void M_ChangeDetail(int choice);
void M_SizeDisplay(int choice);
void M_SizeStatusBar(int option);
void M_StartGame(int choice);
void M_Sound(int choice);
void M_SkyDetail(int option);
void M_Mipmapping(int option);
void M_TexQuality(int option);
void M_ForceTexReload(int option);
void M_FPSCounter(int option);
void M_DynLights(int option);
void M_DLBlend(int option);
void M_SpriteLight(int option);
void M_DLIntensity(int option);
void M_Flares(int option);
void M_FlareStyle(int option);
void M_FlareIntensity(int option);
void M_FlareSize(int option);
void M_SpriteAlign(int option);
void M_SpriteBlending(int option);
void M_3DModels(int option);
void M_Particles(int option);
void M_DetailTextures(int option);
void M_InverseY(int option);
void M_MouseLook(int option);
void M_MouseLookInverse(int option);
void M_MouseXSensi(int option);
void M_MouseYSensi(int option);
void M_EnableJoy(int option);
void M_JoyAxis(int option);
//void M_JoyYAxis(int option);
//void M_JoyZAxis(int option);
void M_JoySensi(int option);
void M_JoyLook(int option);
void M_InverseJoyLook(int option);
void M_POVLook(int option);

void M_FinishReadThis(int choice);
void M_LoadSelect(int choice);
void M_SaveSelect(int choice);
void M_ReadSaveStrings(void);
void M_QuickSave(void);
void M_QuickLoad(void);

void M_DrawMainMenu(void);
void M_DrawReadThis1(void);
void M_DrawReadThis2(void);
void M_DrawNewGame(void);
void M_DrawEpisode(void);
void M_DrawOptions(void);
void M_DrawOptions2(void);
void M_DrawGameplay(void);
void M_DrawHUD(void);
void M_DrawGraphics(void);
void M_DrawMouseOpts(void);
void M_DrawJoyOpts(void);
void M_DrawSound(void);
void M_DrawLoad(void);
void M_DrawSave(void);

void M_DrawSaveLoadBorder(int x,int y);
void M_SetupNextMenu(Menu_t *menudef);
void M_DrawThermo(int x,int y,int thermWidth,int thermDot);
void M_DrawSlider(Menu_t *menu, int index, int width, int dot);
void M_DrawEmptyCell(Menu_t *menu,int item);
void M_DrawSelCell(Menu_t *menu,int item);
int  M_StringHeight(char *string, dpatch_t *font);
void M_StartControlPanel(void);
void M_StartMessage(char *string,void *routine,boolean input);
void M_StopMessage(void);
void M_ClearMenus (void);

//
// DOOM MENU
//
enum
{
    newgame = 0,
	multiplayer,
    options,
    loadgame,
    savegame,
    readthis,
    quitdoom,
    main_end
} main_e;

MenuItem_t MainItems[] =
{
	{ ITT_EFUNC, "NEW GAME", M_NewGame, 0 },// "M_NGAME" },
	{ ITT_EFUNC, "MULTIPLAYER", SCEnterMultiplayerMenu, 0 },
	{ ITT_EFUNC, "OPTIONS", M_Options, 0 },// "M_OPTION" },
	{ ITT_EFUNC, "LOAD GAME", M_LoadGame, 0 },// "M_LOADG" },
	{ ITT_EFUNC, "SAVE GAME", M_SaveGame, 0 },// "M_SAVEG" },
	{ ITT_EFUNC, "READ THIS!", M_ReadThis, 0 },// "M_RDTHIS" },
	{ ITT_EFUNC, "QUIT GAME", M_QuitDOOM, 0 } // "M_QUITG" }
};

Menu_t MainDef =
{
	97, 64,
	M_DrawMainMenu,
	7, MainItems,
	0, MENU_NONE,
	hu_font_b, //1, 0, 0,
	LINEHEIGHT_B,
	0, 7
};

//
// EPISODE SELECT
//
MenuItem_t EpisodeItems[] =
{
	// Text defs TXT_EPISODE1..4.
	{ ITT_EFUNC, "K", M_Episode, 0 /*, "M_EPI1"*/ },
	{ ITT_EFUNC, "T", M_Episode, 1 /*, "M_EPI2"*/ },
	{ ITT_EFUNC, "I", M_Episode, 2 /*, "M_EPI3"*/ },
	{ ITT_EFUNC, "T", M_Episode, 3 /*, "M_EPI4"*/ }
};

Menu_t EpiDef =
{
	48, 63,
	M_DrawEpisode,
	4, EpisodeItems,
	0, MENU_MAIN,
	hu_font_b, //1, 0, 0,
	LINEHEIGHT,
	0, 4
};

//
// NEW GAME
//
MenuItem_t NewGameItems[] =
{
	{ ITT_EFUNC, "I", M_ChooseSkill, 0, "M_JKILL" },
	{ ITT_EFUNC, "H", M_ChooseSkill, 1, "M_ROUGH" },
	{ ITT_EFUNC, "H", M_ChooseSkill, 2, "M_HURT" },
	{ ITT_EFUNC, "U", M_ChooseSkill, 3, "M_ULTRA" },
	{ ITT_EFUNC, "N", M_ChooseSkill, 4, "M_NMARE" }
};

Menu_t NewDef =
{
	48, 63,
	M_DrawNewGame,
	5, NewGameItems,
	2, MENU_EPISODE,
	hu_font_b, //1, 0, 0,
	LINEHEIGHT,
	0, 5
};


//
// OPTIONS MENU
//
enum
{
    endgame,
    messages,
    scrnsize,
    option_empty1,
    mousesens,
    option_empty2,
    soundvol,
    opt_end
} options_e;

MenuItem_t ReadItems1[] =
{
	{ ITT_EFUNC, "", M_ReadThis2, 0 }
};

Menu_t ReadDef1 =
{
	280, 185,
	M_DrawReadThis1,
	1, ReadItems1,
	0, MENU_MAIN,
	hu_font_b, //1, 0, 0,
	LINEHEIGHT,
	0, 1
};

MenuItem_t ReadItems2[] =
{
	{ ITT_EFUNC, "", M_FinishReadThis, 0 }
};

Menu_t ReadDef2 =
{
	330, 175,
	M_DrawReadThis2,
	1, ReadItems2,
	0, MENU_MAIN,
	hu_font_b, //1, 0, 0,
	LINEHEIGHT,
	0, 1
};

//
// LOAD GAME MENU
//

enum { load_end = NUMSAVESLOTS };

MenuItem_t LoadItems[] =
{
	{ ITT_EFUNC, "1", M_LoadSelect, 0, "" },
	{ ITT_EFUNC, "2", M_LoadSelect, 1, "" },
	{ ITT_EFUNC, "3", M_LoadSelect, 2, "" },
	{ ITT_EFUNC, "4", M_LoadSelect, 3, "" },
	{ ITT_EFUNC, "5", M_LoadSelect, 4, "" },
	{ ITT_EFUNC, "6", M_LoadSelect, 5, "" },
	{ ITT_EFUNC, "7", M_LoadSelect, 6, "" },
	{ ITT_EFUNC, "8", M_LoadSelect, 7, "" }
};

Menu_t LoadDef =
{
	80, 54,
	M_DrawLoad,
	NUMSAVESLOTS, LoadItems,
	0, MENU_MAIN,
	hu_font_b, //1, 0, 0,
	LINEHEIGHT,
	0, NUMSAVESLOTS
};

//
// SAVE GAME MENU
//

MenuItem_t SaveItems[] =
{
	{ ITT_EFUNC, "1", M_SaveSelect, 0, "" },
	{ ITT_EFUNC, "2", M_SaveSelect, 1, "" },
	{ ITT_EFUNC, "3", M_SaveSelect, 2, "" },
	{ ITT_EFUNC, "4", M_SaveSelect, 3, "" },
	{ ITT_EFUNC, "5", M_SaveSelect, 4, "" },
	{ ITT_EFUNC, "6", M_SaveSelect, 5, "" },
	{ ITT_EFUNC, "7", M_SaveSelect, 6, "" },
	{ ITT_EFUNC, "8", M_SaveSelect, 7, "" }
};

Menu_t SaveDef =
{
	80, 54,
	M_DrawSave,
	NUMSAVESLOTS, SaveItems,
	0, MENU_MAIN,
	hu_font_b, //1, 0, 0,
	LINEHEIGHT,
	0, NUMSAVESLOTS
};

static MenuItem_t OptionsItems[] =
{
	{ ITT_EFUNC, "END GAME", M_EndGame, 0 },
	{ ITT_EFUNC, "CONTROL PANEL", M_OpenDCP, 0 },
	{ ITT_SETMENU, "GAMEPLAY...", NULL, MENU_GAMEPLAY },
	{ ITT_SETMENU, "HUD...", NULL, MENU_HUD },
	{ ITT_SETMENU, "SOUND...", NULL, MENU_OPTIONS2 },
	{ ITT_SETMENU, "CONTROLS...", NULL, MENU_CONTROLS },
	{ ITT_SETMENU, "MOUSE...", NULL, MENU_MOUSE },
	{ ITT_SETMENU, "JOYSTICK...", NULL, MENU_JOYSTICK }

//	{ ITT_SETMENU, "GRAPHICS...", NULL, MENU_GRAPHICS },
//	{ ITT_SETMENU, "OTHER...", NULL, MENU_OPTIONS2 },
//	{ ITT_SETMENU, "MOUSE OPTIONS...", NULL, MENU_MOUSEOPTS },
//	{ ITT_SETMENU, "JOYSTICK OPTIONS...", NULL, MENU_JOYCONFIG }
};

static Menu_t OptionsDef =
{
	108, 80,
	M_DrawOptions,
	8, OptionsItems,
	0, MENU_MAIN,
	hu_font_a, //1, 0, 0, 
	LINEHEIGHT_A,
	0, 8
};

static MenuItem_t Options2Items[] =
{
	{ ITT_LRFUNC, "SFX VOLUME", M_SfxVol, 0 },
	{ ITT_LRFUNC, "MUSIC VOLUME", M_MusicVol, 0 },
	{ ITT_EFUNC, "AUDIO PANEL", M_OpenDCP, 1 },
/*	{ ITT_LRFUNC, "MUSIC :", M_MusicDevice, 0 },
	{ ITT_EFUNC, "3D SOUNDS :", M_3DSounds, 0 },
	{ ITT_LRFUNC, "REVERB VOLUME", M_ReverbVol, 0 },
	{ ITT_LRFUNC, "SFX FREQUENCY :", M_SfxFreq, 0 },
	{ ITT_EFUNC, "16 BIT INTERPOLATION :", M_Sfx16bit, 0 }*/
};

static Menu_t Options2Def =
{
	70, 40,
	M_DrawOptions2,
	3, Options2Items,
	0, MENU_OPTIONS,
	hu_font_a, 
	LINEHEIGHT_A,
	0, 3
};

static MenuItem_t GameplayItems[] =
{
	{ ITT_EFUNC, "MESSAGES :", M_ChangeMessages, 0 },
	{ ITT_EFUNC, "ALWAYS RUN :", M_AlwaysRun, 0 },
	{ ITT_EFUNC, "LOOKSPRING :", M_LookSpring, 0 },
	{ ITT_EFUNC, "AUTOAIM :", M_NoAutoAim, 0 },
	{ ITT_EFUNC, "JUMPING :", M_AllowJump, 0 },
};

static Menu_t GameplayDef =
{
	70, 40,
	M_DrawGameplay,
	5, GameplayItems,
	0, MENU_OPTIONS,
	hu_font_a, 
	LINEHEIGHT_A,
	0, 5
};

static MenuItem_t HUDItems[] =
{
	{ ITT_EFUNC, "SHOW HEALTH :", M_HUDInfo, HUD_HEALTH },
	{ ITT_EFUNC, "SHOW AMMO :", M_HUDInfo, HUD_AMMO },
	{ ITT_EFUNC, "SHOW KEYS :", M_HUDInfo, HUD_KEYS },
	{ ITT_EFUNC, "SHOW ARMOR :", M_HUDInfo, HUD_ARMOR },
	{ ITT_LRFUNC, "SCALE", M_HUDScale, 0 },
	{ ITT_LRFUNC, "COLOR RED    ", M_HUDRed, 0 },
	{ ITT_LRFUNC, "COLOR GREEN", M_HUDGreen, 0 },
	{ ITT_LRFUNC, "COLOR BLUE  ", M_HUDBlue, 0 },
	{ ITT_LRFUNC, "CROSSHAIR :", M_Xhair, 0 },
	{ ITT_LRFUNC, "CROSSHAIR SIZE", M_XhairSize, 0 },
	{ ITT_LRFUNC, "SCREEN SIZE", M_SizeDisplay, 0 },
	{ ITT_LRFUNC, "STATUS BAR SIZE", M_SizeStatusBar, 0}
};

static Menu_t HUDDef =
{
	70, 40,
	M_DrawHUD,
	12, HUDItems,
	0, MENU_OPTIONS,
	hu_font_a, 
	LINEHEIGHT_A,
	0, 12
};


/*static MenuItem_t GraphicsItems[] = 
{
	{ ITT_EFUNC, "RESOLUTION...", M_EnterResolutionMenu, 0 },
	{ ITT_LRFUNC, "SKY DETAIL", M_SkyDetail, 0 },
	{ ITT_LRFUNC, "MIPMAPPING :", M_Mipmapping, 0 },
	{ ITT_LRFUNC, "TEX QUALITY :", M_TexQuality, 0 },
	{ ITT_EFUNC, "FORCE TEX RELOAD", M_ForceTexReload, 0 },
	{ ITT_EFUNC, "FPS COUNTER :", M_FPSCounter, 0 },
	{ ITT_EFUNC, "DYNAMIC LIGHTS :", M_DynLights, 0 },
	{ ITT_LRFUNC, "DYNLIGHT BLENDING :", M_DLBlend, 0 },
	{ ITT_EFUNC, "LIGHTS ON SPRITES :", M_SpriteLight, 0 },
	{ ITT_LRFUNC, "DYNLIGHT INTENSITY", M_DLIntensity, 0 },
	{ ITT_LRFUNC, "LENS FLARES :", M_Flares, 0 },
	{ ITT_LRFUNC, "FLARE INTENSITY", M_FlareIntensity, 0 },
	{ ITT_LRFUNC, "FLARE SIZE", M_FlareSize, 0 },
	{ ITT_LRFUNC, "ALIGN SPRITES TO :", M_SpriteAlign, 0 },
	{ ITT_EFUNC, "SPRITE BLENDING :", M_SpriteBlending, 0 },
	{ ITT_EFUNC, "3D MODELS :", M_3DModels, 0 },
	{ ITT_EFUNC, "PARTICLES :", M_Particles, 0 },
	{ ITT_EFUNC, "DETAIL TEXTURES :", M_DetailTextures, 0 }
};*/

/*static Menu_t GraphicsDef =
{
	60, 40,
	M_DrawGraphics,
	18, GraphicsItems,
	0, MENU_OPTIONS,
	hu_font_a, //1, 0, 0, 
	LINEHEIGHT_A,
	0, 18
};*/

/*static MenuItem_t ResolutionItems[] =
{
	{ ITT_LRFUNC, "RESOLUTION :", M_ResSlider, 0 },
//	{ ITT_EMPTY, NULL, NULL, 0, MENU_NONE },
	{ ITT_EFUNC, "USE", M_ResSelect, 0 },
	{ ITT_EFUNC, "SET DEFAULT", M_ResSelect, 1 }
};

static Menu_t ResolutionDef =
{
	60, 40,
	M_DrawResolution,
	3, ResolutionItems,
	0, MENU_GRAPHICS,
	hu_font_a, //1, 0, 0, 
	LINEHEIGHT_A,
	0, 3
};*/

static MenuItem_t InputItems[] =
{
	{ ITT_EFUNC, "MOUSE LOOK :", M_MouseLook, 0 },
	{ ITT_EFUNC, "INVERSE LOOK :", M_MouseLookInverse, 0 },
	{ ITT_LRFUNC, "X SENSITIVITY", M_MouseXSensi, 0 },
	{ ITT_LRFUNC, "Y SENSITIVITY", M_MouseYSensi, 0 },
};

static Menu_t InputDef = 
{
	70, 40,
	M_DrawMouseOpts,
	4, InputItems,
	0, MENU_OPTIONS,
	hu_font_a, 
	LINEHEIGHT_A,
	0, 4
};

static MenuItem_t JoyItems[] =
{
	{ ITT_LRFUNC, "X AXIS :", M_JoyAxis, 0 << 8 },
	{ ITT_LRFUNC, "Y AXIS :", M_JoyAxis, 1 << 8 },
	{ ITT_LRFUNC, "Z AXIS :", M_JoyAxis, 2 << 8 },
	{ ITT_LRFUNC, "RX AXIS :", M_JoyAxis, 3 << 8 },
	{ ITT_LRFUNC, "RY AXIS :", M_JoyAxis, 4 << 8 },
	{ ITT_LRFUNC, "RZ AXIS :", M_JoyAxis, 5 << 8 },
	{ ITT_LRFUNC, "SLIDER 1 :", M_JoyAxis, 6 << 8 },
	{ ITT_LRFUNC, "SLIDER 2 :", M_JoyAxis, 7 << 8 },
	{ ITT_EFUNC, "ENABLE JOY LOOK :", M_JoyLook, 0 },
	{ ITT_EFUNC, "INVERSE LOOK :", M_InverseJoyLook, 0 },
	{ ITT_EFUNC, "POV LOOK :", M_POVLook, 0 }
};

static Menu_t JoyDef =
{
	70, 40,
	M_DrawJoyOpts,
	11, JoyItems,
	0, MENU_OPTIONS,
	hu_font_a, 
	LINEHEIGHT_A,
	0, 11
};

Menu_t *menulist[] =
{
	&MainDef,
	&EpiDef,
	&NewDef,
	&OptionsDef,
	&Options2Def,
	&GameplayDef,
	&HUDDef, 
	&ControlsDef,
	&InputDef,
	&JoyDef,
	&LoadDef,
	&SaveDef,
	&MultiplayerMenu,
	&GameSetupMenu,
	&PlayerSetupMenu,
	NULL
};

void M_SetNumItems(Menu_t *menu, int num)
{
	menu->itemCount = menu->numVisItems = num;
}

/*static int findRes(int w, int h)
{
	int i;

	for(i=0; resolutions[i].width; i++)
		if(resolutions[i].width == w && resolutions[i].height == h)
			return i;
	return -1;
}*/

static void ChangeIntCVar(char *name, int delta)
{
	cvar_t	*cv = Con_GetVariable(name);
	int		val = *(int*) cv->ptr;

	val += delta;
	if(val > cv->max) val = cv->max;
	if(val < cv->min) val = cv->min;
	*(int*) cv->ptr = val;
}

static void ChangeFloatCVar(char *name, float delta)
{
	cvar_t	*cv = Con_GetVariable(name);
	float	val = *(float*) cv->ptr;

	val += delta;
	if(val > cv->max) val = cv->max;
	if(val < cv->min) val = cv->min;
	*(float*) cv->ptr = val;
}

//
// M_ReadSaveStrings
//  read the strings from the savegame files
//
void M_ReadSaveStrings(void)
{
    int     i;
    char    name[256];
	
    for(i=0; i < load_end; i++)
    {
		SV_SaveGameFile(i, name);	
		if(!SV_GetSaveDescription(name, savegamestrings[i])) 
		{
			strcpy(savegamestrings[i], EMPTYSTRING);
			LoadItems[i].type = ITT_EMPTY;
		}
		else
			LoadItems[i].type = ITT_EFUNC;
    }
}


//
// M_LoadGame & Cie.
//
void M_DrawLoad(void)
{
    int             i;
	
    GL_DrawPatch(72, 28, W_GetNumForName("M_LOADG"));
    for (i = 0;i < load_end; i++)
    {
		M_DrawSaveLoadBorder(LoadDef.x,LoadDef.y+LINEHEIGHT*i);
		M_WriteText2(LoadDef.x,LoadDef.y+LINEHEIGHT*i,savegamestrings[i],
			hu_font_a, 1, 0, 0);
    }
}



//
// Draw border for the savegame description
//
void M_DrawSaveLoadBorder(int x,int y)
{
    GL_DrawPatch(x-8, y+7, W_GetNumForName("M_LSLEFT"));
	GL_SetPatch(W_GetNumForName("M_LSCNTR"));
	GL_DrawRectTiled(x-3, y-4, 24*8, 14, 8, 14);
    GL_DrawPatch(x+8*24, y+7, W_GetNumForName("M_LSRGHT"));
}



//
// User wants to load this game
//
void M_LoadSelect(int choice)
{
    char    name[256];
	
	SV_SaveGameFile(choice, name);
    G_LoadGame (name);
    M_ClearMenus ();
}

//
// Selected from DOOM menu
//
void M_LoadGame (int choice)
{
    if(IS_CLIENT && !Get(DD_PLAYBACK))
    {
		M_StartMessage(LOADNET, NULL, false);
		return;
    }
    M_SetupNextMenu(&LoadDef);
    M_ReadSaveStrings();
}


//
//  M_SaveGame & Cie.
//
void M_DrawSave(void)
{
    int             i;
	
    GL_DrawPatch(72, 28, W_GetNumForName("M_SAVEG"));
    for (i = 0;i < load_end; i++)
    {
		M_DrawSaveLoadBorder(LoadDef.x,LoadDef.y+LINEHEIGHT*i);
		M_WriteText2(LoadDef.x,LoadDef.y+LINEHEIGHT*i,savegamestrings[i],
			hu_font_a, 1, 0, 0);
    }
    if (saveStringEnter)
    {
		i = M_StringWidth(savegamestrings[saveSlot], hu_font);
		M_WriteText(LoadDef.x + i,LoadDef.y+LINEHEIGHT*saveSlot,"_");
    }
}

//
// M_Responder calls this when user is finished
//
void M_DoSave(int slot)
{
    G_SaveGame(slot, savegamestrings[slot]);
    M_ClearMenus ();
	
    // PICK QUICKSAVE SLOT YET?
    if (quickSaveSlot == -2)
		quickSaveSlot = slot;
}

//
// User wants to save. Start string input for M_Responder
//
void M_SaveSelect(int choice)
{
    // we are going to be intercepting all chars
    saveStringEnter = 1;
    
    saveSlot = choice;
    strcpy(saveOldString,savegamestrings[choice]);
    if (!strcmp(savegamestrings[choice],EMPTYSTRING))
		savegamestrings[choice][0] = 0;
    saveCharIndex = strlen(savegamestrings[choice]);
}

//
// Selected from DOOM menu
//
void M_SaveGame (int choice)
{
    if(!usergame || Get(DD_PLAYBACK))
    {
		M_StartMessage(SAVEDEAD, NULL, false);
		return;
    }
	if(IS_CLIENT)
	{
		M_StartMessage(GET_TXT(TXT_SAVENET), NULL, false);
		return;
	}
    if (gamestate != GS_LEVEL)
		return;
	
    M_SetupNextMenu(&SaveDef);
    M_ReadSaveStrings();
}



//
//      M_QuickSave
//
char    tempstring[80];

void M_QuickSaveResponse(int ch)
{
    if (ch == 'y')
    {
		M_DoSave(quickSaveSlot);
		S_LocalSound(sfx_swtchx, NULL);
    }
}

void M_QuickSave(void)
{
    if (!usergame)
    {
		S_LocalSound(sfx_oof, NULL);
		return;
    }
	
    if (gamestate != GS_LEVEL)
		return;
	
    if (quickSaveSlot < 0)
    {
		M_StartControlPanel();
		M_ReadSaveStrings();
		M_SetupNextMenu(&SaveDef);
		quickSaveSlot = -2;	// means to pick a slot now
		return;
    }
    sprintf(tempstring,QSPROMPT,savegamestrings[quickSaveSlot]);
    M_StartMessage(tempstring,M_QuickSaveResponse,true);
}



//
// M_QuickLoad
//
void M_QuickLoadResponse(int ch)
{
    if (ch == 'y')
    {
		M_LoadSelect(quickSaveSlot);
		S_LocalSound(sfx_swtchx, NULL);
    }
}


void M_QuickLoad(void)
{
    if (IS_NETGAME)
    {
		M_StartMessage(QLOADNET,NULL,false);
		return;
    }
	
    if (quickSaveSlot < 0)
    {
		M_StartMessage(QSAVESPOT,NULL,false);
		return;
    }
    sprintf(tempstring,QLPROMPT,savegamestrings[quickSaveSlot]);
    M_StartMessage(tempstring,M_QuickLoadResponse,true);
}




//
// Read This Menus
// Had a "quick hack to fix romero bug"
//
void M_DrawReadThis1(void)
{
    inhelpscreens = true;
    switch ( gamemode )
    {
	case commercial:
		//V_DrawPatchDirect (0,0,0,W_CacheLumpName("HELP",PU_CACHE));
		GL_DrawPatch(0, 0, W_GetNumForName("HELP"));
		break;
	case shareware:
	case registered:
	case retail:
		//V_DrawPatchDirect (0,0,0,W_CacheLumpName("HELP1",PU_CACHE));
		GL_DrawPatch(0, 0, W_GetNumForName("HELP1"));
		break;
	default:
		break;
    }
    return;
}



//
// Read This Menus - optional second page.
//
void M_DrawReadThis2(void)
{
    inhelpscreens = true;
    switch ( gamemode )
    {
	case retail:
	case commercial:
		// This hack keeps us from having to change menus.
		//V_DrawPatchDirect (0,0,0,W_CacheLumpName("CREDIT",PU_CACHE));
		GL_DrawPatch(0, 0, W_GetNumForName("CREDIT"));
		break;
	case shareware:
	case registered:
		//V_DrawPatchDirect (0,0,0,W_CacheLumpName("HELP2",PU_CACHE));
		GL_DrawPatch(0, 0, W_GetNumForName("HELP2"));
		break;
	default:
		break;
    }
    return;
}


//
// Change Sfx & Music volumes
//
/*void M_DrawSound(void)
{
    GL_DrawPatch(60, 38, W_GetNumForName("M_SVOL"));

    M_DrawThermo(SoundDef.x, SoundDef.y + SoundDef.itemHeight,
		 16, snd_SfxVolume);

    M_DrawThermo(SoundDef.x, SoundDef.y + SoundDef.itemHeight*3,
		 16, snd_MusicVolume);
}*/

/*void M_Sound(int choice)
{
    M_SetupNextMenu(&SoundDef);
}*/

void M_SfxVol(int choice)
{
	int vol = snd_SfxVolume;

    switch(choice)
    {
	case 0:
		if (vol) vol--;
		break;
	case 1:
		if (vol < 15) vol++;
		break;
    }
	//SetSfxVolume(vol*17); // 15*17 = 255
	Set(DD_SFX_VOLUME, vol*17);
}

void M_MusicVol(int choice)
{
	int vol = snd_MusicVolume;

    switch(choice)
    {
	case 0:
		if (vol) vol--;
		break;
	case 1:
		if (vol < 15) vol++;
		break;
    }
	//SetMIDIVolume(vol*17);    
	Set(DD_MUSIC_VOLUME, vol*17);
}

/*void M_MusicDevice(int option)
{
	int snd_MusicDevice = Get(DD_MUSIC_DEVICE);
	
	if(option == RIGHT_DIR)
	{
		if(snd_MusicDevice < 1) 
		{
			snd_MusicDevice++;
			cfg.customMusic = false;
		}
		else if(!cfg.customMusic)
			cfg.customMusic = true;
	}
	else if(cfg.customMusic)
		cfg.customMusic = false;
	else if(snd_MusicDevice > 0) 
		snd_MusicDevice--;

	// Setup the music.
//	SetMusicDevice(snd_MusicDevice); 
//#pragma message("M_MusicDevice: Music preferences not yet implemented.")
	
	if(!snd_MusicDevice) 
		S_StopMusic();
	else
		S_LevelMusic();
}*/

void M_3DSounds(int choice)
{
	CVAR(int, "sound-3d") ^= 1;
}

void M_ReverbVol(int option)
{
	ChangeFloatCVar("sound-reverb-volume", option==RIGHT_DIR? 0.1f : -0.1f);
	// Don't go over 1.0 in the menu.
	if(CVAR(float, "sound-reverb-volume") > 1)
		CVAR(float, "sound-reverb-volume") = 1;
}

void M_SfxFreq(int option)
{
	cvar_t	*cv = Con_GetVariable("sound-rate");
	int		oldval = *(int*) cv->ptr, val = oldval;
	
	val = option==RIGHT_DIR? val*2 : val/2;
	if(val > 44100) val = 44100;
	if(val < 11025) val = 11025;
	*(int*) cv->ptr = val;
}

void M_Sfx16bit(int choice)
{
	CVAR(int, "sound-16bit") ^= 1;
}





//
// M_DrawMainMenu
//
void M_DrawMainMenu(void)
{
    GL_DrawPatch(94,2,W_GetNumForName("M_DOOM"));
}




//
// M_NewGame
//
void M_DrawNewGame(void)
{
    GL_DrawPatch(96,14,W_GetNumForName("M_NEWG"));
    GL_DrawPatch(54,38,W_GetNumForName("M_SKILL"));
}

void M_NewGame(int choice)
{
    if (IS_NETGAME)// && !Get(DD_PLAYBACK))
    {
		M_StartMessage(NEWGAME,NULL,false);
		return;
    }
	
    if ( gamemode == commercial )
		M_SetupNextMenu(&NewDef);
    else
		M_SetupNextMenu(&EpiDef);
}


//
//      M_Episode
//
int     epi;

void M_DrawEpisode(void)
{
    //GL_DrawPatch(54, 38, W_GetNumForName("M_EPISOD"));
	GL_DrawPatch(96, 14, W_GetNumForName("M_NEWG"));
	M_DrawTitle(episodemsg, 40);
}

void M_VerifyNightmare(int ch)
{
    if (ch != 'y')
	return;
		
    G_DeferedInitNew(sk_nightmare,epi+1,1);
    M_ClearMenus ();
}

void M_ChooseSkill(int choice)
{
    if (choice == sk_nightmare)
    {
		M_StartMessage(NIGHTMARE,M_VerifyNightmare,true);
		return;
    }
	
    G_DeferedInitNew(choice,epi+1,1);
    M_ClearMenus ();
}

void M_Episode(int choice)
{
    if((gamemode == shareware) && choice)
    {
		M_StartMessage(SWSTRING, NULL, false);
		M_SetupNextMenu(&ReadDef1);
		return;
    }

    // Yet another hack...
    if((gamemode == registered) && (choice > 2))
    {
		Con_Message("M_Episode: 4th episode requires UltimateDOOM\n");
		choice = 0;
    }
	
    epi = choice;
    M_SetupNextMenu(&NewDef);
}



//
// M_Options
//
char    detailNames[2][9]	= {"M_GDHIGH","M_GDLOW"};
char	msgNames[2][9]		= {"M_MSGOFF","M_MSGON"};


void M_DrawOptions(void)
{
	GL_DrawPatch(94,2,W_GetNumForName("M_DOOM"));
    //GL_DrawPatch(108,60,W_GetNumForName("M_OPTTTL"));
	M_DrawTitle("OPTIONS", 60);
}

void M_DrawOptions2(void)
{
	Menu_t *menu = &Options2Def;
//	char *musDevStr[3] = { "NO", "MIDI", "CUSTOM / MIDI" };
	/*char *freqStr;
	int i, off = 100, freq;*/

	M_DrawTitle("SOUND OPTIONS", menu->y - 20);

	M_DrawSlider(menu, 0, 16, snd_SfxVolume);
	M_DrawSlider(menu, 1, 16, snd_MusicVolume);
/*	i = Get(DD_MUSIC_DEVICE);
	if(i && cfg.customMusic) i = 2;
	if(i < 0) i = 0;
	if(i > 2) i = 2;
	M_WriteMenuText(menu, 2, musDevStr[i]);
	M_WriteMenuText(menu, 3, yesno[CVAR(int, "sound-3d") != 0]);
	M_DrawSlider(menu, 4, 11, CVAR(float, "sound-reverb-volume")*10 + 0.5f);
	freq = CVAR(int, "sound-rate");
	if(freq == 11025) 
		freqStr = "11 KHz";
	else if(freq == 22050)
		freqStr = "22 KHz";
	else 
		freqStr = "44 KHz";
	M_WriteMenuText(menu, 5, freqStr); 
	M_WriteMenuText(menu, 6, yesno[CVAR(int, "sound-16bit") != 0]);
*/
}

void M_DrawGameplay(void)
{
	Menu_t *menu = &GameplayDef;
	
	M_DrawTitle("GAMEPLAY OPTIONS", menu->y-20);

	M_WriteMenuText(menu, 0, yesno[showMessages != 0]); 
	M_WriteMenuText(menu, 1, yesno[cfg.alwaysRun != 0]);
	M_WriteMenuText(menu, 2, yesno[cfg.lookSpring != 0]);
	M_WriteMenuText(menu, 3, yesno[!cfg.noAutoAim]);
	M_WriteMenuText(menu, 4, yesno[cfg.jumpEnabled != 0]);
}

//===========================================================================
// M_DrawHUD
//===========================================================================
void M_DrawHUD(void)
{
	Menu_t *menu = &HUDDef;
	char *xhairnames[NUM_XHAIRS+1] = { "NONE", "CROSS", "ANGLES", "SQUARE",
		"OPEN SQUARE", "DIAMOND", "V" };
	
	M_DrawTitle("HUD OPTIONS", menu->y - 20);

	M_WriteMenuText(menu, 0, yesno[cfg.hudShown[HUD_HEALTH]]); 
	M_WriteMenuText(menu, 1, yesno[cfg.hudShown[HUD_AMMO]]); 
	M_WriteMenuText(menu, 2, yesno[cfg.hudShown[HUD_KEYS]]); 
	M_WriteMenuText(menu, 3, yesno[cfg.hudShown[HUD_ARMOR]]); 
	M_DrawSlider(menu, 4, 10, cfg.hudScale*10 - 3 + .5f);
	M_DrawSlider(menu, 5, 11, cfg.hudColor[0]*10 + .5f);
	M_DrawSlider(menu, 6, 11, cfg.hudColor[1]*10 + .5f);
	M_DrawSlider(menu, 7, 11, cfg.hudColor[2]*10 + .5f);
	M_WriteMenuText(menu, 8, xhairnames[cfg.xhair]); 
	M_DrawSlider(menu, 9, 9, cfg.xhairSize);
	M_DrawSlider(menu, 10, 9, screenblocks-3);
	M_DrawSlider(menu, 11, 20, cfg.sbarscale-1);
}

/*void M_DrawGraphics(void)
{
	char *mipStr[6] = 
	{
		"NEAREST", "LINEAR", "N, MIP N", "L, MIP N", "N, MIP L", "L, MIP L"
	};
	char *texQStr[9] =
	{
		"0 - MINIMUM",
		"1 - VERY LOW",
		"2 - LOW",
		"3 - POOR",
		"4 - AVERAGE",
		"5 - GOOD",
		"6 - HIGH",
		"7 - VERY HIGH",
		"8 - MAXIMUM"
	};
	char *dlblendStr[4] = { "MULTIPLY", "ADD", "NONE", "DON'T RENDER" };
	char *flareStr[6] = { "OFF", "1", "2", "3", "4", "5" };
	char *alignStr[4] = { "CAMERA", "VIEW PLANE", "CAMERA (R)", "VIEW PLANE (R)" };
	Menu_t *menu = &GraphicsDef;

	M_DrawTitle("GRAPHICS OPTIONS", menu->y-20);

	M_DrawSlider(menu, 1, 5, Get(DD_SKY_DETAIL)-3);
	M_WriteMenuText(menu, 2, mipStr[Get(DD_MIPMAPPING)]);
	M_WriteMenuText(menu, 3, texQStr[CVAR(int, "r_texquality")]);
	M_WriteMenuText(menu, 5, yesno[cfg.showFPS]);
	M_WriteMenuText(menu, 6, yesno[CVAR(int, "dynlights")]);
	M_WriteMenuText(menu, 7, dlblendStr[CVAR(int, "dlblend")]);
	M_WriteMenuText(menu, 8, yesno[CVAR(int, "sprlight")]);
	M_DrawSlider(menu, 9, 11, CVAR(float, "dlfactor")*10 + .5f);
	M_WriteMenuText(menu, 10, flareStr[CVAR(int, "flares")]);
	M_DrawSlider(menu, 11, 11, CVAR(int, "flareintensity")/10);
	M_DrawSlider(menu, 12, 11, CVAR(int, "flaresize"));
	M_WriteMenuText(menu, 13, alignStr[CVAR(int, "spralign")]);
	M_WriteMenuText(menu, 14, yesno[CVAR(int, "sprblend")]);
	M_WriteMenuText(menu, 15, yesno[CVAR(int, "usemodels")!=0]);
	M_WriteMenuText(menu, 16, yesno[CVAR(int, "useparticles")!=0]);
	M_WriteMenuText(menu, 17, yesno[CVAR(int, "r_detail")!=0]);
}*/

/*void M_DrawResolution(void)
{
	Menu_t *menu = &ResolutionDef;
	char buffer[80];

	M_DrawTitle("RESOLUTION OPTIONS", menu->y-20);

	if(selRes == -1)
		strcpy(buffer, "NOT AVAILABLE");
	else
	{
		sprintf(buffer, "%d X %d%s",
			resolutions[selRes].width,
			resolutions[selRes].height,
			selRes == findRes(Get(DD_DEFAULT_RES_X), Get(DD_DEFAULT_RES_Y))? 
				" (DEFAULT)" : "");			
	}
	M_WriteMenuText(menu, 0, buffer);
}*/

//===========================================================================
// M_DrawMouseOpts
//===========================================================================
void M_DrawMouseOpts(void)
{
	Menu_t *menu = &InputDef;

	M_DrawTitle("MOUSE OPTIONS", menu->y - 20);

	M_WriteMenuText(menu, 0, yesno[cfg.usemlook]);
	M_WriteMenuText(menu, 1, yesno[cfg.mlookInverseY]);
	M_DrawSlider(menu, 2, 21, cfg.mouseSensiX/2);
	M_DrawSlider(menu, 3, 21, cfg.mouseSensiY/2);
}

//===========================================================================
// M_DrawJoyOpts
//===========================================================================
void M_DrawJoyOpts(void)
{
	Menu_t *menu = &JoyDef;
	char *axisname[] = { "-", "MOVE", "TURN", "STRAFE", "LOOK" };

	M_DrawTitle("JOYSTICK OPTIONS", menu->y - 20);

	M_WriteMenuText(menu, 0, axisname[cfg.joyaxis[0]]);
	M_WriteMenuText(menu, 1, axisname[cfg.joyaxis[1]]);
	M_WriteMenuText(menu, 2, axisname[cfg.joyaxis[2]]);
	M_WriteMenuText(menu, 3, axisname[cfg.joyaxis[3]]);
	M_WriteMenuText(menu, 4, axisname[cfg.joyaxis[4]]);
	M_WriteMenuText(menu, 5, axisname[cfg.joyaxis[5]]);
	M_WriteMenuText(menu, 6, axisname[cfg.joyaxis[6]]);
	M_WriteMenuText(menu, 7, axisname[cfg.joyaxis[7]]);

	M_WriteMenuText(menu, 8, yesno[cfg.usejlook]);
	M_WriteMenuText(menu, 9, yesno[cfg.jlookInverseY]);
	M_WriteMenuText(menu, 10, yesno[cfg.povLookAround]);
}

void M_Options(int choice)
{
    M_SetupNextMenu(&OptionsDef);
}

void M_OpenDCP(int choice)
{
	M_ClearMenus();
	Con_Execute(choice? "panel audio" : "panel", true);
}

//
//      Toggle messages on/off
//
void M_ChangeMessages(int choice)
{
    showMessages = 1 - showMessages;
	P_SetMessage(players + consoleplayer, !showMessages? MSGOFF : MSGON);
    message_dontfuckwithme = true;
}

void M_AlwaysRun(int option)
{
	cfg.alwaysRun = !cfg.alwaysRun;
}

void M_LookSpring(int option)
{
	cfg.lookSpring = !cfg.lookSpring;
}

void M_NoAutoAim(int option)
{
	cfg.noAutoAim = !cfg.noAutoAim;
}

void M_AllowJump(int option)
{
	cfg.jumpEnabled = !cfg.jumpEnabled;
}

void M_HUDInfo(int option)
{
	cfg.hudShown[option] = !cfg.hudShown[option];
}

static void M_FloatMod10(float *variable, int option)
{
	int val = (*variable+.05f) * 10;

	if(option == RIGHT_DIR) 
	{
		if(val < 10) val++;
	}
	else if(val > 0) val--;
	*variable = val/10.0f;
}

void M_HUDScale(int option)
{
	int val = (cfg.hudScale+.05f) * 10;

	if(option == RIGHT_DIR) 
	{
		if(val < 12) val++;
	}
	else if(val > 3) val--;
	cfg.hudScale = val/10.0f;
}

void M_HUDRed(int option)
{
	M_FloatMod10(&cfg.hudColor[0], option);
}

void M_HUDGreen(int option)
{
	M_FloatMod10(&cfg.hudColor[1], option);
}

void M_HUDBlue(int option)
{
	M_FloatMod10(&cfg.hudColor[2], option);
}

void M_Xhair(int option)
{
	if(option == RIGHT_DIR)
	{
		if(cfg.xhair < NUM_XHAIRS) cfg.xhair++;
	}
	else if(cfg.xhair > 0) cfg.xhair--;
}

void M_XhairSize(int option)
{
	if(option == RIGHT_DIR)
	{
		if(cfg.xhairSize < 8) cfg.xhairSize++;
	}
	else if(cfg.xhairSize > 0) cfg.xhairSize--;
}

void M_SizeStatusBar(int option)
{
	if(option == RIGHT_DIR)
	{
		if(cfg.sbarscale < 20) cfg.sbarscale++;
	}
	else 
		if(cfg.sbarscale > 1) cfg.sbarscale--;

	R_SetViewSize(screenblocks, 0);
}


//
// M_EndGame
//
void M_EndGameResponse(int ch)
{
    if (ch != 'y')
	return;
		
    currentMenu->lastOn = itemOn;
    M_ClearMenus ();
    D_StartTitle ();
}

void M_EndGame(int choice)
{
    choice = 0;
    if (!usergame)
    {
		S_LocalSound(sfx_oof, NULL);
		return;
    }
	
    if (IS_NETGAME)
    {
		M_StartMessage(NETEND,NULL,false);
		return;
    }
	
    M_StartMessage(ENDGAME, M_EndGameResponse, true);
}




//
// M_ReadThis
//
void M_ReadThis(int choice)
{
    choice = 0;
    M_SetupNextMenu(&ReadDef1);
}

void M_ReadThis2(int choice)
{
    choice = 0;
    M_SetupNextMenu(&ReadDef2);
}

void M_FinishReadThis(int choice)
{
    choice = 0;
    M_SetupNextMenu(&MainDef);
}




//
// M_QuitDOOM
//
int     quitsounds[8] =
{
    sfx_pldeth,
    sfx_dmpain,
    sfx_popain,
    sfx_slop,
    sfx_telept,
    sfx_posit1,
    sfx_posit3,
    sfx_sgtatk
};

int     quitsounds2[8] =
{
    sfx_vilact,
    sfx_getpow,
    sfx_boscub,
    sfx_slop,
    sfx_skeswg,
    sfx_kntdth,
    sfx_bspact,
    sfx_sgtatk
};



void M_QuitResponse(int ch)
{
    if (ch != 'y')
		return;
	//G_CheckDemoStatus();
	// Don't play any stupid sounds.
#if 0
    if (!IS_NETGAME)
    {
		if (gamemode == commercial)
			S_LocalSound(quitsounds2[(gametic>>2)&7], NULL);
		else
			S_LocalSound(quitsounds[(gametic>>2)&7], NULL);
		//	I_WaitVBL(105);
    }
#endif
    Sys_Quit ();
}




void M_QuitDOOM(int choice)
{
	Con_Open(false);

	// We pick index 0 which is language sensitive,
	//  or one at random, between 1 and maximum number.
	if (language != english )
		sprintf(endstring,"%s\n\n%s", endmsg[0], DOSY );
	else
		sprintf(endstring,"%s\n\n%s", endmsg[(gametic%(NUM_QUITMESSAGES+1))], DOSY);
	
	M_StartMessage(endstring,M_QuitResponse,true);
}




void M_ChangeSensitivity(int choice)
{
    switch(choice)
    {
	case 0:
		if (mouseSensitivity)
			mouseSensitivity--;
		break;
	case 1:
		if (mouseSensitivity < 9)
			mouseSensitivity++;
		break;
    }
}




void M_ChangeDetail(int choice)
{
    choice = 0;
    detailLevel = 1 - detailLevel;

    // FIXME - does not work. Remove anyway?
    fprintf( stderr, "M_ChangeDetail: low detail mode n.a.\n");

    return;
    
    /*R_SetViewSize (screenblocks, detailLevel);

    if (!detailLevel)
	players[consoleplayer].message = DETAILHI;
    else
	players[consoleplayer].message = DETAILLO;*/
}




void M_SizeDisplay(int choice)
{
    switch(choice)
    {
	case 0:
		if (screenSize > 0)
		{
			screenblocks--;
			screenSize--;
		}
		break;
	case 1:
		if (screenSize < 8)
		{
			screenblocks++;
			screenSize++;
		}
		break;
    }
	
	
    R_SetViewSize (screenblocks, detailLevel);
}

void M_SkyDetail(int option)
{
	int skyDetail = Get(DD_SKY_DETAIL);

	if(option == RIGHT_DIR)
	{
		if(skyDetail < 7)
		{
			skyDetail++;
		}
	}
	else if(skyDetail > 3)
	{
		skyDetail--;
	}
	Rend_SkyParams(DD_SKY, DD_COLUMNS, skyDetail);
}


void M_Mipmapping(int option)
{
	int mipmapping = Get(DD_MIPMAPPING);

	if(option == RIGHT_DIR)
	{
		if(mipmapping < 5) mipmapping++;
	}
	else if(mipmapping > 0) mipmapping--;
	
	GL_TextureFilterMode(DD_TEXTURES, mipmapping);
}

void M_TexQuality(int option)
{
	ChangeIntCVar("r_texquality", option==RIGHT_DIR? 1 : -1);
}

void M_ForceTexReload(int option)
{
	Con_Execute("texreset", false);
	P_SetMessage(players + consoleplayer, "All Textures Deleted");
}

void M_FPSCounter(int option)
{
	cfg.showFPS = !cfg.showFPS;
}

void M_DynLights(int option) 
{
	CVAR(int, "dynlights") ^= 1;
}

void M_DLBlend(int option) 
{
	ChangeIntCVar("dlblend", option==RIGHT_DIR? 1 : -1);
}

void M_SpriteLight(int option) 
{
	CVAR(int, "sprlight") ^= 1;
}

void M_DLIntensity(int option) 
{
	cvar_t	*cv = Con_GetVariable("dlfactor");
	float	val = *(float*) cv->ptr;

	val += option==RIGHT_DIR? .1f : -.1f;
	if(val > cv->max) val = cv->max;
	if(val < cv->min) val = cv->min;
	*(float*) cv->ptr = val;
}

void M_Flares(int option) 
{
	ChangeIntCVar("flares", option==RIGHT_DIR? 1 : -1);
}

void M_FlareIntensity(int option) 
{
	ChangeIntCVar("flareintensity", option==RIGHT_DIR? 10 : -10);
}

void M_FlareSize(int option)
{
	ChangeIntCVar("flaresize", option==RIGHT_DIR? 1 : -1);
}

void M_SpriteAlign(int option) 
{
	ChangeIntCVar("spralign", option==RIGHT_DIR? 1 : -1);
}

void M_SpriteBlending(int option)
{
	CVAR(int, "sprblend") ^= 1;
}

void M_3DModels(int option)
{
	CVAR(int, "usemodels") ^= 1;
}

void M_Particles(int option)
{
	CVAR(int, "useparticles") ^= 1;
}

void M_DetailTextures(int option)
{
	CVAR(int, "r_detail") ^= 1;
}

/*void M_EnterResolutionMenu(int option)
{
	selRes = findRes(Get(DD_SCREEN_WIDTH), Get(DD_SCREEN_HEIGHT));
	M_SetupNextMenu(&ResolutionDef);
}*/

/*void M_ResSlider(int option)
{
	if(option == RIGHT_DIR)
	{
		if(resolutions[selRes+1].width)
			selRes++;
	}
	else if(selRes > 0) selRes--;
}*/

/*void M_ResSelect(int option)
{
	switch(option)
	{
	case 0:		// make current
		if(GL_ChangeResolution(resolutions[selRes].width, 
			resolutions[selRes].height, 0))
		{
			P_SetMessage(players + consoleplayer, "Resolution Changed");
		}
		else
		{
			P_SetMessage(players + consoleplayer, "Resolution Change Failed");
		}
		break;

	case 1:		// make default
		Set(DD_DEFAULT_RES_X, resolutions[selRes].width);
		Set(DD_DEFAULT_RES_Y, resolutions[selRes].height);
	}
}*/

void M_InverseY(int option)
{
	Set(DD_MOUSE_INVERSE_Y, !Get(DD_MOUSE_INVERSE_Y));
}

void M_MouseLook(int option)
{
	cfg.usemlook = !cfg.usemlook;
}

void M_MouseLookInverse(int option)
{
	cfg.mlookInverseY = !cfg.mlookInverseY;
}

void M_MouseXSensi(int option)
{
	if(option == RIGHT_DIR)
	{
		if(cfg.mouseSensiX < 39)
		{
			cfg.mouseSensiX+=2;
		}
	}
	else if(cfg.mouseSensiX > 1)
	{
		cfg.mouseSensiX-=2;
	}
}

void M_MouseYSensi(int option)
{
	if(option == RIGHT_DIR)
	{
		if(cfg.mouseSensiY < 39)
		{
			cfg.mouseSensiY+=2;
		}
	}
	else if(cfg.mouseSensiY > 1)
	{
		cfg.mouseSensiY-=2;
	}
}

void M_JoySensi(int option)
{
	int *val = &CVAR(int, "i_joySensi");

	if(option == RIGHT_DIR)
	{
		if(*val < 9) (*val)++;
	}
	else if(*val > 1) (*val)--;
}

void M_EnableJoy(int option)
{
	CVAR(int, "i_usejoystick") ^= 1;
}

//===========================================================================
// M_JoyAxis
//	option >> 8 must be in range 0..7.
//===========================================================================
void M_JoyAxis(int option)
{
	if(option & RIGHT_DIR)
	{
		if(cfg.joyaxis[option >> 8] < 4) cfg.joyaxis[option >> 8]++;
	}
	else
	{
		if(cfg.joyaxis[option >> 8] > 0) cfg.joyaxis[option >> 8]--;
	}
}

/*void M_JoyYAxis(int option)
{
	if(option == RIGHT_DIR)
	{
		if(cfg.joyaxis[1] < 3) cfg.joyaxis[1]++;
	}
	else
	{
		if(cfg.joyaxis[1] > 0) cfg.joyaxis[1]--;
	}
}

void M_JoyZAxis(int option)
{
	if(option == RIGHT_DIR)
	{
		if(cfg.joyaxis[2] < 3) cfg.joyaxis[2]++;
	}
	else
	{
		if(cfg.joyaxis[2] > 0) cfg.joyaxis[2]--;
	}
}*/

void M_JoyLook(int option)
{
	cfg.usejlook = !cfg.usejlook;
}

void M_InverseJoyLook(int option)
{
	cfg.jlookInverseY = !cfg.jlookInverseY;
}

void M_POVLook(int option)
{
	cfg.povLookAround = !cfg.povLookAround;
}


//
//      Menu Functions
//

// Height is in pixels, and determines the scale of the whole thing.
// [left/right:6, middle:8, thumb:5]
void M_DrawThermo2(int x, int y, int thermWidth, int thermDot, int height)
{
	float	scale = height/13.0f;	// 13 is the normal scale.
    int		xx;

    xx = x;
	GL_SetPatch(W_GetNumForName("M_THERML"));
	GL_DrawRect(xx, y, 6*scale, height, 1, 1, 1, menu_alpha);
    xx += 6*scale;
	GL_SetPatch(W_GetNumForName("M_THERM2")); // in jdoom.wad
	GL_DrawRectTiled(xx, y, 8*thermWidth*scale, height, 8*scale, height);
	xx += 8*thermWidth*scale;
	GL_SetPatch(W_GetNumForName("M_THERMR"));
	GL_DrawRect(xx, y, 6*scale, height, 1, 1, 1, menu_alpha);
	GL_SetPatch(W_GetNumForName("M_THERMO"));
	GL_DrawRect(x + (6+thermDot*8)*scale, y, 6*scale, height, 
		1, 1, 1, menu_alpha);
}


void M_DrawSlider(Menu_t *menu, int index, int width, int dot)
{
	int offx = 0;

	if(menu->items[index].text)
		offx = M_StringWidth(menu->items[index].text, menu->font);
	offx /= 4;
	offx *= 4;
	M_DrawThermo2(menu->x+6+offx, menu->y+menu->itemHeight*index,
		width, dot, menu->itemHeight-1);
}


void
M_DrawThermo
( int	x,
  int	y,
  int	thermWidth,
  int	thermDot )
{
/*  
	int		xx;

    xx = x;
    GL_DrawPatch(xx,y,W_GetNumForName("M_THERML"));
    xx += 8;
	GL_SetPatch(W_GetNumForName("M_THERM2")); // in jdoom.wad
	GL_DrawRectTiled(xx, y, 8*thermWidth, 13, 8, 13);
	xx += 8*thermWidth;
    GL_DrawPatch(xx,y,W_GetNumForName("M_THERMR"));

    GL_DrawPatch((x+8) + thermDot*8, y, W_GetNumForName("M_THERMO"));*/

	M_DrawThermo2(x, y, thermWidth, thermDot, 13);
}



void
M_DrawEmptyCell
( Menu_t*	menu,
  int		item )
{
    GL_DrawPatch(menu->x - 10, menu->y+item*menu->itemHeight - 1,
		       W_GetNumForName("M_CELL1"));
}

void
M_DrawSelCell
( Menu_t*	menu,
  int		item )
{
    GL_DrawPatch(menu->x - 10, menu->y+item*menu->itemHeight - 1,
		       W_GetNumForName("M_CELL2"));
}


void
M_StartMessage
( char*		string,
  void*		routine,
  boolean	input )
{
    messageLastMenuActive = menuactive;
    messageToPrint = 1;
    messageString = string;
    messageRoutine = routine;
    messageNeedsInput = input;
    menuactive = true;
	typein_time = 0;
    return;
}



void M_StopMessage(void)
{
    menuactive = messageLastMenuActive;
    messageToPrint = 0;
}



//
// Find string width from hu_font chars
//
int M_StringWidth(char* string, dpatch_t *font)
{
    int             i;
    int             w = 0;
    int             c;
	
    for (i = 0;i < strlen(string);i++)
    {
		c = toupper(string[i]) - HU_FONTSTART;
		if (c < 0 || c >= HU_FONTSIZE)
			w += 4;
		else
			w += SHORT (font[c].width);
    }
    return w;
}



//
//      Find string height from hu_font chars
//
int M_StringHeight(char* string, dpatch_t *font)
{
    int             i;
    int             h;
    int             height = SHORT(font[0].height);
	
    h = height;
    for (i = 0;i < strlen(string);i++)
		if (string[i] == '\n')
			h += height;
	return h;
}


//
//      Write a string using a colored, custom font
//
void
M_WriteText2
( int		x,
 int		y,
 char*		string,
 dpatch_t	*font,
 float		red,
 float		green,
 float		blue
 )
{
    int		w;
    char*	ch;
    int		c;
    int		cx;
    int		cy;
	int		count = 0, maxcount = typein_time*2, yoff;

	// Disable type-in?
	if(cfg.menuEffects > 0) maxcount = 0xffff;

	if(red >= 0) gl.Color4f(red, green, blue, menu_alpha);

    ch = string;
    cx = x;
    cy = y;
	
    while(1)
    {
		c = *ch++;
		count++;
		yoff = 0;
		if(count == maxcount) 
		{
			if(red >= 0) gl.Color4f(1, 1, 1, 1);
		}
		else if(count+1 == maxcount)
		{
			if(red >= 0) gl.Color4f((1+red)/2,
				(1+green)/2, (1+blue)/2, menu_alpha);
		}
		else if(count > maxcount) 
		{
			break;
		}
		if (!c) break;
		if (c == '\n')
		{
			cx = x;
			cy += 12;
			continue;
		}
		
		c = toupper(c) - HU_FONTSTART;
		if (c < 0 || c >= HU_FONTSIZE)
		{
			cx += 4;
			continue;
		}
	
		w = SHORT (font[c].width);
		/*if (cx+w > SCREENWIDTH)
			break;*/
		GL_DrawPatch_CS(cx, cy + yoff, font[c].lump);
		cx+=w;
    }
}

// Menu text is white.
void M_WriteMenuText(Menu_t *menu, int index, char *text)
{
	int off = 0;

	if(menu->items[index].text)
		off = M_StringWidth(menu->items[index].text, menu->font) + 4;

	M_WriteText2(menu->x + off,
		menu->y + menu->itemHeight*index, text, 
		menu->font, 1, 1, 1);
}

void M_DrawTitle(char *text, int y)
{
	M_WriteText2(160 - M_StringWidth(text, hu_font_b)/2,
		y, text, hu_font_b,
		cfg.menuColor[0], cfg.menuColor[1], cfg.menuColor[2]);
}

//
//      Write a string using the hu_font
//
void
M_WriteText
( int		x,
 int		y,
 char*		string)
{
/*    int		w;
    char*	ch;
    int		c;
    int		cx;
    int		cy;
	
	
    ch = string;
    cx = x;
    cy = y;
	
    while(1)
    {
		c = *ch++;
		if (!c)
			break;
		if (c == '\n')
		{
			cx = x;
			cy += 12;
			continue;
		}
		
		c = toupper(c) - HU_FONTSTART;
		if (c < 0 || c>= HU_FONTSIZE)
		{
			cx += 4;
			continue;
		}
		
		w = SHORT (hu_font[c].patch->width);
		if (cx+w > SCREENWIDTH)
			break;
		GL_DrawPatch(cx, cy, hu_font[c].lump);
		cx+=w;
    }*/

	M_WriteText2(x, y, string, hu_font, 1, 1, 1);
}



//
// CONTROL PANEL
//

void SetMenu(MenuType_t menu)
{
	M_SetupNextMenu(menulist[menu]);
}

//
// M_Responder
//
boolean M_Responder (event_t* ev)
{
    int             ch;
    int             i;
    static  int     joywait = 0;
    static  int     mousewait = 0;
    static  int     mousey = 0;
    static  int     lasty = 0;
    static  int     mousex = 0;
    static  int     lastx = 0;
	int				firstVI, lastVI;	// first and last visible item
	MenuItem_t		*item;
	
	if(ev->data1 == DDKEY_RSHIFT)
	{
		shiftdown = (ev->type == ev_keydown || ev->type == ev_keyrepeat);
	}
	if(Ed_Responder(ev)) return true;

    ch = -1;
	
    if (ev->type == ev_joystick && joywait < Sys_GetTime())
    {
		if (ev->data3 == -1)
		{
			ch = DDKEY_UPARROW;
			joywait = Sys_GetTime() + 5;
		}
		else if (ev->data3 == 1)
		{
			ch = DDKEY_DOWNARROW;
			joywait = Sys_GetTime() + 5;
		}
		
		if (ev->data2 == -1)
		{
			ch = DDKEY_LEFTARROW;
			joywait = Sys_GetTime() + 2;
		}
		else if (ev->data2 == 1)
		{
			ch = DDKEY_RIGHTARROW;
			joywait = Sys_GetTime() + 2;
		}
		
		if (ev->data1&1)
		{
			ch = DDKEY_ENTER;
			joywait = Sys_GetTime() + 5;
		}
		if (ev->data1&2)
		{
			ch = DDKEY_BACKSPACE;
			joywait = Sys_GetTime() + 5;
		}
    }
    else
    {
		/*if (ev->type == ev_mouse && mousewait < Sys_GetTime())
		{
			mousey += ev->data3;
			if (mousey < lasty-30)
			{
				ch = DDKEY_DOWNARROW;
				mousewait = Sys_GetTime() + 5;
				mousey = lasty -= 30;
			}
			else if (mousey > lasty+30)
			{
				ch = DDKEY_UPARROW;
				mousewait = Sys_GetTime() + 5;
				mousey = lasty += 30;
			}
			
			mousex += ev->data2;
			if (mousex < lastx-30)
			{
				ch = DDKEY_LEFTARROW;
				mousewait = Sys_GetTime() + 5;
				mousex = lastx -= 30;
			}
			else if (mousex > lastx+30)
			{
				ch = DDKEY_RIGHTARROW;
				mousewait = Sys_GetTime() + 5;
				mousex = lastx += 30;
			}
			
			if (ev->data1&1)
			{
				ch = DDKEY_ENTER;
				mousewait = Sys_GetTime() + 15;
			}
			
			if (ev->data1&2)
			{
				ch = DDKEY_BACKSPACE;
				mousewait = Sys_GetTime() + 15;
			}
		}
		else*/
		if (ev->type == ev_keydown || ev->type == ev_keyrepeat)
		{
			ch = ev->data1;
		}
    }
    
    if (ch == -1)
		return false;
	
    
    // Save Game string input
    if (saveStringEnter)
    {
		switch(ch)
		{
		case DDKEY_BACKSPACE:
			if (saveCharIndex > 0)
			{
				saveCharIndex--;
				savegamestrings[saveSlot][saveCharIndex] = 0;
			}
			break;
			
		case DDKEY_ESCAPE:
			saveStringEnter = 0;
			strcpy(&savegamestrings[saveSlot][0],saveOldString);
			break;
			
		case DDKEY_ENTER:
			saveStringEnter = 0;
			if (savegamestrings[saveSlot][0])
				M_DoSave(saveSlot);
			break;
			
		default:
			ch = toupper(ch);
			if (ch != 32)
				if (ch-HU_FONTSTART < 0 || ch-HU_FONTSTART >= HU_FONTSIZE)
					break;
				if (ch >= 32 && ch <= 127 &&
					saveCharIndex < SAVESTRINGSIZE-1 &&
					M_StringWidth(savegamestrings[saveSlot], hu_font) <
					(SAVESTRINGSIZE-2)*8)
				{
					savegamestrings[saveSlot][saveCharIndex++] = ch;
					savegamestrings[saveSlot][saveCharIndex] = 0;
				}
				break;
		}
		return true;
    }
    
    // Take care of any messages that need input
    if (messageToPrint)
    {
		if (messageNeedsInput == true &&
			!(ch == ' ' || ch == 'n' || ch == 'y' || ch == DDKEY_ESCAPE))
			return false;
		
		menuactive = messageLastMenuActive;
		messageToPrint = 0;
		if (messageRoutine)
			messageRoutine(ch);
		
		menuactive = false;
		S_LocalSound(sfx_swtchx, NULL);
		return true;
    }
	
    if (devparm && ch == DDKEY_F1)
    {
		G_ScreenShot ();
		return true;
    }
	
    
    // Pop-up menu?
    if (!menuactive)
    {
		if (ch == DDKEY_ESCAPE && !chat_on)
		{
			M_StartControlPanel ();
			S_LocalSound(sfx_swtchn, NULL);
			return true;
		}
		return false;
    }

	firstVI = currentMenu->firstItem; 
	lastVI = firstVI + currentMenu->numVisItems-1;	
	if(lastVI > currentMenu->itemCount-1) 
		lastVI = currentMenu->itemCount-1;
	item = &currentMenu->items[itemOn];
	currentMenu->lastOn = itemOn;

    // Keys usable within menu
    switch (ch)
    {
	/*	case DDKEY_DOWNARROW:
	do
	{
	if (itemOn+1 > currentMenu->numitems-1)
				itemOn = 0;
				else itemOn++;
				S_LocalSound(NULL,sfx_pstop);
				} while(currentMenu->menuitems[itemOn].status==-1);
				return true;
				
				  case DDKEY_UPARROW:
				  do
				  {
				  if (!itemOn)
				  itemOn = currentMenu->numitems-1;
				  else itemOn--;
				  S_LocalSound(NULL,sfx_pstop);
				  } while(currentMenu->menuitems[itemOn].status==-1);
				  return true;
				  
					case DDKEY_LEFTARROW:
					if (currentMenu->menuitems[itemOn].routine &&
					currentMenu->menuitems[itemOn].status == 2)
					{
					S_LocalSound(NULL,sfx_stnmov);
					currentMenu->menuitems[itemOn].routine(0);
					}
					return true;
					
					  case DDKEY_RIGHTARROW:
					  if (currentMenu->menuitems[itemOn].routine &&
					  currentMenu->menuitems[itemOn].status == 2)
					  {
					  S_LocalSound(NULL,sfx_stnmov);
					  currentMenu->menuitems[itemOn].routine(1);
					  }
					  return true;
					  
						case DDKEY_ENTER:
						if (currentMenu->menuitems[itemOn].routine &&
						currentMenu->menuitems[itemOn].status)
						{
						currentMenu->lastOn = itemOn;
						if (currentMenu->menuitems[itemOn].status == 2)
						{
						currentMenu->menuitems[itemOn].routine(1);      // right arrow
						S_LocalSound(NULL,sfx_stnmov);
						}
						else
						{
						currentMenu->menuitems[itemOn].routine(itemOn);
						S_LocalSound(NULL,sfx_pistol);
						}
						}
						return true;
						
						  case DDKEY_ESCAPE:
						  currentMenu->lastOn = itemOn;
						  M_ClearMenus ();
						  S_LocalSound(NULL,sfx_swtchx);
						  return true;
						  
							case DDKEY_BACKSPACE:
							currentMenu->lastOn = itemOn;
							if (currentMenu->prevMenu)
							{
							currentMenu = currentMenu->prevMenu;
							itemOn = currentMenu->lastOn;
							S_LocalSound(NULL,sfx_swtchn);
							}
							return true;
							
							  default:
							  for (i = itemOn+1;i < currentMenu->numitems;i++)
							  if (currentMenu->menuitems[i].alphaKey == ch)
							  {
							  itemOn = i;
							  S_LocalSound(NULL,sfx_pstop);
							  return true;
							  }
							  for (i = 0;i <= itemOn;i++)
							  if (currentMenu->menuitems[i].alphaKey == ch)
							  {
							  itemOn = i;
							  S_LocalSound(NULL,sfx_pstop);
							  return true;
							  }
							  break;
		*/				
		
	case DDKEY_DOWNARROW:
		i = 0;
		do
		{
			if(itemOn+1 > lastVI)
			{
				itemOn = firstVI;
			}
			else
			{
				itemOn++;
			}
		} while(currentMenu->items[itemOn].type == ITT_EMPTY
			&& i++ < currentMenu->itemCount);
		menu_color = 0;
		S_LocalSound(sfx_pstop, NULL);
		return(true);
		
	case DDKEY_UPARROW:
		i = 0;
		do
		{
			if(itemOn <= firstVI)
			{
				itemOn = lastVI;
			}
			else
			{
				itemOn--;
			}
		} while(currentMenu->items[itemOn].type == ITT_EMPTY
			&& i++ < currentMenu->itemCount);
		menu_color = 0;
		S_LocalSound(sfx_pstop, NULL);
		return(true);
		
	case DDKEY_LEFTARROW:
		if(item->type == ITT_LRFUNC && item->func != NULL)
		{
			item->func(LEFT_DIR | item->option);
			S_LocalSound(sfx_stnmov, NULL);
		}
		else
		{
			// Let's try to change to the previous page.
			if(currentMenu->firstItem - currentMenu->numVisItems >= 0)
			{
				currentMenu->firstItem -= currentMenu->numVisItems;
				itemOn -= currentMenu->numVisItems;

				//Ensure cursor points to editable item		Anon $09 add next 3 lines
				firstVI = currentMenu->firstItem;
				while(currentMenu->items[itemOn].type == ITT_EMPTY
					&& itemOn > firstVI) itemOn--;

				// Make a sound, too.
				S_LocalSound(sfx_stnmov, NULL);
			}
		}
		return(true);
		
	case DDKEY_RIGHTARROW:
		if(item->type == ITT_LRFUNC && item->func != NULL)
		{
			item->func(RIGHT_DIR | item->option);
			S_LocalSound(sfx_stnmov, NULL);
		}
		else
		{
			// Move on to the next page, if possible.
			if(currentMenu->firstItem + currentMenu->numVisItems < 
				currentMenu->itemCount)
			{
				currentMenu->firstItem += currentMenu->numVisItems;
				itemOn += currentMenu->numVisItems;
				if(itemOn > currentMenu->itemCount-1)
					itemOn = currentMenu->itemCount-1;
				S_LocalSound(sfx_stnmov, NULL);						
			}
		}
		return(true);
		
	case DDKEY_ENTER:
		if(item->type == ITT_SETMENU)
		{
			M_SetupNextMenu(menulist[item->option]);
			S_LocalSound(sfx_pistol, NULL);
		}
		else if(item->func != NULL)
		{
			currentMenu->lastOn = itemOn;
			if(item->type == ITT_LRFUNC)
			{
				item->func(RIGHT_DIR | item->option);
				S_LocalSound(sfx_stnmov, NULL);
			}
			else if(item->type == ITT_EFUNC)
			{
				item->func(item->option);
				S_LocalSound(sfx_pistol, NULL);
			}
		}
		return(true);
		
	case DDKEY_ESCAPE:
		currentMenu->lastOn = itemOn;
		M_ClearMenus();
		S_LocalSound(sfx_swtchx, NULL);
		return(true);
		
	case DDKEY_BACKSPACE:
		currentMenu->lastOn = itemOn;
		if(currentMenu->prevMenu == MENU_NONE)
		{
			currentMenu->lastOn = itemOn;
			M_ClearMenus();
			S_LocalSound(sfx_swtchx, NULL);
		}
		else
		{
			//					SetMenu(currentMenu->prevMenu);
			currentMenu = menulist[currentMenu->prevMenu];
			itemOn = currentMenu->lastOn;
			S_LocalSound(sfx_swtchn, NULL);
			typein_time = 0;
		}
		return(true);
		
	default:
		for(i = firstVI; i <= lastVI; i++)
		{
			if(currentMenu->items[i].text && currentMenu->items[i].type != ITT_EMPTY)
			{
				if(toupper(ch) 
					== toupper(currentMenu->items[i].text[0]))
				{
					itemOn = i;
					return(true);
				}
			}
		}
		break;
	}
	
    return false;
}



//
// M_StartControlPanel
//
void M_StartControlPanel (void)
{
    // intro might call this repeatedly
    if (menuactive)
		return;
    
	Con_Open(false);
    menuactive = 1;
	menu_color = 0;
	skull_angle = 0;
    currentMenu = &MainDef;         // JDC
    itemOn = currentMenu->lastOn;   // JDC
	typein_time = 0;
}

void M_DrawBackground(void)
{
	int i;

	if(cfg.menuEffects > 1) return;

	if(cfg.menuFog == 0)
	{
		gl.Disable(DGL_TEXTURING);
		gl.Color4f(mfAlpha, mfAlpha/2, 0, mfAlpha/3);
		gl.Func(DGL_BLENDING, DGL_ZERO, DGL_ONE_MINUS_SRC_COLOR);
		GL_DrawRectTiled(0, 0, 320, 200, 1, 1);
		gl.Enable(DGL_TEXTURING);
	}

	gl.Bind(menuFogTexture);
	gl.Color3f(mfAlpha, mfAlpha, mfAlpha);
	gl.MatrixMode(DGL_TEXTURE);
	for(i = 0; i < 2; i++)
	{
		if(i || cfg.menuFog == 1)
		{
			gl.Color3f(mfAlpha, mfAlpha, mfAlpha);
			gl.Func(DGL_BLENDING, DGL_ZERO, DGL_ONE_MINUS_SRC_COLOR);
		}
		else
		{
			gl.Color3f(mfAlpha/5, mfAlpha/3, mfAlpha/2);
			gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_SRC_ALPHA);
		}
		gl.LoadIdentity();
		gl.Translatef(mfPos[i][VX]/320, mfPos[i][VY]/200, 0);
		gl.Rotatef(mfAngle[i], 0, 0, 1);
		gl.Translatef(-mfPos[i][VX]/320, -mfPos[i][VY]/200, 0);
		if(cfg.menuFog == 0)
			GL_DrawRectTiled(0, 0, 320, 200, 270/8, 4*225);
		else
			GL_DrawRectTiled(0, 0, 320, 200, 270, 225);
	}
	gl.LoadIdentity();	
	gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_ONE_MINUS_SRC_ALPHA);
}

//
// M_Drawer
// Called after the view has been rendered,
// but before it has been blitted.
//
void M_Drawer (void)
{
    static short	x;
    static short	y;
    short			i;
    short			max;
    char			string[40];
    int				start;
	float			scale;
	int				w, h, off_x, off_y;
	
    inhelpscreens = false;

	if(cfg.showFPS)
	{
		char fpsbuff[80];
		sprintf(fpsbuff, "%d FPS", DD_GetFrameRate());
		M_WriteText(320-M_StringWidth(fpsbuff, hu_font), 0, fpsbuff);
		GL_Update(DDUF_TOP);
	}
    
	// Draw menu background.
	if(mfAlpha) M_DrawBackground();

	// Setup matrix.
	if(messageToPrint || menuactive)
	{
		gl.MatrixMode(DGL_MODELVIEW);
		gl.PushMatrix();
		// Scale by the menuScale.
		gl.Translatef(160, 100, 0);
		gl.Scalef(cfg.menuScale, cfg.menuScale, 1);
		gl.Translatef(-160, -100, 0);
	}

    // Horiz. & Vertically center string and print it.
    if (messageToPrint)
    {
		start = 0;
		y = 100 - M_StringHeight(messageString, hu_font)/2;
		while(*(messageString+start))
		{
			for (i = 0;i < strlen(messageString+start);i++)
				if (*(messageString+start+i) == '\n')
				{
					memset(string,0,40);
					strncpy(string,messageString+start,i);
					start += i+1;
					break;
				}
				
			if (i == strlen(messageString+start))
			{
				strcpy(string,messageString+start);
				start += i;
			}
			
			x = 160 - M_StringWidth(string, hu_font)/2;
			M_WriteText2(x, y, string, hu_font_a, cfg.menuColor[0], 
				cfg.menuColor[1], cfg.menuColor[2]);
			y += SHORT(hu_font[0].height);
		}
		goto end_draw_menu;
    }
	if(!menuactive) return;

    if (currentMenu->drawFunc)
		currentMenu->drawFunc();         // call Draw routine
    
    // DRAW MENU
    x = currentMenu->x;
    y = currentMenu->y;
    max = currentMenu->itemCount;
	
    for(i=currentMenu->firstItem; 
		i<max && i<currentMenu->firstItem+currentMenu->numVisItems; 
		i++)
    {
		if(currentMenu->items[i].lumpname)
		{
			if(currentMenu->items[i].lumpname[0])
				GL_DrawPatch(x, y, W_GetNumForName(currentMenu->items[i].lumpname));
		}
		else if(currentMenu->items[i].text)
		{
			float t, r, g, b;
			// Which color?
			if(currentMenu->items[i].type == ITT_EMPTY)
			{
				r = 1;
				g = .7f;
				b = .3f;
			}
			else if(itemOn == i)
			{
				// Selection!
				if(menu_color <= 50)
					t = menu_color/50.0f;
				else
					t = (100-menu_color)/50.0f;
				r = cfg.menuColor[0]*t + cfg.flashcolor[0]*(1-t);
				g = cfg.menuColor[1]*t + cfg.flashcolor[1]*(1-t);
				b = cfg.menuColor[2]*t + cfg.flashcolor[2]*(1-t);
			}
			else
			{
				r = cfg.menuColor[0];
				g = cfg.menuColor[1];
				b = cfg.menuColor[2];
			}

			M_WriteText2(x, 
				y + currentMenu->itemHeight - currentMenu->font[0].height - 1, 
				currentMenu->items[i].text,
				currentMenu->font, r, g, b);
		}
		y += currentMenu->itemHeight;
    }
	
    // DRAW SKULL
	scale = currentMenu->itemHeight / (float) LINEHEIGHT;
	w = 20*scale; // skull size
	h = 19*scale;
	off_x = x + SKULLXOFF*scale + w/2;
	off_y = currentMenu->y + (itemOn-currentMenu->firstItem)*currentMenu->itemHeight + 
		currentMenu->itemHeight/2 - 1;
	GL_SetPatch(W_GetNumForName(skullName[whichSkull]));
	gl.MatrixMode(DGL_MODELVIEW);
	gl.PushMatrix();
	gl.Translatef(off_x, off_y, 0);
	gl.Scalef(1, 1.0f/1.2f, 1);
	if(skull_angle) gl.Rotatef(skull_angle, 0, 0, 1);
	gl.Scalef(1, 1.2f, 1);
	GL_DrawRect(-w/2, -h/2, w, h, 1, 1, 1, menu_alpha);
	gl.PopMatrix();

end_draw_menu:
	// Restore original matrix.	
	gl.MatrixMode(DGL_MODELVIEW);
	gl.PopMatrix();
}


//
// M_ClearMenus
//
void M_ClearMenus (void)
{
    menuactive = 0;
    // if (!IS_NETGAME && usergame && paused)
    //       sendpause = true;
}




//
// M_SetupNextMenu
//
void M_SetupNextMenu(Menu_t *menudef)
{
	if(!menudef) return;
    currentMenu = menudef;
    itemOn = currentMenu->lastOn;
	menu_color = 0;
	skull_angle = 0;
	typein_time = 0;
}


//
// M_Ticker
//
void M_Ticker (void)
{
	int	i;

	for(i=0; i<2; i++) 
	{
		if(cfg.menuFog == 1)
		{
			mfAngle[i] += mfSpeeds[i]/4;
			mfPosAngle[i] -= mfSpeeds[!i];
			mfPos[i][VX] = 160 + 120*cos(mfPosAngle[i]/180*PI);
			mfPos[i][VY] = 100 + 100*sin(mfPosAngle[i]/180*PI);
		}
		else
		{
			mfAngle[i] += mfSpeeds[i]/2;
			mfPosAngle[i] -= 3*mfSpeeds[!i];
			mfPos[i][VX] = 320 + 320*cos(mfPosAngle[i]/180*PI);
			mfPos[i][VY] = 240 + 240*sin(mfPosAngle[i]/180*PI);
		}
	}
	typein_time++;
	if(menuactive)
	{
		if(mfAlpha < 1) mfAlpha += .1f;
		if(mfAlpha > 1) mfAlpha = 1;
	}
	else 
	{
		if(mfAlpha > 0) mfAlpha -= .1f;
		if(mfAlpha < 0) mfAlpha = 0;
	}

    if (--skullAnimCounter <= 0)
    {
		whichSkull ^= 1;
		skullAnimCounter = 8;
    }
	if(menuactive)
	{
		float rewind = 20;

		MenuTime++;

		menu_color += cfg.flashspeed;
		if(menu_color >= 100) menu_color -= 100;

		if(cfg.turningSkull && currentMenu->items[itemOn].type == ITT_LRFUNC)
			skull_angle += 5;
		else if(skull_angle != 0)
		{
			if(skull_angle <= rewind || skull_angle >= 360-rewind)
				skull_angle = 0;
			else if(skull_angle < 180) 
				skull_angle -= rewind;
			else 
				skull_angle += rewind;
		}
		if(skull_angle >= 360) skull_angle -= 360;
	}
	MN_TickerEx();
}

//===========================================================================
// M_LoadData
//===========================================================================
void M_LoadData(void)
{
	if(!menuFogTexture && !Get(DD_NOVIDEO))
	{
		menuFogTexture = gl.NewTexture();
		gl.TexImage(DGL_LUMINANCE, 64, 64, 0,
			W_CacheLumpName("menufog", PU_CACHE));		
		gl.TexParameter(DGL_WRAP_S, DGL_REPEAT);
		gl.TexParameter(DGL_WRAP_T, DGL_REPEAT);
		gl.TexParameter(DGL_MIN_FILTER, DGL_NEAREST);
		gl.TexParameter(DGL_MAG_FILTER, DGL_LINEAR);
	}
}

//===========================================================================
// M_UnloadData
//===========================================================================
void M_UnloadData(void)
{
	if(Get(DD_NOVIDEO)) return;
	if(menuFogTexture) gl.DeleteTextures(1, &menuFogTexture);
	menuFogTexture = 0;
}

//
// M_Init
//
void M_Init (void)
{
	MenuItem_t *item;
	int i, w, maxw;

	// Init some strings.
	for(i = 0; i < 5; i++) strcpy(gammamsg[i], GET_TXT(TXT_GAMMALVL0+i));
	// Quit messages.	
	endmsg[0] = GET_TXT(TXT_QUITMSG);
	for(i = 1; i <= NUM_QUITMESSAGES; i++)
		endmsg[i] = GET_TXT(TXT_QUITMESSAGE1 + i-1);	
	// Episode names.
	for(i = 0, maxw = 0; i < 4; i++)
	{
		EpisodeItems[i].text = GET_TXT(TXT_EPISODE1 + i);
		w = M_StringWidth(EpisodeItems[i].text, hu_font_b);
		if(w > maxw) maxw = w;
	}
	// Center the episodes menu appropriately.
	EpiDef.x = 160 - maxw/2 + 12;
	// "Choose Episode"
	episodemsg = GET_TXT(TXT_ASK_EPISODE);

	M_LoadData();

    currentMenu = &MainDef;
    menuactive = 0;
    itemOn = currentMenu->lastOn;
    whichSkull = 0;
    skullAnimCounter = 10;
    screenSize = screenblocks - 3;
    messageToPrint = 0;
    messageString = NULL;
    messageLastMenuActive = menuactive;
    quickSaveSlot = -1;
	
    // Here we could catch other version dependencies,
    //  like HELP1/2, and four episodes.
	
	
    switch ( gamemode )
    {
	case commercial:
		// This is used because DOOM 2 had only one HELP
        //  page. I use CREDIT as second page now, but
		//  kept this hack for educational purposes.
		item = &MainItems[readthis];
		item->func = M_QuitDOOM;
		//item->lumpname = "M_QUITG";
		item->text = "QUIT GAME";
		M_SetNumItems(&MainDef, 6);
		MainDef.y = 64 + 8;
		NewDef.prevMenu = MENU_MAIN; //&MainDef;
		ReadDef1.drawFunc = M_DrawReadThis1;
		ReadDef1.x = 330;
		ReadDef1.y = 165;
		ReadItems1[0].func = M_FinishReadThis;
		break;
	case shareware:
		// Episode 2 and 3 are handled,
		//  branching to an ad screen.
	case registered:
		// We need to remove the fourth episode.
		M_SetNumItems(&EpiDef, 3);
		item = &MainItems[readthis];
		item->func = M_ReadThis;
		item->text = "READ THIS!";
		//item->lumpname = "M_RDTHIS";
		M_SetNumItems(&MainDef, 7);
		MainDef.y = 64;
		break;
	case retail:
		// We are fine.
		M_SetNumItems(&EpiDef, 4);
	default:
		break;
    }
    
}


int CCmdMenuAction(int argc, char **argv)
{
    // F-Keys
	//    if (!menuactive)
	
	/*	switch(ch)
	{
	case DDKEY_MINUS:         // Screen size down
		  if (automapactive || chat_on)
		  return false;
		  M_SizeDisplay(0);
		  S_LocalSound(NULL,sfx_stnmov);
		  return true;
		  
			case DDKEY_EQUALS:        // Screen size up
			if (automapactive || chat_on)
			return false;
			M_SizeDisplay(1);
			S_LocalSound(NULL,sfx_stnmov);
			return true;
	*/		  
	//	  case DDKEY_F1:            // Help key
	
	if(!stricmp(argv[0], "HelpScreen")) // F1
	{
		M_StartControlPanel ();
		
		if ( gamemode == retail )
			currentMenu = &ReadDef2;
		else
			currentMenu = &ReadDef1;
		
		itemOn = 0;
		S_LocalSound(sfx_swtchn, NULL);
	}
	else if(!stricmp(argv[0], "SaveGame")) // F2
	{
		M_StartControlPanel();
		S_LocalSound(sfx_swtchn, NULL);
		M_SaveGame(0);
	}
	else if(!stricmp(argv[0], "LoadGame")) // F3
	{
		M_StartControlPanel();
		S_LocalSound(sfx_swtchn, NULL);
		M_LoadGame(0);
	}
	else if(!stricmp(argv[0], "SoundMenu")) // F4
	{
		M_StartControlPanel ();
		currentMenu = &Options2Def;
		itemOn = 0; // sfx_vol
		S_LocalSound(sfx_swtchn, NULL);
	}
	/*		  
	case DDKEY_F5:            // Detail toggle
		  M_ChangeDetail(0);
		  S_LocalSound(sfx_swtchn, NULL);
		  return true;*/
	else if(!stricmp(argv[0], "QuickSave")) // F6
	{
		S_LocalSound(sfx_swtchn, NULL);
		M_QuickSave();
	}
	else if(!stricmp(argv[0], "EndGame")) // F7
	{
		S_LocalSound(sfx_swtchn, NULL);
		M_EndGame(0);
	}
	else if(!stricmp(argv[0], "ToggleMsgs")) // F8
	{
		M_ChangeMessages(0);
		S_LocalSound(sfx_swtchn, NULL);
	}
	else if(!stricmp(argv[0], "QuickLoad")) // F9
	{
		S_LocalSound(sfx_swtchn, NULL);
		M_QuickLoad();
	}
	else if(!stricmp(argv[0], "quit")) // F10
	{
		if(IS_DEDICATED) 
			Con_Execute("quit!", true);
		else
		{
			S_LocalSound(sfx_swtchn, NULL);
			M_QuitDOOM(0);
		}
	}
	else if(!stricmp(argv[0], "ToggleGamma")) // F11
	{
		char buf[50];
		usegamma++;
		if (usegamma > 4)
			usegamma = 0;
		P_SetMessage(players + consoleplayer, gammamsg[usegamma]);
		//I_SetPalette (W_CacheLumpName ("PLAYPAL",PU_CACHE));
		sprintf(buf, "setgamma %i", usegamma);
		Con_Execute(buf, false);
	}
	return true;
}