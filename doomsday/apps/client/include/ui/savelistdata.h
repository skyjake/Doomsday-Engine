/** @file savelistdata.h  UI data items representing available saved games.
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

#ifndef DE_CLIENT_UI_SAVELISTDATA_H
#define DE_CLIENT_UI_SAVELISTDATA_H

#include <de/ui/listdata.h>
#include <de/ui/imageitem.h>
#include <doomsday/gamestatefolder.h>

/**
 * List data model for available saved sessions.
 */
class SaveListData : public de::ui::ListData
{
public:
    struct SaveItem : public de::ui::ImageItem,
                      DE_OBSERVES(de::File, Deletion)
    {
        const GameStateFolder *saveFolder;

        SaveItem(const GameStateFolder &saveFolder);
        ~SaveItem();

        bool isValid() const;
        de::String title() const;
        de::String gameId() const;
        de::String savePath() const;
        de::String name() const;
        de::StringList loadedPackages() const;

        void fileBeingDeleted(const de::File &);
    };

public:
    SaveListData();

    SaveItem &at(Pos pos) override;
    const SaveItem &at(Pos pos) const override;

private:
    DE_PRIVATE(d)
};

#endif // DE_CLIENT_UI_SAVELISTDATA_H
