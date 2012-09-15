/**\file game.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2010-2012 Daniel Swanson <danij@dengine.net>
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
#include "updater/downloaddialog.h"

#include "game.h"
#include "abstractresource.h"

typedef struct {
    struct AbstractResource_s** records;
    size_t numRecords;
} resourcerecordset_t;

struct Game_s {
    /// Unique identifier of the plugin which registered this game.
    pluginid_t pluginId;

    /// Unique identifier string (e.g., "doom1-ultimate").
    ddstring_t identityKey;

    /// Formatted default title suitable for printing (e.g., "The Ultimate DOOM").
    ddstring_t title;

    /// Formatted default author suitable for printing (e.g., "id Software").
    ddstring_t author;

    /// The base directory for all data-class resources.
    ddstring_t dataPath;

    /// The base directory for all defs-class resources.
    ddstring_t defsPath;

    /// Name of the main config file (e.g., "configs/doom/game.cfg").
    ddstring_t mainConfig;

    /// Name of the file used for control bindings, set automatically at creation time.
    ddstring_t bindingConfig;

    /// Vector of records for required game resources (e.g., doomu.wad).
    resourcerecordset_t requiredResources[RESOURCECLASS_COUNT];
};

Game* Game_New(const char* identityKey, const ddstring_t* dataPath,
    const ddstring_t* defsPath, const char* configDir, const char* title,
    const char* author)
{
    int i;
    Game* g = (Game*)malloc(sizeof(*g));

    if(!g) Con_Error("Game::New: Failed on allocation of %lu bytes for new Game.", (unsigned long) sizeof(*g));

    Str_Init(&g->identityKey);
    if(identityKey)
        Str_Set(&g->identityKey, identityKey);

    Str_Init(&g->dataPath);
    if(dataPath && !Str_IsEmpty(dataPath))
        Str_Set(&g->dataPath, Str_Text(dataPath));

    Str_Init(&g->defsPath);
    if(defsPath && !Str_IsEmpty(defsPath))
        Str_Set(&g->defsPath, Str_Text(defsPath));

    Str_Init(&g->mainConfig);
    Str_Appendf(&g->mainConfig, "configs/%s", configDir);
    Str_Strip(&g->mainConfig);
    F_FixSlashes(&g->mainConfig, &g->mainConfig);
    F_AppendMissingSlash(&g->mainConfig);
    Str_Append(&g->mainConfig, "game.cfg");

    Str_Init(&g->bindingConfig);
    Str_Appendf(&g->bindingConfig, "configs/%s", configDir);
    Str_Strip(&g->bindingConfig);
    F_FixSlashes(&g->bindingConfig, &g->bindingConfig);
    F_AppendMissingSlash(&g->bindingConfig);
    Str_Append(&g->bindingConfig, "player/bindings.cfg");

    Str_Init(&g->title);
    if(title)
        Str_Set(&g->title, title);

    Str_Init(&g->author);
    if(author)
        Str_Set(&g->author, author);

    for(i = 0; i < RESOURCECLASS_COUNT; ++i)
    {
        resourcerecordset_t* rset = &g->requiredResources[i];
        rset->numRecords = 0;
        rset->records = 0;
    }

    g->pluginId = 0;

    return g;
}

void Game_Delete(Game* g)
{
    int i;
    assert(g);

    Str_Free(&g->identityKey);
    Str_Free(&g->dataPath);
    Str_Free(&g->defsPath);
    Str_Free(&g->mainConfig);
    Str_Free(&g->bindingConfig);
    Str_Free(&g->title);
    Str_Free(&g->author);

    for(i = 0; i < RESOURCECLASS_COUNT; ++i)
    {
        resourcerecordset_t* rset = &g->requiredResources[i];
        AbstractResource** rec;

        if(!rset || rset->numRecords == 0) continue;

        for(rec = rset->records; *rec; rec++)
        {
            AbstractResource_Delete(*rec);
        }
        free(rset->records);
        rset->records = 0;
        rset->numRecords = 0;
    }

    free(g);
}

AbstractResource* Game_AddResource(Game* g, resourceclass_t rclass,
    AbstractResource* record)
{
    resourcerecordset_t* rset;
    assert(g && record);

    if(!VALID_RESOURCE_CLASS(rclass))
    {
#if _DEBUG
        Con_Message("Game::AddResource: Invalid resource class %i specified, ignoring.\n", (int)rclass);
#endif
        return NULL;
    }
    if(!record)
    {
#if _DEBUG
        Con_Message("Game::AddResource: Received invalid ResourceRecord %p, ignoring.\n", (void*)record);
#endif
        return NULL;
    }

    rset = &g->requiredResources[rclass];
    rset->records = realloc(rset->records, sizeof(*rset->records) * (rset->numRecords+2));
    rset->records[rset->numRecords] = record;
    rset->records[rset->numRecords+1] = 0; // Terminate.
    rset->numRecords++;
    return record;
}

boolean Game_IsRequiredResource(Game* game, const char* absolutePath)
{
    AbstractResource* const* records = Game_Resources(game, RC_PACKAGE, 0);
    if(records)
    {
        AbstractResource* const* recordIt;
        // Is this resource from a container?
        abstractfile_t* file = F_FindLumpFile(absolutePath, NULL);
        if(file)
        {
            // Yes; use the container's path instead.
            absolutePath = Str_Text(AbstractFile_Path(file));
        }

        for(recordIt = records; *recordIt; recordIt++)
        {
            AbstractResource* rec = *recordIt;
            if(AbstractResource_ResourceFlags(rec) & RF_STARTUP)
            {
                const ddstring_t* resolvedPath = AbstractResource_ResolvedPath(rec, true);
                if(resolvedPath && !Str_CompareIgnoreCase(resolvedPath, absolutePath))
                {
                    return true;
                }
            }
        }
    }
    // Not found, so no.
    return false;
}

pluginid_t Game_SetPluginId(Game* g, pluginid_t pluginId)
{
    assert(g);
    return g->pluginId = pluginId;
}

pluginid_t Game_PluginId(Game* g)
{
    assert(g);
    return g->pluginId;
}

const ddstring_t* Game_IdentityKey(Game* g)
{
    assert(g);
    return &g->identityKey;
}

const ddstring_t* Game_DataPath(Game* g)
{
    assert(g);
    return &g->dataPath;
}

const ddstring_t* Game_DefsPath(Game* g)
{
    assert(g);
    return &g->defsPath;
}

const ddstring_t* Game_MainConfig(Game* g)
{
    assert(g);
    return &g->mainConfig;
}

const ddstring_t* Game_BindingConfig(Game* g)
{
    assert(g);
    return &g->bindingConfig;
}

const ddstring_t* Game_Title(Game* g)
{
    assert(g);
    return &g->title;
}

const ddstring_t* Game_Author(Game* g)
{
    assert(g);
    return &g->author;
}

AbstractResource* const* Game_Resources(Game* g, resourceclass_t rclass, int* count)
{
    assert(g);
    if(!VALID_RESOURCE_CLASS(rclass))
    {
        if(count) *count = 0;
        return NULL;
    }

    if(count) *count = (int)g->requiredResources[rclass].numRecords;
    return g->requiredResources[rclass].records? g->requiredResources[rclass].records : 0;
}

Game* Game_FromDef(const GameDef* def)
{
    Game* game;
    ddstring_t dataPath, defsPath;

    if(!def) return NULL;

    Str_Init(&dataPath); Str_Set(&dataPath, def->dataPath);
    Str_Strip(&dataPath);
    F_FixSlashes(&dataPath, &dataPath);
    F_ExpandBasePath(&dataPath, &dataPath);
    F_AppendMissingSlash(&dataPath);

    Str_Init(&defsPath); Str_Set(&defsPath, def->defsPath);
    Str_Strip(&defsPath);
    F_FixSlashes(&defsPath, &defsPath);
    F_ExpandBasePath(&defsPath, &defsPath);
    F_AppendMissingSlash(&defsPath);

    game = Game_New(def->identityKey, &dataPath, &defsPath, def->configDir,
                    def->defaultTitle, def->defaultAuthor);

    Str_Free(&defsPath);
    Str_Free(&dataPath);

    return game;
}

void Game_Notify(int notification, void* param)
{
    DENG_UNUSED(param);

    switch(notification)
    {
    case DD_NOTIFY_GAME_SAVED:
        // If an update has been downloaded and is ready to go, we should
        // re-show the dialog now that the user has saved the game as
        // prompted.
        DEBUG_Message(("Game_Notify: Game saved.\n"));
        Updater_RaiseCompletedDownloadDialog();
        break;
    }
}
