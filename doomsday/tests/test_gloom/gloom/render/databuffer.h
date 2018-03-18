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

#include <de/GLBuffer>
#include <de/GLTexture>
#include <de/GLUniform>
#include <de/Image>
#include <de/Vector>

namespace gloom {

using namespace de;

template <typename Type>
struct DataBuffer
{
    GLUniform var;
    GLBuffer buf;
    GLuint bufTex{0};
    QVector<Type> data;
    Image::Format format;
    gl::Usage usage;

    DataBuffer(const char *  uName,
               Image::Format format,
               gl::Usage     usage            = gl::Stream)
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
            LIBGUI_GL.glDeleteTextures(1, &bufTex);
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
        buf.setData(data.constData(), data.size() * sizeof(data[0]), usage);

        auto &GL = LIBGUI_GL;
        if (!bufTex)
        {
            GL.glGenTextures(1, &bufTex);
            var = bufTex;
        }
        GL.glBindTexture(GL_TEXTURE_BUFFER, bufTex);
        GL.glTexBuffer(GL_TEXTURE_BUFFER, Image::glFormat(format).internalFormat, buf.glName());
        LIBGUI_ASSERT_GL_OK();
        GL.glBindTexture(GL_TEXTURE_BUFFER, 0);
    }
};

} // namespace gloom

#endif // GLOOM_DATABUFFER_H
