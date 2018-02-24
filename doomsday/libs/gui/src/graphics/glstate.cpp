/** @file glstate.cpp  GL state.
 *
 * @todo This implementation assumes OpenGL drawing occurs only in one thread.
 * If multithreaded rendering is done at some point in the future, the GL state
 * stack must be part of the thread-local data.
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

#include "de/GLState"
#include "de/GLFramebuffer"
#include "de/GLWindow"
#include "de/graphics/opengl.h"
#include <de/GLInfo>
#include <de/BitField>
#include <de/Log>

namespace de {

#ifdef DENG2_DEBUG
extern int GLDrawQueue_queuedElems;
#endif

namespace internal
{
    enum Property {
        CullMode,
        DepthTest,
        DepthFunc,
        DepthWrite,
        AlphaTest,
        AlphaLimit,
        Blend,
        BlendFuncSrc,
        BlendFuncDest,
        BlendOp,
        ColorMask,
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

    static BitField::Spec const propSpecs[MAX_PROPERTIES] = {
        { CullMode,       2  },
        { DepthTest,      1  },
        { DepthFunc,      3  },
        { DepthWrite,     1  },
        { AlphaTest,      1  },
        { AlphaLimit,     8  },
        { Blend,          1  },
        { BlendFuncSrc,   4  },
        { BlendFuncDest,  4  },
        { BlendOp,        2  },
        { ColorMask,      4  },
        { Scissor,        1  },
        { ScissorX,       13 }, // 13 bits == 8192 max
        { ScissorY,       13 },
        { ScissorWidth,   13 },
        { ScissorHeight,  13 },
        { ViewportX,      13 },
        { ViewportY,      13 },
        { ViewportWidth,  13 },
        { ViewportHeight, 13 }
    };
    static BitField::Elements const glStateProperties(propSpecs, MAX_PROPERTIES);

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

    /// Observes the current target and clears the pointer if it happens to get
    /// deleted.
    class CurrentTarget : DENG2_OBSERVES(Asset, Deletion) {
        GLFramebuffer *_target;
        void assetBeingDeleted(Asset &asset) {
            if (&asset == _target) {
                LOG_AS("GLState");
                LOGDEV_GL_NOTE("Current target destroyed, clearing pointer");
                _target = 0;
            }
        }
    public:
        CurrentTarget() : _target(0) {}
        ~CurrentTarget() {
            set(0);
        }
        void set(GLFramebuffer *trg) {
            if (_target) {
                _target->audienceForDeletion() -= this;
            }
            _target = trg;
            if (_target) {
                _target->audienceForDeletion() += this;
            }
        }
        CurrentTarget &operator = (GLFramebuffer *trg) {
            set(trg);
            return *this;
        }
        bool operator != (GLFramebuffer *trg) const {
            return _target != trg;
        }
        GLFramebuffer *get() const {
            return _target;
        }
        operator GLFramebuffer *() const {
            return _target;
        }
    };
    static CurrentTarget currentTarget;
}

DENG2_PIMPL(GLState)
{
    BitField       props;
    GLFramebuffer *target;

    Impl(Public *i)
        : Base(i)
        , props(internal::glStateProperties)
        , target(0)
    {}

    Impl(Public *i, Impl const &other)
        : Base(i)
        , props(other.props)
        , target(other.target)
    {}

    static GLenum glComp(gl::Comparison comp)
    {
        switch (comp)
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
        switch (f)
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

    static gl::Blend fromGlBFunc(GLenum e)
    {
        switch (e)
        {
        case GL_ZERO: return gl::Zero;
        case GL_ONE: return gl::One;
        case GL_SRC_COLOR: return gl::SrcColor;
        case GL_ONE_MINUS_SRC_COLOR: return gl::OneMinusSrcColor;
        case GL_SRC_ALPHA: return gl::SrcAlpha;
        case GL_ONE_MINUS_SRC_ALPHA: return gl::OneMinusSrcAlpha;
        case GL_DST_COLOR: return gl::DestColor;
        case GL_ONE_MINUS_DST_COLOR: return gl::OneMinusDestColor;
        case GL_DST_ALPHA: return gl::DestAlpha;
        case GL_ONE_MINUS_DST_ALPHA: return gl::OneMinusDestAlpha;
        }
        return gl::Zero;
    }

    void glApply(internal::Property prop)
    {
        switch (prop)
        {
        case internal::CullMode:
            switch (self().cull())
            {
            case gl::None:
                LIBGUI_GL.glDisable(GL_CULL_FACE);
                break;
            case gl::Front:
                LIBGUI_GL.glEnable(GL_CULL_FACE);
                LIBGUI_GL.glCullFace(GL_FRONT);
                break;
            case gl::Back:
                LIBGUI_GL.glEnable(GL_CULL_FACE);
                LIBGUI_GL.glCullFace(GL_BACK);
                break;
            }
            break;

        case internal::DepthTest:
            if (self().depthTest())
                LIBGUI_GL.glEnable(GL_DEPTH_TEST);
            else
                LIBGUI_GL.glDisable(GL_DEPTH_TEST);
            break;

        case internal::DepthFunc:
            LIBGUI_GL.glDepthFunc(glComp(self().depthFunc()));
            break;

        case internal::DepthWrite:
            if (self().depthWrite())
                LIBGUI_GL.glDepthMask(GL_TRUE);
            else
                LIBGUI_GL.glDepthMask(GL_FALSE);
            break;

        case internal::AlphaTest:
            /*if (self().alphaTest())
                LIBGUI_GL.glEnable(GL_ALPHA_TEST);
            else
                LIBGUI_GL.glDisable(GL_ALPHA_TEST);*/

            // TODO: use a shared GLUniform available to all shaders that need it

            //qDebug() << "[GLState] Alpha test:" << (self().alphaTest()? "enabled" : "disabled");
            break;

        case internal::AlphaLimit:
            //LIBGUI_GL.glAlphaFunc(GL_GREATER, self().alphaLimit());

            // TODO: use a shared GLUniform available to all shaders that need it

            break;

        case internal::Blend:
            if (self().blend())
                LIBGUI_GL.glEnable(GL_BLEND);
            else
                LIBGUI_GL.glDisable(GL_BLEND);
            break;

        case internal::BlendFuncSrc:
        case internal::BlendFuncDest:
            //glBlendFunc(glBFunc(self().srcBlendFunc()), glBFunc(self().destBlendFunc()));
            LIBGUI_GL.glBlendFuncSeparate(glBFunc(self().srcBlendFunc()), glBFunc(self().destBlendFunc()),
                                          GL_ONE, GL_ONE);
            break;

        case internal::BlendOp:
            switch (self().blendOp())
            {
            case gl::Add:
                LIBGUI_GL.glBlendEquation(GL_FUNC_ADD);
                break;
            case gl::Subtract:
                LIBGUI_GL.glBlendEquation(GL_FUNC_SUBTRACT);
                break;
            case gl::ReverseSubtract:
                LIBGUI_GL.glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
                break;
            }
            break;

        case internal::ColorMask:
        {
            gl::ColorMask const mask = self().colorMask();
            LIBGUI_GL.glColorMask((mask & gl::WriteRed)   != 0,
                                  (mask & gl::WriteGreen) != 0,
                                  (mask & gl::WriteBlue)  != 0,
                                  (mask & gl::WriteAlpha) != 0);
            break;
        }

        case internal::Scissor:
        case internal::ScissorX:
        case internal::ScissorY:
        case internal::ScissorWidth:
        case internal::ScissorHeight:
        {
            if (self().scissor() || self().target().hasActiveRect())
            {
                LIBGUI_GL.glEnable(GL_SCISSOR_TEST);

                Rectangleui origScr;
                if (self().scissor())
                {
                    origScr = self().scissorRect();
                }
                else
                {
                    origScr = Rectangleui::fromSize(self().target().size());
                }

                Rectangleui const scr = self().target().scaleToActiveRect(origScr);
                LIBGUI_GL.glScissor(scr.left(), self().target().size().y - scr.bottom(),
                                    scr.width(), scr.height());
            }
            else
            {
                LIBGUI_GL.glDisable(GL_SCISSOR_TEST);
            }
            break;
        }

        case internal::ViewportX:
        case internal::ViewportY:
        case internal::ViewportWidth:
        case internal::ViewportHeight:
        {
            Rectangleui const vp = self().target().scaleToActiveRect(self().viewport());
            //qDebug() << "glViewport" << vp.asText();

            LIBGUI_GL.glViewport(vp.left(), self().target().size().y - vp.bottom(),
                                 vp.width(), vp.height());
            break;
        }

        default:
            break;
        }

        LIBGUI_ASSERT_GL_OK();
    }

    void removeRedundancies(BitField::Ids &changed)
    {
        if (changed.contains(internal::BlendFuncSrc) && changed.contains(internal::BlendFuncDest))
        {
            changed.remove(internal::BlendFuncDest);
        }

        if (changed.contains(internal::ScissorX) || changed.contains(internal::ScissorY) ||
            changed.contains(internal::ScissorWidth) || changed.contains(internal::ScissorHeight))
        {
            changed.insert(internal::ScissorX);
            changed.remove(internal::ScissorY);
            changed.remove(internal::ScissorWidth);
            changed.remove(internal::ScissorHeight);
        }

        if (changed.contains(internal::ViewportX) || changed.contains(internal::ViewportY) ||
            changed.contains(internal::ViewportWidth) || changed.contains(internal::ViewportHeight))
        {
            changed.insert(internal::ViewportX);
            changed.remove(internal::ViewportY);
            changed.remove(internal::ViewportWidth);
            changed.remove(internal::ViewportHeight);
        }
    }
};

GLState::GLState() : d(new Impl(this))
{
    setCull      (gl::None);
    setDepthTest (false);
    setDepthFunc (gl::Less);
    setDepthWrite(true);
    setAlphaTest (true);
    setAlphaLimit(0);
    setBlend     (true);
    setBlendFunc (gl::One, gl::Zero);
    setBlendOp   (gl::Add);
    setColorMask (gl::WriteAll);

    setDefaultTarget();
}

GLState::GLState(GLState const &other) : d(new Impl(this, *other.d))
{}

GLState &GLState::operator=(GLState const &other)
{
    d.reset(new Impl(this, *other.d));
    return *this;
}

bool GLState::operator==(const GLState &other)
{
    return d->target == other.d->target && d->props == other.d->props;
}

GLState &GLState::setCull(gl::Cull mode)
{
    d->props.set(internal::CullMode, duint(mode));
    return *this;
}

GLState &GLState::setDepthTest(bool enable)
{
    d->props.set(internal::DepthTest, enable);
    return *this;
}

GLState &GLState::setDepthFunc(gl::Comparison func)
{
    d->props.set(internal::DepthFunc, duint(func));
    return *this;
}

GLState &GLState::setDepthWrite(bool enable)
{
    d->props.set(internal::DepthWrite, enable);
    return *this;
}

GLState &GLState::setAlphaTest(bool enable)
{
    d->props.set(internal::AlphaTest, enable);
    return *this;
}

GLState &GLState::setAlphaLimit(float greaterThanValue)
{
    d->props.set(internal::AlphaLimit, unsigned(clamp(0.f, greaterThanValue, 1.f) * 255));
    return *this;
}

GLState &GLState::setBlend(bool enable)
{
    d->props.set(internal::Blend, enable);
    return *this;
}

GLState &GLState::setBlendFunc(gl::Blend src, gl::Blend dest)
{
    d->props.set(internal::BlendFuncSrc,  duint(src));
    d->props.set(internal::BlendFuncDest, duint(dest));
    return *this;
}

GLState &GLState::setBlendFunc(gl::BlendFunc func)
{
    d->props.set(internal::BlendFuncSrc,  duint(func.first));
    d->props.set(internal::BlendFuncDest, duint(func.second));
    return *this;
}

GLState &GLState::setBlendOp(gl::BlendOp op)
{
    d->props.set(internal::BlendOp, duint(op));
    return *this;
}

GLState &GLState::setColorMask(gl::ColorMask mask)
{
    d->props.set(internal::ColorMask, duint(mask));
    return *this;
}

GLState &GLState::setTarget(GLFramebuffer &target)
{
    d->target = &target;
    return *this;
}

GLState &GLState::setDefaultTarget()
{
    d->target = 0;
    return *this;
}

GLState &GLState::setViewport(Rectanglei const &viewportRect)
{
    return setViewport(viewportRect.toRectangleui());
}

GLState &GLState::setViewport(Rectangleui const &viewportRect)
{
    d->props.set(internal::ViewportX,      viewportRect.left());
    d->props.set(internal::ViewportY,      viewportRect.top());
    d->props.set(internal::ViewportWidth,  viewportRect.width());
    d->props.set(internal::ViewportHeight, viewportRect.height());
    return *this;
}

GLState &GLState::setNormalizedViewport(Rectanglef const &normViewportRect)
{
    GLFramebuffer::Size const size = target().size();
    Rectangleui vp(Vec2ui(normViewportRect.left() * size.x,
                             normViewportRect.top()  * size.y),
                   Vec2ui(std::ceil(normViewportRect.right()  * size.x),
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
    if (scissor())
    {
        cumulative = scissorRect() & newScissorRect;
    }
    else
    {
        cumulative = newScissorRect;
    }

    d->props.set(internal::Scissor,       true);
    d->props.set(internal::ScissorX,      cumulative.left());
    d->props.set(internal::ScissorY,      cumulative.top());
    d->props.set(internal::ScissorWidth,  cumulative.width());
    d->props.set(internal::ScissorHeight, cumulative.height());
    return *this;
}

GLState &GLState::setNormalizedScissor(Rectanglef const &normScissorRect)
{
    Rectangleui vp = viewport();
    Rectanglei scis(Vec2i(normScissorRect.left()   * vp.width(),
                             normScissorRect.top()    * vp.height()),
                    Vec2i(std::ceil(normScissorRect.right()  * vp.width()),
                             std::ceil(normScissorRect.bottom() * vp.height())));
    return setScissor(scis.moved(vp.topLeft.toVec2i()));
}

GLState &GLState::clearScissor()
{
    d->props.set(internal::Scissor,       false);
    d->props.set(internal::ScissorX,      0u);
    d->props.set(internal::ScissorY,      0u);
    d->props.set(internal::ScissorWidth,  0u);
    d->props.set(internal::ScissorHeight, 0u);
    return *this;
}

gl::Cull GLState::cull() const
{
    return d->props.valueAs<gl::Cull>(internal::CullMode);
}

bool GLState::depthTest() const
{
    return d->props.asBool(internal::DepthTest);
}

gl::Comparison GLState::depthFunc() const
{
    return d->props.valueAs<gl::Comparison>(internal::DepthFunc);
}

bool GLState::depthWrite() const
{
    return d->props.asBool(internal::DepthWrite);
}

bool GLState::alphaTest() const
{
    return d->props.asBool(internal::AlphaTest);
}

float GLState::alphaLimit() const
{
    return float(d->props.asUInt(internal::AlphaLimit)) / 255.f;
}

bool GLState::blend() const
{
    return d->props.asBool(internal::Blend);
}

gl::Blend GLState::srcBlendFunc() const
{
    return d->props.valueAs<gl::Blend>(internal::BlendFuncSrc);
}

gl::Blend GLState::destBlendFunc() const
{
    return d->props.valueAs<gl::Blend>(internal::BlendFuncDest);
}

gl::BlendFunc GLState::blendFunc() const
{
    return gl::BlendFunc(srcBlendFunc(), destBlendFunc());
}

gl::BlendOp GLState::blendOp() const
{
    return d->props.valueAs<gl::BlendOp>(internal::BlendOp);
}

gl::ColorMask GLState::colorMask() const
{
    return d->props.valueAs<gl::ColorMask>(internal::ColorMask);
}

GLFramebuffer &GLState::target() const
{
    if (d->target)
    {
        return *d->target;
    }
    return GLWindow::main().framebuffer();
}

Rectangleui GLState::viewport() const
{
    return Rectangleui(d->props[internal::ViewportX],
                       d->props[internal::ViewportY],
                       d->props[internal::ViewportWidth],
                       d->props[internal::ViewportHeight]);
}

Rectanglef GLState::normalizedViewport() const
{
    GLFramebuffer::Size const size = target().size();
    Rectangleui const vp = viewport();
    return Rectanglef(float(vp.left())   / float(size.x),
                      float(vp.top())    / float(size.y),
                      float(vp.width())  / float(size.x),
                      float(vp.height()) / float(size.y));
}

bool GLState::scissor() const
{
    return d->props.asBool(internal::Scissor);
}

Rectangleui GLState::scissorRect() const
{
    return Rectangleui(d->props[internal::ScissorX],
                       d->props[internal::ScissorY],
                       d->props[internal::ScissorWidth],
                       d->props[internal::ScissorHeight]);
}

void GLState::apply() const
{
    LIBGUI_ASSERT_GL_OK();

#ifdef DENG2_DEBUG
    DENG2_ASSERT(GLDrawQueue_queuedElems == 0);
#endif

    // Actual OpenGL state shouldn't be changed outside the render thread.
    // The main thread can still manipulate shared OpenGL objects, though.
    DENG2_ASSERT_IN_RENDER_THREAD();

    bool forceViewportAndScissor = false;

    // Update the render target.
    GLFramebuffer *newTarget = &target();
    DENG2_ASSERT(newTarget != 0);

    if (internal::currentTarget != newTarget)
    {
        GLFramebuffer const *oldTarget = internal::currentTarget;
        if (oldTarget)
        {
            oldTarget->glRelease();
        }

        internal::currentTarget = newTarget;
        newTarget->glBind();

        if ((oldTarget && oldTarget->hasActiveRect()) || newTarget->hasActiveRect())
        {
            // We can't trust that the viewport or scissor can remain the same
            // as the active rectangle may have changed.
            forceViewportAndScissor = true;
        }
    }

    LIBGUI_ASSERT_GL_OK();

    // Determine which properties have changed.
    BitField::Ids changed;
    if (internal::currentProps.isEmpty())
    {
        // Apply everything.
        changed = d->props.elements().ids();
    }
    else
    {
        // Just apply the changed parts of the state.
        changed = d->props.delta(internal::currentProps);

        if (forceViewportAndScissor)
        {
            changed.insert(internal::ViewportX);
            changed.insert(internal::ScissorX);
        }
    }

    if (!changed.isEmpty())
    {
        d->removeRedundancies(changed);

        // Apply the changed properties.
        foreach (BitField::Id id, changed)
        {
            d->glApply(internal::Property(id));
        }
        internal::currentProps = d->props;
    }

#if 0
    // Verify that the state is correct.
    for (int i = 0; i < d->props.elements().size(); ++i)
    {
        auto const &elem = d->props.elements().at(i);
        int val;
        switch (elem.id)
        {
        case internal::Blend:
            LIBGUI_GL.glGetIntegerv(GL_BLEND, &val);
            DENG2_ASSERT(!val == !d->props.asBool(elem.id));
            break;

        case internal::BlendFuncSrc:
            LIBGUI_GL.glGetIntegerv(GL_BLEND_SRC_RGB, &val);
            DENG2_ASSERT(d->fromGlBFunc(val) == d->props.asUInt(elem.id));
            break;

        case internal::BlendFuncDest:
            LIBGUI_GL.glGetIntegerv(GL_BLEND_DST_RGB, &val);
            DENG2_ASSERT(d->fromGlBFunc(val) == d->props.asUInt(elem.id));
            break;

        case internal::BlendOp:
            LIBGUI_GL.glGetIntegerv(GL_BLEND_EQUATION_RGB, &val);
            val = (val == GL_FUNC_ADD? gl::Add :
                   val == GL_FUNC_SUBTRACT? gl::Subtract :
                   val == GL_FUNC_REVERSE_SUBTRACT? gl::ReverseSubtract : 0);
            DENG2_ASSERT(val == d->props.asUInt(elem.id));
            break;
        }
    }
#endif
}

void GLState::considerNativeStateUndefined()
{
    internal::currentProps.clear();
    internal::currentTarget = 0;
}

GLState &GLState::current()
{
    DENG2_ASSERT(!internal::stack.isEmpty());
    return *internal::stack.last();
}

GLState &GLState::push()
{
    // Duplicate the topmost state.
    push(new GLState(current()));
    return current();
}

GLState &GLState::pop()
{
    delete take();
    return current();
}

void GLState::push(GLState *state)
{
    internal::stack.append(state);
}

GLState *GLState::take()
{
    DENG2_ASSERT(internal::stack.size() > 1);
    return internal::stack.takeLast();
}

dsize GLState::stackDepth()
{
    return internal::stack.size();
}

} // namespace de
