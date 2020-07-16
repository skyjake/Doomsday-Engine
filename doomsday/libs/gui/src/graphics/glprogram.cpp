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

#include "de/glprogram.h"
#include "de/glinfo.h"
#include "de/gluniform.h"
#include "de/glbuffer.h"
#include "de/glshader.h"
#include "de/gltexture.h"
#include "de/guiapp.h"
#include <de/block.h>
#include <de/log.h>

namespace de {

/// The currently used GLProgram.
static const GLProgram *currentProgram = nullptr;

using namespace internal;

DE_PIMPL(GLProgram)
, DE_OBSERVES(GLUniform, ValueChange)
, DE_OBSERVES(GLUniform, Deletion)
{
    typedef Set <const GLUniform *> Uniforms;
    typedef List<const GLUniform *> UniformList;
    typedef Set <const GLShader *>  Shaders;

    /// For each uniform name, there is a stack of bindings. The top binding
    /// in each stack is the active one at any given time.
    Hash<Block, List<const GLUniform *>> stacks;

    Uniforms    allBound;
    Uniforms    active; ///< Currently active bindings.
    Uniforms    changed;
    UniformList samplers;
    bool        samplersChanged;
    int         attribLocation[AttribSpec::MaxSemantics]; ///< Where each attribute is bound.

    GLuint  name;
    Shaders shaders;
    bool    inUse;
    bool    needRebuild;

    Impl(Public *i)
        : Base(i)
        , samplersChanged(false)
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
            name = glCreateProgram();
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
            glDeleteProgram(name);
            name = 0;
        }
    }

    void release()
    {
        unbindAll();
        releaseButRetainBindings();
    }

    void attach(const GLShader *shader)
    {
        DE_ASSERT(shader->isReady());
        alloc();
        glAttachShader(name, shader->glName());
        LIBGUI_ASSERT_GL_OK();
        shaders.insert(holdRef(shader));
    }

    void detach(const GLShader *shader)
    {
        if (shader->isReady())
        {
            glDetachShader(name, shader->glName());
        }
        shaders.remove(shader);
        shader->release();
    }

    void detachAllShaders()
    {
        while (!shaders.empty())
        {
            detach(*shaders.begin()); // gets removed from `shaders`
        }
    }

    void unbindAll()
    {
        for (const GLUniform *u : allBound)
        {
            u->audienceForValueChange() -= this;
            u->audienceForDeletion() -= this;
        }
        allBound.clear();
        stacks.clear();
        active.clear();
        changed.clear();
        samplers.clear();
        samplersChanged = false;
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

        // Look up where the attributes ended up being linked.
        for (uint i = 0; i < AttribSpec::MaxSemantics; ++i)
        {
            const char *var = AttribSpec::semanticVariableName(AttribSpec::Semantic(i));
            attribLocation[i] = glGetAttribLocation(name, var);
        }
    }

    Block getInfoLog() const
    {
        dint32 logSize;
        dint32 count;
        glGetProgramiv(name, GL_INFO_LOG_LENGTH, &logSize);

        Block log{Block::Size(logSize)};
        glGetProgramInfoLog(name, logSize, &count, reinterpret_cast<GLchar *>(log.data()));
        return log;
    }

    void link()
    {
        DE_ASSERT(name != 0);

        glLinkProgram(name);

        // Was linking successful?
        GLint ok;
        glGetProgramiv(name, GL_LINK_STATUS, &ok);
        if (!ok)
        {
            throw LinkerError("GLProgram::link", "Linking failed:\n" + getInfoLog());
        }
    }

    void markAllBoundUniformsChanged()
    {
        for (const GLUniform *u : active)
        {
            changed.insert(u);
        }
        samplersChanged = true;
    }

    void updateUniforms()
    {
        if (changed.isEmpty()) return;

        // Apply the uniform values in this program.
        for (const GLUniform *u : changed)
        {
            DE_ASSERT(active.contains(u));
            if (!u->isSampler())
            {
                u->applyInProgram(self());
            }
        }

        if (samplersChanged)
        {
            // Update the sampler uniforms with the assigned texture image unit values.
            for (duint unit = 0; unit < samplers.size(); ++unit)
            {
                int loc = self().glUniformLocation(samplers[unit]->name().c_str());
                if (loc >= 0)
                {
                    glUniform1i(loc, unit);
//                    qDebug("P%i Sampler %s loc:%i unit:%i",
//                           name,
//                           samplers[unit]->name().constData(),
//                           loc,
//                           unit);
                    LIBGUI_ASSERT_GL_OK();
                }
            }
            samplersChanged = false;
        }

        changed.clear();
    }

    void bindSamplers()
    {
        // Bind textures to the assigned texture image units.
        for (int unit = samplers.sizei() - 1; unit >= 0; --unit)
        {
            samplers[unit]->bindSamplerTexture(unit);
        }
    }

    void rebuild()
    {
        if (name)
        {
            glDeleteProgram(name);
            name = 0;
        }

        alloc();

        for (const GLShader *shader : shaders)
        {
            glAttachShader(name, shader->glName());
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
            if (uniform.isSampler())
            {
                samplersChanged = true;
            }
        }
    }

    void uniformDeleted(GLUniform &uniform)
    {
        self().unbind(uniform);
    }

    void addBinding(const GLUniform *uniform)
    {
        allBound.insert(uniform);
        uniform->audienceForValueChange() += this;
        uniform->audienceForDeletion() += this;

        auto &stack = stacks[uniform->name()];
        if (!stack.isEmpty())
        {
            // The old binding is no longer active.
            active.remove(stack.back());
            changed.remove(stack.back());
        }
        stack.push_back(uniform);

        active.insert(uniform);
        changed.insert(uniform);

        if (uniform->isSampler())
        {
            samplers << uniform;
            samplersChanged = true;
        }
    }

    void removeBinding(const GLUniform *uniform)
    {
        allBound.remove(uniform);
        uniform->audienceForValueChange() -= this;
        uniform->audienceForDeletion() -= this;

        active.remove(uniform);
        changed.remove(uniform);

        auto &stack = stacks[uniform->name()];
        if (stack.back() == uniform)
        {
            stack.back();
            if (!stack.isEmpty())
            {
                // The new topmost binding becomes active.
                active.insert(stack.back());
                changed.insert(stack.back());
            }
        }
        else
        {
            // It might be deeper in the stack.
            int found = stack.indexOf(uniform);
            if (found >= 0)
            {
                stack.removeAt(found);
            }
        }
        if (stack.isEmpty())
        {
            stacks.remove(uniform->name());
        }

        if (uniform->isSampler())
        {
            samplers.removeAll(uniform);
            samplersChanged = true;
        }
    }
};

GLProgram::GLProgram() : d(new Impl(this))
{}

void GLProgram::clear()
{
    d->release();
}

GLProgram &GLProgram::build(const GLShader *vertexShader, const GLShader *fragmentShader)
{
    return build({vertexShader, fragmentShader});
}

GLProgram &GLProgram::build(const List<const GLShader *> &shaders)
{
    d->releaseButRetainBindings();
    for (const GLShader *shd : shaders)
    {
        DE_ASSERT(shd != nullptr);
        DE_ASSERT(shd->isReady());
        d->attach(shd);
    }
    d->bindVertexAttribs();
    d->markAllBoundUniformsChanged();

    setState(Ready);
    return *this;
}

GLProgram &GLProgram::build(const IByteArray &vertexShaderSource,
                            const IByteArray &fragmentShaderSource)
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

GLProgram &GLProgram::operator << (const GLUniform &uniform)
{
    return bind(uniform);
}

GLProgram &GLProgram::bind(const GLUniform &uniform)
{
    if (!d->allBound.contains(&uniform))
    {
        // If the program is already linked, we can check which uniforms it
        // actually has.
        if (!isReady() || glHasUniform(uniform.name().c_str()))
        {
            d->addBinding(&uniform);
        }
    }
    return *this;
}

GLProgram &GLProgram::unbind(const GLUniform &uniform)
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
    DE_ASSERT_IN_RENDER_THREAD();
    DE_ASSERT(isReady());
    DE_ASSERT(!d->inUse);
    LIBGUI_ASSERT_GL_CONTEXT_ACTIVE();

    if (d->needRebuild)
    {
        d->needRebuild = false;
        const_cast<GLProgram *>(this)->rebuild();
    }

    DE_ASSERT(glIsProgram(d->name));

    d->inUse = true;
    currentProgram = this;

    // The program is now ready for use.
    glUseProgram(d->name);

    LIBGUI_ASSERT_GL_OK();

    d->updateUniforms();
    d->bindSamplers();

    LIBGUI_ASSERT_GL_OK();
}

void GLProgram::endUse() const
{
    DE_ASSERT_IN_RENDER_THREAD();
    DE_ASSERT(d->inUse);

    d->inUse = false;
    currentProgram = nullptr;
    glUseProgram(0);
}

const GLProgram *GLProgram::programInUse() // static
{
    return currentProgram;
}

GLuint GLProgram::glName() const
{
    return d->name;
}

int GLProgram::glUniformLocation(const char *uniformName) const
{
    return glGetUniformLocation(d->name, uniformName);
}

bool GLProgram::glHasUniform(const char *uniformName) const
{
    return glUniformLocation(uniformName) >= 0;
}

int GLProgram::attributeLocation(AttribSpec::Semantic semantic) const
{
    DE_ASSERT(semantic >= 0);
    DE_ASSERT(semantic < AttribSpec::MaxSemantics);

    return d->attribLocation[semantic];
}

bool GLProgram::validate() const
{
    glValidateProgram(d->name);

    int valid;
    glGetProgramiv(d->name, GL_VALIDATE_STATUS, &valid);
    if (GLboolean(valid) == GL_FALSE)
    {
        debug("GLProgram %u %p is not validated:\n%s", d->name, this, d->getInfoLog().c_str());
        return false;
    }
    return true;
}

} // namespace de
