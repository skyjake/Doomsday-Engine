#ifndef ILIGHT_H
#define ILIGHT_H

#include <de/Vector>

class ILight
{
public:
    virtual ~ILight() {}

    virtual de::Vector3f lightDirection() const = 0;
    virtual de::Vector3f lightColor() const = 0;
};

#endif // ILIGHT_H
