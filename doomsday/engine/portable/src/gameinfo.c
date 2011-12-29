/**\file gameinfo.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2010-2011 Daniel Swanson <danij@dengine.net>
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
#include "de_filesys.h"

#include "gameinfo.h"
#include "resourcerecord.h"

gameinfo_t* GameInfo_New(const char* identityKey, const ddstring_t* dataPath,
    const ddstring_t* defsPath, const char* mainConfig, const char* title,
    const char* author)
{
    int i;
    gameinfo_t* info = (gameinfo_t*)malloc(sizeof(*info));

    if(!info) Con_Error("GameInfo::New: Failed on allocation of %lu bytes for new GameInfo.", (unsigned long) sizeof(*info));

    Str_Init(&info->_identityKey);
    if(identityKey)
        Str_Set(&info->_identityKey, identityKey);

    Str_Init(&info->_dataPath);
    if(dataPath && !Str_IsEmpty(dataPath))
        Str_Set(&info->_dataPath, Str_Text(dataPath));

    Str_Init(&info->_defsPath);
    if(defsPath && !Str_IsEmpty(defsPath))
        Str_Set(&info->_defsPath, Str_Text(defsPath));

    Str_Init(&info->_mainConfig);
    Str_Init(&info->_bindingConfig);
    if(mainConfig)
    {
        Str_Set(&info->_mainConfig, mainConfig);
        Str_Strip(&info->_mainConfig);
        F_FixSlashes(&info->_mainConfig, &info->_mainConfig);
        Str_PartAppend(&info->_bindingConfig, Str_Text(&info->_mainConfig), 0, Str_Length(&info->_mainConfig)-4);
        Str_Append(&info->_bindingConfig, "-bindings.cfg");
    }

    Str_Init(&info->_title);
    if(title)
        Str_Set(&info->_title, title);

    Str_Init(&info->_author);
    if(author)
        Str_Set(&info->_author, author);

    for(i = 0; i < RESOURCECLASS_COUNT; ++i)
    {
        resourcerecordset_t* rset = &info->_requiredResources[i];
        rset->numRecords = 0;
        rset->records = 0;
    }

    info->_pluginId = 0;

    return info;
}

void GameInfo_Delete(gameinfo_t* info)
{
    int i;
    assert(info);

    Str_Free(&info->_identityKey);
    Str_Free(&info->_dataPath);
    Str_Free(&info->_defsPath);
    Str_Free(&info->_mainConfig);
    Str_Free(&info->_bindingConfig);
    Str_Free(&info->_title);
    Str_Free(&info->_author);

    for(i = 0; i < RESOURCECLASS_COUNT; ++i)
    {
        resourcerecordset_t* rset = &info->_requiredResources[i];
        resourcerecord_t** rec;

        if(!rset || rset->numRecords == 0) continue;

        for(rec = rset->records; *rec; rec++)
        {
            ResourceRecord_Delete(*rec);
        }
        free(rset->records);
        rset->records = 0;
        rset->numRecords = 0;
    }

    free(info);
}

resourcerecord_t* GameInfo_AddResource(gameinfo_t* info, resourceclass_t rclass,
    resourcerecord_t* record)
{
    resourcerecordset_t* rset;
    assert(info && record);

    if(!VALID_RESOURCE_CLASS(rclass))
    {
#if _DEBUG
        Con_Message("GameInfo::AddResource: Invalid resource class %i specified, ignoring.\n", (int)rclass);
#endif
        return NULL;
    }
    if(!record)
    {
#if _DEBUG
        Con_Message("GameInfo::AddResource: Received invalid ResourceRecord %p, ignoring.\n", (void*)record);
#endif
        return NULL;
    }

    rset = &info->_requiredResources[rclass];
    rset->records = realloc(rset->records, sizeof(*rset->records) * (rset->numRecords+2));
    rset->records[rset->numRecords] = record;
    rset->records[rset->numRecords+1] = 0; // Terminate.
    rset->numRecords++;
    return record;
}

pluginid_t GameInfo_SetPluginId(gameinfo_t* info, pluginid_t pluginId)
{
    assert(info);
    return info->_pluginId = pluginId;
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

const ddstring_t* GameInfo_MainConfig(gameinfo_t* info)
{
    assert(info);
    return &info->_mainConfig;
}

const ddstring_t* GameInfo_BindingConfig(gameinfo_t* info)
{
    assert(info);
    return &info->_bindingConfig;
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

resourcerecord_t* const* GameInfo_Resources(gameinfo_t* info, resourceclass_t rclass, size_t* count)
{
    assert(info);
    if(!VALID_RESOURCE_CLASS(rclass))
    {
        if(count) *count = 0;
        return NULL;
    }

    if(count) *count = info->_requiredResources[rclass].numRecords;
    return info->_requiredResources[rclass].records? info->_requiredResources[rclass].records : 0;
}

gameinfo_t* GameInfo_FromDef(const GameDef* def)
{
    gameinfo_t* info;
    ddstring_t dataPath, defsPath;

    if(!def) return NULL;

    Str_Init(&dataPath); Str_Set(&dataPath, def->dataPath);
    Str_Strip(&dataPath);
    F_FixSlashes(&dataPath, &dataPath);
    F_ExpandBasePath(&dataPath, &dataPath);
    if(Str_RAt(&dataPath, 0) != '/')
        Str_AppendChar(&dataPath, '/');

    Str_Init(&defsPath); Str_Set(&defsPath, def->defsPath);
    Str_Strip(&defsPath);
    F_FixSlashes(&defsPath, &defsPath);
    F_ExpandBasePath(&defsPath, &defsPath);
    if(Str_RAt(&defsPath, 0) != '/')
        Str_AppendChar(&defsPath, '/');

    info = GameInfo_New(def->identityKey, &dataPath, &defsPath, def->mainConfig,
                        def->defaultTitle, def->defaultAuthor);

    Str_Free(&defsPath);
    Str_Free(&dataPath);

    return info;
}
