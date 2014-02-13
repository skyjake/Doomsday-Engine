/** @file glshader.h  GL shader.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBGUI_GLSHADER_H
#define LIBGUI_GLSHADER_H

#include <de/libdeng2.h>
#include <de/Error>
#include <de/Counted>
#include <de/Asset>
#include <de/IByteArray>

#include "libgui.h"
#include "opengl.h"

namespace de {

/**
 * GL shader.
 *
 * Shader instances are reference-counted so that they can be shared by many
 * programs.
 *
 * @ingroup gl
 */
class LIBGUI_PUBLIC GLShader : public Counted, public Asset
{
public:
    enum Type { Vertex, Fragment };

    /// There was a failure related to OpenGL resource allocation. @ingroup errors
    DENG2_ERROR(AllocError);

    /// Shader source could not be compiled. @ingroup errors
    DENG2_ERROR(CompilerError);

public:
    GLShader();

    GLShader(Type shaderType, IByteArray const &source);

    Type type() const;

    GLuint glName() const;

    void clear();

    void compile(Type shaderType, IByteArray const &source);

    void recompile();

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBGUI_GLSHADER_H
