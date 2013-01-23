/*
 * The Doomsday Engine Project
 *
 * Copyright (c) 2011-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "de/DelegateRule"

namespace de {

DelegateRule::DelegateRule(Rule const &source)
    : ConstantRule(0), _source(source)
{
    // As a delegate, we are considered part of the owner.
    setDelegate(&_source);

    connect(&source, SIGNAL(valueInvalidated()), this, SLOT(invalidate()));

    invalidate();
}

DelegateRule::~DelegateRule()
{}

void DelegateRule::update()
{
    // The value gets updated by the source.
    const_cast<Rule *>(&_source)->update();

    ConstantRule::update();
}

} // namespace de
