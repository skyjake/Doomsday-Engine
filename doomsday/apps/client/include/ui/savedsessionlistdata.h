/** @file savedsessionlistdata.h  UI data items representing available saved games.
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

#ifndef DENG_CLIENT_UI_SAVEDSESSIONLISTDATA_H
#define DENG_CLIENT_UI_SAVEDSESSIONLISTDATA_H

#include <de/ui/ListData>
#include <de/ui/ImageItem>
#include <doomsday/SavedSession>

/**
 * List data model for available saved sessions.
 */
class SavedSessionListData : public de::ui::ListData
{
public:
    struct SaveItem : public de::ui::ImageItem,
                      DENG2_OBSERVES(de::File, Deletion)
    {
        SavedSession const *session;

        SaveItem(SavedSession const &session);
        ~SaveItem();

        bool isValid() const;
        de::String title() const;
        de::String gameId() const;
        de::String savePath() const;

        void fileBeingDeleted(de::File const &);
    };

public:
    SavedSessionListData();

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_CLIENT_UI_SAVEDSESSIONLISTDATA_H
