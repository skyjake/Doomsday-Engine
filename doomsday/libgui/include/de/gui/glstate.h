/** @file glstate.h  GL state.
 *
 * GL state management is abstracted inside this class to retain plausible
 * independence from OpenGL as the underlying rendering API. Also, Direct3D
 * uses object-based state management.
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

#ifndef LIBGUI_GLSTATE_H
#define LIBGUI_GLSTATE_H

#include <de/libdeng2.h>
#include <de/Rectangle>
#include <utility>

#include "libgui.h"

namespace de {

class GLTarget;

namespace gl
{
    enum Comparison {
        Never,
        Always,
        Equal,
        NotEqual,
        Less,
        Greater,
        LessOrEqual,
        GreaterOrEqual
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
        OneMinusDestAlpha
    };
    typedef std::pair<Blend, Blend> BlendFunc;
    enum BlendOp {
        Add,
        Subtract,
        ReverseSubtract
    };
    enum Cull {
        None,
        Front,
        Back
    };
} // namespace gl

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

    GLState(GLState const &other);

    GLState &setCull(gl::Cull mode);
    GLState &setDepthTest(bool enable);
    GLState &setDepthFunc(gl::Comparison func);
    GLState &setDepthWrite(bool enable);
    GLState &setBlend(bool enable);
    GLState &setBlendFunc(gl::Blend src, gl::Blend dest);
    GLState &setBlendFunc(gl::BlendFunc func);
    GLState &setBlendOp(gl::BlendOp op);
    GLState &setTarget(GLTarget &target);
    GLState &setDefaultTarget();
    GLState &setViewport(Rectangleui const &viewportRect);
    GLState &setScissor(Rectanglei const &scissorRect);
    GLState &setScissor(Rectangleui const &scissorRect);
    GLState &clearScissor();

    gl::Cull cull() const;
    bool depthTest() const;
    gl::Comparison depthFunc() const;
    bool depthWrite() const;
    bool blend() const;
    gl::Blend srcBlendFunc() const;
    gl::Blend destBlendFunc() const;
    gl::BlendFunc blendFunc() const;
    gl::BlendOp blendOp() const;
    GLTarget &target() const;
    Rectangleui viewport() const;
    bool scissor() const;
    Rectangleui scissorRect() const;

    /**
     * Updates the OpenGL state to match this GLState. Until this is called no
     * changes occur in the OpenGL state. Calling this more than once is
     * allowed; the subsequent calls do nothing.
     */
    void apply() const;

public:
    /**
     * Tells GLState to consider the native OpenGL state undefined, meaning
     * that when the next GLState is applied, all properties need to be set
     * rather than just the changed ones.
     */
    static void considerNativeStateUndefined();

    /**
     * Returns the current topmost state on the GL state stack.
     */
    static GLState &top();

    /**
     * Pushes a copy of the current state onto the current thread's GL state
     * stack.
     *
     * @return  Reference to the new state.
     */
    static GLState &push();

    /**
     * Pops the topmost state off the current thread's stack.
     */
    static void pop();

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

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBGUI_GLSTATE_H
