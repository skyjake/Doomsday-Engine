
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
	Zip_Shutdown();

	// Kill the message window if it happens to be open.
	SW_Shutdown();

	// Enables Alt-Tab, Alt-Esc, Ctrl-Alt-Del.
	SystemParametersInfo(SPI_SETSCREENSAVERRUNNING, FALSE, 0, 0);

	// Stop all demo recording.
	for(i = 0; i < MAXPLAYERS; i++) Demo_StopRecording(i);

	Sv_Shutdown();
	R_Shutdown();
	Sys_ConShutdown();
	Def_Destroy();
	F_ShutdownDirec();
	FH_Clear();
	ArgShutdown();
	free(mainzone);
	FreeLibrary(hInstGame);
	DD_ShutdownDGL();
	for(i = 0; hInstPlug[i]; i++) FreeLibrary(hInstPlug[i]);
	hInstGame = NULL;
	memset(hInstPlug, 0, sizeof(hInstPlug));
}
