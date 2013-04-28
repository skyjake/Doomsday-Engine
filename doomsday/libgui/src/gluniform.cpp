/** @file gluniform.cpp  GL uniform.
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

#include "de/GLUniform"
#include "de/GLTexture"
#include "de/GLProgram"
#include "de/gui/opengl.h"
#include <de/Block>
#include <de/Log>
#include <cstring>

namespace de {

DENG2_PIMPL(GLUniform)
{
    Block name;
    Type type;
    union Value {
        dint      int32;
        duint     uint32;
        dfloat    float32;
        Vector4f  *vector;
        Matrix3f  *mat3;
        Matrix4f  *mat4;
        GLTexture *tex;
    } value;

    Instance(Public *i, QLatin1String const &n, Type t)
        : Base(i), name(n.latin1()), type(t)
    {
        name.append('\0');

        // Allocate the value type.
        zap(value);
        switch(type)
        {
        case Vec2:
        case Vec3:
        case Vec4:
            value.vector = new Vector4f;
            break;

        case Mat3:
            value.mat3 = new Matrix3f;
            break;

        case Mat4:
            value.mat4 = new Matrix4f;
            break;

        default:
            break;
        }
    }

    ~Instance()
    {
        DENG2_FOR_PUBLIC_AUDIENCE(Deletion, i) i->uniformDeleted(self);

        switch(type)
        {
        case Vec2:
        case Vec3:
        case Vec4:
            delete value.vector;
            break;

        case Mat3:
            delete value.mat3;
            break;

        case Mat4:
            delete value.mat4;
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

        switch(type)
        {
        case Int:
            if(value.int32 != dint(numValue))
            {
                value.int32 = dint(numValue);
                return true;
            }
            break;

        case UInt:
            if(value.uint32 != duint(numValue))
            {
                value.uint32 = duint(numValue);
                return true;
            }
            break;

        case Float:
            if(!fequal(value.float32, dfloat(numValue)))
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
        DENG2_FOR_PUBLIC_AUDIENCE(ValueChange, i)
        {
            i->uniformValueChanged(self);
        }
    }
};

GLUniform::GLUniform(char const *nameInShader, Type uniformType)
    : d(new Instance(this, QLatin1String(nameInShader), uniformType))
{}

void GLUniform::setName(char const *nameInShader)
{
    d->name = Block(nameInShader);
    d->name.append('\0');
}

QLatin1String GLUniform::name() const
{
    return QLatin1String(d->name);
}

GLUniform::Type GLUniform::type() const
{
    return d->type;
}

GLUniform &GLUniform::operator = (dint value)
{
    if(d->set(value))
    {
        d->markAsChanged();
    }
    return *this;
}

GLUniform &GLUniform::operator = (duint value)
{
    if(d->set(value))
    {
        d->markAsChanged();
    }
    return *this;
}

GLUniform &GLUniform::operator = (dfloat value)
{
    if(d->set(value))
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

    if(Vector2f(*d->value.vector) != vec)
    {
        *d->value.vector = Vector4f(vec);
        d->markAsChanged();
    }
    return *this;
}

GLUniform &GLUniform::operator = (Vector3f const &vec)
{
    DENG2_ASSERT(d->type == Vec3);

    if(Vector3f(*d->value.vector) != vec)
    {
        *d->value.vector = vec;
        d->markAsChanged();
    }
    return *this;
}

GLUniform &GLUniform::operator = (Vector4f const &vec)
{
    DENG2_ASSERT(d->type == Vec4);

    if(*d->value.vector != vec)
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

dint GLUniform::toInt() const
{
    DENG2_ASSERT(d->type == Int || d->type == UInt || d->type == Float);

    switch(d->type)
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

    switch(d->type)
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

    switch(d->type)
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

GLTexture *GLUniform::texture() const
{
    return d->value.tex;
}

void GLUniform::applyInProgram(GLProgram &program) const
{
    int loc = program.glUniformLocation(d->name.constData());
    if(loc < 0)
    {
        // Uniform not in the program.
        LOG_AS("applyInProgram");
        LOG_DEBUG("'%s' not in the program") << d->name.constData();
        return;
    }

    switch(d->type)
    {
    case Int:
        glUniform1i(loc, d->value.int32);
        break;

    case UInt:
        glUniform1i(loc, d->value.uint32);
        break;

    case Float:
        glUniform1f(loc, d->value.float32);
        break;

    case Vec2:
        glUniform2f(loc, d->value.vector->x, d->value.vector->y);
        break;

    case Vec3:
        glUniform3f(loc, d->value.vector->x, d->value.vector->y, d->value.vector->z);
        break;

    case Vec4:
        glUniform4f(loc, d->value.vector->x, d->value.vector->y, d->value.vector->z, d->value.vector->w);
        break;

    case Mat3:
        glUniformMatrix3fv(loc, 1, GL_FALSE, d->value.mat3->values());
        break;

    case Mat4:
        glUniformMatrix4fv(loc, 1, GL_FALSE, d->value.mat4->values());
        break;

    default:
        break;
    }
}

} // namespace de
