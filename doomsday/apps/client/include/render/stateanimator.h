/** @file stateanimator.h
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

#ifndef DENG_CLIENT_RENDER_STATEANIMATOR_H
#define DENG_CLIENT_RENDER_STATEANIMATOR_H

#include "render/modelrenderer.h"

#include <doomsday/world/mobj.h>
#include <de/ModelDrawable>
#include <de/GLProgram>
#include <de/IObject>

namespace render {

/**
 * State-based object animator for `ModelDrawable`s.
 *
 * Used for both mobjs and psprites. The state and movement of the object
 * determine which animation sequences are started.
 *
 * @par Scripting
 *
 * The script object has the following constants:
 * - `ID` (Text): asset identifier.
 * - `ASSET` (Record): asset metadata.
 */
class StateAnimator : public de::ModelDrawable::Animator,
                      public de::IObject
{
public:
    DENG2_ERROR(DefinitionError);

public:
    StateAnimator(de::DotPath const &id, Model const &model);

    Model const &model() const;

    /**
     * Sets the namespace of the animator's owner. Available as a variable in
     * animation scripts.
     *
     * @param names    Owner's namespace.
     * @param varName  Name of the variable that points to @a names.
     */
    void setOwnerNamespace(de::Record &names, de::String const &varName);

    void triggerByState(de::String const &stateName);

    void triggerDamage(int points, struct mobj_s const *inflictor);

    void advanceTime(de::TimeDelta const &elapsed) override;

    de::ddouble currentTime(int index) const override;

    de::ModelDrawable::Appearance const &appearance() const;

    // Implements IObject.
    de::Record &objectNamespace() override;
    de::Record const &objectNamespace() const override;

private:
    DENG2_PRIVATE(d)
};

} // namespace render

#endif // DENG_CLIENT_RENDER_STATEANIMATOR_H
