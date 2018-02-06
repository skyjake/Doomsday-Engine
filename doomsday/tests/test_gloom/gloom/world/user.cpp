#include "user.h"
#include "world.h"
#include "../../src/gloomapp.h"
#include "../audio/audiosystem.h"

#include <de/VRConfig>
#include <de/Animation>
#include <de/Variable>

using namespace de;

namespace gloom {

DENG2_PIMPL(User)
{
    World const *world = nullptr;

    InputState input;
    Vector3f   eyePos;
    float      yaw   = 0;
    float      pitch = 0;
    Vector3f   momentum;
    float      angularMomentum = 0;
    bool       onGround        = false;
    bool       firstUpdate     = true;
    float      crouch          = 0;
    float      crouchMomentum  = 0;

    // For notification:
    Vector3f prevPosition;
    float    prevYaw;

    // Audio:
    TimeSpan  stepElapsed;
    Sound *   fastWind;
    Animation windVolume{0, Animation::Linear};
    Animation windFreq{0, Animation::Linear};

    Impl(Public * i) : Base(i)
    {
        eyePos = Vector3f(0, 0, 0);

        fastWind = &AudioSystem::get().newSound("user.fastwind");
        fastWind->setVolume(0).play(Sound::Looping);

        //App::config()["zeroYaw"].audienceForChange() += this;
        //VRSenseApp::vr().oculusRift().setYawOffset(App::config().getd("zeroYaw"));
    }

    ~Impl()
    {
        //App::config()["zeroYaw"].audienceForChange() -= this;

        DENG2_FOR_PUBLIC_AUDIENCE(Deletion, i) i->userBeingDeleted(self());
    }

    //    void variableValueChanged(Variable &, Value const &newValue)
    //    {
    //        VRSenseApp::vr().oculusRift().setYawOffset(newValue.asNumber());
    //    }

    Vector3f frontVector() const
    {
        float yawForMove =
            yaw; // - radianToDegree(VRSenseApp::vr().oculusRift().headOrientation().z);
        return Matrix4f::rotate(yawForMove, Vector3f(0, 1, 0)) * Vector3f(0, 0, -1);
    }

    void move(TimeSpan const &elapsed)
    {
        float const turnFriction = (input & (TurnLeft | TurnRight) ? 0 : 180);

        int turnSpeed = ((input & TurnLeft ? -1 : 0) + (input & TurnRight ? 1 : 0)) * (input & Shift ? 400 : 100);
        int accel =     ((input & Forward ?   1 : 0) + (input & Backward ? -1 : 0)) * (input & Shift ? 30 : 5);
        int sideAccel = ((input & StepRight ? 1 : 0) + (input & StepLeft ? -1 : 0)) * (input & Shift ? 30 : 5);

        angularMomentum += turnSpeed * elapsed;

        // Apply friction to angular momentum.
        {
            float fric = -sign(angularMomentum) * turnFriction;
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

        //float yawForMove = yaw - radianToDegree(VRSenseApp::vr().oculusRift().headOrientation().z);

        Vector3f front = frontVector();
        Vector3f side  = front.cross(Vector3f(0, -1, 0));

        momentum += (front * accel + side * sideAccel) * elapsed;

#if 1
        // "Vehicle" momentum: keep moving toward the front vector.
        if (onGround)
        {
            Vector2f planarFront = front.xz().normalize();
            if (planarFront.dot(momentum.xz()) < 0) planarFront = -planarFront;

            Vector2f frontMom = planarFront * momentum.xz().length();

            Vector2f delta = frontMom - momentum.xz();

            if (delta.length() > 10 * elapsed)
            {
                delta.setLength(10 * elapsed);
            }

            momentum.x += delta.x;
            momentum.z += delta.y;
        }
#endif

        //if(onGround)
        {

            float moveFriction = 3;

            // Apply friction.
            Vector2f planar = momentum.xz();
            Vector2f fric   = -planar.normalize() * moveFriction * elapsed;

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
        }

        if (world)
        {
            // Gravity.
            if (!onGround)
            {
                momentum.y += elapsed * 9.81f;
            }
        }
        else
        {
            momentum.y = 0;
        }

        eyePos += momentum * elapsed;

        if (world)
        {
            // Keep viewer on the ground.
            float surface = world->groundSurfaceHeight(eyePos);
            if (onGround)
            {
                if (eyePos.y >= surface - 10 * elapsed)
                {
                    double surfaceMomentum = (surface - eyePos.y) / elapsed;
                    if (surfaceMomentum > 0 && surfaceMomentum < 5)
                    {
                        // Stay on ground?
                        eyePos.y = surface;
                    }
                }
            }

            if (eyePos.y >= surface - de::FLOAT_EPSILON)
            {
                if (!onGround)
                {
                    playFallDownSound();
                    if (!firstUpdate)
                    {
                        crouchMomentum = max(crouchMomentum, momentum.y - 14);
                    }
                    momentum.y = 0;
                }
                else
                {
                    double surfaceMomentum = (surface - eyePos.y) / elapsed;
                    if (surfaceMomentum < 0)
                    {
                        // Push upward.
                        momentum.y = surfaceMomentum;
                    }
                }
                eyePos.y = surface;
                onGround = true;
            }
            else
            {
                onGround = false;
            }

            // Hit the ceiling?
            if (eyePos.y < world->ceilingHeight(eyePos))
            {
                eyePos.y   = world->ceilingHeight(eyePos);
                momentum.y = 0;
            }
        }
        else
        {
            onGround = true;
        }

        // Move in crouch.
        crouch += crouchMomentum * elapsed;
        crouchMomentum -= 2 * elapsed;
        if (crouch < 0)
        {
            crouch         = 0;
            crouchMomentum = 0;
        }
        if (crouch > .6f)
        {
            crouch         = .6f;
            crouchMomentum = 0;
        }

        // Notifications.
        DENG2_FOR_PUBLIC_AUDIENCE(PainLevel, i) { i->userPainLevel(self(), crouch / .6f); }

        if (prevPosition != self().position())
        {
            DENG2_FOR_PUBLIC_AUDIENCE(Move, i) { i->userMoved(self(), self().position()); }
            prevPosition = self().position();
        }

        if (!fequal(prevYaw, yaw))
        {
            DENG2_FOR_PUBLIC_AUDIENCE(Turn, i) { i->userTurned(self(), yaw); }
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

        if (momentum.y < 15)
        {
            if (stepElapsed > .3)
            {
                stepElapsed = 0;
                playRandomStepSound();
            }
            return;
        }

        AudioSystem::get().newSound("user.falldown").setFrequency(.85f + .3f * frand()).play();
    }

    void playRandomStepSound()
    {
        AudioSystem::get()
            .newSound(QString("user.step%1").arg(1 + qrand() % 5))
            .setVolume(.4f + .2f * frand())
            .setFrequency(.6f + frand() * .8f)
            .play();
    }

    void playStepSounds(TimeSpan const &elapsed)
    {
        ddouble velocity = momentum.xz().length();

        if (!onGround) return;
        if (velocity < .1)
        {
            // Only play footsteps when on the ground.
            stepElapsed = 0;
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

//void User::resetYaw()
//{
//    OculusRift &ovr = VRSenseApp::vr().oculusRift();
//    ovr.resetYaw();
//    App::config().set("zeroYaw", ovr.yawOffset());
//}

void User::setWorld(World const *world)
{
    d->world       = world;
    d->firstUpdate = true;

    if (d->world)
    {
        World::POI initial = world->initialViewPosition();
        setPosition(initial.position);
        setYaw(initial.yaw);
    }
}

de::Vector3f User::position() const
{
    return d->eyePos + Vector3f(0, d->crouch, 0);
}

float User::yaw() const
{
    return d->yaw;
}

float User::pitch() const
{
    return d->pitch;
}

void User::setPosition(Vector3f const &pos)
{
    auto oldPos = d->eyePos;

    d->onGround = false;
    d->eyePos   = pos;
    d->momentum = Vector3f();

    if ((oldPos - pos).length() > 15)
    {
        DENG2_FOR_AUDIENCE(Warp, i) i->userWarped(*this);
    }
}

void User::setYaw(float yaw)
{
    d->yaw = yaw;
    d->angularMomentum = 0;
}

void User::setPain(float pain)
{
    DENG2_FOR_AUDIENCE(PainLevel, i) { i->userPainLevel(*this, pain); }
}

void User::setInputState(InputState const &state)
{
    d->input = state;
}

void User::turn(float yaw, float pitch)
{
    d->yaw   = de::wrap(d->yaw + yaw, -180.f, 180.f);
    d->pitch = Rangef(-89, 89).clamp(d->pitch + pitch);
}

void User::update(TimeSpan const &elapsed)
{
    d->move(elapsed);
}

Sound &User::fastWindSound()
{
    return *d->fastWind;
}

} // namespace gloom
