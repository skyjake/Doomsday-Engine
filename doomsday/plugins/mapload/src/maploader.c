/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2005-2006 Daniel Swanson <danij@dengine.net>
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

/*
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

typedef struct {
    char    identification[4];
    int     numlumps;
    int     infotableofs;
} wadheader_t;

typedef struct {
    int     filepos;
    size_t  size;
    char    name[8];
} wadlump_t;

static int LoadLumpsHook(int hookType, int parm, void *data);

static const char *bspDir = "bspcache\\";

/**
 * This function is called automatically when the plugin is loaded.
 * We let the engine know what we'd like to do.
 */
void DP_Initialize(void)
{
    Plug_AddHook(HOOK_LOAD_MAP_LUMPS, LoadLumpsHook);
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
    // Hacks'n'Kludges Illustrated: Accessing the Game Mode String!
    game_export_t *gex = (game_export_t*) DD_GetVariable(DD_GAME_EXPORTS);
    const char *sourceFile = W_LumpSourceFile(mainLump);
    filename_t base;
    unsigned short identifier = 0;
    int i;

    M_ExtractFileBase(sourceFile, base);

    for(i = 0; sourceFile[i]; ++i)
        identifier ^= sourceFile[i] << ((i * 3) % 11);

    // The work directory path is relative to the runtime directory.
    sprintf(dir, "%s%s\\%s-%04X\\", bspDir,
            (const char*) gex->GetVariable(DD_GAME_MODE),
            base, identifier);

    M_TranslatePath(dir, dir);
    //Con_Message("BSP work: %s\n", dir);
}

/**
 * Returns true if the lump is a map data lump.
 */
static boolean IsMapLump(int lump)
{
    const char *levelLumps[] = {
        "THINGS", "LINEDEFS", "SIDEDEFS", "VERTEXES", "SEGS",
        "SSECTORS", "NODES", "SECTORS", "REJECT", "BLOCKMAP",
        "BEHAVIOR", "SCRIPTS", NULL
    };
    const char *name = W_LumpName(lump);
    int i;

    if(!name)
        return false;

    for(i = 0; levelLumps[i]; ++i)
    {
        if(!strcmp(name, levelLumps[i]))
            return true;
    }
    return false;
}

/**
 * Write all the lumps of the specified map to a WAD file.
 */
static void DumpMap(int mainLump, const char *fileName)
{
#define MAX_MAP_LUMPS 12
    FILE *file;
    wadheader_t header;
    wadlump_t lumps[MAX_MAP_LUMPS];
    int i;
    char *buffer;

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
    for(i = 1; i < MAX_MAP_LUMPS; ++i)
    {
        // Is this still a map lump?
        if(!IsMapLump(mainLump + i))
            break;

        strncpy(lumps[i].name, W_LumpName(mainLump + i), 8);
        lumps[i].filepos = LONG(ftell(file));
        lumps[i].size = LONG(W_LumpLength(mainLump + i));

        buffer = malloc(LONG(lumps[i].size));
        W_ReadLump(mainLump + i, buffer);
        fwrite(buffer, LONG(lumps[i].size), 1, file);
        free(buffer);
    }

    header.numlumps = LONG(i);
    header.infotableofs = LONG(ftell(file));

    // Write the directory.
    fwrite(lumps, sizeof(wadlump_t), i, file);

    // Rewrite the updated header.
    fseek(file, 0, SEEK_SET);
    fwrite(&header, sizeof(header), 1, file);
    fclose(file);
}

/**
 * Fatal errors are called as a last resort when something serious
 * goes wrong, e.g. out of memory.  This routine should show the
 * error to the user and abort the program.
 */
static void fatal_error(const char *str, ...)
{
    char buffer[2000];
    va_list args;

    va_start(args, str);
    vsnprintf(buffer, sizeof(buffer), str, args);
    va_end(args);

    Con_Error(buffer);
}

/**
 * The print_msg routine is used to display the various messages
 * that occur, e.g. "Building GL nodes on MAP01" and that kind of
 * thing.
 */
static void print_msg(const char *str, ...)
{
    char buffer[2000];
    va_list args;

    va_start(args, str);
    vsnprintf(buffer, sizeof(buffer), str, args);
    va_end(args);

    Con_Message(buffer);
}

/**
 * This routine is called frequently whilst building the nodes, and
 * can be used to keep a GUI responsive to user input.  Many
 * toolkits have a "do iteration" or "check events" type of function
 * that this can call.  Avoid anything that sleeps though, or it'll
 * slow down the build process unnecessarily.
 */
static void ticker(void)
{
}

/**
 * These display routines is used for tasks that can show a progress
 * bar, namely: building nodes, loading the wad, and saving the wad.
 * The command line version could show a percentage value, or even
 * draw a bar using characters.
 *
 * Display_open is called at the beginning, and `type' holds the
 * type of progress (and determines how many bars to display).
 * Returns TRUE if all went well, or FALSE if it failed (in which
 * case the other routines should do nothing when called).
 */
static boolean_g display_open(displaytype_e type)
{
    return FALSE;
}

/**
 * For GUI versions this can be used to set the title of the
 * progress window.  OK to ignore it (e.g. command line version).
 */
static void display_setTitle(const char *str)
{
}

/**
 * The next three routines control the appearance of each progress
 * bar.  Display_setBarText is called to change the message above
 * the bar.  Display_setBarLimit sets the integer limit of the
 * progress (the target value), and display_setBar sets the current
 * value (which will count up from 0 to the limit, inclusive).
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
 * The display_close routine is called when the task is finished,
 * and should remove the progress indicator/window from the screen.
 */
static void display_close(void)
{
}


/**
 * This function is called when Doomsday is loading a map.  'parm' is
 * the index number of the map lump identifier.  'data' points to an
 * array of integers, which will be used to return the lump numbers
 * for the data (normal + GL).
 */
static int LoadLumpsHook(int hookType, int parm, void *data)
{
    int *returnedLumps = (int*) data;
    char glLumpName[20];
    filename_t workDir;
    filename_t mapDataFile;
    filename_t bspDataFile;
    uint sourceTime;
    uint bspTime;

    // The bsp-build cvar determines whether we'll try to load
    // existing data (from a GWA) or build the nodes ourselves (and
    // cache them for later).
    if(!Con_GetInteger("bsp-build"))
    {
        // We shouldn't build anything.  Let's see if GL data is
        // already loaded.
        returnedLumps[0] = parm;
        sprintf(glLumpName, "GL_%s", W_LumpName(parm));
        returnedLumps[1] = W_CheckNumForName(glLumpName);

        // Determine if the GL data was loaded before the map, which
        // would indicate that it belongs to some other map.
        if(returnedLumps[1] < parm)
        {
            returnedLumps[1] = -1;
        }
        return true;
    }

    GetWorkDir(workDir, parm);

    // Make sure the work directory exists.
    M_CheckPath(workDir);

    // The source data must not be newer than the BSP data.
    sourceTime = F_LastModified(W_LumpSourceFile(parm));

    // First test if we already have valid cached BSP data.
    sprintf(mapDataFile, "%s%s", workDir, W_LumpName(parm));
    M_TranslatePath(mapDataFile, mapDataFile);
    strcpy(bspDataFile, mapDataFile);
    strcat(mapDataFile, ".wad");
    strcat(bspDataFile, ".gwa");

    bspTime = F_LastModified(bspDataFile);

    //Con_Message("source=%i, bsp=%i\n", sourceTime, bspTime);

    if(!Con_GetInteger("bsp-cache") || !F_Access(bspDataFile) ||
       bspTime < sourceTime)
    {
        nodebuildinfo_t info;
        nodebuildfuncs_t funcs;
        nodebuildcomms_t comms;

        memset(&info, 0, sizeof(info));
        memset(&funcs, 0, sizeof(funcs));
        memset(&comms, 0, sizeof(comms));

        // Rebuild the cached data.
        DumpMap(parm, mapDataFile);

        info.input_file = mapDataFile;
        info.output_file = bspDataFile;
        info.factor = Con_GetInteger("bsp-factor");
        info.no_progress = TRUE;
        info.gwa_mode = TRUE;
        info.no_prune = TRUE;
        info.block_limit = 44000; // not used?

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

        // Remove the dumped map data.
        remove(mapDataFile);
    }

    // Load the cached data.  The lumps are loaded into the auxiliary
    // lump cache, which means they use special index numbers and will
    // be automatically deleted when the next map is loaded.

    returnedLumps[0] = parm; // normal map data (original)
    returnedLumps[1] = W_OpenAuxiliary(bspDataFile); // GL data
    return true;
}
