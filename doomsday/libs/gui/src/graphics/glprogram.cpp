/** @file glprogram.cpp  GL shader program.
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

#include "de/GLProgram"
#include "de/GLInfo"
#include "de/GLUniform"
#include "de/GLBuffer"
#include "de/GLShader"
#include "de/GLTexture"
#include "de/GuiApp"
#include <de/Block>
#include <de/Log>

#include <QList>
#include <QSet>
#include <QHash>
#include <QStack>

namespace de {

/// The currently used GLProgram.
static GLProgram const *currentProgram = 0;

using namespace internal;

DENG2_PIMPL(GLProgram)
, DENG2_OBSERVES(GLUniform, ValueChange)
, DENG2_OBSERVES(GLUniform, Deletion)
{
    typedef QSet<GLUniform const *>  Uniforms;
    typedef QList<GLUniform const *> UniformList;
    typedef QSet<GLShader const *>   Shaders;

    /// For each uniform name, there is a stack of bindings. The top binding
    /// in each stack is the active one at any given time.
    QHash<Block, QStack<GLUniform const *>> stacks;

    Uniforms    allBound;
    Uniforms    active; ///< Currently active bindings.
    Uniforms    changed;
    UniformList textures;
    bool        texturesChanged;
    int         attribLocation[AttribSpec::MaxSemantics]; ///< Where each attribute is bound.

    GLuint  name;
    Shaders shaders;
    bool    inUse;
    bool    needRebuild;

    Impl(Public *i)
        : Base(i)
        , texturesChanged(false)
        , name(0)
        , inUse(false)
        , needRebuild(false)
    {}

    ~Impl()
    {
        release();
    }

    void alloc()
    {
        if (!name)
        {
            name = LIBGUI_GL.glCreateProgram();
            if (!name)
            {
                throw AllocError("GLProgram::alloc", "Failed to create program");
            }
        }
    }

    void releaseButRetainBindings()
    {
        self().setState(NotReady);
        detachAllShaders();
        if (name)
        {
            LIBGUI_GL.glDeleteProgram(name);
            name = 0;
        }
    }

    void release()
    {
        unbindAll();
        releaseButRetainBindings();
    }

    void attach(GLShader const *shader)
    {
        DENG2_ASSERT(shader->isReady());
        alloc();
        LIBGUI_GL.glAttachShader(name, shader->glName());
        LIBGUI_ASSERT_GL_OK();
        shaders.insert(holdRef(shader));
    }

    void detach(GLShader const *shader)
    {
        if (shader->isReady())
        {
            LIBGUI_GL.glDetachShader(name, shader->glName());
        }
        shaders.remove(shader);
        shader->release();
    }

    void detachAllShaders()
    {
        foreach (GLShader const *shader, shaders)
        {
            detach(shader);
        }
        shaders.clear();
    }

    void unbindAll()
    {
        for (GLUniform const *u : allBound)
        {
            u->audienceForValueChange() -= this;
            u->audienceForDeletion() -= this;
        }
        allBound.clear();
        stacks.clear();
        active.clear();
        changed.clear();
        textures.clear();
        texturesChanged = false;
    }

    /**
     * Bind all known vertex attributes to the indices used by GLBuffer. The
     * program is automatically (re)linked after binding the vertex attributes,
     * if there are already shaders attached.
     */
    void bindVertexAttribs()
    {
        alloc();

        if (!shaders.isEmpty())
        {
            link();
        }

        auto &GL = LIBGUI_GL;

        // Look up where the attributes ended up being linked.
        for (uint i = 0; i < AttribSpec::MaxSemantics; ++i)
        {
            char const *var = AttribSpec::semanticVariableName(AttribSpec::Semantic(i));
            attribLocation[i] = GL.glGetAttribLocation(name, var);
        }
    }

    Block getInfoLog() const
    {
        dint32 logSize;
        dint32 count;
        LIBGUI_GL.glGetProgramiv(name, GL_INFO_LOG_LENGTH, &logSize);

        Block log{Block::Size(logSize)};
        LIBGUI_GL.glGetProgramInfoLog(name, logSize, &count, reinterpret_cast<GLchar *>(log.data()));
        return log;
    }

    void link()
    {
        DENG2_ASSERT(name != 0);

        LIBGUI_GL.glLinkProgram(name);

        // Was linking successful?
        GLint ok;
        LIBGUI_GL.glGetProgramiv(name, GL_LINK_STATUS, &ok);
        if (!ok)
        {
            throw LinkerError("GLProgram::link", "Linking failed:\n" + getInfoLog());
        }
    }

    void markAllBoundUniformsChanged()
    {
        foreach (GLUniform const *u, active)
        {
            changed.insert(u);
        }
    }

    void updateUniforms()
    {
        if (changed.isEmpty()) return;

        // Apply the uniform values in this program.
        foreach (GLUniform const *u, changed)
        {
            DENG2_ASSERT(active.contains(changed));
            if (!u->isSampler())
            {
                u->applyInProgram(self());
            }
        }

        if (texturesChanged)
        {
            // Update the sampler uniforms.
            for (int unit = 0; unit < textures.size(); ++unit)
            {
                int loc = self().glUniformLocation(textures[unit]->name());
                if (loc >= 0)
                {
                    LIBGUI_GL.glUniform1i(loc, unit);
                    LIBGUI_ASSERT_GL_OK();
                }
            }
            texturesChanged = false;
        }

        changed.clear();
    }

    void bindTextures()
    {
        // Update the sampler uniforms.
        for (int unit = textures.size() - 1; unit >= 0; --unit)
        {
            GLTexture const *tex = *textures[unit];
            if (tex)
            {
                tex->glBindToUnit(unit);
            }
        }
    }

    void rebuild()
    {
        if (name)
        {
            LIBGUI_GL.glDeleteProgram(name);
            name = 0;
        }

        alloc();

        foreach (GLShader const *shader, shaders)
        {
            LIBGUI_GL.glAttachShader(name, shader->glName());
            LIBGUI_ASSERT_GL_OK();
        }

        bindVertexAttribs();
        markAllBoundUniformsChanged();
    }

    void uniformValueChanged(GLUniform &uniform)
    {
        if (active.contains(&uniform))
        {
            changed.insert(&uniform);
        }
    }

    void uniformDeleted(GLUniform &uniform)
    {
        self().unbind(uniform);
    }

    void addBinding(GLUniform const *uniform)
    {
        allBound.insert(uniform);
        uniform->audienceForValueChange() += this;
        uniform->audienceForDeletion() += this;

        auto &stack = stacks[uniform->name()];
        if (!stack.isEmpty())
        {
            // The old binding is no longer active.
            active.remove(stack.top());
            changed.remove(stack.top());
        }
        stack.push(uniform);

        active.insert(uniform);
        changed.insert(uniform);

        if (uniform->isSampler())
        {
            textures << uniform;
            texturesChanged = true;
        }
    }

    void removeBinding(GLUniform const *uniform)
    {
        allBound.remove(uniform);
        uniform->audienceForValueChange() -= this;
        uniform->audienceForDeletion() -= this;

        active.remove(uniform);
        changed.remove(uniform);

        auto &stack = stacks[uniform->name()];
        if (stack.top() == uniform)
        {
            stack.pop();
            if (!stack.isEmpty())
            {
                // The new topmost binding becomes active.
                active.insert(stack.top());
                changed.insert(stack.top());
            }
        }
        else
        {
            // It might be deeper in the stack.
            //stack.removeAll(uniform); // added in Qt 5.4
            int found = stack.indexOf(uniform);
            if (found >= 0)
            {
                stack.remove(found);
            }
        }
        if (stack.isEmpty())
        {
            stacks.remove(uniform->name());
        }

        if (uniform->isSampler())
        {
            textures.removeAll(uniform);
            texturesChanged = true;
        }
    }
};

GLProgram::GLProgram() : d(new Impl(this))
{}

void GLProgram::clear()
{
    d->release();
}

GLProgram &GLProgram::build(GLShader const *vertexShader, GLShader const *fragmentShader)
{
    return build({vertexShader, fragmentShader});
}

GLProgram &GLProgram::build(QVector<GLShader const *> const &shaders)
{
    d->releaseButRetainBindings();
    for (GLShader const *shd : shaders)
    {
        DENG2_ASSERT(shd != nullptr);
        DENG2_ASSERT(shd->isReady());
        d->attach(shd);
    }
    d->bindVertexAttribs();
    d->markAllBoundUniformsChanged();

    setState(Ready);
    return *this;
}

GLProgram &GLProgram::build(IByteArray const &vertexShaderSource,
                            IByteArray const &fragmentShaderSource)
{
    return build(refless(new GLShader(GLShader::Vertex,   vertexShaderSource)),
                 refless(new GLShader(GLShader::Fragment, fragmentShaderSource)));
}

void GLProgram::rebuildBeforeNextUse()
{
    d->needRebuild = true;
}

void GLProgram::rebuild()
{
    d->rebuild();
}

GLProgram &GLProgram::operator << (GLUniform const &uniform)
{
    return bind(uniform);
}

GLProgram &GLProgram::bind(GLUniform const &uniform)
{
    if (!d->allBound.contains(&uniform))
    {
        // If the program is already linked, we can check which uniforms it
        // actually has.
        if (!isReady() || glHasUniform(uniform.name()))
        {
            d->addBinding(&uniform);
        }
    }
    return *this;
}

GLProgram &GLProgram::unbind(GLUniform const &uniform)
{
    if (d->allBound.contains(&uniform))
    {
        d->removeBinding(&uniform);
    }
    return *this;
}

void GLProgram::beginUse() const
{
    LIBGUI_ASSERT_GL_OK();
    DENG2_ASSERT_IN_RENDER_THREAD();
    DENG2_ASSERT(isReady());
    DENG2_ASSERT(!d->inUse);
    LIBGUI_ASSERT_GL_CONTEXT_ACTIVE();

    if (d->needRebuild)
    {
        d->needRebuild = false;
        const_cast<GLProgram *>(this)->rebuild();
    }

    DENG2_ASSERT(LIBGUI_GL.glIsProgram(d->name));

    d->inUse = true;
    currentProgram = this;

    // The program is now ready for use.
    LIBGUI_GL.glUseProgram(d->name);

    LIBGUI_ASSERT_GL_OK();

    d->updateUniforms();
    d->bindTextures();

    LIBGUI_ASSERT_GL_OK();
}

void GLProgram::endUse() const
{
    DENG2_ASSERT_IN_RENDER_THREAD();
    DENG2_ASSERT(d->inUse);

    d->inUse = false;
    currentProgram = 0;
    LIBGUI_GL.glUseProgram(0);
}

GLProgram const *GLProgram::programInUse() // static
{
    return currentProgram;
}

GLuint GLProgram::glName() const
{
    return d->name;
}

int GLProgram::glUniformLocation(char const *uniformName) const
{
    return LIBGUI_GL.glGetUniformLocation(d->name, uniformName);
}

bool GLProgram::glHasUniform(char const *uniformName) const
{
    return glUniformLocation(uniformName) >= 0;
}

int GLProgram::attributeLocation(AttribSpec::Semantic semantic) const
{
    DENG2_ASSERT(semantic >= 0);
    DENG2_ASSERT(semantic < AttribSpec::MaxSemantics);

    return d->attribLocation[semantic];
}

bool GLProgram::validate() const
{
    LIBGUI_GL.glValidateProgram(d->name);

    int valid;
    LIBGUI_GL.glGetProgramiv(d->name, GL_VALIDATE_STATUS, &valid);
    if (valid == GL_FALSE)
    {
        qDebug() << "GLProgram" << d->name << this << "is not validated:";
        qDebug() << d->getInfoLog().constData();
        return false;
    }
    return true;
}

} // namespace de
