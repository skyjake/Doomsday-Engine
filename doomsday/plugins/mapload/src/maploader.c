/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2007 Daniel Swanson <danij@dengine.net>
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
 * maploader.c: Doomsday Plugin for Loading Maps
 *
 * This plugin has been built on glBSP 2.20, which is hosted on
 * SourceForge: http://sourceforge.net/projects/glbsp/
 *
 * The purpose of a map loader plugin is to provide Doomsday with the
 * raw byte data of any requested map.  Doomsday will give the plugin
 * the lump name of the map to load.  The plugin will return the data
 * lumps, each allocated from the memory zone (level purged).
 *
 * The plugin uses glBSP to build accurate GL nodes data on the fly,
 * Just In Time.  The generated GL data is stored under the runtime
 * directory (in "bspcache/(game-mode)/").
 */

// HEADER FILES ------------------------------------------------------------

#ifdef WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif

#include "doomsday.h"
#include "dd_api.h"

#include "glbsp.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

typedef struct {
    char    identification[4];
    int32_t numlumps;
    int32_t infotableofs;
} wadheader_t;

typedef struct {
    int32_t filepos;
    uint32_t size;
    char    name[8];
} wadlump_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static int loadLumpsHook(int hookType, int parm, void *data);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static const char *bspDir = "bspcache\\";

// CODE --------------------------------------------------------------------

/**
 * This function is called automatically when the plugin is loaded.
 * We let the engine know what we'd like to do.
 */
void DP_Initialize(void)
{
    Plug_AddHook(HOOK_LOAD_MAP_LUMPS, loadLumpsHook);
}

#ifdef WIN32
/**
 * Windows calls this when the DLL is loaded.
 */
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    switch (fdwReason)
    {
    case DLL_PROCESS_ATTACH:
        // Register our hooks.
        DP_Initialize();
        break;
    }
    return TRUE;
}
#endif

/**
 * Compose the path where to put the temporary data and built GL BSP
 * data.
 */
static void GetWorkDir(char *dir, int mainLump)
{
    //// \kludge Hacks'n'Kludges Illustrated: Accessing the Game Mode String!
    game_export_t *gex = (game_export_t*) DD_GetVariable(DD_GAME_EXPORTS);
    const char *sourceFile = W_LumpSourceFile(mainLump);
    filename_t  base;
    unsigned short identifier = 0;
    int         i;

    M_ExtractFileBase(sourceFile, base);

    for(i = 0; sourceFile[i]; ++i)
        identifier ^= sourceFile[i] << ((i * 3) % 11);

    // The work directory path is relative to the runtime directory.
    sprintf(dir, "%s%s\\%s-%04X\\", bspDir,
            (const char*) gex->GetVariable(DD_GAME_MODE),
            base, identifier);

    M_TranslatePath(dir, dir);
}

enum {
    ML_INVALID = -1,
    ML_THINGS, 
    ML_LINEDEFS,
    ML_SIDEDEFS,
    ML_VERTEXES,
    ML_SEGS,
    ML_SSECTORS,
    ML_NODES,
    ML_SECTORS,
    ML_REJECT,
    ML_BLOCKMAP,
    ML_BEHAVIOR,
    ML_SCRIPTS
};

static int mapLumpTypeForName(const char *name)
{
    struct maplumpinfo_s {
        int         type;
        const char *name;
    } mapLumpInfos[] =
    {
        {ML_THINGS,     "THINGS"}, 
        {ML_LINEDEFS,   "LINEDEFS"},
        {ML_SIDEDEFS,   "SIDEDEFS"},
        {ML_VERTEXES,   "VERTEXES"},
        {ML_SEGS,       "SEGS"},
        {ML_SSECTORS,   "SSECTORS"},
        {ML_NODES,      "NODES"},
        {ML_SECTORS,    "SECTORS"},
        {ML_REJECT,     "REJECT"},
        {ML_BLOCKMAP,   "BLOCKMAP"},
        {ML_BEHAVIOR,   "BEHAVIOR"},
        {ML_SCRIPTS,    "SCRIPTS"},
        {ML_INVALID,    NULL}
    };

    int         i;

    if(!name)
        return ML_INVALID;

    for(i = 0; mapLumpInfos[i].type > ML_INVALID; ++i)
    {
        if(!strcmp(name, mapLumpInfos[i].name))
            return mapLumpInfos[i].type;
    }

    return ML_INVALID;
}

/**
 * Write all the lumps of the specified map to a WAD file.
 */
static void DumpMap(int mainLump, const char *fileName)
{
#define MAX_MAP_LUMPS 12

    FILE       *file;
    wadheader_t header;
    wadlump_t   lumps[MAX_MAP_LUMPS];
    int         i, id;
    char       *buffer;

    if((file = fopen(fileName, "wb")) == NULL)
    {
        Con_Error("dpMapLoad.DumpMap: couldn't open %s for writing.\n",
                  fileName);
        return;
    }

    // The header will be updated as we write the lumps.
    memset(&header, 0, sizeof(header));
    strncpy(header.identification, "PWAD", 4);

    memset(lumps, 0, sizeof(lumps));
    strncpy(lumps[0].name, W_LumpName(mainLump), 8);

    // Write all the lumps that belong to the map (consecutive).
    for(id = 1, i = 1; i < MAX_MAP_LUMPS; ++i)
    {
        int         lumpType;
        boolean     finished = false;

        // Is this still a map lump?
        lumpType = mapLumpTypeForName(W_LumpName(mainLump + i));
        switch(lumpType)
        {
        case ML_INVALID:
            finished = true;
            break;

        case ML_SEGS:
        case ML_SSECTORS:
        case ML_NODES:
            // Skip these lumps.
            continue;

        default:
            break;
        }

        if(finished)
            break;

        strncpy(lumps[id].name, W_LumpName(mainLump + i), 8);
        lumps[id].filepos = LONG(ftell(file));
        lumps[id].size = LONG(W_LumpLength(mainLump + i));

        buffer = malloc(LONG(lumps[id].size));
        W_ReadLump(mainLump + i, buffer);
        fwrite(buffer, LONG(lumps[id].size), 1, file);
        free(buffer);
        id++;
    }

    header.numlumps = LONG(id);
    header.infotableofs = LONG(ftell(file));

    // Write the directory.
    fwrite(lumps, sizeof(wadlump_t), id, file);

    // Rewrite the updated header.
    fseek(file, 0, SEEK_SET);
    fwrite(&header, sizeof(header), 1, file);
    fclose(file);

#undef MAX_MAP_LUMPS
}

/**
 * Fatal errors are called as a last resort when something serious goes
 * wrong, e.g. out of memory. This routine should show the error to the
 * user and abort the program.
 */
static void fatal_error(const char *str, ...)
{
    char        buffer[2000];
    va_list     args;

    va_start(args, str);
    vsnprintf(buffer, sizeof(buffer), str, args);
    va_end(args);

    Con_Error(buffer);
}

/**
 * The print_msg routine is used to display the various messages that
 * occur, e.g. "Building GL nodes on MAP01" and that kind of thing.
 */
static void print_msg(const char *str, ...)
{
    char        buffer[2000];
    va_list     args;

    va_start(args, str);
    vsnprintf(buffer, sizeof(buffer), str, args);
    va_end(args);

    Con_Message(buffer);
}

/**
 * This routine is called frequently whilst building the nodes, and can be
 * used to keep a GUI responsive to user input. Many toolkits have a
 * "do iteration" or "check events" type of function that this can call.
 * Avoid anything that sleeps though, or it'll slow down the build process
 * unnecessarily.
 */
static void ticker(void)
{
}

/**
 * These display routines is used for tasks that can show a progress bar,
 * namely: building nodes, loading the wad, and saving the wad. The command
 * line version could show a percentage value, or even draw a bar using
 * characters.
 *
 * Display_open is called at the beginning, and `type' holds the type of
 * progress (and determines how many bars to display).
 *
 * @return              @c TRUE, if all went well, or
 *                      @c FALSE, if it failed (in which case the other
 *                      routines should do nothing when called).
 */
static boolean_g display_open(displaytype_e type)
{
    return FALSE;
}

/**
 * For GUI versions this can be used to set the title of the progress
 * window. OK to ignore it (e.g. command line version).
 */
static void display_setTitle(const char *str)
{
}

/**
 * The next three routines control the appearance of each progress bar. 
 * Display_setBarText is called to change the message above the bar. 
 * Display_setBarLimit sets the integer limit of the progress
 * (the target value), and display_setBar sets the current value (which
 * will count up from 0 to the limit, inclusive).
 */
static void display_setBar(int barnum, int count)
{
}

static void display_setBarLimit(int barnum, int limit)
{
}

static void display_setBarText(int barnum, const char *str)
{
}

/**
 * The display_close routine is called when the task is finished, and should
 * remove the progress indicator/window from the screen.
 */
static void display_close(void)
{
}

/**
 * This function is called when Doomsday is loading a map.
 *
 * @param parm          Lump index number of the map lump identifier.
 * @param data          Ptr to an array of integers, which will be used to
 *                      return the lump numbers for the data (normal + GL).
 * @return              Non-zero if successful.
 */
static int loadLumpsHook(int hookType, int parm, void *data)
{
    int        *returnedLumps = (int*) data;
    filename_t  workDir;
    filename_t  cachedMapFile;
    uint        sourceTime;
    uint        bspTime, startTime;
    boolean     built = false;

    GetWorkDir(workDir, parm);

    // Make sure the work directory exists.
    M_CheckPath(workDir);

    // The source data must not be newer than the BSP data.
    sourceTime = F_LastModified(W_LumpSourceFile(parm));

    // First test if we already have valid cached BSP data.
    sprintf(cachedMapFile, "%s%s", workDir, W_LumpName(parm));
    M_TranslatePath(cachedMapFile, cachedMapFile);
    strcat(cachedMapFile, ".wad");

    bspTime = F_LastModified(cachedMapFile);

    //Con_Message("source=%i, bsp=%i\n", sourceTime, bspTime);

    startTime = Sys_GetRealTime();
    if(!Con_GetInteger("bsp-cache") || !F_Access(cachedMapFile) ||
       bspTime < sourceTime)
    {
        nodebuildinfo_t info;
        nodebuildfuncs_t funcs;
        nodebuildcomms_t comms;

        memset(&info, 0, sizeof(info));
        memset(&funcs, 0, sizeof(funcs));
        memset(&comms, 0, sizeof(comms));

        // Only copy the lumps containing the map data structures we need.
        DumpMap(parm, cachedMapFile);

        info.input_file = cachedMapFile;
        info.output_file = cachedMapFile;
        info.factor = Con_GetInteger("bsp-factor");
        info.no_progress = TRUE;
        info.force_normal = TRUE;
        info.merge_vert = TRUE;
        info.gwa_mode = FALSE;
        info.prune_sect = TRUE;
        info.block_limit = 44000;

        funcs.fatal_error = fatal_error;
        funcs.print_msg = print_msg;
        funcs.ticker = ticker;
        funcs.display_open = display_open;
        funcs.display_setTitle = display_setTitle;
        funcs.display_setBar = display_setBar;
        funcs.display_setBarLimit = display_setBarLimit;
        funcs.display_setBarText = display_setBarText;
        funcs.display_close = display_close;

        // Invoke glBSP.
        GlbspBuildNodes(&info, &funcs, &comms);
        built = true;
    }

    // Load the cached data. The lumps are loaded into the auxiliary
    // lump cache, which means they use special index numbers and will
    // be automatically deleted when the next map is loaded.
    *returnedLumps = W_OpenAuxiliary(cachedMapFile); // All map data (original)

    // How much time did we spend?
    print_msg(" %s nodes in %.2f seconds.\n", (built? "Built" : "Loaded cached"),
             (Sys_GetRealTime() - startTime) / 1000.0f);
    return true;
}
