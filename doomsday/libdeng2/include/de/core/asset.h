/** @file asset.h  Information about the state of an asset (e.g., resource).
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

#ifndef LIBDENG2_ASSET_H
#define LIBDENG2_ASSET_H

#include "../libdeng2.h"
#include "../Observers"

namespace de {

/**
 * Information about the state of an asset (e.g., resource). This class
 * provides a uniform way for various resources to declare their state to
 * whoever needs the resources.
 */
class DENG2_PUBLIC Asset
{
public:
    enum State {
        NotReady,       ///< Asset is not available at the moment.
        Ready,          ///< Asset is available immediately.
        Recoverable     ///< Asset is available but not immediately (e.g., needs reloading from disk).
    };

    enum Policy {
        Ignore,         ///< State of the asset should be ignored.
        Block,          ///< Dependents cannot operate without the asset.
        SuspendTime     ///< Time cannot advance without the asset.
    };

    /**
     * Notified whenever the state of the asset changes.
     */
    DENG2_DEFINE_AUDIENCE(StateChange, void assetStateChanged(Asset &))

public:
    Asset(Policy assetPolicy = Ignore, State initialState = NotReady);
    virtual ~Asset();

    void setPolicy(Policy p);
    Policy policy() const;

    void setState(State s);
    State state() const;

    /**
     * Determines if the asset is ready for use (immediately).
     */
    bool isReady() const;

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBDENG2_ASSET_H
