/** @file gluniform.h  GL uniform.
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

#ifndef LIBGUI_GLUNIFORM_H
#define LIBGUI_GLUNIFORM_H

#include <de/libcore.h>
#include <de/vector.h>
#include <de/matrix.h>
#include <de/observers.h>

#include "libgui.h"

namespace de {

class GLProgram;
class GLTexture;

/**
 * Constant variable or a sampler in a shader.
 *
 * GLUniform's public interface allows the uniform value to be manipulated like
 * any other native variable (assignment, arithmetic, etc.). Think of GLUniform
 * instances as a native manifestation of shader uniform/attribute variables.
 *
 * The value of the uniform is stored locally in the GLUniform instance. When
 * the uniform has been bound to programs and its value changes, the programs
 * are notified and they mark the uniform as changed. When the program is then
 * later taken into use, the updated value of the changed uniforms is sent to
 * GL.
 *
 * @ingroup gl
 */
class LIBGUI_PUBLIC GLUniform
{
public:
    enum Type {
        Int,
        UInt,
        Float,
        Vec2,
        Vec3,
        Vec4,
        Mat3,
        Mat4,
        Sampler2D,
        SamplerCube,
        SamplerBuffer,
        IntArray,
        FloatArray,
        Vec2Array,
        Vec3Array,
        Vec4Array,
        Mat4Array
    };

    /**
     * Notified when the value of the uniform changes.
     */
    DE_AUDIENCE(ValueChange, void uniformValueChanged(GLUniform &))

    /**
     * Notified when the uniform instance is deleted.
     */
    DE_AUDIENCE(Deletion, void uniformDeleted(GLUniform &))

public:
    GLUniform(const Block &nameInShader, Type uniformType, duint elements = 1);

    void setName(const Block &nameInShader);

    /**
     * Returns the name of the uniform as it appears in shaders.
     */
    Block name() const;

    /**
     * Returns the value type of the shader.
     */
    Type type() const;

    bool isSampler() const;
    void bindSamplerTexture(duint unit) const;

    GLUniform &operator=(dint value);
    GLUniform &operator=(duint value);
    GLUniform &operator=(dfloat value);
    GLUniform &operator=(ddouble value);
    GLUniform &operator=(const Vec2f &vec);
    GLUniform &operator=(const Vec3f &vec);
    GLUniform &operator=(const Vec4f &vec);
    GLUniform &operator=(const Mat3f &vec);
    GLUniform &operator=(const Mat4f &mat);
    GLUniform &operator=(const GLTexture &texture);
    GLUniform &operator=(const GLTexture *texture);

    GLUniform &set(duint elementIndex, dfloat value);
    GLUniform &set(duint elementIndex, dint value);
    GLUniform &set(duint elementIndex, const Vec2f &vec);
    GLUniform &set(duint elementIndex, const Vec3f &vec);
    GLUniform &set(duint elementIndex, const Vec4f &vec);
    GLUniform &set(duint elementIndex, const Mat4f &mat);

    GLUniform &setUsedElementCount(duint elementCount);

    GLUniform &set(const dint *intArray, dsize count);
    GLUniform &set(const float *floatArray, dsize count);
    GLUniform &set(const Vec2f *vectorArray, dsize count);
    GLUniform &set(const Vec3f *vectorArray, dsize count);
    GLUniform &set(const Vec4f *vectorArray, dsize count);
    GLUniform &set(const Mat4f *mat4Array, dsize count);

    operator dint() const              { return toInt(); }
    operator duint() const             { return toUInt(); }
    operator dfloat() const            { return toFloat(); }
    operator ddouble() const           { return ddouble(toFloat()); }
    operator Vec2f() const             { return toVec2f(); }
    operator Vec3f() const             { return toVec3f(); }
    operator Vec4f() const             { return toVec4f(); }
    operator const Mat3f &() const     { return toMat3f(); }
    operator const Mat4f &() const     { return toMat4f(); }
    operator const GLTexture *() const { return texture(); }

    dint toInt() const;
    duint toUInt() const;
    dfloat toFloat() const;
    const Vec2f &toVec2f() const;
    const Vec3f &toVec3f() const;
    const Vec4f &toVec4f() const;
    const Mat3f &toMat3f() const;
    const Mat4f &toMat4f() const;

    const GLTexture *texture() const;

    /**
     * Updates the value of the uniform in a particular GL program.
     *
     * @param program  GL program instance.
     */
    void applyInProgram(GLProgram &program) const;

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBGUI_GLUNIFORM_H
