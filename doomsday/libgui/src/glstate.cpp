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
        Scissor,
        ScissorX,
        ScissorY,
        ScissorWidth,
        ScissorHeight,
        ViewportX,
        ViewportY,
        ViewportWidth,
        ViewportHeight,
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
            { CullMode,       2  },
            { DepthTest,      1  },
            { DepthFunc,      3  },
            { DepthWrite,     1  },
            { Blend,          1  },
            { BlendFuncSrc,   4  },
            { BlendFuncDest,  4  },
            { BlendOp,        2  },
            { Scissor,        1  },
            { ScissorX,       12 }, // 12 bits == 4096 max
            { ScissorY,       12 },
            { ScissorWidth,   12 },
            { ScissorHeight,  12 },
            { ViewportX,      12 },
            { ViewportY,      12 },
            { ViewportWidth,  12 },
            { ViewportHeight, 12 }
        };
        props.addElements(propSpecs, MAX_PROPERTIES);
    }

    Instance(Public *i, Instance const &other)
        : Base(i),
          props(other.props),
          target(other.target)
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
            //glBlendFunc(glBFunc(self.srcBlendFunc()), glBFunc(self.destBlendFunc()));
            glBlendFuncSeparate(glBFunc(self.srcBlendFunc()), glBFunc(self.destBlendFunc()),
                                GL_ONE, GL_ONE);
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

        case Scissor:
        case ScissorX:
        case ScissorY:
        case ScissorWidth:
        case ScissorHeight:
        {
            if(self.scissor() || self.target().hasActiveRect())
            {
                glEnable(GL_SCISSOR_TEST);

                Rectangleui origScr;
                if(self.scissor())
                {
                    origScr = self.scissorRect();
                }
                else
                {
                    origScr = Rectangleui::fromSize(self.target().size());
                }

                Rectangleui const scr = self.target().scaleToActiveRect(origScr);
                glScissor(scr.left(), self.target().size().y - scr.bottom(),
                          scr.width(), scr.height());
            }
            else
            {
                glDisable(GL_SCISSOR_TEST);
            }
            break;
        }

        case ViewportX:
        case ViewportY:
        case ViewportWidth:
        case ViewportHeight:
        {
            Rectangleui const vp = self.target().scaleToActiveRect(self.viewport());
            //qDebug() << "glViewport" << vp.asText();

            glViewport(vp.left(), self.target().size().y - vp.bottom(),
                       vp.width(), vp.height());
            break;
        }

        default:
            break;
        }
    }

    void removeRedundancies(BitField::Ids &changed)
    {
        if(changed.contains(BlendFuncSrc) && changed.contains(BlendFuncDest))
        {
            changed.remove(BlendFuncDest);
        }

        if(changed.contains(ScissorX) || changed.contains(ScissorY) ||
           changed.contains(ScissorWidth) || changed.contains(ScissorHeight))
        {
            changed.insert(ScissorX);
            changed.remove(ScissorY);
            changed.remove(ScissorWidth);
            changed.remove(ScissorHeight);
        }

        if(changed.contains(ViewportX) || changed.contains(ViewportY) ||
           changed.contains(ViewportWidth) || changed.contains(ViewportHeight))
        {
            changed.insert(ViewportX);
            changed.remove(ViewportY);
            changed.remove(ViewportWidth);
            changed.remove(ViewportHeight);
        }
    }
};

GLState::GLState() : d(new Instance(this))
{
    setCull      (gl::None);
    setDepthTest (false);
    setDepthFunc (gl::Less);
    setDepthWrite(true);
    setBlend     (true);
    setBlendFunc (gl::One, gl::Zero);
    setBlendOp   (gl::Add);

    setDefaultTarget();
}

GLState::GLState(GLState const &other) : d(new Instance(this, *other.d))
{}

GLState &GLState::operator = (GLState const &other)
{
    d.reset(new Instance(this, *other.d));
    return *this;
}

GLState &GLState::setCull(gl::Cull mode)
{
    d->props.set(CullMode, duint(mode));
    return *this;
}

GLState &GLState::setDepthTest(bool enable)
{
    d->props.set(DepthTest, enable);
    return *this;
}

GLState &GLState::setDepthFunc(gl::Comparison func)
{
    d->props.set(DepthFunc, duint(func));
    return *this;
}

GLState &GLState::setDepthWrite(bool enable)
{
    d->props.set(DepthWrite, enable);
    return *this;
}

GLState &GLState::setBlend(bool enable)
{
    d->props.set(Blend, enable);
    return *this;
}

GLState &GLState::setBlendFunc(gl::Blend src, gl::Blend dest)
{
    d->props.set(BlendFuncSrc,  duint(src));
    d->props.set(BlendFuncDest, duint(dest));
    return *this;
}

GLState &GLState::setBlendFunc(gl::BlendFunc func)
{
    d->props.set(BlendFuncSrc,  duint(func.first));
    d->props.set(BlendFuncDest, duint(func.second));
    return *this;
}

GLState &GLState::setBlendOp(gl::BlendOp op)
{
    d->props.set(BlendOp, duint(op));
    return *this;
}

GLState &GLState::setTarget(GLTarget &target)
{
    d->target = &target;
    return *this;
}

GLState &GLState::setDefaultTarget()
{
    d->target = 0;
    return *this;
}

GLState &GLState::setViewport(Rectangleui const &viewportRect)
{
    d->props.set(ViewportX,      viewportRect.left());
    d->props.set(ViewportY,      viewportRect.top());
    d->props.set(ViewportWidth,  viewportRect.width());
    d->props.set(ViewportHeight, viewportRect.height());
    return *this;
}

GLState &GLState::setNormalizedViewport(Rectanglef const &normViewportRect)
{
    GLTarget::Size const size = target().size();
    Rectangleui vp(Vector2ui(normViewportRect.left() * size.x,
                             normViewportRect.top()  * size.y),
                   Vector2ui(std::ceil(normViewportRect.right()  * size.x),
                             std::ceil(normViewportRect.bottom() * size.y)));
    return setViewport(vp);
}

GLState &GLState::setScissor(Rectanglei const &scissorRect)
{
    return setScissor(scissorRect.toRectangleui());
}

GLState &GLState::setScissor(Rectangleui const &newScissorRect)
{
    Rectangleui cumulative;
    if(scissor())
    {
        cumulative = scissorRect() & newScissorRect;
    }
    else
    {
        cumulative = newScissorRect;
    }

    d->props.set(Scissor,       true);
    d->props.set(ScissorX,      cumulative.left());
    d->props.set(ScissorY,      cumulative.top());
    d->props.set(ScissorWidth,  cumulative.width());
    d->props.set(ScissorHeight, cumulative.height());
    return *this;
}

GLState &GLState::setNormalizedScissor(Rectanglef const &normScissorRect)
{
    Rectangleui vp = viewport();
    Rectanglei scis(Vector2i(normScissorRect.left()   * vp.width(),
                             normScissorRect.top()    * vp.height()),
                    Vector2i(std::ceil(normScissorRect.right()  * vp.width()),
                             std::ceil(normScissorRect.bottom() * vp.height())));
    return setScissor(scis);
}

GLState &GLState::clearScissor()
{
    d->props.set(Scissor,       false);
    d->props.set(ScissorX,      0u);
    d->props.set(ScissorY,      0u);
    d->props.set(ScissorWidth,  0u);
    d->props.set(ScissorHeight, 0u);
    return *this;
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
    return CanvasWindow::main().canvas().renderTarget();
}

Rectangleui GLState::viewport() const
{
    return Rectangleui(d->props[ViewportX],
                       d->props[ViewportY],
                       d->props[ViewportWidth],
                       d->props[ViewportHeight]);
}

bool GLState::scissor() const
{
    return d->props.asBool(Scissor);
}

Rectangleui GLState::scissorRect() const
{
    return Rectangleui(d->props[ScissorX],
                       d->props[ScissorY],
                       d->props[ScissorWidth],
                       d->props[ScissorHeight]);
}

void GLState::apply() const
{
    bool forceViewportAndScissor = false;


    // Update the render target.
    if(currentTarget != d->target)
    {
        GLTarget const &oldTarget = (currentTarget? *currentTarget :
                                     CanvasWindow::main().canvas().renderTarget());

        if(currentTarget) currentTarget->glRelease();
        currentTarget = d->target;
        if(currentTarget) currentTarget->glBind();

        if(oldTarget.hasActiveRect() || target().hasActiveRect())
        {
            // We can't trust that the viewport or scissor can remain the same
            // as the active rectangle may have changed.
            forceViewportAndScissor = true;
        }
    }

    // Determine which properties have changed.
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

        if(forceViewportAndScissor)
        {
            changed.insert(ViewportX);
            changed.insert(ScissorX);
        }
    }

    if(!changed.isEmpty())
    {
        d->removeRedundancies(changed);

        // Apply the changed properties.
        foreach(BitField::Id id, changed)
        {
            d->glApply(Property(id));
        }
        currentProps = d->props;
    }
}

void GLState::considerNativeStateUndefined()
{
    currentProps.clear();
    currentTarget = 0;
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

GLState &GLState::pop()
{
    delete take();
    return top();
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

dsize GLState::stackDepth()
{
    return stack.size();
}

} // namespace de
