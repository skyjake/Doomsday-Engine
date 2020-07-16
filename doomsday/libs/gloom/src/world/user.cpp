/** @file user.cpp
 *
 * @authors Copyright (c) 2018 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "gloom/world/user.h"
#include "gloom/world/iworld.h"
#include "gloom/audio/audiosystem.h"

#include <de/animation.h>
#include <de/variable.h>

using namespace de;

namespace gloom {

DE_PIMPL(User)
{
    const IWorld *world = nullptr;

    InputState input;
    Vec3f      pos;                // Current position of the user (feet).
    float      height     = 1.8f;  // Height from feet to top of the head.
    float      viewHeight = 1.66f; // Eye height.
    float      yaw        = 0;
    float      pitch      = 0;
    Vec3f      momentum;
    float      angularMomentum = 0;
    bool       onGround        = false;
    bool       firstUpdate     = true;
    float      crouch          = 0;
    float      crouchMomentum  = 0;
    bool       jumpPending     = false;

    // For notification:
    Vec3f prevPosition;
    float prevYaw;

    // Audio:
    TimeSpan  stepElapsed;
    Sound *   fastWind;
    Animation windVolume{0, Animation::Linear};
    Animation windFreq{0, Animation::Linear};

    Impl(Public * i) : Base(i)
    {
        pos = Vec3f(0.f);

        fastWind = &AudioSystem::get().newSound("user.fastwind");
        fastWind->setVolume(0).play(Sound::Looping);
    }

    ~Impl()
    {
        DE_NOTIFY_PUBLIC_VAR(Deletion, i) i->userBeingDeleted(self());
    }

    Vec3f frontVector() const
    {
        float yawForMove = yaw;
        return Mat4f::rotate(yawForMove, Vec3f(0, -1, 0)) * Vec3f(0, 0, -1);
    }

    void move(TimeSpan elapsed)
    {
        const float turnFriction = (input & (TurnLeft | TurnRight) ? 0 : 180);

        int turnSpeed = ((input & TurnLeft ? -1 : 0) + (input & TurnRight ? 1 : 0)) * (input & Shift ? 400 : 100);
        int accel =     ((input & Forward ?   1 : 0) + (input & Backward ? -1 : 0)) * (input & Shift ? 30 : 5);
        int sideAccel = ((input & StepRight ? 1 : 0) + (input & StepLeft ? -1 : 0)) * (input & Shift ? 30 : 5);

        angularMomentum += turnSpeed * elapsed;

        // Apply friction to angular momentum.
        {
            float fric = sign(-angularMomentum) * turnFriction;
            if (abs(fric * elapsed) > abs(angularMomentum))
            {
                angularMomentum = 0;
            }
            else
            {
                angularMomentum += fric * elapsed;
            }
            angularMomentum = clamp(angularMomentum, 150.f);
        }

        // Turn according to momentum.
        yaw += angularMomentum * elapsed;

        const Vec3f front = frontVector();
        const Vec3f side  = front.cross(Vec3f(0, 1, 0));

        momentum += (front * accel + side * sideAccel) * elapsed;

#if 0
        // "Vehicle" momentum: keep moving toward the front vector.
        if (onGround)
        {
            Vec2f planarFront = front.xz().normalize();
            if (planarFront.dot(momentum.xz()) < 0) planarFront = -planarFront;

            Vec2f frontMom = planarFront * momentum.xz().length();

            Vec2f delta = frontMom - momentum.xz();

            if (delta.length() > 10 * elapsed)
            {
                delta.setLength(10 * elapsed);
            }

            momentum.x += delta.x;
            momentum.z += delta.y;
        }
#endif

        if (onGround)
        {
            float moveFriction = 2; //5;

            // Apply friction.
            Vec2f planar = momentum.xz();
            Vec2f fric   = -planar.normalize() * moveFriction * elapsed;

            if (fric.length() > planar.length())
            {
                momentum.x = 0;
                momentum.z = 0;
            }
            else
            {
                momentum.x += fric.x;
                momentum.z += fric.y;
            }

            // Jump.
            if (input & Jump)
            {
                jumpPending = true;
            }
            else if (jumpPending)
            {
                jumpPending = false;
                momentum.y += 9;
            }
        }
        else
        {
            // Can't jump in air.
            jumpPending = false;
        }

        // Gravity.
        momentum.y -= elapsed * 9.81f;

        /*if (world)
        {
            // Gravity.
            //if (!onGround)
            {
                momentum.y -= elapsed * 9.81f;
            }
        }
        else
        {
            momentum.y = 0;
        }*/

        pos += momentum * elapsed;

        if (world)
        {
            // Keep viewer on the ground.
            const float surface = world->groundSurfaceHeight(pos);

            if (onGround)
            {
                /*if (pos.y <= surface + 10 * elapsed)
                {
                    double surfaceMomentum = (pos.y - surface) / elapsed;
                    if (surfaceMomentum < 0 && surfaceMomentum > 5)
                    {
                        // Stay on ground?
                        pos.y = surface;
                        momentum.y = 0;
                    }
                    else
                    {
                        onGround = false;
                    }
                }*/
            }

            if (pos.y <= surface + FLOAT_EPSILON)
            {
                pos.y = surface;

                if (!onGround)
                {
                    playFallDownSound();
                    if (!firstUpdate)
                    {
                        crouchMomentum = min(crouchMomentum, momentum.y + 8);
                    }
                    //momentum.y = 0;
                }
                /*else
                {
                    double surfaceMomentum = (surface - pos.y) / elapsed;
                    if (surfaceMomentum > 0)
                    {
                        // Push upward.
                        momentum.y = surfaceMomentum/20;
                    }
                }*/
                //pos.y = surface;
                momentum.y = 0;
                onGround = true;
            }
            else
            {
                onGround = false;
            }

            // Hit the ceiling?
            const float ceiling = world->ceilingHeight(pos);
            if (pos.y + height > ceiling)
            {
                pos.y      = ceiling - height;
                momentum.y = 0;
            }
        }
        else
        {
            onGround = true;
        }

        // Move in crouch.
        crouch += crouchMomentum * elapsed;
        crouchMomentum += 3 * elapsed;
        if (crouch > 0)
        {
            crouch         = 0;
            crouchMomentum = 0;
        }
        const float maxCrouch = -.6f;
        if (crouch < maxCrouch) // max crouch?
        {
            crouch         = maxCrouch;
            crouchMomentum = 0;
        }

        // Notifications.
        DE_NOTIFY_PUBLIC_VAR(PainLevel, i) { i->userPainLevel(self(), crouch / maxCrouch); }

        if (prevPosition != self().position())
        {
            DE_NOTIFY_PUBLIC_VAR(Move, i) { i->userMoved(self(), self().position()); }
            prevPosition = self().position();
        }

        if (!fequal(prevYaw, yaw))
        {
            DE_NOTIFY_PUBLIC_VAR(Turn, i) { i->userTurned(self(), yaw); }
            prevYaw = yaw;
        }

        playStepSounds(elapsed);

        // Update wind in the ears.
        windVolume.setValue(clamp(0.0f, float((float(momentum.length() / 20 - .3))), 1.0f), .1);
        windFreq  .setValue(clamp(.6f, .6f + float(momentum.length() / 50 - .3), 1.15f), .1);
        fastWind->setVolume(windVolume);
        fastWind->setFrequency(windFreq);

        firstUpdate = false;
    }

    void playFallDownSound()
    {
        if (firstUpdate) return;

        if (momentum.y > -15)
        {
            if (stepElapsed > .3)
            {
                stepElapsed = 0.0;
                playRandomStepSound();
            }
            return;
        }

        AudioSystem::get().newSound("user.falldown").setFrequency(.85f + .3f * randf()).play();
    }

    void playRandomStepSound()
    {
        AudioSystem::get()
            .newSound(Stringf("user.step%i", 1 + rand() % 5))
            .setVolume(.4f + .2f * randf())
            .setFrequency(.6f + randf() * .8f)
            .play();
    }

    void playStepSounds(TimeSpan elapsed)
    {
        ddouble velocity = momentum.xz().length();

        if (!onGround) return;
        if (velocity < .1)
        {
            // Only play footsteps when on the ground.
            stepElapsed = 0.0;
            return;
        }

        // Count time since the previous footstep.
        stepElapsed += elapsed;

        TimeSpan interval = clamp(.4, 1.0 / velocity, .8);

        if (stepElapsed > interval)
        {
            stepElapsed -= interval;
            playRandomStepSound();
        }
    }
};

User::User() : d(new Impl(this))
{}

void User::setWorld(const IWorld *world)
{
    d->world       = world;
    d->firstUpdate = true;

    if (d->world)
    {
        IWorld::POI initial = world->initialViewPosition();
        setPosition(initial.position);
        setYaw(initial.yaw);
    }
}

Vec3f User::position() const
{
    return d->pos + Vec3f(0, d->viewHeight + d->crouch, 0);
}

float User::yaw() const
{
    return d->yaw;
}

float User::pitch() const
{
    return d->pitch;
}

void User::setPosition(const Vec3f &pos)
{
    auto oldPos = d->pos;

    d->onGround = false;
    d->pos   = pos;
    d->momentum = Vec3f();

    if ((oldPos - pos).length() > 15)
    {
        DE_NOTIFY_VAR(Warp, i) i->userWarped(*this);
    }
}

void User::setYaw(float yaw)
{
    d->yaw = yaw;
    d->angularMomentum = 0;
}

void User::setPain(float pain)
{
    DE_NOTIFY_VAR(PainLevel, i) { i->userPainLevel(*this, pain); }
}

void User::setInputState(const InputState &state)
{
    d->input = state;
}

void User::turn(float yaw, float pitch)
{
    d->yaw   = wrap(d->yaw + yaw, -180.f, 180.f);
    d->pitch = Rangef(-89, 89).clamp(d->pitch + pitch);
}

void User::update(TimeSpan elapsed)
{
    d->move(elapsed);
}

Sound &User::fastWindSound()
{
    return *d->fastWind;
}

} // namespace gloom
