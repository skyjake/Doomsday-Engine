/** @file context.h
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

#ifndef GLOOM_CONTEXT_H
#define GLOOM_CONTEXT_H

#include "gloom/render/view.h"
#include "gloom/world/map.h"

#include <de/AtlasTexture>
#include <de/ImageBank>
#include <de/GLProgram>
#include <de/GLShaderBank>
#include <de/GLTextureFramebuffer>

namespace gloom {

class GBuffer;
class SSAO;
class LightRender;

struct Context {
    const de::ImageBank *     images;
    de::GLShaderBank *        shaders;
    const Map *               map;
    de::AtlasTexture *        atlas;
    View                      view;
    SSAO *                    ssao;
    GBuffer *                 gbuffer;
    de::GLTextureFramebuffer *framebuf;
    LightRender *             lights;

    de::GLUniform uAtlas        {"uTex",            de::GLUniform::Sampler2D};
    de::GLUniform uCurrentTime  {"uCurrentTime",    de::GLUniform::Float};
    de::GLUniform uLightMatrix  {"uLightMatrix",    de::GLUniform::Mat4};

    void bindTo(de::GLProgram &) const;
};

} // namespace gloom

#endif // GLOOM_CONTEXT_H
