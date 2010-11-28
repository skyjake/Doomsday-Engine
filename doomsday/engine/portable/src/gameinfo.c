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
    }}

    return info;
}

void P_DestroyGameInfo(gameinfo_t* info)
{
    assert(info);

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
    const ddstring_t* names)
{
    assert(info && VALID_RESOURCE_CLASS(rclass) && names);
    {
    gameresource_recordset_t* rset = &info->_requiredResources[rclass];
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

gameresource_record_t* const* GameInfo_Resources(gameinfo_t* info, resourceclass_t rclass, size_t* count)
{
    assert(info && VALID_RESOURCE_CLASS(rclass));
    if(count)
        *count = info->_requiredResources[rclass].numRecords;
    return info->_requiredResources[rclass].records;
}
