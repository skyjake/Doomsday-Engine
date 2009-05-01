/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
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
 * r_extres.c: External Resources.
 *
 * Routines for locating external resource files.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_system.h"
#include "de_console.h"
#include "de_refresh.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

#define MAX_EXTENSIONS      10

// TYPES -------------------------------------------------------------------

typedef struct resclass_s {
    char    path[256];
    char    overridePath[256];
} resclass_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// The base directory for all resource directories.
char    dataPath[256];

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// Command line options for setting the path explicitly.
static char *explicitOption[NUM_RESOURCE_CLASSES][2] = {
    {"-texdir", "-texdir2"},
    {"-flatdir", "-flatdir2"},
    {"-patdir", "-patdir2"},
    {"-lmdir", "-lmdir2"},
    {"-flaredir", "-flaredir2"},
    {"-musdir", "-musdir2"},
    {"-sfxdir", "-sfxdir2"},
    {"-gfxdir", "-gfxdir2"}
};

// Class paths.
static const char *defaultResourcePath[NUM_RESOURCE_CLASSES] = {
    "Textures\\",
    "Flats\\",
    "Patches\\",
    "LightMaps\\",
    "Flares\\",
    "Music\\",
    "Sfx\\",
    "Graphics\\"
};

// Recognized extensions (in order of importance). "*" means 'anything'.
static const char *classExtension[NUM_RESOURCE_CLASSES][MAX_EXTENSIONS] = {
    // Graphics favor quality.
    {".png", ".tga", ".pcx", NULL},
    {".png", ".tga", ".pcx", NULL},
    {".png", ".tga", ".pcx", NULL},
    {".png", ".tga", ".pcx", NULL},
    {".png", ".tga", ".pcx", NULL},

    // Extension doesn't matter with music.
    {".ogg", ".mp3", ".wav", ".mod", ".mid", NULL},

    // Only WAV files for sound effects.
    {".wav", NULL},

    {".png", ".tga", ".pcx", NULL}
};

static resclass_t resClasses[NUM_RESOURCE_CLASSES];

// CODE --------------------------------------------------------------------

/**
 * Set the initial path names.
 */
void R_InitExternalResources(void)
{
    R_InitDataPaths("}Data\\", false);
}

/**
 * @return              Ptr to a string containing the general data path.
 */
const char *R_GetDataPath(void)
{
    return dataPath;
}

/**
 * Set the data path. The game module is responsible for calling this.
 */
void R_SetDataPath(const char *path)
{
    R_InitDataPaths(path, true);
}

/**
 * Set the data path. The game module is responsible for calling this.
 */
void R_InitDataPaths(const char *path, boolean justGamePaths)
{
    int         i;

    M_TranslatePath(path, dataPath);
    Dir_ValidDir(dataPath);
    VERBOSE(Con_Message("R_SetDataPath: %s\n", M_PrettyPath(dataPath)));

    // Update the paths of each class.
    for(i = 0; i < NUM_RESOURCE_CLASSES; ++i)
    {
        // The Graphics class resources are under Doomsday's control.
        if(justGamePaths && i == RC_GRAPHICS)
            continue;

        memset(&resClasses[i], 0, sizeof(resClasses[i]));

        // The -texdir option specifies a path to look for TGA textures.
        if(ArgCheckWith(explicitOption[i][0], 1))
        {
            M_TranslatePath(ArgNext(), resClasses[i].path);
        }
        else
        {
            // Build the path for the resource class using the default
            // elements.
            strcpy(resClasses[i].path, dataPath);
            strcat(resClasses[i].path, defaultResourcePath[i]);
        }

        Dir_ValidDir(resClasses[i].path);

        // The overriding path.
        if(ArgCheckWith(explicitOption[i][1], 1))
        {
            M_TranslatePath(ArgNext(), resClasses[i].overridePath);
        }
        Dir_ValidDir(resClasses[i].overridePath);

        VERBOSE2(Con_Message
                 ("  %i: %s (%s)\n", i, M_PrettyPath(resClasses[i].path),
                  M_PrettyPath(resClasses[i].overridePath)));
    }
}

/**
 * @param origPath      If a relative path, the data path is added in
 *                      front of it.
 */
void R_PrependDataPath(const char *origPath, char *newPath)
{
    char        buf[300];

    if(Dir_IsAbsolute(origPath))
    {
        // Can't prepend to absolute paths.
        strcpy(newPath, origPath);
    }
    else
    {
        sprintf(buf, "%s%s", dataPath, origPath);
        strcpy(newPath, buf);
    }
}

/**
 * Callback function used in R_TryResourceFile.
 */
int R_FileFinder(const char *fn, filetype_t type, void *buf)
{
    // Skip directories.
    if(type == FT_DIRECTORY)
        return true;

    // This'll do fine!
    if(buf)
        strcpy(buf, fn);

    // Return false to stop searching.
    return false;
}

/**
 * Check all possible extensions to see if the resource exists.
 *
 * @param path          Is an absolute path to the file, sans extension.
 *
 * @return              @c true, if it's found.
 */
boolean R_TryResourceFile(resourceclass_t resClass, const char *path,
                          char *foundFileName)
{
    const char **ext;
    char        buf[256], *extPlace;
    int         i;

    strcpy(buf, path);
    extPlace = buf + strlen(buf);

    for(i = 0, ext = classExtension[resClass]; *ext; ext++)
    {
        if(**ext == '*') // Anything goes?
        {
            strcpy(extPlace, ".*");
            if(!F_ForAll(buf, foundFileName, R_FileFinder))
            {   // A match was found.
                return true;
            }
        }
        else
        {
            strcpy(extPlace, *ext);
            if(F_Access(buf))
            {   // Found it.
                if(foundFileName)
                    strcpy(foundFileName, buf);
                return true;
            }
        }
    }

    // No hits.
    return false;
}

/**
 * Attempt to locate an external file for the specified resource. Returns
 * true if successful. The file name is returned in the fileName parameter.
 * The fileName parm can be NULL, which makes the routine just check for
 * the existence of the file. Set optionalSuffix to NULL if it's not
 * needed.
 */
boolean R_FindResource(resourceclass_t resClass, const char *name,
                       const char *optionalSuffix, char *fileName)
{
    resclass_t *info = &resClasses[resClass];
    const char *gameMode;
    char    path[256];
    char    fn[256];
    int     i;

    // The tries:
    // 0. override + game
    // 1. override
    // 2. game
    // 3. default
    // 4. base path
    for(i = 0; i <= 4; i++)
    {
        // First try the overriding path, if it's set.
        if(i < 2)
        {
            if(info->overridePath[0])
                strcpy(path, info->overridePath);
            else
                continue;
        }
        else
        {
            if(i == 4)
            {
                // Begin with the base path.
                strcpy(path, ddBasePath);
            }
            else
            {
                strcpy(path, info->path);
            }
        }

        // Should the game mode subdir be included?
        if(i == 0 || i == 2)
        {
            // A string that identifies the game mode (e.g. doom2-plut).
            gameMode = gx.GetVariable(DD_GAME_MODE);
            if(!gameMode || !gameMode[0])
                continue;       // Not set?!
            strcat(path, gameMode);
            strcat(path, "\\");
        }

        // First try with the optional suffix.
        if(optionalSuffix)
        {
            strcpy(fn, path);
            strcat(fn, name);
            strcat(fn, optionalSuffix);
            if(R_TryResourceFile(resClass, fn, fileName))
                return true;
        }

        // Try without a suffix.
        strcpy(fn, path);
        strcat(fn, name);
        if(R_TryResourceFile(resClass, fn, fileName))
            return true;
    }

    // Couldn't find anything.
    VERBOSE2(Con_Message("Failed to locate high resolution replacement resource for: %s\n", name));
    return false;
}
