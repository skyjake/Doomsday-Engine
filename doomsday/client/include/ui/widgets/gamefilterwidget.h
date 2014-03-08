/** @file gamefilterwidget.h
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DENG_CLIENT_GAMEFILTERWIDGET_H
#define DENG_CLIENT_GAMEFILTERWIDGET_H

#include <de/GuiWidget>
#include <de/IPersistent>

/**
 * Filtering and sorting parameters for a game selection widget. Allows picking the type
 * of games to show (singleplayer or multiplayer) and how the games are sorted.
 *
 * The widget defines its own height, but width must be set manually.
 *
 * The filter and sort setting is saved persistently.
 */
class GameFilterWidget : public de::GuiWidget, public de::IPersistent
{
    Q_OBJECT

public:
    enum FilterFlag
    {
        Singleplayer = 0x1,
        Multiplayer  = 0x2,
        AllGames     = Singleplayer | Multiplayer
    };
    Q_DECLARE_FLAGS(Filter, FilterFlag)

    enum SortOrder
    {
        SortByTitle,
        SortByIdentityKey
    };

public:
    GameFilterWidget(de::String const &name = "gamefilter");

    void useInvertedStyle();
    void setFilter(Filter flt);

    Filter filter() const;
    SortOrder sortOrder() const;

    // Implements IPersistent.
    void operator >> (de::PersistentState &toState) const;
    void operator << (de::PersistentState const &fromState);

signals:
    void filterChanged();
    void sortOrderChanged();

private:
    DENG2_PRIVATE(d)
};

Q_DECLARE_OPERATORS_FOR_FLAGS(GameFilterWidget::Filter)

#endif // DENG_CLIENT_GAMEFILTERWIDGET_H
