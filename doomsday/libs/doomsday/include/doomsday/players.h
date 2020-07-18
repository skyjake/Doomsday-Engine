/** @file doomsday/players.h
 *
 * @authors Copyright (c) 2015-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDOOMSDAY_PLAYERS_H
#define LIBDOOMSDAY_PLAYERS_H

/// Maximum number of players supported by the engine.
#define DDMAXPLAYERS        16

#ifdef __cplusplus

#include "libdoomsday.h"
#include <de/libcore.h>
#include <functional>

class Player;
struct ddplayer_s; // shared with plugins

/**
 * Base class for player state: common functionality shared by both the server
 * and the client.
 */
class LIBDOOMSDAY_PUBLIC Players
{
public:
    typedef std::function<Player *()> Constructor;

public:
    /**
     * Constructs a new Players array, and populates it with DDMAXPLAYERS players.
     *
     * @param playerConstructor  Function for creating new player instances.
     */
    Players(const Constructor& playerConstructor);

    Player &at(int index) const;

    inline Player &operator[](int index) const { return at(index); }

    int count() const;

    de::LoopResult forAll(const std::function<de::LoopResult (Player &)>& func) const;

    /**
     * Finds the index number of a player.
     *
     * @param player  Player.
     *
     * @return Index of the player, or -1 if not found.
     */
    int indexOf(const Player *player) const;

    /**
     * Finds the index number of a player based on the public data.
     *
     * @param player  Player's public data.
     *
     * @return Index of the player, or -1 if not found.
     */
    int indexOf(const ddplayer_s *publicData) const;

    void initBindings();

private:
    DE_PRIVATE(d)
};

#endif // __cplusplus

#endif // LIBDOOMSDAY_PLAYERS_H

