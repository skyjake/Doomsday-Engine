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
 */
class DENG2_PUBLIC Asset
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
    DENG2_DEFINE_AUDIENCE(StateChange, void assetStateChanged(Asset &))

    /**
     * Notified when the asset is destroyed.
     */
    DENG2_DEFINE_AUDIENCE(Deletion, void assetDeleted(Asset &))

public:
    Asset(State initialState = NotReady);
    virtual ~Asset();

    void setState(State s);
    void setState(bool assetReady);
    State state() const;

    /**
     * Determines if the asset is ready for use (immediately).
     */
    virtual bool isReady() const;

private:
    State _state;
};

/**
 * Set of dependendent assets. An object can use one or more of these to track
 * pools of dependencies, and quickly check whether all the required
 * dependencies are currently available.
 *
 * AssetGroup is derived from Asset so it is possible to group assets
 * together and depend on the groups as a whole.
 *
 * @todo Any better name for this class?
 */
class DENG2_PUBLIC AssetGroup : public Asset,
                                DENG2_OBSERVES(Asset, Deletion),
                                DENG2_OBSERVES(Asset, StateChange)
{
    DENG2_NO_COPY  (AssetGroup)
    DENG2_NO_ASSIGN(AssetGroup)

public:
    enum Policy {
        Ignore,         ///< State of the asset should be ignored.
        Required        ///< Dependents cannot operate without the asset.
    };

    typedef std::map<Asset const *, Policy> Members;

public:
    AssetGroup();
    virtual ~AssetGroup();

    int size() const;

    inline bool isEmpty() const { return !size(); }

    void clear();

    void insert(Asset const &dep, Policy policy = Required);

    AssetGroup &operator += (Asset const &dep) {
        insert(dep, Required);
        return *this;
    }

    AssetGroup &operator -= (Asset const &dep) {
        remove(dep);
        return *this;
    }

    bool has(Asset const &dep) const;

    void setPolicy(Asset const &asset, Policy policy);

    void remove(Asset const &asset);

    Members const &all() const;

    // Observes contained Assets.
    void assetDeleted(Asset &);
    void assetStateChanged(Asset &);

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBDENG2_ASSET_H
