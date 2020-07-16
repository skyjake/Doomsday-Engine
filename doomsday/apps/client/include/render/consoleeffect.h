/** @file consoleeffect.h
 *
 * @authors Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DE_CLIENT_CONSOLEEFFECT_H
#define DE_CLIENT_CONSOLEEFFECT_H

#include <de/libcore.h>
#include <de/rectangle.h>
#include <de/glshaderbank.h>

/**
 * Draws camera lens effects for a particular player console. Maintains
 * console-specific state and carries out the actual GL operations.
 */
class ConsoleEffect
{
public:
    ConsoleEffect(int console);

    virtual ~ConsoleEffect();

    /**
     * Returns the console number.
     */
    int console() const;

    /**
     * Determines the console's view rectangle in window coordinates.
     */
    de::Rectanglei viewRect() const;

    bool isInited() const;

    de::GLShaderBank &shaders() const;

    /**
     * Allocate and prepare GL resources for drawing. Derived classes must call
     * the base class method if they override it.
     */
    virtual void glInit();

    /**
     * Release GL resources. Derived classes must call the base class method if
     * they override it.
     */
    virtual void glDeinit();

    /**
     * Called for all console effects when a frame begins. The methods
     * are called in the console's stack order.
     */
    virtual void beginFrame();

    /**
     * Called for all console effects in stack order, after the raw frame has
     * been drawn.
     */
    virtual void draw();

    /**
     * Called for all console effects when a frame ends. The methods are called
     * in reverse stack order, after the draw() methods have all been called.
     */
    virtual void endFrame();

private:
    DE_PRIVATE(d)
};

/// Dynamic stack of effects. (Used currently as a fixed array, though.)
struct ConsoleEffectStack
{
    typedef de::List<ConsoleEffect *> EffectList;
    EffectList effects;

    ~ConsoleEffectStack() {
        clear();
    }

    void clear() {
        deleteAll(effects);
        effects.clear();
    }
};

#endif // DE_CLIENT_CONSOLEEFFECT_H
