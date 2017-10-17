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

#include "de/GLUniform"
#include "de/GLTexture"
#include "de/GLProgram"
#include "de/GLInfo"
#include <de/Block>
#include <de/Log>
#include <cstring>

namespace de {

DENG2_PIMPL(GLUniform)
, DENG2_OBSERVES(Asset, Deletion)
{
    Block name;
    Type type;
    union Value {
        dint     int32;
        duint    uint32;
        dfloat   float32;
        dfloat   *floats;
        Vector3f *vec3array;
        Vector4f *vector;
        Matrix3f *mat3;
        Matrix4f *mat4;
        GLTexture const *tex;
    } value;
    duint16 usedElemCount;
    duint16 elemCount;

    Impl(Public *i, QLatin1String const &n, Type t, duint elems)
        : Base(i)
        , name(n.latin1())
        , type(t)
        , usedElemCount(duint16(elems))
        , elemCount(duint16(elems))
    {
        name.append('\0');

        DENG2_ASSERT(elemCount == 1 || (elemCount > 1 && (type == FloatArray ||
                                                          type == Mat4Array  ||
                                                          type == Vec4Array  ||
                                                          type == Vec3Array )));
        // Allocate the value type.
        zap(value);
        switch (type)
        {
        case Vec2:
        case Vec3:
        case Vec4:
            value.vector = new Vector4f;
            break;

        case FloatArray:
            value.floats = new float[elemCount];
            break;

        case Vec3Array:
            value.vec3array = new Vector3f[elemCount];
            break;

        case Vec4Array:
            value.vector = new Vector4f[elemCount];
            break;

        case Mat3:
            value.mat3 = new Matrix3f;
            break;

        case Mat4:
            value.mat4 = new Matrix4f;
            break;

        case Mat4Array:
            value.mat4 = new Matrix4f[elemCount];
            break;

        default:
            break;
        }
    }

    ~Impl()
    {
        DENG2_FOR_PUBLIC_AUDIENCE2(Deletion, i) i->uniformDeleted(self());

        switch (type)
        {
        case Vec2:
        case Vec3:
        case Vec4:
            delete value.vector;
            break;

        case FloatArray:
            delete [] value.floats;
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
        DENG2_ASSERT(type == Int || type == UInt || type == Float);

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
        DENG2_FOR_PUBLIC_AUDIENCE2(ValueChange, i)
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
                value.tex = 0;
            }
        }
    }

    DENG2_PIMPL_AUDIENCE(Deletion)
    DENG2_PIMPL_AUDIENCE(ValueChange)
};

DENG2_AUDIENCE_METHOD(GLUniform, Deletion)
DENG2_AUDIENCE_METHOD(GLUniform, ValueChange)

GLUniform::GLUniform(char const *nameInShader, Type uniformType, duint elements)
    : d(new Impl(this, QLatin1String(nameInShader), uniformType, elements))
{}

void GLUniform::setName(char const *nameInShader)
{
    d->name = Block(nameInShader);
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
    return d->type == Sampler2D || d->type == SamplerCube;
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

GLUniform &GLUniform::operator = (Vector2f const &vec)
{
    DENG2_ASSERT(d->type == Vec2);

    if (Vector2f(*d->value.vector) != vec)
    {
        *d->value.vector = Vector4f(vec);
        d->markAsChanged();
    }
    return *this;
}

GLUniform &GLUniform::operator = (Vector3f const &vec)
{
    DENG2_ASSERT(d->type == Vec3);

    if (Vector3f(*d->value.vector) != vec)
    {
        *d->value.vector = vec;
        d->markAsChanged();
    }
    return *this;
}

GLUniform &GLUniform::operator = (Vector4f const &vec)
{
    DENG2_ASSERT(d->type == Vec4);

    if (*d->value.vector != vec)
    {
        *d->value.vector = vec;
        d->markAsChanged();
    }
    return *this;
}

GLUniform &GLUniform::operator = (Matrix3f const &mat)
{
    DENG2_ASSERT(d->type == Mat3);

    *d->value.mat3 = mat;
    d->markAsChanged();

    return *this;
}

GLUniform &GLUniform::operator = (Matrix4f const &mat)
{
    DENG2_ASSERT(d->type == Mat4);

    *d->value.mat4 = mat;
    d->markAsChanged();

    return *this;
}

GLUniform &GLUniform::operator = (GLTexture const &texture)
{
    return *this = &texture;
}

GLUniform &GLUniform::operator = (GLTexture const *texture)
{
    if (texture && texture->isReady())
    {
        DENG2_ASSERT(d->type != Sampler2D   || !texture->isCubeMap());
        DENG2_ASSERT(d->type != SamplerCube ||  texture->isCubeMap());
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
    DENG2_ASSERT(d->type == FloatArray);
    DENG2_ASSERT(elementIndex < d->elemCount);

    if (!fequal(d->value.floats[elementIndex], value))
    {
        d->value.floats[elementIndex] = value;
        d->markAsChanged();
    }
    return *this;
}

GLUniform &GLUniform::set(duint elementIndex, Vector3f const &vec)
{
    DENG2_ASSERT(d->type == Vec3Array);
    DENG2_ASSERT(elementIndex < d->elemCount);

    if (d->value.vec3array[elementIndex] != vec)
    {
        d->value.vec3array[elementIndex] = vec;
        d->usedElemCount = d->elemCount;
        d->markAsChanged();
    }
    return *this;
}

GLUniform &GLUniform::set(duint elementIndex, Vector4f const &vec)
{
    DENG2_ASSERT(d->type == Vec4Array);
    DENG2_ASSERT(elementIndex < d->elemCount);

    if (d->value.vector[elementIndex] != vec)
    {
        d->value.vector[elementIndex] = vec;
        d->usedElemCount = d->elemCount;
        d->markAsChanged();
    }
    return *this;
}

GLUniform &GLUniform::set(duint elementIndex, Matrix4f const &mat)
{
    DENG2_ASSERT(d->type == Mat4Array);
    DENG2_ASSERT(elementIndex < d->elemCount);

    d->value.mat4[elementIndex] = mat;
    d->usedElemCount = d->elemCount;
    d->markAsChanged();

    return *this;
}

GLUniform &GLUniform::set(float const *floatArray, dsize count)
{
    DENG2_ASSERT(d->type == FloatArray);
    DENG2_ASSERT(count <= d->elemCount);

    memcpy(d->value.floats, floatArray, sizeof(float) * count);
    d->usedElemCount = duint16(count);
    d->markAsChanged();
    return *this;
}

GLUniform &GLUniform::set(Vector4f const *vectorArray, dsize count)
{
    DENG2_ASSERT(d->type == Vec4Array);
    DENG2_ASSERT(count <= d->elemCount);

    memcpy(d->value.vector, vectorArray, sizeof(Vector4f) * count);
    d->usedElemCount = duint16(count);
    d->markAsChanged();
    return *this;
}

GLUniform &GLUniform::setUsedElementCount(duint elementCount)
{
    DENG2_ASSERT(elementCount <= d->elemCount);
    d->usedElemCount = elementCount;
    d->markAsChanged();
    return *this;
}

dint GLUniform::toInt() const
{
    DENG2_ASSERT(d->type == Int || d->type == UInt || d->type == Float);

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

duint de::GLUniform::toUInt() const
{
    DENG2_ASSERT(d->type == Int || d->type == UInt || d->type == Float);

    switch (d->type)
    {
    case Int:
        return duint(d->value.int32);

    case UInt:
        return d->value.uint32;

    case Float:
        return duint(d->value.float32);

    default:
        return 0;
    }
}

dfloat GLUniform::toFloat() const
{
    DENG2_ASSERT(d->type == Int || d->type == UInt || d->type == Float);

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

Vector2f const &GLUniform::toVector2f() const
{
    DENG2_ASSERT(d->type == Vec2 || d->type == Vec3 || d->type == Vec4);
    return *d->value.vector;
}

Vector3f const &GLUniform::toVector3f() const
{
    DENG2_ASSERT(d->type == Vec2 || d->type == Vec3 || d->type == Vec4);
    return *d->value.vector;
}

Vector4f const &GLUniform::toVector4f() const
{
    DENG2_ASSERT(d->type == Vec2 || d->type == Vec3 || d->type == Vec4);
    return *d->value.vector;
}

Matrix3f const &GLUniform::toMatrix3f() const
{
    DENG2_ASSERT(d->type == Mat3);
    return *d->value.mat3;
}

Matrix4f const &GLUniform::toMatrix4f() const
{
    DENG2_ASSERT(d->type == Mat4);
    return *d->value.mat4;
}

GLTexture const *GLUniform::texture() const
{
    DENG2_ASSERT(isSampler());
    return d->value.tex;
}

void GLUniform::applyInProgram(GLProgram &program) const
{
    LIBGUI_ASSERT_GL_OK();

    int loc = program.glUniformLocation(d->name.constData());
    if (loc < 0)
    {
        // Uniform not in the program.
        LOG_AS("applyInProgram");
        LOGDEV_GL_WARNING("'%s' not in the program") << d->name.constData();
        return;
    }

    switch (d->type)
    {
    case Int:
        LIBGUI_GL.glUniform1i(loc, d->value.int32);
        break;

    case UInt:
        LIBGUI_GL.glUniform1i(loc, d->value.uint32);
        break;

    case Float:
        LIBGUI_GL.glUniform1f(loc, d->value.float32);
        break;

    case FloatArray:
        LIBGUI_GL.glUniform1fv(loc, d->usedElemCount, d->value.floats);
        break;

    case Vec2:
        LIBGUI_GL.glUniform2f(loc, d->value.vector->x, d->value.vector->y);
        break;

    case Vec3:
        LIBGUI_GL.glUniform3f(loc, d->value.vector->x, d->value.vector->y, d->value.vector->z);
        break;

    case Vec3Array:
        LIBGUI_GL.glUniform3fv(loc, d->usedElemCount, &d->value.vec3array->x); // sequentially laid out
        break;

    case Vec4:
    case Vec4Array:
        LIBGUI_GL.glUniform4fv(loc, d->usedElemCount, &d->value.vector->x); // sequentially laid out
        break;

    case Mat3:
        LIBGUI_GL.glUniformMatrix3fv(loc, 1, GL_FALSE, d->value.mat3->values());
        break;

    case Mat4:
    case Mat4Array:
        LIBGUI_GL.glUniformMatrix4fv(loc, d->usedElemCount, GL_FALSE, d->value.mat4->values()); // sequentially laid out
        break;

    default:
        break;
    }

#if defined (DENG2_DEBUG)
    {
        GLenum err = LIBGUI_GL.glGetError();
        if (err != GL_NO_ERROR)
        {
            qDebug() << "[GLUniform] Failure with uniform:" << d->name << "loc:" << loc;
        }
    }
#endif
}

} // namespace de
