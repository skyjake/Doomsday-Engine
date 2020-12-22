/** @file context.cpp
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

#include "gloom/render/context.h"
#include "gloom/render/gbuffer.h"
#include "gloom/render/maprender.h"
#include "gloom/render/materiallib.h"

using namespace de;

namespace gloom {

Context &Context::bindCamera(GLProgram &program)
{
    program << uCurrentTime
            << view.uCameraPos
            << view.uCameraMvpMatrix
            << view.uModelViewMatrix
            << view.uViewToWorldMatrix
            << view.uProjMatrix
            << view.uInverseProjMatrix
            << view.uViewToWorldRotate
            << view.uWorldToViewRotate;
    return *this;
}

Context &Context::bindGBuffer(GLProgram &program)
{
    program << gbuffer->uGBufferDiffuse()
            << gbuffer->uGBufferNormal()
            << gbuffer->uGBufferDepth()
            << gbuffer->uGBufferSpecGloss()
            << gbuffer->uGBufferEmissive()
            << gbuffer->uViewportSize();
    return *this;
}

Context &Context::bindMaterials(GLProgram &program)
{
    program << uDiffuseAtlas
            << uEmissiveAtlas
            << uSpecGlossAtlas
            << uNormalDisplAtlas
            << uEnvMap
            << uEnvIntensity
            << mapRender->materialLibrary().uTextureMetrics();
    return *this;
}

} // namespace gloom
