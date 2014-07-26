/** @file clientmobjthinkerdata.cpp  Private client-side data for mobjs.
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

#include "world/clientmobjthinkerdata.h"

using namespace de;

DENG2_PIMPL(ClientMobjThinkerData)
{
    QScopedPointer<NetworkState> net;

    Instance(Public *i) : Base(i)
    {}

    Instance(Public *i, Instance const &other) : Base(i)
    {
        if(!other.net.isNull())
        {
            net.reset(new NetworkState(*other.net));
        }
    }
};

ClientMobjThinkerData::ClientMobjThinkerData(mobj_t *mobj)
    : MobjThinkerData(mobj)
    , d(new Instance(this))
{}

ClientMobjThinkerData::ClientMobjThinkerData(const ClientMobjThinkerData &other)
    : MobjThinkerData(other)
    , d(new Instance(this, *other.d))
{}

Thinker::IData *ClientMobjThinkerData::duplicate() const
{
    return new ClientMobjThinkerData(*this);
}

bool ClientMobjThinkerData::hasNetworkState() const
{
    return !d->net.isNull();
}

ClientMobjThinkerData::NetworkState &ClientMobjThinkerData::networkState()
{
    if(!hasNetworkState())
    {
        d->net.reset(new NetworkState);
    }
    return *d->net;
}
