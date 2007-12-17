/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2007 Daniel Swanson <danij@dengine.net>
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
 *
 * \bug Not 64bit clean: In function 'DD_GetInteger': cast from pointer to integer of different size
 * \bug Not 64bit clean: In function 'DD_SetInteger': cast to pointer from integer of different size
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

#define MAXWADFILES 4096

// TYPES -------------------------------------------------------------------

typedef struct ddvalue_s {
    int    *readPtr;
    int    *writePtr;
} ddvalue_t;

typedef struct autoload_s {
    boolean loadFiles;   // Should files be loaded right away.
    int count;           // Number of files loaded successfully.
} autoload_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void    G_CheckDemoStatus();
void    F_Drawer(void);
void    S_InitScript(void);
void    Net_Drawer(void);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static int DD_StartupWorker(void *parm);
static int DD_StartupWorker2(void *parm);
static void HandleArgs(int state);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

#ifdef WIN32
extern HINSTANCE hInstDGL;
#endif

extern int renderTextures;
extern char skyFlatName[9];
extern int gotframe;
extern int monochrome;
extern int gamedataformat;
extern int gamedrawhud;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

directory_t ddRuntimeDir, ddBinDir;
int     verbose = 0;            // For debug messages (-verbose).
boolean DevMaps;                // true = Map development mode
char   *DevMapsDir = "";        // development maps directory
int     shareware;              // true if only episode 1 present
boolean debugmode;              // checkparm of -debug
boolean cdrom;                  // true if cd-rom mode active
boolean cmdfrag;                // true if a CMD_FRAG packet should be sent out
boolean singletics;             // debug flag to cancel adaptiveness
int     isDedicated = false;
int     maxzone = 0x2000000;    // Default zone heap. (32meg)
boolean autostart;
FILE   *outFile;                // Output file for console messages.

char   *iwadlist[64];
char   *defaultWads = "";       /* A list of wad names, whitespace in between
                                   (in .cfg). */
filename_t configFileName;
filename_t bindingsConfigFileName;
filename_t defsFileName, topDefsFileName;
filename_t ddBasePath = "";     // Doomsday root directory is at...?

int     queryResult = 0;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static char *wadfiles[MAXWADFILES];
static boolean userDirOk = true;

// CODE --------------------------------------------------------------------

/**
 * Convert propertyType enum constant into a string for error/debug messages.
 */
const char* value_Str(int val)
{
    static char valStr[40];
    struct val_s {
        int val;
        const char* str;
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
        { DDVT_FLAT_INDEX, "DDVT_FLAT_INDEX" },
        { DDVT_BLENDMODE, "DDVT_BLENDMODE" },
        { DDVT_VERT_IDX, "DDVT_VERT_IDX" },
        { DDVT_LINE_IDX, "DDVT_LINE_IDX" },
        { DDVT_SIDE_IDX, "DDVT_SIDE_IDX" },
        { DDVT_SECT_IDX, "DDVT_SECT_IDX" },
        { DDVT_SEG_IDX, "DDVT_SEG_IDX" },
        { 0, NULL }
    };
    uint        i;

    for(i = 0; valuetypes[i].str; ++i)
        if(valuetypes[i].val == val)
            return valuetypes[i].str;

    sprintf(valStr, "(unnamed %i)", val);
    return valStr;
}

/**
 * Adds the given IWAD to the list of default IWADs.
 */
void DD_AddIWAD(const char *path)
{
    int     i = 0;
    char    buf[256];

    while(iwadlist[i])
        i++;
    M_TranslatePath(path, buf);
    iwadlist[i] = M_Calloc(strlen(buf) + 1);   // This mem is not freed?
    strcpy(iwadlist[i], buf);
}

#define ATWSEPS ",; \t"
static void AddToWadList(char *list)
{
    int     len = strlen(list);
    char   *buffer = M_Malloc(len + 1), *token;

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
static int autoDataAdder(const char *fileName, filetype_t type, void *ptr)
{
    autoload_t *data = ptr;

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
 * directory are added to the wadfiles list.  Returns the number of new
 * files that were loaded.
 */
int DD_AddAutoData(boolean loadFiles)
{
    autoload_t data;
    const char *extensions[] = {
        "wad", "lmp", "pk3", "zip", "deh",
#ifdef UNIX
        "WAD", "LMP", "PK3", "ZIP", "DEH", // upper case alternatives
#endif
        NULL
    };
    char    pattern[256];
    uint    i;

    data.loadFiles = loadFiles;
    data.count = 0;

    for(i = 0; extensions[i]; ++i)
    {
        sprintf(pattern, "%sauto\\*.%s", R_GetDataPath(), extensions[i]);
        Dir_FixSlashes(pattern);
        F_ForAll(pattern, &data, autoDataAdder);
    }

    return data.count;
}

void DD_SetConfigFile(char *filename)
{
    strcpy(configFileName, filename);
    Dir_FixSlashes(configFileName);
    strcpy(bindingsConfigFileName, configFileName);
    strcpy(bindingsConfigFileName + strlen(bindingsConfigFileName) - 4,
           "-bindings.cfg");
}

/**
 * Set the primary DED file, which is included immediately after
 * Doomsday.ded.
 */
void DD_SetDefsFile(char *filename)
{
    sprintf(topDefsFileName, "%sdefs\\%s", ddBasePath, filename);
    Dir_FixSlashes(topDefsFileName);
}

/**
 * Define Auto mappings for the runtime directory.
 */
void DD_DefineBuiltinVDM(void)
{
    filename_t dest;

    // Data files.
    sprintf(dest, "%sauto", R_GetDataPath());
    F_AddMapping("auto", dest);

    // Definition files.
    Def_GetAutoPath(dest);
    F_AddMapping("auto", dest);
}

/**
 * Looks for new files to autoload. Runs until there are no more files to
 * autoload.
 */
void DD_AutoLoad(void)
{
    int p;

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
    int         exitCode;

#ifdef UNIX
#   ifndef MACOSX
        if(getenv("HOME"))
        {
            filename_t homeDir;
            sprintf(homeDir, "%s/.deng", getenv("HOME"));
            M_CheckPath(homeDir);
            Dir_MakeDir(homeDir, &ddRuntimeDir);
            userDirOk = Dir_ChDir(&ddRuntimeDir);
        }
#   endif
#endif

    // The -userdir option sets the working directory.
    if(ArgCheckWith("-userdir", 1))
    {
        Dir_MakeDir(ArgNext(), &ddRuntimeDir);
        userDirOk = Dir_ChDir(&ddRuntimeDir);
    }

    // The current working directory is the runtime dir.
    Dir_GetDir(&ddRuntimeDir);

#ifdef UNIX
    /** The base path is always the same and depends on the build
     * configuration.  Usually this is something like
     * "/usr/share/deng/".
     */
#   ifdef MACOSX
    strcpy(ddBasePath, "./");
#   else
    strcpy(ddBasePath, DENG_BASE_DIR);
#   endif
#endif

#ifdef WIN32
    // The standard base directory is two levels upwards.
    if(ArgCheck("-stdbasedir"))
    {
        strcpy(ddBasePath, "..\\..\\");
    }
#endif

    if(ArgCheckWith("-basedir", 1))
    {
        strcpy(ddBasePath, ArgNext());
        Dir_ValidDir(ddBasePath);
    }

    Dir_MakeAbsolute(ddBasePath);
    Dir_ValidDir(ddBasePath);

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
             | (verbose? BUSYF_CONSOLE_OUTPUT : 0), DD_StartupWorker, NULL);

    // Engine initialization is complete. Now finish up with the GL.
    if(!isDedicated)
    {
        GL_Init();
        GL_InitRefresh(true, true);
    }

    // Do deferred uploads.
    Con_Busy(BUSYF_PROGRESS_BAR | BUSYF_STARTUP | BUSYF_ACTIVITY
             | (verbose? BUSYF_CONSOLE_OUTPUT : 0), DD_StartupWorker2, NULL);

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

    if(netgame)
    {   // Quit netgame if one is in progress.
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
    int     p = 0;

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
    R_InitExternalResources();

    // The name of the .cfg will invariably be overwritten by the Game.
    strcpy(configFileName, "doomsday.cfg");
    sprintf(defsFileName, "%sdefs\\doomsday.ded", ddBasePath);

    // Was the change to userdir OK?
    if(!userDirOk)
        Con_Message("--(!)-- User directory not found " "(check -userdir).\n");

    bamsInit();                 // Binary angle calculations.

    // Initialize the zip file database.
    Zip_Init();

    Def_Init();

    autostart = false;
    shareware = false;          // Always false for Hexen

    HandleArgs(0);              // Everything but WADs.

    novideo = ArgCheck("-novideo") || isDedicated;

    DAM_Init();

    if(gx.PreInit)
        gx.PreInit();

    Con_SetProgress(30);

    // We now accept no more custom properties.
    DAM_LockCustomPropertys();

    // Automatically create an Auto mapping in the runtime directory.
    DD_DefineBuiltinVDM();

    // Initialize subsystems
    Net_Init();                 // Network before anything else.

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
            char   *arg = ArgNext();

            if(!arg || arg[0] == '-')
                break;
            Con_Message("Parsing: %s\n", arg);
            Con_ParseCommands(arg, false);
        }
    }

    Con_SetProgress(40);

    if(defaultWads)
        AddToWadList(defaultWads);  // These must take precedence.
    HandleArgs(1);              // Only the WADs.

    Con_Message("W_Init: Init WADfiles.\n");

    // Add real files from the Auto directory to the wadfiles list.
    DD_AddAutoData(false);

    W_InitMultipleFiles(wadfiles);
    F_InitDirec();

    Con_SetProgress(75);

    // Load help resources. Now virtual files are available as well.
    if(!isDedicated)
        DD_InitHelp();

    // Final autoload round.
    DD_AutoLoad();

    Con_SetProgress(80);

    // No more WADs will be loaded in startup mode after this point.
    W_EndStartup();

    VERBOSE(W_PrintMapList());

    // Execute the startup script (Startup.cfg).
    Con_ParseCommands("startup.cfg", false);

    // Now the game can identify the game mode.
    gx.UpdateState(DD_GAME_MODE);

    // Now that we've read the WADs we can initialize definitions.
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
        char   *arg = ArgNext();
        char    fname[100];
        FILE   *file;
        int     lump = W_GetNumForName(arg);
        byte   *lumpPtr = W_CacheLumpNum(lump, PU_STATIC);

        sprintf(fname, "%s.dum", arg);
        file = fopen(fname, "wb");
        if(!file)
        {
            Con_Error("Couldn't open %s for writing. %s\n", fname,
                      strerror(errno));
        }
        fwrite(lumpPtr, 1, lumpinfo[lump].size, file);
        fclose(file);
        Con_Error("%s dumped to %s.\n", arg, fname);
    }

    if(ArgCheck("-dumpwaddir"))
    {
        char buff[10];
        printf("Lumps (%d total):\n", numlumps);
        for(p = 0; p < numlumps; p++)
        {
            strncpy(buff, lumpinfo[p].name, 8);
            buff[8] = 0;
            printf("%04i - %-8s (hndl: %p, pos: %i, size: %lu)\n", p, buff,
                   lumpinfo[p].handle, lumpinfo[p].position,
                   (unsigned long) lumpinfo[p].size);
        }
        Con_Error("---End of lumps---\n");
    }

    Con_SetProgress(100);

    Con_Message("B_Init: Init bindings.\n");
    B_Init();
    Con_ParseCommands(bindingsConfigFileName, false);

    Con_SetProgress(125);

    Con_Message("R_Init: Init the refresh daemon.\n");
    R_Init();

    Con_SetProgress(199);

    // Palette information will be needed for preparing textures.
    GL_EarlyInitTextureManager();

    Con_Message("Net_InitGame: Initializing game data.\n");
    Net_InitGame();
    Demo_Init();

    if(gx.PostInit)
        gx.PostInit();

    // Now the defs have been read we can init the map format info
    P_InitData();

    // Try to load the autoexec file. This is done here to make sure
    // everything is initialized: the user can do here anything that
    // s/he'd be able to do in the game.
    Con_ParseCommands("autoexec.cfg", false);

    // Parse additional files.
    if(ArgCheckWith("-parse", 1))
    {
        for(;;)
        {
            char   *arg = ArgNext();

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
            char   *arg = Argv(p);

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

    Con_SetProgress(200);

    Plug_DoHook(HOOK_INIT, 0, 0);   // Any initialization hooks?
    Con_UpdateKnownWords();         // For word completion (with Tab).

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
    int     order, p;

    if(state == 0)
    {
        debugmode = ArgExists("-debug");
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
                if((order == 1 &&
                    (stricmp(Argv(p), "-f") && stricmp(Argv(p), "-file"))) ||
                   (order == 0 &&
                    stricmp(Argv(p), "-iwad")))
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
        if(ArgCheckWith("-timedemo", 1) // Timedemo mode.
           || ArgCheckWith("-playdemo", 1)) // Play-once mode.
        {
            char    buf[200];

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
void DD_AddStartupWAD(const char *file)
{
    int     i;
    char   *new, temp[300];

    i = 0;
    while(wadfiles[i])
        i++;
    M_TranslatePath(file, temp);
    new = M_Calloc(strlen(temp) + 1);  // This is never freed?
    strcat(new, temp);
    wadfiles[i] = new;
}

void DD_UpdateEngineState(void)
{
    // Update refresh.
    Con_Message("Updating state...\n");

    // Update the dir/WAD translations.
    F_InitDirec();

    gx.UpdateState(DD_PRE);
    R_Update();
    gx.UpdateState(DD_POST);

    // Reset the anim groups (if in-game)
    R_ResetAnimGroups();

    // \fixme We need to update surfaces.
    //R_UpdateAllSurfaces(true);
}

/**
 * Queries are a (poor) way to extend the API without adding new functions.
 */
void DD_CheckQuery(int query, int parm)
{
    switch (query)
    {
    case DD_TEXTURE_HEIGHT_QUERY:
        queryResult = textures[parm]->info.height;
        break;
#if 0
    // Unused
    case DD_NET_QUERY:
        switch (parm)
        {
        case DD_PROTOCOL:
            queryResult = (int) N_GetProtocolName();
            break;
        }
        break;
#endif
    default:
        break;
    }
}

/* *INDENT-OFF* */
ddvalue_t ddValues[DD_LAST_VALUE - DD_FIRST_VALUE - 1] = {
    {&netgame, 0},
    {&isServer, 0},                         // An *open* server?
    {&isClient, 0},
    {&allowFrames, &allowFrames},
    {0, 0},
    {&viewwindowx, &viewwindowx},
    {&viewwindowy, &viewwindowy},
    {&viewwidth, &viewwidth},
    {&viewheight, &viewheight},
    {&consoleplayer, &consoleplayer},
    {&displayplayer, &displayplayer},
    {0, 0},
    {&mipmapping, 0},
    {&linearRaw, 0},
    {&defResX, &defResX},
    {&defResY, &defResY},
    {&skyDetail, 0},
    {&sfx_volume, &sfx_volume},
    {&mus_volume, &mus_volume},
    {0, 0}, //{&mouseInverseY, &mouseInverseY},
    {&queryResult, 0},
    {&levelFullBright, &levelFullBright},
    {&CmdReturnValue, 0},
    {&gameReady, &gameReady},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {&isDedicated, 0},
    {&novideo, 0},
    {&defs.count.mobjs.num, 0},
    {&gotframe, 0},
    {&playback, 0},
    {&defs.count.sounds.num, 0},
    {&defs.count.music.num, 0},
    {&numlumps, 0},
    {&send_all_players, &send_all_players},
    {&clientPaused, &clientPaused},
    {&weaponOffsetScaleY, &weaponOffsetScaleY},
    {&monochrome, &monochrome},
    {&gamedataformat, &gamedataformat},
    {&gamedrawhud, 0},
    {&upscaleAndSharpenPatches, &upscaleAndSharpenPatches}
};
/* *INDENT-ON* */

int skyFlatNum;

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
            return (int) GL_PrepareLSTexture(LST_DYNAMIC, NULL);

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

        case DD_SKYFLATNUM:
            return skyFlatNum;
        }
        return 0;
    }

    // We have to separately calculate the 35 Hz ticks.
    if(ddvalue == DD_GAMETIC)
        return SECONDS_TO_TICKS(gameTime);

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
        DD_CheckQuery(ddvalue, parm);
        // How about some special values?
        if(ddvalue == DD_SKYFLATNUM)
        {
            skyFlatNum = parm;
        }
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
        case DD_VIEWX:
            return &viewX;

        case DD_VIEWY:
            return &viewY;

        case DD_VIEWZ:
            return &viewZ;

        case DD_VIEWX_OFFSET:
            return &viewXOffset;

        case DD_VIEWY_OFFSET:
            return &viewYOffset;

        case DD_VIEWZ_OFFSET:
            return &viewZOffset;

        case DD_VIEWANGLE:
            return &viewAngle;

        case DD_VIEWANGLE_OFFSET:
            return &viewAngleOffset;

        case DD_SECTOR_COUNT:
            return &numsectors;

        case DD_LINE_COUNT:
            return &numlines;

        case DD_SIDE_COUNT:
            return &numsides;

        case DD_VERTEX_COUNT:
            return &numvertexes;

        case DD_POLYOBJ_COUNT:
            return &po_NumPolyobjs;

        case DD_SEG_COUNT:
            return &numsegs;

        case DD_SUBSECTOR_COUNT:
            return &numsubsectors;

        case DD_NODE_COUNT:
            return &numnodes;

        case DD_THING_COUNT:
            return &numthings;

        case DD_TRACE_ADDRESS:
            return &trace;

        case DD_TRANSLATIONTABLES_ADDRESS:
            return translationtables;

        case DD_MAP_NAME:
        {
            gamemap_t *map = P_GetCurrentMap();
            ded_mapinfo_t *mapInfo = Def_GetMapInfo(P_GetMapID(map));

            if(mapInfo && mapInfo->name[0])
                return mapInfo->name;
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
                return &map->bbox[BOXLEFT];
            else
                return NULL;
        }
        case DD_MAP_MIN_Y:
        {
            gamemap_t  *map = P_GetCurrentMap();
            if(map)
                return &map->bbox[BOXBOTTOM];
            else
                return NULL;
        }
        case DD_MAP_MAX_X:
        {
            gamemap_t  *map = P_GetCurrentMap();
            if(map)
                return &map->bbox[BOXRIGHT];
            else
                return NULL;
        }
        case DD_MAP_MAX_Y:
        {
            gamemap_t  *map = P_GetCurrentMap();
            if(map)
                return &map->bbox[BOXTOP];
            else
                return NULL;
        }
        case DD_PSPRITE_OFFSET_X:
            return &pspOffset[VX];

        case DD_PSPRITE_OFFSET_Y:
            return &pspOffset[VY];

        case DD_SHARED_FIXED_TRIGGER:
            return &sharedFixedTrigger;

        case DD_CPLAYER_THRUST_MUL:
            return &cplr_thrust_mul;

        case DD_GRAVITY:
            return &mapGravity;

#ifdef WIN32
        case DD_WINDOW_HANDLE:
            return Sys_GetWindowHandle(windowIDX);
#endif
        }
        return 0;
    }
    if(ddvalue == DD_OPENRANGE)
        return &openrange;
    if(ddvalue == DD_OPENTOP)
        return &opentop;
    if(ddvalue == DD_OPENBOTTOM)
        return &openbottom;
    if(ddvalue == DD_LOWFLOOR)
        return &lowfloor;

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
        case DD_VIEWX:
            viewX = *(float*) parm;
            return;

        case DD_VIEWY:
            viewY = *(float*) parm;
            return;

        case DD_VIEWZ:
            viewZ = *(float*) parm;
            return;

        case DD_VIEWX_OFFSET:
            viewXOffset = *(float*) parm;
            return;

        case DD_VIEWY_OFFSET:
            viewYOffset = *(float*) parm;
            return;

        case DD_VIEWZ_OFFSET:
            viewZOffset = *(float*) parm;
            return;

        case DD_VIEWANGLE:
            viewAngle = *(angle_t*) parm;
            return;

        case DD_VIEWANGLE_OFFSET:
            viewAngleOffset = *(int*) parm;
            return;

        case DD_CPLAYER_THRUST_MUL:
            cplr_thrust_mul = *(float*) parm;
            return;

        case DD_GRAVITY:
            mapGravity = *(float*) parm;
            return;

        case DD_SKYMASKMATERIAL_NAME:
            memset(skyFlatName, 0, 9);
            strncpy(skyFlatName, parm, 9);
            return;

        case DD_PSPRITE_OFFSET_X:
            pspOffset[VX] = *(float*) parm;
            return;

        case DD_PSPRITE_OFFSET_Y:
            pspOffset[VY] = *(float*) parm;
            return;

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
    return (ddplayer_t *) &players[number];
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
