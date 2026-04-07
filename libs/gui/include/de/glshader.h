/** @file glshader.h  GL shader.
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

#ifndef LIBGUI_GLSHADER_H
#define LIBGUI_GLSHADER_H

#include <de/libcore.h>
#include <de/error.h>
#include <de/counted.h>
#include <de/asset.h>
#include <de/block.h>
#include <de/ibytearray.h>

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
    enum Type { Vertex, Geometry, Fragment };

    /// There was a failure related to OpenGL resource allocation. @ingroup errors
    DE_ERROR(AllocError);

    /// Shader source could not be compiled. @ingroup errors
    DE_ERROR(CompilerError);

public:
    GLShader();

    GLShader(Type shaderType, const IByteArray &source);

    Type type() const;

    GLuint glName() const;

    void clear();

    void compile(Type shaderType, const IByteArray &source);

//    void recompile();

    /**
     * Prefixes a piece of shader source code to another shader source. This takes
     * into account that certain elements must remain at the beginning of the source
     * (\#version).
     *
     * @param source  Main source where prefixing is done.
     * @param prefix  Source to prefix in the beginning of @a source.
     *
     * @return The resulting combination.
     */
    static String prefixToSource(const String &source, const String &prefix);

    static int constexpr MAX_BATCH_UNIFORMS = 64;

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBGUI_GLSHADER_H
