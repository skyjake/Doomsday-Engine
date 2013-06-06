/** @file glprogram.h  GL shader program.
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

#ifndef LIBGUI_GLPROGRAM_H
#define LIBGUI_GLPROGRAM_H

#include <de/libdeng2.h>
#include <de/IByteArray>
#include <de/Error>
#include <de/Asset>
#include <de/Observers>

#include "libgui.h"
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
    DENG2_ERROR(AllocError);

    /// Failed to link the program. @ingroup errors
    DENG2_ERROR(LinkerError);

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
    GLProgram &build(GLShader const *vertexShader, GLShader const *fragmentShader);

    GLProgram &build(IByteArray const &vertexShaderSource,
                     IByteArray const &fragmentShaderSource);

    GLProgram &operator << (GLUniform const &uniform);

    GLProgram &bind(GLUniform const &uniform);

    GLProgram &unbind(GLUniform const &uniform);

    void beginUse() const;

    void endUse() const;

    GLuint glName() const;

    int glUniformLocation(char const *uniformName) const;

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBGUI_GLPROGRAM_H
