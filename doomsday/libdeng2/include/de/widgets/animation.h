/** @file animation.h Animation function.
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_ANIMATION_H
#define LIBDENG2_ANIMATION_H

#include "../String"
#include "../Time"
#include "../Clock"
#include "../ISerializable"

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
class Animation : public ISerializable
{
public:
    enum Style {
        Linear,
        EaseIn,
        EaseOut,
        EaseBoth,
        Bounce,
        FixedBounce
    };

    /// Animation has no defined time source. @ingroup errors
    DENG2_ERROR(ClockMissingError);

public:
    Animation(float value = 0, Style style = EaseIn);

    virtual ~Animation();

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
    void setValue(float toValue, TimeDelta transitionSpan = 0, TimeDelta startDelay = 0);

    void setValue(int toValue, TimeDelta transitionSpan = 0, TimeDelta startDelay = 0);

    /**
     * Starts a new transition.
     *
     * @param fromValue       Value to start from.
     * @param toValue         Target value.
     * @param transitionSpan  Number of seconds that the entire transition will take.
     *                        This includes @a startDelay.
     * @param startDelay      Number of seconds to wait before starting the transition.
     */
    void setValueFrom(float fromValue, float toValue, TimeDelta transitionSpan = 0, TimeDelta startDelay = 0);

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
    TimeDelta remainingTime() const;

    /**
     * Move the current value and the target value by @a valueDelta.
     * Does not influence an ongoing transition.
     */
    void shift(float valueDelta);

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
        setValue(v);
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
    Clock const &clock();

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
    static void setClock(Clock const *clock);

    static Time currentTime();

private:
    struct Instance;
    Instance *d;

    static Clock const *_clock;
};

} // namespace de

#endif // LIBDENG2_ANIMATION_H
