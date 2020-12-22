/** @file savelistdata.cpp
 *
 * @authors Copyright (c) 2016-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "ui/savelistdata.h"

#include <doomsday/doomsdayapp.h>
#include <doomsday/games.h>
#include <doomsday/abstractsession.h>
#include <doomsday/savegames.h>
#include <de/loop.h>
#include <de/textvalue.h>

using namespace de;

DE_PIMPL(SaveListData)
, DE_OBSERVES(FileIndex, Addition)
, DE_OBSERVES(FileIndex, Removal)
{
    Dispatch dispatch;

    Impl(Public *i) : Base(i)
    {
        SaveGames::get().saveIndex().audienceForAddition() += this;
        SaveGames::get().saveIndex().audienceForRemoval()  += this;
    }

    bool shouldAddFolder(const GameStateFolder &save) const
    {
        return save.path().beginsWith("/home/savegames"); // Ignore non-user savegames.
    }

    void fileAdded(const File &file, const FileIndex &)
    {
        const GameStateFolder &saveFolder = file.as<GameStateFolder>();
        if (shouldAddFolder(saveFolder))
        {
            dispatch += [this, &saveFolder] ()
            {
                // Needs to be added.
                self().append(new SaveItem(saveFolder));
            };
        }
    }

    void fileRemoved(const File &, const FileIndex &)
    {
        // Remove obsolete entries.
        dispatch += [this] ()
        {
            for (ui::Data::Pos idx = self().size() - 1; idx < self().size(); --idx)
            {
                if (!self().at(idx).isValid())
                {
                    self().remove(idx);
                }
            }
        };
    }

    void addAllFromIndex()
    {
        for (File *file : SaveGames::get().saveIndex().files())
        {
            try
            {
                GameStateFolder &save = file->as<GameStateFolder>();
                if (shouldAddFolder(save))
                {
                    self().append(new SaveItem(save));
                }
            }
            catch (const Error &er)
            {
                LOG_ERROR("Save file %s has corrupt metadata: %s")
                        << file->description()
                        << er.asText();
            }
        }
    }
};

SaveListData::SaveListData()
    : d(new Impl(this))
{
    //d->updateFromSavedIndex();
    d->addAllFromIndex();
}

// SaveItem -------------------------------------------------------------------

SaveListData::SaveItem::SaveItem(const GameStateFolder &saveFolder)
    : ImageItem(ShownAsButton)
    , saveFolder(&saveFolder)
{
    saveFolder.audienceForDeletion() += this;

    setData(TextValue(savePath())); // for looking it up later
    setLabel(title());

    Games &games = DoomsdayApp::games();
    if (games.contains(gameId()))
    {
        setImage(Style::get().images().image(games[gameId()].logoImageId()));
    }
}

SaveListData::SaveItem::~SaveItem()
{
    if (saveFolder) saveFolder->audienceForDeletion() -= this;
}

bool SaveListData::SaveItem::isValid() const
{
    return saveFolder != nullptr;
}

String SaveListData::SaveItem::title() const
{
    if (saveFolder)
    {
        return saveFolder->metadata().gets("userDescription");
    }
    return "";
}

String SaveListData::SaveItem::gameId() const
{
    if (saveFolder)
    {
        return saveFolder->metadata().gets("gameIdentityKey");
    }
    return "";
}

String SaveListData::SaveItem::savePath() const
{
    if (saveFolder)
    {
        return saveFolder->path();
    }
    return "";
}

String SaveListData::SaveItem::name() const
{
    if (saveFolder)
    {
        return saveFolder->name().fileNameWithoutExtension();
    }
    return "";
}

StringList SaveListData::SaveItem::loadedPackages() const
{
    if (saveFolder)
    {
        const Record &meta = saveFolder->metadata();
        if (meta.has("packages"))
        {
            return meta.getStringList("packages");
        }
    }
    return StringList();
}

void SaveListData::SaveItem::fileBeingDeleted(const File &)
{
    saveFolder = nullptr;
}

SaveListData::SaveItem &SaveListData::at(Pos pos)
{
    return ListData::at(pos).as<SaveItem>();
}

const SaveListData::SaveItem &SaveListData::at(Pos pos) const
{
    return ListData::at(pos).as<SaveItem>();
}
