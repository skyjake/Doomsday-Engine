/** @file asset.h  Information about the state of an asset (e.g., resource).
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCORE_ASSET_H
#define LIBCORE_ASSET_H

#include "de/libcore.h"
#include "de/observers.h"

#include <map>

namespace de {

/**
 * Information about the state of an asset (e.g., resource). This class
 * provides a uniform way for various resources to declare their state to
 * whoever needs the resources.
 *
 * Only use this for assets that may be unavailable at times: for instance, an
 * OpenGL shader may or may not be compiled and ready to be used, but a native
 * file in the FileSystem is always considered available (as it can be read via
 * the native file system at any time).
 *
 * @ingroup core
 */
class DE_PUBLIC Asset
{
public:
    enum State {
        NotReady,       ///< Asset is not available at the moment.
        Ready,          ///< Asset is available immediately.
        Recoverable,    ///< Asset is available but not immediately (e.g., needs reloading from disk).
        Recovering      ///< Asset is presently being recovered and will soon be available.
    };

    /**
     * Notified whenever the state of the asset changes.
     */
    DE_AUDIENCE(StateChange, void assetStateChanged(Asset &))

    /**
     * Notified when the asset is being destroyed.
     */
    DE_AUDIENCE(Deletion, void assetBeingDeleted(Asset &))

public:
    Asset(State initialState = NotReady);
    Asset(const Asset &other);

    virtual ~Asset();

    void  setState(State s);
    void  setState(bool assetReady);
    State state() const;

    /**
     * Determines if the asset is ready for use (immediately).
     */
    virtual bool isReady() const;
    
    virtual String asText() const;

    void waitForState(State s) const;

private:
    DE_PRIVATE(d)
};

/**
 * Set of dependendent assets. An object can use one or more of these to track
 * pools of dependencies, and quickly check whether all the required
 * dependencies are currently available.
 *
 * AssetGroup is derived from Asset so it is possible to group assets
 * together and depend on the groups as a whole.
 *
 * @ingroup core
 *
 * @todo Any better name for this class?
 */
class DE_PUBLIC AssetGroup
    : public Asset
    , DE_OBSERVES(Asset, Deletion)
    , DE_OBSERVES(Asset, StateChange)
{
    DE_NO_COPY  (AssetGroup)
    DE_NO_ASSIGN(AssetGroup)

public:
    enum Policy {
        Ignore,         ///< State of the asset should be ignored.
        Required        ///< Dependents cannot operate without the asset.
    };

    typedef std::map<const Asset *, Policy> Members;

public:
    AssetGroup();

    virtual ~AssetGroup();

    inline bool    isEmpty() const { return !size(); }
    dint           size() const;
    bool           has(const Asset &dep) const;
    const Members &all() const;

    void clear();
    void insert(const Asset &dep, Policy policy = Required);
    void remove(const Asset &asset);
    void setPolicy(const Asset &asset, Policy policy);

    AssetGroup &operator += (const Asset &dep) {
        insert(dep, Required);
        return *this;
    }

    AssetGroup &operator -= (const Asset &dep) {
        remove(dep);
        return *this;
    }

    String asText() const override;
    
    // Observes contained Assets.
    void assetBeingDeleted(Asset &);
    void assetStateChanged(Asset &);

private:
    DE_PRIVATE(d)
};

/**
 * Interface for objects that have an asset group.
 */
class DE_PUBLIC IAssetGroup
{
public:
    virtual ~IAssetGroup();
    virtual AssetGroup &assets() = 0;

    /**
     * Implicit conversion to an Asset reference, for convenience.
     */
    operator Asset &();
};

} // namespace de

#endif // LIBCORE_ASSET_H
