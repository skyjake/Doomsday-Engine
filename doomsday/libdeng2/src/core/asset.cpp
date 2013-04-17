/** @file asset.cpp  Information about the state of an asset (e.g., resource).
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

#include "de/Asset"

namespace de {

DENG2_PIMPL_NOREF(Asset)
{
    Policy policy;
    State state;

    Instance(Policy p, State s) : policy(p), state(s) {}
};

Asset::Asset(Policy assetPolicy, State initialState)
    : d(new Instance(assetPolicy, initialState))
{}

Asset::~Asset()
{}

void Asset::setPolicy(Policy p)
{
    d->policy = p;
}

Asset::Policy Asset::policy() const
{
    return d->policy;
}

void Asset::setState(State s)
{
    State old = d->state;
    d->state = s;
    if(old != d->state)
    {
        DENG2_FOR_AUDIENCE(StateChange, i) i->assetStateChanged(*this);
    }
}

Asset::State Asset::state() const
{
    return d->state;
}

bool Asset::isReady() const
{
    return d->state == Ready;
}

} // namespace de
