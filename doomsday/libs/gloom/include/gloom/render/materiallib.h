/** @file materiallib.h  Material library.
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

#ifndef GLOOM_RENDER_MATERIALLIB_H
#define GLOOM_RENDER_MATERIALLIB_H

#include "gloom/render/render.h"

#include <de/gluniform.h>

namespace gloom {

using namespace de;

/**
 * Material library.
 */
class MaterialLib : public Render
{
public:
    typedef Hash<String, uint32_t> Ids;

    enum MaterialFlag {
        Opaque      = 0x0,
        Transparent = 0x1, // refracts
        Reflective  = 0x2,
    };
    using MaterialFlags = Flags;

    enum MetricsFlag {
        Metrics_AnimationMask  = 1,
        Metrics_VerticalAspect = 2,
    };
    using MetricsFlags = Flags;

public:
    MaterialLib();

    void glInit(Context &) override;
    void glDeinit() override;
    void render() override;

    /**
     * Loads a set of materials, and unloads any previously loaded materials that are not
     * on the provided list. In practice, the texture images of the materials and the texture
     * metrics are copied to GPU textures/buffers. Only loaded materials can be used for
     * rendering.
     *
     * @param materials  List of materials to load.
     */
    void loadMaterials(const StringList &materials) const;

    const Ids &materials() const;

    bool isTransparent(const String &matId) const;

    GLUniform &uTextureMetrics();

private:
    DE_PRIVATE(d)
};

} // namespace gloom

#endif // GLOOM_RENDER_MATERIALLIB_H
