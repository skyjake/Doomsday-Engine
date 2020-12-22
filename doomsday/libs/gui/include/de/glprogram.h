/** @file glprogram.h  GL shader program.
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

#ifndef LIBGUI_GLPROGRAM_H
#define LIBGUI_GLPROGRAM_H

#include <de/libcore.h>
#include <de/ibytearray.h>
#include <de/error.h>
#include <de/asset.h>
#include <de/observers.h>

#include "libgui.h"
#include "glbuffer.h"
#include "opengl.h"

namespace de {

class GLUniform;
class GLShader;

/**
 * GL shader program consisting of a vertex and fragment shaders.
 *
 * GLProgram instances work together with GLUniform to manage the program
 * state. To allow a particular uniform to be used in a program, it first
 * has to be bound to the program.
 *
 * When binding texture uniforms, the order of the binding calls is important
 * as it determines which texture sampling unit each of the textures is
 * allocated: the first bound texture uniform gets unit #0, the second one gets
 * unit #1, etc.
 *
 * @ingroup gl
 */
class LIBGUI_PUBLIC GLProgram : public Asset
{
public:
    /// Failed to allocate a new GL program object. @ingroup errors
    DE_ERROR(AllocError);

    /// Failed to link the program. @ingroup errors
    DE_ERROR(LinkerError);

public:
    GLProgram();

    /**
     * Resets the program back to an empty state. All uniform bindings are
     * removed.
     */
    void clear();

    /**
     * Builds a program out of two shaders. GLProgram retains a reference to
     * both shaders.
     *
     * @param vertexShader    Vertex shader.
     * @param fragmentShader  Fragment shader.
     *
     * @return Reference to this program.
     */
    GLProgram &build(const GLShader *vertexShader, const GLShader *fragmentShader);

    /**
     * Builds a program out of a list of shaders. GLProgram retains a reference to
     * all the shaders.
     *
     * @param shaders  One or more shaders.
     *
     * @return Reference to this program.
     */
    GLProgram &build(const List<const GLShader *> &shaders);

    GLProgram &build(const IByteArray &vertexShaderSource,
                     const IByteArray &fragmentShaderSource);

    void rebuildBeforeNextUse();

    void rebuild();

    GLProgram &operator << (const GLUniform &uniform);

    GLProgram &bind(const GLUniform &uniform);

    GLProgram &unbind(const GLUniform &uniform);

    /**
     * Takes this program into use. Only one GLProgram can be in use at a time.
     */
    void beginUse() const;

    void endUse() const;

    /**
     * Returns the program currently in use.
     */
    static const GLProgram *programInUse();

    GLuint glName() const;

    int glUniformLocation(const char *uniformName) const;

    bool glHasUniform(const char *uniformName) const;

    /**
     * Determines which attribute location is used for a particular attribute semantic.
     * These locations are available after the program has been successfully built
     * (linked).
     *
     * @param semantic  Attribute semantic.
     *
     * @return Attribute location, or -1 if the semantic is not used in the program.
     */
    int attributeLocation(internal::AttribSpec::Semantic semantic) const;

    bool validate() const;

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBGUI_GLPROGRAM_H
