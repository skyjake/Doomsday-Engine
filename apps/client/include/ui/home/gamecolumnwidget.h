/** @file gamecolumnwidget.h
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

#ifndef DE_CLIENT_UI_HOME_GAMECOLUMNWIDGET_H
#define DE_CLIENT_UI_HOME_GAMECOLUMNWIDGET_H

#include "columnwidget.h"

#include <de/ipersistent.h>

class SaveListData;

class GameColumnWidget : public ColumnWidget, public de::IPersistent
{
public:
    GameColumnWidget(const de::String &gameFamily,
                     const SaveListData &savedItems);

    de::String tabHeading() const override;
    int tabShortcut() const override;
    de::String configVariableName() const override;
    void setHighlighted(bool highlighted) override;

    // Implements IPersistent.
    void operator>>(de::PersistentState &toState) const override;
    void operator<<(const de::PersistentState &fromState) override;

public:
    static const char *SORT_GAME_ID;
    static const char *SORT_MODS;
    static const char *SORT_RECENTLY_PLAYED;
    static const char *SORT_RELEASE_DATE;
    static const char *SORT_TITLE;

private:
    DE_PRIVATE(d)
};

#endif // DE_CLIENT_UI_HOME_GAMECOLUMNWIDGET_H
