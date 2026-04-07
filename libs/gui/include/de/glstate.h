/** @file glstate.h  GL state.
 *
 * GL state management is abstracted inside this class to retain plausible
 * independence from OpenGL as the underlying rendering API. Also, Direct3D
 * uses object-based state management.
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

#ifndef LIBGUI_GLSTATE_H
#define LIBGUI_GLSTATE_H

#include <de/libcore.h>
#include <de/rectangle.h>
#include <utility>

#include "de/glinfo.h"

namespace de {

class GLFramebuffer;

namespace gfx /// OpenGL constants, flags, and other definitions.
{
    enum ColorMaskFlag {
        WriteNone  = 0,
        WriteRed   = 0x1,
        WriteGreen = 0x2,
        WriteBlue  = 0x4,
        WriteAlpha = 0x8,
        WriteAll   = WriteRed | WriteGreen | WriteBlue | WriteAlpha
    };
    using ColorMask = Flags;

    enum Face {
        None,
        Front,
        Back,
        FrontAndBack,
    };
    enum Comparison {
        Never,
        Always,
        Equal,
        NotEqual,
        Less,
        Greater,
        LessOrEqual,
        GreaterOrEqual,
    };
    enum Blend {
        Zero,
        One,
        SrcColor,
        OneMinusSrcColor,
        SrcAlpha,
        OneMinusSrcAlpha,
        DestColor,
        OneMinusDestColor,
        DestAlpha,
        OneMinusDestAlpha,
    };
    typedef std::pair<Blend, Blend> BlendFunc;
    enum BlendOp {
        Add,
        Subtract,
        ReverseSubtract,
    };
    enum class StencilOp {
        Keep,
        Zero,
        Replace,
        Increment,
        IncrementWrap,
        Decrement,
        DecrementWrap,
        Invert,
    };
    struct StencilOps {
        StencilOp stencilFail;
        StencilOp depthFail;
        StencilOp depthPass;
    };
    struct StencilFunc {
        Comparison func;
        dint ref;
        duint mask;
    };
} // namespace gfx

/**
 * GL state.
 *
 * All manipulation of OpenGL state must occur through this class. If OpenGL
 * state is changed manually, it will result in GLState not knowing about it,
 * potentially leading to the incorrect state being in effect later on.
 *
 * GLState instances can either be created on demand with GLState::push(), or
 * one can keep a GLState instance around for repeating use. The stack exists
 * to aid structured drawing: it is not required to use the stack for all
 * drawing. GLState::apply() can be called for any GLState instance to use it
 * as the current GL state.
 *
 * @note The default viewport is (0,0)&rarr;(0,0). The viewport has to be set
 * when the desired size is known (for instance when a Canvas is resized).
 *
 * @ingroup gl
 */
class LIBGUI_PUBLIC GLState
{
public:
    /**
     * Constructs a GL state with the default values for all properties.
     */
    GLState();

    GLState(const GLState &other);
    GLState &operator=(const GLState &other);
    bool operator==(const GLState &);

    GLState &setCull(gfx::Face cullFace);
    GLState &setDepthTest(bool enable);
    GLState &setDepthFunc(gfx::Comparison func);
    GLState &setDepthWrite(bool enable);
    GLState &setAlphaTest(bool enable);
    GLState &setAlphaLimit(float greaterThanValue);
    GLState &setBlend(bool enable);
    GLState &setBlendFunc(gfx::Blend src, gfx::Blend dest);
    GLState &setBlendFunc(gfx::BlendFunc func);
    GLState &setBlendOp(gfx::BlendOp op);
    GLState &setColorMask(gfx::ColorMask mask);
    GLState &setStencilTest(bool enable);
    GLState &setStencilFunc(gfx::Comparison func,
                            dint            ref,
                            duint           mask,
                            gfx::Face       face = gfx::FrontAndBack);
    GLState &setStencilOp(gfx::StencilOp stencilFail,
                          gfx::StencilOp depthFail,
                          gfx::StencilOp depthPass,
                          gfx::Face      face = gfx::FrontAndBack);
    GLState &setStencilMask(duint mask, gfx::Face face = gfx::FrontAndBack);
    GLState &setTarget(GLFramebuffer &target);
    GLState &setDefaultTarget();
    GLState &setViewport(const Rectanglei &viewportRect);
    GLState &setViewport(const Rectangleui &viewportRect);
    inline GLState &setViewport(const Vec2ui &size) { return setViewport(Rectangleui::fromSize(size)); }

    /**
     * Sets a viewport using coordinates that have been normalized within the
     * current render target. This is useful for operations that should be
     * independent of target size.
     *
     * @param normViewportRect  Normalized viewport rectangle.
     */
    GLState &setNormalizedViewport(const Rectanglef &normViewportRect);

    GLState &setScissor(const Rectanglei &scissorRect);
    GLState &setScissor(const Rectangleui &scissorRect);

    /**
     * Sets a scissor using coordinates that have been normalized within the
     * current viewport.
     *
     * @param normScissorRect  Normalized scissor rectangle.
     */
    GLState &setNormalizedScissor(const Rectanglef &normScissorRect);

    GLState &clearScissor();

    gfx::Face        cull() const;
    bool             depthTest() const;
    gfx::Comparison  depthFunc() const;
    bool             depthWrite() const;
    bool             alphaTest() const;
    float            alphaLimit() const;
    bool             blend() const;
    gfx::Blend       srcBlendFunc() const;
    gfx::Blend       destBlendFunc() const;
    gfx::BlendFunc   blendFunc() const;
    gfx::BlendOp     blendOp() const;
    gfx::ColorMask   colorMask() const;
    bool             stencilTest() const;
    duint            stencilMask(gfx::Face face = gfx::Front) const;
    gfx::StencilOps  stencilOp(gfx::Face face = gfx::Front) const;
    gfx::StencilFunc stencilFunc(gfx::Face face = gfx::Front) const;
    GLFramebuffer &  target() const;
    Rectangleui      viewport() const;
    Rectanglef       normalizedViewport() const;
    bool             scissor() const;
    Rectangleui      scissorRect() const;

    /**
     * Updates the OpenGL state to match this GLState. Until this is called no
     * changes occur in the OpenGL state. Calling this more than once is
     * allowed; the subsequent calls do nothing.
     *
     * @todo Remove excess calls to apply() once all direct GL1 state
     * manipulation has been removed.
     */
    void apply() const;

public:
    /**
     * Tells GLState to consider the native OpenGL state undefined, meaning
     * that when the next GLState is applied, all properties need to be set
     * rather than just the changed ones.
     *
     * @todo Remove this once all direct OpenGL state changes have been
     * removed.
     */
    static void considerNativeStateUndefined();

    static GLFramebuffer *currentTarget();

    /**
     * Returns the current (i.e., topmost) state on the GL state stack.
     */
    static GLState &current();

    /**
     * Pushes a copy of the current state onto the current thread's GL state
     * stack.
     *
     * @return  Reference to the new state.
     */
    static GLState &push();

    /**
     * Pops the topmost state off the current thread's stack. Returns a reference
     * to the state that has now become the new current state.
     */
    static GLState &pop();

    /**
     * Pushes a state onto the current thread's GL state stack.
     *
     * @param state  State to push. Ownership taken.
     */
    static void push(GLState *state);

    /**
     * Removes the topmost state off of the current thread's stack.
     *
     * @return State instance. Ownership given to caller.
     */
    static GLState *take();

    static dsize stackDepth();

private:
    DE_PRIVATE(d)
};

/// State stack. Each GL context has its own state stack that is activated together with
/// the context.
struct GLStateStack : public List<GLState *> {
    GLStateStack()
    {
        // Initialize with a default state.
        append(new GLState);
    }
    ~GLStateStack() { deleteAll(*this); }

    /**
     * Specifies a state stack to be operated on with the static GLState push/pop methods.
     */
    static void activate(GLStateStack &stack);
};

} // namespace de

#endif // LIBGUI_GLSTATE_H
