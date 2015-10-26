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
#include <de/ModelDrawable>
#include <de/GLProgram>

namespace render {

/**
 * State-based object animator for `ModelDrawable`s.
 *
 * Used for both mobjs and psprites. The state and movement of the object
 * determine which animation sequences are started.
 */
class StateAnimator : public de::ModelDrawable::Animator
{
public:
    DENG2_ERROR(DefinitionError);

public:
    StateAnimator(de::DotPath const &id, Model const &model);

    Model const &model() const;

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

    de::ModelDrawable::Appearance const &appearance() const;

    enum BindOperation { Bind, Unbind };

    /**
     * Binds or unbinds uniforms that apply to all rendering passes.
     *
     * @param program    Program where bindings are made.
     * @param operation  Bind or unbind.
     */
    void bindUniforms(de::GLProgram &program, BindOperation operation) const;

    /**
     * Binds or unbinds uniforms that apply to a single rendering pass.
     *
     * @param program    Program where bindings are made.
     * @param passName   Name of the rendering pass. The render variables are
     *                   named, e.g., "render.(passName).uName".
     * @param operation  Bind or unbind.
     */
    void bindPassUniforms(de::GLProgram &program,
                          de::String const &passName,
                          BindOperation operation) const;

private:
    DENG2_PRIVATE(d)
};

} // namespace render

#endif // DENG_CLIENT_RENDER_STATEANIMATOR_H
