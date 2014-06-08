/** @file gltextureunit.h GL texture unit.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DENG_CLIENT_GLTEXTUREUNIT_H
#define DENG_CLIENT_GLTEXTUREUNIT_H

#include "Texture"
#include <de/GLTexture>
#include <de/Vector>

/**
 * Of the available GL texture units, only this many will be utilized.
 *
 * @todo Find a more suitable home (here because all other GL-domain headers
 * also include system headers, which aren't needed in components whose API(s)
 * do not depend on them (and pollute on Windows)).
 */
#define MAX_TEX_UNITS           2 // More aren't currently used.

namespace de {

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
        gl::Wrapping wrapS;
        gl::Wrapping wrapT;
        gl::Filter filter;

        Unmanaged(GLuint glName       = 0,
                  gl::Wrapping wrapS  = gl::Repeat,
                  gl::Wrapping wrapT  = gl::Repeat,
                  gl::Filter   filter = gl::Linear)
            : glName(glName)
            , wrapS(wrapS)
            , wrapT(wrapT)
            , filter(filter)
        {}
        Unmanaged(Unmanaged const &other)
            : glName(other.glName)
            , wrapS(other.wrapS)
            , wrapT(other.wrapT)
            , filter(other.filter)
        {}

        Unmanaged &operator = (Unmanaged const &other) {
            glName = other.glName;
            wrapS  = other.wrapS;
            wrapT  = other.wrapT;
            filter = other.filter;
            return *this;
        }

        bool operator == (Unmanaged const &other) const {
            if(glName != other.glName) return false;
            if(wrapS != other.wrapS)   return false;
            if(wrapT != other.wrapT)   return false;
            if(filter != other.filter) return false;
            return true;
        }

        bool operator != (Unmanaged const &other) const {
            return !(*this == other);
        }
    } unmanaged;

    /// Shared properties:
    float opacity;
    Vector2f scale;
    Vector2f offset;

    GLTextureUnit()
        : texture(0)
        , opacity(1)
        , scale(1, 1)
    {}
    GLTextureUnit(TextureVariant &textureVariant,
                  Vector2f const &scale          = Vector2f(1, 1),
                  Vector2f const &offset         = Vector2f(0, 0),
                  float opacity                  = 1)
        : texture(&textureVariant)
        , opacity(opacity)
        , scale(scale)
        , offset(offset)
    {}
    GLTextureUnit(GLuint textureGLName, gl::Wrapping textureGLWrapS = gl::Repeat,
        gl::Wrapping textureGLWrapT = gl::Repeat)
        : texture(0)
        , unmanaged(textureGLName, textureGLWrapS, textureGLWrapT)
        , opacity(1)
        , scale(1, 1)
    {}
    GLTextureUnit(GLTextureUnit const &other)
        : texture(other.texture)
        , unmanaged(other.unmanaged)
        , opacity(other.opacity)
        , scale(other.scale)
        , offset(other.offset)
    {}

    GLTextureUnit &operator = (GLTextureUnit const &other) {
        texture   = other.texture;
        unmanaged = other.unmanaged;
        opacity   = other.opacity;
        scale     = other.scale;
        offset    = other.offset;
        return *this;
    }

    bool hasTexture() const {
        return (texture && texture->glName() != 0) || unmanaged.glName != 0;
    }

    GLuint getTextureGLName() const {
        return texture? texture->glName() : unmanaged.glName;
    }
};

} // namespace de

#endif // DENG_CLIENT_GLTEXTUREUNIT_H
