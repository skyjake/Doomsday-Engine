/** @file gltexture.cpp  GL texture atlas.
 *
 * @authors Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/gltexture.h"
#include "de/glinfo.h"
#include "de/opengl.h"

namespace de {

namespace internal {

GLenum glComp(gfx::Comparison comp); // glstate.cpp

enum TextureFlag {
    AutoMips        = 0x1,
    MipmapAvailable = 0x2,
    ParamsChanged   = 0x4
};
using TextureFlags = Flags;

} // namespace internal

using namespace internal;
using namespace gfx;

DE_PIMPL(GLTexture)
{
    Size                size;
    Image::Format       format    = Image::Unknown;
    GLuint              name      = 0;
    GLenum              texTarget = GL_TEXTURE_2D;
    Filter              minFilter = Linear;
    Filter              magFilter = Linear;
    MipFilter           mipFilter = MipNone;
    Wraps               wrap{Repeat, Repeat};
    dfloat              maxAnisotropy = 1.0f;
    dfloat              maxLevel      = 1000.f;
    Vec4f               borderColor;
    gfx::ComparisonMode compareMode = gfx::CompareNone;
    gfx::Comparison     compareFunc = gfx::Always;
    TextureFlags        flags       = ParamsChanged;

    Impl(Public *i) : Base(i)
    {}

    ~Impl() override
    {
        release();
    }

    void alloc()
    {
        if (!name)
        {
            glGenTextures(1, &name);
        }
    }

    void release()
    {
        if (name)
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
        flags |= ParamsChanged;
        self().setState(NotReady);
    }

    bool isCube() const
    {
        return texTarget == GL_TEXTURE_CUBE_MAP;
    }

    static GLenum glCompareMode(gfx::ComparisonMode mode)
    {
       switch (mode)
       {
       case CompareNone: return GL_NONE;
       case CompareRefToTexture: return GL_COMPARE_REF_TO_TEXTURE;
       }
       return GL_NONE;
    }

    static GLenum glWrap(gfx::Wrapping w)
    {
        switch (w)
        {
        case Repeat:         return GL_REPEAT;
        case RepeatMirrored: return GL_MIRRORED_REPEAT;
        case ClampToEdge:    return GL_CLAMP_TO_EDGE;
        case ClampToBorder:  return GL_CLAMP_TO_BORDER;
        }
        return GL_REPEAT;
    }

    static GLenum glMinFilter(gfx::Filter min, gfx::MipFilter mip)
    {
        if (mip == MipNone)
        {
            if (min == Nearest) return GL_NEAREST;
            if (min == Linear)  return GL_LINEAR;
        }
        else if (mip == MipNearest)
        {
            if (min == Nearest) return GL_NEAREST_MIPMAP_NEAREST;
            if (min == Linear)  return GL_LINEAR_MIPMAP_NEAREST;
        }
        else // MipLinear
        {
            if (min == Nearest) return GL_NEAREST_MIPMAP_LINEAR;
            if (min == Linear)  return GL_LINEAR_MIPMAP_LINEAR;
        }
        return GL_NEAREST;
    }

    static GLenum glFace(gfx::CubeFace face)
    {
        switch (face)
        {
        case PositiveX: return GL_TEXTURE_CUBE_MAP_POSITIVE_X;
        case NegativeX: return GL_TEXTURE_CUBE_MAP_NEGATIVE_X;
        case PositiveY: return GL_TEXTURE_CUBE_MAP_POSITIVE_Y;
        case NegativeY: return GL_TEXTURE_CUBE_MAP_NEGATIVE_Y;
        case PositiveZ: return GL_TEXTURE_CUBE_MAP_POSITIVE_Z;
        case NegativeZ: return GL_TEXTURE_CUBE_MAP_NEGATIVE_Z;
        }
        return GL_TEXTURE_CUBE_MAP_POSITIVE_X;
    }

    void glBind() const
    {
        glBindTexture(texTarget, name); LIBGUI_ASSERT_GL_OK();
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
        glTexParameteri (texTarget, GL_TEXTURE_WRAP_S,       glWrap(wrap.x));
        glTexParameteri (texTarget, GL_TEXTURE_WRAP_T,       glWrap(wrap.y));
        glTexParameteri (texTarget, GL_TEXTURE_MAG_FILTER,   magFilter == Nearest? GL_NEAREST : GL_LINEAR);
        glTexParameteri (texTarget, GL_TEXTURE_MIN_FILTER,   glMinFilter(minFilter, mipFilter));
        glTexParameterf (texTarget, GL_TEXTURE_MAX_LEVEL,    maxLevel);
        glTexParameterfv(texTarget, GL_TEXTURE_BORDER_COLOR, &borderColor[0]);
        glTexParameteri (texTarget, GL_TEXTURE_COMPARE_MODE, glCompareMode(compareMode));
        glTexParameteri (texTarget, GL_TEXTURE_COMPARE_FUNC, internal::glComp(compareFunc));

        if (GLInfo::extensions().EXT_texture_filter_anisotropic)
        {
            glTexParameterf(texTarget, gl33ext::GL_TEXTURE_MAX_ANISOTROPY_EXT, maxAnisotropy);
        }

        LIBGUI_ASSERT_GL_OK();

        flags &= ~ParamsChanged;
    }

    void glImage(int level, const Size &size, const GLPixelFormat &glFormat,
                 const void *data, CubeFace face = PositiveX)
    {
        /*qDebug() << "glTexImage2D:" << name << (isCube()? glFace(face) : texTarget)
                << level << internalFormat << size.x << size.y << 0
                << glFormat.format << glFormat.type << data;*/

        if (data) glPixelStorei(GL_UNPACK_ALIGNMENT, GLint(glFormat.rowStartAlignment));
        glTexImage2D(isCube() ? glFace(face) : texTarget,
                     level,
                     glFormat.internalFormat,
                     size.x,
                     size.y,
                     0,
                     glFormat.format,
                     glFormat.type,
                     data);
        LIBGUI_ASSERT_GL_OK();
    }

    void glSubImage(int level, const Vec2i &pos, const Size &size,
                    const GLPixelFormat &glFormat, const void *data, CubeFace face = PositiveX)
    {
        if (data) glPixelStorei(GL_UNPACK_ALIGNMENT, GLint(glFormat.rowStartAlignment));
        glTexSubImage2D(isCube() ? glFace(face) : texTarget,
                        level,
                        pos.x,
                        pos.y,
                        size.x,
                        size.y,
                        glFormat.format,
                        glFormat.type,
                        data);
        LIBGUI_ASSERT_GL_OK();
    }

    void glSubImage(int level, const Rectanglei &rect, const Image &image,
                    CubeFace face = PositiveX)
    {
        const auto &glFormat = image.glFormat();

        glPixelStorei(GL_UNPACK_ALIGNMENT,  GLint(glFormat.rowStartAlignment));
        glPixelStorei(GL_UNPACK_ROW_LENGTH, GLint(image.width()));

        const int bytesPerPixel = image.depth() / 8;

        glTexSubImage2D(isCube() ? glFace(face) : texTarget,
                        level,
                        rect.left(),
                        rect.top(),
                        rect.width(),
                        rect.height(),
                        glFormat.format,
                        glFormat.type,
                        static_cast<const dbyte *>(image.bits()) + bytesPerPixel * rect.left() +
                            image.stride() * rect.top());

        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

        LIBGUI_ASSERT_GL_OK();
    }
};

GLTexture::GLTexture() : d(new Impl(this))
{}

GLTexture::GLTexture(GLuint existingTexture, const Size &size) : d(new Impl(this))
{
    d->size = size;
    d->name = existingTexture;
    d->flags |= ParamsChanged;
}

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

void GLTexture::setMaxAnisotropy(dfloat maxAnisotropy)
{
    d->maxAnisotropy = maxAnisotropy;
    d->flags |= ParamsChanged;
}

void GLTexture::setMaxLevel(dfloat maxLevel)
{
    d->maxLevel = maxLevel;
    d->flags |= ParamsChanged;
}

void GLTexture::setBorderColor(const Vec4f &color)
{
    d->borderColor = color;
    d->flags |= ParamsChanged;
}

void GLTexture::setComparisonMode(gfx::ComparisonMode mode, gfx::Comparison func)
{
    d->compareMode = mode;
    d->compareFunc = func;
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

dfloat GLTexture::maxAnisotropy() const
{
    return d->maxAnisotropy;
}

dfloat GLTexture::maxLevel() const
{
    return d->maxLevel;
}

bool GLTexture::isCubeMap() const
{
    return d->isCube();
}

void GLTexture::setAutoGenMips(bool genMips)
{
    applyFlagOperation(d->flags, AutoMips, genMips);
}

bool GLTexture::autoGenMips() const
{
    return d->flags.testFlag(AutoMips);
}

void GLTexture::setUndefinedImage(const GLTexture::Size &size, Image::Format format, int level)
{
    d->texTarget = GL_TEXTURE_2D;
    d->size = size;
    d->format = format;

    d->alloc();
    d->glBind();
    d->glImage(level, size, Image::glFormat(format), nullptr);
    d->glUnbind();

    setState(Ready);
}

void GLTexture::setUndefinedImage(CubeFace face, const GLTexture::Size &size,
                                  Image::Format format, int level)
{
    d->texTarget = GL_TEXTURE_CUBE_MAP;
    d->size = size;
    d->format = format;

    d->alloc();
    d->glBind();
    d->glImage(level, size, Image::glFormat(format), nullptr, face);
    d->glUnbind();

    setState(Ready);
}

void GLTexture::setUndefinedContent(const Size &size, const GLPixelFormat &glFormat, int level)
{
    d->texTarget = GL_TEXTURE_2D;
    d->size = size;
    d->format = Image::Unknown;

    d->alloc();
    d->glBind();
    d->glImage(level, size, glFormat, nullptr);
    d->glUnbind();

    setState(Ready);
}

void GLTexture::setUndefinedContent(CubeFace face, const Size &size, const GLPixelFormat &glFormat, int level)
{
    d->texTarget = GL_TEXTURE_CUBE_MAP;
    d->size = size;
    d->format = Image::Unknown;

    d->alloc();
    d->glBind();
    d->glImage(level, size, glFormat, nullptr, face);
    d->glUnbind();

    setState(Ready);
}

void GLTexture::setDepthStencilContent(const Size &size)
{
    setUndefinedContent(size, GLPixelFormat(GL_DEPTH24_STENCIL8,
                                            GL_DEPTH_STENCIL,
                                            GL_UNSIGNED_INT_24_8));
}

void GLTexture::setImage(const Image &image, int level)
{
    d->texTarget = GL_TEXTURE_2D;
    d->size = image.size();
    d->format = image.format();

    d->alloc();
    d->glBind();
    d->glImage(level, image.size(), image.glFormat(), image.bits());
    d->glUnbind();

    if (!level && d->flags.testFlag(AutoMips))
    {
        generateMipmap();
    }

    setState(Ready);
}

void GLTexture::setImage(CubeFace face, const Image &image, int level)
{
    d->texTarget = GL_TEXTURE_CUBE_MAP;
    d->size = image.size();
    d->format = image.format();

    d->alloc();
    d->glBind();
    d->glImage(level, image.size(), image.glFormat(), image.bits(), face);
    d->glUnbind();

    if (!level && d->flags.testFlag(AutoMips))
    {
        generateMipmap();
    }

    setState(Ready);
}

void GLTexture::setSubImage(const Image &image, const Vec2i &pos, int level)
{
    d->texTarget = GL_TEXTURE_2D;

    d->alloc();
    d->glBind();
    d->glSubImage(level, pos, image.size(), image.glFormat(), image.bits());
    d->glUnbind();

    if (!level && d->flags.testFlag(AutoMips))
    {
        generateMipmap();
    }
}

void GLTexture::setSubImage(const Image &image, const Rectanglei &rect, int level)
{
    d->texTarget = GL_TEXTURE_2D;

    d->alloc();
    d->glBind();
    d->glSubImage(level, rect, image);
    d->glUnbind();

    if (!level && d->flags.testFlag(AutoMips))
    {
        generateMipmap();
    }
}

void GLTexture::setSubImage(CubeFace face, const Image &image, const Vec2i &pos, int level)
{
    d->texTarget = GL_TEXTURE_CUBE_MAP;

    d->alloc();
    d->glBind();
    d->glSubImage(level, pos, image.size(), image.glFormat(), image.bits(), face);
    d->glUnbind();

    if (!level && d->flags.testFlag(AutoMips))
    {
        generateMipmap();
    }
}

void GLTexture::setSubImage(CubeFace face, const Image &image, const Rectanglei &rect, int level)
{
    d->texTarget = GL_TEXTURE_CUBE_MAP;

    d->alloc();
    d->glBind();
    d->glSubImage(level, rect, image, face);
    d->glUnbind();

    if (!level && d->flags.testFlag(AutoMips))
    {
        generateMipmap();
    }
}

void GLTexture::generateMipmap()
{
    if (d->name)
    {
        d->glBind();
        glGenerateMipmap(d->texTarget); LIBGUI_ASSERT_GL_OK();
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
    if (!isReady()) return 0;
    return d->flags.testFlag(MipmapAvailable)? levelsForSize(d->size) : 1;
}

GLTexture::Size GLTexture::levelSize(int level) const
{
    if (level < 0) return Size();
    return levelSize(d->size, level);
}

GLuint GLTexture::glName() const
{
    return d->name;
}

void GLTexture::glBindToUnit(duint unit) const
{
    glActiveTexture(GL_TEXTURE0 + unit);

    aboutToUse();

    d->glBind();

    if (d->flags.testFlag(ParamsChanged))
    {
        d->glUpdateParamsOfBoundTexture();
    }
}

void GLTexture::glApplyParameters()
{
    if (d->flags.testFlag(ParamsChanged))
    {
        d->glBind();
        d->glUpdateParamsOfBoundTexture();
        d->glUnbind();
    }
}

Image::Format GLTexture::imageFormat() const
{
    return d->format;
}

GLTexture::Size GLTexture::maximumSize()
{
    int v = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &v); LIBGUI_ASSERT_GL_OK();
    return Size(v, v);
}

void GLTexture::aboutToUse() const
{
    // nothing to do
}

int GLTexture::levelsForSize(const GLTexture::Size &size)
{
    int mipLevels = 0;
    duint w = size.x;
    duint h = size.y;
    while (w > 1 || h > 1)
    {
        w = de::max(1u, w >> 1);
        h = de::max(1u, h >> 1);
        mipLevels++;
    }
    return mipLevels;
}

GLTexture::Size GLTexture::levelSize(const GLTexture::Size &size0, int level)
{
    Size s = size0;
    for (int i = 0; i < level; ++i)
    {
        s.x = de::max(1u, s.x >> 1);
        s.y = de::max(1u, s.y >> 1);
    }
    return s;
}

} // namespace de
