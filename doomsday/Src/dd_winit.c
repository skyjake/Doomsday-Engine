
//**************************************************************************
//**
//** DD_WINIT.C
//**
//** Win32 init.
//** Create windows, load DLLs, setup APIs.
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#define WIN32_LEAN_AND_MEAN

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <io.h>
#include <windows.h>
#include "resource.h"

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_play.h"
#include "de_refresh.h"
#include "de_network.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

#define MAX_PLUGS	10

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

LRESULT CALLBACK MainWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

HWND hWndMain;			// The window handle to the main window.
HINSTANCE hInstApp;		// Instance handle to the application.
HINSTANCE hInstGame;	// Instance handle to the game DLL.
HINSTANCE hInstPlug[MAX_PLUGS];	// Instances to plugin DLLs.

// The game imports and exports.
game_import_t	gi;		
game_export_t	gx;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

int CheckArg(char *tag, char **value)
{
	int i = ArgCheck(tag);
	char *next = ArgNext();

	if(!i) return 0;
	if(value && next) *value = next;
	return 1;
}

//===========================================================================
// ErrorBox
//===========================================================================
void ErrorBox(boolean error, char *format, ...)
{
	char	buff[200];
	va_list args;

	va_start(args, format);
	vsprintf(buff, format, args);
	va_end(args);
	MessageBox(NULL, buff, "Doomsday "DOOMSDAY_VERSION_TEXT, 
		MB_OK | (error? MB_ICONERROR : MB_ICONWARNING));
}

BOOL InitApplication(HINSTANCE hInst)
{
	WNDCLASS wc;

	// We need to register a window class for our window.
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = MainWndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInst;
	wc.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_DOOMSDAY));
	wc.hCursor = NULL; //LoadCursor(hInst, IDC_ARROW);
	wc.hbrBackground = (HBRUSH) (COLOR_ACTIVEBORDER+1);
	wc.lpszMenuName = NULL;
	wc.lpszClassName = "DoomsdayMainWClass";
	return RegisterClass(&wc);
}

BOOL InitInstance(HINSTANCE hInst, int cmdShow)
{
	HDC hdc;
	char buf[256];

	// Include game ID in the title.
	sprintf(buf, "Doomsday "DOOMSDAY_VERSION_TEXT" : %s", 
		gx.Get(DD_GAME_ID));

	// Create the main window. It's shown right before GL init.
	hWndMain = CreateWindow("DoomsdayMainWClass", buf,
		/*WS_VISIBLE | */ WS_CAPTION | WS_CLIPCHILDREN | WS_CLIPSIBLINGS 
		| WS_MINIMIZEBOX,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		NULL,
		NULL,
		hInst,
		NULL);
	if(!hWndMain) return FALSE;

	/*ShowWindow(hWndMain, cmdShow);
	UpdateWindow(hWndMain);*/

	// Set the font.
	hdc = GetDC(hWndMain);
	SetMapMode(hdc, MM_TEXT);
	SelectObject(hdc, GetStockObject(SYSTEM_FIXED_FONT));

	// Tell DGL of our main window.
	gl.SetInteger(DGL_WINDOW_HANDLE, (int) hWndMain);
	return TRUE;
}

void SetGameImports(game_import_t *imp)
{
	memset(imp, 0, sizeof(*imp));
	imp->apiSize = sizeof(*imp);
	imp->version = DOOMSDAY_VERSION;

/*	imp->SetupLevel = DD_SetupLevel;
	imp->PrecacheLevel = R_PrecacheLevel;

	imp->Quit = I_Quit;
	imp->AddStartupWAD = DD_AddWADFile;
	imp->AddIWAD = DD_AddIWAD;
	imp->SetConfigFile = DD_SetConfigFile;
	imp->SetDefsFile = DD_SetDefsFile;
	imp->DefineActions = DD_DefineActions;
	imp->Get = DD_GetInteger;
	imp->Set = DD_SetInteger;
	imp->GetDef = DD_GetDef;

	imp->Message = ST_Message;
	imp->Error = I_Error;

	imp->conprintf = CON_Printf;
	imp->flconprintf = CON_FPrintf;
	imp->SetConsoleFont = CON_SetFont;
	imp->AddCommand = CON_AddCommand;
	imp->AddVariable = CON_AddVariable;
	imp->OpenConsole = CON_Open;
	imp->GetCVar = CvarGet;
	imp->Execute = CON_Execute;

	imp->Z_Malloc = Z_Malloc;
	imp->Z_Free = Z_Free;
	imp->Z_FreeTags = Z_FreeTags;
	imp->Z_ChangeTag = Z_ChangeTag2;
	imp->Z_CheckHeap = Z_CheckHeap;

	// Networking.
	imp->GetPlayer = DD_GetPlayer;
	imp->GetPlayerName = I_NetGetPlayerName;
	imp->GetPlayerID = I_NetGetPlayerID;
	imp->GetTicCmd = Net_GetTicCmd;
	imp->SendPacket = DD_SendPacket;
	//imp->Sv_TextureChanges = Sv_TextureChanges;
	imp->Sv_PlaneSound = Sv_PlaneSound;
	//imp->Sv_SectorReport = Sv_SectorReport;
	//imp->Sv_LightReport = Sv_LightReport;

	imp->SetState = P_SetState;

	// Map.
	imp->LoadBlockMap = P_LoadBlockMap;
	imp->LoadReject = P_LoadReject;
	imp->ApproxDistance = P_AproxDistance;
	imp->PointOnLineSide = P_PointOnLineSide;
	imp->BoxOnLineSide = P_BoxOnLineSide;
	imp->MakeDivline = P_MakeDivline;
	imp->PointOnDivlineSide = P_PointOnDivlineSide;
	imp->InterceptVector = P_InterceptVector;
	imp->LineOpening = P_LineOpening;
	imp->LinkThing = P_LinkThing;
	imp->UnlinkThing = P_UnlinkThing;
	imp->BlockLinesIterator = P_BlockLinesIterator;
	imp->BlockThingsIterator = P_BlockThingsIterator;
	imp->ThingLinesIterator = P_ThingLinesIterator;
	imp->ThingSectorsIterator = P_ThingSectorsIterator;
	imp->LineThingsIterator = P_LineThingsIterator;
	imp->SectorTouchingThingsIterator = P_SectorTouchingThingsIterator;
	imp->GetBlockRootIdx = P_GetBlockRootIdx;
	imp->PathTraverse = P_PathTraverse;
	imp->CheckSight = P_CheckSight;
	imp->MovePolyobj = PO_MovePolyobj;
	imp->RotatePolyobj = PO_RotatePolyobj;
	imp->LinkPolyobj = PO_LinkPolyobj;
	imp->UnLinkPolyobj = PO_UnLinkPolyobj;
	imp->SetPolyobjFunc = PO_SetCallback;

	imp->GetSpriteInfo = R_GetSpriteInfo;
	imp->SetBorderGfx = R_SetBorderGfx;
	imp->RenderPlayerView = R_RenderPlayerView;
	imp->ViewWindow = R_SetViewSize;
	imp->R_FlatNumForName = R_FlatNumForName;
	imp->R_CheckTextureNumForName = R_CheckTextureNumForName;
	imp->R_TextureNumForName = R_TextureNumForName;
	imp->R_TextureNameForNum = R_TextureNameForNum;
	imp->R_PointToAngle2 = R_PointToAngle2;
	imp->R_PointInSubsector = R_PointInSubsector;

	imp->W_CheckNumForName = W_CheckNumForName;
	imp->W_GetNumForName = W_GetNumForName;
	imp->W_CacheLumpName = W_CacheLumpName;
	imp->W_CacheLumpNum = W_CacheLumpNum;
	imp->W_LumpLength = W_LumpLength;
	imp->W_ReadLump = W_ReadLump;
	imp->W_ChangeCacheTag = W_ChangeCacheTag;

	// Graphics.
	imp->GetDGL = DD_GetDGL;
	imp->Update = DD_GameUpdate;
	imp->ChangeResolution = I_ChangeResolution;
	imp->SetFlatTranslation = R_SetFlatTranslation;
	imp->SetTextureTranslation = R_SetTextureTranslation;
	imp->GL_UseFog = GL_UseWhiteFog;
	imp->ScreenShot = M_ScreenShot;
	imp->GL_ResetData = GL_ResetData;
	imp->GL_ResetTextures = GL_TexReset;
	imp->GL_ClearTextureMem = GL_ClearTextureMemory;
	imp->GL_TexFilterMode = GL_TextureFilterMode;
	imp->GL_SetColorAndAlpha = GL_SetColorAndAlpha;
	imp->GL_SetColor = GL_SetColor;
	imp->GL_SetNoTexture = GL_SetNoTexture;
	imp->GL_SetFlat = GL_SetFlat;
	imp->GL_SetPatch = GL_SetPatch;
	imp->GL_SetRawImage = GL_SetRawImage;
	imp->GL_DrawPatch = GL_DrawPatch;
	imp->GL_DrawFuzzPatch = GL_DrawFuzzPatch;
	imp->GL_DrawAltFuzzPatch = GL_DrawAltFuzzPatch;
	imp->GL_DrawShadowedPatch = GL_DrawShadowedPatch;
	imp->GL_DrawPatchLitAlpha = GL_DrawPatchLitAlpha;
	imp->GL_DrawRawScreen = GL_DrawRawScreen;
	imp->GL_DrawPatch = GL_DrawPatch;
	imp->GL_DrawRect = GL_DrawRect;
	imp->GL_DrawRectTiled = GL_DrawRectTiled;
	imp->GL_DrawCutRectTiled = GL_DrawCutRectTiled;
	imp->GL_DrawPSprite = GL_DrawPSprite;
	imp->GL_SetFilter = GL_SetFilter;
	imp->GL_DrawPatchCS = GL_DrawPatch_CS;
	imp->SkyParams = R_SkyParams;

	imp->GetTime = I_GetTime;
	imp->GetRealTime = I_GetRealTime;
	imp->FrameRate = I_GetFrameRate;
	imp->ClearKeyRepeaters = I_ClearKeyRepeaters;
	imp->EventBuilder = B_EventConverter;
	imp->BindingsForCommand = B_BindingsForCommand;

	imp->InitThinkers = P_InitThinkers;
	imp->AddThinker = P_AddThinker;
	imp->RemoveThinker = P_RemoveThinker;
	imp->RunThinkers = RunThinkers;

	// Sound and music.
#ifdef A3D_SUPPORT
	if(use_jtSound)
	{
#endif
		// DirectSound(3D) + EAX, if available.
		imp->PlaySound = I_Play2DSound;
		imp->UpdateSound = I_Update2DSound;
		imp->StopSound = I_StopSound;
		imp->SoundIsPlaying = I_SoundIsPlaying;
		imp->Play3DSound = I_Play3DSound;
		imp->Update3DSound = I_Update3DSound;
		imp->UpdateListener = I_UpdateListener;
#ifdef A3D_SUPPORT
	}
	else
	{
		// Use A3D 3.0 sound routines.
		imp->PlaySound = I3_PlaySound2D;
		imp->UpdateSound = I3_UpdateSound2D;
		imp->Play3DSound = I3_PlaySound3D;
		imp->Update3DSound = I3_UpdateSound3D;
		imp->UpdateListener = I3_UpdateListener;
		imp->StopSound = I3_StopSound;
		imp->SoundIsPlaying = I3_IsSoundPlaying;
	}
#endif
	imp->SetSfxVolume = I_SetSfxVolume;
	
	imp->PlaySong = I_PlaySong;
	imp->SongIsPlaying = I_QrySongPlaying;
	imp->StopSong = I_StopSong;
	imp->PauseSong = I_PauseSong;
	imp->ResumeSong = I_ResumeSong;
	imp->SetMIDIVolume = I_SetMusicVolume;
	imp->SetMusicDevice = I_SetMusicDevice;
	imp->CD = I_CDControl;

	// Misc.
	imp->Argc = Argc;
	imp->Argv = Argv;
	imp->NextArg = NextArg;
	imp->ArgvPtr = ArgvPtr;
	imp->AddParm = AddParm;
	imp->ParmExists = ParmExists;
	imp->CheckParm = CheckParm;
	imp->CheckParmArgs = CheckParmArgs;
	imp->IsParm = IsParm;
	imp->ReadFile = M_ReadFile;
	imp->ReadFileClib = M_ReadFileCLib;
	imp->WriteFile = M_WriteFile;
	imp->ExtractFileBase = M_ExtractFileBase;
	imp->ClearBox = M_ClearBox;
	imp->AddToBox = M_AddToBox;
	imp->CheckPath = M_CheckPath;
	imp->FileExists = M_FileExists;*/

	// Data.
	imp->mobjinfo = &mobjinfo;
	imp->states = &states;
	imp->sprnames = &sprnames;
//	imp->sounds = &sounds;
//	imp->music = &music;
	imp->text = &texts;

	imp->validcount = &validcount;
	imp->topslope = &topslope;
	imp->bottomslope = &bottomslope;
	imp->thinkercap = &thinkercap;

	imp->numvertexes = &numvertexes;
	imp->numsegs = &numsegs;
	imp->numsectors = &numsectors;
	imp->numsubsectors = &numsubsectors;
	imp->numnodes = &numnodes;
	imp->numlines = &numlines;
	imp->numsides = &numsides;

	imp->vertexes = &vertexes;
	imp->segs = &segs;
	imp->sectors = &sectors;
	imp->subsectors = &subsectors;
	imp->nodes = &nodes;
	imp->lines = &lines;
	imp->sides = &sides;

	imp->blockmaplump = &blockmaplump;
	imp->blockmap = &blockmap;
	imp->bmapwidth = &bmapwidth;
	imp->bmapheight = &bmapheight;
	imp->bmaporgx = &bmaporgx;
	imp->bmaporgy = &bmaporgy;
	imp->rejectmatrix = &rejectmatrix;
	imp->polyblockmap = &polyblockmap;
	imp->polyobjs = &polyobjs;
	imp->numpolyobjs = &po_NumPolyobjs;
}

#if 0
void SetDGLImports(gl_import_t *imp)
{
	/*memset(imp, 0, sizeof(*imp));

	imp->apiSize = sizeof(*imp);
	imp->Message = Con_Message;
	imp->Error = Con_Error;
	imp->AddParm = AddParm;
	imp->CheckParm = CheckParm;
	imp->CheckParmArgs = CheckParmArgs;
	imp->IsParm = IsParm;
	imp->Argc = Argc;
	imp->Argv = Argv;
	imp->NextArg = NextArg;*/
}
#endif

BOOL InitGameDLL()
{
	char	*dllName = NULL;	// Pointer to the filename of the game DLL.
	GETGAMEAPI GetGameAPI = NULL;
	game_export_t *gameExPtr;

	// First we need to locate the dll name among the command line arguments.
	CheckArg("-game", &dllName);

	// Was a game dll specified?
	if(!dllName) 
	{
		ErrorBox(true, "InitGameDLL: No game DLL was specified.\n");
		return FALSE;
	}

	// Now, load the DLL and get the API/exports.
	hInstGame = LoadLibrary(dllName);
	if(!hInstGame)
	{
		ErrorBox(true, "InitGameDLL: Loading of %s failed (error %d).\n", 
			dllName, GetLastError());
		return FALSE;
	}

	// Get the function.
	GetGameAPI = (GETGAMEAPI) GetProcAddress(hInstGame, "GetGameAPI");
	if(!GetGameAPI)
	{
		ErrorBox(true, "InitGameDLL: Failed to get proc address of GetGameAPI (error %d).\n",
			GetLastError());
		return FALSE;
	}		

	// Put the imported stuff into the imports.
	SetGameImports(&gi);

	// Do the API transfer.
	memset(&gx, 0, sizeof(gx));
	gameExPtr = GetGameAPI(&gi);
	memcpy(&gx, gameExPtr, MIN_OF(sizeof(gx), gameExPtr->apiSize));

	// Everything seems to be working...
	return TRUE;
}

//===========================================================================
// LoadPlugin
//	Loads the given plugin. Returns TRUE iff the plugin was loaded 
//	succesfully.
//===========================================================================
int LoadPlugin(const char *filename)
{
	int i;

	// Find the first empty plugin instance.
	for(i = 0; hInstPlug[i]; i++);

	// Try to load it.
	if(!(hInstPlug[i] = LoadLibrary(filename))) return FALSE; // Failed!

	// That was all; the plugin registered itself when it was loaded.	
	return TRUE;
}

//===========================================================================
// InitPlugins
//	Loads all the plugins from the startup directory. 
//===========================================================================
int InitPlugins(void)
{
	long hFile;
	struct _finddata_t fd;
	char plugfn[256];

	sprintf(plugfn, "%sdp*.dll", ddBinDir.path);
	if((hFile = _findfirst(plugfn, &fd)) == -1L) return TRUE;
	do LoadPlugin(fd.name); while(!_findnext(hFile, &fd));
	return TRUE;
}

//===========================================================================
// WinMain
//===========================================================================
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, 
		LPSTR lpCmdLine, int nCmdShow)
{
	char path[256];

	// Where are we?
	GetModuleFileName(hInstance, path, 255);
	Dir_FileDir(path, &ddBinDir);

	// Make the instance handle global knowledge.
	hInstApp = hInstance;

	// Prepare the command line arguments.
	ArgInit(GetCommandLine());

	// Register some abbreviations for command line options.
	ArgAbbreviate("-game", "-g");
	ArgAbbreviate("-gl", "-r"); // As in (R)enderer...
	ArgAbbreviate("-defs", "-d");
	ArgAbbreviate("-width", "-w");
	ArgAbbreviate("-height", "-h");
	ArgAbbreviate("-winsize", "-wh");
	ArgAbbreviate("-bpp", "-b");
	ArgAbbreviate("-window", "-wnd");
	ArgAbbreviate("-nocenter", "-noc");
	ArgAbbreviate("-paltex", "-ptx");
	ArgAbbreviate("-file", "-f");
	ArgAbbreviate("-maxzone", "-mem");
	ArgAbbreviate("-config", "-cfg");
	ArgAbbreviate("-parse", "-p");
	ArgAbbreviate("-cparse", "-cp");
	ArgAbbreviate("-command", "-cmd");
	ArgAbbreviate("-fontdir", "-fd");
	ArgAbbreviate("-modeldir", "-md");
	ArgAbbreviate("-basedir", "-bd");
	ArgAbbreviate("-stdbasedir", "-sbd");
	ArgAbbreviate("-userdir", "-ud");
	ArgAbbreviate("-texdir", "-td");
	ArgAbbreviate("-texdir2", "-td2");
	ArgAbbreviate("-anifilter", "-ani");
	ArgAbbreviate("-verbose", "-v");

	// Load the rendering DLL.
	if(!DD_InitDGL()) return FALSE;

	// Load the game DLL.
	if(!InitGameDLL()) return FALSE;

	// Load all plugins that are found.
	if(!InitPlugins()) return FALSE; // Fatal error occured?

	if(!InitApplication(hInstance))
	{
		ErrorBox(true, "Couldn't initialize application.");
		return FALSE;
	}
	if(!InitInstance(hInstance, nCmdShow)) 
	{
		ErrorBox(true, "Couldn't initialize instance.");
		return FALSE;
	}

	// Fire up the engine. The game loop will also act as the message pump.
	DD_Main();
    return 0;
}

//===========================================================================
// MainWndProc
//	All messages go to the default window message processor.
//===========================================================================
LRESULT CALLBACK MainWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch(msg)
	{
	case WM_CLOSE:
		// FIXME: Allow closing via the close button.
		break;

	default:
		return DefWindowProc(hWnd, msg, wParam, lParam);
	}
	return 0;
}
 
//===========================================================================
// DD_Shutdown
//	Shuts down the engine.
//===========================================================================
void DD_Shutdown()
{
	extern memzone_t *mainzone;
	int i;

	DD_ShutdownHelp();

	// Kill the message window if it happens to be open.
	SW_Shutdown();

	// Enables Alt-Tab, Alt-Esc, Ctrl-Alt-Del.
	SystemParametersInfo(SPI_SETSCREENSAVERRUNNING, FALSE, 0, 0);

	// Stop all demo recording.
	for(i = 0; i < MAXPLAYERS; i++) Demo_StopRecording(i);

	R_Shutdown();
	Sys_ConShutdown();
	Def_Destroy();
	F_ShutdownDirec();
	ArgShutdown();
	free(mainzone);
	FreeLibrary(hInstGame);
	DD_ShutdownDGL();
	for(i = 0; hInstPlug[i]; i++) FreeLibrary(hInstPlug[i]);
	hInstGame = NULL;
	memset(hInstPlug, 0, sizeof(hInstPlug));
}
