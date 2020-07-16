/** @file savegames.cpp  Save games.
 *
 * @authors Copyright © 2005-2015 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2016-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "doomsday/savegames.h"
#include "doomsday/doomsdayapp.h"
#include "doomsday/games.h"
#include "doomsday/abstractsession.h"
#include "doomsday/gamestatefolder.h"
#include "doomsday/console/cmd.h"

#include <de/dscript.h>
#include <de/directoryfeed.h>
#include <de/filesystem.h>
#include <de/folder.h>
#include <de/logbuffer.h>
#include <de/loop.h>
#include <de/regexp.h>
#include <de/task.h>
#include <de/taskpool.h>

using namespace de;

// Script Bindings ----------------------------------------------------------------------

/**
 * Native Doomsday Script utility for scheduling conversion of a single legacy savegame.
 */
static Value *Function_GameStateFolder_Convert(Context &, const Function::ArgumentValues &args)
{
    String gameId     = args[0]->asText();
    String sourcePath = args[1]->asText();
    return new NumberValue(SaveGames::get().convertLegacySavegames(gameId, sourcePath));
}

/**
 * Native Doomsday Script utility for scheduling conversion of @em all legacy savegames
 * for the specified gameId.
 */
static Value *Function_GameStateFolder_ConvertAll(Context &, const Function::ArgumentValues &args)
{
    String gameId = args[0]->asText();
    return new NumberValue(SaveGames::get().convertLegacySavegames(gameId));
}

// SaveGames ----------------------------------------------------------------------------

DE_PIMPL(SaveGames)
, DE_OBSERVES(Games, Addition)  // savegames folder setup
, DE_OBSERVES(Loop,  Iteration) // post savegame conversion FS population
{
    Binder binder;
    Record savedSessionModule; // GameStateFolder: manipulation, conversion, etc... (based on native class GameStateFolder)
    TaskPool convertSavegameTasks;

    Impl(Public *i) : Base(i)
    {
        // Setup the GameStateFolder module.
        binder.init(savedSessionModule)
                << DE_FUNC(GameStateFolder_Convert,    "convert",    "gameId" << "savegamePath")
                << DE_FUNC(GameStateFolder_ConvertAll, "convertAll", "gameId");
        ScriptSystem::get().addNativeModule("SavedSession", savedSessionModule);
    }

    ~Impl()
    {
        convertSavegameTasks.waitForDone();
    }

    void gameAdded(Game &game) // Called from a non-UI thread.
    {
        LOG_AS("SaveGames");

        // Make the /home/savegames/<gameId> subfolder in the local FS if it does not yet exist.
        FileSystem::get().makeFolder(String("/home/savegames") / game.id());
    }

    /**
     * Asynchronous task that attempts conversion of a legacy savegame. Each converter
     * plugin is tried in turn.
     */
    class ConvertSavegameTask : public Task
    {
        ddhook_savegame_convert_t parm;

    public:
        ConvertSavegameTask(const String &sourcePath, const String &gameId)
        {
            // Ensure the game is defined (sanity check).
            if (DoomsdayApp::games().contains(gameId))
            {
                // Ensure the output folder exists if it doesn't already.
                const String outputPath = String("/home/savegames") / gameId;
                FileSystem::get().makeFolder(outputPath);

                Str_Set(Str_InitStd(&parm.sourcePath),     sourcePath);
                Str_Set(Str_InitStd(&parm.outputPath),     outputPath);
                Str_Set(Str_InitStd(&parm.fallbackGameId), gameId);
            }
            else
            {
                LOG_ERROR("Game \"%s\" does not exist") << gameId;
            }
        }

        ~ConvertSavegameTask()
        {
            Str_Free(&parm.sourcePath);
            Str_Free(&parm.outputPath);
            Str_Free(&parm.fallbackGameId);
        }

        void runTask()
        {
            DoomsdayApp::plugins().callAllHooks(HOOK_SAVEGAME_CONVERT, 0, &parm);
        }
    };

    void loopIteration()
    {
        /// @todo Refactor: TaskPool has a signal (or audience) when all tasks are complete.
        /// No need to check on every loop iteration.
        if (convertSavegameTasks.isDone())
        {
            LOG_AS("SaveGames");
            Loop::get().audienceForIteration() -= this;
            try
            {
                // The newly converted savegame(s) should now be somewhere in /home/savegames
                FileSystem::get().root().locate<Folder>("/home/savegames").populate();
            }
            catch (const Folder::NotFoundError &)
            {} // Ignore.
        }
    }

    void beginConvertLegacySavegame(const String &sourcePath, const String &gameId)
    {
        LOG_AS("SaveGames");
        LOG_TRACE("Scheduling legacy savegame conversion for %s (gameId:%s)", sourcePath << gameId);
        Loop::get().audienceForIteration() += this;
        convertSavegameTasks.start(new ConvertSavegameTask(sourcePath, gameId));
    }

    void locateLegacySavegames(const String &gameId)
    {
        LOG_AS("SaveGames");
        const String legacySavePath = String("/sys/legacysavegames") / gameId;
        if (Folder *oldSaveFolder = FileSystem::tryLocate<Folder>(legacySavePath))
        {
            // Add any new legacy savegames which may have appeared in this folder.
            oldSaveFolder->populate(Folder::PopulateOnlyThisFolder /* no need to go deep */);
        }
        else
        {
            try
            {
                // Make and setup a feed for the /sys/legacysavegames/<gameId> subfolder if the game
                // might have legacy savegames we may need to convert later.
                const NativePath oldSavePath = DoomsdayApp::games()[gameId].legacySavegamePath();
                if (oldSavePath.exists() && oldSavePath.isReadable())
                {
                    FileSystem::get().makeFolderWithFeed(legacySavePath,
                            new DirectoryFeed(oldSavePath),
                            Folder::PopulateOnlyThisFolder /* no need to go deep */);
                }
            }
            catch (const Games::NotFoundError &)
            {} // Ignore this error
        }
    }
};

SaveGames::SaveGames() : d(new Impl(this))
{}

void SaveGames::setGames(Games &games)
{
    games.audienceForAddition() += d;
}

void SaveGames::initialize()
{
    auto &fs = FileSystem::get();

    // Create the user saved session folder in the local FS if it doesn't yet exist.
    // Once created, any GameStateFolders in this folder will be found and indexed
    // automatically into the file system.
    fs.makeFolder("/home/savegames");

    // Create the legacy savegame folder.
    fs.makeFolder("/sys/legacysavegames");
}

const FileIndex &SaveGames::saveIndex() const
{
    return FileSystem::get().indexFor(DE_TYPE_NAME(GameStateFolder));
}

bool SaveGames::convertLegacySavegames(const String &gameId, const String &sourcePath)
{
    // A converter plugin is required.
    if (!Plug_CheckForHook(HOOK_SAVEGAME_CONVERT)) return false;

    // Populate /sys/legacysavegames/<gameId> with new savegames which may have appeared.
    d->locateLegacySavegames(gameId);

    auto &rootFolder = FileSystem::get().root();

    bool didSchedule = false;
    if (sourcePath.isEmpty())
    {
        // Process all legacy savegames.
        if (const Folder *saveFolder = rootFolder.tryLocate<Folder>("sys/legacysavegames"/gameId))
        {
            /// @todo File name pattern matching should not be done here. This is to prevent
            /// attempting to convert Hexen's map state sidecar files separately when this
            /// is called from Doomsday Script (in bootstrap.de).
            const Game &game = DoomsdayApp::games()[gameId];
            if (game.legacySavegameNameExp())
            {
                RegExp namePattern(game.legacySavegameNameExp(), CaseInsensitive);
                saveFolder->forContents([this, &gameId, &namePattern, &didSchedule] (String name, File &file)
                {
                    if (namePattern.exactMatch(name.fileName()))
                    {
                        // Schedule the conversion task.
                        d->beginConvertLegacySavegame(file.path(), gameId);
                        didSchedule = true;
                    }
                    return LoopContinue;
                });
            }
        }
    }
    else if (rootFolder.has(sourcePath)) // Just the one legacy savegame.
    {
        // Schedule the conversion task.
        d->beginConvertLegacySavegame(sourcePath, gameId);
        didSchedule = true;
    }

    return didSchedule;
}

SaveGames &SaveGames::get() // static
{
    return DoomsdayApp::saveGames();
}

// Console Commands ---------------------------------------------------------------------

D_CMD(InspectSavegame)
{
    DE_UNUSED(src, argc);

    String savePath = argv[1];

    // Append a .save extension if none exists.
    if (savePath.fileNameExtension().isEmpty())
    {
        savePath += ".save";
    }

    // If a game is loaded assume the user is referring to those savegames if not specified.
    if (savePath.fileNamePath().isEmpty() && DoomsdayApp::currentGameProfile())
    {
        savePath = DoomsdayApp::currentGameProfile()->savePath() / savePath;
    }

    if (const GameStateFolder *saved = FileSystem::tryLocate<GameStateFolder>(savePath))
    {
        LOG_SCR_MSG("%s") << saved->metadata().asStyledText();
        LOG_SCR_MSG(_E(D) "Resource: " _E(.)_E(i) "\"%s\"") << saved->path();
        return true;
    }

    LOG_WARNING("Failed to locate savegame with \"%s\"") << savePath;
    return false;
}

void SaveGames::consoleRegister() // static
{
    C_CMD("inspectsavegame", "s",   InspectSavegame)
}

String SaveGames::savePath()
{
    if (auto *gp = DoomsdayApp::currentGameProfile())
    {
        return gp->savePath();
    }
    return {};
}

