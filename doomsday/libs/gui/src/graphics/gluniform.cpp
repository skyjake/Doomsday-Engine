/** @file gluniform.cpp  GL uniform.
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

#include "de/gluniform.h"
#include "de/gltexture.h"
#include "de/glprogram.h"
#include "de/glinfo.h"
#include <de/block.h>
#include <de/log.h>
#include <cstring>

namespace de {

DE_PIMPL(GLUniform)
, DE_OBSERVES(Asset, Deletion)
{
    Block name;
    Type  type;
    union Value{
        dint             int32;
        duint            uint32;
        dfloat           float32;
        dint *           ints;
        dfloat *         floats;
        Vec2f *          vec2array;
        Vec3f *          vec3array;
        Vec4f *          vector;
        Mat3f *          mat3;
        Mat4f *          mat4;
        const GLTexture *tex;
    } value;
    duint16 usedElemCount;
    duint16 elemCount;

    Impl(Public *i, const Block &n, Type t, duint elems)
        : Base(i)
        , name(n)
        , type(t)
        , usedElemCount(duint16(elems))
        , elemCount(duint16(elems))
    {
        name.append('\0');

        DE_ASSERT(elemCount == 1 || (elemCount > 1 && (type == IntArray   ||
                                                       type == FloatArray ||
                                                       type == Mat4Array  ||
                                                       type == Vec4Array  ||
                                                       type == Vec3Array  ||
                                                       type == Vec2Array)));
        // Allocate the value type.
        zap(value);
        switch (type)
        {
        case Vec2:
        case Vec3:
        case Vec4:
            value.vector = new Vec4f;
            break;

        case IntArray:
            value.ints = new int[elemCount];
            break;

        case FloatArray:
            value.floats = new float[elemCount];
            break;

        case Vec2Array:
            value.vec2array = new Vec2f[elemCount];
            break;

        case Vec3Array:
            value.vec3array = new Vec3f[elemCount];
            break;

        case Vec4Array:
            value.vector = new Vec4f[elemCount];
            break;

        case Mat3:
            value.mat3 = new Mat3f;
            break;

        case Mat4:
            value.mat4 = new Mat4f;
            break;

        case Mat4Array:
            value.mat4 = new Mat4f[elemCount];
            break;

        default:
            break;
        }
    }

    ~Impl()
    {
        DE_NOTIFY_PUBLIC(Deletion, i) i->uniformDeleted(self());

        switch (type)
        {
        case Vec2:
        case Vec3:
        case Vec4:
            delete value.vector;
            break;

        case IntArray:
            delete [] value.ints;
            break;

        case FloatArray:
            delete [] value.floats;
            break;

        case Vec2Array:
            delete [] value.vec2array;
            break;

        case Vec3Array:
            delete [] value.vec3array;
            break;

        case Vec4Array:
            delete [] value.vector;
            break;

        case Mat3:
            delete value.mat3;
            break;

        case Mat4:
            delete value.mat4;
            break;

        case Mat4Array:
            delete [] value.mat4;
            break;

        case Sampler2D:
        case SamplerCube:
        case SamplerBuffer:
            //if (value.tex) value.tex->audienceForDeletion() -= this;
            break;

        default:
            break;
        }
    }

    /**
     * @return Returns @c true if the value changed.
     */
    template <typename Type>
    bool set(Type numValue)
    {
        DE_ASSERT(type == Int || type == UInt || type == Float || type == SamplerBuffer);

        switch (type)
        {
        case Int:
            if (value.int32 != dint(numValue))
            {
                value.int32 = dint(numValue);
                return true;
            }
            break;

        case UInt:
        case SamplerBuffer:
            if (value.uint32 != duint(numValue))
            {
                value.uint32 = duint(numValue);
                return true;
            }
            break;

        case Float:
            if (!fequal(value.float32, dfloat(numValue)))
            {
                value.float32 = dfloat(numValue);
                return true;
            }
            break;

        default:
            break;
        }

        return false;
    }

    void markAsChanged()
    {
        DE_NOTIFY_PUBLIC(ValueChange, i)
        {
            i->uniformValueChanged(self());
        }
    }

    void assetBeingDeleted(Asset &asset)
    {
        if (self().isSampler())
        {
            if (value.tex == &asset)
            {
                // No more texture to refer to.
                value.tex = nullptr;
            }
        }
    }

    DE_PIMPL_AUDIENCES(Deletion, ValueChange)
};

DE_AUDIENCE_METHODS(GLUniform, Deletion, ValueChange)

GLUniform::GLUniform(const Block &nameInShader, Type uniformType, duint elements)
    : d(new Impl(this, nameInShader, uniformType, elements))
{}

void GLUniform::setName(const Block &nameInShader)
{
    d->name = nameInShader;
    d->name.append('\0');
}

Block GLUniform::name() const
{
    return d->name;
}

GLUniform::Type GLUniform::type() const
{
    return d->type;
}

bool GLUniform::isSampler() const
{
    return d->type == Sampler2D || d->type == SamplerCube || d->type == SamplerBuffer;
}

void GLUniform::bindSamplerTexture(duint unit) const
{
    if (d->type == SamplerBuffer)
    {
        // Buffer textures are not represented by GLTexture.
        glActiveTexture(GLenum(GL_TEXTURE0 + unit));
        glBindTexture(GL_TEXTURE_BUFFER, d->value.uint32);
    }
    else
    {
        if (const GLTexture *tex = texture())
        {
            tex->glBindToUnit(unit);
        }
    }
}

GLUniform &GLUniform::operator = (dint value)
{
    if (d->set(value))
    {
        d->markAsChanged();
    }
    return *this;
}

GLUniform &GLUniform::operator = (duint value)
{
    if (d->set(value))
    {
        d->markAsChanged();
    }
    return *this;
}

GLUniform &GLUniform::operator = (dfloat value)
{
    if (d->set(value))
    {
        d->markAsChanged();
    }
    return *this;
}

GLUniform &GLUniform::operator = (ddouble value)
{
    return *this = dfloat(value);
}

GLUniform &GLUniform::operator = (const Vec2f &vec)
{
    DE_ASSERT(d->type == Vec2);

    if (Vec2f(*d->value.vector) != vec)
    {
        *d->value.vector = Vec4f(vec);
        d->markAsChanged();
    }
    return *this;
}

GLUniform &GLUniform::operator = (const Vec3f &vec)
{
    DE_ASSERT(d->type == Vec3);

    if (Vec3f(*d->value.vector) != vec)
    {
        *d->value.vector = vec;
        d->markAsChanged();
    }
    return *this;
}

GLUniform &GLUniform::operator = (const Vec4f &vec)
{
    DE_ASSERT(d->type == Vec4);

    if (*d->value.vector != vec)
    {
        *d->value.vector = vec;
        d->markAsChanged();
    }
    return *this;
}

GLUniform &GLUniform::operator = (const Mat3f &mat)
{
    DE_ASSERT(d->type == Mat3);

    *d->value.mat3 = mat;
    d->markAsChanged();

    return *this;
}

GLUniform &GLUniform::operator = (const Mat4f &mat)
{
    DE_ASSERT(d->type == Mat4);

    *d->value.mat4 = mat;
    d->markAsChanged();

    return *this;
}

GLUniform &GLUniform::operator = (const GLTexture &texture)
{
    return *this = &texture;
}

GLUniform &GLUniform::operator = (const GLTexture *texture)
{
    if (texture && texture->isReady())
    {
        DE_ASSERT(d->type != Sampler2D   || !texture->isCubeMap());
        DE_ASSERT(d->type != SamplerCube ||  texture->isCubeMap());
    }

    if (d->value.tex != texture)
    {
        // We will observe the texture this uniform refers to.
        if (d->value.tex) d->value.tex->audienceForDeletion() -= d;

        d->value.tex = texture;
        d->markAsChanged();

        if (d->value.tex) d->value.tex->audienceForDeletion() += d;
    }
    return *this;
}

GLUniform &GLUniform::set(duint elementIndex, dfloat value)
{
    DE_ASSERT(d->type == FloatArray);
    DE_ASSERT(elementIndex < d->elemCount);

    if (!fequal(d->value.floats[elementIndex], value))
    {
        d->value.floats[elementIndex] = value;
        d->markAsChanged();
    }
    return *this;
}

GLUniform &GLUniform::set(duint elementIndex, const Vec3f &vec)
{
    DE_ASSERT(d->type == Vec3Array);
    DE_ASSERT(elementIndex < d->elemCount);

    if (d->value.vec3array[elementIndex] != vec)
    {
        d->value.vec3array[elementIndex] = vec;
        d->usedElemCount = d->elemCount;
        d->markAsChanged();
    }
    return *this;
}

GLUniform &GLUniform::set(duint elementIndex, const Vec4f &vec)
{
    DE_ASSERT(d->type == Vec4Array);
    DE_ASSERT(elementIndex < d->elemCount);

    if (d->value.vector[elementIndex] != vec)
    {
        d->value.vector[elementIndex] = vec;
        d->usedElemCount = d->elemCount;
        d->markAsChanged();
    }
    return *this;
}

GLUniform &GLUniform::set(duint elementIndex, const Mat4f &mat)
{
    DE_ASSERT(d->type == Mat4Array);
    DE_ASSERT(elementIndex < d->elemCount);

    d->value.mat4[elementIndex] = mat;
    d->usedElemCount = d->elemCount;
    d->markAsChanged();

    return *this;
}

GLUniform &GLUniform::set(const int *intArray, dsize count)
{
    DE_ASSERT(d->type == IntArray);
    DE_ASSERT(count <= d->elemCount);

    memcpy(d->value.ints, intArray, sizeof(int) * count);
    d->usedElemCount = duint16(count);
    d->markAsChanged();
    return *this;
}

GLUniform &GLUniform::set(const float *floatArray, dsize count)
{
    DE_ASSERT(d->type == FloatArray);
    DE_ASSERT(count <= d->elemCount);

    memcpy(d->value.floats, floatArray, sizeof(float) * count);
    d->usedElemCount = duint16(count);
    d->markAsChanged();
    return *this;
}

GLUniform &GLUniform::set(const Vec3f *vectorArray, dsize count)
{
    DE_ASSERT(d->type == Vec3Array);
    DE_ASSERT(count <= d->elemCount);

    memcpy(d->value.vector, vectorArray, sizeof(Vec3f) * count);
    d->usedElemCount = duint16(count);
    d->markAsChanged();
    return *this;
}

GLUniform &GLUniform::set(const Vec4f *vectorArray, dsize count)
{
    DE_ASSERT(d->type == Vec4Array);
    DE_ASSERT(count <= d->elemCount);

    memcpy(d->value.vector, vectorArray, sizeof(Vec4f) * count);
    d->usedElemCount = duint16(count);
    d->markAsChanged();
    return *this;
}

GLUniform &GLUniform::set(const Mat4f *mat4Array, dsize count)
{
    DE_ASSERT(d->type == Mat4Array);
    DE_ASSERT(count <= d->elemCount);

    memcpy(d->value.mat4, mat4Array, sizeof(Mat4f) * count);
    d->usedElemCount = duint16(count);
    d->markAsChanged();
    return *this;
}

GLUniform &GLUniform::setUsedElementCount(duint elementCount)
{
    DE_ASSERT(elementCount <= d->elemCount);
    d->usedElemCount = duint16(elementCount);
    d->markAsChanged();
    return *this;
}

dint GLUniform::toInt() const
{
    DE_ASSERT(d->type == Int || d->type == UInt || d->type == Float);

    switch (d->type)
    {
    case Int:
        return d->value.int32;

    case UInt:
        return dint(d->value.uint32);

    case Float:
        return dint(d->value.float32);

    default:
        return 0;
    }
}

duint GLUniform::toUInt() const
{
    DE_ASSERT(d->type == Int || d->type == UInt || d->type == Float || d->type == SamplerBuffer);

    switch (d->type)
    {
    case Int:
        return duint(d->value.int32);

    case UInt:
    case SamplerBuffer:
        return d->value.uint32;

    case Float:
        return duint(d->value.float32);

    default:
        return 0;
    }
}

dfloat GLUniform::toFloat() const
{
    DE_ASSERT(d->type == Int || d->type == UInt || d->type == Float);

    switch (d->type)
    {
    case Int:
        return dfloat(d->value.int32);

    case UInt:
        return dfloat(d->value.uint32);

    case Float:
        return d->value.float32;

    default:
        return 0;
    }
}

const Vec2f &GLUniform::toVec2f() const
{
    DE_ASSERT(d->type == Vec2 || d->type == Vec3 || d->type == Vec4);
    return *d->value.vector;
}

const Vec3f &GLUniform::toVec3f() const
{
    DE_ASSERT(d->type == Vec2 || d->type == Vec3 || d->type == Vec4);
    return *d->value.vector;
}

const Vec4f &GLUniform::toVec4f() const
{
    DE_ASSERT(d->type == Vec2 || d->type == Vec3 || d->type == Vec4);
    return *d->value.vector;
}

const Mat3f &GLUniform::toMat3f() const
{
    DE_ASSERT(d->type == Mat3);
    return *d->value.mat3;
}

const Mat4f &GLUniform::toMat4f() const
{
    DE_ASSERT(d->type == Mat4);
    return *d->value.mat4;
}

const GLTexture *GLUniform::texture() const
{
    DE_ASSERT(isSampler());
    DE_ASSERT(d->type != SamplerBuffer); // GLTexture not used
    return d->value.tex;
}

void GLUniform::applyInProgram(GLProgram &program) const
{
    LIBGUI_ASSERT_GL_OK();

    int loc = program.glUniformLocation(d->name.c_str());
    if (loc < 0)
    {
        // Uniform not in the program.
        LOG_AS("applyInProgram");
        LOGDEV_GL_WARNING("'%s' not in the program") << d->name.constData();
        DE_ASSERT_FAIL("[GLUniform] Attempted to apply a uniform that is not in the shader program");
        return;
    }

    switch (d->type)
    {
    case Int:
        glUniform1i(loc, d->value.int32);
        break;

    case IntArray:
        glUniform1iv(loc, d->usedElemCount, d->value.ints);
        break;

    case UInt:
        glUniform1ui(loc, d->value.uint32);
        break;

    case Float:
        glUniform1f(loc, d->value.float32);
        break;

    case FloatArray:
        glUniform1fv(loc, d->usedElemCount, d->value.floats);
        break;

    case Vec2:
        glUniform2f(loc, d->value.vector->x, d->value.vector->y);
        break;

    case Vec2Array:
        glUniform2fv(loc, d->usedElemCount, &d->value.vec2array->x);
        break;

    case Vec3:
        glUniform3f(loc, d->value.vector->x, d->value.vector->y, d->value.vector->z);
        break;

    case Vec3Array:
        glUniform3fv(loc, d->usedElemCount, &d->value.vec3array->x); // sequentially laid out
        break;

    case Vec4:
    case Vec4Array:
        glUniform4fv(loc, d->usedElemCount, &d->value.vector->x); // sequentially laid out
        break;

    case Mat3:
        glUniformMatrix3fv(loc, 1, GL_FALSE, d->value.mat3->values());
        break;

    case Mat4:
    case Mat4Array:
        glUniformMatrix4fv(loc, d->usedElemCount, GL_FALSE, d->value.mat4->values()); // sequentially laid out
        break;

    case Sampler2D:
    case SamplerCube:
    case SamplerBuffer:
        // Not set here. GLProgram sets the sampler values according to where textures are bound.
        break;
    }

#if defined (DE_DEBUG)
    {
        GLenum err = glGetError();
        if (err != GL_NO_ERROR)
        {
            debug("[GLUniform] Failure with uniform: %s loc: %i", d->name.c_str(), loc);
        }
    }
#endif
}

} // namespace de
