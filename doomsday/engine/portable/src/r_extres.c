/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#define MAX_EXTENSIONS      (10)

#define DD_BASEDATAPATH     "data\\"

// TYPES -------------------------------------------------------------------

// Resource Class Flags (RCF):
#define RCF_USE_BASEDATAPATH (0x1)

typedef struct resclass_s {
    byte            flags; // RCF_* flags.
    const char*     defaultResourcePath;
    ddstring_t      path;
    filehash_t*     fileHash;
} resclass_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// The base directory for all resource directories.
static ddstring_t* dataPath = NULL;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// Recognized extensions (in order of importance).
static const char* typeExtension[NUM_RESOURCE_TYPES][MAX_EXTENSIONS] = {
    { "png", "tga", "pcx", NULL }, // Graphic, favor quality.
    { "dmd", "md2", NULL }, // Model, favour DMD over MD2.
    { "wav", NULL }, // Sound, only WAV files.
    { "ogg", "mp3", "wav", "mod", "mid", NULL } // Music
};

// Default resource classes for resource types.
static const ddresourceclass_t defResClassForType[NUM_RESOURCE_TYPES] = {
    DDRC_GRAPHICS,
    DDRC_MODEL,
    DDRC_SFX,
    DDRC_MUSIC
};

// Command line options for setting the path explicitly.
static const char* explicitOption[NUM_RESOURCE_CLASSES][2] = {
    { "-texdir",    "-texdir2" },
    { "-flatdir",   "-flatdir2" },
    { "-patdir",    "-patdir2" },
    { "-lmdir",     "-lmdir2" },
    { "-flaredir",  "-flaredir2" },
    { "-musdir",    "-musdir2" },
    { "-sfxdir",    "-sfxdir2" },
    { "-gfxdir",    "-gfxdir2" },
    { "-modeldir",  "-modeldir2" } // Additional paths, take precendence.
};

static resclass_t resClasses[NUM_RESOURCE_CLASSES] = {
    { 0, "textures\\" },
    { 0, "flats\\" },
    { 0, "patches\\" },
    { 0, "lightmaps\\" },
    { 0, "flares\\" },
    { 0, "music\\" },
    { 0, "sfx\\" },
    { RCF_USE_BASEDATAPATH, "graphics\\" },
    { 0, "models\\" }
};

static boolean inited = false;

// CODE --------------------------------------------------------------------

static void updateFileHash(resclass_t* info)
{
    if(!info->fileHash) // || rebuildHash
    {
        if(info->fileHash)
            FileHash_Destroy(info->fileHash);

        info->fileHash = FileHash_Create(Str_Text(&info->path));
        assert(info->fileHash);
    }
}

static void initDataPath(const char* baseDataPath)
{
    filename_t          filePath;

    if(!dataPath)
        dataPath = Str_New();

    M_TranslatePath(filePath, baseDataPath, FILENAME_T_MAXLEN);
    Dir_ValidDir(filePath, FILENAME_T_MAXLEN);

    Str_Set(dataPath, filePath);
}

/**
 * Set the data path. The game module is responsible for calling this.
 */
static void initClassDataPaths(void)
{
    int                 i;
    filename_t          filePath;
    // A string that identifies the game mode (e.g. doom2-plut).
    const char*         gameMode = gx.GetVariable(DD_GAME_MODE);

    VERBOSE(
    Con_Message("initDataPaths: %s\n", M_PrettyPath(Str_Text(dataPath))));

    // Update the paths of each class.
    for(i = 0; i < NUM_RESOURCE_CLASSES; ++i)
    {
        resclass_t*         rc = &resClasses[i];

        Str_Clear(&rc->path);

        if(ArgCheckWith(explicitOption[i][0], 1))
        {   // An explicit option specifies the path.
            M_TranslatePath(filePath, ArgNext(), FILENAME_T_MAXLEN);
        }
        else
        {
            // Build the path using the default elements.
            if(!(rc->flags & RCF_USE_BASEDATAPATH))
                strncpy(filePath, Str_Text(dataPath), FILENAME_T_MAXLEN);
            else
                strncpy(filePath, DD_BASEDATAPATH, FILENAME_T_MAXLEN);
            strncat(filePath, rc->defaultResourcePath, FILENAME_T_MAXLEN);
        }
        Dir_ValidDir(filePath, FILENAME_T_MAXLEN);

        //Str_Prepend(&rc->path, ";");
        Str_Prepend(&rc->path, filePath);

        if(!(rc->flags & RCF_USE_BASEDATAPATH) && gameMode && gameMode[0])
        {
            filename_t          other;

            dd_snprintf(other, FILENAME_T_MAXLEN, "%s%s", filePath, gameMode);
            Dir_ValidDir(other, FILENAME_T_MAXLEN);

            Str_Prepend(&rc->path, ";");
            Str_Prepend(&rc->path, other);
        }

        // The overriding path.
        if(ArgCheckWith(explicitOption[i][1], 1))
        {
            M_TranslatePath(filePath, ArgNext(), FILENAME_T_MAXLEN);
            Dir_ValidDir(filePath, FILENAME_T_MAXLEN);

            Str_Prepend(&rc->path, ";");
            Str_Prepend(&rc->path, filePath);

            if(!(rc->flags & RCF_USE_BASEDATAPATH) &&
               gameMode && gameMode[0])
            {
                filename_t          other;

                dd_snprintf(other, FILENAME_T_MAXLEN, "%s\\%s", filePath,
                         gameMode);
                Dir_ValidDir(other, FILENAME_T_MAXLEN);

                Str_Prepend(&rc->path, ";");
                Str_Prepend(&rc->path, other);
            }
        }

        if(verbose >= 2)
        {
            int                 n = 0;
            const char*         p = Str_Text(&rc->path);
            ddstring_t          path;

            Str_Init(&path);

            Con_Message("RC %i:\n", i);
            while((p = Str_CopyDelim(&path, p, ';')))
            {
                Con_Message("  %i \"%s\"\n", n++,
                            M_PrettyPath(Str_Text(&path)));
            }

            Str_Free(&path);
        }
    }
}

static boolean tryFindFile(const resclass_t* info, char* foundFileName,
                           const char* path, size_t len)
{
    if(info)
    {
        if(FileHash_Find(info->fileHash, foundFileName, path, len))
            return true;
    }
    else
    {
        if(F_Access(path))
        {
            if(foundFileName)
                strncpy(foundFileName, path, len);
            return true;
        }
    }

    return false;
}

/**
 * Check all possible extensions to see if the resource exists.
 *
 * @param path          File path to search for.
 *
 * @return              @c true, if it's found.
 */
static boolean tryResourceFile(resourcetype_t resType,
                               ddresourceclass_t resClass, const char* path,
                               char* foundFileName, size_t len)
{
    boolean             found = false;
    const resclass_t*   info = NULL;
    char*               ptr;

    // Do we need to rebuild a filehash?
    if(resClass != DDRC_NONE)
    {
        updateFileHash(&resClasses[resClass]);
        info = &resClasses[resClass];
    }

    // Has an extension been specified?
    ptr = M_FindFileExtension((char*)path);
    if(ptr && *ptr != '*') // Try this first.
        found = tryFindFile(info, foundFileName, path, len);

    if(!found)
    {
        int                 i;
        const char**        ext;
        ddstring_t          path2, tmp;

        Str_Init(&path2);
        Str_Init(&tmp);

        // Create a copy of the path minus file extension.
        if(ptr)
        {
            Str_PartAppend(&path2, path, 0, ptr - path);
        }
        else
        {
            Str_Set(&path2, path);
            Str_AppendChar(&path2, '.');
        }

        for(i = 0, ext = typeExtension[resType]; *ext; ext++)
        {
            Str_Copy(&tmp, &path2);
            Str_Appendf(&tmp, "%s", *ext);

            if((found = tryFindFile(info, foundFileName, Str_Text(&tmp),
                                    len)))
                break;
        }

        Str_Free(&path2);
        Str_Free(&tmp);
    }

    return found;
}

static boolean findResource(resourcetype_t resType,
                            ddresourceclass_t resClass, char* fileName,
                            const char* name, const char* optionalSuffix,
                            size_t len)
{
    boolean             found = false;

    // First try with the optional suffix.
    if(optionalSuffix)
    {
        ddstring_t*         fn = Str_New();
        char*               ptr, ext[10];

        // Has an extension been specified?
        ptr = M_FindFileExtension((char*)name);
        if(ptr && *ptr != '*')
        {
            strncpy(ext, ptr, 10);

            Str_PartAppend(fn, name, 0, ptr - 1 - name);
            Str_Append(fn, optionalSuffix);
            Str_Append(fn, ext);
        }
        else
        {
            Str_Set(fn, name);
            Str_Append(fn, optionalSuffix);
        }

        if(tryResourceFile(resType, resClass, Str_Text(fn), fileName, len))
            found = true;

        Str_Delete(fn);
    }

    // Try without a suffix.
    if(!found)
    {
        if(tryResourceFile(resType, resClass, name, fileName, len))
            found = true;
    }

    return found;
}

static boolean tryLocateResource(resourcetype_t resType,
                                 ddresourceclass_t resClass, char* fileName,
                                 const char* origName,
                                 const char* optionalSuffix, size_t len)
{
    ddstring_t          name;
    boolean             found;

    assert(inited);

    // Fix the directory seperators early so we don't need to many times
    // over, further down the line.
    Str_Init(&name);
    Str_Set(&name, origName);
    Dir_FixSlashes(Str_Text(&name), Str_Length(&name));

    // If this is an absolute path, locate using it.
    if(Dir_IsAbsolute(Str_Text(&name)))
    {
        found = findResource(resType, DDRC_NONE, fileName, Str_Text(&name),
                             optionalSuffix, len);
    }
    else
    {   // Else, prepend the base path and try that.
        ddstring_t          fn;
        const char*         path;

        // Make this an absolute, base-relative path.
        // If only checking the base path and not the expected location
        // for the resource type (below); re-use the current string.
        if(resClass != DDRC_NONE)
        {
            Str_Init(&fn);
            Str_Copy(&fn, &name);
            Str_Prepend(&fn, ddBasePath);
            path = Str_Text(&fn);
        }
        else
        {
            Str_Prepend(&name, ddBasePath);
            path = Str_Text(&name);
        }

        // Try loading using the base path as the starting point.
        found = findResource(resType, DDRC_NONE, fileName, path,
                             optionalSuffix, len);

        if(resClass != DDRC_NONE)
            Str_Free(&fn);
    }

    // Try expected location for this resource type and class?
    if(!found && resClass != DDRC_NONE)
        found = findResource(resType, resClass, fileName, Str_Text(&name),
                             optionalSuffix, len);

    Str_Free(&name);

    return found;
}

static void initClassData(void)
{
    int                 i;

    for(i = 0; i < NUM_RESOURCE_CLASSES; ++i)
    {
        resclass_t*         info = &resClasses[i];

        Str_Init(&info->path);
        info->fileHash = NULL;
    }
}

static void freeClassData(void)
{
    int                 i;

    for(i = 0; i < NUM_RESOURCE_CLASSES; ++i)
    {
        resclass_t*         info = &resClasses[i];

        Str_Free(&info->path);

        if(info->fileHash)
            FileHash_Destroy(info->fileHash);
        info->fileHash = NULL;
    }
}

/**
 * Set the initial path names.
 */
void R_InitResourceLocator(void)
{
    if(!inited)
    {
        initClassData();
        inited = true;
    }

    initClassDataPaths();
}

void R_ShutdownResourceLocator(void)
{
    if(!inited)
        return;

    freeClassData();

    Str_Free(dataPath);
    dataPath = NULL;

    inited = false;
}

/**
 * @return              Ptr to a string containing the general data path.
 */
const char* R_GetDataPath(void)
{
    return Str_Text(dataPath);
}

/**
 * Set the data path. The game module is responsible for calling this.
 */
void R_SetDataPath(const char* path)
{
    if(dataPath && !stricmp(path, Str_Text(dataPath)))
        return;

    // The base data path has changed, rebuild everything!
    initDataPath(path);

    freeClassData();
    initClassData();
}

/**
 * @param origPath      If a relative path, the data path is added in
 *                      front of it.
 */
void R_PrependDataPath(char* newPath, const char* origPath, size_t len)
{
    if(Dir_IsAbsolute(origPath))
    {
        // Can't prepend to absolute paths.
        strncpy(newPath, origPath, len);
    }
    else
    {
        dd_snprintf(newPath, len, "%s%s", Str_Text(dataPath), origPath);
    }
}

/**
 * Appends or prepends a new path to the list of resource search paths.
 */
void R_AddClassDataPath(ddresourceclass_t resClass, const char* addPath,
                        boolean append)
{
    resclass_t*         info;

    if(!addPath || !addPath[0] || !stricmp(addPath, DIR_SEP_STR))
        return;

    info = &resClasses[resClass];

    // Compile the new search path.
    if(append)
    {
        Str_Append(&info->path, ";");
        Str_Append(&info->path, addPath);
    }
    else
    {
        Str_Prepend(&info->path, ";");
        Str_Prepend(&info->path, addPath);
    }
}

void R_ClearClassDataPath(ddresourceclass_t resClass)
{
    resclass_t*         info = &resClasses[resClass];

    Str_Clear(&info->path);
}

/**
 * @return              Ptr to a string containing the model data path.
 */
const char* R_GetClassDataPath(ddresourceclass_t resClass)
{
    resclass_t*         info = &resClasses[resClass];

    return Str_Text(&info->path);
}

/**
 * Attempt to locate an external file for the specified resource.
 *
 * @param resType       Type of resource being searched for (if known).
 * @param resClass      Class specifier; alters search behavior including
 *                      locations to be searched.
 * @param fileName      If a file is found, the fully qualified path will
 *                      be written back to here.
 *                      Can be @c NULL, which makes the routine just check
 *                      for the existence of the file.
 * @param name          Name of the resource being searched for.
 * @param optionalSuffix An optional name suffix. If not @c NULL, append to
 *                      @p name and look for matches. If not found or not
 *                      specified then search for matches to @p name.
 * @param len           Size of @p fileName in bytes.
 *
 * @return              @c true, iff a file was found.
 */
boolean R_FindResource2(resourcetype_t resType, ddresourceclass_t resClass,
                        char* fileName, const char* name,
                        const char* optionalSuffix, size_t len)
{
    if(resType < RT_FIRST || resType >= NUM_RESOURCE_TYPES)
        Con_Error("R_FindResource: Invalid resource type %i.\n", resType);

    if(resClass != DDRC_NONE && // No resource class means use the base path.
       (resClass < DDRC_FIRST || resClass >= NUM_RESOURCE_CLASSES))
        Con_Error("R_FindResource: Invalid resource class %i.\n", resClass);

    return tryLocateResource(resType, resClass, fileName, name,
                             optionalSuffix, len);
}

/**
 * Same as R_FindResource2 except that the resource class is chosen
 * automatically, using a set of logical defaults.
 */
boolean R_FindResource(resourcetype_t resType, char* fileName,
                       const char* name, const char* optionalSuffix,
                       size_t len)
{
    if(resType < RT_FIRST || resType >= NUM_RESOURCE_TYPES)
        Con_Error("R_FindResource: Invalid resource type %i.\n", resType);

    return tryLocateResource(resType, defResClassForType[resType], fileName,
                             name, optionalSuffix, len);
}
