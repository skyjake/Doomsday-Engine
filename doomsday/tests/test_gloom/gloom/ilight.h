#ifndef ILIGHT_H
#define ILIGHT_H

#include <de/Vector>

namespace gloom {

class ILight
{
public:
    virtual ~ILight() {}

    virtual de::Vector3f lightDirection() const = 0;
    virtual de::Vector3f lightColor() const = 0;
};

} // namespace gloom

#endif // ILIGHT_H
