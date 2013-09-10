/** @file game.h  Base class for games.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_GAME_H
#define LIBDENG2_GAME_H

#include "../String"

namespace de {
namespace game {

/**
 * Base class for games.
 *
 * Represents a specific playable game that runs on top of Doomsday. There can
 * be only one game loaded at a time. Examples of games are "Doom II" and
 * "Ultimate Doom".
 *
 * The 'load' command can be used to load a game based on its identifier:
 * <pre>load doom2</pre>
 *
 * @todo The 'game' namespace can be removed once the client/server apps don't
 * declare their own de::Game classes any more.
 */
class DENG2_PUBLIC Game
{
public:
    Game(String const &gameId);
    virtual ~Game();

    /**
     * Sets the game that this game is a variant of. For instance, "Final Doom:
     * Plutonia Experiment" (doom2-plut) is a variant of "Doom II" (doom2).
     *
     * The source game can be used as a fallback for resources, configurations,
     * and other data.
     *
     * @param gameId  Identifier of a game.
     */
    void setVariantOf(String const &gameId);

    bool isNull() const;
    String id() const;
    String variantOf() const;

    DENG2_AS_IS_METHODS()

private:
    DENG2_PRIVATE(d)
};

} // namespace game
} // namespace de

#endif // LIBDENG2_GAME_H
