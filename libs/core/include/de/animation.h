/** @file animation.h Animation function.
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCORE_ANIMATION_H
#define LIBCORE_ANIMATION_H

#include "de/string.h"
#include "de/time.h"
#include "de/clock.h"
#include "de/iserializable.h"
#include "de/deletable.h"

namespace de {

/**
 * Animates a value with a transition function.
 *
 * @note Calling Animation::setFrameTime() is mandatory in the beginning of
 * each frame. This will update the time that all animation instances will use
 * during the frame.
 *
 * @ingroup math
 */
class DE_PUBLIC Animation : public ISerializable, public Deletable
{
public:
    enum Style {
        Linear,
        EaseOut,
        EaseOutSofter,
        EaseIn,
        EaseBoth,
        Bounce,
        FixedBounce
    };

    /// Animation has no defined time source. @ingroup errors
    DE_ERROR(ClockMissingError);

public:
    Animation(float value = 0, Style style = EaseOut);
    Animation(const Animation &other);
    Animation &operator = (const Animation &other);

    void setStyle(Style s);
    void setStyle(Style style, float bounce);

    Style style() const;
    float bounce() const;

    /**
     * Starts a new transition.
     *
     * @param toValue         Target value.
     * @param transitionSpan  Number of seconds that the entire transition will take.
     *                        This includes @a startDelay.
     * @param startDelay      Number of seconds to wait before starting the transition.
     */
    void setValue(float toValue, TimeSpan transitionSpan = 0.0, TimeSpan startDelay = 0.0);

    void setValue(int toValue, TimeSpan transitionSpan = 0.0, TimeSpan startDelay = 0.0);

    /**
     * Starts a new transition.
     *
     * @param fromValue       Value to start from.
     * @param toValue         Target value.
     * @param transitionSpan  Number of seconds that the entire transition will take.
     *                        This includes @a startDelay.
     * @param startDelay      Number of seconds to wait before starting the transition.
     */
    void setValueFrom(float    fromValue,
                      float    toValue,
                      TimeSpan transitionSpan = 0.0,
                      TimeSpan startDelay     = 0.0);

    /**
     * Current value.
     */
    float value() const;

    /**
     * Determines whether the transition has been completed.
     */
    bool done() const;

    /**
     * Current target value.
     */
    float target() const;

    /**
     * Changes the target value without modifying any other parameters.
     *
     * @param newTarget  Target value.
     */
    void adjustTarget(float newTarget);

    /**
     * Number of seconds remaining in the ongoing transition.
     */
    TimeSpan remainingTime() const;

    /**
     * Number of seconds for the entire transition.
     */
    TimeSpan transitionTime() const;

    /**
     * Move the current value and the target value by @a valueDelta.
     * Does not influence an ongoing transition.
     */
    void shift(float valueDelta);

    /**
     * Pauses the animation, if it is currently ongoing. resume() can then be called
     * later to continue at a later point in time, with the end point appropriately
     * postponed into the future.
     */
    void pause();

    /**
     * Resumes a previously paused animation.
     */
    void resume();

    /**
     * Complete the ongoing transition immediately.
     */
    void finish();

    /**
     * Regular assignment simply changes the value of immediately.
     *
     * @param value  New value for the Animation, set without a transition.
     */
    inline Animation &operator = (float value) {
        setValue(value);
        return *this;
    }

    /**
     * Implicit conversion to float (current value).
     */
    inline operator float() const { return value(); }

    String asText() const;

    /**
     * Returns the clock used for this animation.
     */
    const Clock &clock();

    // Implements ISerializable.
    void operator >> (Writer &to) const;
    void operator << (Reader &from);

public:
    /**
     * The clock that controls time of all animation instances.
     * This must be called before any animations are instantiated.
     *
     * @param clock  Clock.
     */
    static void setClock(const Clock *clock);

    static TimeSpan currentTime();

    static Animation range(Style style, float from, float to, TimeSpan span, TimeSpan delay = 0.0);

private:
    DE_PRIVATE(d)

    static const Clock *_clock;
};

} // namespace de

#endif // LIBCORE_ANIMATION_H
