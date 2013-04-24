/** @file glprogram.cpp  GL shader program.
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

#include "de/GLProgram"
#include "de/GLUniform"
#include "de/GLBuffer"
#include "de/GLShader"
#include "de/GLTexture"
#include "de/gui/opengl.h"
#include <de/Block>

#include <QSet>
#include <QList>

namespace de {

using namespace internal;

DENG2_PIMPL(GLProgram),
DENG2_OBSERVES(GLUniform, ValueChange),
DENG2_OBSERVES(GLUniform, Deletion)
{
    typedef QSet<GLUniform const *> Uniforms;
    typedef QList<GLUniform const *> UniformList;
    typedef QSet<GLShader const *> Shaders;

    Uniforms bound;
    Uniforms changed;
    UniformList textures;
    bool texturesChanged;

    GLuint name;
    Shaders shaders;
    bool inUse;

    Instance(Public *i)
        : Base(i),
          texturesChanged(false),
          name(0),
          inUse(false)
    {}

    ~Instance()
    {
        release();
    }

    void alloc()
    {
        if(!name)
        {
            name = glCreateProgram();
            if(!name)
            {
                throw AllocError("GLProgram::alloc", "Failed to create program");
            }
        }
    }

    void release()
    {
        self.setState(NotReady);
        detachAllShaders();
        unbindAll();
        if(name)
        {
            glDeleteProgram(name);
            name = 0;
        }
    }

    void attach(GLShader const &shader)
    {
        DENG2_ASSERT(shader.isReady());
        glAttachShader(name, shader.glName());
        shaders.insert(holdRef(&shader));
    }

    void detach(GLShader const &shader)
    {
        if(shader.isReady())
        {
            glDetachShader(name, shader.glName());
        }
        shaders.remove(&shader);
        shader.release();
    }

    void detachAllShaders()
    {
        foreach(GLShader const *shader, shaders)
        {
            detach(*shader);
        }
        shaders.clear();
    }

    void unbindAll()
    {
        foreach(GLUniform const *u, bound)
        {
            u->audienceForValueChange -= this;
            u->audienceForDeletion -= this;
        }
        texturesChanged = false;
        bound.clear();
        textures.clear();
        changed.clear();
    }

    /**
     * Bind all known vertex attributes to the indices used by GLBuffer. The
     * program is automatically (re)linked after binding the vertex attributes,
     * if there are already shaders attached.
     */
    void bindVertexAttribs()
    {
        alloc();

        // The names of shader attributes are defined here:
        static struct {
            AttribSpec::Semantic semantic;
            char const *varName;
        }
        const names[] = {
            { AttribSpec::Position,  "aVertex"    },
            { AttribSpec::TexCoord0, "aUV"        },
            { AttribSpec::TexCoord1, "aUV2"       },
            { AttribSpec::TexCoord2, "aUV3"       },
            { AttribSpec::TexCoord3, "aUV4"       },
            { AttribSpec::Color,     "aColor"     },
            { AttribSpec::Normal,    "aNormal"    },
            { AttribSpec::Tangent,   "aTangent"   },
            { AttribSpec::Bitangent, "aBitangent" }
        };

        for(int i = 0; sizeof(names)/sizeof(names[0]); ++i)
        {
            glBindAttribLocation(name, GLuint(names[i].semantic), names[i].varName);
        }

        if(!shaders.isEmpty())
        {
            link();
        }
    }

    void link()
    {
        DENG2_ASSERT(name != 0);

        glLinkProgram(name);

        // Was linking successful?
        GLint ok;
        glGetProgramiv(name, GL_LINK_STATUS, &ok);
        if(!ok)
        {
            dint32 logSize;
            dint32 count;
            glGetProgramiv(name, GL_INFO_LOG_LENGTH, &logSize);

            Block log(logSize);
            glGetProgramInfoLog(name, logSize, &count, reinterpret_cast<GLchar *>(log.data()));

            throw LinkerError("GLProgram::link", "Linking failed:\n" + log);
        }
    }

    void updateUniforms()
    {
        if(changed.isEmpty()) return;

        // Apply the uniform values in this program.
        foreach(GLUniform const *u, changed)
        {
            if(u->type() != GLUniform::Texture2D)
            {
                u->applyInProgram(self);
            }
        }

        if(texturesChanged)
        {
            // Update the sampler uniforms.
            for(int unit = 0; unit < textures.size(); ++unit)
            {
                int loc = self.glUniformLocation(textures[unit]->name().latin1());
                if(loc >= 0)
                {
                    glUniform1i(loc, unit);
                }
            }
            texturesChanged = false;
        }

        changed.clear();
    }

    void bindTextures()
    {
        // Update the sampler uniforms.
        for(int unit = textures.size() - 1; unit >= 0; --unit)
        {
            GLTexture const *tex = *textures[unit];
            if(tex)
            {
                tex->glBindToUnit(unit);
            }
        }
    }

    void uniformValueChanged(GLUniform &uniform)
    {
        changed.insert(&uniform);
    }

    void uniformDeleted(GLUniform &uniform)
    {
        self.unbind(uniform);
    }
};

GLProgram::GLProgram() : d(new Instance(this))
{}

void GLProgram::clear()
{
    d->release();
}

GLProgram &GLProgram::build(GLShader const &vertexShader, GLShader const &fragmentShader)
{
    DENG2_ASSERT(vertexShader.isReady());
    DENG2_ASSERT(vertexShader.type() == GLShader::Vertex);
    DENG2_ASSERT(fragmentShader.isReady());
    DENG2_ASSERT(fragmentShader.type() == GLShader::Fragment);

    d->detachAllShaders();
    d->attach(vertexShader);
    d->attach(fragmentShader);
    d->bindVertexAttribs();

    setState(Ready);

    return *this;
}

GLProgram &GLProgram::operator << (GLUniform const &uniform)
{
    return bind(uniform);
}

GLProgram &GLProgram::bind(GLUniform const &uniform)
{
    if(!d->bound.contains(&uniform))
    {
        d->bound.insert(&uniform);
        d->changed.insert(&uniform);

        uniform.audienceForValueChange += d.get();
        uniform.audienceForDeletion += d.get();

        if(uniform.type() == GLUniform::Texture2D)
        {
            d->textures << &uniform;
            d->texturesChanged = true;
        }
    }
    return *this;
}

GLProgram &GLProgram::unbind(GLUniform const &uniform)
{
    if(d->bound.contains(&uniform))
    {
        d->bound.remove(&uniform);
        d->changed.remove(&uniform);

        uniform.audienceForValueChange -= d.get();
        uniform.audienceForDeletion -= d.get();

        if(uniform.type() == GLUniform::Texture2D)
        {
            d->textures.removeOne(&uniform);
            d->texturesChanged = true;
        }
    }
    return *this;
}

void GLProgram::beginUse() const
{
    DENG2_ASSERT(isReady());
    DENG2_ASSERT(!d->inUse);

    d->inUse = true;

    // The program is now ready for use.
    glUseProgram(d->name);

    d->updateUniforms();
    d->bindTextures();
}

void GLProgram::endUse() const
{
    DENG2_ASSERT(d->inUse);

    d->inUse = false;
    glUseProgram(0);
}

GLuint GLProgram::glName() const
{
    return d->name;
}

int GLProgram::glUniformLocation(char const *uniformName) const
{
    GLint loc = glGetUniformLocation(d->name, uniformName);
    // Could check loc here for validity.
    return loc;
}

} // namespace de
