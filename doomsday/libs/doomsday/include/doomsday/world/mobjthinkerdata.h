/** @file mobjthinkerdata.h
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef LIBDOOMSDAY_MOBJTHINKERDATA_H
#define LIBDOOMSDAY_MOBJTHINKERDATA_H

#include "thinkerdata.h"
#include "mobj.h"
#include "def_share.h"

/**
 * Private mobj data common to both client and server.
 *
 * @todo Game-side IData should be here; eventually the games don't need to add any
 * custom members to mobj_s, just to their own private data instance. -jk
 */
class LIBDOOMSDAY_PUBLIC MobjThinkerData : public ThinkerData
{
public:
    MobjThinkerData(const de::Id &id = de::Id::none());
    MobjThinkerData(const MobjThinkerData &other);

    void think() override;
    IData *duplicate() const override;

    mobj_t *mobj();
    const mobj_t *mobj() const;

    void initBindings() override;

    /**
     * Called whenever the current state of the mobj has changed.
     *
     * @param previousState  Previous state of the object.
     */
    virtual void stateChanged(const state_t *previousState);

    /**
     * Called whenever the mobj receives damage. This is a notification of
     * damage already received.
     *
     * @param points     Amount of damage.
     * @param direction  If not @c nullptr, identifies the source of the damage.
     */
    virtual void damageReceived(int points, const mobj_t *inflictor);

    void operator << (de::Reader &from) override;

private:
    DE_PRIVATE(d)
};

#endif // LIBDOOMSDAY_MOBJTHINKERDATA_H
