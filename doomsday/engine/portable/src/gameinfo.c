/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2010 Daniel Swanson <danij@dengine.net>
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

#include "de_base.h"
#include "de_console.h"
#include "de_misc.h"

#include "gameinfo.h"

#define MAX_SEARCHPATHS         (6)

/**
 * Resource Class Path (RCP) identifiers.
 *
 * @ingroup fs
 */
typedef enum {
    RCP_NONE = 0, // Not a real path.
    RCP_FIRST = 1,
    RCP_BASEPATH = RCP_FIRST, // i.e., "}"
    RCP_BASEPATH_DATA, // i.e., "}data/"
    RCP_BASEPATH_DEFS, // i.e., "}defs/"
    RCP_GAMEPATH_DATA, // e.g., "}data/jdoom/"
    RCP_GAMEPATH_DEFS, // e.g., "}defs/jdoom/"
    RCP_GAMEMODEPATH_DATA, // e.g., "}data/jdoom/doom2-plut/"
    RCP_GAMEMODEPATH_DEFS, // e.g., "}defs/jdoom/doom2-plut/" 
    RCP_DOOMWADDIR // any valid absolute or relative path
} resourceclasspath_t;

// Command line options for setting the resource path explicitly.
// Additional paths, take precendence.
static const char* resourceClassPathOverrides[NUM_RESOURCE_CLASSES][2] = {
    { 0,            0 },
    { 0,            0 },
    { 0,            0 },
    { "-texdir",    "-texdir2" },
    { "-flatdir",   "-flatdir2" },
    { "-patdir",    "-patdir2" },
    { "-lmdir",     "-lmdir2" },
    { "-flaredir",  "-flaredir2" },
    { "-musdir",    "-musdir2" },
    { "-sfxdir",    "-sfxdir2" },
    { "-gfxdir",    "-gfxdir2" },
    { "-modeldir",  "-modeldir2" }
};

static const char* resourceClassDefaultPaths[NUM_RESOURCE_CLASSES] = {
    { 0 },
    { 0 },
    { 0 },
    { "textures\\" },
    { "flats\\" },
    { "patches\\" },
    { "lightmaps\\" },
    { "flares\\" },
    { "music\\" },
    { "sfx\\" },
    { "graphics\\" },
    { "models\\" }
};

// Resource locator search order (in order of least-importance, left to right).
static const resourceclasspath_t resourceClassPathSearchOrder[NUM_RESOURCE_CLASSES][MAX_SEARCHPATHS] = {
    { RCP_DOOMWADDIR, RCP_GAMEPATH_DATA, 0 },
    { RCP_DOOMWADDIR, RCP_GAMEPATH_DATA, 0 },
    { RCP_BASEPATH_DEFS, RCP_GAMEPATH_DEFS, RCP_GAMEMODEPATH_DEFS, 0 },
    { RCP_GAMEPATH_DATA, RCP_GAMEMODEPATH_DATA, 0 },
    { RCP_GAMEPATH_DATA, RCP_GAMEMODEPATH_DATA, 0 },
    { RCP_GAMEPATH_DATA, RCP_GAMEMODEPATH_DATA, 0 },
    { RCP_GAMEPATH_DATA, RCP_GAMEMODEPATH_DATA, 0 },
    { RCP_GAMEPATH_DATA, RCP_GAMEMODEPATH_DATA, 0 },
    { RCP_GAMEPATH_DATA, RCP_GAMEMODEPATH_DATA, 0 },
    { RCP_GAMEPATH_DATA, RCP_GAMEMODEPATH_DATA, 0 },
    { RCP_BASEPATH_DATA, 0 },
    { RCP_GAMEPATH_DATA, RCP_GAMEMODEPATH_DATA, 0 }
};

static __inline size_t countElements(gameresource_record_t* const* list)
{
    size_t n = 0;
    if(list)
        while(list[n++]);
    return n;
}

static void buildResourceClassPathList(gameinfo_t* info, ddresourceclass_t rc)
{
    assert(info && VALID_RESOURCE_CLASS(rc));
    {
    ddstring_t* pathList = &info->_searchPathLists[rc];
    boolean usingGameModePathData = false;
    boolean usingGameModePathDefs = false;

    Str_Clear(pathList);

    if(resourceClassPathOverrides[(int)rc][0] && ArgCheckWith(resourceClassPathOverrides[(int)rc][0], 1))
    {   // A command line override of the default path.
        filename_t newPath;
        M_TranslatePath(newPath, ArgNext(), FILENAME_T_MAXLEN);
        Dir_ValidDir(newPath, FILENAME_T_MAXLEN);
        Str_Prepend(pathList, ";"); Str_Prepend(pathList, newPath);
    }

    { int i;
    for(i = 0; resourceClassPathSearchOrder[(int)rc][i] != RCP_NONE; ++i)
    {
        switch(resourceClassPathSearchOrder[(int)rc][i])
        {
        case RCP_BASEPATH:
            { filename_t newPath;
            M_TranslatePath(newPath, "}", FILENAME_T_MAXLEN);
            GameInfo_AddResourceSearchPath(info, rc, newPath, false);
            break;
            }
        case RCP_BASEPATH_DATA:
            { filename_t newPath;
            if(resourceClassDefaultPaths[(int)rc])
            {
                filename_t other;
                dd_snprintf(other, FILENAME_T_MAXLEN, DD_BASEPATH_DATA"%s", resourceClassDefaultPaths[(int)rc]);
                M_TranslatePath(newPath, other, FILENAME_T_MAXLEN);
            }
            else
            {
                M_TranslatePath(newPath, DD_BASEPATH_DATA, FILENAME_T_MAXLEN);
            }
            GameInfo_AddResourceSearchPath(info, rc, newPath, false);
            break;
            }
        case RCP_BASEPATH_DEFS:
            { filename_t newPath;
            if(resourceClassDefaultPaths[(int)rc])
            {
                filename_t other;
                dd_snprintf(other, FILENAME_T_MAXLEN, DD_BASEPATH_DEFS"%s", resourceClassDefaultPaths[(int)rc]);
                M_TranslatePath(newPath, other, FILENAME_T_MAXLEN);
            }
            else
            {
                M_TranslatePath(newPath, DD_BASEPATH_DEFS, FILENAME_T_MAXLEN);
            }
            GameInfo_AddResourceSearchPath(info, rc, newPath, false);
            break;
            }
        case RCP_GAMEPATH_DATA:
            if(resourceClassDefaultPaths[(int)rc])
            {
                filename_t newPath;
                dd_snprintf(newPath, FILENAME_T_MAXLEN, "%s%s", Str_Text(&info->_dataPath), resourceClassDefaultPaths[(int)rc]);
                GameInfo_AddResourceSearchPath(info, rc, newPath, false);
            }
            else
            {
                GameInfo_AddResourceSearchPath(info, rc, Str_Text(&info->_dataPath), false);
            }
            break;
        case RCP_GAMEPATH_DEFS:
            if(resourceClassDefaultPaths[(int)rc])
            {
                filename_t newPath;
                dd_snprintf(newPath, FILENAME_T_MAXLEN, "%s%s", Str_Text(&info->_defsPath), resourceClassDefaultPaths[(int)rc]);
                GameInfo_AddResourceSearchPath(info, rc, newPath, false);
            }
            else
            {
                GameInfo_AddResourceSearchPath(info, rc, Str_Text(&info->_defsPath), false);
            }
            break;
        case RCP_GAMEMODEPATH_DATA:
            usingGameModePathData = true;
            if(resourceClassDefaultPaths[(int)rc] && Str_Length(&info->_identityKey))
            {
                filename_t newPath;
                dd_snprintf(newPath, FILENAME_T_MAXLEN, "%s%s%s", Str_Text(&info->_dataPath), resourceClassDefaultPaths[(int)rc], Str_Text(&info->_identityKey));
                GameInfo_AddResourceSearchPath(info, rc, newPath, false);
            }
            break;
        case RCP_GAMEMODEPATH_DEFS:
            usingGameModePathDefs = true;
            if(resourceClassDefaultPaths[(int)rc] && Str_Length(&info->_identityKey))
            {
                filename_t newPath;
                dd_snprintf(newPath, FILENAME_T_MAXLEN, "%s%s%s", Str_Text(&info->_defsPath), resourceClassDefaultPaths[(int)rc], Str_Text(&info->_identityKey));
                GameInfo_AddResourceSearchPath(info, rc, newPath, false);
            }
            break;
        case RCP_DOOMWADDIR:
            if(!ArgCheck("-nowaddir") && getenv("DOOMWADDIR"))
            {
                filename_t newPath;
                M_TranslatePath(newPath, getenv("DOOMWADDIR"), FILENAME_T_MAXLEN);
                GameInfo_AddResourceSearchPath(info, rc, newPath, false);
            }
            break;

        default: Con_Error("collateResourceClassPathSet: Invalid path ident %(int)rc.", (int)resourceClassPathSearchOrder[(int)rc][i]);
        };
    }}

    // The overriding path.
    if(resourceClassPathOverrides[(int)rc][1] && ArgCheckWith(resourceClassPathOverrides[(int)rc][1], 1))
    {
        filename_t newPath;
        M_TranslatePath(newPath, ArgNext(), FILENAME_T_MAXLEN);
        GameInfo_AddResourceSearchPath(info, rc, newPath, false);

        if((usingGameModePathData || usingGameModePathDefs) && Str_Length(&info->_identityKey))
        {
            filename_t other;
            dd_snprintf(other, FILENAME_T_MAXLEN, "%s\\%s", newPath, Str_Text(&info->_identityKey));
            GameInfo_AddResourceSearchPath(info, rc, other, false);
        }
    }
    }
}

static void collateResourceClassPathSet(gameinfo_t* info)
{
    assert(info);
    { int i;
    for(i = (int)DDRC_FIRST; i < NUM_RESOURCE_CLASSES; ++i)
    {
        buildResourceClassPathList(info, (ddresourceclass_t)i);
    }}
}

static __inline void clearResourceClassSearchPathList(gameinfo_t* info, ddresourceclass_t resClass)
{
    assert(info && VALID_RESOURCE_CLASS(resClass));
    Str_Free(&info->_searchPathLists[resClass]);
}

gameinfo_t* P_CreateGameInfo(pluginid_t pluginId, const char* identityKey, const char* dataPath,
    const char* defsPath, const ddstring_t* mainDef, const char* title, const char* author, const ddstring_t* cmdlineFlag,
    const ddstring_t* cmdlineFlag2)
{
    gameinfo_t* info = M_Malloc(sizeof(*info));

    info->_pluginId = pluginId;

    Str_Init(&info->_identityKey);
    if(identityKey)
        Str_Set(&info->_identityKey, identityKey);

    Str_Init(&info->_dataPath);
    if(dataPath)
        Str_Set(&info->_dataPath, dataPath);

    Str_Init(&info->_defsPath);
    if(defsPath)
        Str_Set(&info->_defsPath, defsPath);

    Str_Init(&info->_mainDef);
    if(mainDef)
        Str_Copy(&info->_mainDef, mainDef);

    Str_Init(&info->_title);
    if(title)
        Str_Set(&info->_title, title);

    Str_Init(&info->_author);
    if(author)
        Str_Set(&info->_author, author);

    if(cmdlineFlag)
    {
        info->_cmdlineFlag = Str_New();
        Str_Copy(info->_cmdlineFlag, cmdlineFlag);
    }
    else
        info->_cmdlineFlag = 0;

    if(cmdlineFlag2)
    {
        info->_cmdlineFlag2 = Str_New();
        Str_Copy(info->_cmdlineFlag2, cmdlineFlag2);
    }
    else
        info->_cmdlineFlag2 = 0;

    { size_t i;
    for(i = 0; i < NUM_RESOURCE_CLASSES; ++i)
    {
        info->_requiredResources[i] = 0;
        Str_Init(&info->_searchPathLists[i]);
    }}

    collateResourceClassPathSet(info);
    return info;
}

void P_DestroyGameInfo(gameinfo_t* info)
{
    assert(info);

    GameInfo_ClearResourceSearchPaths(info);

    Str_Free(&info->_identityKey);
    Str_Free(&info->_dataPath);
    Str_Free(&info->_defsPath);
    Str_Free(&info->_mainDef);
    Str_Free(&info->_title);
    Str_Free(&info->_author);

    if(info->_cmdlineFlag) { Str_Delete(info->_cmdlineFlag);  info->_cmdlineFlag  = 0; }
    if(info->_cmdlineFlag2){ Str_Delete(info->_cmdlineFlag2); info->_cmdlineFlag2 = 0; }

    { size_t i;
    for(i = 0; i < NUM_RESOURCE_CLASSES; ++i)
    {
        if(!info->_requiredResources[i])
            continue;

        { gameresource_record_t** rec;
        for(rec = info->_requiredResources[i]; *rec; rec++)
        {
            Str_Free(&(*rec)->name);
            Str_Free(&(*rec)->path);
            if((*rec)->lumpNames)
            {
                size_t j;
                for(j = 0; (*rec)->lumpNames[j]; ++j)
                    Str_Delete((*rec)->lumpNames[j]);
                M_Free((*rec)->lumpNames); (*rec)->lumpNames = 0;
            }
            M_Free(*rec);
        }}
        M_Free(info->_requiredResources[i]);
    }}

    M_Free(info);
}

gameresource_record_t* GameInfo_AddResource(gameinfo_t* info, resourcetype_t resType, ddresourceclass_t resClass, const char* name)
{
    assert(info && VALID_RESOURCE_TYPE(resType) && VALID_RESOURCE_CLASS(resClass) && name && name[0]);
    {
    size_t num = countElements(info->_requiredResources[resClass]);
    gameresource_record_t* rec;

    info->_requiredResources[resClass] = M_Realloc(info->_requiredResources[resClass], sizeof(*info->_requiredResources[resClass]) * MAX_OF(num+1, 2));
    rec = info->_requiredResources[resClass][num!=0?num-1:0] = M_Malloc(sizeof(*rec));
    info->_requiredResources[resClass][num!=0?num:1] = 0; // Terminate.

    rec->resType = resType;
    rec->resClass = resClass;
    rec->lumpNames = 0;
    Str_Init(&rec->name); Str_Set(&rec->name, name);
    Str_Init(&rec->path);
    return rec;
    }
}

void GameInfo_ClearResourceSearchPaths2(gameinfo_t* info, ddresourceclass_t resClass)
{
    assert(info);
    if(resClass == NUM_RESOURCE_CLASSES)
    {
        int i;
        for(i = 0; i < NUM_RESOURCE_CLASSES; ++i)
        {
            clearResourceClassSearchPathList(info, (ddresourceclass_t)i);
        }
        return;
    }
    clearResourceClassSearchPathList(info, resClass);
}

void GameInfo_ClearResourceSearchPaths(gameinfo_t* info)
{
    GameInfo_ClearResourceSearchPaths2(info, NUM_RESOURCE_CLASSES);
}

boolean GameInfo_AddResourceSearchPath(gameinfo_t* info, ddresourceclass_t resClass,
    const char* newPath, boolean append)
{
    assert(info && VALID_RESOURCE_CLASS(resClass));
    {
    ddstring_t* pathList;
    filename_t absNewPath;

    if(!newPath || !newPath[0] || !stricmp(newPath, DIR_SEP_STR))
        return false; // Not suitable.

    // Convert all slashes to the host OS's directory separator,
    // for compatibility with the sys_filein routines.
    strncpy(absNewPath, newPath, FILENAME_T_MAXLEN);
    Dir_ValidDir(absNewPath, FILENAME_T_MAXLEN);
    M_PrependBasePath(absNewPath, absNewPath, FILENAME_T_MAXLEN);

    // Have we seen this path already?
    pathList = &info->_searchPathLists[resClass];
    if(Str_Length(pathList))
    {
        const char* p = Str_Text(pathList);
        ddstring_t curPath;
        boolean ignore = false;

        Str_Init(&curPath);
        while(!ignore && (p = Str_CopyDelim(&curPath, p, ';')))
        {
            if(!Str_CompareIgnoreCase(&curPath, absNewPath))
                ignore = true;
        }
        Str_Free(&curPath);

        if(ignore) return true; // We don't want duplicates.
    }
    
    // Append the new search path.
    if(append)
    {
        Str_Append(pathList, absNewPath);
        Str_Append(pathList, ";");
    }
    else
    {
        Str_Prepend(pathList, ";");
        Str_Prepend(pathList, absNewPath);
    }
    return true;
    }
}

const ddstring_t* GameInfo_ResourceSearchPaths(gameinfo_t* info, ddresourceclass_t resClass)
{
    assert(info && VALID_RESOURCE_CLASS(resClass));
    return &info->_searchPathLists[resClass];
}

pluginid_t GameInfo_PluginId(gameinfo_t* info)
{
    assert(info);
    return info->_pluginId;
}

const ddstring_t* GameInfo_IdentityKey(gameinfo_t* info)
{
    assert(info);
    return &info->_identityKey;
}

const ddstring_t* GameInfo_DataPath(gameinfo_t* info)
{
    assert(info);
    return &info->_dataPath;
}

const ddstring_t* GameInfo_DefsPath(gameinfo_t* info)
{
    assert(info);
    return &info->_defsPath;
}

const ddstring_t* GameInfo_MainDef(gameinfo_t* info)
{
    assert(info);
    return &info->_mainDef;
}

const ddstring_t* GameInfo_CmdlineFlag(gameinfo_t* info)
{
    assert(info);
    return info->_cmdlineFlag;
}

const ddstring_t* GameInfo_CmdlineFlag2(gameinfo_t* info)
{
    assert(info);
    return info->_cmdlineFlag2;
}

const ddstring_t* GameInfo_Title(gameinfo_t* info)
{
    assert(info);
    return &info->_title;
}

const ddstring_t* GameInfo_Author(gameinfo_t* info)
{
    assert(info);
    return &info->_author;
}

gameresource_record_t* const* GameInfo_Resources(gameinfo_t* info, ddresourceclass_t resClass, size_t* count)
{
    assert(info && VALID_RESOURCE_CLASS(resClass));
    if(count)
        *count = countElements(info->_requiredResources[resClass]);
    return info->_requiredResources[resClass];
}
