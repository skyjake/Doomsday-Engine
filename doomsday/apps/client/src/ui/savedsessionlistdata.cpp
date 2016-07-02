/** @file savedsessionlistdata.cpp
 *
 * @authors Copyright (c) 2016 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "ui/savedsessionlistdata.h"

#include <doomsday/doomsdayapp.h>
#include <doomsday/games.h>
#include <doomsday/Session>
#include <de/Loop>

using namespace de;

DENG2_PIMPL(SavedSessionListData)
, DENG2_OBSERVES(Session::SavedIndex, AvailabilityUpdate)
{
    LoopCallback mainCall;

    Instance(Public *i) : Base(i)
    {
        Session::savedIndex().audienceForAvailabilityUpdate() += this;
    }

    void updateFromSavedIndex()
    {
        // Remove obsolete entries.
        for (ui::Data::Pos idx = self.size() - 1; idx < self.size(); --idx)
        {
            if (!self.at(idx).as<SaveItem>().isValid())
            {
                self.remove(idx);
            }
        }

        // Add new entries.
        DENG2_FOR_EACH_CONST(Session::SavedIndex::All, i, Session::savedIndex().all())
        {
            ui::Data::Pos found = self.findData(i.key()); // savePath
            if (found == ui::Data::InvalidPos)
            {
                SavedSession &session = *i.value();
                if (session.path().beginsWith("/home/savegames")) // Ignore non-user savegames.
                {
                    // Needs to be added.
                    self.append(new SaveItem(session));
                }
            }
        }
    }

    void savedIndexAvailabilityUpdate(Session::SavedIndex const &)
    {
        mainCall.enqueue([this] () { updateFromSavedIndex(); });
    }
};

SavedSessionListData::SavedSessionListData()
    : d(new Instance(this))
{
    d->updateFromSavedIndex();
}

// SaveItem -------------------------------------------------------------------

SavedSessionListData::SaveItem::SaveItem(SavedSession const &session)
    : ImageItem(ShownAsButton)
    , session(&session)
{
    session.audienceForDeletion() += this;

    setData(savePath()); // for looking it up later
    setLabel(title());

    Games &games = DoomsdayApp::games();
    if (games.contains(gameId()))
    {
        setImage(Style::get().images().image(games[gameId()].logoImageId()));
    }
}

SavedSessionListData::SaveItem::~SaveItem()
{
    if (session) session->audienceForDeletion() -= this;
}

bool SavedSessionListData::SaveItem::isValid() const
{
    return session != nullptr;
}

String SavedSessionListData::SaveItem::title() const
{
    if (session)
    {
        return session->metadata().gets("userDescription");
    }
    return "";
}

String SavedSessionListData::SaveItem::gameId() const
{
    if (session)
    {
        return session->metadata().gets("gameIdentityKey");
    }
    return "";
}

String SavedSessionListData::SaveItem::savePath() const
{
    if (session)
    {
        return session->path().toLower();
    }
    return "";
}

String SavedSessionListData::SaveItem::name() const
{
    if (session)
    {
        return session->name().fileNameWithoutExtension();
    }
    return "";
}

void SavedSessionListData::SaveItem::fileBeingDeleted(File const &)
{
    session = nullptr;
}

SavedSessionListData::SaveItem &SavedSessionListData::at(Pos pos)
{
    return ListData::at(pos).as<SaveItem>();
}

SavedSessionListData::SaveItem const &SavedSessionListData::at(Pos pos) const
{
    return ListData::at(pos).as<SaveItem>();
}
