/** @file mobjanimator.h
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

#ifndef DENG_CLIENT_RENDER_MOBJANIMATOR_H
#define DENG_CLIENT_RENDER_MOBJANIMATOR_H

#include "render/modelrenderer.h"
#include <de/ModelDrawable>
#include <de/GLProgram>

/**
 * Mobj-specific model animator.
 *
 * The state and movement of the mobj determine which animation sequences are started.
 * Also used for psprites, since they are pretty much light-weight mobjs.
 */
class MobjAnimator : public de::ModelDrawable::Animator
{
public:
    DENG2_ERROR(DefinitionError);

public:
    MobjAnimator(de::DotPath const &id, de::ModelDrawable const &model);

    /**
     * Sets the namespace of the animator's owner. Available as "self" in animation
     * scripts.
     *
     * @param names  Owner's namespace.
     */
    void setOwnerNamespace(de::Record &names);

    void triggerByState(de::String const &stateName);

    void advanceTime(de::TimeDelta const &elapsed);

    de::ddouble currentTime(int index) const;

    void bindUniforms(de::GLProgram &program) const;

    void unbindUniforms(de::GLProgram &program) const;

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_CLIENT_RENDER_MOBJANIMATOR_H
