/** @file gltexture.cpp  GL texture atlas.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/GLTexture"
#include "de/gui/opengl.h"

namespace de {

namespace internal
{
    enum TextureFlag {
        AutoMips,
        MipmapAvailable,
        ParamsChanged
    };
    Q_DECLARE_FLAGS(TextureFlags, TextureFlag)
}

Q_DECLARE_OPERATORS_FOR_FLAGS(internal::TextureFlags)

using namespace internal;
using namespace gl;

DENG2_PIMPL(GLTexture)
{
    Size size;
    GLuint name;
    GLenum texTarget;
    Filter minFilter;
    Filter magFilter;
    MipFilter mipFilter;
    Wraps wrap;
    TextureFlags flags;

    Instance(Public *i)
        : Base(i),
          name(0),
          texTarget(GL_TEXTURE_2D),
          minFilter(Linear), magFilter(Linear), mipFilter(MipNone),
          wrap(Wraps(Repeat, Repeat)),
          flags(ParamsChanged)
    {}

    ~Instance()
    {
        release();
    }

    void alloc()
    {
        if(!name)
        {
            glGenTextures(1, &name);
        }
    }

    void release()
    {
        if(name)
        {
            glDeleteTextures(1, &name);
            name = 0;
        }
    }

    void clear()
    {
        release();
        size = Size();
        texTarget = GL_TEXTURE_2D;
        self.setState(NotReady);
    }

    bool isCube() const
    {
        return texTarget == GL_TEXTURE_CUBE_MAP;
    }

    static GLenum glWrap(gl::Wrapping w)
    {
        switch(w)
        {
        case Repeat:         return GL_REPEAT;
        case RepeatMirrored: return GL_MIRRORED_REPEAT;
        case ClampToEdge:    return GL_CLAMP_TO_EDGE;
        }
        return GL_REPEAT;
    }

    static GLenum glMinFilter(gl::Filter min, gl::MipFilter mip)
    {
        if(mip == MipNone)
        {
            if(min == Nearest) return GL_NEAREST;
            if(min == Linear)  return GL_LINEAR;
        }
        else if(mip == MipNearest)
        {
            if(min == Nearest) return GL_NEAREST_MIPMAP_NEAREST;
            if(min == Linear)  return GL_LINEAR_MIPMAP_NEAREST;
        }
        else // MipLinear
        {
            if(min == Nearest) return GL_NEAREST_MIPMAP_LINEAR;
            if(min == Linear)  return GL_LINEAR_MIPMAP_LINEAR;
        }
        return GL_NEAREST;
    }

    static GLenum glFace(gl::CubeFace face)
    {
        switch(face)
        {
        case PositiveX: return GL_TEXTURE_CUBE_MAP_POSITIVE_X;
        case PositiveY: return GL_TEXTURE_CUBE_MAP_POSITIVE_Y;
        case PositiveZ: return GL_TEXTURE_CUBE_MAP_POSITIVE_Z;
        case NegativeX: return GL_TEXTURE_CUBE_MAP_NEGATIVE_X;
        case NegativeY: return GL_TEXTURE_CUBE_MAP_NEGATIVE_Y;
        case NegativeZ: return GL_TEXTURE_CUBE_MAP_NEGATIVE_Z;
        }
        return GL_TEXTURE_CUBE_MAP_POSITIVE_X;
    }

    void glBind() const
    {
        glBindTexture(texTarget, self.isReady()? name : 0);
    }

    void glUnbind() const
    {
        glBindTexture(texTarget, 0);
    }

    /**
     * Update the OpenGL texture parameters. You must bind the texture before
     * calling.
     */
    void glUpdateParamsOfBoundTexture()
    {
        glTexParameteri(texTarget, GL_TEXTURE_WRAP_S,     glWrap(wrap.x));
        glTexParameteri(texTarget, GL_TEXTURE_WRAP_T,     glWrap(wrap.y));
        glTexParameteri(texTarget, GL_TEXTURE_MAG_FILTER, minFilter == Nearest? GL_NEAREST : GL_LINEAR);
        glTexParameteri(texTarget, GL_TEXTURE_MIN_FILTER, glMinFilter(minFilter, mipFilter));

        flags &= ~ParamsChanged;
    }

    void glImage(int level, Size const &size, Image::GLFormat const &glFormat,
                 void const *data, CubeFace face = PositiveX)
    {
        glPixelStorei(GL_UNPACK_ALIGNMENT, glFormat.rowAlignment);
        glTexImage2D(isCube()? glFace(face) : texTarget,
                     level, glFormat.format, size.x, size.y, 0,
                     glFormat.format, glFormat.type, data);
    }

    void glSubImage(int level, Vector2i const &pos, Size const &size,
                    Image::GLFormat const &glFormat, void const *data, CubeFace face = PositiveX)
    {
        glPixelStorei(GL_UNPACK_ALIGNMENT, glFormat.rowAlignment);
        glTexSubImage2D(isCube()? glFace(face) : texTarget,
                        level, pos.x, pos.y, size.x, size.y,
                        glFormat.format, glFormat.type, data);
    }
};

GLTexture::GLTexture() : d(new Instance(this))
{}

void GLTexture::clear()
{
    d->clear();
}

void GLTexture::setMagFilter(Filter magFilter)
{
    d->magFilter = magFilter;
    d->flags |= ParamsChanged;
}

void GLTexture::setMinFilter(Filter minFilter, MipFilter mipFilter)
{
    d->minFilter = minFilter;
    d->mipFilter = mipFilter;
    d->flags |= ParamsChanged;
}

void GLTexture::setWrapS(Wrapping mode)
{
    d->wrap.x = mode;
    d->flags |= ParamsChanged;
}

void GLTexture::setWrapT(Wrapping mode)
{
    d->wrap.y = mode;
    d->flags |= ParamsChanged;
}

Filter GLTexture::minFilter() const
{
    return d->minFilter;
}

Filter GLTexture::magFilter() const
{
    return d->magFilter;
}

MipFilter GLTexture::mipFilter() const
{
    return d->mipFilter;
}

Wrapping GLTexture::wrapS() const
{
    return d->wrap.x;
}

Wrapping GLTexture::wrapT() const
{
    return d->wrap.y;
}

GLTexture::Wraps GLTexture::wrap() const
{
    return d->wrap;
}

bool GLTexture::isCubeMap() const
{
    return d->isCube();
}

void GLTexture::setAutoGenMips(bool genMips)
{
    if(genMips)
        d->flags |= AutoMips;
    else
        d->flags &= ~AutoMips;
}

bool GLTexture::autoGenMips() const
{
    return d->flags.testFlag(AutoMips);
}

void GLTexture::setUndefinedImage(GLTexture::Size const &size, Image::Format format, int level)
{
    d->texTarget = GL_TEXTURE_2D;

    d->alloc();
    d->glBind();
    d->glImage(level, size, Image::glFormat(format), NULL);
    d->glUnbind();

    setState(Ready);
}

void GLTexture::setUndefinedImage(CubeFace face, GLTexture::Size const &size,
                                  Image::Format format, int level)
{
    d->texTarget = GL_TEXTURE_CUBE_MAP;

    d->alloc();
    d->glBind();
    d->glImage(level, size, Image::glFormat(format), NULL, face);
    d->glUnbind();

    setState(Ready);
}

void GLTexture::setImage(Image const &image, int level)
{
    d->texTarget = GL_TEXTURE_2D;

    d->alloc();
    d->glBind();
    d->glImage(level, image.size(), image.glFormat(), image.bits());
    d->glUnbind();

    if(!level && d->flags.testFlag(AutoMips))
    {
        generateMipmap();
    }

    setState(Ready);
}

void GLTexture::setImage(CubeFace face, Image const &image, int level)
{
    d->texTarget = GL_TEXTURE_CUBE_MAP;

    d->alloc();
    d->glBind();
    d->glImage(level, image.size(), image.glFormat(), image.bits(), face);
    d->glUnbind();

    if(!level && d->flags.testFlag(AutoMips))
    {
        generateMipmap();
    }

    setState(Ready);
}

void GLTexture::setSubImage(Image const &image, Vector2i const &pos, int level)
{
    d->texTarget = GL_TEXTURE_2D;

    d->alloc();
    d->glBind();
    d->glSubImage(level, pos, image.size(), image.glFormat(), image.bits());
    d->glUnbind();

    if(!level && d->flags.testFlag(AutoMips))
    {
        generateMipmap();
    }
}

void GLTexture::setSubImage(CubeFace face, Image const &image, Vector2i const &pos, int level)
{
    d->texTarget = GL_TEXTURE_CUBE_MAP;

    d->alloc();
    d->glBind();
    d->glSubImage(level, pos, image.size(), image.glFormat(), image.bits(), face);
    d->glUnbind();

    if(!level && d->flags.testFlag(AutoMips))
    {
        generateMipmap();
    }
}

void GLTexture::generateMipmap()
{
    if(d->name)
    {
        d->glBind();
        glGenerateMipmap(d->texTarget);
        d->glUnbind();

        d->flags |= MipmapAvailable;
    }
}

GLTexture::Size GLTexture::size() const
{
    return d->size;
}

int GLTexture::mipLevels() const
{
    if(!isReady()) return 0;
    return d->flags.testFlag(MipmapAvailable)? levelsForSize(d->size) : 1;
}

GLTexture::Size GLTexture::levelSize(int level) const
{
    if(level < 0) return Size();
    return levelSize(d->size, level);
}

GLuint GLTexture::glName() const
{
    return d->name;
}

void GLTexture::glBindToUnit(int unit) const
{
    glActiveTexture(GL_TEXTURE0 + unit);
    d->glBind();

    if(d->flags.testFlag(ParamsChanged))
    {
        d->glUpdateParamsOfBoundTexture();
    }
}

int GLTexture::levelsForSize(GLTexture::Size const &size)
{
    int mipLevels = 0;
    duint w = size.x;
    duint h = size.y;
    while(w > 1 || h > 1)
    {
        w = de::max(1u, w >> 1);
        h = de::max(1u, h >> 1);
        mipLevels++;
    }
    return mipLevels;
}

GLTexture::Size GLTexture::levelSize(GLTexture::Size const &size0, int level)
{
    Size s = size0;
    for(int i = 0; i < level; ++i)
    {
        s.x = de::max(1u, s.x >> 1);
        s.y = de::max(1u, s.y >> 1);
    }
    return s;
}

} // namespace de
