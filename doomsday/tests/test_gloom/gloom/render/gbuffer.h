/** @file gbuffer.h
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

#ifndef GLOOM_GBUFFER_H
#define GLOOM_GBUFFER_H

#include <de/GLFramebuffer>
#include <de/GLTexture>

#include "gloom/render/render.h"

namespace gloom {

/**
 * G-buffer used for rendering.
 */
class GBuffer : public Render
{
public:
    GBuffer();

    void glInit(const Context &) override;
    void glDeinit() override;
    void render() override;

    void clear();
    void resize(const de::Vec2ui &size);
    de::Vec2ui size() const;
    void setDebugMode(int debugMode);

    de::GLFramebuffer &framebuf();
    de::GLUniform &uGBufferAlbedo();
    de::GLUniform &uGBufferNormal();
    de::GLUniform &uGBufferDepth();

private:
    DENG2_PRIVATE(d)
};

} // namespace gloom

#endif // GLOOM_GBUFFER_H
