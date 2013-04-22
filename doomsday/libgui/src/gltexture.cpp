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

DENG2_PIMPL(GLTexture)
{
    GLuint name;

    Instance(Public *i) : Base(i), name(0)
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
};

GLTexture::GLTexture() : d(new Instance(this))
{}

void GLTexture::glBindToUnit(int unit)
{
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, d->name);
}

} // namespace de
