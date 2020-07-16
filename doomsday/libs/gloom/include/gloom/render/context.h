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

#include <de/atlastexture.h>
#include <de/imagebank.h>
#include <de/glprogram.h>
#include <de/glshaderbank.h>
#include <de/gltextureframebuffer.h>

namespace gloom {

using namespace de;

class GBuffer;
class SSAO;
class Bloom;
class MapRender;
class LightRender;
class Tonemap;

struct Context {
    const ImageBank *     images;
    GLShaderBank *        shaders;
    const Map *           map;
    AtlasTexture **       atlas;
    View                  view;
    SSAO *                ssao;
    GBuffer *             gbuffer;
    Bloom *               bloom;
    GLTextureFramebuffer *framebuf;
    MapRender *           mapRender;
    LightRender *         lights;
    Tonemap *             tonemap;

    GLUniform uCurrentTime          {"uCurrentTime",      GLUniform::Float};
    GLUniform uCurrentFrameRate     {"uCurrentFrameRate", GLUniform::Float};

    GLUniform uDiffuseAtlas         {"uTextureAtlas[0]",  GLUniform::Sampler2D};
    GLUniform uSpecGlossAtlas       {"uTextureAtlas[1]",  GLUniform::Sampler2D};
    GLUniform uEmissiveAtlas        {"uTextureAtlas[2]",  GLUniform::Sampler2D};
    GLUniform uNormalDisplAtlas     {"uTextureAtlas[3]",  GLUniform::Sampler2D};
    GLUniform uEnvMap               {"uEnvMap",           GLUniform::SamplerCube};
    GLUniform uEnvIntensity         {"uEnvIntensity",     GLUniform::Vec3};

    GLUniform uLightMatrix          {"uLightMatrix",      GLUniform::Mat4};
    GLUniform uInverseLightMatrix   {"uInverseLightMatrix", GLUniform::Mat4};
    GLUniform uLightOrigin          {"uLightOrigin",      GLUniform::Vec3};
    GLUniform uLightFarPlane        {"uFarPlane",         GLUniform::Float};
    GLUniform uLightCubeMatrices    {"uCubeFaceMatrices", GLUniform::Mat4Array, 6};

    GLUniform uDebugTex             {"uDebugTex",         GLUniform::Sampler2D};
    GLUniform uDebugMode            {"uDebugMode",        GLUniform::Int};

    Context &bindCamera(GLProgram &);
    Context &bindGBuffer(GLProgram &);
    Context &bindMaterials(GLProgram &);
};

} // namespace gloom

#endif // GLOOM_CONTEXT_H
