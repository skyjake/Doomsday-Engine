/*
 * The Doomsday Engine Project
 *
 * Copyright © 2011-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCORE_ANIMATIONRULE_H
#define LIBCORE_ANIMATIONRULE_H

#include "rule.h"

#include "de/animation.h"
#include "de/time.h"

namespace de {

/**
 * Rule with an animated value. The value is animated over time using de::Animation.
 * @ingroup widgets
 */
class DE_PUBLIC AnimationRule : public Rule, DE_OBSERVES(Clock, TimeChange)
{
public:
    explicit AnimationRule(float initialValue, Animation::Style style = Animation::EaseOut);

    /**
     * Constructs an AnimationRule whose value will animate to the specified target
     * rule. If the animation finishes and the target changes, a new animation begins
     * using the same transition time.
     *
     * @param target      Target value.
     * @param transition  Transition time to reach the current or future target values.
     * @param style       Animation style.
     */
    explicit AnimationRule(const Rule &target, TimeSpan transition, Animation::Style style = Animation::EaseOut);

    void set(float target, TimeSpan transition = 0.0, TimeSpan delay = 0.0);

    void set(const Rule &target, TimeSpan transition = 0.0, TimeSpan delay = 0.0);

    /**
     * Sets the animation style of the rule.
     *
     * @param style  Animation style.
     */
    void setStyle(Animation::Style style);

    void setStyle(Animation::Style style, float bounceSpring);

    enum Behavior {
        Singleshot               = 0x10,
        RestartWhenTargetChanges = 0x20,
        DontAnimateFromZero      = 0x40,

        FlagMask = 0xf0
    };
    using Behaviors = Flags;

    /**
     * Sets the behavior for updating the animation target. The default is Singleshot,
     * which means that if the target rule changes, the animation of the AnimationRule
     * will adjust its target accordingly without altering the ongoing or finished
     * animation.
     *
     * @param behavior  Target update behavior.
     */
    void setBehavior(Behaviors behavior);

    Behaviors behavior() const;

    /**
     * Read-only access to the scalar animation.
     */
    const Animation &animation() const {
        return _animation;
    }

    /**
     * Shifts the scalar rule's animation target and current value without
     * affecting any ongoing animation.
     *
     * @param delta  Value delta for the shift.
     */
    void shift(float delta);

    void   finish();
    void   pause();
    void   resume();
    String description() const override;

protected:
    ~AnimationRule() override;
    void update() override;
    void timeChanged(const Clock &) override;

private:
    Animation _animation;
    const Rule *_targetRule;
};

} // namespace de

#endif // LIBCORE_ANIMATIONRULE_H
