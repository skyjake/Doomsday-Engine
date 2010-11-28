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
#include "dd_main.h"
#include "de_console.h"
#include "de_misc.h"

#include "gameinfo.h"

/**
 * Search Path Identifiers.
 *
 * @ingroup fs
 */
typedef enum {
    SPI_NONE = 0, // Not a real path.
    SPI_FIRST = 1,
    SPI_BASEPATH = SPI_FIRST, // i.e., "}"
    SPI_BASEPATH_DATA, // i.e., "}data/"
    SPI_BASEPATH_DEFS, // i.e., "}defs/"
    SPI_GAMEPATH_DATA, // e.g., "}data/jdoom/"
    SPI_GAMEPATH_DEFS, // e.g., "}defs/jdoom/"
    SPI_GAMEMODEPATH_DATA, // e.g., "}data/jdoom/doom2-plut/"
    SPI_GAMEMODEPATH_DEFS, // e.g., "}defs/jdoom/doom2-plut/" 
    SPI_DOOMWADDIR
} searchpathid_t;

/**
 * @param searchOrder       Resource path search order (in order of least-importance, left to right).
 * @param overrideFlag      Command line options for setting the resource path explicitly.
 * @param overrideFlag2     Takes precendence.
 */
static void formResourceSearchPaths(gameinfo_t* info, resourcenamespaceid_t rni, ddstring_t* pathList,
    searchpathid_t* searchOrder, const char* defaultPath, const char* overrideFlag,
    const char* overrideFlag2)
{
    assert(info && F_IsValidResourceNamespaceId(rni) && pathList && searchOrder);
    {
    boolean usingGameModePathData = false;
    boolean usingGameModePathDefs = false;

    Str_Clear(pathList);

    if(overrideFlag && overrideFlag[0] && ArgCheckWith(overrideFlag, 1))
    {   // A command line override of the default path.
        filename_t newPath;
        M_TranslatePath(newPath, ArgNext(), FILENAME_T_MAXLEN);
        Dir_ValidDir(newPath, FILENAME_T_MAXLEN);
        Str_Prepend(pathList, ";"); Str_Prepend(pathList, newPath);
    }

    { size_t i;
    for(i = 0; searchOrder[i] != SPI_NONE; ++i)
    {
        switch(searchOrder[i])
        {
        case SPI_BASEPATH:
            { filename_t newPath;
            M_TranslatePath(newPath, "}", FILENAME_T_MAXLEN);
            GameInfo_AddResourceSearchPath(info, rni, newPath, false);
            break;
            }
        case SPI_BASEPATH_DATA:
            { filename_t newPath;
            if(defaultPath && defaultPath[0])
            {
                filename_t other;
                dd_snprintf(other, FILENAME_T_MAXLEN, DD_BASEPATH_DATA"%s", defaultPath);
                M_TranslatePath(newPath, other, FILENAME_T_MAXLEN);
            }
            else
            {
                M_TranslatePath(newPath, DD_BASEPATH_DATA, FILENAME_T_MAXLEN);
            }
            GameInfo_AddResourceSearchPath(info, rni, newPath, false);
            break;
            }
        case SPI_BASEPATH_DEFS:
            { filename_t newPath;
            if(defaultPath && defaultPath[0])
            {
                filename_t other;
                dd_snprintf(other, FILENAME_T_MAXLEN, DD_BASEPATH_DEFS"%s", defaultPath);
                M_TranslatePath(newPath, other, FILENAME_T_MAXLEN);
            }
            else
            {
                M_TranslatePath(newPath, DD_BASEPATH_DEFS, FILENAME_T_MAXLEN);
            }
            GameInfo_AddResourceSearchPath(info, rni, newPath, false);
            break;
            }
        case SPI_GAMEPATH_DATA:
            if(defaultPath && defaultPath[0])
            {
                filename_t newPath;
                dd_snprintf(newPath, FILENAME_T_MAXLEN, "%s%s", Str_Text(&info->_dataPath), defaultPath);
                GameInfo_AddResourceSearchPath(info, rni, newPath, false);
            }
            else
            {
                GameInfo_AddResourceSearchPath(info, rni, Str_Text(&info->_dataPath), false);
            }
            break;
        case SPI_GAMEPATH_DEFS:
            if(defaultPath && defaultPath[0])
            {
                filename_t newPath;
                dd_snprintf(newPath, FILENAME_T_MAXLEN, "%s%s", Str_Text(&info->_defsPath), defaultPath);
                GameInfo_AddResourceSearchPath(info, rni, newPath, false);
            }
            else
            {
                GameInfo_AddResourceSearchPath(info, rni, Str_Text(&info->_defsPath), false);
            }
            break;
        case SPI_GAMEMODEPATH_DATA:
            usingGameModePathData = true;
            if(defaultPath && defaultPath[0] && Str_Length(&info->_identityKey))
            {
                filename_t newPath;
                dd_snprintf(newPath, FILENAME_T_MAXLEN, "%s%s%s", Str_Text(&info->_dataPath), defaultPath, Str_Text(&info->_identityKey));
                GameInfo_AddResourceSearchPath(info, rni, newPath, false);
            }
            break;
        case SPI_GAMEMODEPATH_DEFS:
            usingGameModePathDefs = true;
            if(Str_Length(&info->_identityKey))
            {
                if(defaultPath && defaultPath[0])
                {
                    filename_t newPath;
                    dd_snprintf(newPath, FILENAME_T_MAXLEN, "%s%s", Str_Text(&info->_defsPath), Str_Text(&info->_identityKey));
                    GameInfo_AddResourceSearchPath(info, rni, newPath, false);
                }
                else
                {
                    filename_t newPath;
                    dd_snprintf(newPath, FILENAME_T_MAXLEN, "%s%s%s", Str_Text(&info->_defsPath), defaultPath, Str_Text(&info->_identityKey));
                    GameInfo_AddResourceSearchPath(info, rni, newPath, false);
                }
            }
            break;
        case SPI_DOOMWADDIR:
            if(!ArgCheck("-nowaddir") && getenv("DOOMWADDIR"))
            {
                filename_t newPath;
                M_TranslatePath(newPath, getenv("DOOMWADDIR"), FILENAME_T_MAXLEN);
                GameInfo_AddResourceSearchPath(info, rni, newPath, false);
            }
            break;

        default: Con_Error("collateResourceSearchPathSet: Invalid path ident %i.", searchOrder[i]);
        };
    }}

    // The overriding path.
    if(overrideFlag2 && overrideFlag2[0] && ArgCheckWith(overrideFlag2, 1))
    {
        filename_t newPath;
        M_TranslatePath(newPath, ArgNext(), FILENAME_T_MAXLEN);
        GameInfo_AddResourceSearchPath(info, rni, newPath, false);

        if((usingGameModePathData || usingGameModePathDefs) && Str_Length(&info->_identityKey))
        {
            filename_t other;
            dd_snprintf(other, FILENAME_T_MAXLEN, "%s/%s", newPath, Str_Text(&info->_identityKey));
            GameInfo_AddResourceSearchPath(info, rni, other, false);
        }
    }
    }
}

static void collateResourceSearchPathSet(gameinfo_t* info)
{
    assert(info);
    { searchpathid_t searchOrder[] = { SPI_DOOMWADDIR, SPI_BASEPATH_DATA, SPI_GAMEPATH_DATA, 0 };
    formResourceSearchPaths(info, 1+RC_PACKAGE,     &info->_searchPathLists[RC_PACKAGE], searchOrder, "", "", "");}

    { searchpathid_t searchOrder[] = { SPI_BASEPATH_DEFS, SPI_GAMEPATH_DEFS, SPI_GAMEMODEPATH_DEFS, 0 };
    formResourceSearchPaths(info, 1+RC_DEFINITION,  &info->_searchPathLists[RC_DEFINITION], searchOrder, "",         "-defdir",  "-defdir2");}

    { searchpathid_t searchOrder[] = { SPI_GAMEPATH_DATA, SPI_GAMEMODEPATH_DATA, 0 };
    formResourceSearchPaths(info, 1+RC_GRAPHIC,     &info->_searchPathLists[RC_GRAPHIC], searchOrder, "textures\\",  "-texdir",  "-texdir2");}

    { searchpathid_t searchOrder[] = { SPI_GAMEPATH_DATA, SPI_GAMEMODEPATH_DATA, 0 };
    formResourceSearchPaths(info, 1+RC_GRAPHIC,     &info->_searchPathLists[RC_GRAPHIC], searchOrder, "flats\\",     "-flatdir", "-flatdir2");}

    { searchpathid_t searchOrder[] = { SPI_GAMEPATH_DATA, SPI_GAMEMODEPATH_DATA, 0 };
    formResourceSearchPaths(info, 1+RC_GRAPHIC,     &info->_searchPathLists[RC_GRAPHIC], searchOrder, "patches\\",   "-patdir",  "-patdir2");}

    { searchpathid_t searchOrder[] = { SPI_GAMEPATH_DATA, SPI_GAMEMODEPATH_DATA, 0 };
    formResourceSearchPaths(info, 1+RC_GRAPHIC,     &info->_searchPathLists[RC_GRAPHIC], searchOrder, "lightmaps\\", "-lmdir",   "-lmdir2");}

    { searchpathid_t searchOrder[] = { SPI_GAMEPATH_DATA, SPI_GAMEMODEPATH_DATA, 0 };
    formResourceSearchPaths(info, 1+RC_GRAPHIC,     &info->_searchPathLists[RC_GRAPHIC], searchOrder, "flares\\",    "-flaredir", "-flaredir2");}

    { searchpathid_t searchOrder[] = { SPI_GAMEPATH_DATA, SPI_GAMEMODEPATH_DATA, 0 };
    formResourceSearchPaths(info, 1+RC_MUSIC,       &info->_searchPathLists[RC_MUSIC],   searchOrder, "music\\",     "-musdir",  "-musdir2");}

    { searchpathid_t searchOrder[] = { SPI_GAMEPATH_DATA, SPI_GAMEMODEPATH_DATA, 0 };
    formResourceSearchPaths(info, 1+RC_SOUND,       &info->_searchPathLists[RC_SOUND],   searchOrder, "sfx\\",       "-sfxdir",  "-sfxdir2");}

    { searchpathid_t searchOrder[] = { SPI_BASEPATH_DATA, 0 };
    formResourceSearchPaths(info, 1+RC_GRAPHIC,     &info->_searchPathLists[RC_GRAPHIC], searchOrder, "graphics\\",  "-gfxdir",  "-gfxdir2");}

    { searchpathid_t searchOrder[] = { SPI_GAMEPATH_DATA, SPI_GAMEMODEPATH_DATA, 0 };
    formResourceSearchPaths(info, 1+RC_MODEL,       &info->_searchPathLists[RC_MODEL],   searchOrder, "models\\",   "-modeldir", "-modeldir2");}
}

static __inline void clearResourceSearchPathList(gameinfo_t* info, resourcenamespaceid_t rni)
{
    assert(info && F_IsValidResourceNamespaceId(rni));
    Str_Free(&info->_searchPathLists[rni]);
}

gameinfo_t* P_CreateGameInfo(pluginid_t pluginId, const char* identityKey, const char* dataPath,
    const char* defsPath, const ddstring_t* mainDef, const char* title, const char* author,
    const ddstring_t* cmdlineFlag, const ddstring_t* cmdlineFlag2)
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
        gameresource_recordset_t* rset = &info->_requiredResources[i];
        rset->numRecords = 0;
        rset->records = 0;

        Str_Init(&info->_searchPathLists[i]);
    }}

    collateResourceSearchPathSet(info);
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
        gameresource_recordset_t* rset = &info->_requiredResources[i];

        if(!rset || rset->numRecords == 0)
            continue;
        
        { gameresource_record_t** rec;
        for(rec = rset->records; *rec; rec++)
        {
            Str_Free(&(*rec)->names);
            Str_Free(&(*rec)->path);
            if((*rec)->identityKeys)
            {
                size_t j;
                for(j = 0; (*rec)->identityKeys[j]; ++j)
                    Str_Delete((*rec)->identityKeys[j]);
                M_Free((*rec)->identityKeys); (*rec)->identityKeys = 0;
            }
            M_Free(*rec);
        }}
        M_Free(rset->records);
        rset->records = 0;
        rset->numRecords = 0;
    }}

    M_Free(info);
}

gameresource_record_t* GameInfo_AddResource(gameinfo_t* info, resourceclass_t rclass,
    resourcenamespaceid_t rni, const ddstring_t* names)
{
    assert(info && VALID_RESOURCE_CLASS(rclass) && F_IsValidResourceNamespaceId(rni) && names);
    {
    gameresource_recordset_t* rset = &info->_requiredResources[rni-1];
    gameresource_record_t* record;

    rset->records = M_Realloc(rset->records, sizeof(*rset->records) * (rset->numRecords+2));
    record = rset->records[rset->numRecords] = M_Malloc(sizeof(*record));
    rset->records[rset->numRecords+1] = 0; // Terminate.
    rset->numRecords++;

    Str_Init(&record->names); Str_Copy(&record->names, names);
    Str_Init(&record->path);
    record->rclass = rclass;
    switch(record->rclass)
    {
    case RC_PACKAGE:
        record->identityKeys = 0;
        break;
    default:
        break;
    }
    return record;
    }
}

void GameInfo_ClearResourceSearchPaths2(gameinfo_t* info, resourcenamespaceid_t rni)
{
    assert(info);
    if(rni == 0)
    {
        uint i, numResourceNamespaces = F_NumResourceNamespaces();
        for(i = 1; i < numResourceNamespaces+1; ++i)
        {
            clearResourceSearchPathList(info, (resourcenamespaceid_t)i);
        }
        return;
    }
    clearResourceSearchPathList(info, rni);
}

void GameInfo_ClearResourceSearchPaths(gameinfo_t* info)
{
    GameInfo_ClearResourceSearchPaths2(info, 0);
}

boolean GameInfo_AddResourceSearchPath(gameinfo_t* info, resourcenamespaceid_t rni,
    const char* newPath, boolean append)
{
    assert(info && F_IsValidResourceNamespaceId(rni));
    {
    ddstring_t* pathList;
    filename_t absNewPath;

    if(!newPath || !newPath[0] || !strcmp(newPath, DIR_SEP_STR))
        return false; // Not suitable.

    // Convert all slashes to the host OS's directory separator,
    // for compatibility with the sys_filein routines.
    strncpy(absNewPath, newPath, FILENAME_T_MAXLEN);
    Dir_ValidDir(absNewPath, FILENAME_T_MAXLEN);
    M_PrependBasePath(absNewPath, absNewPath, FILENAME_T_MAXLEN);

    // Have we seen this path already?
    pathList = &info->_searchPathLists[rni-1];
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
    
    // Add the new search path.
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

const ddstring_t* GameInfo_ResourceSearchPaths(gameinfo_t* info, resourcenamespaceid_t rni)
{
    assert(info);
    if(!F_IsValidResourceNamespaceId(rni))
        Con_Error("GameInfo_ResourceSearchPaths: Internal error, invalid resource namespace id %i.", (int)rni);
    return &info->_searchPathLists[rni-1];
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

gameresource_record_t* const* GameInfo_Resources(gameinfo_t* info, resourcenamespaceid_t rni, size_t* count)
{
    assert(info);
    if(!F_IsValidResourceNamespaceId(rni))
        Con_Error("GameInfo_Resources: Internal error, invalid resource namespace id %i.", (int)rni);
    if(count)
        *count = info->_requiredResources[rni-1].numRecords;
    return info->_requiredResources[rni-1].records;
}
