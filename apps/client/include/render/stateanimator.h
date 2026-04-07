/** @file stateanimator.h
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef DE_CLIENT_RENDER_STATEANIMATOR_H
#define DE_CLIENT_RENDER_STATEANIMATOR_H

#include "render/modelrenderer.h"

#include <doomsday/world/mobj.h>
#include <de/modeldrawable.h>
#include <de/glprogram.h>
#include <de/scripting/iobject.h>
#include <de/scripting/scheduler.h>

namespace render {

/**
 * State-based object animator for `ModelDrawable`s. Triggers animation sequences
 * based on state changes and damage points received by the owner.
 *
 * Used for both mobjs and psprites. The state and movement of the object
 * determine which animation sequences are started.
 *
 * @par Scripting
 *
 * The script object has the following constants:
 * - `ID` (Text): asset identifier.
 * - `__asset__` (Record): asset metadata.
 */
class StateAnimator : public de::ModelDrawable::Animator,
                      public de::IObject
{
public:
    DE_ERROR(DefinitionError);

public:
    StateAnimator();
    StateAnimator(const de::DotPath &id, const Model &model);

    const Model &model() const;

    /**
     * Returns the script scheduler specific to this animator. A new scheduler will
     * be created the first time this is called.
     */
    de::Scheduler &scheduler();

    /**
     * Sets the namespace of the animator's owner. Available as a variable in
     * animation scripts.
     *
     * @param names    Owner's namespace.
     * @param varName  Name of the variable that points to @a names.
     */
    void setOwnerNamespace(de::Record &names, const de::String &varName);

    /**
     * Returns the name of the variable pointing to the owning object's namespace.
     */
    de::String ownerNamespaceName() const;

    void triggerByState(const de::String &stateName);

    void triggerDamage(int points, const struct mobj_s *inflictor);

    void startAnimation(int animationId, int priority, bool looping,
                        const de::String &node = "");

    int animationId(const de::String &name) const;

    const de::ModelDrawable::Appearance &appearance() const;

    // ModelDrawable::Animator
    void advanceTime(de::TimeSpan elapsed) override;
    double currentTime(int index) const override;
    de::Vec4f extraRotationForNode(const de::String &nodeName) const override;

    // Implements IObject.
    de::Record &       objectNamespace()       override;
    const de::Record & objectNamespace() const override;

    // ISerializable.
    void operator >> (de::Writer &to) const override;
    void operator << (de::Reader &from) override;

private:
    DE_PRIVATE(d)
};

} // namespace render

#endif // DE_CLIENT_RENDER_STATEANIMATOR_H
