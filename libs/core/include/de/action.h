/** @file widgets/action.h  Abstract base class for UI actions.
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

#ifndef LIBCORE_ACTION_H
#define LIBCORE_ACTION_H

#include "observers.h"
#include "counted.h"

namespace de {

/**
 * Abstract base class for user interface actions.
 *
 * Actions are reference-counted so that they can be shared by widgets and data model
 * items. Also, the same actions could be used by different sets of widgets representing
 * a single data item.
 *
 * @ingroup widgets
 */
class DE_PUBLIC Action : public Counted
{
public:
    /**
     * Audience to be notified when the action is triggerd.
     */
    DE_AUDIENCE(Triggered, void actionTriggered(Action &))

    Action();

    /**
     * Perform the action this instance represents. Derived classes must call
     * this or manually notify the Triggered audience in their own
     * implementation of the method.
     */
    virtual void trigger();

    DE_CAST_METHODS()

protected:
    virtual ~Action(); // ref counted, hence not publicly deletable

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBCORE_ACTION_H
