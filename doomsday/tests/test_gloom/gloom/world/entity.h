#ifndef ENTITY_H
#define ENTITY_H

#include <de/Vector>

class Entity
{
public:
    enum Type
    {
        Tree1 = 0,
        Tree2,
        Tree3
    };

public:
    Entity();

    void setType(Type t);
    void setPosition(de::Vector3f const &pos);
    void setScale(float scale);
    void setScale(de::Vector3f const &scale);
    void setAngle(float yawDegrees);

    Type type() const;
    de::Vector3f position() const;
    de::Vector3f scale() const;
    float angle() const;

private:
    DENG2_PRIVATE(d)
};

#endif // ENTITY_H
