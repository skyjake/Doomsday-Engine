/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006-2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * dd_main.c: Engine Core
 */

// HEADER FILES ------------------------------------------------------------

#ifdef WIN32
#  define _WIN32_DCOM
#  include <objbase.h>
#endif

#include "de_platform.h"

#ifdef WIN32
#  include <direct.h>
#endif

#ifdef UNIX
#  include <ctype.h>
#  include <SDL.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>

#include "de_base.h"
#include "de_system.h"
#include "de_console.h"
#include "de_network.h"
#include "de_play.h"
#include "de_refresh.h"
#include "de_render.h"
#include "de_graphics.h"
#include "de_audio.h"
#include "de_misc.h"
#include "de_dam.h"

#include "dd_pinit.h"

// MACROS ------------------------------------------------------------------

#define MAXWADFILES         (4096)

// TYPES -------------------------------------------------------------------

typedef struct ddvalue_s {
    int*            readPtr;
    int*            writePtr;
} ddvalue_t;

typedef struct autoload_s {
    boolean         loadFiles; // Should files be loaded right away.
    int             count; // Number of files loaded successfully.
} autoload_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void            G_CheckDemoStatus(void);
void            F_Drawer(void);
void            S_InitScript(void);
void            Net_Drawer(void);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static int      DD_StartupWorker(void* parm);
static int      DD_StartupWorker2(void* parm);
static void     HandleArgs(int state);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

#ifdef WIN32
extern HINSTANCE hInstDGL;
#endif

extern int renderTextures;
extern char skyFlatName[9];
extern int gotFrame;
extern int monochrome;
extern int gameDataFormat;
extern int gameDrawHUD;
extern int symbolicEchoMode;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

directory_t ddRuntimeDir, ddBinDir;
int verbose = 0; // For debug messages (-verbose).
boolean cmdfrag; // true if a CMD_FRAG packet should be sent out
int isDedicated = false;
int maxZone = 0x2000000; // Default zone heap. (32meg)
boolean autoStart;
FILE* outFile; // Output file for console messages.

char* iwadList[64];
char* defaultWads = ""; // List of wad names, whitespace seperating(in .cfg).

filename_t configFileName;
filename_t bindingsConfigFileName;
filename_t defsFileName, topDefsFileName;
filename_t ddBasePath = "";     // Doomsday root directory is at...?

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static char *wadFiles[MAXWADFILES];

// CODE --------------------------------------------------------------------

/**
 * Convert propertyType enum constant into a string for error/debug messages.
 */
const char* value_Str(int val)
{
    static char         valStr[40];
    struct val_s {
        int                 val;
        const char*         str;
    } valuetypes[] =
    {
        { DDVT_BOOL, "DDVT_BOOL" },
        { DDVT_BYTE, "DDVT_BYTE" },
        { DDVT_SHORT, "DDVT_SHORT" },
        { DDVT_INT, "DDVT_INT" },
        { DDVT_UINT, "DDVT_UINT" },
        { DDVT_FIXED, "DDVT_FIXED" },
        { DDVT_ANGLE, "DDVT_ANGLE" },
        { DDVT_FLOAT, "DDVT_FLOAT" },
        { DDVT_LONG, "DDVT_LONG" },
        { DDVT_ULONG, "DDVT_ULONG" },
        { DDVT_PTR, "DDVT_PTR" },
        { DDVT_BLENDMODE, "DDVT_BLENDMODE" },
        { 0, NULL }
    };
    uint                i;

    for(i = 0; valuetypes[i].str; ++i)
        if(valuetypes[i].val == val)
            return valuetypes[i].str;

    sprintf(valStr, "(unnamed %i)", val);
    return valStr;
}

/**
 * Adds the given IWAD to the list of default IWADs.
 */
void DD_AddIWAD(const char* path)
{
    int                 i = 0;
    filename_t          buf;

    while(iwadList[i])
        i++;

    M_TranslatePath(buf, path, FILENAME_T_MAXLEN);
    iwadList[i] = M_Calloc(strlen(buf) + 1);   // This mem is not freed?
    strcpy(iwadList[i], buf);
}

#define ATWSEPS ",; \t"
static void AddToWadList(char* list)
{
    size_t          len = strlen(list);
    char           *buffer = M_Malloc(len + 1), *token;

    strcpy(buffer, list);
    token = strtok(buffer, ATWSEPS);
    while(token)
    {
        DD_AddStartupWAD(token /*, false */ );
        token = strtok(NULL, ATWSEPS);
    }
    M_Free(buffer);
}

/**
 * (f_forall_func_t)
 */
static int autoDataAdder(const char *fileName, filetype_t type, void* ptr)
{
    autoload_t*         data = ptr;

    // Skip directories.
    if(type == FT_DIRECTORY)
        return true;

    if(data->loadFiles)
    {
        if(W_AddFile(fileName, false))
            ++data->count;
    }
    else
    {
        DD_AddStartupWAD(fileName);
    }

    // Continue searching.
    return true;
}

/**
 * Files with the extensions wad, lmp, pk3, zip and deh in the automatical data
 * directory are added to the wadFiles list.
 *
 * @return              Number of new files that were loaded.
 */
int DD_AddAutoData(boolean loadFiles)
{
    autoload_t          data;
    const char*         extensions[] = {
        "wad", "lmp", "pk3", "zip", "deh",
#ifdef UNIX
        "WAD", "LMP", "PK3", "ZIP", "DEH", // upper case alternatives
#endif
        NULL
    };
    filename_t          pattern;
    uint                i;

    data.loadFiles = loadFiles;
    data.count = 0;

    for(i = 0; extensions[i]; ++i)
    {
        dd_snprintf(pattern, FILENAME_T_MAXLEN, "%sauto\\*.%s",
                    R_GetDataPath(), extensions[i]);

        Dir_FixSlashes(pattern, FILENAME_T_MAXLEN);
        F_ForAll(pattern, &data, autoDataAdder);
    }

    return data.count;
}

void DD_SetConfigFile(const char* file)
{
    strncpy(configFileName, file, FILENAME_T_MAXLEN);
    configFileName[FILENAME_T_LASTINDEX] = '\0';

    Dir_FixSlashes(configFileName, FILENAME_T_MAXLEN);

    strncpy(bindingsConfigFileName, configFileName, FILENAME_T_MAXLEN);
    bindingsConfigFileName[FILENAME_T_LASTINDEX] = '\0';

    strncpy(bindingsConfigFileName + strlen(bindingsConfigFileName) - 4,
            "-bindings.cfg", FILENAME_T_MAXLEN - strlen(bindingsConfigFileName) - 4);
    bindingsConfigFileName[FILENAME_T_LASTINDEX] = '\0';
}

/**
 * Set the primary DED file, which is included immediately after
 * Doomsday.ded.
 */
void DD_SetDefsFile(const char* file)
{
    dd_snprintf(topDefsFileName, FILENAME_T_MAXLEN, "%sdefs\\%s",
                ddBasePath, file);
    Dir_FixSlashes(topDefsFileName, FILENAME_T_MAXLEN);
}

/**
 * Define Auto mappings for the runtime directory.
 */
void DD_DefineBuiltinVDM(void)
{
    filename_t          dest;

    // Data files.
    dd_snprintf(dest, FILENAME_T_MAXLEN, "%sauto", R_GetDataPath());
    F_AddMapping("auto", dest);

    // Definition files.
    Def_GetAutoPath(dest, FILENAME_T_MAXLEN);
    F_AddMapping("auto", dest);
}

/**
 * Looks for new files to autoload. Runs until there are no more files to
 * autoload.
 */
void DD_AutoLoad(void)
{
    int                 p;

    /**
     * Load files from the Auto directory.  (If already loaded, won't
     * be loaded again.)  This is done again because virtual files may
     * now exist in the Auto directory.  Repeated until no new files
     * found.
     */
    while((p = DD_AddAutoData(true)) > 0)
    {
        VERBOSE(Con_Message("Autoload round completed with %i new files.\n",
                            p));
    }
}

/**
 * Engine and game initialization. When complete, starts the game loop.
 */
int DD_Main(void)
{
    int             winWidth, winHeight, winBPP, winX, winY;
    uint            winFlags = DDWF_VISIBLE | DDWF_CENTER;
    boolean         noCenter = false;
    int             exitCode;

    // By default, use the resolution defined in (default).cfg.
    winX = 0;
    winY = 0;
    winWidth = defResX;
    winHeight = defResY;
    winBPP = defBPP;
    winFlags |= (defFullscreen? DDWF_FULLSCREEN : 0);

    // Check for command line options modifying the defaults.
    if(ArgCheckWith("-width", 1))
        winWidth = atoi(ArgNext());
    if(ArgCheckWith("-height", 1))
        winHeight = atoi(ArgNext());
    if(ArgCheckWith("-winsize", 2))
    {
        winWidth = atoi(ArgNext());
        winHeight = atoi(ArgNext());
    }
    if(ArgCheckWith("-bpp", 1))
        winBPP = atoi(ArgNext());
    // Ensure a valid value.
    if(winBPP != 16 && winBPP != 32)
        winBPP = 32;
    if(ArgCheck("-nocenter"))
        noCenter = true;
    if(ArgCheckWith("-xpos", 1))
    {
        winX = atoi(ArgNext());
        noCenter = true;
    }
    if(ArgCheckWith("-ypos", 1))
    {
        winY = atoi(ArgNext());
        noCenter = true;
    }
    if(noCenter)
        winFlags &= ~DDWF_CENTER;

    if(ArgExists("-nofullscreen") || ArgExists("-window"))
        winFlags &= ~DDWF_FULLSCREEN;

    if(!Sys_SetWindow(windowIDX, winX, winY, winWidth, winHeight, winBPP,
                      winFlags, 0))
        return -1;

    if(!isDedicated)
    {
        if(!GL_EarlyInit())
        {
            Sys_CriticalMessage("GL_EarlyInit() failed.");
            return -1;
        }
    }

    /**
     * \note This must be called from the main thread due to issues with
     * the devices we use via the WINAPI, MCI (cdaudio, mixer etc).
     */
    Con_Message("Sys_Init: Setting up machine state.\n");
    Sys_Init();

    // Enter busy mode until startup complete.
    Con_InitProgress(200);
    Con_Busy(BUSYF_NO_UPLOADS | BUSYF_STARTUP | BUSYF_PROGRESS_BAR
             | (verbose? BUSYF_CONSOLE_OUTPUT : 0), "Starting up...",
             DD_StartupWorker, NULL);

    // Engine initialization is complete. Now finish up with the GL.
    if(!isDedicated)
    {
        GL_Init();
        GL_InitRefresh();

        // \todo we could be loading these in busy mode.
        GL_LoadSystemTextures();
    }

    // Do deferred uploads.
    Con_Busy(BUSYF_PROGRESS_BAR | BUSYF_STARTUP | BUSYF_ACTIVITY
             | (verbose? BUSYF_CONSOLE_OUTPUT : 0), NULL, DD_StartupWorker2, NULL);

    // Client connection command.
    if(ArgCheckWith("-connect", 1))
        Con_Executef(CMDS_CMDLINE, false, "connect %s", ArgNext());

    // Server start command.
    // (shortcut for -command "net init tcpip; net server start").
    if(ArgExists("-server"))
    {
        if(!N_InitService(true))
            Con_Message("Can't start server: network init failed.\n");
        else
            Con_Executef(CMDS_CMDLINE, false, "net server start");
    }

    // Final preparations for using the console UI.
    Con_InitUI();

    // Start the game loop.
    exitCode = DD_GameLoop();

    // Time to shutdown.

    if(netGame)
    {   // Quit netGame if one is in progress.
        Con_Execute(CMDS_DDAY, isServer ? "net server close" : "net disconnect",
                    true, false);
    }

    Demo_StopPlayback();
    Con_SaveDefaults();
    Sys_Shutdown();
    B_Shutdown();

    return exitCode;
}

static int DD_StartupWorker(void *parm)
{
    int                 p = 0;
    float               starttime;

#ifdef WIN32
    // Initialize COM for this thread (needed for DirectInput).
    CoInitialize(NULL);
#endif

    F_InitMapping();

    // Initialize the key mappings.
    DD_InitInput();

    Con_SetProgress(10);

    // Any startup hooks?
    Plug_DoHook(HOOK_STARTUP, 0, 0);

    Con_SetProgress(20);

    DD_AddStartupWAD("}data\\doomsday.pk3");
    R_SetDataPath("}data\\");

    // The name of the .cfg will invariably be overwritten by the Game.
    strncpy(configFileName, "doomsday.cfg", FILENAME_T_MAXLEN);
    dd_snprintf(defsFileName, FILENAME_T_MAXLEN, "%sdefs\\doomsday.ded",                    ddBasePath);

    // Was the change to userdir OK?
    if(!app.userDirOk)
        Con_Message("--(!)-- User directory not found " "(check -userdir).\n");

    bamsInit();                 // Binary angle calculations.

    // Initialize the zip file database.
    Zip_Init();

    Def_Init();

    autoStart = false;

    HandleArgs(0);              // Everything but WADs.

    DAM_Init();

    if(gx.PreInit)
        gx.PreInit();

    Con_SetProgress(40);

    // We now accept no more custom properties.
    //DAM_LockCustomPropertys();

    // Automatically create an Auto mapping in the runtime directory.
    DD_DefineBuiltinVDM();

    // Initialize subsystems
    Net_Init(); // Network before anything else.

    // Now we can hide the mouse cursor for good.
    Sys_HideMouse();

    // Load defaults before initing other systems
    Con_Message("Parsing configuration files.\n");
    // Check for a custom config file.
    if(ArgCheckWith("-config", 1))
    {
        // This will override the default config file.
        strcpy(configFileName, ArgNext());
        Con_Message("Custom config file: %s\n", configFileName);
    }

    // This'll be the default config file.
    Con_ParseCommands(configFileName, true);

    // Parse additional files (that should be parsed BEFORE init).
    if(ArgCheckWith("-cparse", 1))
    {
        for(;;)
        {
            const char*         arg = ArgNext();

            if(!arg || arg[0] == '-')
                break;

            Con_Message("Parsing: %s\n", arg);
            Con_ParseCommands(arg, false);
        }
    }

    Con_SetProgress(50);

    if(defaultWads && defaultWads[0])
        AddToWadList(defaultWads);  // These must take precedence.
    HandleArgs(1);              // Only the WADs.

    Con_Message("W_Init: Init WADfiles.\n");
    starttime = Sys_GetSeconds();

    // Add real files from the Auto directory to the wadFiles list.
    DD_AddAutoData(false);

    Con_SetProgress(60);

    W_InitMultipleFiles(wadFiles);

    F_InitDirec();
    VERBOSE(Con_Message("W_Init: Done in %.2f seconds.\n",
                        Sys_GetSeconds() - starttime));

    Con_SetProgress(70);

    // Load help resources. Now virtual files are available as well.
    if(!isDedicated)
        DD_InitHelp();

    // Final autoload round.
    DD_AutoLoad();

    Con_SetProgress(80);

    // No more WADs will be loaded in startup mode after this point.
    W_EndStartup();

    // Execute the startup script (Startup.cfg).
    Con_ParseCommands("startup.cfg", false);

    // Now the game can identify the game mode.
    gx.UpdateState(DD_GAME_MODE);

    Con_SetProgress(90);

    // We can now initialize the resource locator (game mode identified).
    R_InitResourceLocator();

    Con_SetProgress(100);

    GL_EarlyInitTextureManager();

    // Get the material manager up and running.
    P_InitMaterialManager();
    R_InitTextures();
    R_InitFlats();
    R_PreInitSprites();

    Con_SetProgress(130);

    // Now that we've generated the auto-materials we can initialize
    // definitions.
    Def_Read();

#ifdef WIN32
    if(ArgCheck("-nowsk"))      // No Windows system keys?
    {
        // Disable Alt-Tab, Alt-Esc, Ctrl-Alt-Del.
        // A bit of a hack, I'm afraid...
        SystemParametersInfo(SPI_SETSCREENSAVERRUNNING, TRUE, 0, 0);
        Con_Message("Windows system keys disabled.\n");
    }
#endif

    if(ArgCheckWith("-dumplump", 1))
    {
        const char*         lumpName = ArgNext();
        lumpnum_t           lump;

        if((lump = W_CheckNumForName(lumpName)) != -1)
            W_DumpLump(lump, NULL);
    }

    if(ArgCheck("-dumpwaddir"))
        W_DumpLumpDir();

    Con_SetProgress(140);

    Con_Message("B_Init: Init bindings.\n");
    B_Init();
    Con_ParseCommands(bindingsConfigFileName, false);

    Con_SetProgress(150);

    Con_Message("R_Init: Init the refresh daemon.\n");
    R_Init();

    Con_SetProgress(165);

    Con_Message("Net_InitGame: Initializing game data.\n");
    Net_InitGame();
    Demo_Init();

    if(gx.PostInit)
        gx.PostInit();

    Con_SetProgress(175);

    // Defs have been read; we can now init the map format info.
    P_InitData();

    Con_SetProgress(190);

    // Try to load the autoexec file. This is done here to make sure
    // everything is initialized: the user can do here anything that
    // s/he'd be able to do in the game.
    Con_ParseCommands("autoexec.cfg", false);

    // Parse additional files.
    if(ArgCheckWith("-parse", 1))
    {
        for(;;)
        {
            const char*         arg = ArgNext();

            if(!arg || arg[0] == '-')
                break;

            Con_Message("Parsing: %s\n", arg);
            Con_ParseCommands(arg, false);
        }
    }

    // A console command on the command line?
    for(p = 1; p < Argc() - 1; p++)
    {
        if(stricmp(Argv(p), "-command") && stricmp(Argv(p), "-cmd"))
            continue;

        for(++p; p < Argc(); p++)
        {
            const char*         arg = Argv(p);

            if(arg[0] == '-')
            {
                p--;
                break;
            }
            Con_Execute(CMDS_CMDLINE, arg, false, false);
        }
    }

    // In dedicated mode the console must be opened, so all input events
    // will be handled by it.
    if(isDedicated)
        Con_Open(true);

    Con_SetProgress(199);

    Plug_DoHook(HOOK_INIT, 0, 0);   // Any initialization hooks?
    Con_UpdateKnownWords();         // For word completion (with Tab).

    Con_SetProgress(200);

#ifdef WIN32
    // This thread has finished using COM.
    CoUninitialize();
#endif

    Con_BusyWorkerEnd();
    return 0;
}

/**
 * This only exists so we have something to call while the deferred uploads of the
 * startup are processed.
 */
static int DD_StartupWorker2(void *parm)
{
    Con_SetProgress(200);
    Con_BusyWorkerEnd();
    return 0;
}

static void HandleArgs(int state)
{
    int             order, p;

    if(state == 0)
    {
        renderTextures = !ArgExists("-notex");
    }

    // Process all -iwad and -file options. -iwad options are processed
    // first so that the loading order remains correct.
    if(state)
    {
        for(order = 0; order < 2; order++)
        {
            for(p = 0; p < Argc(); p++)
            {
                if((order == 1 && !ArgRecognize("-file", Argv(p))) ||
                   (order == 0 && !ArgRecognize("-iwad", Argv(p))))
                    continue;

                while(++p != Argc() && !ArgIsOption(p))
                    DD_AddStartupWAD(Argv(p));

                p--;/* For ArgIsOption(p) necessary, for p==Argc() harmless */
            }
        }
    }
}

void DD_CheckTimeDemo(void)
{
    static boolean checked = false;

    if(!checked)
    {
        checked = true;
        if(ArgCheckWith("-timedemo", 1) || // Timedemo mode.
           ArgCheckWith("-playdemo", 1)) // Play-once mode.
        {
            char            buf[200];

            sprintf(buf, "playdemo %s", ArgNext());
            Con_Execute(CMDS_CMDLINE, buf, false, false);
        }
    }
}

/**
 * This is a 'public' WAD file addition routine. The caller can put a
 * greater-than character (>) in front of the name to prepend the base
 * path to the file name (providing it's a relative path).
 */
void DD_AddStartupWAD(const char* file)
{
    int                 i;
    char*               new;
    filename_t          temp;

    i = 0;
    while(wadFiles[i])
        i++;

    M_TranslatePath(temp, file, FILENAME_T_MAXLEN);
    new = M_Calloc(strlen(temp) + 1);  // This is never freed?
    strcat(new, temp);
    wadFiles[i] = new;
}

static int DD_UpdateWorker(void* parm)
{
    GL_InitRefresh();
    R_Update();

    Con_BusyWorkerEnd();
    return 0;
}

void DD_UpdateEngineState(void)
{
    boolean             hadFog;

    // Update refresh.
    Con_Message("Updating state...\n");

    // Update the dir/WAD translations.
    F_InitDirec();

    gx.UpdateState(DD_PRE);

    // Stop playing sounds and music.
    Demo_StopPlayback();
    S_Reset();

    //GL_InitVarFont();

    /*glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, theWindow->width, theWindow->height, 0, -1, 1);*/

    hadFog = usingFog;
    GL_TotalReset();
    GL_TotalRestore(); // Bring GL back online.

    // Make sure the fog is enabled, if necessary.
    if(hadFog)
        GL_UseFog(true);

    // Now that GL is back online, we can continue the update in busy mode.
    Con_InitProgress(200);
    Con_Busy(BUSYF_ACTIVITY | BUSYF_PROGRESS_BAR
             | (verbose? BUSYF_CONSOLE_OUTPUT : 0), "Updating engine state...",
             DD_UpdateWorker, NULL);

    //GL_ShutdownVarFont();

    /*glMatrixMode(GL_PROJECTION);
    glPopMatrix();*/

    gx.UpdateState(DD_POST);

    // Reset the anim groups (if in-game)
    R_ResetAnimGroups();
}

/* *INDENT-OFF* */
ddvalue_t ddValues[DD_LAST_VALUE - DD_FIRST_VALUE - 1] = {
    {&netGame, 0},
    {&isServer, 0},                         // An *open* server?
    {&isClient, 0},
    {&allowFrames, &allowFrames},
    {&viewwindowx, &viewwindowx},
    {&viewwindowy, &viewwindowy},
    {&viewwidth, &viewwidth},
    {&viewheight, &viewheight},
    {&consolePlayer, &consolePlayer},
    {&displayPlayer, &displayPlayer},
    {&mipmapping, 0},
    {&linearRaw, 0},
    {&defResX, &defResX},
    {&defResY, &defResY},
    {&skyDetail, 0},
    {&sfxVolume, &sfxVolume},
    {&musVolume, &musVolume},
    {0, 0}, //{&mouseInverseY, &mouseInverseY},
    {&levelFullBright, &levelFullBright},
    {&CmdReturnValue, 0},
    {&gameReady, &gameReady},
    {&isDedicated, 0},
    {&novideo, 0},
    {&defs.count.mobjs.num, 0},
    {&gotFrame, 0},
    {&playback, 0},
    {&defs.count.sounds.num, 0},
    {&defs.count.music.num, 0},
    {&numLumps, 0},
    {&clientPaused, &clientPaused},
    {&weaponOffsetScaleY, &weaponOffsetScaleY},
    {&monochrome, &monochrome},
    {&gameDataFormat, &gameDataFormat},
    {&gameDrawHUD, 0},
    {&upscaleAndSharpenPatches, &upscaleAndSharpenPatches},
    {&symbolicEchoMode, &symbolicEchoMode},
    {&GL_state.maxTexUnits, 0}
};
/* *INDENT-ON* */

/**
 * Get a 32-bit signed integer value.
 */
int DD_GetInteger(int ddvalue)
{
    if(ddvalue >= DD_LAST_VALUE || ddvalue <= DD_FIRST_VALUE)
    {
        // How about some specials?
        switch(ddvalue)
        {
        case DD_DYNLIGHT_TEXTURE:
            return (int) GL_PrepareLSTexture(LST_DYNAMIC);

        case DD_MAP_MUSIC:
        {
            gamemap_t *map = P_GetCurrentMap();
            ded_mapinfo_t *mapInfo = Def_GetMapInfo(P_GetMapID(map));

            if(mapInfo)
                return Def_GetMusicNum(mapInfo->music);
            return -1;
        }
        case DD_WINDOW_WIDTH:
            return theWindow->width;

        case DD_WINDOW_HEIGHT:
            return theWindow->height;

        default:
            break;
        }
        return 0;
    }

    if(ddValues[ddvalue].readPtr == NULL)
        return 0;

    return *ddValues[ddvalue].readPtr;
}

/**
 * Set a 32-bit signed integer value.
 */
void DD_SetInteger(int ddvalue, int parm)
{
    if(ddvalue <= DD_FIRST_VALUE || ddvalue >= DD_LAST_VALUE)
    {
        // How about some special values?
        return;
    }
    if(ddValues[ddvalue].writePtr)
        *ddValues[ddvalue].writePtr = parm;
}

/**
 * Get a pointer to the value of a variable. Not all variables support
 * this. Added for 64-bit support.
 */
void* DD_GetVariable(int ddvalue)
{
    if(ddvalue >= DD_LAST_VALUE || ddvalue <= DD_FIRST_VALUE)
    {
        // How about some specials?
        switch(ddvalue)
        {
        case DD_GAME_EXPORTS:
            return &gx;

        case DD_VIEW_X:
            return &viewX;

        case DD_VIEW_Y:
            return &viewY;

        case DD_VIEW_Z:
            return &viewZ;

        case DD_VIEW_ANGLE:
            return &viewAngle;

        case DD_VIEW_PITCH:
            return &viewPitch;

        case DD_SECTOR_COUNT:
            return &numSectors;

        case DD_LINE_COUNT:
            return &numLineDefs;

        case DD_SIDE_COUNT:
            return &numSideDefs;

        case DD_VERTEX_COUNT:
            return &numVertexes;

        case DD_POLYOBJ_COUNT:
            return &numPolyObjs;

        case DD_SEG_COUNT:
            return &numSegs;

        case DD_SUBSECTOR_COUNT:
            return &numSSectors;

        case DD_NODE_COUNT:
            return &numNodes;

        case DD_MATERIAL_COUNT:
            return &numMaterialBinds;

        case DD_TRACE_ADDRESS:
            return &traceLOS;

        case DD_TRANSLATIONTABLES_ADDRESS:
            return translationTables;

        case DD_MAP_NAME:
        {
            gamemap_t *map = P_GetCurrentMap();
            ded_mapinfo_t *mapInfo = Def_GetMapInfo(P_GetMapID(map));

            if(mapInfo && mapInfo->name[0])
            {
                int                 id;

                if((id = Def_Get(DD_DEF_TEXT, mapInfo->name, NULL)) != -1)
                {
                    return defs.text[id].text;
                }

                return mapInfo->name;
            }
            break;
        }
        case DD_MAP_AUTHOR:
        {
            gamemap_t *map = P_GetCurrentMap();
            ded_mapinfo_t *mapInfo = Def_GetMapInfo(P_GetMapID(map));

            if(mapInfo && mapInfo->author[0])
                return mapInfo->author;
            break;
        }
        case DD_MAP_MIN_X:
        {
            gamemap_t  *map = P_GetCurrentMap();
            if(map)
                return &map->bBox[BOXLEFT];
            else
                return NULL;
        }
        case DD_MAP_MIN_Y:
        {
            gamemap_t  *map = P_GetCurrentMap();
            if(map)
                return &map->bBox[BOXBOTTOM];
            else
                return NULL;
        }
        case DD_MAP_MAX_X:
        {
            gamemap_t  *map = P_GetCurrentMap();
            if(map)
                return &map->bBox[BOXRIGHT];
            else
                return NULL;
        }
        case DD_MAP_MAX_Y:
        {
            gamemap_t  *map = P_GetCurrentMap();
            if(map)
                return &map->bBox[BOXTOP];
            else
                return NULL;
        }
        case DD_PSPRITE_OFFSET_X:
            return &pspOffset[VX];

        case DD_PSPRITE_OFFSET_Y:
            return &pspOffset[VY];

        case DD_PSPRITE_LIGHTLEVEL_MULTIPLIER:
            return &pspLightLevelMultiplier;

        case DD_SHARED_FIXED_TRIGGER:
            return &sharedFixedTrigger;

        case DD_CPLAYER_THRUST_MUL:
            return &cplrThrustMul;

        case DD_GRAVITY:
            return &mapGravity;

        case DD_TORCH_RED:
            return &torchColor[CR];

        case DD_TORCH_GREEN:
            return &torchColor[CG];

        case DD_TORCH_BLUE:
            return &torchColor[CB];

        case DD_TORCH_ADDITIVE:
            return &torchAdditive;
#ifdef WIN32
        case DD_WINDOW_HANDLE:
            return Sys_GetWindowHandle(windowIDX);
#endif

        // We have to separately calculate the 35 Hz ticks.
        case DD_GAMETIC:
            {
            static timespan_t       fracTic;
            fracTic = gameTime * TICSPERSEC;
            return &fracTic;
            }
        case DD_OPENRANGE:
            return &openrange;

        case DD_OPENTOP:
            return &opentop;

        case DD_OPENBOTTOM:
            return &openbottom;

        case DD_LOWFLOOR:
            return &lowfloor;

        default:
            break;
        }
        return 0;
    }

    // Other values not supported.
    return ddValues[ddvalue].writePtr;
}

/**
 * Set the value of a variable. The pointer can point to any data, its
 * interpretation depends on the variable. Added for 64-bit support.
 */
void DD_SetVariable(int ddvalue, void *parm)
{
    if(ddvalue <= DD_FIRST_VALUE || ddvalue >= DD_LAST_VALUE)
    {
        switch(ddvalue)
        {
        case DD_VIEW_X:
            viewX = *(float*) parm;
            return;

        case DD_VIEW_Y:
            viewY = *(float*) parm;
            return;

        case DD_VIEW_Z:
            viewZ = *(float*) parm;
            return;

        case DD_VIEW_ANGLE:
            viewAngle = *(angle_t*) parm;
            return;

        case DD_VIEW_PITCH:
            viewPitch = *(float*) parm;
            return;

        case DD_CPLAYER_THRUST_MUL:
            cplrThrustMul = *(float*) parm;
            return;

        case DD_GRAVITY:
            mapGravity = *(float*) parm;
            return;

        case DD_PSPRITE_OFFSET_X:
            pspOffset[VX] = *(float*) parm;
            return;

        case DD_PSPRITE_OFFSET_Y:
            pspOffset[VY] = *(float*) parm;
            return;

        case DD_PSPRITE_LIGHTLEVEL_MULTIPLIER:
            pspLightLevelMultiplier = *(float*) parm;
            return;

        case DD_TORCH_RED:
            torchColor[CR] = MINMAX_OF(0, *((float*) parm), 1);
            return;

        case DD_TORCH_GREEN:
            torchColor[CG] = MINMAX_OF(0, *((float*) parm), 1);
            return;

        case DD_TORCH_BLUE:
            torchColor[CB] = MINMAX_OF(0, *((float*) parm), 1);
            return;

        case DD_TORCH_ADDITIVE:
            torchAdditive = (*(int*) parm)? true : false;
            break;

        default:
            break;
        }
    }
}

/**
 * Gets the data of a player.
 */
ddplayer_t *DD_GetPlayer(int number)
{
    return (ddplayer_t *) &ddPlayers[number].shared;
}

#ifdef UNIX
/**
 * Some routines are not available on the *nix platform.
 */
char *strupr(char *string)
{
    char   *ch = string;

    for(; *ch; ch++)
        *ch = toupper(*ch);
    return string;
}

char *strlwr(char *string)
{
    char   *ch = string;

    for(; *ch; ch++)
        *ch = tolower(*ch);
    return string;
}
#endif

/**
 * Prints a formatted string into a fixed-size buffer. At most @c size
 * characters will be
 * written to the output buffer @c str. The output will always contain a
 * terminating null character.
 *
 * @param str           Output buffer.
 * @param size          Size of the output buffer.
 * @param format        Format of the output.
 * @param ap            Variable-size argument list.
 *
 * @return              Number of characters written to the output buffer
 *                      if lower than or equal to @c size, else @c -1.
 */
int dd_vsnprintf(char* str, size_t size, const char* format, va_list ap)
{
    int                 result = vsnprintf(str, size, format, ap);

#ifdef WIN32
    // Always terminate.
    str[size - 1] = 0;
    return result;
#else
    return result >= size? -1 : size;
#endif
}

/**
 * Prints a formatted string into a fixed-size buffer. At most @c size
 * characters will be written to the output buffer @c str. The output will
 * always contain a terminating null character.
 *
 * @param str           Output buffer.
 * @param size          Size of the output buffer.
 * @param format        Format of the output.
 *
 * @return              Number of characters written to the output buffer
 *                      if lower than or equal to @c size, else @c -1.
 */
int dd_snprintf(char* str, size_t size, const char* format, ...)
{
    int                 result = 0;

    va_list args;
    va_start(args, format);
    result = dd_vsnprintf(str, size, format, args);
    va_end(args);

    return result;
}
