/** @file game.cpp  Base class for games.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small> 
 */

#include "de/game/Game"

namespace de {
namespace game {

DENG2_PIMPL(Game)
{
    String id;
    String variantOf;

    Instance(Public *i, String const &ident) : Base(i), id(ident)
    {}
};

Game::Game(String const &gameId) : d(new Instance(this, gameId))
{}

Game::~Game()
{}

void Game::setVariantOf(String const &gameId)
{
    d->variantOf = gameId;
}

bool Game::isNull() const
{
    return d->id.isEmpty();
}

String Game::id() const
{
    return d->id;
}

String Game::variantOf() const
{
    return d->variantOf;
}

} // namespace game
} // namespace de
