/** @file databuffer.h
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

#ifndef GLOOM_DATABUFFER_H
#define GLOOM_DATABUFFER_H

#include <de/glbuffer.h>
#include <de/gltexture.h>
#include <de/gluniform.h>
#include <de/image.h>
#include <de/vector.h>

namespace gloom {

using namespace de;

template <typename Type>
struct DataBuffer
{
    GLUniform     var;
    GLBuffer      buf;
    GLuint        bufTex{0};
    List<Type>    data;
    Image::Format format;
    gfx::Usage     usage;

    DataBuffer(const char *uName, Image::Format format, gfx::Usage usage = gfx::Stream)
        : var{uName, GLUniform::SamplerBuffer}
        , buf{GLBuffer::Texture}
        , format(format)
        , usage(usage)
    {}

    void init(int count)
    {
        data.resize(count);
        data.fill(Type{});
    }

    void clear()
    {
        if (bufTex)
        {
            glDeleteTextures(1, &bufTex);
            bufTex = 0;
        }
        buf.clear();
        data.clear();
    }

    int size() const
    {
        return data.size();
    }

    void setData(uint index, const Type &value)
    {
        data[index] = value;
    }

    uint32_t append(const Type &value)
    {
        const uint32_t oldSize = data.size();
        data << value;
        return oldSize;
    }

    void update()
    {
        buf.setData(data.data(), data.size() * sizeof(Type), usage);

        if (!bufTex)
        {
            glGenTextures(1, &bufTex);
            var = bufTex;
        }
        glBindTexture(GL_TEXTURE_BUFFER, bufTex);
        glTexBuffer(GL_TEXTURE_BUFFER, Image::glFormat(format).internalFormat, buf.glName());
        LIBGUI_ASSERT_GL_OK();
        glBindTexture(GL_TEXTURE_BUFFER, 0);
    }
};

} // namespace gloom

#endif // GLOOM_DATABUFFER_H
