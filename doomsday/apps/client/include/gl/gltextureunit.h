/** @file gltextureunit.h GL texture unit.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef DE_CLIENT_GLTEXTUREUNIT_H
#define DE_CLIENT_GLTEXTUREUNIT_H

#include "resource/clienttexture.h"
#include <de/gltexture.h>
#include <de/vector.h>

namespace de {

/**
 * Of the available GL texture units, only this many will be utilized.
 */
#define MAX_TEX_UNITS           2 // Classic renderer only uses two.

/**
 * GL Texture unit config.
 *
 * @ingroup gl
 */
class GLTextureUnit
{
public:
    /// Managed GL textures encapsulate filter and wrapping management.
    TextureVariant *texture;

    /// Unmanged GL textures have an independent state.
    struct Unmanaged {
        GLuint glName;
        gfx::Wrapping wrapS;
        gfx::Wrapping wrapT;
        gfx::Filter filter;

        Unmanaged(GLuint glName       = 0,
                  gfx::Wrapping wrapS  = gfx::Repeat,
                  gfx::Wrapping wrapT  = gfx::Repeat,
                  gfx::Filter   filter = gfx::Linear)
            : glName(glName)
            , wrapS(wrapS)
            , wrapT(wrapT)
            , filter(filter)
        {}
        Unmanaged(const Unmanaged &other)
            : glName(other.glName)
            , wrapS(other.wrapS)
            , wrapT(other.wrapT)
            , filter(other.filter)
        {}

        Unmanaged &operator = (const Unmanaged &other) {
            glName = other.glName;
            wrapS  = other.wrapS;
            wrapT  = other.wrapT;
            filter = other.filter;
            return *this;
        }

        bool operator == (const Unmanaged &other) const {
            if(glName != other.glName) return false;
            if(wrapS != other.wrapS)   return false;
            if(wrapT != other.wrapT)   return false;
            if(filter != other.filter) return false;
            return true;
        }

        bool operator != (const Unmanaged &other) const {
            return !(*this == other);
        }
    } unmanaged;

    /// Shared properties:
    float opacity;
    Vec2f scale;
    Vec2f offset;

    GLTextureUnit()
        : texture(0)
        , opacity(1)
        , scale(1, 1)
    {}
    GLTextureUnit(TextureVariant &textureVariant,
                  const Vec2f &scale          = Vec2f(1, 1),
                  const Vec2f &offset         = Vec2f(0, 0),
                  float opacity               = 1)
        : texture(&textureVariant)
        , opacity(opacity)
        , scale(scale)
        , offset(offset)
    {}
    GLTextureUnit(GLuint textureGLName, gfx::Wrapping textureGLWrapS = gfx::Repeat,
        gfx::Wrapping textureGLWrapT = gfx::Repeat)
        : texture(0)
        , unmanaged(textureGLName, textureGLWrapS, textureGLWrapT)
        , opacity(1)
        , scale(1, 1)
    {}
    GLTextureUnit(const GLTextureUnit &other)
        : texture(other.texture)
        , unmanaged(other.unmanaged)
        , opacity(other.opacity)
        , scale(other.scale)
        , offset(other.offset)
    {}

    GLTextureUnit &operator = (const GLTextureUnit &other) {
        texture   = other.texture;
        unmanaged = other.unmanaged;
        opacity   = other.opacity;
        scale     = other.scale;
        offset    = other.offset;
        return *this;
    }

    bool operator == (const GLTextureUnit &other) const {
        if(texture)
        {
            if(texture != other.texture) return false;
        }
        else
        {
            if(unmanaged != other.unmanaged) return false;
        }
        if(!de::fequal(opacity, other.opacity)) return false;
        if(scale != other.scale) return false;
        if(offset != other.offset) return false;
        return true;
    }
    bool operator != (GLTextureUnit const other) const {
        return !(*this == other);
    }

    bool hasTexture() const {
        return (texture && texture->glName() != 0) || unmanaged.glName != 0;
    }

    GLuint getTextureGLName() const {
        return texture? texture->glName() : unmanaged.glName;
    }
};

} // namespace de

#endif // DE_CLIENT_GLTEXTUREUNIT_H
