#ifndef USER_H
#define USER_H

#include <de/Vector>
#include <de/Time>
#include <de/Observers>
#include <de/Sound>

class World;

class User
{
public:
    enum InputBit {
        Inert     = 0,
        TurnLeft  = 0x1,
        TurnRight = 0x2,
        Forward   = 0x4,
        Backward  = 0x8,
        StepLeft  = 0x10,
        StepRight = 0x20,
        Shift     = 0x40
    };
    Q_DECLARE_FLAGS(InputState, InputBit)

    DENG2_DEFINE_AUDIENCE(Deletion,  void userBeingDeleted  (User &))
    DENG2_DEFINE_AUDIENCE(Warp,      void userWarped        (User const &))
    DENG2_DEFINE_AUDIENCE(PainLevel, void userPainLevel     (User const &, float pain))
    DENG2_DEFINE_AUDIENCE(Move,      void userMoved         (User const &, de::Vector3f const &pos))
    DENG2_DEFINE_AUDIENCE(Turn,      void userTurned        (User const &, float yaw))

public:
    User();

    void setWorld(World const *world);
    void setPosition(de::Vector3f const &pos);
    void setYaw(float yaw);
    void setPain(float pain);
    void setInputState(InputState const &state);
    void turn(float yaw, float pitch);
    void turn(const de::Vector2f &angles) { turn(angles.x, angles.y); }

    void update(de::TimeSpan const &elapsed);

    de::Vector3f position() const;
    float        yaw() const;
    float        pitch() const;
    de::Sound &  fastWindSound();

private:
    DENG2_PRIVATE(d)
};

Q_DECLARE_OPERATORS_FOR_FLAGS(User::InputState)

#endif // USER_H
