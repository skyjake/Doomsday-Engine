/** @file glstate.cpp  GL state.
 *
 * @todo This implementation assumes OpenGL drawing occurs only in one thread.
 * If multithreaded rendering is done at some point in the future, the GL state
 * stack must be part of the thread-local data.
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

#include "de/GLState"
#include "de/PersistentCanvasWindow"
#include "de/gui/opengl.h"
#include <de/BitField>

namespace de {

namespace internal
{
    enum Property {
        CullMode,
        DepthTest,
        DepthFunc,
        DepthWrite,
        Blend,
        BlendFuncSrc,
        BlendFuncDest,
        BlendOp,
        MAX_PROPERTIES
    };

    /// The GL state stack.
    struct GLStateStack : public QList<GLState *> {
        GLStateStack() {
            // Initialize with a default state.
            append(new GLState);
        }
        ~GLStateStack() { qDeleteAll(*this); }
    };
    static GLStateStack stack;

    /// Currently applied GL state properties.
    static BitField currentProps;
    static GLTarget *currentTarget;
}

using namespace internal;

DENG2_PIMPL(GLState)
{
    BitField props;
    GLTarget *target;

    Instance(Public *i) : Base(i), target(0)
    {
        static BitField::Spec const propSpecs[MAX_PROPERTIES] = {
            { CullMode,      2 },
            { DepthTest,     1 },
            { DepthFunc,     3 },
            { DepthWrite,    1 },
            { Blend,         1 },
            { BlendFuncSrc,  4 },
            { BlendFuncDest, 4 },
            { BlendOp,       2 }
        };
        props.addElements(propSpecs, MAX_PROPERTIES);
    }

    Instance(Public *i, Instance const &other)
        : Base(i), props(other.props), target(other.target)
    {}

    static GLenum glComp(gl::Comparison comp)
    {
        switch(comp)
        {
        case gl::Never:          return GL_NEVER;
        case gl::Always:         return GL_ALWAYS;
        case gl::Equal:          return GL_EQUAL;
        case gl::NotEqual:       return GL_NOTEQUAL;
        case gl::Less:           return GL_LESS;
        case gl::Greater:        return GL_GREATER;
        case gl::LessOrEqual:    return GL_LEQUAL;
        case gl::GreaterOrEqual: return GL_GEQUAL;
        }
        return GL_NEVER;
    }

    static GLenum glBFunc(gl::Blend f)
    {
        switch(f)
        {
        case gl::Zero:              return GL_ZERO;
        case gl::One:               return GL_ONE;
        case gl::SrcColor:          return GL_SRC_COLOR;
        case gl::OneMinusSrcColor:  return GL_ONE_MINUS_SRC_COLOR;
        case gl::SrcAlpha:          return GL_SRC_ALPHA;
        case gl::OneMinusSrcAlpha:  return GL_ONE_MINUS_SRC_ALPHA;
        case gl::DestColor:         return GL_DST_COLOR;
        case gl::OneMinusDestColor: return GL_ONE_MINUS_DST_COLOR;
        case gl::DestAlpha:         return GL_DST_ALPHA;
        case gl::OneMinusDestAlpha: return GL_ONE_MINUS_DST_ALPHA;
        }
        return GL_ZERO;
    }

    void glApply(Property prop)
    {
        switch(prop)
        {
        case CullMode:
            switch(self.cull())
            {
            case gl::None:
                glDisable(GL_CULL_FACE);
                break;
            case gl::Front:
                glEnable(GL_CULL_FACE);
                glCullFace(GL_FRONT);
                break;
            case gl::Back:
                glEnable(GL_CULL_FACE);
                glCullFace(GL_BACK);
                break;
            }
            break;

        case DepthTest:
            if(self.depthTest())
                glEnable(GL_DEPTH_TEST);
            else
                glDisable(GL_DEPTH_TEST);
            break;

        case DepthFunc:
            glDepthFunc(glComp(self.depthFunc()));
            break;

        case DepthWrite:
            if(self.depthWrite())
                glDepthMask(GL_TRUE);
            else
                glDepthMask(GL_FALSE);
            break;

        case Blend:
            if(self.blend())
                glEnable(GL_BLEND);
            else
                glDisable(GL_BLEND);
            break;

        case BlendFuncSrc:
        case BlendFuncDest:
            glBlendFunc(glBFunc(self.srcBlendFunc()), glBFunc(self.destBlendFunc()));
            break;

        case BlendOp:
            switch(self.blendOp())
            {
            case gl::Add:
                glBlendEquation(GL_FUNC_ADD);
                break;
            case gl::Subtract:
                glBlendEquation(GL_FUNC_SUBTRACT);
                break;
            case gl::ReverseSubtract:
                glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
                break;
            }
            break;

        default:
            break;
        }
    }
};

GLState::GLState() : d(new Instance(this))
{
    setCull      (gl::None);
    setDepthTest (false);
    setDepthFunc (gl::Less);
    setDepthWrite(true);
    setBlend     (false);
    setBlendFunc (gl::One, gl::Zero);
    setBlendOp   (gl::Add);

    setDefaultTarget();
}

GLState::GLState(GLState const &other) : d(new Instance(this, *other.d))
{}

void GLState::setCull(gl::Cull mode)
{
    d->props.set(CullMode, duint(mode));
}

void GLState::setDepthTest(bool enable)
{
    d->props.set(DepthTest, enable);
}

void GLState::setDepthFunc(gl::Comparison func)
{
    d->props.set(DepthFunc, duint(func));
}

void GLState::setDepthWrite(bool enable)
{
    d->props.set(DepthWrite, enable);
}

void GLState::setBlend(bool enable)
{
    d->props.set(Blend, enable);
}

void GLState::setBlendFunc(gl::Blend src, gl::Blend dest)
{
    d->props.set(BlendFuncSrc,  duint(src));
    d->props.set(BlendFuncDest, duint(dest));
}

void GLState::setBlendFunc(gl::BlendFunc func)
{
    d->props.set(BlendFuncSrc,  duint(func.first));
    d->props.set(BlendFuncDest, duint(func.second));
}

void GLState::setBlendOp(gl::BlendOp op)
{
    d->props.set(BlendOp, duint(op));
}

void GLState::setTarget(GLTarget &target)
{
    d->target = &target;
}

void GLState::setDefaultTarget()
{
    d->target = 0;
}

gl::Cull GLState::cull() const
{
    return d->props.valueAs<gl::Cull>(CullMode);
}

bool GLState::depthTest() const
{
    return d->props.asBool(DepthTest);
}

gl::Comparison GLState::depthFunc() const
{
    return d->props.valueAs<gl::Comparison>(DepthFunc);
}

bool GLState::depthWrite() const
{
    return d->props.asBool(DepthWrite);
}

bool GLState::blend() const
{
    return d->props.asBool(Blend);
}

gl::Blend GLState::srcBlendFunc() const
{
    return d->props.valueAs<gl::Blend>(BlendFuncSrc);
}

gl::Blend GLState::destBlendFunc() const
{
    return d->props.valueAs<gl::Blend>(BlendFuncDest);
}

gl::BlendFunc GLState::blendFunc() const
{
    return gl::BlendFunc(srcBlendFunc(), destBlendFunc());
}

gl::BlendOp GLState::blendOp() const
{
    return d->props.valueAs<gl::BlendOp>(BlendOp);
}

GLTarget &GLState::target() const
{
    if(d->target)
    {
        return *d->target;
    }
    return PersistentCanvasWindow::main().canvas().renderTarget();
}

void GLState::apply() const
{
    BitField::Ids changed;

    if(!currentProps.size())
    {
        // Apply everything.
        changed = d->props.elementIds();
    }
    else
    {
        // Just apply the changed parts of the state.
        changed = d->props.delta(currentProps);
    }

    if(!changed.isEmpty())
    {
        currentProps = d->props;

        // The blend func only needs to be set once.
        if(changed.contains(BlendFuncSrc) && changed.contains(BlendFuncDest))
        {
            changed.remove(BlendFuncDest);
        }

        // Apply the changed properties.
        foreach(BitField::Id id, changed)
        {
            d->glApply(Property(id));
        }
    }

    // Update the render target.
    if(currentTarget != d->target)
    {
        if(currentTarget) currentTarget->glRelease();
        currentTarget = d->target;
        if(currentTarget) currentTarget->glBind();
    }
}

GLState &GLState::top()
{
    DENG2_ASSERT(!stack.isEmpty());
    return *stack.last();
}

GLState &GLState::push()
{
    // Duplicate the topmost state.
    push(new GLState(top()));
    return top();
}

void GLState::pop()
{
    delete take();
}

void GLState::push(GLState *state)
{
    stack.append(state);
}

GLState *GLState::take()
{
    DENG2_ASSERT(stack.size() > 1);
    return stack.takeLast();
}

} // namespace de
