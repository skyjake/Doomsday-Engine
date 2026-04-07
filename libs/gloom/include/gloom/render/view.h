/** @file view.h
 *
 * @authors Copyright (c) 2018 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef GLOOM_VIEW_H
#define GLOOM_VIEW_H

#include <de/gluniform.h>
#include "gloom/render/icamera.h"

namespace gloom {

struct View
{
    const ICamera *camera = nullptr;

    de::GLUniform uCameraPos            {"uCameraPos",          de::GLUniform::Vec4};
    de::GLUniform uCameraMvpMatrix      {"uCameraMvpMatrix",    de::GLUniform::Mat4};
    de::GLUniform uModelViewMatrix      {"uModelViewMatrix",    de::GLUniform::Mat4};
    de::GLUniform uViewToWorldMatrix    {"uViewToWorldMatrix",  de::GLUniform::Mat4};
    de::GLUniform uProjMatrix           {"uProjMatrix",         de::GLUniform::Mat4};
    de::GLUniform uInverseProjMatrix    {"uInverseProjMatrix",  de::GLUniform::Mat4};
    de::GLUniform uWorldToViewRotate    {"uWorldToViewRotate",  de::GLUniform::Mat3};
    de::GLUniform uViewToWorldRotate    {"uViewToWorldRotate",  de::GLUniform::Mat3};

    void setCamera(const ICamera &camera);
};

} // namespace gloom

#endif // GLOOM_VIEW_H
