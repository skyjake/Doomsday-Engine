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

#include "de/glstate.h"
#include "de/glframebuffer.h"
#include "de/opengl.h"
#include <de/glinfo.h>
#include <de/bitfield.h>
#include <de/log.h>

namespace de {

#ifdef DE_DEBUG
extern int GLDrawQueue_queuedElems;
#endif

namespace internal {

GLenum glComp(gfx::Comparison comp)
{
    switch (comp)
    {
    case gfx::Never:          return GL_NEVER;
    case gfx::Always:         return GL_ALWAYS;
    case gfx::Equal:          return GL_EQUAL;
    case gfx::NotEqual:       return GL_NOTEQUAL;
    case gfx::Less:           return GL_LESS;
    case gfx::Greater:        return GL_GREATER;
    case gfx::LessOrEqual:    return GL_LEQUAL;
    case gfx::GreaterOrEqual: return GL_GEQUAL;
    }
    return GL_NEVER;
}

enum Property {
    CullFace,
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
    StencilTest,
    StencilFrontMask,
    StencilFrontOp,
    StencilFrontFunc,
    StencilBackMask,
    StencilBackOp,
    StencilBackFunc,
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
    { CullFace,             2  },
    { DepthTest,            1  },
    { DepthFunc,            3  },
    { DepthWrite,           1  },
    { AlphaTest,            1  },
    { AlphaLimit,           8  },
    { Blend,                1  },
    { BlendFuncSrc,         4  },
    { BlendFuncDest,        4  },
    { BlendOp,              2  },
    { ColorMask,            4  },
    { StencilTest,          1  },
    { StencilFrontMask,     8  },
    { StencilBackMask,      8  },
    { StencilFrontOp,       9  },
    { StencilBackOp,        9  },
    { StencilFrontFunc,     19 },
    { StencilBackFunc,      19 },
    { Scissor,              1  },
    { ScissorX,             13 }, // 13 bits == 8192 max
    { ScissorY,             13 },
    { ScissorWidth,         13 },
    { ScissorHeight,        13 },
    { ViewportX,            13 },
    { ViewportY,            13 },
    { ViewportWidth,        13 },
    { ViewportHeight,       13 }
};
static BitField::Elements const glStateProperties(propSpecs, MAX_PROPERTIES);

static GLStateStack *currentStateStack;

/// Currently applied GL state properties.
static BitField currentProps;

/// Observes the current target and clears the pointer if it happens to get
/// deleted.
class CurrentTarget : DE_OBSERVES(Asset, Deletion)
{
    GLFramebuffer *_target;

    void assetBeingDeleted(Asset &asset)
    {
        if (&asset == _target)
        {
            LOG_AS("GLState");
            LOGDEV_GL_NOTE("Current target destroyed, clearing pointer");
            _target = 0;
        }
    }

public:
    CurrentTarget()
        : _target(nullptr)
    {}
    ~CurrentTarget() { set(nullptr); }
    void set(GLFramebuffer *trg)
    {
        if (_target)
        {
            _target->audienceForDeletion() -= this;
        }
        _target = trg;
        if (_target)
        {
            _target->audienceForDeletion() += this;
        }
    }
    GLFramebuffer *get() const { return _target; }
    CurrentTarget &operator=(GLFramebuffer *trg)
    {
        set(trg);
        return *this;
    }
    bool operator!=(GLFramebuffer *trg) const { return _target != trg; }
    operator GLFramebuffer *() const { return _target; }
};
static CurrentTarget currentTarget;

} // namespace internal

DE_PIMPL(GLState)
{
    BitField       props;
    GLFramebuffer *target;

    Impl(Public *i)
        : Base(i)
        , props(internal::glStateProperties)
        , target(nullptr)
    {}

    Impl(Public *i, const Impl &other)
        : Base(i)
        , props(other.props)
        , target(other.target)
    {}

    static GLenum glFace(gfx::Face face)
    {
        switch (face)
        {
        case gfx::None:         return GL_NONE;
        case gfx::Front:        return GL_FRONT;
        case gfx::Back:         return GL_BACK;
        case gfx::FrontAndBack: return GL_FRONT_AND_BACK;
        }
        return GL_NONE;
    }

    static GLenum glStencilOp(gfx::StencilOp op)
    {
        switch (op)
        {
        case gfx::StencilOp::Keep:          return GL_KEEP;
        case gfx::StencilOp::Zero:          return GL_ZERO;
        case gfx::StencilOp::Replace:       return GL_REPLACE;
        case gfx::StencilOp::Increment:     return GL_INCR;
        case gfx::StencilOp::IncrementWrap: return GL_INCR_WRAP;
        case gfx::StencilOp::Decrement:     return GL_DECR;
        case gfx::StencilOp::DecrementWrap: return GL_DECR_WRAP;
        case gfx::StencilOp::Invert:        return GL_INVERT;
        }
        return GL_KEEP;
    }

    static GLenum glBFunc(gfx::Blend f)
    {
        switch (f)
        {
        case gfx::Zero:              return GL_ZERO;
        case gfx::One:               return GL_ONE;
        case gfx::SrcColor:          return GL_SRC_COLOR;
        case gfx::OneMinusSrcColor:  return GL_ONE_MINUS_SRC_COLOR;
        case gfx::SrcAlpha:          return GL_SRC_ALPHA;
        case gfx::OneMinusSrcAlpha:  return GL_ONE_MINUS_SRC_ALPHA;
        case gfx::DestColor:         return GL_DST_COLOR;
        case gfx::OneMinusDestColor: return GL_ONE_MINUS_DST_COLOR;
        case gfx::DestAlpha:         return GL_DST_ALPHA;
        case gfx::OneMinusDestAlpha: return GL_ONE_MINUS_DST_ALPHA;
        }
        return GL_ZERO;
    }

    static gfx::Blend fromGlBFunc(GLenum e)
    {
        switch (e)
        {
        case GL_ZERO:                return gfx::Zero;
        case GL_ONE:                 return gfx::One;
        case GL_SRC_COLOR:           return gfx::SrcColor;
        case GL_ONE_MINUS_SRC_COLOR: return gfx::OneMinusSrcColor;
        case GL_SRC_ALPHA:           return gfx::SrcAlpha;
        case GL_ONE_MINUS_SRC_ALPHA: return gfx::OneMinusSrcAlpha;
        case GL_DST_COLOR:           return gfx::DestColor;
        case GL_ONE_MINUS_DST_COLOR: return gfx::OneMinusDestColor;
        case GL_DST_ALPHA:           return gfx::DestAlpha;
        case GL_ONE_MINUS_DST_ALPHA: return gfx::OneMinusDestAlpha;
        default: break;
        }
        return gfx::Zero;
    }

    void glApply(internal::Property prop)
    {
        switch (prop)
        {
        case internal::CullFace:
            switch (self().cull())
            {
            case gfx::None:
                glDisable(GL_CULL_FACE);
                break;
            case gfx::Front:
            case gfx::Back:
            case gfx::FrontAndBack:
                glEnable(GL_CULL_FACE);
                glCullFace(glFace(self().cull()));
                break;
            }
            break;

        case internal::DepthTest:
            if (self().depthTest())
                glEnable(GL_DEPTH_TEST);
            else
                glDisable(GL_DEPTH_TEST);
            break;

        case internal::DepthFunc:
            glDepthFunc(internal::glComp(self().depthFunc()));
            break;

        case internal::DepthWrite:
            if (self().depthWrite())
                glDepthMask(GL_TRUE);
            else
                glDepthMask(GL_FALSE);
            break;

        case internal::AlphaTest:
            /*if (self().alphaTest())
                glEnable(GL_ALPHA_TEST);
            else
                glDisable(GL_ALPHA_TEST);*/

            // TODO: use a shared GLUniform available to all shaders that need it

            //qDebug() << "[GLState] Alpha test:" << (self().alphaTest()? "enabled" : "disabled");
            break;

        case internal::AlphaLimit:
            //glAlphaFunc(GL_GREATER, self().alphaLimit());

            // TODO: use a shared GLUniform available to all shaders that need it

            break;

        case internal::Blend:
            if (self().blend())
                glEnable(GL_BLEND);
            else
                glDisable(GL_BLEND);
            break;

        case internal::BlendFuncSrc:
        case internal::BlendFuncDest:
            glBlendFuncSeparate(
                glBFunc(self().srcBlendFunc()), glBFunc(self().destBlendFunc()), GL_ONE, GL_ONE);
            break;

        case internal::BlendOp:
            switch (self().blendOp())
            {
            case gfx::Add:
                glBlendEquation(GL_FUNC_ADD);
                break;
            case gfx::Subtract:
                glBlendEquation(GL_FUNC_SUBTRACT);
                break;
            case gfx::ReverseSubtract:
                glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
                break;
            }
            break;

        case internal::ColorMask:
        {
            const gfx::ColorMask mask = self().colorMask();
            glColorMask((mask & gfx::WriteRed) != 0,
                           (mask & gfx::WriteGreen) != 0,
                           (mask & gfx::WriteBlue) != 0,
                           (mask & gfx::WriteAlpha) != 0);
            break;
        }

        case internal::StencilTest:
            if (self().stencilTest())
                glEnable(GL_STENCIL_TEST);
            else
                glDisable(GL_STENCIL_TEST);
            break;

        case internal::StencilFrontMask:
        case internal::StencilBackMask:
        {
            const gfx::Face face = (prop == internal::StencilFrontMask? gfx::Front : gfx::Back);
            glStencilMaskSeparate(glFace(face), self().stencilMask(face));
            break;
        }

        case internal::StencilFrontFunc:
        case internal::StencilBackFunc:
        {
            const gfx::Face face = (prop == internal::StencilFrontFunc? gfx::Front : gfx::Back);
            const auto stf = self().stencilFunc(face);
            glStencilFuncSeparate(glFace(face), internal::glComp(stf.func), stf.ref, stf.mask);
            break;
        }

        case internal::StencilFrontOp:
        case internal::StencilBackOp:
        {
            const gfx::Face face = (prop == internal::StencilFrontOp? gfx::Front : gfx::Back);
            const auto sop = self().stencilOp(face);
            glStencilOpSeparate(glFace(face),
                                   glStencilOp(sop.stencilFail),
                                   glStencilOp(sop.depthFail),
                                   glStencilOp(sop.depthPass));
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
                glEnable(GL_SCISSOR_TEST);

                Rectangleui origScr;
                if (self().scissor())
                {
                    origScr = self().scissorRect();
                }
                else
                {
                    origScr = Rectangleui::fromSize(self().target().size());
                }

                const Rectangleui scr = self().target().scaleToActiveRect(origScr);
                glScissor(scr.left(), self().target().size().y - scr.bottom(),
                                    scr.width(), scr.height());
            }
            else
            {
                glDisable(GL_SCISSOR_TEST);
            }
            break;
        }

        case internal::ViewportX:
        case internal::ViewportY:
        case internal::ViewportWidth:
        case internal::ViewportHeight:
        {
            const Rectangleui vp = self().target().scaleToActiveRect(self().viewport());
            glViewport(vp.left(), self().target().size().y - vp.bottom(),
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
    setCull       (gfx::None);
    setDepthTest  (false);
    setDepthFunc  (gfx::Less);
    setDepthWrite (true);
    setAlphaTest  (true);
    setAlphaLimit (0);
    setBlend      (true);
    setBlendFunc  (gfx::One, gfx::Zero);
    setBlendOp    (gfx::Add);
    setColorMask  (gfx::WriteAll);
    setStencilTest(false);
    setStencilMask(255);
    setStencilOp  (gfx::StencilOp::Keep, gfx::StencilOp::Keep, gfx::StencilOp::Keep);
    setStencilFunc(gfx::Always, 0, 255);

    setDefaultTarget();
}

GLState::GLState(const GLState &other) : d(new Impl(this, *other.d))
{}

GLState &GLState::operator=(const GLState &other)
{
    d.reset(new Impl(this, *other.d));
    return *this;
}

bool GLState::operator==(const GLState &other)
{
    return d->target == other.d->target && d->props == other.d->props;
}

GLState &GLState::setCull(gfx::Face mode)
{
    d->props.set(internal::CullFace, duint(mode));
    return *this;
}

GLState &GLState::setDepthTest(bool enable)
{
    d->props.set(internal::DepthTest, enable);
    return *this;
}

GLState &GLState::setDepthFunc(gfx::Comparison func)
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

GLState &GLState::setBlendFunc(gfx::Blend src, gfx::Blend dest)
{
    d->props.set(internal::BlendFuncSrc,  duint(src));
    d->props.set(internal::BlendFuncDest, duint(dest));
    return *this;
}

GLState &GLState::setBlendFunc(gfx::BlendFunc func)
{
    d->props.set(internal::BlendFuncSrc,  duint(func.first));
    d->props.set(internal::BlendFuncDest, duint(func.second));
    return *this;
}

GLState &GLState::setBlendOp(gfx::BlendOp op)
{
    d->props.set(internal::BlendOp, duint(op));
    return *this;
}

GLState &GLState::setColorMask(gfx::ColorMask mask)
{
    d->props.set(internal::ColorMask, duint(mask));
    return *this;
}

GLState &de::GLState::setStencilTest(bool enable)
{
    d->props.set(internal::StencilTest, enable);
    return *this;
}

GLState &GLState::setStencilFunc(gfx::Comparison func, dint ref, duint mask, gfx::Face face)
{
    const duint packed = duint(func) | ((duint(ref) & 255) << 3) | ((mask & 255) << 11);

    if (face == gfx::Front || face == gfx::FrontAndBack)
    {
        d->props.set(internal::StencilFrontFunc, packed);
    }
    if (face == gfx::Back || face == gfx::FrontAndBack)
    {
        d->props.set(internal::StencilBackFunc, packed);
    }
    return *this;
}

GLState &GLState::setStencilOp(gfx::StencilOp stencilFail,
                               gfx::StencilOp depthFail,
                               gfx::StencilOp depthPass,
                               gfx::Face      face)
{
    const duint packed = duint(stencilFail) | (duint(depthFail) << 3) | (duint(depthPass) << 6);

    if (face == gfx::Front || face == gfx::FrontAndBack)
    {
        d->props.set(internal::StencilFrontOp, packed);
    }
    if (face == gfx::Back || face == gfx::FrontAndBack)
    {
        d->props.set(internal::StencilBackOp, packed);
    }
    return *this;
}

GLState &GLState::setStencilMask(duint mask, gfx::Face face)
{
    if (face == gfx::Front || face == gfx::FrontAndBack)
    {
        d->props.set(internal::StencilFrontMask, mask);
    }
    if (face == gfx::Back || face == gfx::FrontAndBack)
    {
        d->props.set(internal::StencilBackMask, mask);
    }
    return *this;
}

GLState &GLState::setTarget(GLFramebuffer &target)
{
    d->target = &target;
    return *this;
}

GLState &GLState::setDefaultTarget()
{
    d->target = nullptr;
    return *this;
}

GLState &GLState::setViewport(const Rectanglei &viewportRect)
{
    return setViewport(viewportRect.toRectangleui());
}

GLState &GLState::setViewport(const Rectangleui &viewportRect)
{
    d->props.set(internal::ViewportX,      viewportRect.left());
    d->props.set(internal::ViewportY,      viewportRect.top());
    d->props.set(internal::ViewportWidth,  viewportRect.width());
    d->props.set(internal::ViewportHeight, viewportRect.height());
    return *this;
}

GLState &GLState::setNormalizedViewport(const Rectanglef &normViewportRect)
{
    const GLFramebuffer::Size size = target().size();
    Rectangleui vp(Vec2ui(normViewportRect.left() * size.x,
                          normViewportRect.top() * size.y),
                   Vec2ui(std::ceil(normViewportRect.right() * size.x),
                          std::ceil(normViewportRect.bottom() * size.y)));
    return setViewport(vp);
}

GLState &GLState::setScissor(const Rectanglei &scissorRect)
{
    return setScissor(scissorRect.toRectangleui());
}

GLState &GLState::setScissor(const Rectangleui &newScissorRect)
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

GLState &GLState::setNormalizedScissor(const Rectanglef &normScissorRect)
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

gfx::Face GLState::cull() const
{
    return d->props.valueAs<gfx::Face>(internal::CullFace);
}

bool GLState::depthTest() const
{
    return d->props.asBool(internal::DepthTest);
}

gfx::Comparison GLState::depthFunc() const
{
    return d->props.valueAs<gfx::Comparison>(internal::DepthFunc);
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

gfx::Blend GLState::srcBlendFunc() const
{
    return d->props.valueAs<gfx::Blend>(internal::BlendFuncSrc);
}

gfx::Blend GLState::destBlendFunc() const
{
    return d->props.valueAs<gfx::Blend>(internal::BlendFuncDest);
}

gfx::BlendFunc GLState::blendFunc() const
{
    return gfx::BlendFunc(srcBlendFunc(), destBlendFunc());
}

gfx::BlendOp GLState::blendOp() const
{
    return d->props.valueAs<gfx::BlendOp>(internal::BlendOp);
}

gfx::ColorMask GLState::colorMask() const
{
    return d->props.valueAs<gfx::ColorMask>(internal::ColorMask);
}

bool GLState::stencilTest() const
{
    return d->props.asBool(internal::StencilTest);
}

duint GLState::stencilMask(gfx::Face face) const
{
    return d->props.asUInt(face == gfx::Back? internal::StencilBackMask : internal::StencilFrontMask);
}

gfx::StencilOps GLState::stencilOp(gfx::Face face) const
{
    const duint packed =
        d->props.asUInt(face == gfx::Back ? internal::StencilBackOp : internal::StencilFrontOp);
    return {
        gfx::StencilOp( packed       & 7),
        gfx::StencilOp((packed >> 3) & 7),
        gfx::StencilOp((packed >> 6) & 7)
    };
}

gfx::StencilFunc GLState::stencilFunc(gfx::Face face) const
{
    const duint packed =
        d->props.asUInt(face == gfx::Back ? internal::StencilBackFunc : internal::StencilFrontFunc);
    return {
        gfx::Comparison(packed        & 7),
        dint          ((packed >> 3)  & 255),
        duint         ((packed >> 11) & 255)
    };
}

GLFramebuffer &GLState::target() const
{
    DE_ASSERT(d->target != nullptr);
    return *d->target;
}

Rectangleui GLState::viewport() const
{
    return {d->props[internal::ViewportX],
            d->props[internal::ViewportY],
            d->props[internal::ViewportWidth],
            d->props[internal::ViewportHeight]};
}

Rectanglef GLState::normalizedViewport() const
{
    const GLFramebuffer::Size size = target().size();
    const Rectangleui vp = viewport();
    return {float(vp.left())   / float(size.x),
            float(vp.top())    / float(size.y),
            float(vp.width())  / float(size.x),
            float(vp.height()) / float(size.y)};
}

bool GLState::scissor() const
{
    return d->props.asBool(internal::Scissor);
}

Rectangleui GLState::scissorRect() const
{
    return {d->props[internal::ScissorX],
            d->props[internal::ScissorY],
            d->props[internal::ScissorWidth],
            d->props[internal::ScissorHeight]};
}

void GLState::apply() const
{
    LIBGUI_ASSERT_GL_OK();

#ifdef DE_DEBUG
    DE_ASSERT(GLDrawQueue_queuedElems == 0);
#endif

    // Actual OpenGL state shouldn't be changed outside the render thread.
    // The main thread can still manipulate shared OpenGL objects, though.
    DE_ASSERT_IN_RENDER_THREAD();

    bool forceViewportAndScissor = false;

    // Update the render target.
    GLFramebuffer *newTarget = &target();
    DE_ASSERT(newTarget != nullptr);

    if (internal::currentTarget != newTarget)
    {
        const GLFramebuffer *oldTarget = internal::currentTarget;
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
        for (BitField::Id id : changed)
        {
            d->glApply(internal::Property(id));
        }
        internal::currentProps = d->props;
    }

#if 0
    // Verify that the state is correct.
    for (int i = 0; i < d->props.elements().size(); ++i)
    {
        const auto &elem = d->props.elements().at(i);
        int val;
        switch (elem.id)
        {
        case internal::Blend:
            glGetIntegerv(GL_BLEND, &val);
            DE_ASSERT(!val == !d->props.asBool(elem.id));
            break;

        case internal::BlendFuncSrc:
            glGetIntegerv(GL_BLEND_SRC_RGB, &val);
            DE_ASSERT(d->fromGlBFunc(val) == d->props.asUInt(elem.id));
            break;

        case internal::BlendFuncDest:
            glGetIntegerv(GL_BLEND_DST_RGB, &val);
            DE_ASSERT(d->fromGlBFunc(val) == d->props.asUInt(elem.id));
            break;

        case internal::BlendOp:
            glGetIntegerv(GL_BLEND_EQUATION_RGB, &val);
            val = (val == GL_FUNC_ADD? gfx::Add :
                   val == GL_FUNC_SUBTRACT? gfx::Subtract :
                   val == GL_FUNC_REVERSE_SUBTRACT? gfx::ReverseSubtract : 0);
            DE_ASSERT(val == d->props.asUInt(elem.id));
            break;
        }
    }
#endif
}

void GLState::considerNativeStateUndefined()
{
    internal::currentProps.clear();
    internal::currentTarget = nullptr;
}

GLFramebuffer *GLState::currentTarget()
{
    return internal::currentTarget;
}

GLState &GLState::current()
{
    DE_ASSERT(internal::currentStateStack);
    DE_ASSERT(!internal::currentStateStack->isEmpty());
    return *internal::currentStateStack->last();
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
    DE_ASSERT(internal::currentStateStack);
    internal::currentStateStack->append(state);
}

GLState *GLState::take()
{
    DE_ASSERT(internal::currentStateStack);
    DE_ASSERT(internal::currentStateStack->size() > 1);
    return internal::currentStateStack->takeLast();
}

dsize GLState::stackDepth()
{
    DE_ASSERT(internal::currentStateStack);
    return internal::currentStateStack->size();
}

void GLStateStack::activate(GLStateStack &stack) // static
{
    internal::currentStateStack = &stack;
    GLState::considerNativeStateUndefined();
}

} // namespace de
